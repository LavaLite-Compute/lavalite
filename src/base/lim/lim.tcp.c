/*
 * Copyright (C) LavaLite Contributors
 * GPL v2
 */

#include "base/lim/lim.h"

static void shutdown_tcp_chan(int ch_id)
{
    epoll_ctl(lim_efd, EPOLL_CTL_DEL, chan_sock(ch_id), NULL);
    chan_close(ch_id);
}

static void get_load(XDR *xdrs, int ch_id)
{
    uint32_t nhosts = ll_list_count(&node_list);
    struct wire_load *hosts = calloc(nhosts, sizeof(struct wire_load));
    if (!hosts) {
        LS_ERR("calloc failed");
         return;
    }

    int i = 0;
    struct ll_list_entry *e;
    for (e = node_list.head; e != NULL; e = e->next) {
        struct lim_node *n = (struct lim_node *)e;

        snprintf(hosts[i].hostname, MAXHOSTNAMELEN, "%s", n->host->name);
        hosts[i].status = n->status;
        hosts[i].num_metrics = NUM_METRICS;

        for (int j = 0; j < NUM_METRICS; j++)
            hosts[i].li[j] = n->load_index[j];
        i++;
    }

    /*
     * Estimate buffer size.
     * header + array length + host records
     */
    size_t bufsiz = sizeof(struct protocol_header) +
        sizeof(uint32_t) + nhosts * sizeof(struct wire_load)
        + LL_BUFSIZ_256;

    struct chan_buffer *buf;
    if (chan_alloc_buf(&buf, bufsiz) < 0) {
        LS_ERR("chan_alloc_buf failed op=%d bufsiz=%ld",
               LIM_REPLY_LOAD, bufsiz);
        free(hosts);
        return;
    }

    XDR xdrs_out;
    xdrmem_create(&xdrs_out, buf->data, bufsiz, XDR_ENCODE);

    struct protocol_header hdr;
    init_pack_hdr(&hdr);
    hdr.operation = LIM_REPLY_LOAD;
    hdr.status = LIM_OK;

    if (!xdr_pack_hdr(&xdrs_out, &hdr)) {
        LS_ERR("xdr_pack_hdr failed");
        goto fail;
    }

    if (!xdr_wire_load_array(&xdrs_out, &hosts, &nhosts)) {
        LS_ERR("xdr_wire_load_array failed");
        goto fail;
    }

    buf->len = (size_t)xdr_getpos(&xdrs_out);
    if (chan_enqueue(ch_id, buf) < 0) {
        LS_ERR("chan_enqueue failed to=%s len=%d",
               chan_addr_str(ch_id), buf->len);
        xdr_destroy(&xdrs_out);
        chan_free_buf(buf);
        free(hosts);
        return;
    }

    xdr_destroy(&xdrs_out);
    free(hosts);
    return;

fail:
    xdr_destroy(&xdrs_out);
    chan_free_buf(buf);
    free(hosts);
}

