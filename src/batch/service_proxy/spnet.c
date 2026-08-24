/*
 * Copyright (C) LavaLite Contributors
 * GPL v2
 */

#include <string.h>
#include <syslog.h>
#include <assert.h>
#include <errno.h>

#include "base/lib/auth.h"
#include "base/lib/ll.conf.h"
#include "base/lib/ll.syslog.h"
#include "base/lib/ll.channel.h"
#include "base/lib/ll.sys.h"
#include "batch/lib/rpc.h"
#include "batch/lib/wire.h"
#include "batch/service_proxy/service_proxy.h"

static int32_t sp_enqueue_payload(int chan_id, struct protocol_header *hdr,
                                  void *payload, size_t siz,
                                  bool_t (*xdr_func)())
{
    struct chan_buffer *buf;
    XDR xdrs;

    if (chan_alloc_buf(&buf, siz) < 0) {
        LL_ERR("chan_alloc_buf failed op=%d siz=%ld", hdr->operation,
               (long) siz);
        return -1;
    }

    xdrmem_create(&xdrs, buf->data, siz, XDR_ENCODE);

    if (!ll_encode_msg(&xdrs, (char *) payload, xdr_func, hdr)) {
        LL_ERRX("ll_encode_msg failed op=%d", hdr->operation);
        xdr_destroy(&xdrs);
        chan_free_buf(buf);
        return -1;
    }

    buf->len = (size_t) xdr_getpos(&xdrs);
    xdr_destroy(&xdrs);

    if (chan_enqueue(chan_id, buf) < 0) {
        LL_ERR("chan_enqueue failed op=%d len=%d", hdr->operation,
               (int) buf->len);
        chan_free_buf(buf);
        return -1;
    }

    if (chan_set_write_interest(chan_id, sp_efd, 1) < 0) {
        LL_ERR("chan_set_write_interest failed");
        chan_free_buf(buf);
        return -1;
    }

    return 0;
}

// Create a permanent channel to mbd, then switch it to nonblocking I/O.
int sp_mbd_connect(void)
{
    int port;
    ll_atoi(ll_params[LL_MBD_PORT].val, &port);

    sp_mbd_chan = chan_tcp_client();
    if (sp_mbd_chan < 0) {
        LL_ERR("failed to get channel to mbd");
        return -1;
    }
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    get_host_addrv4(&mbd_node, &addr);
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t) port);

    // 3s: datacenter LAN; if mbd doesn't answer it's down
    if (chan_connect(sp_mbd_chan, &addr, 3) < 0) {
        LL_ERR("cannot connect to mbd on chan=%d", sp_mbd_chan);
        if (errno == EPROTO)
            LL_ERR("rejected TCP self-connect while connecting to mbd");
        sp_chan_shutdown(sp_mbd_chan);
        sp_mbd_chan = -1;
        return -1;
    }

    // Now we set the socket as non blocking for all our
    // communication with mbd
    if (io_non_block(chan_sock(sp_mbd_chan)) < 0) {
        LL_ERR("failed to set mbd socket nonblocking");
        sp_chan_shutdown(sp_mbd_chan);
        sp_mbd_chan = -1;
        return -1;
    }

    struct epoll_event ev = {.events = EPOLLIN,
                             .data.u32 = (uint32_t) sp_mbd_chan};

    if (epoll_ctl(sp_efd, EPOLL_CTL_ADD, chan_sock(sp_mbd_chan), &ev) < 0) {
        LL_ERR("epoll_ctl() failed to add mbd connection to epoll");
        sp_chan_shutdown(sp_mbd_chan);
        sp_mbd_chan = -1;
        return -1;
    }
    LL_INFO("connected to mbd chan=%d", sp_mbd_chan);

    return sp_mbd_chan;
}

void sp_mbd_link_down(void)
{
    if (sp_mbd_chan >= 0)
        chan_close(sp_mbd_chan);

    sp_mbd_chan = -1;

    /* Unlike sbd, service_proxy has no per-job on-disk state to rewrite
     * here -- the registry it maintains (svc_id -> forwarding state) is
     * rebuilt from mbd's RESYNC on reconnect, not from local storage. */

    LL_ERRX("mbd link down");
}

// Check if mbd is connected
bool_t sp_mbd_link_ready(void)
{
    return (sp_mbd_chan >= 0);
}

