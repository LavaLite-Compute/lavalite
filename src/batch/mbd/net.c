// Copyright (C) LavaLite Contributors
// GPL v2

#include <string.h>
#include <assert.h>
#include <syslog.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "base/lib/auth.h"
#include "batch/lib/wire.h"
#include "batch/lib/rpc.h"
#include "base/lib/ll.conf.h"
#include "batch/mbd/mbd.h"

int valid_batch_op(int op)
{
    switch (op) {
    case BATCH_JOB_SUBMIT:
    case BATCH_JOB_SUBMIT_ACK:
    case BATCH_JOB_SIGNAL:
    case BATCH_JOB_SIGNAL_ACK:
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
    case BATCH_NEW_JOB:
    case BATCH_NEW_JOB_REPLY:
    case BATCH_NEW_JOB_REPLY_ACK:
    case BATCH_JOB_EXECUTE:
    case BATCH_JOB_EXECUTE_ACK:
    case BATCH_JOB_FINISH:
    case BATCH_JOB_FINISH_ACK:
    case BATCH_COMPACT_ACK:
    case BATCH_SBD_JOB_SIGNAL:
    case BATCH_SBD_JOB_SIGNAL_REPLY:
        return 1;
    default:
        return 0;
    }
}

static void route(int chan_id)
{
    struct chan_buffer *buf;
    struct protocol_header hdr;
    XDR xdrs;

    if (chan_has_error(chan_id)) {
        LS_DEBUG("channel=%d from=%s closed connection",
                 chan_id, chan_addr_str(chan_id));
        chan_shutdown(chan_id);
        return;
    }

    if (chan_dequeue(chan_id, &buf) < 0) {
        LS_ERR("chan_dequeue failed chan_id=%d", chan_id);
        chan_shutdown(chan_id);
        return;
    }

    xdrmem_create(&xdrs, buf->data, buf->len, XDR_DECODE);
    if (!xdr_pack_hdr(&xdrs, &hdr)) {
        LS_ERR("xdr_pack_hdr failed chan_id=%d", chan_id);
        xdr_destroy(&xdrs);
        chan_free_buf(buf);
        chan_shutdown(chan_id);
        return;
    }

    /* validate opcode and version early */
    if (!valid_batch_op(hdr.operation)) {
        LS_ERR("invalid opcode=%d from=%s", hdr.operation,
               chan_addr_str(chan_id));
        xdr_destroy(&xdrs);
        chan_free_buf(buf);
        chan_shutdown(chan_id);
        return;
    }

    if (auth_verify_header(&hdr) < 0) {
        LS_ERR("failed validate header opcode=%s from=%s",
               batch_op_str(hdr.operation), chan_addr_str(chan_id));
        xdr_destroy(&xdrs);
        chan_free_buf(buf);
        chan_shutdown(chan_id);
        return;
    }

    if (hdr.version != CURRENT_PROTOCOL_VERSION) {
        LS_ERR("unsupported version=0x%x from=%s",
               hdr.version, chan_addr_str(chan_id));
        xdr_destroy(&xdrs);
        chan_free_buf(buf);
        chan_shutdown(chan_id);
        return;
    }

    LS_DEBUG("chan_id=%d protocol=%s", chan_id, batch_op_str(hdr.operation));

    switch (hdr.operation) {
    case BATCH_JOB_SUBMIT:
        if (job_register(&xdrs, chan_id) < 0)
            chan_shutdown(chan_id);
        break;
    case BATCH_JOB_SIGNAL:
        if (job_signal(&xdrs, chan_id) < 0)
            chan_shutdown(chan_id);
        break;
    case BATCH_GROUP_INFO:
        if (host_group_info(&xdrs, chan_id) < 0)
            chan_shutdown(chan_id);
        break;
    case BATCH_QUEUE_INFO:
        if (queue_info(&xdrs, chan_id) < 0)
            chan_shutdown(chan_id);
        break;
    case BATCH_JOB_INFO:
        if (job_info(&xdrs, chan_id) < 0)
            chan_shutdown(chan_id);
        break;
    case BATCH_HOST_INFO:
        if (host_info(&xdrs, chan_id) < 0)
            chan_shutdown(chan_id);
        break;
    case BATCH_SBD_REGISTER:
        if (mbd_sbd_register(&xdrs, chan_id) < 0)
            chan_shutdown(chan_id);
        break;
    case BATCH_COMPACT_DONE:
    case BATCH_COMPACT_FAILED:
        if (compact_done(&xdrs, chan_id) < 0)
            chan_shutdown(chan_id);
        break;
    }

    xdr_destroy(&xdrs);
    chan_free_buf(buf);
}

void mbd_message(int chan_id)
{
    char key[LL_BUFSIZ_32];

    LS_DEBUG("chan_id=%d sock=%d chan_events=%d",
             chan_id, channels[chan_id].sock, channels[chan_id].chan_events);

    snprintf(key, sizeof(key), "%d", chan_id);
    struct mbd_host *n = ll_hash_search(&sbd_chan_hash, key);
    if (n != NULL) {
        LS_DEBUG("the client is an sbd %s", n->net.name);
        assert(n->sbd_chan == chan_id);
        mbd_sbd_route(n);
        return;
    }

    route(chan_id);
}

