/*
 * Copyright (C) LavaLite Contributors
 * GPL v2
 */

#include <string.h>
#include <syslog.h>
#include <assert.h>
#include <errno.h>
#include <sys/socket.h>

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
 * Unlinks and frees relay -- both legs' epoll registrations and
 * sockets go with sp_chan_shutdown(), same teardown mbd's own
 * chan_shutdown() does. Called both from sp_svc_remove() (service
 * torn down, every relay on it goes too) and from sp_relay_pump()
 * below (one leg errored/closed mid-stream, the other leg is now
 * orphaned and must go with it -- a client disconnect must not leak
 * the backend leg forever).
 */
static void sp_relay_close(struct sp_relay *relay)
{
    if (relay->client_chan >= 0)
        sp_chan_shutdown(relay->client_chan);
    if (relay->backend_chan >= 0)
        sp_chan_shutdown(relay->backend_chan);

    ll_list_remove(&relay->inst->relays, &relay->ent);

    LL_DEBUG("sp_relay_close: svc_id=%s client_chan=%d backend_chan=%d "
            "closed", relay->inst->svc_id, relay->client_chan,
            relay->backend_chan);

    free(relay);
}

/*
 * bind() itself is the authoritative check -- no separate probe step,
 * same reasoning that moved port ownership from mbd to spd in the
 * first place: a probe-then-bind-later window is exactly the TOCTOU
 * the old mbd-side allocator had.
 */
static int sp_bind_port(int *out_port)
{
    static int next_port = SP_SVC_PORT_MIN;

    for (int tries = 0; tries <= (SP_SVC_PORT_MAX - SP_SVC_PORT_MIN);
         tries++) {
        int port = next_port++;
        if (next_port > SP_SVC_PORT_MAX)
            next_port = SP_SVC_PORT_MIN;

        int ch = chan_tcp_server((uint16_t) port);
        if (ch >= 0) {
            *out_port = port;
            return ch;
        }
    }

    return -1;
}

/*
 * BATCH_SVC_ADD handler. Binds a real listening socket, adds it to
 * spd's epoll set, and creates the sp_instance side-table entry that
 * BATCH_SVC_UPDATE/BATCH_SVC_REMOVE and accept-time lookups all key
 * off of. run_host stays empty (calloc'd zero) until UPDATE arrives --
 * no client can reach a backend that isn't dispatched yet, and
 * sp_relay_accept() checks for exactly that.
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

    struct wire_svc_add_ack ack;
    memset(&ack, 0, sizeof(ack));
    ll_strlcpy(ack.svc_id, req.svc_id, sizeof(ack.svc_id));

    int port;
    int listen_chan = sp_bind_port(&port);
    if (listen_chan < 0) {
        LL_ERRX("sp_svc_add: svc_id=%s no free port in %d..%d", req.svc_id,
                SP_SVC_PORT_MIN, SP_SVC_PORT_MAX);
        int cc = sp_send_msg(BATCH_SVC_ADD_ACK, ENOSPC, &ack, LL_BUFSIZ_1K,
                             xdr_wire_svc_add_ack);
        if (cc < 0) {
            LL_ERR("listen failed and sp_send_msg failed");
            return -1;
        }
        return 0;
    }

    struct epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.events = EPOLLIN;
    ev.data.u32 = (uint32_t) listen_chan;
    if (epoll_ctl(sp_efd, EPOLL_CTL_ADD, chan_sock(listen_chan), &ev) < 0) {
        LL_ERR("sp_svc_add: epoll_ctl failed svc_id=%s chan=%d", req.svc_id,
               listen_chan);
        int save_errno = errno;
        chan_close(listen_chan);
        int cc = sp_send_msg(BATCH_SVC_ADD_ACK, save_errno, &ack, LL_BUFSIZ_1K,
                             xdr_wire_svc_add_ack);
        if (cc < 0) {
            LL_ERR("epoll_ctl failed and sp_send_msg failed");
            return -1;
        }
        return 0;
    }

    struct sp_instance *inst = calloc(1, sizeof(*inst));
    if (inst == NULL) {
        LL_ERR("sp_svc_add: calloc failed svc_id=%s", req.svc_id);
        sp_chan_shutdown(listen_chan);
        sp_fatal(SP_FATAL_OOM);
        return -1; /* unreached -- sp_fatal() exits */
    }

    ll_strlcpy(inst->svc_id, req.svc_id, sizeof(inst->svc_id));
    inst->listen_chan = listen_chan;
    inst->port = port;
    inst->app_port = req.app_port;
    ll_list_init(&inst->relays);
    ll_list_append(&sp_instance_list, &inst->ent);

    LL_INFO("sp_svc_add: svc_id=%s bound port=%d app_port=%d chan=%d",
           inst->svc_id, port, inst->app_port, listen_chan);

    ack.port = port;
    int cc = sp_send_msg(BATCH_SVC_ADD_ACK, MBD_OK, &ack, LL_BUFSIZ_1K,
                         xdr_wire_svc_add_ack);
    if (cc < 0) {
        LL_ERR("sp_send_msg failed");
        return -1;
    }

    return 0;
}