int sp_register(void)
{
    if (!sp_mbd_link_ready())
        return -1;

    char host[MAXHOSTNAMELEN];

    if (gethostname(host, sizeof(host)) < 0) {
        LL_ERR("cannot get local hostname: %m");
        abort();
    }

    struct wire_sp_register reg;
    memset(&reg, 0, sizeof(reg));
    ll_strlcpy(reg.hostname, host, sizeof(reg.hostname));

    if (sp_send_msg(BATCH_SP_REGISTER, MBD_OK, &reg, LL_BUFSIZ_1K,
                    xdr_wire_sp_register) < 0) {
        LL_ERR("spd on host=%s registration failed", host);
        return -1;
    }

    LL_INFO("spd registered I am host=%s", host);

    return 0;
}

/*
 * BATCH_SVC_ADD handler. TODO: real port allocation -- bind() over
 * SP_SVC_PORT_MIN..SP_SVC_PORT_MAX and hold the socket open, which is
 * the entire reason this design moved port ownership here instead of
 * mbd doing a probe-then-close. Stubbed with a placeholder port for
 * now so the ADD/ADD_ACK round trip and mbd's job_prepare()/
 * job_commit() path can be exercised end to end while the real bind()/
 * forwarding table gets built -- same "prototype now, harden later"
 * call already made for the port range itself.
 */
static int sp_svc_add(XDR *xdrs, const struct protocol_header *hdr)
{
    (void) hdr;

    struct wire_svc_add req;
    memset(&req, 0, sizeof(req));

    if (!xdr_wire_svc_add(xdrs, &req)) {
        LL_ERR("sp_svc_add: xdr decode failed");
        return -1;
    }

    static int next_port = SP_SVC_PORT_MIN;
    int port = next_port++;
    if (next_port > SP_SVC_PORT_MAX)
        next_port = SP_SVC_PORT_MIN;

    LL_INFO("sp_svc_add: svc_id=%s port=%d (stub, no real bind yet)",
            req.svc_id, port);

    struct wire_svc_add_ack ack;
    memset(&ack, 0, sizeof(ack));
    ll_strlcpy(ack.svc_id, req.svc_id, sizeof(ack.svc_id));
    // This is the port the proxy is bound to on behalf of the client
    ack.port = port;

    int cc = sp_send_msg(BATCH_SVC_ADD_ACK, MBD_OK, &ack, LL_BUFSIZ_1K,
                         xdr_wire_svc_add_ack);
    if (cc < 0) {
        LL_ERR("sp_send_msg failed");
        return -1;
    }

    return 0;
}

/*
 * BATCH_SVC_UPDATE handler. TODO: apply run_host to the real
 * forwarding table once it exists -- stub just acks for now.
 */
static int sp_svc_update(XDR *xdrs, const struct protocol_header *hdr)
{
    (void) hdr;

    struct wire_svc_update req;
    memset(&req, 0, sizeof(req));

    if (!xdr_wire_svc_update(xdrs, &req)) {
        LL_ERR("sp_svc_update: xdr decode failed");
        return -1;
    }

    LL_INFO("sp_svc_update: svc_id=%s run_host=%s (no forwarding table yet)",
            req.svc_id, req.run_host);

    struct wire_svc_update_ack ack;
    memset(&ack, 0, sizeof(ack));
    ll_strlcpy(ack.svc_id, req.svc_id, sizeof(ack.svc_id));

    return sp_send_msg(BATCH_SVC_UPDATE_ACK, MBD_OK, &ack, LL_BUFSIZ_1K,
                       xdr_wire_svc_update_ack);
}

/*
 * BATCH_SVC_REMOVE handler. TODO: close the real listening socket and
 * free the port once the forwarding table exists -- stub just acks.
 */
static int sp_svc_remove(XDR *xdrs, const struct protocol_header *hdr)
{
    (void) hdr;

    struct wire_svc_remove req;
    memset(&req, 0, sizeof(req));

    if (!xdr_wire_svc_remove(xdrs, &req)) {
        LL_ERR("sp_svc_remove: xdr decode failed");
        return -1;
    }

    LL_INFO("sp_svc_remove: svc_id=%s (no forwarding table yet, nothing to "
            "free)", req.svc_id);

    struct wire_svc_remove_ack ack;
    memset(&ack, 0, sizeof(ack));
    ll_strlcpy(ack.svc_id, req.svc_id, sizeof(ack.svc_id));

    return sp_send_msg(BATCH_SVC_REMOVE_ACK, MBD_OK, &ack, LL_BUFSIZ_1K,
                       xdr_wire_svc_remove_ack);
}

