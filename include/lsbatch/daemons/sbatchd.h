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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA  02110-1301, USA
 */

#pragma once

#include "lsf/lib/ll.sys.h"
#include "lsf/lib/ll.sysenv.h"
#include "lsf/lib/lproto.h"
#include "lsf/lib/ll.host.h"
#include "lsf/lib/lib.channel.h"
#include "lsbatch/lib/lsb.xdr.h"
#include "lsbatch/lib/lsb.h"      // jobSpecs, sbdReplyType
#include "lsf/lib/ll.list.h"      // struct ll_list, struct ll_list_entry
#include "lsf/lib/ll.hash.h"      // struct ll_hash
#include "lsbatch/daemons/daemons.h"

extern int sbd_debug;
// chan epoll
extern int sbd_efd;
// the sbd channel to talk to its own clients
extern int sbd_chan;
// sbd has a timer to driver periodic operation and this is the
// associated channel
extern int sbd_timer_chan;
// channel to mbd, sbd is a permanent client of mbd
extern int sbd_mbd_chan;

int sbd_mbd_register(void);
int sbd_connect_mbd(void);

// handle mbd messagges
int sbd_handle_mbd(int);


// Basic sbatchd job states.
// Keep it small; mbd already has its own view.
enum sbd_job_state {
    SBD_JOB_PENDING = 0,          // received, not yet forked
    SBD_JOB_RUNNING,              // forked/exec'd, still alive
    SBD_JOB_EXITED,               // exited normally
    SBD_JOB_FAILED,               // failed before/at exec
    SBD_JOB_KILLED                // killed by signal
};

// sbatchd-local view of a job.
// This replaces the old jobCard horror.
struct sbd_job {
    struct ll_list_entry list;    // intrusive link in global job list
    int          job_id;          // jobId
    struct jobSpecs spec;         // full job description as received
    pid_t        pid;             // main job pid (child)
    pid_t        pgid;            // process group id
    enum sbd_job_state state;     // sbatchd-local state
    time_t       start_time;      // when we exec'd
    int          exit_status;     // waitpid() status
    bool_t       exit_status_valid;
};

// Global containers (defined in sbd.job.c or sbd.main.c)
extern struct ll_list sbd_job_list;   // intrusive list of all active jobs
extern struct ll_hash *sbd_job_hash;  // key: job_id -> value: struct sbd_job*

// ---- sbd_job workers ----

// Look up a job by jobId (NULL if not found).
struct sbd_job *sbd_job_lookup(int job_id);

// Allocate + initialise a new sbd_job from jobSpecs.
// Does not insert into list/hash.
struct sbd_job *sbd_job_create(const struct jobSpecs *spec);

// Insert job into global list + hash.
void sbd_job_insert(struct sbd_job *job);

// Remove job from global list + hash (does not free()).
void sbd_job_unlink(struct sbd_job *job);

// Free job structure and any attached resources.
void sbd_job_free(struct sbd_job *job);

// Mapping between sbatchd state and lsbatch.h JOB_STAT_* bitmask
int sbd_state_to_jstatus(enum sbd_job_state);

// Refresh job status from mbd
void sbd_job_sync_jstatus(struct sbd_job *);
