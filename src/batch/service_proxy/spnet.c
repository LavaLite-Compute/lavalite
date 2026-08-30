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
     * here -- the registry it maintains (job_id -> forwarding state) is
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

    LL_DEBUG("sp_relay_close: uid=%ld client_chan=%d backend_chan=%d "
            "closed", (long) relay->inst->uid, relay->client_chan,
            relay->backend_chan);

    free(relay);
}

static int sp_bind_port(int *out_port)
{
    for (int port = SP_SVC_PORT_MIN; port <= SP_SVC_PORT_MAX; port++) {
        int ch = chan_tcp_server((uint16_t)port);

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
    ack.job_id = req.job_id;

    int port;
    int listen_chan = sp_bind_port(&port);
    if (listen_chan < 0) {
        LL_ERRX("uid=%ld job_id=%ld no free port in %d..%d", (long) req.uid,
                req.job_id, SP_SVC_PORT_MIN, SP_SVC_PORT_MAX);
        int cc = sp_send_msg(BATCH_SVC_ADD_ACK, ENOSPC, &ack, LL_BUFSIZ_1K,
                             xdr_wire_svc_add_ack);
        if (cc < 0) {
            LL_ERR("listen failed and sp_send_msg failed");
            return -1;
        }
        return 0;
    }
    ack.port = port;

    struct epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.events = EPOLLIN;
    ev.data.u32 = (uint32_t) listen_chan;
    if (epoll_ctl(sp_efd, EPOLL_CTL_ADD, chan_sock(listen_chan), &ev) < 0) {
        LL_ERR("epoll_ctl failed uid=%ld job_id=%ld chan=%d",
               (long) req.uid, req.job_id, listen_chan);
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
        LL_ERR("calloc failed uid=%ld job_id=%ld", (long) req.uid,
               req.job_id);
        sp_chan_shutdown(listen_chan);
        sp_fatal(SP_FATAL_OOM);
        return -1; /* unreached -- sp_fatal() exits */
    }

    inst->uid = req.uid;
    inst->listen_chan = listen_chan;
    inst->port = port;
    inst->app_port = req.app_port;
    inst->job_id = req.job_id;
    ll_list_init(&inst->relays);
    ll_list_append(&sp_instance_list, &inst->ent);

    LL_INFO("uid=%ld job_id=%ld bound port=%d app_port=%d chan=%d",
            (long) inst->uid, inst->job_id, port, inst->app_port, listen_chan);

    int cc = sp_send_msg(BATCH_SVC_ADD_ACK, MBD_OK, &ack, LL_BUFSIZ_1K,
                         xdr_wire_svc_add_ack);
    if (cc < 0) {
        LL_ERR("sp_send_msg failed");
        return -1;
    }

    return 0;
}

static struct sp_instance *sp_find_instance_by_job_id(int64_t job_id)
{
    for (struct ll_list_entry *e = sp_instance_list.head; e != NULL;
         e = e->next) {
        struct sp_instance *inst = (struct sp_instance *) e;
        if (inst->job_id == job_id)
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
        LL_ERR("xdr decode failed");
        return -1;
    }

    struct wire_svc_update_ack ack;
    memset(&ack, 0, sizeof(ack));
    ack.job_id = req.job_id;

    struct sp_instance *inst = sp_find_instance_by_job_id(req.job_id);
    if (inst == NULL) {
        LL_ERRX("unknown job_id=%ld", req.job_id);
        return sp_send_msg(BATCH_SVC_UPDATE_ACK, ESRCH, &ack, LL_BUFSIZ_1K,
                           xdr_wire_svc_update_ack);
    }

    ll_strlcpy(inst->run_host, req.run_host, sizeof(inst->run_host));

    LL_INFO("uid=%ld job_id=%ld run_host=%s", (long) inst->uid,
            inst->job_id, inst->run_host);

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
        LL_ERR("xdr decode failed");
        return -1;
    }

    struct wire_svc_remove_ack ack;
    memset(&ack, 0, sizeof(ack));
    ack.job_id = req.job_id;

    struct sp_instance *inst = sp_find_instance_by_job_id(req.job_id);
    if (inst == NULL) {
        LL_ERRX("unknown job_id=%ld", req.job_id);
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

    LL_INFO("uid=%ld job_id=%ld removed, port=%d freed",
            (long) inst->uid, inst->job_id, inst->port);

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
        LL_ERR("sp_relay_accept: chan_accept failed uid=%ld: %m",
               (long) inst->uid);
        return;
    }
    channels[client_chan].type = TCP_RELAY;

    if (inst->run_host[0] == '\0') {
        LL_ERRX("sp_relay_accept: uid=%ld no run_host yet, rejecting "
                "client", (long) inst->uid);
        chan_close(client_chan);
        return;
    }

    struct ll_host backend_node;
    if (get_host_by_name(inst->run_host, &backend_node) < 0) {
        LL_ERR("sp_relay_accept: cannot resolve run_host=%s uid=%ld",
               inst->run_host, (long) inst->uid);
        chan_close(client_chan);
        return;
    }

    int backend_chan = chan_tcp_client();
    if (backend_chan < 0) {
        LL_ERR("sp_relay_accept: chan_tcp_client failed uid=%ld",
               (long) inst->uid);
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
               "uid=%ld: %m", inst->run_host, inst->app_port,
               (long) inst->uid);
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
               "uid=%ld", client_chan, (long) inst->uid);
        chan_close(backend_chan);
        chan_close(client_chan);
        return;
    }

    ev.data.u32 = (uint32_t) backend_chan;
    if (epoll_ctl(sp_efd, EPOLL_CTL_ADD, chan_sock(backend_chan), &ev) < 0) {
        LL_ERR("sp_relay_accept: epoll_ctl add backend_chan=%d failed "
               "uid=%ld", backend_chan, (long) inst->uid);
        sp_chan_shutdown(client_chan);
        chan_close(backend_chan);
        return;
    }

    struct sp_relay *relay = calloc(1, sizeof(*relay));
    if (relay == NULL) {
        LL_ERR("sp_relay_accept: calloc failed uid=%ld", (long) inst->uid);
        sp_chan_shutdown(client_chan);
        sp_chan_shutdown(backend_chan);
        return;
    }

    relay->inst = inst;
    relay->client_chan = client_chan;
    relay->backend_chan = backend_chan;
    ll_list_append(&inst->relays, &relay->ent);

    LL_INFO("sp_relay_accept: uid=%ld client_chan=%d backend_chan=%d "
           "%s:%d connected", (long) inst->uid, client_chan, backend_chan,
           inst->run_host, inst->app_port);
}

