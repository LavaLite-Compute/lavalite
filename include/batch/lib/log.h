/*
 * Copyright (C) LavaLite Contributors
 * GPL v2
 */

#pragma once

#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <sys/types.h>
#include <limits.h>

#ifndef MAXHOSTNAMELEN
#define MAXHOSTNAMELEN 256
#endif

#define LOG_VERSION     2
#define LOG_VERSION_STR "2"

/*
 * Event types. Values are stable on disk -- do not reorder.
 */
enum event_type {
    EVENT_NULL        = 0,
    EVENT_JOB_NEW     = 1,
    EVENT_JOB_START   = 2,
    EVENT_JOB_ACCEPT  = 3,
    EVENT_JOB_EXECUTE = 4,
    EVENT_JOB_STATUS  = 5,
    EVENT_JOB_FINISH  = 6,
    EVENT_COUNT
};

/*
 * Self-contained fixed-buffer job record for the log.
 * Independent of wire structs and job_info -- intentionally stable.
 * Not all fields are meaningful for every event type.
 */
struct log_job {
    int64_t  job_id;
    uid_t    uid;
    int32_t  status;
    int32_t  exit_status;

    time_t   submit_time;
    time_t   start_time;
    time_t   end_time;
    float    cpu_time;

    /* execution context */
    char     from_host[MAXHOSTNAMELEN];
    char     cwd[PATH_MAX];
    char     exec_hosts[PATH_MAX];   /* space-separated */
    int32_t  num_exec_hosts;
    int32_t  job_pid;

    /* submission fields */
    char     job_name[64];
    char     queue[64];
    char     command[1024];
    char     in_file[PATH_MAX];
    char     out_file[PATH_MAX];
    char     err_file[PATH_MAX];
    char     project_name[64];
    char     hosts[256];             /* requested, space-separated */
    int32_t  num_cpu;
    int32_t  num_hosts;
    int32_t  num_gpu;
    uint64_t mem_mb;
    time_t   begin_time;
    time_t   term_time;
};

struct event_rec {
    int             version;
    enum event_type type;
    time_t          event_time;
    struct log_job  job;
};

struct event_rec *log_read(FILE *, int *lineno);
int               log_write(FILE *, struct event_rec *);
