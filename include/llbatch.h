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
#define JOB_STAT_UNKNOWN 0x10000

#define JOB_IS_PEND(s) ((s) & (JOB_STAT_PEND | JOB_STAT_PSUSP))
#define JOB_IS_RUN(s)  ((s) & (JOB_STAT_RUN  | JOB_STAT_SUSP))
#define JOB_IS_DONE(s) ((s) & (JOB_STAT_DONE | JOB_STAT_EXIT))
#define JOB_IS_SUSP(s) ((s) & (JOB_STAT_PSUSP | JOB_STAT_SUSP))

/* -----------------------------------------------------------------------
 * Host status
 * ----------------------------------------------------------------------- */
enum host_stat {
    HOST_OK,
    HOST_UNAVAIL,
};

#define HOST_CLOSED  0x01   /* admin disabled/locked — orthogonal to state */

/* -----------------------------------------------------------------------
 * Error codes rename BATCH_ERR_NONE BATCH_ERR_NO_JOB BE or BERR
 * ----------------------------------------------------------------------- */
enum llb_error {
    LLBE_NONE = 0,
    LLBE_NO_JOB,
    LLBE_NOT_STARTED,
    LLBE_JOB_STARTED,
    LLBE_JOB_FINISH,
    LLBE_HOST,
    LLBE_HOST_GROUP,
    LLBE_QUEUE,
    LLBE_SIGNAL,
    LLBE_SYS_CALL,
    LLBE_PROTOCOL,
    LLBE_NUM_ERR,   /* must remain last */
};

extern __thread enum llb_error lberrno;

/* -----------------------------------------------------------------------
 * Structs
 * ----------------------------------------------------------------------- */

struct job_submit {
    char *job_name;
    char *queue;
    char *hosts;
    char *group;
    int32_t num_cpu;
    int32_t num_hosts;
    uint64_t  mem_mb;
    int32_t num_gpu;
    time_t begin_time;
    time_t term_time;
    char *in_file;
    char *out_file;
    char *err_file;
    char *command;
    char *project_name;
};

struct job_resource_info {
    pid_t pgid;
    /* future: cpu_usage, mem_bytes, gpu_usage from cgroup
     */
};

// llb_job_info API options
#define LLB_JOB_ALL  0x0001
#define LLB_JOB_DONE 0x0002
#define LLB_JOB_PEND 0x0004
#define LLB_JOB_SUSP 0x0008
#define LLB_JOB_RUN  0x0040

struct job_info {
    int64_t job_id;
    uid_t uid;
    int32_t status;
    char *name;
    char *project;
    char *comment;
    struct job_resource_info resources;
    time_t submit_time;
    time_t start_time;
    time_t end_time;
    float cpu_time;
    char *cwd;
    char *from_host;
    int32_t num_exec_hosts;
    char *exec_hosts;
    struct job_submit submit;
    int32_t exit_status;
};

struct host_info {
    char *name;
    int32_t status;
    int32_t max_jobs;
    int32_t num_jobs;
    int32_t num_run;
    int32_t num_susp;
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
int64_t llb_submit(struct job_submit *);

// bjobs
struct job_info *llb_job_info(int64_t, int32_t *, int32_t);
void llb_free_job_info(struct job_info *, int32_t);

// bhosts
struct host_info *llb_host_info(int32_t *);
void llb_free_host_info(struct host_info *, int32_t);

struct host_group *llb_group_info(int32_t *);
void llb_free_group_info(struct host_group *, int32_t);

// bqueues
struct queue_info *llb_queue_info(int32_t *);
void llb_free_queue_info(struct queue_info *, int32_t);

// bkill
int32_t llb_signal_job(int64_t, int32_t);

// llb errors
const char *llbe_str(enum llb_error);
