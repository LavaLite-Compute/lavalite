/*
 * Copyright (C) LavaLite Contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 */
#pragma once

#define LOG_VERSION 2

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
    double   cpu_time;
    /* execution context */
    char     from_host[MAXHOSTNAMELEN];
    char     exec_hosts[LL_BUFSIZ_1K];       /* space-separated */
    char     cwd[PATH_MAX];
    int32_t  num_exec_hosts;
    int32_t  job_pid;
    /* submission fields */
    char     job_name[LL_BUFSIZ_64];
    char     queue[LL_BUFSIZ_64];
    char     command[LL_BUFSIZ_1K];
    char     in_file[PATH_MAX];
    char     out_file[PATH_MAX];
    char     err_file[PATH_MAX];
    char     project_name[LL_BUFSIZ_64];
    char     hosts[LL_BUFSIZ_1K];            /* requested, space-separated */
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

int log_read(FILE *, int *, struct event_rec *);
int log_write(FILE *, struct event_rec *);
