// Copyright (C) LavaLite Contributors
// GPL V2
#pragma once

#include "base/lib/ll.protocol.h"
#include "batch/lib/rpc.h"
#include "batch/lib/wire.h"
#include "base/lib/ll.bufsiz.h"
#include "base/lib/ll.sys.h"
#include "base/lib/ll.hash.h"
#include "base/lib/ll.host.h"
#include "base/lib/ll.syslog.h"
#include "base/lib/ll.list.h"
#include "base/lib/ll.channel.h"

#include "llbatch.h"

enum job_list_id {
    JOB_LIST_PEND   = 0,
    JOB_LIST_RUN    = 1,
    JOB_LIST_FINISH = 2,
};

// mbd_die exit value
enum mbd_exit {
    MBD_EXIT_CONF   = 200,
    MBD_EXIT_NET    = 201,
    MBD_EXIT_EVENTS = 202,
    MBD_EXIT_JOBS   = 203,
    MBD_EXIT_MEM    = 204,
    MBD_EXIT_FATAL  = 205,
};

struct mbd_manager {
    uid_t uid;
    gid_t gid;
};

/* one pool request, parsed from wire tokenpool string at job_register time */
struct job_token {
    struct ll_list_entry ent;
    char name[LL_BUFSIZ_64];
    int  count;
};

/* what the user requested at submit time */
struct job_resources {
    int32_t  num_cpus;
    int32_t  num_hosts;
    int32_t  num_gpus;
    char     gpu_type[LL_BUFSIZ_256];
    uint64_t mem_mb;
    uint64_t storage_mb;
    int32_t  wall_seconds;
    char     machines_str[LL_BUFSIZ_4K];
    struct ll_hash machines;
    char    tokenpool_str[LL_BUFSIZ_256];
    struct ll_list tokens;
};

/* what the job actually used, reported by sbd */
struct job_runtime_usage {
    pid_t    pid;
    gid_t    gid;
    uint64_t mem_mb;
    uint64_t storage_mb;
    double   cpu_time;
};

struct job_dep {
    struct ll_list_entry ent;
    int type;
    int64_t job_id;
};

struct job_data {
    struct ll_list_entry ent;
    int64_t job_id;
    uid_t uid;
    gid_t gid;
    char user[LL_BUFSIZ_64];
    int state;
    int exit_status;
    int priority;
    time_t  submit_time;      /* bsub received                      */
    time_t  dispatch_time;    /* mbd sent BATCH_NEW_JOB to sbd      */
    time_t fork_time;         /* sbd forked, pid received by mbd    */
    time_t  execute_time;     /* sbd confirmed process is executing */
    time_t  end_time;         /* job exited (DONE or EXIT)          */
    time_t susp_time;
    time_t unknown_time;
    time_t begin_time;
    time_t term_time;
    time_t signal_time;
    struct mbd_queue *queue;
    char project[LL_BUFSIZ_256];
    char name[LL_BUFSIZ_64];
    uint32_t flags;
    enum job_list_id list_id;
    enum pend_reason pend_reason;
    struct ll_list deps;
    struct job_resources res;    /* requested at submit */
    struct job_runtime_usage usage;  /* reported by sbd */
    int run_nhosts;    /* the number of hosts where the job will run */
    struct mbd_host **run_hosts;
};

/*
 * Host resource capacities and current availability.
 * total_* is configured (llb.hosts), free_* is updated from sbd heartbeat.
 * used_* is derivable as total - free, computed at display/event time.
 * total_gpu/free_gpu are aggregates for fast scheduling checks;
 * gpu_list is walked only when a specific gpu_type is requested.
 */
struct host_resources {
    int      max_jobs;
    int      total_cpu;
    int      free_cpu;
    int      total_gpu;
    int      free_gpu;
    uint64_t total_mem_mb;
    uint64_t free_mem_mb;
    uint64_t total_storage_mb;
    uint64_t free_storage_mb;
    struct ll_list gpu_list;   /* list of mbd_gpu */
    struct ll_hash gpu_hash;   /* key gpu_type, value mbd_gpu */
};

/*
 * One GPU device (or MIG partition) on a host.
 * count/free track total configured vs available for scheduling.
 */
struct mbd_gpu {
    struct ll_list_entry ent;
    int  gpu_id;
    char model[LL_BUFSIZ_64];    /* RTX4090, H100, A100 */
    char gpu_type[LL_BUFSIZ_64]; /* full, 3g.40gb, 2g.20gb */
    int  count;                  /* configured */
    int  free;                   /* available for scheduling */
};

/* runtime state of a connected execution host */
struct mbd_host {
    struct ll_list_entry ent;
    struct ll_host        net;   /* resolved network identity */
    struct host_resources res;   /* capacity + availability + gpu_list */
    uint16_t port;               /* 0 = use LL_SBD_PORT; sim override otherwise */
    int    host_idx;             /* dense index assigned at conf_init */
    int    state;
    int    candidate;            /* set by mark_candidates() each sched cycle */
    int    exclusive;            /* host is exclusively allocated */
    int    num_jobs;
    int    num_run;
    int    num_susp;
    int    sbd_chan;      /* -1 if not connected */
    int    num_cpus_used;  /* CPUs consumed by running jobs on this host */
};

struct queue_conf {
    char name[LL_BUFSIZ_64];
    char desc[LL_BUFSIZ_256];
    char hosts_spec[LL_BUFSIZ_256]; /* group name or single hostname */
    char users[LL_BUFSIZ_256];      /* space-separated, empty = all */
    int  priority;
    int  state;
};

