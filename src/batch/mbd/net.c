// Copyright (C) LavaLite Contributors
// GPL v2
int network_init(void)
{
    mbd_efd = epoll_create1(0);
    if (mbd_efd < 0) {
        LS_ERR("epoll_create1 failed");
        return -1;
    }

    mbd_chan = chan_tcp_server(lim_port);
    if (mbd_chan < 0) {
        LS_ERR("chan_tcp_server failed");
        close(mbd_fd);
        return -1;
    }

    if (epoll_ctl(mbd_efd, EPOLL_CTL_ADD, chan_sock(mbd_chan), &ev) < 0) {
        LS_ERR("epoll_ctl mbd_chan=%d failed", mbd_chan);
        close(mbd_efd);
        chan_close(mbd_chan);
        return -1;
    }

    sched_timer = chan_create_timer(5);
    if (sched_timer < 0) {
        close(mbd_efd);
        chan_close(mbd_chan);
        return -1;
    }

    if (epoll_ctl(mbd_efd, EPOLL_CTL_ADD, chan_sock(sched_timer), &ev) < 0) {
        LS_ERR("epoll_ctl sched_timer=%d failed", sched_timer);
        close(mbd_efd);
        chan_close(mbd_chan);
        chan_close(sched_timer);
        return -1;
    }

    return 0;
}

int mbd_accept(int ch_id)
{
    struct sockaddr_in from;

    int accept_id = chan_accept(chan_id, &from);
    if (ch_id < 0) {
        LS_ERR("%s: chan_accept() failed: %m", __func__);
        return -1;
    }

    char addr[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &from.sin_addr, addr, sizeof(addr));
    struct mbd_node *n = ll_hash_search(&node_addr_hash, addr);
    if (n == NULL) {
        LS_ERR("rejected accept from unknown host %s", addr_to_str(&from));
        chan_close(accept_id);
        return -1;
    }

    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLRDHUP;
    ev.data.u32 = ch_id;
    if (epoll_ctl(mbd_efd, EPOLL_CTL_ADD, chan_sock(ch_id), &ev) < 0) {
        LS_ERR("epoll_ctl chan=%d failed");
        chan_close(accept_id);
        return -1;
    }

    return accept_id;
}

void mbd_message(int ch_id)
{
    LS_DEBUG("ch_id=%d sock=%d events=0x%x",
             ch_id, channels[ch_id].sock, channels[ch_id].chan_events);

    // See if it is an sbd node
    char key[LL_BUFSIZ_32];
    struct hData *host_data;

    snprintf(key, sizeof(key), "%d", ch_id);
    host_data = ll_hash_search(&hdata_by_chan, key);
    // Here its a little assymetric with non sbd client
    // handling. If there is an exception on the channel with
    // sbd we have to handle it inside this call because beside
    // just shutting down the client we have to do other operations
    // like clean up caches, jobs states etc
    // so the asymmetry is intentional and correct.
    if (host_data) {
        // handle the sbd client
        LS_DEBUG("the client is an sbd %s", host_data->host);
        // dispatch the payload decoder
        sbd_dispatch(host_data->sbd_node);
        return;
    }

    client_dispatch(client);
    // No client found for this channel id: internal inconsistency.
    LS_ERR("no client found for chanfd=%d", ch_id);
}

int client_dispatch(int ch_id)
{
    if (chan_has_error(ch_id)) {
        LS_DEBUG("channel=%d from=%s closed connection", ch_id,
                 chan_addr_str(ch_id));
        shutdown_tcp_chan(ch_id);
        return -1;
    }

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
    case BATCH_JOB_SUB:
        job_submit(&xdrs, ch_id);
        break;
    case BATCH_JOB_SIG:
        job_signal(&xdrs, ch_id);
        xdr_destroy(&xdrs);
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
        int cc = sbd_register(&xdrs, ch_id);
        break;
    case BATCH_COMPACT_DONE:
    case BATCH_COMPACT_FAILED:
        compact_done(&xdrs, ch_id, &req_hdr);
        break;
    default:
        LS_ERR("unknown request=%d from=%s", req_hdr.operation,
               chan_addr_str(ch_id));
        if (req_hdr.version <= CURRENT_PROTOCOL_VERSION)
            LS_ERRX("protocol version=0x%x error", hdr.version);
            break;
    }

    xdr_destroy(&xdrs);
    chan_free_buf(buf);

    return 0;
}