static struct sp_instance *sp_find_instance_by_svc_id(const char *svc_id)
{
    for (struct ll_list_entry *e = sp_instance_list.head; e != NULL;
         e = e->next) {
        struct sp_instance *inst = (struct sp_instance *) e;
        if (strcmp(inst->svc_id, svc_id) == 0)
            return inst;
    }
    return NULL;
}

/*
 * BATCH_SVC_UPDATE handler. run_host is the one field that changes
 * across a service instance's life (restart/redispatch can move it) --
 * everything else on sp_instance is set once at ADD and never touched
 * again.
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

    struct wire_svc_update_ack ack;
    memset(&ack, 0, sizeof(ack));
    ll_strlcpy(ack.svc_id, req.svc_id, sizeof(ack.svc_id));

    struct sp_instance *inst = sp_find_instance_by_svc_id(req.svc_id);
    if (inst == NULL) {
        LL_ERRX("sp_svc_update: unknown svc_id=%s", req.svc_id);
        return sp_send_msg(BATCH_SVC_UPDATE_ACK, ESRCH, &ack, LL_BUFSIZ_1K,
                           xdr_wire_svc_update_ack);
    }

    ll_strlcpy(inst->run_host, req.run_host, sizeof(inst->run_host));

    LL_INFO("sp_svc_update: svc_id=%s run_host=%s", inst->svc_id,
           inst->run_host);

    int cc = sp_send_msg(BATCH_SVC_UPDATE_ACK, MBD_OK, &ack, LL_BUFSIZ_1K,
                         xdr_wire_svc_update_ack);
    if (cc < 0) {
        LL_ERR("sp_send_msg failed");
        return -1;
    }

    return 0;
}

/*
 * BATCH_SVC_REMOVE handler. Every active relay on this instance goes
 * first (sp_relay_close() unlinks itself from inst->relays as it
 * goes, hence saving `next` before closing -- standard
 * removal-while-iterating shape for an intrusive list), then the
 * listening socket itself, then the instance.
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

    struct wire_svc_remove_ack ack;
    memset(&ack, 0, sizeof(ack));
    ll_strlcpy(ack.svc_id, req.svc_id, sizeof(ack.svc_id));

    struct sp_instance *inst = sp_find_instance_by_svc_id(req.svc_id);
    if (inst == NULL) {
        LL_ERRX("sp_svc_remove: unknown svc_id=%s", req.svc_id);
        return sp_send_msg(BATCH_SVC_REMOVE_ACK, ESRCH, &ack, LL_BUFSIZ_1K,
                           xdr_wire_svc_remove_ack);
    }

    struct ll_list_entry *e = inst->relays.head;
    while (e != NULL) {
        struct ll_list_entry *next = e->next;
        sp_relay_close((struct sp_relay *) e);
        e = next;
    }

    sp_chan_shutdown(inst->listen_chan);
    ll_list_remove(&sp_instance_list, &inst->ent);

    LL_INFO("sp_svc_remove: svc_id=%s removed, port=%d freed", inst->svc_id,
           inst->port);

    free(inst);

    int cc = sp_send_msg(BATCH_SVC_REMOVE_ACK, MBD_OK, &ack, LL_BUFSIZ_1K,
                         xdr_wire_svc_remove_ack);
    if (cc < 0) {
        LL_ERR("sp_send_msg failed");
        return -1;
    }

    return 0;
}

void sp_chan_shutdown(int chan_id)
{
    epoll_ctl(sp_efd, EPOLL_CTL_DEL, chan_sock(chan_id), NULL);
    chan_close(chan_id);
}

struct sp_instance *sp_find_instance_by_listen(int chan_id)
{
    for (struct ll_list_entry *e = sp_instance_list.head; e != NULL;
         e = e->next) {
        struct sp_instance *inst = (struct sp_instance *) e;
        if (inst->listen_chan == chan_id)
            return inst;
    }
    return NULL;
}

struct sp_relay *sp_find_relay(int chan_id)
{
    for (struct ll_list_entry *ie = sp_instance_list.head; ie != NULL;
         ie = ie->next) {
        struct sp_instance *inst = (struct sp_instance *) ie;

        for (struct ll_list_entry *re = inst->relays.head; re != NULL;
             re = re->next) {
            struct sp_relay *relay = (struct sp_relay *) re;
            if (relay->client_chan == chan_id
                || relay->backend_chan == chan_id)
                return relay;
        }
    }
    return NULL;
}

/*
 * inst->listen_chan is accept-ready. Accepts the client, then dials
 * out to inst->run_host:inst->app_port -- the proxy is the client on
 * this second leg, same shape as sp_mbd_connect() dialing mbd, just a
 * different peer. Blocking-with-timeout chan_connect(), same call
 * already made for the mbd link: correctness first, a slow backend
 * stalling one accept is a hardening concern (chan_connect_begin/
 * chan_connect_finish exist for that), not a correctness one.
 *
 * If run_host is still empty (client connected before mbd's
 * BATCH_SVC_UPDATE arrived -- shouldn't happen in practice since
 * bservice only hands the URL to its caller after RUNNING, but
 * defended against rather than assumed away) the client is dropped
 * with no backend to reach.
 */