static void get_hosts(XDR *xdrs, int ch_id)
{
    uint32_t nhosts = ll_list_count(&node_list);
    struct wire_host *wh = calloc(nhosts, sizeof(struct wire_host));
    if (!wh) {
        LS_ERR("calloc failed");
         return;
    }

    int i = 0;
    struct ll_list_entry *e;
    for (e = node_list.head; e != NULL; e = e->next) {
        struct lim_node *n = (struct lim_node *)e;

        snprintf(wh[i].hostname, MAXHOSTNAMELEN, "%s", n->host->name);
        snprintf(wh[i].machine, LL_BUFSIZ_32, "%s", n->machine);
        wh[i].max_mem = n->max_mem;
        wh[i].max_swap = n->max_swap;
        wh[i].max_tmp = n->max_tmp;
        wh[i].num_cpus = n->num_cpus;

        i++;
    }

    /*
     * Estimate buffer size.
     * header + array length + host records
     */
    size_t bufsiz = sizeof(struct protocol_header) +
        sizeof(uint32_t) + nhosts * sizeof(struct wire_host)
        + LL_BUFSIZ_256;

    struct chan_buffer *buf;
    if (chan_alloc_buf(&buf, bufsiz) < 0) {
        LS_ERR("chan_alloc_buf failed op=%d bufsiz=%ld",
               LIM_REPLY_HOSTS, bufsiz);
        free(wh);
        return;
    }

    XDR xdrs_out;
    xdrmem_create(&xdrs_out, buf->data, bufsiz, XDR_ENCODE);

    struct protocol_header hdr;
    init_pack_hdr(&hdr);
    hdr.operation = LIM_REPLY_HOSTS;
    hdr.status = LIM_OK;

    if (!xdr_pack_hdr(&xdrs_out, &hdr)) {
        LS_ERR("xdr_pack_hdr failed");
        goto fail;
    }

    if (!xdr_wire_hosts_array(&xdrs_out, &wh, &nhosts)) {
        LS_ERR("xdr_wire_load_array failed");
        goto fail;
    }

    buf->len = (size_t)xdr_getpos(&xdrs_out);
    if (chan_enqueue(ch_id, buf) < 0) {
        LS_ERR("chan_enqueue failed to=%s len=%d",
               chan_addr_str(ch_id), buf->len);
        xdr_destroy(&xdrs_out);
        chan_free_buf(buf);
        free(wh);
        return;
    }

    xdr_destroy(&xdrs_out);
    free(wh);
    return;

fail:
    xdr_destroy(&xdrs_out);
    chan_free_buf(buf);
    free(wh);
}
static const char *proto_to_str(int32_t op)
{
    switch (op) {
    case LIM_GET_LOAD:
        return "LIM_GET_LOAD";
    case LIM_GET_HOSTS:
        return "LIM_GET_HOSTS";
    default:
        return "unknown";
    }
}

static void tcp_dispatch(int ch_id)
{
    struct chan_buffer *buf;
    struct protocol_header hdr;
    XDR xdrs;

    if (chan_dequeue(ch_id, &buf) < 0) {
        LS_ERR("chan_dequeue() failed");
        shutdown_tcp_chan(ch_id);
        return;
    }

    xdrmem_create(&xdrs, buf->data, buf->len, XDR_DECODE);
    if (!xdr_pack_hdr(&xdrs, &hdr)) {
        LS_ERR("xdr_pack_hdr failed");
        xdr_destroy(&xdrs);
        shutdown_tcp_chan(ch_id);
        return;
    }

    LS_DEBUG("protocol=%s", proto_to_str(hdr.operation));

    switch (hdr.operation) {
    case LIM_GET_LOAD:
        get_load(&xdrs, ch_id);
        xdr_destroy(&xdrs);
        shutdown_tcp_chan(ch_id);
        break;
    case LIM_GET_HOSTS:
        get_hosts(&xdrs, ch_id);
        xdr_destroy(&xdrs);
        shutdown_tcp_chan(ch_id);
        break;
    default:
        LS_ERR("invalid operation %d", hdr.operation);
        xdr_destroy(&xdrs);
        shutdown_tcp_chan(ch_id);
        break;
    }
}

int tcp_message(int ch_id)
{
    if (chan_has_error(ch_id)) {
        LS_DEBUG("lost connection from %s", chan_addr_str(ch_id));
        shutdown_tcp_chan(ch_id);
        return -1;
    }

    tcp_dispatch(ch_id);

    return 0;
}

int tcp_accept(void)
{
    struct sockaddr_in from;

    int ch_id = chan_accept(tcp_chan, &from);
    if (ch_id < 0) {
        LS_ERR("%s: chan_accept() failed: %m", __func__);
        return -1;
    }

    char addr[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &from.sin_addr, addr, sizeof(addr));
    struct lim_node *n = ll_hash_search(&node_addr_hash, addr);
    if (n == NULL) {
        LS_ERR("rejected accept from unknown host %s", addr_to_str(&from));
        chan_close(ch_id);
        return -1;
    }

    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.u32 = ch_id;
    epoll_ctl(lim_efd, EPOLL_CTL_ADD, chan_sock(ch_id), &ev);

    return 0;
}
