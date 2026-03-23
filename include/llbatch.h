/*
 * Copyright (C) LavaLite Contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 */
#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>

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
 * Queue status and attributes
 * ----------------------------------------------------------------------- */
enum queue_stat {
    QUEUE_OPEN,
    QUEUE_CLOSED,
};

/* -----------------------------------------------------------------------
 * Error codes rename BATCH_ERR_NONE BATCH_ERR_NO_JOB BE or BERR
 * ----------------------------------------------------------------------- */
enum batch_error {
    LLBE_NONE = 0,
    LLBE_NO_JOB,
    LLBE_NOT_STARTED,
    LLBE_JOB_STARTED,
    LLBE_JOB_FINISH,
    LLBE_NO_USER,
    LLBE_BAD_USER,
    LLBE_BAD_QUEUE,
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

struct job_resources {
    pid_t pgid;
    /* future: cpu_usage, mem_bytes, gpu_usage from cgroup
     */
};

#define LLB_JOB_ALL  0x0001
#define LLB_JOB_DONE 0x0002
#define LLB_JOB_PEND 0x0004
#define LLB_JOB_SUSP 0x0008
#define LLB_JOB_RUN  0x0040

struct job_info {
    int64_t job_id;
    uid_t uid;
    int32_t status;
    struct job_resources resources;
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
    char *host;
    int32_t status;
    int32_t max_jobs;
    int32_t num_jobs;
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

int64_t batch_submit(struct job_submit *);

struct job_info *batch_job_info(int64_t, int32_t, int32_t);
void batch_free_job_info(struct job_info *, int32_t);

struct host_info *batch_host_info(int32_t *);
struct host_group *batch_group_info(int32_t *);

struct queue_info batch_queue_info(int32_t *);

int32_t batch_signal_job(int64_t, int32_t);
