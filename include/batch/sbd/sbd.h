/*
 * Copyright (C) LavaLite Contributors
 * GPLv2
 */

#pragma once

#include "base/lib/ll.sys.h"
#include "base/lib/ll.host.h"
#include "base/lib/ll.channel.h"
#include "base/lib/ll.list.h"
#include "base/lib/ll.hash.h"
#include "batch/lib/wire.h"

// chan epoll
extern int sbd_efd;
// the sbd channel to talk to its own clients
extern int sbd_listen_chan;
// sbd has a timer to driver periodic operation and this is the
// associated channel
extern int sbd_timer_chan;
// channel to mbd, sbd is a permanent client of mbd
extern int sbd_mbd_chan;
extern pid_t pruner_pid;
extern int non_root;
extern struct ll_host mbd_host;

// LavaLite sbd root dir working directory for jobs
extern char sbd_root_dir[PATH_MAX];
extern char sbd_state_dir[PATH_MAX];
extern char sbd_job_dir[PATH_MAX];
extern char sbd_archive_dir[PATH_MAX];

enum sbd_policy {
    SBD_OPERATION_TIMER  = 1,
    SBD_RESEND_ACK_TIMEOUT = 1,
    SBD_ARCHIVE_RETENTION  = 24 * 3600,
};

// sbatchd-local view of a job.
// This replaces the old jobCard horror.
struct sbd_job {
    struct ll_list_entry list;        // intrusive link in global job list

    int64_t job_id;                   // global jobId (stable, assigned by mbd)
    // job spec sent from mbd are unmutable
    // struct jobSpecs specs;     // job specification as received from mbd

    pid_t pid;                        // main job PID (child spawned by sbatchd)
    pid_t pgid;                       // process group ID for the job
    char exec_user[LL_BUFSIZ_64];
    char exec_home[PATH_MAX];
    char exec_cwd[PATH_MAX];   // execution working directory
    uid_t exec_uid;
    gid_t exec_gid;
    char jobfile[PATH_MAX]; // copy of the job_file from mbd jobSpecs
    char command[LL_BUFSIZ_512];
    char job_name[LL_BUFSIZ_256];
    char queue[LL_BUFSIZ_64];
    char from_host[MAXHOSTNAMELEN];
    char hosts[LL_BUFSIZ_4K];
    char in_file[PATH_MAX];
    char out_file[PATH_MAX];
    char err_file[PATH_MAX];
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
    time_t time_pid_acked;  // time when pid_acked was set (diagnostics)
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

    struct job_resources jres;
};

// Struct sbd job state to save the minimum status of the job to the
// state file for the purpose of reloading it if/when the sbd restarts
struct sbd_job_state {
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

int write_all(int fd, const char *, size_t);

// Look up a job by jobId (NULL if not found).
struct sbd_job *sbd_job_lookup(int64_t job_id);

// Allocate + initialise a new sbd_job from jobSpecs.
// Does not insert into list/hash.
//struct sbd_job *sbd_job_create(const struct jobSpecs *spec);
void sbd_job_file_remove(struct sbd_job *);
void sbd_job_state_archive(struct sbd_job *);
void sbd_prune_archive(void);

// Insert job into global list + hash.
void sbd_job_insert(struct sbd_job *);

// Refresh job status from mbd
void sbd_job_sync_jstatus(struct sbd_job *);

//int sbd_enqueue_new_job_reply(struct sbd_job *, struct jobReply *);
int sbd_enqueue_execute(struct sbd_job *);
int sbd_enqueue_finish( struct sbd_job *);
int sbd_mbd_link_ready(void);

// sbd record function to save the state of jobs from
// sbd point of view restore the latest state after
// restart
int sbd_storage_init(void);
int sbd_job_state_load_all(void);
int sbd_job_state_read(struct sbd_job *, char *);
int sbd_job_state_write(struct sbd_job *);
int sbd_job_cleanup_files(struct sbd_job *);
void sbd_prune_acked_jobs(void);

// sbd has command to query its internal status
int sbd_accept(int);
int sbd_client(int);
int sbd_reply_hdr_only(int, int, struct protocol_header *);

// Use the xdr_func() signiture without argument otherwise we should
// use void * and every xdr function will have to take it and cast it
// to its own xdr data structure
int sbd_reply_payload(int, int, struct protocol_header *,
                      void *, bool_t (*xdr_func)());
int sbd_read_exit_status_file(struct sbd_job *, int *, time_t *);

enum sbd_fatal_cause {
    SBD_FATAL_STORAGE = 1,
    SBD_FATAL_INVARIANT,
    SBD_FATAL_PROTO,
    SBD_FATAL_OOM,
    SBD_FATAL_ENQUEUE
};

void sbd_fatal(enum sbd_fatal_cause);
void sbd_prune_archive_try(void);

int sbd_mbd_connect(void);
int sbd_register(void);
void sbd_mbd_link_down(void);
void sbd_chan_shutdown(int);

// handle mbd messagges
// daemon + object + action
struct sbd_job;
int sbd_mbd_route(int);
void sbd_job_new(XDR *);
void sbd_job_insert(struct sbd_job *);
int sbd_job_script_write(struct sbd_job *, const struct wire_job_script *);
int sbd_job_sidecar_write(struct sbd_job *,
                          const struct wire_job_sidecar *);
int sbd_job_new_reply(struct sbd_job *);
void sbd_job_new_ack(XDR *);

int sbd_job_execute(struct sbd_job *);
void sbd_job_execute_ack(XDR *);

int sbd_job_finish(struct sbd_job *);
void sbd_job_finish_ack(XDR *);

int sbd_job_signal(XDR *);
int sbd_enqueue_job_unknown(int64_t);
//int sbd_job_signal_reply(int, struct protocol_header *,
//                         struct wire_job_sig_reply *);
void sbd_register_ack(XDR *);
//void free_job_specs(struct jobSpecs *);
int32_t sbd_enqueue_payload(int, struct protocol_header *,
                            void *, size_t, bool_t (*xdr_func)());
