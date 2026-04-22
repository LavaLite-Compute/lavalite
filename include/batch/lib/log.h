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

#define LOG_VERSION 3

/*
 * Event types. Values are stable on disk -- do not reorder.
 */
enum event_type {
    EVENT_NULL            = 0,
    EVENT_JOB_NEW         = 1,  /* job submitted, enters PEND          */
    EVENT_JOB_START       = 2,  /* scheduler dispatched, enters RUN    */
    EVENT_JOB_ACCEPT      = 3,  /* sbd forked child, pid known         */
    EVENT_JOB_EXECUTE     = 4,  /* sbd confirmed execution, cwd known  */
    EVENT_JOB_SIGNAL      = 5,  /* mbd sent signal to job via sbd      */
    EVENT_JOB_FINISH      = 6,  /* job done, exit_status tells story   */
    EVENT_JOB_PEND_SUSP   = 7,  /* user suspended pending job          */
    EVENT_JOB_PEND_RESUME = 8, /* user resumed suspended pending job */
    EVENT_JOB_SUSP        = 9,  /* sbd suspended running job           */
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
 *
 * log_job_new: everything mbd needs for scheduling, display and replay.
 * Fields only needed at execution time (cwd, command, in/out/err files)
 * live in the per-job sidecar, not here.
 */
struct log_job_new {
    int64_t  job_id;
    uid_t    uid;
    gid_t    gid;
    int32_t  status;
    time_t   submit_time;
    time_t   begin_time;
    time_t   term_time;
    int32_t  num_cpu;
    int32_t  num_hosts;
    int32_t  num_gpus;
    uint64_t mem_mb;
    uint32_t flags;
    char     username[LL_BUFSIZ_64];
    char     job_name[LL_BUFSIZ_64];
    char     queue[LL_BUFSIZ_64];
    char     project_name[LL_BUFSIZ_64];
    char     gpu_type[LL_BUFSIZ_64];
    char     from_host[MAXHOSTNAMELEN];
    char     hosts[LL_BUFSIZ_1K];
    char     comment[LL_BUFSIZ_1K];
};

/*
 * log_job_start: mbd dispatched the job to sbd.
 * exec_host is the single host mbd sent the job to.
 */
struct log_job_start {
    int64_t job_id;
    char    exec_host[MAXHOSTNAMELEN];
};

/*
 * log_job_accept: sbd forked the child. pid is now known.
 * This is the barrier -- no signal or operation is valid before this.
 */
struct log_job_accept {
    int64_t job_id;
    int32_t job_pid;
};

/*
 * log_job_execute: sbd confirmed the job is executing.
 * Carries the confirmed runtime cwd.
 */
struct log_job_execute {
    int64_t job_id;
    int32_t job_pid;
    char    cwd[PATH_MAX];
};

/*
 * log_job_signal: mbd sent a signal to the job via sbd.
 * signal_num is the Unix signal number (SIGKILL, SIGSTOP, etc.)
 */
struct log_job_signal {
    int64_t job_id;
    int32_t signal_num;
};

/*
 * log_job_finish: job is done. Complete accounting record.
 */
struct log_job_finish {
    int64_t  job_id;
    uid_t    uid;
    int32_t status; /* JOB_STAT_EXIT, JOB_STAT_DONE */
    int32_t  exit_status;
    time_t   submit_time;
    time_t   start_time;
    time_t   end_time;
    double   cpu_time;
    char     job_name[LL_BUFSIZ_64];
    char     queue[LL_BUFSIZ_64];
    char     from_host[MAXHOSTNAMELEN];
    char     exec_host[MAXHOSTNAMELEN];
};

struct log_job_pend_susp {
    int64_t job_id;
};

struct log_job_pend_resume {
    int64_t job_id;
};

struct log_job_susp {
    int64_t job_id;
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
int log_parse_job_signal(const struct event_rec *, struct log_job_signal *);
int log_parse_job_finish(const struct event_rec *, struct log_job_finish *);
int log_parse_job_susp(const struct event_rec *, struct log_job_susp *);
int log_parse_job_pend_resume(const struct event_rec *,
                              struct log_job_pend_resume *);
int log_parse_job_pend_susp(const struct event_rec *,
                            struct log_job_pend_susp *);

/* Writers -- write header + payload + newline in one call */
int log_write_job_new(FILE *, const struct log_job_new *);
int log_write_job_start(FILE *, const struct log_job_start *);
int log_write_job_accept(FILE *, const struct log_job_accept *);
int log_write_job_execute(FILE *, const struct log_job_execute *);
int log_write_job_signal(FILE *, const struct log_job_signal *);
int log_write_job_finish(FILE *, const struct log_job_finish *);
int log_write_job_susp(FILE *, const struct log_job_susp *);
int log_write_job_pend_resume(FILE *,
                              const struct log_job_pend_resume *);
int log_write_job_pend_susp(FILE *, const struct log_job_pend_susp *);
