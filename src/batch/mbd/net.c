// Copyright (C) LavaLite Contributors
// GPL v2
s
static int valid_batch_op(int op)
{
    switch (op) {
    case BATCH_JOB_SUB:
    case BATCH_JOB_SUB_ACK:
    case BATCH_JOB_SIG:
    case BATCH_JOB_SIG_ACK:
    case BATCH_JOB_INFO:
    case BATCH_JOB_INFO_ACK:
    case BATCH_HOST_INFO:
    case BATCH_HOST_INFO_ACK:
    case BATCH_QUEUE_INFO:
    case BATCH_QUEUE_INFO_ACK:
    case BATCH_GROUP_INFO:
    case BATCH_GROUP_INFO_ACK:
    case BATCH_SBD_REGISTER:
    case BATCH_SBD_REGISTER_ACK:
    case BATCH_COMPACT_DONE:
    case BATCH_COMPACT_FAILED:
    case BATCH_COMPACT_ACK:
        return 1;
    default:
        return 0;
    }
}

static void route(int ch_id)
{
    struct chan_buffer *buf;
    struct protocol_header hdr;
    XDR xdrs;

    if (chan_has_error(ch_id)) {
        LS_DEBUG("channel=%d from=%s closed connection",
                 ch_id, chan_addr_str(ch_id));
        shutdown_chan(ch_id);
        return;
    }

    if (chan_dequeue(ch_id, &buf) < 0) {
        LS_ERR("chan_dequeue failed ch_id=%d", ch_id);
        shutdown_chan(ch_id);
        return;
    }

    xdrmem_create(&xdrs, buf->data, buf->len, XDR_DECODE);
    if (!xdr_pack_hdr(&xdrs, &hdr)) {
        LS_ERR("xdr_pack_hdr failed ch_id=%d", ch_id);
        xdr_destroy(&xdrs);
        chan_free_buf(buf);
        shutdown_chan(ch_id);
        return;
    }

    /* validate opcode and version early */
    if (!valid_batch_op(hdr.operation)) {
        LS_ERR("invalid opcode=%d from=%s", hdr.operation, chan_addr_str(ch_id));
        xdr_destroy(&xdrs);
        chan_free_buf(buf);
        shutdown_chan(ch_id);
        return;
    }

    if (hdr.version != CURRENT_PROTOCOL_VERSION) {
        LS_ERR("unsupported version=0x%x from=%s",
               hdr.version, chan_addr_str(ch_id));
        xdr_destroy(&xdrs);
        chan_free_buf(buf);
        shutdown_chan(ch_id);
        return;
    }

    LS_DEBUG("ch_id=%d protocol=%s", ch_id, proto_to_str(hdr.operation));

    switch (hdr.operation) {
    case BATCH_JOB_SUB:
        job_submit(&xdrs, ch_id);
        break;
    case BATCH_JOB_SIG:
        job_signal(&xdrs, ch_id);
        break;
    case BATCH_GROUP_INFO:
        host_group_info(&xdrs, ch_id);
        break;
    case BATCH_QUEUE_INFO:
        queue_info(&xdrs, ch_id);
        break;
    case BATCH_JOB_INFO:
        job_info(&xdrs, ch_id);
        break;
    case BATCH_HOST_INFO:
        host_info(&xdrs, ch_id);
        break;
    case BATCH_SBD_REGISTER:
        sbd_register(&xdrs, ch_id);
        break;
    case BATCH_COMPACT_DONE:
    case BATCH_COMPACT_FAILED:
        compact_done(&xdrs, ch_id);
        break;
    default:
        LS_ERR("unknown request=%d from=%s",
               hdr.operation, chan_addr_str(ch_id));
        if (hdr.version != CURRENT_PROTOCOL_VERSION)
            LS_ERR("unsupported protocol version=0x%x", hdr.version);
        break;
    }

    xdr_destroy(&xdrs);
    chan_free_buf(buf);
}