void sp_relay_accept(struct sp_instance *inst)
{
    struct sockaddr_in from;
    memset(&from, 0, sizeof(from));

    int client_chan = chan_accept(inst->listen_chan, &from);
    if (client_chan < 0) {
        LL_ERR("sp_relay_accept: chan_accept failed svc_id=%s: %m",
               inst->svc_id);
        return;
    }
    channels[client_chan].type = TCP_RELAY;

    if (inst->run_host[0] == '\0') {
        LL_ERRX("sp_relay_accept: svc_id=%s no run_host yet, rejecting "
                "client", inst->svc_id);
        chan_close(client_chan);
        return;
    }

    struct ll_host backend_node;
    if (get_host_by_name(inst->run_host, &backend_node) < 0) {
        LL_ERR("sp_relay_accept: cannot resolve run_host=%s svc_id=%s",
               inst->run_host, inst->svc_id);
        chan_close(client_chan);
        return;
    }

    int backend_chan = chan_tcp_client();
    if (backend_chan < 0) {
        LL_ERR("sp_relay_accept: chan_tcp_client failed svc_id=%s",
               inst->svc_id);
        chan_close(client_chan);
        return;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    get_host_addrv4(&backend_node, &addr);
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t) inst->app_port);

    if (chan_connect(backend_chan, &addr, 3) < 0) {
        LL_ERR("sp_relay_accept: cannot connect to backend %s:%d "
               "svc_id=%s: %m", inst->run_host, inst->app_port,
               inst->svc_id);
        chan_close(backend_chan);
        chan_close(client_chan);
        return;
    }
    channels[backend_chan].type = TCP_RELAY;

    struct epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.events = EPOLLIN;
    ev.data.u32 = (uint32_t) client_chan;
    if (epoll_ctl(sp_efd, EPOLL_CTL_ADD, chan_sock(client_chan), &ev) < 0) {
        LL_ERR("sp_relay_accept: epoll_ctl add client_chan=%d failed "
               "svc_id=%s", client_chan, inst->svc_id);
        chan_close(backend_chan);
        chan_close(client_chan);
        return;
    }

    ev.data.u32 = (uint32_t) backend_chan;
    if (epoll_ctl(sp_efd, EPOLL_CTL_ADD, chan_sock(backend_chan), &ev) < 0) {
        LL_ERR("sp_relay_accept: epoll_ctl add backend_chan=%d failed "
               "svc_id=%s", backend_chan, inst->svc_id);
        sp_chan_shutdown(client_chan);
        chan_close(backend_chan);
        return;
    }

    struct sp_relay *relay = calloc(1, sizeof(*relay));
    if (relay == NULL) {
        LL_ERR("sp_relay_accept: calloc failed svc_id=%s", inst->svc_id);
        sp_chan_shutdown(client_chan);
        sp_chan_shutdown(backend_chan);
        return;
    }

    relay->inst = inst;
    relay->client_chan = client_chan;
    relay->backend_chan = backend_chan;
    ll_list_append(&inst->relays, &relay->ent);

    LL_INFO("sp_relay_accept: svc_id=%s client_chan=%d backend_chan=%d "
           "%s:%d connected", inst->svc_id, client_chan, backend_chan,
           inst->run_host, inst->app_port);
}

