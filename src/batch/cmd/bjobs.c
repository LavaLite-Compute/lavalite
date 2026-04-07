/* Copyright (C) LavaLite Contributors
 * GPL v2
 */
#include <errno.h>
#include <getopt.h>
#include <pwd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "llbatch.h"

static const char *
uid_to_name(uid_t uid)
{
    struct passwd *pw;

    pw = getpwuid(uid);
    if (pw == NULL)
        return "unknown";
    return pw->pw_name;
}

static const char *
job_status_str(int32_t status)
{
    switch (status) {
    case JOB_STAT_PEND:
        return "PEND";
    case JOB_STAT_PSUSP:
        return "PSUSP";
    case JOB_STAT_RUN:
        return "RUN";
    case JOB_STAT_SUSP:
        return "SUSP";
    case JOB_STAT_EXIT:
        return "EXIT";
    case JOB_STAT_DONE:
        return "DONE";
    default:
        return "UNKNOWN";
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
    if (a > b)
        return a;
    return b;
}

static int
ndigits(int64_t n)
{
    int d;

    if (n <= 0)
        return 1;
    d = 0;
    while (n > 0) {
        d++;
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
};

static void
compute_widths(struct job_info *jobs, int n, struct col_widths *w)
{
    int i;
    struct job_info *j;
    const char *exec_host;

    w->jobid     = (int)strlen("JOBID");
    w->user      = (int)strlen("USER");
    w->stat      = (int)strlen("STAT");
    w->queue     = (int)strlen("QUEUE");
    w->from_host = (int)strlen("FROM_HOST");
    w->exec_host = (int)strlen("EXEC_HOST");
    w->name      = (int)strlen("JOB_NAME");

    for (i = 0; i < n; i++) {
        j = &jobs[i];

        w->jobid     = imax(w->jobid,     ndigits(j->job_id));
        w->user      = imax(w->user,      (int)strlen(uid_to_name(j->uid)));
        w->stat      = imax(w->stat,      (int)strlen(job_status_str(j->status)));
        w->queue     = imax(w->queue,     (int)strlen(j->queue));
        w->from_host = imax(w->from_host, (int)strlen(j->from_host));
        w->name      = imax(w->name,      (int)strlen(j->name));

        exec_host = (j->exec_host[0] != '\0') ? j->exec_host : "-";
        w->exec_host = imax(w->exec_host, (int)strlen(exec_host));
    }
}

static void
print_header(const struct col_widths *w)
{
    printf("%-*s  %-*s  %-*s  %-*s  %-*s  %-*s  %-*s  %s\n",
           w->jobid,     "JOBID",
           w->user,      "USER",
           w->stat,      "STAT",
           w->queue,     "QUEUE",
           w->from_host, "FROM_HOST",
           w->exec_host, "EXEC_HOST",
           w->name,      "JOB_NAME",
           "SUBMIT_TIME");
}

static void
print_job(const struct job_info *j, const struct col_widths *w)
{
    const char *exec_host;

    exec_host = (j->exec_host[0] != '\0') ? j->exec_host : "-";

    printf("%-*ld  %-*s  %-*s  %-*s  %-*s  %-*s  %-*s  %s\n",
           w->jobid,     j->job_id,
           w->user,      uid_to_name(j->uid),
           w->stat,      job_status_str(j->status),
           w->queue,     j->queue,
           w->from_host, j->from_host,
           w->exec_host, exec_host,
           w->name,      j->name,
           fmt_time(j->submit_time));
}

static void
usage(void)
{
    fprintf(stderr,
        "Usage: bjobs [options] [job_id]\n"
        "\n"
        "Display jobs. Without options, shows active jobs for the"
        " current user.\n"
        "\n"
        "Options:\n"
        "  --all          Show active jobs for all users (admin only)\n"
        "  --pend         Show pending jobs only\n"
        "  --run          Show running jobs only\n"
        "  --done         Show finished jobs (DONE and EXIT)\n"
        "  --help         Display this help and exit\n"
        "  --version      Output version information and exit\n"
        "\n"
        "Arguments:\n"
        "  job_id         Show a specific job; mutually exclusive with"
        " filter options\n");
}

static struct option longopts[] = {
    {"help",    no_argument, NULL, 'h'},
    {"version", no_argument, NULL, 'V'},
    {"all",     no_argument, NULL, 'a'},
    {"pend",    no_argument, NULL, 'p'},
    {"run",     no_argument, NULL, 'r'},
    {"done",    no_argument, NULL, 'd'},
    {NULL, 0, NULL, 0}
};

int
main(int argc, char **argv)
{
    int flags = 0;
    int cc;

    while ((cc = getopt_long(argc, argv, "hVaprd", longopts, NULL)) != EOF) {
        switch (cc) {
        case 'V':
            fprintf(stderr, "%s\n", LAVALITE_VERSION_STR);
            return 0;
        case 'h':
            usage();
            return 0;
        case 'a':
            flags |= LLB_JOB_PEND | LLB_JOB_RUN |LLB_JOB_DONE;
            break;
        case 'p':
            flags |= LLB_JOB_PEND;
            break;
        case 'r':
            flags |= LLB_JOB_RUN;
            break;
        case 'd':
            flags |= LLB_JOB_DONE;
            break;
        default:
            usage();
            return 1;
        }
    }

    int64_t job_id = -1;
    if (optind < argc) {
        if (flags) {
            fprintf(stderr,
                    "bjobs: job_id is mutually exclusive with"
                    " filter options\n");
            return 1;
        }
        char *end;
        job_id = strtoll(argv[optind], &end, 10);
        if (end == argv[optind] || *end != '\0') {
            fprintf(stderr, "bjobs: invalid job_id '%s'\n", argv[optind]);
            return 1;
        }
    }

    int njobs;
    struct job_info *jobs = llb_job_info(job_id, &njobs, flags);
    if (jobs == NULL) {
        if (errno != 0) {
            fprintf(stderr, "bjobs: %s\n", strerror(errno));
            return 1;
        }
        printf("No jobs found.\n");
        return 0;
    }

    struct col_widths w;
    compute_widths(jobs, njobs, &w);
    print_header(&w);

    for (int i = 0; i < njobs; i++)
        print_job(&jobs[i], &w);

    llb_free_job_info(jobs, njobs);
    return 0;
}
