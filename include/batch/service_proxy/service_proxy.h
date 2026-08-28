/*
 * Copyright (C) LavaLite Contributors
 * GPL v2
 */
#pragma once

#include "base/lib/ll.channel.h"
#include "base/lib/ll.list.h"
#include "batch/lib/wire.h"

#define SP_OPERATION_TIMER 5   /* seconds between reconnect/maintenance ticks */

/* Prototype range, deliberately not in ll.conf yet -- promote to
 * LL_SVC_PORT_MIN/MAX there once the design settles, same call already
 * made for service.c's resource defaults. */
#define SP_SVC_PORT_MIN 30000
#define SP_SVC_PORT_MAX 35000

extern int sp_efd;
extern int sp_mbd_chan;
extern int sp_timer_chan;
extern struct ll_host mbd_node;
extern char sim_name[MAXHOSTNAMELEN]; /* empty = not in sim mode, mirrors sbd */

/* -----------------------------------------------------------------------
 * Relay bookkeeping (mbd <-> spd control channel, i.e. everything above,
 * only tells spd WHAT to forward. The structs below track HOW -- the
 * actual listening sockets and client<->backend byte pumps. The channel
 * code has no view into "whose" a channel is (chan_data carries no
 * opaque pointer, deliberately), so this is the side-table that
 * supplies it, same role mbd_node/sbd_chan_hash play for sbd
 * connections -- except here a plain list + linear scan, not a hash:
 * no ll_hash_remove() is visible in what this file has to work with,
 * and instance/relay counts are the same "tens, not thousands" scale
 * already used to justify linear scans elsewhere in this codebase
 * (svc_find_by_name(), svc_find_instance_by_id()).
 * ----------------------------------------------------------------------- */

/*
 * One configured service instance the proxy is fronting: one listening
 * socket (bound once at BATCH_SVC_ADD, held open for the instance's
 * whole life -- clients connect/disconnect freely, the listener itself
 * is never touched by that) plus every currently-active client<->backend
 * relay pair riding through it.
 */
struct sp_instance {
    struct ll_list_entry ent;      /* linkage in sp_instance_list */
    int listen_chan;               /* TCP_SERVER, bound at ADD time */
    int port;                      /* external port this is bound to */
    uid_t uid;                     /* user of the instance */
    int64_t job_id;                /* service job_id */
    int app_port;                  /* backend's internal port, from ADD */
    char run_host[MAXHOSTNAMELEN]; /* empty until BATCH_SVC_UPDATE arrives */
    struct ll_list relays;         /* active sp_relay pairs on this instance */
};

/*
 * One accepted client connection and its matching outbound leg to the
 * backend. Both chan_ids are TCP_RELAY. Two independent byte pipes ride
 * on this pair -- client->backend (c2b) and backend->client (b2c) --
 * each with its own small pending-write buffer, used only when the
 * destination can't take a full read()'d chunk in one send(): same
 * partial-write problem chan_data's own send list solves for framed
 * messages, just sized for one relay chunk instead of a queue of them.
 */
struct sp_relay {
    struct ll_list_entry ent;   /* linkage in sp_instance.relays */
    struct sp_instance *inst;   /* owning instance, for lookup/cleanup/logs */
    int client_chan;            /* accepted from inst->listen_chan */
    int backend_chan;           /* connected to inst->run_host:app_port */
    char c2b_buf[LL_BUFSIZ_8K];
    int c2b_len;
    int c2b_pos;
    char b2c_buf[LL_BUFSIZ_8K];
    int b2c_len;
    int b2c_pos;
    int close_state;
};

extern struct ll_list sp_instance_list;

/* spnet.c */
int sp_mbd_connect(void);
void sp_mbd_link_down(void);
bool_t sp_mbd_link_ready(void);
int sp_register(void);
void sp_register_ack(XDR *xdrs);
void sp_chan_shutdown(int chan_id);
int sp_mbd_route(int chan_id);
int sp_send_msg(int32_t op, int32_t status, void *payload, size_t siz,
                bool_t (*xdr_func)());

/* relay lookups, used by spmain.c's dispatch loop to tell "which
 * service's listener" or "which relay leg" a ready chan_id belongs to */
struct sp_instance *sp_find_instance_by_listen(int chan_id);
struct sp_relay *sp_find_relay(int chan_id);
void sp_relay_accept(struct sp_instance *inst);
void sp_relay_event(struct sp_relay *relay, int chan_id, uint32_t events);

enum sp_fatal_cause {
    SP_FATAL_PROTO,
    SP_FATAL_OOM,
    SP_FATAL_ENQUEUE,
};
void sp_fatal(enum sp_fatal_cause cause);
