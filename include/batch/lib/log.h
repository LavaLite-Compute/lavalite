/*
 * Copyright (C) LavaLite Contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 */
#pragma once

#include <stdint.h>
#include <sys/types.h>
#include <sys/param.h>
#include <time.h>

#include "base/lib/ll.bufsiz.h"

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
 * event_rec: the record header.
 * log_read_hdr() reads one line from the file, parses the header fields,
 * and stores the unparsed remainder in rest[] for the payload reader.
 */
struct event_rec {
    int             version;
    enum event_type type;
    time_t          event_time;
    char            rest[LL_BUFSIZ_4K]; /* unparsed payload tail */
};

/*
 * Per-event payload structs.
 * Each contains exactly the fields written by its event.
 */

struct log_job_new {
    int64_t  job_id;
    uid_t    uid;
    int32_t  status;
    time_t   submit_time;
    time_t   begin_time;
    time_t   term_time;
    int32_t  num_cpu;
    int32_t  num_hosts;
    uint64_t mem_mb;
    char     job_name[LL_BUFSIZ_64];
    char     queue[LL_BUFSIZ_64];
    char     from_host[MAXHOSTNAMELEN];
    char     cwd[PATH_MAX];
    char     command[LL_BUFSIZ_1K];
    char     in_file[PATH_MAX];
    char     out_file[PATH_MAX];
    char     err_file[PATH_MAX];
    char     project_name[LL_BUFSIZ_64];
    char     hosts[LL_BUFSIZ_1K];
};

struct log_job_start {
    int64_t job_id;
    int32_t status;
    int32_t job_pid;
    int32_t num_exec_hosts;
    char    exec_hosts[LL_BUFSIZ_1K];
};

struct log_job_accept {
    int64_t job_id;
    int32_t job_pid;
};

struct log_job_execute {
    int64_t job_id;
    int32_t job_pid;
    char    cwd[PATH_MAX];
};

struct log_job_status {
    int64_t job_id;
    int32_t status;
    int32_t exit_status;
    double  cpu_time;
    time_t  end_time;
};

struct log_job_finish {
    int64_t  job_id;
    uid_t    uid;
    int32_t  status;
    int32_t  exit_status;
    time_t   submit_time;
    time_t   start_time;
    time_t   end_time;
    double   cpu_time;
    char     job_name[LL_BUFSIZ_64];
    char     queue[LL_BUFSIZ_64];
    char     from_host[MAXHOSTNAMELEN];
    char     exec_hosts[LL_BUFSIZ_1K];
    char     cwd[PATH_MAX];
    char     command[LL_BUFSIZ_1K];
};

/*
 * Read the record header from one line. Skips comments and blank lines.
 * The unparsed payload tail is stored in rec->rest.
 * Returns 0 on success, -1 on EOF or parse error.
 */
int log_read_hdr(FILE *, int *lineno, struct event_rec *);

/* Payload parsers -- operate on rec->rest from log_read_hdr */
int log_parse_job_new(const struct event_rec *, struct log_job_new *);
int log_parse_job_start(const struct event_rec *, struct log_job_start *);
int log_parse_job_accept(const struct event_rec *, struct log_job_accept *);
int log_parse_job_execute(const struct event_rec *, struct log_job_execute *);
int log_parse_job_status(const struct event_rec *, struct log_job_status *);
int log_parse_job_finish(const struct event_rec *, struct log_job_finish *);

/* Writers -- write header + payload + newline in one call */
int log_write_job_new(FILE *, const struct log_job_new *);
int log_write_job_start(FILE *, const struct log_job_start *);
int log_write_job_accept(FILE *, const struct log_job_accept *);
int log_write_job_execute(FILE *, const struct log_job_execute *);
int log_write_job_status(FILE *, const struct log_job_status *);
int log_write_job_finish(FILE *, const struct log_job_finish *);