void sp_chan_shutdown(int chan_id)
{
    epoll_ctl(sp_efd, EPOLL_CTL_DEL, chan_sock(chan_id), NULL);
    chan_close(chan_id);
}

// the chan_id in input is the channel we have opened with mbatchd
//
int sp_mbd_route(int chan_id)
{
    struct chan_data *chan = &channels[chan_id];

    if (chan->chan_events == CHAN_EPOLLERR) {
        LL_ERRX("lost connection with mbd on channel=%d socket err=%d",
                chan_id, chan_sock_error(chan_id));
        sp_mbd_link_down();
        return -1;
    }

    if (chan->chan_events != CHAN_EPOLLIN) {
        // channel is not ready
        return 0;
    }

    // Get the packet header from the channel first
    struct chan_buffer *buf;
    if (chan_dequeue(chan_id, &buf) < 0) {
        LL_ERR("chan_dequeue() failed");
        return -1;
    }

    if (!buf || buf->len < PACKET_HEADER_SIZE) {
        LL_ERR("short header from mbd on channel=%d: len=%d", chan_id,
               buf ? buf->len : 0);
        chan_free_buf(buf);
        sp_mbd_link_down();
        return -1;
    }

    XDR xdrs;
    struct protocol_header hdr;
    // Allocate the buffer data based on what was sent
    xdrmem_create(&xdrs, buf->data, buf->len, XDR_DECODE);
    if (!xdr_pack_hdr(&xdrs, &hdr)) {
        LL_ERR("xdr_pack_hdr failed");
        xdr_destroy(&xdrs);
        return -1;
    }

    if (auth_verify_header(&hdr) < 0) {
        LL_ERR("failed validate header opcode=%s from=%s",
               batch_op_str(hdr.operation), chan_addr_str(chan_id));
        xdr_destroy(&xdrs);
        chan_free_buf(buf);
        sp_mbd_link_down();
        return -1;
    }

    LL_DEBUG("mbd requesting operation=%s", batch_op_str(hdr.operation));

    // service_proxy handler
    switch (hdr.operation) {
    case BATCH_SP_REGISTER_ACK:
        sp_register_ack(&xdrs);
        break;
    case BATCH_SVC_ADD:
        sp_svc_add(&xdrs, &hdr);
        break;
    case BATCH_SVC_UPDATE:
        sp_svc_update(&xdrs, &hdr);
        break;
    case BATCH_SVC_REMOVE:
        sp_svc_remove(&xdrs, &hdr);
        break;
    default:
        LL_ERRX("unknown protocol operation=%s on chan=%d",
                batch_op_str(hdr.operation), chan_id);
        sp_mbd_link_down();
        break;
    }

    xdr_destroy(&xdrs);
    chan_free_buf(buf);

    return 0;
}

void sp_register_ack(XDR *xdrs)
{
    struct wire_sp_register reg_ack;
    memset(&reg_ack, 0, sizeof(struct wire_sp_register));

    if (!xdr_wire_sp_register(xdrs, &reg_ack)) {
        LL_ERR("xdr_wire_sp_register decode failed");
        sp_mbd_link_down();
        return;
    }

    LL_INFO("mbd acked registration");

    /* TODO once RESYNC exists: mbd may push the full service registry
     * here (ADD per live instance) instead of a bare ack, mirroring how
     * sbd_register_ack() reconciles job state against mbd's view on
     * reconnect. */
}

int sp_send_msg(int32_t op, int32_t status, void *payload, size_t siz,
               bool_t (*xdr_func)())
{
    struct protocol_header hdr;

    init_protocol_header(&hdr);
    hdr.operation = op;
    hdr.status = status;

    if (auth_sign_header(&hdr) < 0) {
        LL_ERR("auth_sign_header failed op=%d", op);
        return -1;
    }

    int cc = sp_enqueue_payload(sp_mbd_chan, &hdr, payload, siz, xdr_func);
    if (cc < 0) {
        LL_ERR("sp_enqueue_payload failed closing chan=%d to mbd",
               sp_mbd_chan);
        sp_mbd_link_down();
        return -1;
    }

    return 0;
}
