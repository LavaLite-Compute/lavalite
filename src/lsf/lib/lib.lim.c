/* $Id: lib.lim.c,v 1.9 2007/08/15 22:18:50 tmizan Exp $
 * Copyright (C) 2007 Platform Computing Inc
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 USA
 *
 */
#include "lsf/lib/lib.h"
#include "lsf/lib/lib.channel.h"
#include "lsf/lib/ll.sysenv.h"
#include "lsf/lib/ll.host.h"

#define CONNECT_TIMEOUT 5
#define RECV_TIMEOUT 20

struct sockaddr_in sock_addr_in[SOCK_COUNT] = {
    [UDP] = {0},
    [TCP] = {0},
};

int lim_chans[SOCK_COUNT] = {
    [UDP] = -1,
    [TCP] = -1,
};

static_assert((sizeof sock_addr_in / sizeof sock_addr_in[0]) == SOCK_COUNT,
              "sock_addr_in size must match lim_sock_t");

static_assert((sizeof lim_chans / sizeof lim_chans[0]) == SOCK_COUNT,
              "lim_chans size must match lim_sock_t");

static int conntimeout_ = CONNECT_TIMEOUT;
static int recvtimeout_ = RECV_TIMEOUT;

static int callLimTCP_(char *, char **, size_t, struct packet_header *, int);
static int callLimUDP_(char *, char *, size_t, struct packet_header *, char *,
                       int);

int32_t lsf_lim_version = -1;

// Global, defined elsewhere; we just clear it here.
extern bool_t masterLimDown;

int callLim_(enum limReqCode reqCode, void *dsend, bool_t (*xdr_sfunc)(),
             void *drecv, bool_t (*xdr_rfunc)(), char *host, int options,
             struct packet_header *hdr)
{
    char sbuf[LL_BUFSIZ_256];
    char rbuf[LL_BUFSIZ_4K];
    static bool_t first = true;

    // host parameter is legacy we always talk to local LIM for UDP
    (void) host;

    masterLimDown = false;

    // We require a transport flag
    if (!(options & (_USE_UDP_ | _USE_TCP_))) {
        // transport must be explicit now; catch legacy call sites
        lserrno = LSE_BAD_ARGS;
        return -1;
    }

    if (first) {
        if (initLimSock_() < 0)
            return -1;
        first = false;

        if (genParams[LSF_API_CONNTIMEOUT].paramValue) {
            conntimeout_ = atoi(genParams[LSF_API_CONNTIMEOUT].paramValue);
            if (conntimeout_ <= 0)
                conntimeout_ = CONNECT_TIMEOUT;
        }

        if (genParams[LSF_API_RECVTIMEOUT].paramValue) {
            recvtimeout_ = atoi(genParams[LSF_API_RECVTIMEOUT].paramValue);
            if (recvtimeout_ <= 0)
                recvtimeout_ = RECV_TIMEOUT;
        }
    }

    struct packet_header reqHdr;
    init_pack_hdr(&reqHdr);
    reqHdr.operation = reqCode;
    reqHdr.version = CURRENT_PROTOCOL_VERSION;

    XDR xdrs;
    xdrmem_create(&xdrs, sbuf, sizeof(sbuf), XDR_ENCODE);
    if (!xdr_encodeMsg(&xdrs, dsend, &reqHdr, xdr_sfunc, 0, NULL)) {
        xdr_destroy(&xdrs);
        lserrno = LSE_BAD_XDR;
        return -1;
    }

    struct packet_header reply_hdr;
    char *rep_buf = NULL;
    size_t req_len = xdr_getpos(&xdrs);
    xdr_destroy(&xdrs);

