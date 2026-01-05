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

#include "lsbatch/daemons/sbatchd.h"

extern int sbd_mbd_chan;      /* defined in sbatchd.main.c */
extern int efd;               /* epoll fd from sbatchd.main.c */

// Library stuff not in any header file
extern int _lsb_conntimeout;
extern int _lsb_recvtimeout;
// LavaLite define it as external for now as I dont want to include
// all the stuff from lib.h
extern char *resolve_master_with_retry(void);

// Create a permanent channel to mbd
int sbd_connect_mbd(void)
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

/*
 * Simple wire struct for registration.
 * Put this in sbatchd.h if not already there:
 *
 * struct wire_sbd_register {
 *     char hostname[256];
 * };
 *
 * bool_t xdr_wire_sbd_register(XDR *xdrs, struct wire_sbd_register *msg)
 * {
 *     return xdr_opaque(xdrs, msg->hostname, sizeof(msg->hostname));
 * }
 */
int sbd_mbd_register(void)
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
    hdr.operation = SBD_REGISTER;

    char request_buf[LL_BUFSIZ_4K];
    XDR xdrs_req;
    xdrmem_create(&xdrs_req, request_buf, sizeof(request_buf), XDR_ENCODE);

    if (!xdr_encodeMsg(&xdrs_req,
                       (char *)&req,
                       &hdr,
                       xdr_wire_sbd_register,
                       0,
                       NULL)) {
        lsberrno = LSBE_XDR;
        xdr_destroy(&xdrs_req);
        return -1;
    }

    struct Buffer req_buf = {
        .data = request_buf,
        .len  = (size_t)xdr_getpos(&xdrs_req),
        .forw = NULL
    };
    struct Buffer reply_buf = {0};

    // do blocking io with mbd reply_buf must be not NULL so the call
    // will wait for a reply, we only want back the header for how
    int cc = chan_rpc(sbd_mbd_chan, &req_buf, &reply_buf, &hdr,
                      _lsb_recvtimeout * 1000);
    xdr_destroy(&xdrs_req);
    if (cc < 0) {
        lsberrno = LSBE_SYS_CALL;
        return -1;
    }

    if (hdr.operation != SBD_REGISTER_REPLY ) {
        LS_ERR("mbd registration failed error: %d", lsberrno);
        if (reply_buf.data != NULL)
            free(reply_buf.data);
        return -1;
    }

    if (reply_buf.data != NULL)
        free(reply_buf.data);


    LS_INFO("sbatchd registered with mbd as host: %s", host);

    return 0;
}
