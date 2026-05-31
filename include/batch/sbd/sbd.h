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

/*
 * Global daemon state.
 */
extern int sbd_efd;
extern int sbd_listen_chan;
extern int sbd_timer_chan;
extern int sbd_mbd_chan;

extern pid_t pruner_pid;
extern int non_root;
extern struct ll_host mbd_node;

extern char sbd_root_dir[PATH_MAX];
extern char sbd_state_dir[PATH_MAX];
extern char sbd_job_dir[PATH_MAX];
extern char sim_name[MAXHOSTNAMELEN];

enum sbd_policy {
    SBD_OPERATION_TIMER = 1,
    SBD_RESEND_ACK_TIMEOUT = 1,
};

enum sbd_fatal_cause {
    SBD_FATAL_STORAGE = 1,
    SBD_FATAL_INVARIANT,
    SBD_FATAL_PROTO,
    SBD_FATAL_OOM,
    SBD_FATAL_ENQUEUE
};

/*
 * sbatchd-local view of a job.
 */
struct sbd_job {
    struct ll_list_entry list;

    int64_t job_id;

    pid_t pid;
    pid_t pgid;

    uid_t exec_uid;
    gid_t exec_gid;
    uint32_t umask;
    int32_t ncpus;
    uint64_t mem_mb;

    char exec_user[LL_BUFSIZ_64];
    char exec_home[PATH_MAX];
    char exec_cwd[PATH_MAX];

    char command[LL_BUFSIZ_512];
    char job_name[LL_BUFSIZ_256];
    char queue[LL_BUFSIZ_64];
    char hosts[LL_BUFSIZ_4K];

    char in_file[PATH_MAX];
    char out_file[PATH_MAX];
    char err_file[PATH_MAX];

    bool_t pid_acked;
    time_t time_pid_acked;
    time_t reply_last_send;

    bool_t finish_acked;
    time_t time_finish_acked;
    time_t finish_last_send;

    int retry_exit_count;

    int exit_status;
    bool_t exit_status_valid;
    time_t end_time;
    struct job_res_usage res_usage;
};

/*
 * Minimal persistent job state used to recover after sbd restart.
 */
struct sbd_job_state {
    int64_t job_id;

    pid_t pid;
    pid_t pgid;

    bool_t pid_acked;
    bool_t finish_acked;

    bool_t exit_status_valid;
    int exit_status;
    time_t end_time;
};

/*
 * Job containers.
 */
extern struct ll_list sbd_job_list;
extern struct ll_hash *sbd_job_hash;

/*
 * Fatal handling.
 */
void sbd_fatal(enum sbd_fatal_cause);

/*
 * mbd link.
 */
int sbd_mbd_connect(void);
int sbd_register(void);
void sbd_register_ack(XDR *);
void sbd_mbd_link_down(void);
int sbd_mbd_link_ready(void);
int sbd_mbd_route(int);
void sbd_chan_shutdown(int);

/*
 * Job lifecycle.
 */
void sbd_job_new(XDR *);
struct sbd_job *sbd_job_lookup(int64_t);
void sbd_job_insert(struct sbd_job *);

int sbd_job_script_write(struct sbd_job *, const struct wire_job_script *);

int sbd_job_new_reply(struct sbd_job *);
void sbd_job_new_reply_ack(XDR *);

int sbd_job_finish(struct sbd_job *);
void sbd_job_finish_ack(XDR *);

int sbd_job_signal(XDR *);
int sbd_enqueue_job_unknown(int64_t);

/*
 * Job storage/state.
 */
int sbd_storage_init(void);
int sbd_job_state_load_all(void);
int sbd_job_state_read(struct sbd_job *, char *);
int sbd_job_state_write(struct sbd_job *);
int sbd_job_cleanup_files(struct sbd_job *);
int sbd_read_exit_status_file(struct sbd_job *, int *, time_t *);
void sbd_prune_jobs_try(void);
void reset_signals(void);
void reset_except_fd(int);
void sbd_job_file_remove(struct sbd_job *);
void sbd_job_state_remove(struct sbd_job *);
void fsync_dir(const char *);

/*
 * Local client/status interface.
 */
int sbd_accept(int);
int sbd_client(int);

/*
 * Wire send helpers.
 */
int sbd_send_msg(int32_t, int32_t, void *, size_t, bool_t (*)());
int write_all(int, const char *, size_t);

/* cgroups
 */
int cgroup_init(void);
int cgroup_job_create(int64_t, uint64_t, int32_t);
int cgroup_job_assign(int64_t, pid_t);
void cgroup_job_destroy(int64_t);
int cgroup_job_freeze(int64_t);
int cgroup_job_thaw(int64_t);
int cgroup_job_kill(int64_t);
int cgroup_job_collect(int64_t, struct job_res_usage *);