    if (options & _USE_TCP_) {
        if (callLimTCP_(sbuf, &rep_buf, req_len, &reply_hdr, options) < 0)
            return -1;

        if (reply_hdr.length != 0)
            xdrmem_create(&xdrs, rep_buf, reply_hdr.length, XDR_DECODE);
        else
            xdrmem_create(&xdrs, rbuf, sizeof(rbuf), XDR_DECODE);

    } else { // _USE_UDP_
        if (callLimUDP_(sbuf, rbuf, req_len, &reqHdr, NULL, options) < 0)
            return -1;
        if (options & _NON_BLOCK_)
            return 0;
        xdrmem_create(&xdrs, rbuf, sizeof(rbuf), XDR_DECODE);

        if (!xdr_pack_hdr(&xdrs, &reply_hdr)) {
            xdr_destroy(&xdrs);
            lserrno = LSE_BAD_XDR;
            return -1;
        }
    }

    enum limReplyCode limReplyCode;
    limReplyCode = reply_hdr.operation;
    lsf_lim_version = reply_hdr.version;

    if (limReplyCode != LIME_NO_ERR) {
        xdr_destroy(&xdrs);
        if (options & _USE_TCP_)
            FREEUP(rep_buf);
        err_return_(limReplyCode);
        return -1;
    }

    // Success: decode reply body if requested.
    if (drecv != NULL) {
        if (!(*xdr_rfunc)(&xdrs, drecv, &reply_hdr)) {
            xdr_destroy(&xdrs);
            if (options & _USE_TCP_)
                FREEUP(rep_buf);
            lserrno = LSE_BAD_XDR;
            return -1;
        }
    }

    xdr_destroy(&xdrs);
    if (options & _USE_TCP_)
        FREEUP(rep_buf);

    if (hdr != NULL)
        *hdr = reply_hdr;

    return 0;
}

/*
 * KISS: resolve master if needed, connect via TCP, RPC once, optionally keep
 * the connection open if caller sets _KEEP_CONNECT_.
 */
static int callLimTCP_(char *reqbuf, char **rep_buf, size_t req_size,
                       struct packet_header *reply_hdr, int options)
{
    // Ensure we know where master is. This always goes via local LIM UDP.
    if (sock_addr_in[TCP].sin_addr.s_addr == 0) {
        if (ls_getmastername() == NULL) {
            lserrno = LSE_MASTR_UNKNW;
            return -1;
        }
    }

    int cc;
    struct Buffer sndbuf = {.data = reqbuf, .len = req_size};
    struct Buffer rcvbuf = {0};

    if (lim_chans[TCP] < 0) {
        lim_chans[TCP] = chan_client_socket(AF_INET, SOCK_STREAM, 0);
        if (lim_chans[TCP] < 0)
            return -1;

        cc = chan_connect(lim_chans[TCP], &sock_addr_in[TCP],
                          conntimeout_ * 1000, 0);
        if (cc < 0) {
            if (errno == ECONNREFUSED)
                lserrno = LSE_LIM_DOWN;

            CLOSECD(lim_chans[TCP]);
            sock_addr_in[TCP].sin_addr.s_addr = 0;
            sock_addr_in[TCP].sin_port = 0;
            return -1;
        }
    }

    cc = chan_rpc(lim_chans[TCP], &sndbuf, &rcvbuf, reply_hdr,
                  recvtimeout_ * 1000);
    if (cc < 0) {
        CLOSECD(lim_chans[TCP]);
        return -1;
    }

    // At this point we have a protocol-level reply from "whoever"
    // we connected to.
    switch (reply_hdr->operation) {
    case LIME_WRONG_MASTER:
    case LIME_MASTER_UNKNW:
        // Remote told us our target is stale or unknown: drop cache.
        lserrno = LSE_MASTR_UNKNW;
        FREEUP(rcvbuf.data);
        CLOSECD(lim_chans[TCP]);
        sock_addr_in[TCP].sin_addr.s_addr = 0;
        sock_addr_in[TCP].sin_port = 0;
        return -1;

    default:
        break;
    }

    *rep_buf = rcvbuf.data;

    if (!(options & _KEEP_CONNECT_))
        CLOSECD(lim_chans[TCP]);

    return 0;
}

/*
 * KISS: always send to local LIM via UDP on UDP.
 * host parameter is legacy and ignored; we never multicast or probe others.
 */
static int callLimUDP_(char *req_buf, char *rep_buf, size_t len,
                       struct packet_header *reqHdr, char *host, int options)
{
    // Legacy stuff remove later
    (void) reqHdr;
    (void) host;

