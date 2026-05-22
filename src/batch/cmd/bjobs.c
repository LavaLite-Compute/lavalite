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

static const char *uid_to_name(uid_t uid)
{
    struct passwd *pw;

    pw = getpwuid(uid);
    if (pw == NULL)
        return "unknown";
    return pw->pw_name;
}

static const char *job_state_str(int32_t state)
{
    switch (state) {
    case JOB_PENDING:
        return "PEND";
    case JOB_HELD:
        return "HELD";
    case JOB_RUNNING:
        return "RUN";
    case JOB_SUSPENDED:
        return "SUSP";
    case JOB_EXITED:
        return "EXIT";
    case JOB_DONE:
        return "DONE";
    case JOB_ORPHAN:
        return "ORPHAN";
    case JOB_UNKNOWN:
        return "UNKNOWN";
    default:
        return "BADSTATE";
    }
}

static const char *fmt_time(time_t t)
{
    static char buf[32];

    if (t == 0)
        return "-";
    strftime(buf, sizeof(buf), "%b %d %H:%M", localtime(&t));
    return buf;
}

static int imax(int a, int b)
{
    if (a > b)
        return a;
    return b;
}

static int ndigits(int64_t n)
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

/*
 * Walk exec_hosts string "hostA*2 hostB*2" and return the width
 * of the longest token.
 */
static int exec_hosts_width(const char *s)
{
    char buf[4096];
    int max = 0;

    if (s == NULL || s[0] == '\0')
        return 1; /* "-" */

    strncpy(buf, s, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = 0;

    char *tok = strtok(buf, " ");
    while (tok != NULL) {
        int len = (int)strlen(tok);
        if (len > max)
            max = len;
        tok = strtok(NULL, " ");
    }
    return max;
}

struct col_widths {
    int jobid;
    int user;
    int stat;
    int queue;
    int exec_hosts;
    int name;
};

static void compute_widths(struct job_info *jobs, int n, struct col_widths *w)
{
    int i;
    struct job_info *j;

    w->jobid      = (int)strlen("JOBID");
    w->user       = (int)strlen("USER");
    w->stat       = (int)strlen("STAT");
    w->queue      = (int)strlen("QUEUE");
    w->exec_hosts = (int)strlen("EXEC_HOSTS");
    w->name       = (int)strlen("JOB_NAME");

    for (i = 0; i < n; i++) {
        j = &jobs[i];

        w->jobid      = imax(w->jobid,      ndigits(j->job_id));
        w->user       = imax(w->user,       (int)strlen(uid_to_name(j->uid)));
        w->stat       = imax(w->stat,       (int)strlen(job_state_str(j->state)));
        w->queue      = imax(w->queue,      (int)strlen(j->queue));
        w->name       = imax(w->name,       (int)strlen(j->name));
        w->exec_hosts = imax(w->exec_hosts, exec_hosts_width(j->exec_hosts));
    }
}

static void print_header(const struct col_widths *w)
{
    printf("%-*s  %-*s  %-*s  %-*s  %-*s  %-*s  %s\n",
           w->jobid,      "JOBID",
           w->user,       "USER",
           w->stat,       "STAT",
           w->queue,      "QUEUE",
           w->exec_hosts, "EXEC_HOSTS",
           w->name,       "JOB_NAME",
           "SUBMIT_TIME");
}

static void print_pend_reason(const struct job_info *j,
                              const struct col_widths *w)
{
    (void)w;

    if (j->state != JOB_PENDING)
        return;
    printf("  PEND: %s\n", pend_reason_msg[j->pend_reason]);
}

static void print_job(const struct job_info *j,
                      const struct col_widths *w,
                      int show_reason)
{
    char buf[4096];
    int first = 1;

    if (j->exec_hosts == NULL || j->exec_hosts[0] == '\0') {
        printf("%-*ld  %-*s  %-*s  %-*s  %-*s  %-*s  %s\n",
               w->jobid,      j->job_id,
               w->user,       uid_to_name(j->uid),
               w->stat,       job_state_str(j->state),
               w->queue,      j->queue,
               w->exec_hosts, "-",
               w->name,       j->name,
               fmt_time(j->submit_time));
        if (show_reason)
            print_pend_reason(j, w);
        return;
    }

    strncpy(buf, j->exec_hosts, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = 0;

    char *tok = strtok(buf, " ");
    while (tok != NULL) {
        if (first) {
            printf("%-*ld  %-*s  %-*s  %-*s  %-*s  %-*s  %s\n",
                   w->jobid,      j->job_id,
                   w->user,       uid_to_name(j->uid),
                   w->stat,       job_state_str(j->state),
                   w->queue,      j->queue,
                   w->exec_hosts, tok,
                   w->name,       j->name,
                   fmt_time(j->submit_time));
            first = 0;
        } else {
            printf("%-*s  %-*s  %-*s  %-*s  %-*s\n",
                   w->jobid,      "",
                   w->user,       "",
                   w->stat,       "",
                   w->queue,      "",
                   w->exec_hosts, tok);
        }
        tok = strtok(NULL, " ");
    }
    if (show_reason)
        print_pend_reason(j, w);
}

static void usage(void)
{
    fprintf(stderr,
        "Usage: bjobs [options] [job_id]\n"
        "\n"
        "Display jobs. Without options, shows active jobs for the"
        " current user.\n"
        "\n"
        "Options:\n"
        "  --all          Show active jobs for all users (admin only)\n"
        "  --pend         Show pending jobs with pending reason\n"
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

int main(int argc, char **argv)
{
    int flags = 0;
    int show_reason = 0;
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
            flags |= LLB_JOB_PEND | LLB_JOB_RUN | LLB_JOB_SUSP
                | LLB_JOB_DONE | LLB_JOB_HELD;
            break;
        case 'p':
            flags |= LLB_JOB_PEND;
            show_reason = 1;
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
        print_job(&jobs[i], &w, show_reason);

    llb_free_job_info(jobs, njobs);
    return 0;
}