/*
 * Relay I/O helpers.
 *
 * These functions never free or close the relay. They only move bytes
 * and report EOF/error to the caller. sp_relay_event() owns the relay
 * lifetime and decides when it is safe to tear both legs down.
 */

enum sp_relay_io {
    SP_RELAY_IO_OK = 0,
    SP_RELAY_IO_EOF,
    SP_RELAY_IO_ERROR,
};

enum sp_relay_close_state {
    SP_RELAY_OPEN = 0,
    SP_RELAY_CLOSE_AFTER_C2B,
    SP_RELAY_CLOSE_AFTER_B2C,
};

struct sp_relay_dir {
    int src;
    int dst;
    char *buf;
    int *len;
    int *pos;
    enum sp_relay_close_state close_state;
};

static enum sp_relay_io sp_relay_read(int src, char *buf, int *len, int *pos)
{
    if (*pos != *len)
        return SP_RELAY_IO_OK;

    ssize_t cc = recv(chan_sock(src), buf, LL_BUFSIZ_8K, 0);
    if (cc > 0) {
        *len = (int) cc;
        *pos = 0;
        return SP_RELAY_IO_OK;
    }

    if (cc == 0)
        return SP_RELAY_IO_EOF;

    if (errno == EAGAIN || errno == EWOULDBLOCK)
        return SP_RELAY_IO_OK;

    return SP_RELAY_IO_ERROR;
}

static enum sp_relay_io sp_relay_write(int dst, char *buf, int *len, int *pos)
{
    if (*pos == *len) {
        *pos = 0;
        *len = 0;

        if (chan_set_write_interest(dst, sp_efd, 0) < 0)
            return SP_RELAY_IO_ERROR;

        return SP_RELAY_IO_OK;
    }

    ssize_t cc = send(chan_sock(dst), buf + *pos, *len - *pos, 0);
    if (cc > 0) {
        *pos += (int) cc;

        if (*pos == *len) {
            *pos = 0;
            *len = 0;
        }

        if (chan_set_write_interest(dst, sp_efd, *len != 0) < 0)
            return SP_RELAY_IO_ERROR;

        return SP_RELAY_IO_OK;
    }

    if (cc < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
        if (chan_set_write_interest(dst, sp_efd, 1) < 0)
            return SP_RELAY_IO_ERROR;

        return SP_RELAY_IO_OK;
    }

