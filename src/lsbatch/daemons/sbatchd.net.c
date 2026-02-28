/*
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

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#include "lsbatch/daemons/sbd.h"

extern int sbd_mbd_chan;      /* defined in sbatchd.main.c */
extern int efd;               /* epoll fd from sbatchd.main.c */

// Library stuff not in any header file
extern int _lsb_conntimeout;
extern int _lsb_recvtimeout;
// LavaLite define it as external for now as I dont want to include
// all the stuff from lib.h
extern char *resolve_master_with_retry(void);

/*
 * sbd_nb_connect_mbd()
 *
 * Create a TCP client channel to MBD and initiate a non-blocking connect.
 *
 * Master name is obtained via resolve_master_try() and resolved to IPv4
 * using get_host_by_name(). IPv6 is intentionally out of scope for now.
 *
 * On success, the channel socket is registered with epoll:
 * - If connect completes immediately, EPOLLIN|EPOLLERR|EPOLLRDHUP is used
 *   and *connected is set to TRUE.
 * - If connect is in progress, EPOLLOUT|EPOLLERR|EPOLLRDHUP is used and
 *   *connected is set to FALSE (caller will finish on EPOLLOUT).
 *
 * Return values:
 *   >=0  channel id on success
 *   -1   on failure; lsberrno/lserrno is set
 */
int sbd_mbd_nb_connect(bool_t *connected)
{
    if (connected != NULL)
        *connected = false;

    char *master = resolve_master_try();
    if (master == NULL)
        return -1;

    uint16_t port = get_mbd_port();
    if (port == 0) {
        lsberrno = LSBE_PROTOCOL;
        return -1;
    }

    struct ll_host hp;
    memset(&hp, 0, sizeof(hp));
    if (get_host_by_name(master, &hp) < 0) {
        // Keep mapping simple for now; get_host_by_name() can set lserrno.
        lsberrno = LSBE_LSLIB;
        return -1;
    }

    if (hp.family != AF_INET) {
        // IPv6 out of scope for now
        lsberrno = LSBE_PROTOCOL;
        return -1;
    }

    // Convert ll_host sockaddr_storage -> sockaddr_in and set port.
    struct sockaddr_in peer;
    memset(&peer, 0, sizeof(peer));
    peer = *(struct sockaddr_in *)&hp.sa;
    peer.sin_port = htons(port);

    int ch_id = chan_client_socket(AF_INET, SOCK_STREAM, 0);
    if (ch_id < 0)
        return -1;

    // LavaLite gives the client the buffers
    channels[ch_id].send = chan_make_buf();
    channels[ch_id].recv = chan_make_buf();

    if (channels[ch_id].send == NULL || channels[ch_id].recv == NULL) {
        lserrno = LSE_NO_MEM;
        chan_close(ch_id);
        return -1;
    }

    int rc = connect_begin(chan_sock(ch_id),
                           (struct sockaddr *)&peer,
                           sizeof(peer));
    if (rc < 0) {
        lserrno = LSE_CONN_SYS;
        chan_close(ch_id);
        return -1;
    }

    struct epoll_event ev;
    if (rc == 0) {
        if (connected != NULL)
            *connected = true;

        ev.events = EPOLLIN | EPOLLERR | EPOLLRDHUP;
        ev.data.u32 = (uint32_t)ch_id;

        if (epoll_ctl(sbd_efd, EPOLL_CTL_ADD, chan_sock(ch_id), &ev) < 0) {
            LS_ERR("epoll_ctl() failed to add mbd chan");
            chan_close(ch_id);
            return -1;
        }

        return ch_id;
    }

    ev.events = EPOLLOUT | EPOLLERR | EPOLLRDHUP;
    ev.data.u32 = (uint32_t)ch_id;

    if (epoll_ctl(sbd_efd, EPOLL_CTL_ADD, chan_sock(ch_id), &ev) < 0) {
        LS_ERR("epoll_ctl() failed to add mbd connecting chan");
        chan_close(ch_id);
        return -1;
    }

    return ch_id;
}

// Create a permanent channel to mbd using a blocking connect
int sbd_mbd_connect(void)
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
    // LavaLite give the client the buffers
    channels[ch_id].send = chan_make_buf();
    channels[ch_id].recv = chan_make_buf();

    struct epoll_event ev = {.events = EPOLLIN, .data.u32 = (uint32_t)ch_id};

    if (epoll_ctl(sbd_efd, EPOLL_CTL_ADD, chan_sock(ch_id), &ev) < 0) {
        LS_ERR("epoll_ctl() failed to add sbd chan");
        chan_close(ch_id);
        return -1;
    }

    return ch_id;
}

void sbd_mbd_link_down(void)
{
    if (sbd_mbd_chan >= 0)
        chan_close(sbd_mbd_chan);

    sbd_mbd_chan = -1;
    sbd_mbd_connecting = false;

    struct ll_list_entry *e;
    for (e = sbd_job_list.head; e; e = e->next) {
        struct sbd_job *job = (struct sbd_job *)e;

        job->reply_last_send = job->execute_last_send
            = job->finish_last_send = 0;
        // Write the latest job state
        if (sbd_job_state_write(job) < 0) {
            LS_ERRX("job=%ld state write failed", job->job_id);
            sbd_fatal(SBD_FATAL_STORAGE);
        }
    }

    LS_ERR("mbd link down: cleared pending sent flags for resend and state");
}

int sbd_register(int chan_id)
{
    char host[MAXHOSTNAMELEN];

    if (gethostname(host, sizeof(host)) < 0) {
        LS_ERR("cannot get local hostname: %m");
        snprintf(host, sizeof(host), "unknown");
    }

    struct wire_sbd_register req;
    memset(&req, 0, sizeof(req));
    snprintf(req.hostname, sizeof(req.hostname), "%s", host);

    struct packet_header hdr;
    init_pack_hdr(&hdr);
    hdr.operation = BATCH_SBD_REGISTER;

    struct Buffer *buf = NULL;
    if (chan_alloc_buf(&buf, LL_BUFSIZ_4K) < 0) {
        LS_ERR("sbd register: chan_alloc_buf failed");
        return -1;
    }

    XDR xdrs;
    xdrmem_create(&xdrs, buf->data, LL_BUFSIZ_4K, XDR_ENCODE);

    if (!xdr_encodeMsg(&xdrs,
                       (char *)&req,
                       &hdr,
                       xdr_wire_sbd_register,
                       0,
                       NULL)) {
        LS_ERR("sbd register: xdr_encodeMsg failed");
        xdr_destroy(&xdrs);
        chan_free_buf(buf);
        lsberrno = LSBE_XDR;
        return -1;
    }

    buf->len = (size_t)xdr_getpos(&xdrs);
    xdr_destroy(&xdrs);

    /*
     * Queue for send. This must append buf to the channel send queue and
     * ensure EPOLLOUT interest is enabled so dowrite() will run.
     */
    if (chan_enqueue(chan_id, buf) < 0) {
        LS_ERR("sbd register: send enqueue failed");
        chan_free_buf(buf);
        return -1;
    }

    // Always rememeber to enable EPOLLOUT on the main sbd_efd
    // to have dowrite() to send out the request
    chan_set_write_interest(chan_id, true);

    LS_INFO("sbd register: enqueued request as host: %s", host);

    return 0;
}
