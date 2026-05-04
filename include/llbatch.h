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
 * Job status
 * ----------------------------------------------------------------------- */
#define JOB_STAT_PEND    0x01   /* pending */
#define JOB_STAT_PSUSP   0x02   /* suspended while pending */
#define JOB_STAT_RUN     0x04   /* running */
#define JOB_STAT_SUSP    0x08   /* suspended by system */
#define JOB_STAT_EXIT    0x20   /* exited */
#define JOB_STAT_DONE    0x40   /* done */
#define JOB_STAT_ORPHAN  0x80   /* where is my queue/or host? */
#define JOB_STAT_UNKNOWN 0x10000

#define JOB_IS_PEND(s) ((s) & (JOB_STAT_PEND | JOB_STAT_PSUSP))
#define JOB_IS_RUN(s)  ((s) & (JOB_STAT_RUN  | JOB_STAT_SUSP))
#define JOB_IS_DONE(s) ((s) & (JOB_STAT_DONE | JOB_STAT_EXIT))
#define JOB_IS_SUSP(s) ((s) & (JOB_STAT_PSUSP | JOB_STAT_SUSP))

/* -----------------------------------------------------------------------
 * Host status
 * ----------------------------------------------------------------------- */
enum host_stat {
    HOST_OK = 0,
    HOST_UNAVAIL = 2,
};

#define HOST_CLOSED  0x01   /* admin disabled/locked — orthogonal to state */

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
    char        *name;          /* --name        */
    char        *queue;         /* --queue       */
    char        *machines;      /* --machines    */
    char        *gpu_type;      /* --gpu-type    */
    char        *depend_cond;   /* --dependency  */
    char        *in_file;       /* --stdin       */
    char        *out_file;      /* --stdout      */
    char        *err_file;      /* --stderr      */
    char        *command;       /* command [args]*/
    char        *project;       /* --project     */
    char        *comment;       /* --comment     */
    int32_t      num_cpus;      /* --cpus        (per host) */
    int32_t      num_hosts;    /* --nhosts      */
    int32_t      num_gpus;      /* --gpus        (per host) */
    int32_t      wall_seconds;  /* --wall        */
    uint64_t     mem_mb;        /* --mem         (per host) */
    uint64_t     storage_mb;    /* --storage     (per host) */
    char        *tokenpool;     /* --pool name=N[,name=N]  */
    time_t       begin_time;    /* --begin       */
    time_t       term_time;     /* --terminate   */
    uint32_t     flags;         /* JOB_FLAG_*    */
};

// llb_job_info API options
#define LLB_JOB_DONE 0x0001
#define LLB_JOB_PEND 0x0002
#define LLB_JOB_SUSP 0x0004
#define LLB_JOB_RUN  0x0008

/* runtime resource usage, reported periodically by sbd via cgroup */
struct job_res_info {
    pid_t    pid;
    uint64_t mem_mb;
    double   cpu_time;
};

struct job_info {
    int64_t  job_id;
    uid_t    uid;
    int32_t  status;
    int32_t  exit_status;
    int32_t  priority;
    time_t   submit_time;
    time_t   start_time;
    time_t   end_time;
    time_t   susp_time;
    char    *name;
    char    *queue;
    char    *from_host;
    char    *exec_host;
    char    *comment;
    struct job_res_info res;
};

struct host_info {
    char    *name;
    int32_t  status;
    int32_t  max_jobs;
    int32_t  total_cpu;
    int32_t  total_gpu;
    uint64_t total_mem_mb;
    uint64_t total_storage_mb;
    int32_t  num_jobs;
    int32_t  num_run;
    int32_t  num_susp;
};

enum queue_stat {
    QUEUE_OPEN,
    QUEUE_CLOSED,
};

struct queue_info {
    char *name;
    char *description;
    char *hosts;
    int32_t status;
    int32_t priority;
    int32_t max_jobs;
    int32_t num_pend;
    int32_t num_run;
    int32_t num_susp;
};

struct job_signal {
    int64_t job_id;
    int signal;
};

struct host_group {
    char *name;
    char *members;
};

/* -----------------------------------------------------------------------
 * Public API
 * ----------------------------------------------------------------------- */

// bsub
int32_t llb_submit(const struct job_submit *, int64_t *);

// bjobs
struct job_info *llb_job_info(int64_t, int32_t *, int32_t);
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
