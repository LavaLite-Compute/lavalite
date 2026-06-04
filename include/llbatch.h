/*
 * Copyright (C) LavaLite Contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 */
#pragma once
#include "config.h"

/* -----------------------------------------------------------------------
 * Job state
 * -----------------------------------------------------------------------
 */
enum job_state {
    JOB_PENDING = 1,
    JOB_HELD,
    JOB_RUNNING,
    JOB_SUSPENDED,
    JOB_EXITED,
    JOB_DONE,
    JOB_ORPHAN,
    JOB_UNKNOWN,
};

/* These are the classical unix job exit status values
 */
#define JOB_SUCCESS 0
#define JOB_FAILURE 1
enum pend_reason {
    PEND_NONE = 0,
    PEND_JOB_NOT_READY,
    PEND_QUEUE_CLOSED,
    PEND_TOKENS,
    PEND_NO_HOSTS,
    PEND_NOT_ENOUGH_CPUS,
    PEND_NOT_ENOUGH_MEM,
    PEND_NOT_ENOUGH_STORAGE,
    PEND_NOT_ENOUGH_GPUS,
    PEND_GPU_TYPE,
    PEND_HOST_EXCLUSIVE,
};

// Pending messages table
extern const char *pend_reason_msg[];

/* -----------------------------------------------------------------------
 * Host status
 * ----------------------------------------------------------------------- */
enum host_stat {
    HOST_OK = 0,
    HOST_UNAVAIL = 2,
};

#define HOST_CLOSED 0x01 /* admin disabled/locked — orthogonal to state */

/* -----------------------------------------------------------------------
 * Structs
 * ----------------------------------------------------------------------- */
/*
 * job_submit: populated by bsub from command line options.
 * All char * fields are pointers into argv or heap; caller owns memory.
 * Zero/NULL means "not specified; use default".
 */

#define JOB_FLAG_EXCLUSIVE 0x01
#define JOB_FLAG_HOLD 0x02

struct job_submit {
    char *name;          /* --name        */
    char *queue;         /* --queue       */
    char *machines;      /* --machines    */
    char *gpu_type;      /* --gpu-type    */
    char *depend_cond;   /* --dependency  */
    char *in_file;       /* --stdin       */
    char *out_file;      /* --stdout      */
    char *err_file;      /* --stderr      */
    char *command;       /* command [args]*/
    char *project;       /* --project     */
    char *comment;       /* --comment     */
    int32_t num_cpus;    /* --cpus        (per host) */
    int32_t num_hosts;   /* --nhosts      */
    int32_t num_gpus;    /* --gpus        (per host) */
    uint64_t mem_mb;     /* --mem         (per host) */
    uint64_t storage_mb; /* --storage     (per host) */
    char *tokenpool;     /* --pool name=N[,name=N]  */
    time_t begin_time;   /* --begin       */
    time_t term_time;    /* --terminate   */
    uint32_t flags;      /* JOB_FLAG_*    */
};

// llb_job_info API options
#define LLB_JOB_DONE 0x0001
#define LLB_JOB_PEND 0x0002
#define LLB_JOB_SUSP 0x0004
#define LLB_JOB_RUN 0x0008
#define LLB_JOB_HELD 0x0010

struct job_info_req {
    int64_t job_id;  /* 0 = all */
    int32_t uid;     /* -1 = all */
    int32_t flags;   /* LLB_JOB_* */
};

/* runtime resource usage, reported sbd via cgroup at the end of the job
 */
struct job_res_usage {
    uint64_t mem_mb;
    uint64_t swap_mb;
    double cpu_time;
};

struct job_info {
    int64_t job_id;
    uid_t uid;
    pid_t pid;
    int32_t state;
    int32_t exit_status;
    int32_t priority;
    enum pend_reason pend_reason;
    time_t submit_time;
    time_t dispatch_time;
    time_t end_time;
    time_t susp_time;
    char *name;
    char *queue;
    char *submit_host;
    char *run_hosts;
    char *comment;
};

struct host_info {
    char *name;                /* hostname */
    int32_t state;             /* HOST_OK | HOST_UNAVAIL | HOST_CLOSED */
    int32_t max_jobs;          /* max concurrent jobs, 0 = unlimited */
    int32_t total_cpu;         /* total CPUs available on host */
    int32_t free_cpu;          /* CPUs available for scheduling */
    int32_t total_gpu;         /* total GPUs available on host */
    int32_t free_gpu;          /* GPUs available for scheduling */
    uint64_t total_mem_mb;     /* total RAM in MB */
    uint64_t free_mem_mb;      /* RAM available for scheduling */
    uint64_t total_storage_mb; /* total scratch storage in MB */
    uint64_t free_storage_mb;  /* scratch storage available for scheduling */
    int32_t num_jobs;          /* total jobs: run + susp */
    int32_t num_run;           /* running jobs */
    int32_t num_susp;          /* suspended jobs */
    char *gpu_type;
    char *gpu_ids;
};