int network_init(void)
{
    struct epoll_event ev;

    mbd_efd = epoll_create1(0);
    if (mbd_efd < 0) {
        LS_ERR("epoll_create1 failed");
        return -1;
    }

    mbd_chan = chan_tcp_server(mbd_port);
    if (mbd_chan < 0) {
        LS_ERR("chan_tcp_server failed port=%u", mbd_port);
        close(mbd_efd);
        mbd_efd = -1;
        return -1;
    }

    memset(&ev, 0, sizeof(ev));
    ev.events = EPOLLIN | EPOLLRDHUP;
    ev.data.u32 = mbd_chan;
    if (epoll_ctl(mbd_efd, EPOLL_CTL_ADD, chan_sock(mbd_chan), &ev) < 0) {
        LS_ERR("epoll_ctl add mbd_chan=%d failed", mbd_chan);
        chan_close(mbd_chan);
        mbd_chan = -1;
        close(mbd_efd);
        mbd_efd = -1;
        return -1;
    }

    sched_timer = chan_create_timer(timer_sched);
    if (sched_timer < 0) {
        LS_ERR("chan_create_timer failed");
        epoll_ctl(mbd_efd, EPOLL_CTL_DEL, chan_sock(mbd_chan), NULL);
        chan_close(mbd_chan);
        mbd_chan = -1;
        close(mbd_efd);
        mbd_efd = -1;
        return -1;
    }

    memset(&ev, 0, sizeof(ev));
    ev.events = EPOLLIN | EPOLLRDHUP;
    ev.data.u32 = sched_timer;
    if (epoll_ctl(mbd_efd, EPOLL_CTL_ADD, chan_sock(sched_timer), &ev) < 0) {
        LS_ERR("epoll_ctl add sched_timer=%d failed", sched_timer);
        chan_close(sched_timer);
        sched_timer = -1;
        epoll_ctl(mbd_efd, EPOLL_CTL_DEL, chan_sock(mbd_chan), NULL);
        chan_close(mbd_chan);
        mbd_chan = -1;
        close(mbd_efd);
        mbd_efd = -1;
        return -1;
    }

    return 0;
}

int mbd_accept(int ch_id)
{
    struct sockaddr_in from;
    struct epoll_event ev;
    struct mbd_node *n;
    char addr[INET_ADDRSTRLEN];
    int accept_id;

    memset(&from, 0, sizeof(from));
    accept_id = chan_accept(ch_id, &from);
    if (accept_id < 0) {
        LS_ERR("%s: chan_accept failed: %m", __func__);
        return -1;
    }

    if (inet_ntop(AF_INET, &from.sin_addr, addr, sizeof(addr)) == NULL) {
        LS_ERR("inet_ntop failed chan=%d: %m", accept_id);
        chan_close(accept_id);
        return -1;
    }

    n = ll_hash_search(&node_addr_hash, addr);
    if (n == NULL) {
        LS_ERR("rejected accept from unknown host %s", addr);
        chan_close(accept_id);
        return -1;
    }

    memset(&ev, 0, sizeof(ev));
    ev.events = EPOLLIN | EPOLLRDHUP;
    ev.data.u32 = accept_id;
    if (epoll_ctl(mbd_efd, EPOLL_CTL_ADD, chan_sock(accept_id), &ev) < 0) {
        LS_ERR("epoll_ctl add chan=%d failed", accept_id);
        chan_close(accept_id);
        return -1;
    }

    return accept_id;
}

void mbd_message(int ch_id)
{
    char key[LL_BUFSIZ_32];
    struct mbd_node *n;

    LS_DEBUG("ch_id=%d sock=%d events=0x%x",
             ch_id, channels[ch_id].sock, channels[ch_id].chan_events);

    snprintf(key, sizeof(key), "%d", ch_id);
    n = ll_hash_search(&sbd_by_chan, key);
    if (n != NULL) {
        LS_DEBUG("the client is an sbd %s", n->net->name);
        assert(n->sbd_chan == ch_id);
        sbd_route(n);
        return;
    }

    route(ch_id);
}

void shutdown_chan(int ch_id)
{
    epoll_ctl(mbd_efd, EPOLL_CTL_DEL, chan_sock(ch_id), NULL);
    chan_close(ch_id);
}

int32_t enqueue_payload(int ch_id, struct protocol_header *hdr,
                        void *payload, size_t siz, bool (*xdr_func)())
{
    struct chan_buffer *buf;
    XDR xdrs;

    if (hdr == NULL) {
        LS_ERR("enqueue_payload: NULL header");
        return -1;
    }

    if (chan_alloc_buf(&buf, siz) < 0) {
        LS_ERR("chan_alloc_buf failed op=%d siz=%ld",
               hdr->operation, (long)siz);
        return -1;
    }

    xdrmem_create(&xdrs, buf->data, siz, XDR_ENCODE);

    if (!ll_encode_msg(&xdrs, (char *)payload, xdr_func, hdr)) {
        LS_ERR("ll_encode_msg failed op=%d", hdr->operation);
        xdr_destroy(&xdrs);
        chan_free_buf(buf);
        return -1;
    }

    buf->len = (size_t)XDR_GETPOS(&xdrs);
    xdr_destroy(&xdrs);

    if (chan_enqueue(ch_id, buf) < 0) {
        LS_ERR("chan_enqueue failed op=%d len=%d",
               hdr->operation, (int)buf->len);
        chan_free_buf(buf);
        return -1;
    }

    chan_set_write_interest(mbd_efd, ch_id, 1);

    return 0;
}
