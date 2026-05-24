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
    case JOB_PENDING:   return "PEND";
    case JOB_HELD:      return "HELD";
    case JOB_RUNNING:   return "RUN";
    case JOB_SUSPENDED: return "SUSP";
    case JOB_EXITED:    return "EXIT";
    case JOB_DONE:      return "DONE";
    case JOB_ORPHAN:    return "ORPHAN";
    case JOB_UNKNOWN:   return "UNKNOWN";
    default:            return "?";
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

static const char *str_or_dash(const char *s)
{
    if (s == NULL || s[0] == '\0')
        return "-";
    return s;
}

static int imax(int a, int b)
{
    return a > b ? a : b;
}

static int ndigits(int64_t n)
{
    if (n <= 0)
        return 1;
    int d = 0;
    while (n > 0) {
        d++;
        n /= 10;
    }
    return d;
}

/* -----------------------------------------------------------------------
 * Short format (default) -- one line per job, like bjobs.
 * Columns: JOBID USER STAT QUEUE NAME PROJECT SUBMIT_TIME END_TIME
 * ----------------------------------------------------------------------- */

struct col_widths {
    int jobid;
    int user;
    int stat;
    int queue;
    int name;
    int project;
};

static void compute_widths(struct job_hist_info *jobs, int32_t n,
                            struct col_widths *w)
{
    w->jobid   = (int)strlen("JOBID");
    w->user    = (int)strlen("USER");
    w->stat    = (int)strlen("STAT");
    w->queue   = (int)strlen("QUEUE");
    w->name    = (int)strlen("NAME");
    w->project = (int)strlen("PROJECT");

    for (int32_t i = 0; i < n; i++) {
        struct job_hist_info *j = &jobs[i];

        w->jobid   = imax(w->jobid,   ndigits(j->job_id));
        w->user    = imax(w->user,    (int)strlen(str_or_dash(j->username)));
        w->stat    = imax(w->stat,    (int)strlen(job_state_str(j->state)));
        w->queue   = imax(w->queue,   (int)strlen(str_or_dash(j->queue)));
        w->name    = imax(w->name,    (int)strlen(str_or_dash(j->name)));
        w->project = imax(w->project, (int)strlen(str_or_dash(j->project)));
    }
}

static void print_header(const struct col_widths *w)
{
    printf("%-*s  %-*s  %-*s  %-*s  %-*s  %-*s  %-14s  %-14s\n",
           w->jobid,   "JOBID",
           w->user,    "USER",
           w->stat,    "STAT",
           w->queue,   "QUEUE",
           w->name,    "NAME",
           w->project, "PROJECT",
           "SUBMIT_TIME",
           "END_TIME");
}

static void print_job(const struct job_hist_info *j, const struct col_widths *w)
{
    const char *end_time_str = "-";

    if (j->num_runs > 0) {
        struct job_run *last = &j->runs[j->num_runs - 1];
        if (last->end_time > 0)
            end_time_str = fmt_time(last->end_time);
    }

    printf("%-*ld  %-*s  %-*s  %-*s  %-*s  %-*s  %-14s  %-14s\n",
           w->jobid,   j->job_id,
           w->user,    str_or_dash(j->username),
           w->stat,    job_state_str(j->state),
           w->queue,   str_or_dash(j->queue),
           w->name,    str_or_dash(j->name),
           w->project, str_or_dash(j->project),
           fmt_time(j->submit_time),
           end_time_str);
}

/* -----------------------------------------------------------------------
 * Long format (-l) -- dense block, ~5-6 lines per job.
 * stdio redirects shown only when non-default.
 * Multiple dispatch blocks for requeued jobs, no "Run N:" label.
 * ----------------------------------------------------------------------- */

static void print_run_long(const struct job_run *r)
{
    printf("  Dispatched: %s  Host: %s  PID: %d",
           fmt_time(r->dispatch_time),
           str_or_dash(r->from_host),
           (int)r->pid);

    if (r->end_time > 0)
        printf("  Ended: %s  Exit: %d", fmt_time(r->end_time), r->exit_status);

    printf("\n");
}

static void print_job_long(const struct job_hist_info *j)
{
    printf("Job <%ld>  User <%s>  Queue <%s>  Status <%s>  Name <%s>  Project <%s>\n",
           j->job_id,
           str_or_dash(j->username),
           str_or_dash(j->queue),
           job_state_str(j->state),
           str_or_dash(j->name),
           str_or_dash(j->project));

    printf("  Submitted: %s  CWD: %s\n",
           fmt_time(j->submit_time),
           str_or_dash(j->cwd));

    printf("  Resources: %d host(s)  %d cpu(s)/host  %d gpu(s)/host  %lu MB mem\n",
           j->num_hosts, j->num_cpus, j->num_gpus,
           (unsigned long)j->mem_mb);

    printf("  Command:   %s\n", str_or_dash(j->command));

    /* stdio only when non-default */
    int has_io = (j->in_file  != NULL && j->in_file[0]  != '\0') ||
                 (j->out_file != NULL && j->out_file[0] != '\0') ||
                 (j->err_file != NULL && j->err_file[0] != '\0');
    if (has_io) {
        if (j->in_file  != NULL && j->in_file[0]  != '\0')
            printf("  stdin:     %s\n", j->in_file);
        if (j->out_file != NULL && j->out_file[0] != '\0')
            printf("  stdout:    %s\n", j->out_file);
        if (j->err_file != NULL && j->err_file[0] != '\0')
            printf("  stderr:    %s\n", j->err_file);
    }

    for (int32_t r = 0; r < j->num_runs; r++)
        print_run_long(&j->runs[r]);
}