enum queue_stat {
    QUEUE_OPEN,
    QUEUE_CLOSED,
};

struct queue_info {
    char *name;             /* queue name */
    char *description;      /* human readable description */
    char **hosts;           /* expanded host list */
    int32_t num_hosts;
    char **users;           /* expanded user list, num_users=0 means "all" */
    int32_t num_users;
    int32_t status;         /* QUEUE_OPEN | QUEUE_CLOSED */
    int32_t priority;       /* scheduling priority, higher wins */
    int32_t max_jobs;       /* max concurrent jobs, 0 = unlimited */
    int32_t num_jobs;       /* total: pend + held + run + susp */
    int32_t num_pend;       /* pending jobs */
    int32_t num_held;       /* held jobs */
    int32_t num_run;        /* running jobs */
    int32_t num_susp;       /* suspended jobs */
    int32_t num_cpus_used;  /* CPUs consumed by running jobs */
    int32_t num_hosts_used; /* distinct exec hosts in use */
};

struct job_signal {
    int64_t job_id;
    int signal;
};

struct host_group {
    char *name;
    char *members;
};

struct token_pool_info {
    char *name;    /* pool name */
    int32_t total; /* total tokens configured */
    int32_t free;  /* tokens available */
    int32_t used;  /* total - free */
};

/* llb_hist_info flags
 */
#define LLB_HIST_ALL  0x0001

/*
 * One event in the life of a job, decoded from the event log.
 * type is EVENT_JOB_* from log.h. Fields are populated according
 * to type; unused fields are zero/NULL.
 */
struct job_event {
    int32_t        type;          /* EVENT_JOB_* from log.h         */
    time_t         event_time;
    int32_t        state;         /* JOB_START, JOB_FINISH          */
    int32_t        exit_status;   /* JOB_FINISH                     */
    int32_t        signal;        /* JOB_SIGNAL                     */
    pid_t          pid;           /* JOB_FORK                       */
    char          *run_hosts;     /* JOB_START                      */
    char *from_queue;   /* JOB_MOVE */
    char *to_queue;     /* JOB_MOVE */
    int32_t old_priority;   /* JOB_PRIORITY */
    int32_t new_priority;   /* JOB_PRIORITY */
    char *gpu_assigned; /* JOB_START: assigned CUDA device IDs */
};

/*
 * Job submission context (from JOB_NEW + submit sidecar) plus
 * the ordered sequence of events in the job's life.
 * usage is from the usage sidecar, valid after job finishes.
 */
struct job_hist_info {
    int64_t        job_id;
    uid_t          uid;
    int32_t        state;          /* current state  */
    int32_t        priority;
    time_t         submit_time;
    int32_t        num_cpus;
    int32_t        num_hosts;
    int32_t        num_gpus;
    uint64_t       mem_mb;
    uint64_t       storage_mb;
    char          *username;
    char          *name;
    char          *queue;
    char          *project;
    char          *submit_host;   /* submit host (where bsub was run)       */
    char          *machines;      /* --machines constraint                  */
    char          *cwd;
    char          *command;
    char          *depend_cond;   /* --dependency                           */
    char          *in_file;
    char          *out_file;
    char          *err_file;
    char          *comment;
    char          *gpu_type;      /* --gpu-type                             */
    char          *tokenpool;     /* --pool                                 */
    time_t         begin_time;    /* --begin                                */
    time_t         term_time;     /* --terminate                            */
    struct job_res_usage usage;   /* from usage sidecar, valid after finish */
    int32_t        num_events;
    struct job_event *events;
};

/* -----------------------------------------------------------------------
 * Public API
 * ----------------------------------------------------------------------- */

// bsub
int32_t llb_submit(const struct job_submit *, int64_t *);

// bjobs
struct job_info *llb_job_info(const struct job_info_req *, int32_t *);
void llb_free_job_info(struct job_info *, int32_t);

// bhosts
struct host_info *llb_host_info(int32_t *);
void llb_free_host_info(struct host_info *, int32_t);

// bmgroup
struct host_group *llb_group_info(int32_t *);
void llb_free_group_info(struct host_group *, int32_t);

// bqueues
struct queue_info *llb_queue_info(int32_t *);
void llb_free_queue_info(struct queue_info *, int32_t);

// bkill
int32_t llb_signal_job(int64_t, int32_t);

/* btokens */
struct token_pool_info *llb_token_info(int32_t *);
void llb_free_token_info(struct token_pool_info *, int32_t);

/* admin */
int32_t llb_queue_admin(const char *, int32_t);
int32_t llb_host_admin(const char *, int32_t);

/* bhist */
struct job_hist_info *llb_hist_info(int64_t job_id, uid_t uid,
                                    int32_t flags, int32_t *num);
void llb_free_hist_info(struct job_hist_info *, int32_t);

// bmove
int32_t llb_move_job(int64_t, const char *);

// bpriority
int32_t llb_priority_job(int64_t, int32_t);