/*
 * Move at most one LL_BUFSIZ_8K chunk from src to dst for one
 * direction of a relay pair. do_read gates whether to attempt a new
 * recv() at all -- EPOLLOUT-triggered calls only want to flush an
 * already-buffered chunk, not pull in more. Only reads when the
 * pending buffer for this direction is fully flushed (pos == len) --
 * otherwise a fast reader could overwrite bytes not yet sent to a
 * slow dst.
 *
 * If dst can't take everything in one send(), the remainder stays
 * buffered and dst's EPOLLOUT interest turns on so sp_relay_event()
 * gets called again to flush it -- mirrors chan_data's own dowrite()
 * continuation for framed messages, same problem, same shape of fix,
 * just for one relay chunk instead of a queue of protocol messages.
 *
 * Returns -1 if the relay was closed during this call (peer EOF or a
 * real I/O error) -- caller must not touch relay again in that case.
 */
static int sp_relay_pump(struct sp_relay *relay, int src, int dst,
                         char *buf, int *len, int *pos, int do_read)
{
    if (do_read && *pos == *len) {
        ssize_t cc = recv(chan_sock(src), buf, LL_BUFSIZ_8K, 0);
        if (cc > 0) {
            *len = (int) cc;
            *pos = 0;
        } else if (cc == 0) {
            LL_DEBUG("sp_relay: svc_id=%s chan=%d peer closed",
                    relay->inst->svc_id, src);
            sp_relay_close(relay);
            return -1;
        } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
            LL_DEBUG("sp_relay: svc_id=%s chan=%d recv failed: %m",
                    relay->inst->svc_id, src);
            sp_relay_close(relay);
            return -1;
        }
        /* EAGAIN: nothing ready right now, buffer stays as it was */
    }

    if (*len > *pos) {
        ssize_t cc = send(chan_sock(dst), buf + *pos, *len - *pos, 0);
        if (cc > 0) {
            *pos += (int) cc;
        } else if (cc < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
            LL_DEBUG("sp_relay: svc_id=%s chan=%d send failed: %m",
                    relay->inst->svc_id, dst);
            sp_relay_close(relay);
            return -1;
        }

        if (chan_set_write_interest(dst, sp_efd, *pos < *len) < 0)
            LL_ERR("sp_relay: chan_set_write_interest failed chan=%d", dst);
    }

    return 0;
}

