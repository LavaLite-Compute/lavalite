/* Copyright (C) LavaLite Contributors
 * GPL v2
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pwd.h>
#include <getopt.h>

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
    while (n > 0) { d++;
        n /= 10;
    }
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

static struct option longopts[] = {
    {"help", no_argument, NULL, 'h'},
    {"version", no_argument, NULL, 'V'},
    {"all", no_argument, NULL, 'a'},
    {"pend", no_argument, NULL, 'p'},
    {"run", no_argument, NULL, 'r'},
    {"done", no_argument, NULL, 'd'},
    {NULL, 0, NULL, 0}
};
static void usage(void)
{
    fprintf(stderr, "bjobs: output the jobs belonging to the use running "
            "the command\n"
            "--help display this help and exit\n"
            "--version output version information and exit\n"
            "--pend output all pending jobs in the system\n"
            "--run output all running jobs in the system\n"
            "--done output all done jobs in the system\n"
            "job_id the job_id option this mutually exclusive with the\n"
            "--all, --pend, --run and --done\n");
}

int main(int argc, char **argv)
{
    int cc;
    while ((cc = getopt_long(argc, argv, "hV", longopts, NULL)) != EOF) {
        switch (cc) {
        case 'V':
            fprintf(stderr, "%s\n", LAVALITE_VERSION_STR);
            return 0;
        case 'h':
        default:
            usage();
            return 0;
        }
    }

    if (optind >= argc) {
        usage();
        return -1;
    }

    char *end;
    int64_t job_id = strtoll(argv[optind], &end, 10);
    if (end == argv[optind] || *end != 0) {
        fprintf(stderr, "bkill: invalid jobid '%s'\n", argv[optind]);
        return -1;
    }

    int njobs;
    struct job_info *jobs = llb_job_info(job_id, &njobs, LLB_JOB_NOFLAGS);
    if (!jobs) {
        fprintf(stderr, "bjobs: failed\n");
        return -1;
    }

    struct col_widths w;
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