struct mbd_queue {
    struct ll_list_entry ent;
    char    name[LL_BUFSIZ_64];
    char    description[LL_BUFSIZ_256];
    char    hosts_spec[LL_BUFSIZ_256];
    char    users[LL_BUFSIZ_256];
    int     priority;
    int     max_jobs;
    int     num_jobs;
    int     num_pend;
    int     num_run;
    int     num_susp;
    int     num_held;
    int     state;
    int     num_cpus_used;    /* CPUs consumed by running jobs in this queue */
    int     num_hosts_used;   /* distinct exec hosts in use by running jobs  */
    struct ll_hash host_hash;  /* expanded host membership, keyed by hostname */
};

struct mbd_group {
    struct ll_list_entry ent;
    char name[LL_BUFSIZ_64];
    int  num_members;
    char members[LL_BUFSIZ_1K];     /* space-separated */
};

/*
 * Named countable resource — jobs consume N tokens on dispatch,
 * return them on finish. Configured in llb.hosts Begin TokenPool.
 */
struct mbd_token_pool {
    struct ll_list_entry ent;
    char name[LL_BUFSIZ_64];
    int  total;
    int  free;
};

#define JOB_BUCKETS 10
#define SCHED_PLAN_MAX 1024
#define SCHED_TIMER 5

struct sched_plan {
    struct mbd_host *hosts[SCHED_PLAN_MAX]; /* hosts[0] is exec host */
    int  nhosts;
};

// Pending reason and their priority order
struct pend_diag {
    int not_ready;
    int queue_closed;
    int no_token;
    int not_in_queue;
    int no_hosts;
    int no_gpus;
    int gpu_type;
    int exclusive;
    int no_mem;
    int no_storage;
    int no_cpus;
};

extern int64_t job_id_seq;
extern struct ll_hash job_id_hash;

extern struct ll_list pend_jobs_list;
extern struct ll_list run_jobs_list;
extern struct ll_list finish_jobs_list;

extern struct ll_list host_list;
extern struct ll_hash host_name_hash;
extern struct ll_hash host_addr_hash;

extern struct ll_hash sbd_chan_hash;

extern struct ll_list group_list;
extern struct ll_hash group_name_hash;

extern struct ll_list token_pool_list;
extern struct ll_hash token_pool_name_hash;

extern struct ll_list queue_list;
extern struct ll_hash queue_name_hash;

extern struct mbd_manager mbd_mgr;
extern int chan_mbd;
extern int mbd_efd;
extern uint16_t mbd_port;
extern int sched_timer;
extern int chan_timer;
extern char jobs_dir[];

// main.c
void mbd_die(enum mbd_exit);

// conf.c
int conf_init(void);
int init_manager(void);
int is_manager(uid_t);

// compact.c
void compact_start(void);
void compact_shutdown(void);
void handle_compact_done(XDR *, int32_t, struct protocol_header *);
void clean_jobs(time_t);
void reopen_job_events(void);

// net.c
int network_init(void);
int mbd_accept(int);
void mbd_message(int);
int enqueue_payload(int, struct protocol_header *,
                    void *, size_t, bool_t (*xdr_func)());
int32_t enqueue_header(int, int, int);
void chan_shutdown(int);
int valid_batch_op(int);

// sched.c
void schedule(void);
int mbd_dispatch_job(struct job_data *);

// events.c
int events_init(void);
void reopen_job_events(void);
void event_job_new(const struct job_data *, const struct wire_job_submit *);
void event_job_start(const struct job_data *);
void event_job_fork(const struct job_data *);
void event_job_execute(const struct job_data *, const char *);
void event_job_signal(const struct job_data *, const struct wire_job_sig *);
void event_job_finish(const struct job_data *);
void event_job_pend_susp(const struct job_data *);
void event_job_pend_resume(const struct job_data *);
void event_job_susp(const struct job_data *);
void maybe_compact_events(void);

// dispatch.c
int jobs_signal(XDR *, int);
int jobs_info(XDR *, int);
int hosts_info(XDR *, int);
int queues_info(XDR *, int);
int host_group_info(XDR *, int);
int mbd_sbd_register(XDR *, int);
int compact_done(XDR *, int);
int tokens_info(XDR *, int);

// job.c
int jobs_replay(void);
void new_job_reply(XDR *, int32_t);
int job_init(void);
int job_register(XDR *, int);
struct job_data *job_find(int64_t);
void job_set_list(struct job_data *, struct ll_list *, enum job_list_id);
void job_move_list(struct job_data *, struct ll_list *,
                   struct ll_list *, enum job_list_id);
void machines_hash_populate(struct ll_hash *, const char *);
void mbd_job_signal_reply(struct mbd_host *, XDR *, struct protocol_header *);
char *job_state_str(int);
void token_alloc(const struct job_data *);
void token_free(const struct job_data *);
void job_free(struct job_data *);

// sbd.c
int32_t mbd_sbd_route(struct mbd_host *);
int mbd_sbd_disconnect(struct mbd_host *);
void mbd_new_job_reply(struct mbd_host *, XDR *);
void mbd_job_execute(struct mbd_host *, XDR *);
void mbd_job_finish(struct mbd_host *, XDR *);

// queue.c
int queue_user_allowed(const struct mbd_queue *, uid_t);
int queue_admin(XDR *, int);
int host_admin(XDR *, int);

// debug counters
void mbd_assert_counters(void);

// admin.c
void host_state_write(const struct mbd_host *);
int host_state_init(void);
void queue_state_write(const struct mbd_queue *);
int queue_state_init(void);
