/*
 * Copyright (C) LavaLite Contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */
#pragma once

#include "lsbatch/lib/lsb.h"
#include "lsbatch/daemons/daemonout.h"
#include "lsbatch/daemons/daemons.h"
#include "lsbatch/daemons/jgrp.h"
#include "lsf/lib/ll.list.h"
#include "lsf/lib/ll.hash.h"

// LavaLite global host list as returned by lsb_gethostinfo()
extern struct hostInfo *host_list;
extern int host_count;

// LavaLite model, cleant and most honest approach model — codify
// the assumption that the daemon is launched by the cluster admin,
// and make that identity explicit and reusable.
// One user, fixed identity, zero UID gymnastics.
struct mbd_manager {
    uid_t uid;
    gid_t gid;
    char *name;
};
extern struct mbd_manager *mbd_mgr;
extern bool_t is_manager(const char *);
extern int mbd_chan;
extern int mbd_efd;
extern uint16_t mbd_port;
extern struct epoll_event *mbd_events;
extern int mbd_max_events;

struct mbd_manager *mbd_init_manager(void);
int mbd_init_networking(void);

// hData hashed by channel id connected to that sbd
extern struct ll_hash hdata_by_chan;

int mbd_init(int);
int mbd_dispatch_sbd(struct mbd_client_node *);
// handler entry point
int mbd_handle_sbd(int);
int mbd_sbd_register(XDR *, struct mbd_client_node *, struct packet_header *);
int mbd_new_job_reply(struct mbd_client_node *,
                      XDR *,
                      struct packet_header *);
int mbd_set_status_execute(struct mbd_client_node *, XDR *,
                           struct packet_header *);
int mbd_set_status_finish(struct mbd_client_node *, XDR *,
                          struct packet_header *);
int mbd_set_rusage_update(struct mbd_client_node *, XDR *,
                          struct packet_header *);
int mbd_sbd_disconnect(struct mbd_client_node *);
int mbd_enqueue_hdr(struct mbd_client_node *, int);
int mbd_init_tables(void);
int mbd_send_event_ack(struct mbd_client_node *, int,
                       const struct job_status_ack *);
int mbd_handle_slave_restart(struct mbd_client_node *,
                             struct packet_header *,
                             XDR *);
int mbd_read_job_file(struct jobSpecs *, struct jData *);
int mbd_handle_signal_req(XDR *, struct mbd_client_node *,
                          struct packet_header *, struct lsfAuth *);
int mbd_signal_all_jobs(int, struct signalReq *, struct lsfAuth *);
int mbd_signal_pending_job(struct jData *,  struct signalReq *,
                           struct lsfAuth *);
int mbd_finish_pend_job(struct jData *);
int mbd_stop_pend_job(struct jData *);
int mbd_signal_running_job(struct jData *, struct signalReq *,
                           struct lsfAuth *);
int mbd_job_signal_reply(struct mbd_client_node *, XDR *, struct packet_header *);
void mbd_job_status_change(struct jData *, int, time_t, const char *);

void logJobInfo(struct submitReq *, struct jData *, struct wire_job_file *);

// job signaling
struct sig_host_bucket {
    struct hData *host; // the host node where the jobids are
    int64_t *job_ids;   // jobids in the backet
    uint32_t n;         // num jobs in the backet
    uint32_t cap;       // grow cap of the bucket
};