int network_init(void)
{
    struct epoll_event ev;

    mbd_efd = epoll_create1(0);
    if (mbd_efd < 0) {
        LS_ERR("epoll_create1 failed");
        return -1;
    }

    chan_init();
    if (! ll_atoi(ll_params[LL_MBD_PORT].val, (int *)&mbd_port)) {
        LS_ERRX("cannot convert to int LL_MBD_PORT=%s",
                ll_params[LL_MBD_PORT].val);
        close(mbd_efd);
        return -1;
    }

    chan_mbd = chan_tcp_server(mbd_port);
    if (chan_mbd < 0) {
        LS_ERR("chan_tcp_server failed port=%u", mbd_port);
        close(mbd_efd);
        mbd_efd = -1;
        return -1;
    }

    memset(&ev, 0, sizeof(ev));
    ev.events = EPOLLIN | EPOLLRDHUP;
    ev.data.u32 = chan_mbd;
    if (epoll_ctl(mbd_efd, EPOLL_CTL_ADD, chan_sock(chan_mbd), &ev) < 0) {
        LS_ERR("epoll_ctl add chan_mbd=%d failed", chan_mbd);
        chan_close(chan_mbd);
        chan_mbd = -1;
        close(mbd_efd);
        mbd_efd = -1;
        return -1;
    }

    // default 5 secs in mbd.h
    chan_timer = chan_create_timer(sched_timer);
    if (chan_timer < 0) {
        LS_ERR("chan_create_timer=%d failed", sched_timer);
        chan_close(chan_mbd);
        chan_mbd = -1;
        close(mbd_efd);
        mbd_efd = -1;
        return -1;
    }

    memset(&ev, 0, sizeof(ev));
    ev.events = EPOLLIN | EPOLLRDHUP;
    ev.data.u32 = chan_timer;
    if (epoll_ctl(mbd_efd, EPOLL_CTL_ADD, chan_sock(chan_timer), &ev) < 0) {
        LS_ERR("epoll_ctl add sched_timer=%d failed", chan_timer);
        chan_close(sched_timer);
        chan_close(chan_mbd);
        chan_mbd = -1;
        close(mbd_efd);
        mbd_efd = -1;
        return -1;
    }

    return 0;
}

int mbd_accept(int chan_id)
{
    struct sockaddr_in from;
    memset(&from, 0, sizeof(from));
    int ch_accept = chan_accept(chan_id, &from);
    if (ch_accept < 0) {
        LS_ERR("%s: chan_accept failed: %m", __func__);
        return -1;
    }

    char addr[INET_ADDRSTRLEN];
    if (inet_ntop(AF_INET, &from.sin_addr, addr, sizeof(addr)) == NULL) {
        LS_ERR("inet_ntop failed chan=%d: %m", ch_accept);
        chan_close(ch_accept);
        return -1;
    }

    struct mbd_node *n = ll_hash_search(&host_addr_hash, addr);
    if (n == NULL) {
        LS_ERR("rejected accept from unknown host %s", addr);
        chan_close(ch_accept);
        return -1;
    }

    struct epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.events = EPOLLIN | EPOLLRDHUP;
    ev.data.u32 = ch_accept;
    if (epoll_ctl(mbd_efd, EPOLL_CTL_ADD, chan_sock(ch_accept), &ev) < 0) {
        LS_ERR("epoll_ctl add chan=%d failed", ch_accept);
        chan_close(ch_accept);
        return -1;
    }

    return ch_accept;
}

void chan_shutdown(int chan_id)
{
    epoll_ctl(mbd_efd, EPOLL_CTL_DEL, chan_sock(chan_id), NULL);
    chan_close(chan_id);
}

int32_t enqueue_payload(int chan_id, struct protocol_header *hdr,
                        void *payload, size_t siz, bool_t (*xdr_func)())
{
    struct chan_buffer *buf;
    XDR xdrs;

    if (chan_alloc_buf(&buf, siz) < 0) {
        LS_ERR("chan_alloc_buf failed op=%d siz=%ld",
               hdr->operation, (long)siz);
        return -1;
    }

    xdrmem_create(&xdrs, buf->data, siz, XDR_ENCODE);

    if (! ll_encode_msg(&xdrs, (char *)payload, xdr_func, hdr)) {
        LS_ERRX("ll_encode_msg failed op=%d", hdr->operation);
        xdr_destroy(&xdrs);
        chan_free_buf(buf);
        return -1;
    }

    buf->len = (size_t)xdr_getpos(&xdrs);
    xdr_destroy(&xdrs);

    if (chan_enqueue(chan_id, buf) < 0) {
        LS_ERR("chan_enqueue failed op=%d len=%d",
               hdr->operation, (int)buf->len);
        chan_free_buf(buf);
        return -1;
    }

    if (chan_set_write_interest(chan_id, mbd_efd, 1) < 0) {
        LS_ERR("chan_set_write_interest failed");
        chan_free_buf(buf);
        return -1;
    }

    return 0;
}

int32_t enqueue_header(int chan_id, int operation, int status)
{
    struct chan_buffer *buf;

    if (chan_alloc_buf(&buf, LL_BUFSIZ_256) < 0) {
        LS_ERR("chan_alloc_buf failed op=%d", operation);
        return -1;
    }

    struct protocol_header hdr;
    init_protocol_header(&hdr);
    hdr.operation = operation;
    hdr.status    = status;
    hdr.length    = 0;

    XDR xdrs;
    xdrmem_create(&xdrs, buf->data, LL_BUFSIZ_256, XDR_ENCODE);
    if (!xdr_pack_hdr(&xdrs, &hdr)) {
        LS_ERRX("xdr_pack_hdr failed op=%d", operation);
        xdr_destroy(&xdrs);
        chan_free_buf(buf);
        return -1;
    }
    buf->len = (size_t)xdr_getpos(&xdrs);
    xdr_destroy(&xdrs);

    if (chan_enqueue(chan_id, buf) < 0) {
        LS_ERR("chan_enqueue failed op=%d", operation);
        chan_free_buf(buf);
        return -1;
    }
    if (chan_set_write_interest(chan_id, mbd_efd, 1) < 0) {
        LS_ERR("chan_set_write_interest failed op=%d", operation);
        chan_free_buf(buf);
        return -1;
    }
    return 0;
}
