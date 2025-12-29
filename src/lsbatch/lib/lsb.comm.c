/* ------------------------------------------------------------------------
 * LavaLite — High-Performance Job Scheduling Infrastructure
 *
 * Copyright (C) LavaLite Contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 * ------------------------------------------------------------------------ */

#include "lsbatch/lib/lsb.h"
#include "lsf/lib/lib.channel.h"
#include "lsf/lib/ll.host.h"
#include "lsf/lib/lib.xdr.h"

extern int logclass;
extern int _lsb_conntimeout;
extern int _lsb_recvtimeout;

/* Resolve master with retry
 */
char *resolve_master_with_retry(void)
{
    int retry = 3;

    while (retry-- > 0) {
        // ls_getmastername() returns __thread buffer
        char *master = ls_getmastername();
        if (master == NULL) {
            fprintf(stderr,
                    "LSF daemon (LIM) not responding ... still trying\n");
            millisleep_(_lsb_conntimeout * 1000);
            continue;
        }
        return master;
    }
    lsberrno = LSBE_LSLIB;
    return NULL;
}

/*
 * Pattern 1: Simple RPC - connect, send, receive, close
 * Used by most API calls: lsb_runjob, lsb_signaljob, lsb_kill, etc.
 */

/* Synchronous RPC to MBD - single request/response, closes connection
 *
 * Args:
 *   req      - XDR-encoded request buffer
 *   req_len  - Length of request
 *   reply    - Receives allocated reply buffer (caller must free)
 *   hdr      - Receives packet header from reply
 *   extra    - typically the job file, but could be any extra data
 *              we want to be in the packet
 *
 * Returns: Number of bytes in reply on success, -1 on error
 */
int call_mbd(void *req, size_t req_len, char **reply, struct packet_header *hdr,
             struct lenData *extra)
{
    uint16_t port;
    int rc;

    char *master = resolve_master_with_retry();
    if (master == NULL) {
        return -1;
    }

    port = get_mbd_port();
    if (port == 0) {
        lsberrno = LSBE_PROTOCOL;
        return -1;
    }

    int ch_id = serv_connect(master, port, _lsb_conntimeout);
    if (ch_id < 0) {
        lsberrno = LSBE_CONN_REFUSED;
        return -1;
    }

    struct Buffer e;
    struct Buffer req_buf = {.data = req, .len = req_len, .forw = NULL};
    if (extra && extra->len > 0) {
        e.data = extra->data;
        e.len = extra->len;
        e.forw = NULL;
        req_buf.forw = &e;
    }
    struct Buffer reply_buf = {0};

    rc = chan_rpc(ch_id, &req_buf, &reply_buf, hdr, _lsb_recvtimeout * 1000);
    chan_close(ch_id);
    if (rc < 0) {
        lsberrno = LSBE_PROTOCOL;
        return -1;
    }

    *reply = reply_buf.data;
    return hdr->length;
}

/*
 * Pattern 2: Streaming - open connection, read multiple records, close
 * Used by job query APIs that can return thousands of records
 */

/* Open connection and send initial request, keep socket open for streaming
 *
 * This is for APIs like lsb_openjobinfo() that need to stream many records
 * back from MBD without buffering them all in memory. The pattern is:
 *
 *   sock = open_mbd_stream(...)  // Send query, get first response
 *   while (read_mbd_stream(...)) // Read records one at a time
 *       process_record(...)
 *   close_mbd_stream(sock)        // Clean up
 *
 * Args:
 *   req      - XDR-encoded request buffer
 *   req_len  - Length of request
 *   reply    - Receives allocated reply buffer for first response
 *   hdr      - Receives packet header from first response
 *
 * Returns: Socket fd on success (caller must close), -1 on error
 */
int open_mbd_stream(void *req, size_t req_len, char **reply,
                    struct packet_header *hdr)
{
    char *master = resolve_master_with_retry();
    if (master == NULL) {
        return -1;
    }

    uint16_t port = get_mbd_port();
    if (port == 0) {
        lsberrno = LSBE_PROTOCOL;
        return -1;
    }

    int ch_id = serv_connect(master, port, _lsb_conntimeout);
    if (ch_id < 0) {
        lsberrno = LSBE_CONN_REFUSED;
        return -1;
    }

    struct Buffer req_buf = {.data = req, .len = req_len, .forw = NULL};
    struct Buffer reply_buf = {0};

    int rc =
        chan_rpc(ch_id, &req_buf, &reply_buf, hdr, _lsb_recvtimeout * 1000);
    if (rc < 0) {
        chan_close(ch_id);
        lsberrno = LSBE_PROTOCOL;
        return -1;
    }

    *reply = reply_buf.data;

    /* Return the id of the channel correspinding to the
     * socket for subsequent reads
     */
    return ch_id;
}

/* Read one packet from socket with timeout
 *
 * Reads packet header, then payload. Allocates buffer for payload.
 *
 * Args:
 *   buffer      - Receives allocated buffer (caller must free)
 *   timeout_sec - Timeout in seconds
 *   hdr         - Receives decoded packet header
 *   sock        - Socket to read from
 *
 * Returns: Number of payload bytes on success, -1 on error
 */
int readNextPacket(char **msg_buf, int _lsb_recvtimeout,
                   struct packet_header *hdr, int mbd_sock)
{
    struct Buffer reply_buf;

    if (mbd_sock < 0) {
        lsberrno = LSBE_CONN_NONEXIST;
        return -1;
    }

    // timeout is in seconds
    int cc = chan_rpc(mbd_sock, NULL, &reply_buf, hdr, _lsb_recvtimeout);
    if (cc < 0) {
        lsberrno = LSBE_LSLIB;
        return -1;
    }

