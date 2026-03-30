/* Copyright (C) LavaLite Contributors
 * GPL v2
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pwd.h>

#include "llbatch.h"

static const char *
uid_to_name(uid_t uid)
{
    struct passwd *pw;

    pw = getpwuid(uid);
    if (!pw)
        return "unknown";
    return pw->pw_name;
}

static const char *
job_status_str(int32_t status)
{
    switch (status) {
    case JOB_STAT_PEND:    return "PEND";
    case JOB_STAT_PSUSP:   return "PSUSP";
    case JOB_STAT_RUN:     return "RUN";
    case JOB_STAT_SUSP:    return "SUSP";
    case JOB_STAT_EXIT:    return "EXIT";
    case JOB_STAT_DONE:    return "DONE";
    default:               return "UNKNOWN";
    }
}

static const char *
fmt_time(time_t t)
{
    static char buf[32];

    if (t == 0)
        return "-";
    strftime(buf, sizeof(buf), "%b %d %H:%M", localtime(&t));
    return buf;
}

static int
imax(int a, int b)
{
    return a > b ? a : b;
}

static int
ndigits(int64_t n)
{
    if (n <= 0)
        return 1;
    int d = 0;
    while (n > 0) { d++; n /= 10; }
    return d;
}

struct col_widths {
    int jobid;
    int user;
    int stat;
    int queue;
    int from_host;
    int exec_host;
    int name;
    int submit_time;
};

static void
compute_widths(struct job_info *jobs, int n, struct col_widths *w)
{
    w->jobid       = imax(ndigits(0), strlen("JOBID"));
    w->user        = strlen("USER");
    w->stat        = strlen("STAT");
    w->queue       = strlen("QUEUE");
    w->from_host   = strlen("FROM_HOST");
    w->exec_host   = strlen("EXEC_HOST");
    w->name        = strlen("JOB_NAME");
    w->submit_time = strlen("SUBMIT_TIME");

    for (int i = 0; i < n; i++) {
        struct job_info *j = &jobs[i];
        w->jobid = imax(w->jobid, ndigits(j->job_id));
        w->user = imax(w->user, strlen(uid_to_name(j->uid)));
        w->stat  = imax(w->stat, strlen(job_status_str(j->status)));
        w->queue = imax(w->queue, strlen(j->queue));
        w->from_host = imax(w->from_host, strlen(j->from_host));
        w->exec_host = imax(w->exec_host, j->exec_host ? strlen(j->exec_host) : 1);
        w->name = imax(w->name,  strlen(j->name));
    }
}

int
main(void)
{
    int njobs;
    struct job_info *jobs;
    struct col_widths w;

    jobs = llb_job_info(-1, &njobs, LLB_JOB_ALL);
    if (!jobs) {
        fprintf(stderr, "bjobs: failed\n");
        return -1;
    }
    compute_widths(jobs, njobs, &w);
    printf("%-*s  %-*s  %-*s  %-*s  %-*s  %-*s  %-*s  %s\n",
           w.jobid,     "JOBID",
           w.user,      "USER",
           w.stat,      "STAT",
           w.queue,     "QUEUE",
           w.from_host, "FROM_HOST",
           w.exec_host, "EXEC_HOST",
           w.name,      "JOB_NAME",
           "SUBMIT_TIME");

    for (int i = 0; i < njobs; i++) {
        struct job_info *j = &jobs[i];
        printf("%-*ld  %-*s  %-*s  %-*s  %-*s  %-*s  %-*s  %s\n",
               w.jobid,     j->job_id,
               w.user,      uid_to_name(j->uid),
               w.stat,      job_status_str(j->status),
               w.queue,     j->queue,
               w.from_host, j->from_host,
               w.exec_host, j->exec_host ? j->exec_host : "-",
               w.name,      j->name,
               fmt_time(j->submit_time));
    }
    llb_free_job_info(jobs, njobs);
    return 0;
}
