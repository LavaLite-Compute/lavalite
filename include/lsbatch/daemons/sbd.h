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
extern int sbd_listen_chan;
// sbd has a timer to driver periodic operation and this is the
// associated channel
extern int sbd_timer_chan;
// channel to mbd, sbd is a permanent client of mbd
extern int sbd_mbd_chan;
extern bool_t connected;
extern bool_t sbd_mbd_connecting;

// LavaLite sbd root dir working directory for jobs
extern char sbd_root_dir[PATH_MAX];

int sbd_connect_mbd(void);
int sbd_nb_connect_mbd(bool_t *);
int sbd_enqueue_register(int);
bool_t sbd_mbd_link_ready(void);
void sbd_mbd_link_down(void);
void sbd_mbd_shutdown(void);

// handle mbd messagges
int sbd_handle_mbd(int);
void sbd_new_job(int chfd, XDR *, struct packet_header *);
void sbd_new_job_reply_ack(int, XDR *, struct packet_header *);
void sbd_job_execute_ack(int, XDR *, struct packet_header *);
void sbd_job_finish_ack(int ch_id, XDR *, struct packet_header *);
int sbd_signal_job(int, XDR *, struct packet_header *);
int sbd_enqueue_signal_job_reply(int, struct packet_header *,
                                 struct wire_job_sig_reply *);

// timeout is in second
#define DEFAULT_SBD_OPERATION_TIMER 1
#define DEFAUL_RESEND_ACK_TIMEOUT 1

// sbatchd-local view of a job.
// This replaces the old jobCard horror.
struct sbd_job {
    struct ll_list_entry list;        // intrusive link in global job list

    int64_t job_id;                   // global jobId (stable, assigned by mbd)
    // job spec sent from mbd are unmutable
    struct jobSpecs specs;             // job specification as received from mbd

    pid_t pid;                        // main job PID (child spawned by sbatchd)
    pid_t pgid;                       // process group ID for the job
    char exec_user[LL_BUFSIZ_32];
    char exec_home[PATH_MAX];
    char exec_cwd[PATH_MAX];   // execution working directory
    uid_t exec_uid;
    gid_t exec_gid;
    char jobfile[PATH_MAX]; // copy of the job_file from mbd jobSpecs
    /*
     * pid_acked (PID_COMMITTED):
     *   Set to TRUE when sbatchd processes BATCH_NEW_JOB_ACK from mbd.
     *   This ACK means mbd has *committed* (logged) the pid/pgid snapshot
     *   for this job in its event log (lsb.events is the source of truth).
     *
     *   Protocol ordering gate:
     *     - EXECUTE and FINISH must not be emitted before pid_acked.
     */
    bool_t pid_acked;
    time_t time_pid_acked;         // time when pid_acked was set (diagnostics)
    time_t reply_last_send; // last time we tried to sent the reply
    /*
     * execute_acked (EXECUTE_COMMITTED):
     *   Set to TRUE when sbatchd processes the ACK for the EXECUTE event,
     *   meaning mbd has logged/committed the EXECUTE record for this job.
     *
     *   Ordering:
     *     - execute_acked implies pid_acked.
     */
    bool_t execute_acked;
    time_t time_execute_acked;
    time_t execute_last_send; // last time we tried to send execute status
    int retry_exit_count;
    /*
     * finish_acked (FINISH_COMMITTED):
     *   Set to TRUE when sbatchd processes the ACK for the FINISH event,
     *   meaning mbd has logged/committed the terminal record for this job.
     *
     *   Ordering:
     *     - finish_acked implies execute_acked.
     *     - FINISH is only eligible once exit_status_valid is true.
     */
    bool_t finish_acked;
    time_t time_finish_acked;
    time_t finish_last_send; // last time we tried to send finish status

    int exit_status;                  // raw waitpid() status
    bool_t exit_status_valid;   // TRUE once waitpid() has captured exit_status
    time_t end_time;     // job finish time

    struct lsfRusage lsf_rusage;  // resource usage snapshot (zero for now;
                                  // later populated from cgroupv2)
};

// Struct sbd job state to save the minimum status of the job to the
// state file for the purpose of reloading it if/when the sbd restarts
struct sbd_job_record {
    int64_t job_id;

    pid_t pid;
    pid_t pgid;

    bool_t reply_committed;    // PID_COMMITTED (op30)
    bool_t execute_committed;  // EXECUTE committed (op31)
    bool_t finish_committed;   // FINISH committed (op32)

    bool_t finished_locally;   // we reaped it
    int    exit_status;        // raw waitpid() status
    time_t end_time;  // if the job has finih record the time
};


// Global containers (defined in sbd.job.c or sbd.main.c)
extern struct ll_list sbd_job_list;   // intrusive list of all active jobs
extern struct ll_hash *sbd_job_hash;  // key: job_id -> value: struct sbd_job*

// ---- sbd_job workers ----

// sbd write helper make sure all buffer is drained
int write_all(int fd, const char *, size_t);

// Look up a job by jobId (NULL if not found).
struct sbd_job *sbd_job_lookup(int job_id);

// Allocate + initialise a new sbd_job from jobSpecs.
// Does not insert into list/hash.
struct sbd_job *sbd_job_create(const struct jobSpecs *spec);

// Insert job into global list + hash.
void sbd_job_insert(struct sbd_job *);
// Remove and destroy job from global list + hash + free.
void sbd_job_destroy(struct sbd_job *);
void sbd_job_free(void *);

// Refresh job status from mbd
void sbd_job_sync_jstatus(struct sbd_job *);

int sbd_enqueue_new_job_reply(struct sbd_job *);
int sbd_enqueue_execute(struct sbd_job *);
int sbd_enqueue_finish( struct sbd_job *);
bool_t sbd_mbd_link_ready(void);

// sbd record function to save the state of jobs from
// sbd point of view restore the latest state after
// restart
int sbd_job_record_dir_init(void);
int sbd_job_record_load_all(void);
int sbd_job_record_read(struct sbd_job *, char *);
int sbd_job_record_write(struct sbd_job *);
int sbd_job_record_remove(struct sbd_job *);
int sbd_jobfile_remove(struct sbd_job *);
void sbd_prune_acked_jobs(void);
int sbd_go_write(struct sbd_job *);


// sbd has command to query its internal status
int handle_sbd_accept(int);
int handle_sbd_client(int);
int sbd_reply_hdr_only(int, int, struct packet_header *);

// Use the xdr_func() signiture without argument otherwise we should
// use void * and every xdr function will have to take it and cast it
// to its own xdr data structure
int sbd_reply_payload(int, int, struct packet_header *,
                      void *, bool_t (*xdr_func)());
int sbd_read_exit_status_file(struct sbd_job *, int *, time_t *);
void sbd_child_open_log(const struct jobSpecs *);