    if (hdr->length == 0) {
        close(mbd_sock);
        lsberrno = LSBE_EOF;
        return -1;
    }

    *msg_buf = reply_buf.data;
    return reply_buf.len;
}

/* Read next record from open stream
 *
 * Reads one packet from the stream. This is like fgets() - call it in a loop
 * until it returns -1 to indicate end of stream or error.
 *
 * Args:
 *   sock    - Socket fd from open_mbd_stream()
 *   buffer  - Receives allocated buffer with packet data (caller must free)
 *   hdr     - Receives packet header
 *
 * Returns: Number of bytes read on success, -1 on EOF or error
 */
int read_mbd_stream(int sock, char **buffer, struct packet_header *hdr)
{
    int rc;

    if (sock < 0) {
        lsberrno = LSBE_BAD_ARG;
        return -1;
    }

    rc = readNextPacket(buffer, _lsb_recvtimeout, hdr, sock);
    if (rc < 0) {
        lsberrno = LSBE_EOF;
        return -1;
    }

    return rc;
}

int serv_connect(char *serv_host, ushort serv_port, int timeout)
{
    struct ll_host hs;

    int cc = get_host_by_name(serv_host, &hs);
    if (cc < 0) {
        lsberrno = LSBE_LSLIB;
        return -1;
    }

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));

    // Cast the sockaddr_storage buffer to the structure
    // that is specific to the given socket family AF_INET
    // in this case
    serv_addr.sin_family = AF_INET;
    struct sockaddr_in *sin = (struct sockaddr_in *) &hs.sa;

    memcpy(&serv_addr.sin_addr, &sin->sin_addr, sizeof(struct in_addr));
    serv_addr.sin_port = htons(serv_port);

    int ch_id = chan_client_socket(AF_INET, SOCK_STREAM, 0);
    if (ch_id < 0) {
        lsberrno = LSBE_LSLIB;
        return -1;
    }

    cc = chan_connect(ch_id, &serv_addr, timeout * 1000, 0);
    if (cc < 0) {
        // Some translation between lserrno and lsberrno
        switch (lserrno) {
        case LSE_TIME_OUT:
            lsberrno = LSBE_CONN_TIMEOUT;
            break;
        default:
            lsberrno = LSBE_SYS_CALL;
        }
        chan_close(ch_id);
        return -1;
    }

    return ch_id;
}

/* Close streaming connection
 *
 * Args:
 *   ch_id - Channle id from open_mbd_stream()
 */
void close_mbd_stream(int ch_id)
{
    if (ch_id >= 0) {
        chan_close(ch_id);
    }
}

/*
 * Async messaging for daemon-to-daemon communication
 * Used by MBD <-> SBD communication for job execution
 */

/* Enqueue message to MBD (from SBD)
 *
 * This is asynchronous - message is queued and sent when channel is ready.
 * Used by sbatchd to send job status updates, completion notifications, etc.
 *
 * Args:
 *   chan_fd - Channel file descriptor
 *   msg     - XDR-encoded message buffer
 *   msg_len - Length of message
 *
 * Returns: 0 on success, -1 on error
 */
int enqueue_to_mbd(int chan_id, void *msg, size_t msg_len)
{
    struct Buffer buf = {.data = msg, .len = msg_len, .forw = NULL};

    if (chan_id < 0 || msg == NULL || msg_len == 0) {
        return -1;
    }

    if (chan_enqueue(chan_id, &buf) < 0) {
        return -1;
    }

    return 0;
}

/* Enqueue message to SBD (from MBD)
 *
 * This is asynchronous - message is queued and sent when channel is ready.
 * Used by mbatchd to send job dispatch, signal, kill, etc.
 *
 * Args:
 *   chan_id - Channel id of the socket file descriptor
 *   msg     - XDR-encoded message buffer
 *   msg_len - Length of message
 *
 * Returns: 0 on success, -1 on error
 */
int enqueue_to_sbd(int chan_id, void *msg, size_t msg_len)
{
    struct Buffer buf = {.data = msg, .len = msg_len, .forw = NULL};

    if (chan_id < 0 || msg == NULL || msg_len == 0) {
        return -1;
    }

    if (chan_enqueue(chan_id, &buf) < 0) {
        return -1;
    }

    return 0;
}

/* Send ACK response
 *
 * Quick acknowledgment for async messages. Often better to use enqueue_mbd/sbd
 * with a proper response message, but this is convenient for simple ACKs.
 *
 * Args:
 *   chan_id - Channel id file descriptor
 *   seq     - Sequence number to acknowledge
 *
 * Returns: 0 on success, -1 on error
 */
int send_ack(int chan_id, uint32_t seq)
{
    struct packet_header ack = {
        .sequence = seq, .operation = BATCH_STATUS_ACK, .length = 0};

    char buf[sizeof(struct packet_header)];
    XDR xdrs;

    xdrmem_create(&xdrs, buf, sizeof(buf), XDR_ENCODE);

    if (!xdr_pack_hdr(&xdrs, &ack)) {
        xdr_destroy(&xdrs);
        return -1;
    }
    xdr_destroy(&xdrs);

    struct Buffer ack_buf = {.data = buf, .len = sizeof(buf), .forw = NULL};

    return chan_enqueue(chan_id, &ack_buf);
}
int authTicketTokens_(struct lsfAuth *auth, char *toHost)
{
    if (toHost == NULL) {
        char *master = resolve_master_with_retry();
        if (master == NULL)
            return -1;
        putEnv("LSF_EAUTH_SERVER", master);
    } else {
        putEnv("LSF_EAUTH_SERVER", "sbatchd");
    }
    putEnv("LSF_EAUTH_CLIENT", "user");

    if (getAuth_(auth, toHost) == -1) {
        lsberrno = LSBE_LSLIB;
        return -1;
    }

    return 0;
}
