/*
 * Copyright (C) LavaLite Contributors
 * GPL v2
 */
#pragma once

#include "base/lib/ll.channel.h"
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

enum sp_fatal_cause {
    SP_FATAL_PROTO,
    SP_FATAL_OOM,
    SP_FATAL_ENQUEUE,
};
void sp_fatal(enum sp_fatal_cause cause);