/* -----------------------------------------------------------------------
 * Very long format (-ll) -- full detail, all fields always.
 * ----------------------------------------------------------------------- */

static void print_run_vlong(const struct job_run *r, int show_seq)
{
    if (show_seq)
        printf("  --- run %d ---\n", r->run_seq);

    printf("  Dispatched:  %s\n", fmt_time(r->dispatch_time));
    printf("  Forked:      %s\n", fmt_time(r->fork_time));
    printf("  Ended:       %s\n", fmt_time(r->end_time));

    if (r->end_time > 0 && r->dispatch_time > 0)
        printf("  Wall time:   %ld sec\n", (long)(r->end_time - r->dispatch_time));

    printf("  State:       %s\n", job_state_str(r->state));
    printf("  Exit status: %d\n", r->exit_status);
    printf("  PID:         %d\n", (int)r->pid);
    printf("  From host:   %s\n", str_or_dash(r->from_host));
    printf("  Exec hosts:  %s\n", str_or_dash(r->exec_hosts));

    if (r->usage.cpu_time > 0 || r->usage.mem_mb > 0) {
        printf("  CPU time:    %.2f sec\n", r->usage.cpu_time);
        printf("  Max memory:  %lu MB\n",   (unsigned long)r->usage.mem_mb);
        printf("  Max swap:    %lu MB\n",   (unsigned long)r->usage.swap_mb);
    }
}

static void print_job_vlong(const struct job_hist_info *j)
{
    printf("Job <%ld>  User <%s>  Queue <%s>  Status <%s>  Name <%s>  Project <%s>\n",
           j->job_id,
           str_or_dash(j->username),
           str_or_dash(j->queue),
           job_state_str(j->state),
           str_or_dash(j->name),
           str_or_dash(j->project));

    printf("  Submitted:  %s\n", fmt_time(j->submit_time));
    printf("  CWD:        %s\n", str_or_dash(j->cwd));
    printf("  Command:    %s\n", str_or_dash(j->command));
    printf("  stdin:      %s\n", str_or_dash(j->in_file));
    printf("  stdout:     %s\n", str_or_dash(j->out_file));
    printf("  stderr:     %s\n", str_or_dash(j->err_file));
    printf("  Resources:  %d host(s)  %d cpu(s)/host  %d gpu(s)/host\n",
           j->num_hosts, j->num_cpus, j->num_gpus);
    printf("  Memory:     %lu MB  Storage: %lu MB\n",
           (unsigned long)j->mem_mb, (unsigned long)j->storage_mb);

    if (j->num_runs == 0) {
        printf("  Never dispatched.\n");
        return;
    }

    for (int32_t r = 0; r < j->num_runs; r++)
        print_run_vlong(&j->runs[r], j->num_runs > 1);
}

/* -----------------------------------------------------------------------
 * Main
 * ----------------------------------------------------------------------- */

static void usage(void)
{
    fprintf(stderr,
            "Usage: bhist [options] [job_id]\n"
            "\n"
            "Options:\n"
            "  -u, --user USER   Show jobs for USER (default: current user)\n"
            "  -l                Long format\n"
            "  -ll               Very long format (full detail)\n"
            "  -h, --help        Show this help\n"
            "  -V, --version     Show version\n"
            "\n"
            "Arguments:\n"
            "  job_id            Show history for a single job\n");
}

static struct option longopts[] = {
    { "help",    no_argument,       NULL, 'h' },
    { "version", no_argument,       NULL, 'V' },
    { "user",    required_argument, NULL, 'u' },
    { NULL, 0, NULL, 0 }
};

int main(int argc, char **argv)
{
    int64_t job_id = 0;
    const char *user = NULL;
    int32_t njobs = 0;
    int long_fmt = 0;
    int cc;

    while ((cc = getopt_long(argc, argv, "hVu:l", longopts, NULL)) != EOF) {
        switch (cc) {
        case 'V':
            printf("%s\n", LAVALITE_VERSION_STR);
            return 0;
        case 'h':
            usage();
            return 0;
        case 'u':
            user = optarg;
            break;
        case 'l':
            long_fmt++;
            break;
        default:
            usage();
            return 1;
        }
    }

    if (optind < argc) {
        if (user != NULL) {
            fprintf(stderr, "bhist: job_id and -u are mutually exclusive\n");
            return 1;
        }
        char *end;
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

    if (job_id <= 0 && user == NULL) {
        struct passwd *pw = getpwuid(getuid());
        if (pw == NULL) {
            fprintf(stderr, "bhist: cannot determine current user\n");
            return 1;
        }
        user = pw->pw_name;
    }

    struct job_hist_info *jobs = llb_hist_info(job_id, user, &njobs);
    if (jobs == NULL) {
        if (errno != 0) {
            fprintf(stderr, "bhist: %s\n", strerror(errno));
            return 1;
        }
        printf("No historical jobs found.\n");
        return 0;
    }

    if (long_fmt >= 2) {
        for (int i = 0; i < njobs; i++) {
            print_job_vlong(&jobs[i]);
            if (i + 1 < njobs)
                printf("\n");
        }
    } else if (long_fmt == 1) {
        for (int i = 0; i < njobs; i++) {
            print_job_long(&jobs[i]);
            if (i + 1 < njobs)
                printf("\n");
        }
    } else {
        struct col_widths w;
        compute_widths(jobs, njobs, &w);
        print_header(&w);
        for (int i = 0; i < njobs; i++)
            print_job(&jobs[i], &w);
    }

    llb_free_hist_info(jobs, njobs);
    return 0;
}