    return SP_RELAY_IO_ERROR;
}

static int sp_relay_read_dir(struct sp_relay *relay, struct sp_relay_dir *dir,
                  uint32_t events)
{
    if (!(events & EPOLLIN))
        return 0;

    enum sp_relay_io rc =
        sp_relay_read(dir->src, dir->buf, dir->len, dir->pos);

    if (rc == SP_RELAY_IO_ERROR) {
        LL_DEBUG("sp_relay_event: uid=%ld chan=%d read failed: %m",
                 (long) relay->inst->uid, dir->src);
        return -1;
    }

    if (sp_relay_write(dir->dst, dir->buf, dir->len,
                       dir->pos) == SP_RELAY_IO_ERROR) {
        LL_DEBUG("sp_relay_event: uid=%ld chan=%d write failed: %m",
                 (long) relay->inst->uid, dir->dst);
        return -1;
    }

    if (rc == SP_RELAY_IO_EOF)
        relay->close_state = dir->close_state;

    return 0;
}

static int sp_relay_flush_dir(struct sp_relay *relay, struct sp_relay_dir *dir)
{
    if (sp_relay_write(dir->dst, dir->buf, dir->len,
                       dir->pos) == SP_RELAY_IO_ERROR) {
        LL_DEBUG("sp_relay_event: uid=%ld chan=%d write failed: %m",
                 (long) relay->inst->uid, dir->dst);
        return -1;
    }

    return 0;
}

static int sp_relay_should_close(struct sp_relay *relay)
{
    if (relay->close_state == SP_RELAY_CLOSE_AFTER_C2B)
        return relay->c2b_len == 0;

    if (relay->close_state == SP_RELAY_CLOSE_AFTER_B2C)
        return relay->b2c_len == 0;

    return 0;
}

/*
 * One epoll-ready event on either leg of a relay.
 *
 * EOF/HUP on one side means: flush bytes already buffered in that
 * direction, then close both legs. We deliberately do not implement
 * TCP half-close semantics here.
 *
 * EPOLLIN is processed before EPOLLRDHUP/EPOLLHUP because epoll may
 * report readable data and peer shutdown in the same event.
 */
void sp_relay_event(struct sp_relay *relay, int chan_id, uint32_t events)
{
    struct sp_relay_dir c2b = {
        .src = relay->client_chan,
        .dst = relay->backend_chan,
        .buf = relay->c2b_buf,
        .len = &relay->c2b_len,
        .pos = &relay->c2b_pos,
        .close_state = SP_RELAY_CLOSE_AFTER_C2B
    };

    struct sp_relay_dir b2c = {
        .src = relay->backend_chan,
        .dst = relay->client_chan,
        .buf = relay->b2c_buf,
        .len = &relay->b2c_len,
        .pos = &relay->b2c_pos,
        .close_state = SP_RELAY_CLOSE_AFTER_B2C
    };

    if (events & EPOLLERR) {
        LL_DEBUG("sp_relay_event: uid=%ld chan=%d err",
                 (long) relay->inst->uid, chan_id);
        sp_relay_close(relay);
        return;
    }

    if (chan_id == relay->client_chan) {
        if (sp_relay_read_dir(relay, &c2b, events) < 0) {
            sp_relay_close(relay);
            return;
        }

        if (events & EPOLLOUT) {
            if (sp_relay_flush_dir(relay, &b2c) < 0) {
                sp_relay_close(relay);
                return;
            }
        }

        if (events & (EPOLLHUP | EPOLLRDHUP))
            relay->close_state = SP_RELAY_CLOSE_AFTER_C2B;

        if (sp_relay_should_close(relay))
            sp_relay_close(relay);

        return;
    }

    if (chan_id == relay->backend_chan) {
        if (sp_relay_read_dir(relay, &b2c, events) < 0) {
            sp_relay_close(relay);
            return;
        }

        if (events & EPOLLOUT) {
            if (sp_relay_flush_dir(relay, &c2b) < 0) {
                sp_relay_close(relay);
                return;
            }
        }

        if (events & (EPOLLHUP | EPOLLRDHUP))
            relay->close_state = SP_RELAY_CLOSE_AFTER_B2C;

        if (sp_relay_should_close(relay))
            sp_relay_close(relay);

        return;
    }

    LL_ERR("sp_relay_event: chan_id=%d matches neither client nor backend "
           "of relay, uid=%ld -- caller/relay bookkeeping is corrupted",
           chan_id, (long) relay->inst->uid);
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