/*
 * One epoll-ready event on either leg (chan_id) of relay. events is
 * spmain.c's raw sp_events[i].events -- TCP_RELAY channels bypass
 * chan_events entirely (chan_epoll() only sets a fixed CHAN_EPOLLIN so
 * the generic CHAN_EPOLLNONE skip in the main loop doesn't eat the
 * event; it carries no read/write distinction for this type, unlike
 * the framed-message channels).
 *
 * Two independent pipes ride on one relay pair: client->backend (c2b)
 * and backend->client (b2c). EPOLLIN on a leg means that leg has new
 * data (advances the pipe whose SOURCE is this leg); EPOLLOUT on a
 * leg means that leg can now accept more writes (flushes the pipe
 * whose DESTINATION is this leg, only relevant if a previous send()
 * stalled).
 */
void sp_relay_event(struct sp_relay *relay, int chan_id, uint32_t events)
{
    if (events & (EPOLLHUP | EPOLLRDHUP | EPOLLERR)) {
        LL_DEBUG("sp_relay_event: svc_id=%s chan=%d hup/err",
                relay->inst->svc_id, chan_id);
        sp_relay_close(relay);
        return;
    }

    if (chan_id == relay->client_chan) {
        if (events & EPOLLIN) {
            if (sp_relay_pump(relay, relay->client_chan, relay->backend_chan,
                              relay->c2b_buf, &relay->c2b_len,
                              &relay->c2b_pos, 1) < 0) {
                LL_ERR("sp_relay_event: svc_id=%s chan=%d c2b pump failed",
                       relay->inst->svc_id, chan_id);
                return; /* relay closed and freed inside sp_relay_pump() */
            }
        }

        /* EPOLLOUT must stay last in this block: sp_relay_pump() may
         * free relay on failure, and nothing here checks for that --
         * safe only because there is no statement after it. Add one
         * and you must add an explicit return on failure too. */
        if (events & EPOLLOUT) {
            if (sp_relay_pump(relay, relay->backend_chan, relay->client_chan,
                              relay->b2c_buf, &relay->b2c_len,
                              &relay->b2c_pos, 0) < 0)
                LL_ERR("sp_relay_event: svc_id=%s chan=%d b2c pump failed",
                       relay->inst->svc_id, chan_id);
        }

        return;
    }

    if (chan_id == relay->backend_chan) {
        if (events & EPOLLIN) {
            if (sp_relay_pump(relay, relay->backend_chan, relay->client_chan,
                              relay->b2c_buf, &relay->b2c_len,
                              &relay->b2c_pos, 1) < 0) {
                LL_ERR("sp_relay_event: svc_id=%s chan=%d b2c pump failed",
                       relay->inst->svc_id, chan_id);
                return; /* relay closed and freed inside sp_relay_pump() */
            }
        }

        /* EPOLLOUT must stay last in this block -- see comment above. */
        if (events & EPOLLOUT) {
            if (sp_relay_pump(relay, relay->client_chan, relay->backend_chan,
                              relay->c2b_buf, &relay->c2b_len,
                              &relay->c2b_pos, 0) < 0)
                LL_ERR("sp_relay_event: svc_id=%s chan=%d c2b pump failed",
                       relay->inst->svc_id, chan_id);
        }

        return;
    }

    LL_ERR("sp_relay_event: chan_id=%d matches neither client nor backend "
          "of relay, svc_id=%s -- caller/relay bookkeeping is corrupted",
          chan_id, relay->inst->svc_id);
    abort();
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