    if (lim_chans[UDP] < 0) {
        lim_chans[UDP] = chan_client_socket(AF_INET, SOCK_DGRAM, 0);
        if (lim_chans[UDP] < 0)
            return -1;
    }

    int cc = chan_send_dgram(lim_chans[UDP], req_buf, len, &sock_addr_in[UDP]);
    if (cc < 0)
        return -1;

    if (options & _NON_BLOCK_)
        return 0;

    struct sockaddr_storage from;
    cc = chan_recv_dgram_(lim_chans[UDP], rep_buf, MSGSIZE, &from,
                       conntimeout_ * 1000);
    if (cc < 0)
        return -1;

    return 0;
}

int initLimSock_(void)
{
    uint16_t service_port;

    if (initenv_(NULL, NULL) < 0)
        return -1;

    if (genParams[LSF_LIM_PORT].paramValue == NULL) {
        lserrno = LSE_LIM_NREG;
        return -1;
    }

    service_port = atoi(genParams[LSF_LIM_PORT].paramValue);
    if (service_port == 0) {
        lserrno = LSE_SOCK_SYS;
        return -1;
    }

    // Local LIM UDP endpoint which is the loopback address
    sock_addr_in[UDP].sin_family = AF_INET;
    sock_addr_in[UDP].sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    sock_addr_in[UDP].sin_port = htons(service_port);
    lim_chans[UDP] = -1;

    // TCP slot for master: port/address resolved via ls_getmastername().
    sock_addr_in[TCP].sin_family = AF_INET;
    sock_addr_in[TCP].sin_addr.s_addr = 0;
    sock_addr_in[TCP].sin_port = 0;
    lim_chans[TCP] = -1;

    return 0;
}

void err_return_(enum limReplyCode limReplyCode)
{
    switch (limReplyCode) {
    case LIME_BAD_RESREQ:
        lserrno = LSE_BAD_EXP;
        return;

    case LIME_NO_OKHOST:
        lserrno = LSE_NO_HOST;
        return;

    case LIME_NO_ELHOST:
        lserrno = LSE_NO_ELHOST;
        return;

    case LIME_BAD_DATA:
        lserrno = LSE_BAD_ARGS;
        return;

    case LIME_MASTER_UNKNW:
    case LIME_WRONG_MASTER:
        lserrno = LSE_MASTR_UNKNW;
        return;

    case LIME_IGNORED:
        lserrno = LSE_LIM_IGNORE;
        return;

    case LIME_DENIED:
        lserrno = LSE_LIM_DENIED;
        return;

    case LIME_UNKWN_HOST:
        lserrno = LSE_LIM_BADHOST;
        return;

    case LIME_LOCKED_AL:
        lserrno = LSE_LIM_ALOCKED;
        return;

    case LIME_NOT_LOCKED:
        lserrno = LSE_LIM_NLOCKED;
        return;

    case LIME_UNKWN_MODEL:
        lserrno = LSE_LIM_BADMOD;
        return;

    case LIME_BAD_SERVID:
        lserrno = LSE_BAD_SERVID;
        return;

    case LIME_NAUTH_HOST:
        lserrno = LSE_NLSF_HOST;
        return;

    case LIME_UNKWN_RNAME:
        lserrno = LSE_UNKWN_RESNAME;
        return;

    case LIME_UNKWN_RVAL:
        lserrno = LSE_UNKWN_RESVALUE;
        return;

    case LIME_BAD_FILTER:
        lserrno = LSE_BAD_NAMELIST;
        return;

    case LIME_NO_MEM:
        lserrno = LSE_LIM_NOMEM;
        return;

    case LIME_BAD_REQ_CODE:
        lserrno = LSE_PROTOC_LIM;
        return;

    case LIME_BAD_RESOURCE:
        lserrno = LSE_BAD_RESOURCE;
        return;

    case LIME_NO_RESOURCE:
        lserrno = LSE_NO_RESOURCE;
        return;

    default:
        lserrno = limReplyCode;
        return;
    }
}
