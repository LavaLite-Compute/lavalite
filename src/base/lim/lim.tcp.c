/*
 * Copyright (C) LavaLite Contributors
 * GPL v2
 */

#include "base/lim/lim.h"

int tcp_message(int ch_id)
{
    if (chan_has_error(ch_id)) {
        LS_DEBUG("lost connection from %s", chah_addr(ch_id));
        shutdown_tcp_chan(ch_id);
        return -1;
    }

    tcp_dispatch(ch_id);

    return 0;
}

int tcp_accept(void)
{
    struct sockaddr_in from;
    struct host_node *host;

    int ch_id = chan_accept(lim_tcp_chan, &from);
    if (ch_id < 0) {
        LS_ERR("%s: chan_accept() failed: %m", __func__);
        return -1;
    }

    char addr[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &from->sin_addr, addr, sizeof(addr));
    struct lim_node *n = ll_hash_search(&lim_node_addr_hash, addr);
    if (n == NULL) {
        log_warn("rejected connection from unknown host %s", addr);
        chan_close(ch_id);
        return -1;
    }

    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.u32 = ch_id;
    epoll_ctl(efd, EPOLL_CTL_ADD, chan_sock(ch_id), &ev);

    return 0;
}

static void tcp_dispatch(int ch_id)
{
    struct Buffer *buf;
    struct protocol_header hdr;
    XDR xdrs;

    if (chan_dequeue(ch_id, &buf) < 0) {
        LS_ERR("chan_dequeue() failed");
        shutdown_tcp_chan(client);
        return;
    }

    xdrmem_create(&xdrs, buf->data, buf->len, XDR_DECODE);
    if (!xdr_pack_hdr(&xdrs, &hdr)) {
        LS_ERR("xdr_pack_hdr failed");
        xdr_destroy(&xdrs);
        shutdown_tcp_chan(client);
        return;
    }

    LS_DEBUG("protocol %d", hdr.operation);

    // the client data structure is owned by this
    // layer so the callers should not free it
    switch (hdr.operation) {
    case LIM_LOAD_REQ:
        load_req(&xdrs, client, &hdr);
        xdr_destroy(&xdrs);
        shutdown_tcp_chan(client);
        break;
    case LIM_GET_HOSTINFO:
        host_info_req(&xdrs, client, &hdr);
        xdr_destroy(&xdrs);
        shutdown_tcp_chan(client);
        break;
    case LIM_GET_RESOUINFO:
        resource_info_req(&xdrs, client, &hdr);
        xdr_destroy(&xdrs);
        shutdown_tcp_chan(client);
        break;
    case LIM_GET_INFO:
        info_req(&xdrs, client, &hdr);
        xdr_destroy(&xdrs);
        shutdown_tcp_chan(client);
        break;
    case LIM_PING:
        // Master takeover ping - no reply needed
        xdr_destroy(&xdrs);
        shutdown_tcp_chan(client);
        break;
    case LIM_GET_CLUSINFO:
        clus_info_req(&xdrs, client, &hdr);
        xdr_destroy(&xdrs);
        shutdown_tcp_chan(client);
        break;
    default:
        LS_ERR("invalid operation %d", hdr.operation);
        xdr_destroy(&xdrs);
        shutdown_tcp_chan(client);
        break;
    }
}

static void shutdown_tcp_chan(int ch_id)
{
    epoll_ctl(efd, EPOLL_CTL_DEL, chan_sock(ch_id), NULL);
    chan_close(ch_id);
}
