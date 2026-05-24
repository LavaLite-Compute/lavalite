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

static int exec_hosts_width(const char *s)
{
    char buf[4096];
    int max = 0;
    char *tok;

    if (s == NULL || s[0] == '\0')
        return 1;

    strncpy(buf, s, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = 0;

    tok = strtok(buf, " ");
    while (tok != NULL) {
        max = imax(max, (int) strlen(tok));
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

static void compute_widths(struct job_hist_info *jobs, int n,
                           struct col_widths *w)
{
    int i;
    struct job_hist_info *j;

    w->jobid = (int) strlen("JOBID");
    w->user = (int) strlen("USER");
    w->stat = (int) strlen("STAT");
    w->queue = (int) strlen("QUEUE");
    w->exec_hosts = (int) strlen("EXEC_HOSTS");
    w->name = (int) strlen("JOB_NAME");

    for (i = 0; i < n; i++) {
        j = &jobs[i];

        w->jobid = imax(w->jobid, ndigits(j->job_id));
        w->user = imax(w->user, (int) strlen(j->username));
        w->stat = imax(w->stat, (int) strlen(job_state_str(j->state)));
        w->queue = imax(w->queue, (int) strlen(j->queue));
        w->exec_hosts = imax(w->exec_hosts, exec_hosts_width(j->exec_hosts));
        w->name = imax(w->name, (int) strlen(j->name));
    }
}

static void print_header(const struct col_widths *w)
{
    printf("%-*s  %-*s  %-*s  %-*s  %-*s  %-*s  %-12s  %-12s\n", w->jobid,
           "JOBID", w->user, "USER", w->stat, "STAT", w->queue, "QUEUE",
           w->exec_hosts, "EXEC_HOSTS", w->name, "JOB_NAME", "SUBMIT_TIME",
           "END_TIME");
}

static void print_job_line(const struct job_hist_info *j,
                           const struct col_widths *w, const char *exec_host,
                           int first)
{
    if (first) {
        printf("%-*ld  %-*s  %-*s  %-*s  %-*s  %-*s  %-12s  %-12s\n", w->jobid,
               j->job_id, w->user, j->username, w->stat,
               job_state_str(j->state), w->queue, j->queue, w->exec_hosts,
               exec_host, w->name, j->name, fmt_time(j->submit_time),
               fmt_time(j->end_time));
        return;
    }

    printf("%-*s  %-*s  %-*s  %-*s  %-*s\n", w->jobid, "", w->user, "", w->stat,
           "", w->queue, "", w->exec_hosts, exec_host);
}

static void print_job(const struct job_hist_info *j, const struct col_widths *w)
{
    char buf[4096];
    char *tok;
    int first = 1;

    if (j->exec_hosts == NULL || j->exec_hosts[0] == '\0') {
        print_job_line(j, w, "-", 1);
        return;
    }

    strncpy(buf, j->exec_hosts, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = 0;

    tok = strtok(buf, " ");
    while (tok != NULL) {
        print_job_line(j, w, tok, first);
        first = 0;
        tok = strtok(NULL, " ");
    }
}
static void print_job_long(const struct job_hist_info *j)
{
    printf("\n");
    printf("  Job <%ld>, User <%s>, Queue <%s>, Status <%s>\n", j->job_id,
           j->username, j->queue, job_state_str(j->state));

    printf("  Times:\n");
    printf("    Submitted:  %s\n", fmt_time(j->submit_time));
    printf("    Dispatched: %s\n", fmt_time(j->dispatch_time));
    printf("    Forked:     %s\n", fmt_time(j->fork_time));
    printf("    Executed:   %s\n", fmt_time(j->execute_time));
    printf("    Ended:      %s\n", fmt_time(j->end_time));

    printf("  Requested resources:\n");
    printf("    Hosts:      %d\n", j->num_hosts);
    printf("    CPUs/host:  %d\n", j->num_cpus);
    printf("    GPUs/host:  %d\n", j->num_gpus);
    printf("    Memory:     %lu MB\n", j->usage.mem_mb);
    printf("    Storage:    %lu MB\n", j->storage_mb);
    if (j->end_time > 0)
        printf("    Wall time:  %ld seconds\n", j->end_time - j->dispatch_time);

    printf("  Usage:\n");
    printf("    CPU time:   %.2f sec\n", j->usage.cpu_time);
    printf("    Max memory: %lu MB\n", j->usage.mem_mb);
    printf("    Max swap:   %lu MB\n", j->usage.swap_mb);

    printf("  Execution:\n");
    printf("    Hosts:      %s\n",
           j->exec_hosts && j->exec_hosts[0] ? j->exec_hosts : "-");
    printf("    PID:        %d\n", (int) j->pid);
    printf("    CWD:        %s\n", j->cwd && j->cwd[0] ? j->cwd : "-");
    printf("    Command:    %s\n",
           j->command && j->command[0] ? j->command : "-");
    printf("    stdin:      %s\n",
           j->in_file && j->in_file[0] ? j->in_file : "-");
    printf("    stdout:     %s\n",
           j->out_file && j->out_file[0] ? j->out_file : "-");
    printf("    stderr:     %s\n",
           j->err_file && j->err_file[0] ? j->err_file : "-");
}

static void usage(void)
{
    fprintf(stderr, "Usage: bhist [options] [job_id]\n"
                    "\n"
                    "Display historical job information.\n"
                    "\n"
                    "Options:\n"
                    "  -u, --user USER   Show historical jobs for USER\n"
                    "  -h, --help        Display this help and exit\n"
                    "  -V, --version     Output version information and exit\n"
                    "  -l --long         Output long version\n"
                    "\n"
                    "Arguments:\n"
                    "  job_id            Show history for one job\n");
}

static struct option longopts[] = {{"help", no_argument, NULL, 'h'},
                                   {"version", no_argument, NULL, 'V'},
                                   {"user", required_argument, NULL, 'u'},
                                   {"long", no_argument, NULL, 'l'},
                                   {NULL, 0, NULL, 0}};

int main(int argc, char **argv)
{
    int64_t job_id = 0;
    const char *user = NULL;
    int32_t njobs = 0;
    struct job_hist_info *jobs;
    struct col_widths w;

    int cc;
    int long_format = 0;
    while ((cc = getopt_long(argc, argv, "hVu:l", longopts, NULL)) != EOF) {
        switch (cc) {
        case 'V':
            fprintf(stderr, "%s\n", LAVALITE_VERSION_STR);
            return 0;
        case 'h':
            usage();
            return 0;
        case 'u':
            user = optarg;
            break;
        case 'l':
            long_format = 1;
            break;
        default:
            usage();
            return 1;
        }
    }

    if (optind < argc) {
        char *end;

        if (user != NULL) {
            fprintf(stderr,
                    "bhist: job_id is mutually exclusive with --user\n");
            return 1;
        }

        job_id = strtoll(argv[optind], &end, 10);
        if (end == argv[optind] || *end != '\0' || job_id <= 0) {
            fprintf(stderr, "bhist: invalid job_id '%s'\n", argv[optind]);
            return 1;
        }

        optind++;
    }

    if (optind < argc) {
        usage();
        return 1;
    }

    if (job_id <= 0 && (user == NULL || user[0] == 0)) {
        usage();
        return 1;
    }

    jobs = llb_hist_info(job_id, user, &njobs);
    if (jobs == NULL) {
        if (errno != 0) {
            fprintf(stderr, "bhist: %s\n", strerror(errno));
            return 1;
        }
        printf("No historical jobs found.\n");
        return 0;
    }

    compute_widths(jobs, njobs, &w);
    print_header(&w);

    for (int i = 0; i < njobs; i++) {
        if (long_format) {
            print_job_long(&jobs[i]);
            if (long_format && i + 1 < njobs)
                printf("\n");
        } else {
            print_job(&jobs[i], &w);
        }
    }

    llb_free_hist_info(jobs, njobs);
    return 0;
}
