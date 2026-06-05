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
#include "batch/lib/log.h"

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
    default:
        return "?";
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

static const char *event_type_str(int32_t type)
{
    switch (type) {
    case EVENT_JOB_NEW:
        return "Submitted";
    case EVENT_JOB_START:
        return "Dispatched";
    case EVENT_JOB_FORK:
        return "Forked";
    case EVENT_JOB_SIGNAL:
        return "Signal";
    case EVENT_JOB_FINISH:
        return "Finished";
    case EVENT_JOB_PEND_SUSP:
        return "Suspended (pend)";
    case EVENT_JOB_PEND_RESUME:
        return "Resumed (pend)";
    case EVENT_JOB_SUSP:
        return "Suspended";
    case EVENT_JOB_MOVE:
        return "Moved queue";
    case EVENT_JOB_PRIORITY:
        return "Priority";
    case EVENT_JOB_PEND:
        return "Pending";
    default:
        return "?";
    }
}

static const struct job_event *find_event(const struct job_hist_info *j,
                                          int32_t type)
{
    int32_t i;

    for (i = 0; i < j->num_events; i++) {
        if (j->events[i].type == type)
            return &j->events[i];
    }

    return NULL;
}

static void print_run_hosts(const char *s, int indent)
{
    char buf[LL_BUFSIZ_4K];
    char *tok;
    int col;

    snprintf(buf, sizeof(buf), "%s", s);
    col = indent;

    for (tok = strtok(buf, " "); tok != NULL; tok = strtok(NULL, " ")) {
        int len = (int)strlen(tok);
        if (col + 1 + len > 80 && col > indent) {
            printf("\n%*s", indent, "");
            col = indent;
        }
        printf(" %s", tok);
        col += 1 + len;
    }
    printf("\n");
}

/* -----------------------------------------------------------------------
 * compact history table
 * ----------------------------------------------------------------------- */

struct col_widths {
    int jobid;
    int user;
    int stat;
    int queue;
    int run_hosts;
    int name;
    int pri;
};

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

static int run_hosts_width(const char *s)
{
    char buf[LL_BUFSIZ_4K];
    int max = 0;

    if (s == NULL || s[0] == '\0')
        return 1;

    snprintf(buf, sizeof(buf), "%s", s);

    char *tok = strtok(buf, " ");
    while (tok != NULL) {
        int len = (int)strlen(tok);
        if (len > max)
            max = len;
        tok = strtok(NULL, " ");
    }
    return max;
}

static void compute_widths(struct job_hist_info *jobs, int n,
                           struct col_widths *w)
{
    int i;

    w->jobid     = (int)strlen("JOBID");
    w->user      = (int)strlen("USER");
    w->stat      = (int)strlen("STAT");
    w->queue     = (int)strlen("QUEUE");
    w->run_hosts = (int)strlen("RUN_HOSTS");
    w->name      = (int)strlen("JOB_NAME");
    w->pri       = (int)strlen("PRI");

    for (i = 0; i < n; i++) {
        const struct job_hist_info *j = &jobs[i];
        const struct job_event *start = find_event(j, EVENT_JOB_START);
        const char *rh = NULL;

        if (start != NULL)
            rh = start->run_hosts;

        w->jobid     = imax(w->jobid,     ndigits(j->job_id));
        w->user      = imax(w->user,      (int)strlen(str_or_dash(j->username)));
        w->stat      = imax(w->stat,      (int)strlen(job_state_str(j->state)));
        w->queue     = imax(w->queue,     (int)strlen(str_or_dash(j->queue)));
        w->run_hosts = imax(w->run_hosts, run_hosts_width(rh));
        w->name      = imax(w->name,      (int)strlen(str_or_dash(j->name)));
        w->pri = imax(w->pri, ndigits(j->priority));
    }
}

static void print_compact_header(const struct col_widths *w)
{
    printf("%-*s  %-*s  %-*s  %-*s  %-*s  %-*s  %-*s  %s\n",
           w->jobid,     "JOBID",
           w->user,      "USER",
           w->stat,      "STAT",
           w->queue,     "QUEUE",
           w->pri,       "PRI",
           w->run_hosts, "RUN_HOSTS",
           w->name,      "JOB_NAME",
           "SUBMIT_TIME");
}

static void print_job_compact(const struct job_hist_info *j,
                              const struct col_widths *w)
{
    const struct job_event *start = find_event(j, EVENT_JOB_START);
    char buf[LL_BUFSIZ_4K];
    char *tok;
    int first = 1;
    const char *rh = NULL;

    if (start != NULL && start->run_hosts != NULL &&
        start->run_hosts[0] != '\0')
        rh = start->run_hosts;

    if (rh == NULL) {
        printf("%-*ld  %-*s  %-*s  %-*s  %-*d  %-*s  %-*s  %s\n",
               w->jobid,     j->job_id,
               w->user,      str_or_dash(j->username),
               w->stat,      job_state_str(j->state),
               w->queue,     str_or_dash(j->queue),
               w->pri,       j->priority,
               w->run_hosts, "-",
               w->name,      str_or_dash(j->name),
               fmt_time(j->submit_time));
        return;
    }

    snprintf(buf, sizeof(buf), "%s", rh);
    tok = strtok(buf, " ");
    while (tok != NULL) {
        if (first) {
            printf("%-*ld  %-*s  %-*s  %-*s  %-*d  %-*s  %-*s  %s\n",
                   w->jobid,     j->job_id,
                   w->user,      str_or_dash(j->username),
                   w->stat,      job_state_str(j->state),
                   w->queue,     str_or_dash(j->queue),
                   w->pri,       j->priority,
                   w->run_hosts, tok,
                   w->name,      str_or_dash(j->name),
                   fmt_time(j->submit_time));
            first = 0;
        } else {
            printf("%-*s  %-*s  %-*s  %-*s  %-*s  %-*s\n",
                   w->jobid,     "",
                   w->user,      "",
                   w->stat,      "",
                   w->queue,     "",
                   w->pri,       "",
                   w->run_hosts, tok);
        }
        tok = strtok(NULL, " ");
    }
}

/* -----------------------------------------------------------------------
 * print_job_full: sidecar + event sequence
 * used by both bhist <job_id> and bhist -l
 * ----------------------------------------------------------------------- */

static void print_job_full(const struct job_hist_info *j)
{
    const struct job_event *start = find_event(j, EVENT_JOB_START);
    int i;

    printf("Job <%ld>  User <%s>  Queue <%s>  Status <%s>\n",
           j->job_id,
           str_or_dash(j->username),
           str_or_dash(j->queue),
           job_state_str(j->state));

    printf("  Submitted:    %s\n", fmt_time(j->submit_time));
    if (j->submit_host != NULL && j->submit_host[0] != '\0')
        printf("  Submit host:  %s\n", j->submit_host);
    if (j->name != NULL && j->name[0] != '\0')
        printf("  Name:         %s\n", j->name);
    if (j->project != NULL && j->project[0] != '\0')
        printf("  Project:      %s\n", j->project);
    if (j->comment != NULL && j->comment[0] != '\0')
        printf("  Comment:      %s\n", j->comment);

    printf("  CWD:          %s\n", str_or_dash(j->cwd));
    printf("  Command:      %s\n", str_or_dash(j->command));
    printf("  Priority:     %d\n", j->priority);

    if (j->depend_cond != NULL && j->depend_cond[0] != '\0')
        printf("  Depends:      %s\n", j->depend_cond);
    if (j->in_file != NULL && j->in_file[0] != '\0')
        printf("  stdin:        %s\n", j->in_file);
    if (j->out_file != NULL && j->out_file[0] != '\0')
        printf("  stdout:       %s\n", j->out_file);
    if (j->err_file != NULL && j->err_file[0] != '\0')
        printf("  stderr:       %s\n", j->err_file);

    printf("  Requested resources:  %d host(s)  %d cpu(s)/host"
           "  %d gpu(s)/host  %lu MB mem\n",
           j->num_hosts, j->num_cpus, j->num_gpus,
           (unsigned long)j->mem_mb);

    if (j->gpu_type != NULL && j->gpu_type[0] != '\0')
        printf("  GPU type:     %s\n", j->gpu_type);
    if (j->machines != NULL && j->machines[0] != '\0')
        printf("  Machines:     %s\n", j->machines);
    if (j->tokenpool != NULL && j->tokenpool[0] != '\0')
        printf("  Token pool:   %s\n", j->tokenpool);
    if (j->begin_time != 0)
        printf("  Begin:        %s\n", fmt_time(j->begin_time));
    if (j->term_time != 0)
        printf("  Terminate:    %s\n", fmt_time(j->term_time));

    printf("\n");

    for (i = 0; i < j->num_events; i++) {
        const struct job_event *e = &j->events[i];

        printf("  %s  %s", fmt_time(e->event_time), event_type_str(e->type));

        switch (e->type) {
        case EVENT_JOB_START:
            if (e->run_hosts != NULL && e->run_hosts[0] != 0) {
                printf(" to: ");
                print_run_hosts(e->run_hosts, 34);
            } else {
                printf("\n");
            }
            if (e->gpu_assigned != NULL && e->gpu_assigned[0] != 0)
                printf("  %*s  GPU devices:  %s\n", 14, "", e->gpu_assigned);
            continue;
        case EVENT_JOB_FORK:
            printf("  pid: %d", (int)e->pid);
            break;
        case EVENT_JOB_SIGNAL:
            printf("  signal: %d", e->signal);
            break;
        case EVENT_JOB_FINISH:
            printf("  state: %s exit_status: %d",
                   job_state_str(e->state), e->exit_status);
            break;
        case EVENT_JOB_MOVE:
            printf("  %s -> %s",
                   str_or_dash(e->from_queue), str_or_dash(e->to_queue));
            break;
        case EVENT_JOB_PRIORITY:
            printf("  %d -> %d", e->old_priority, e->new_priority);
            break;
        default:
            break;
        }

        printf("\n");
    }

    if (j->usage.cpu_time > 0 || j->usage.mem_mb > 0) {
        printf("\n");
        printf("  CPU time:     %.2f sec\n", j->usage.cpu_time);
        printf("  Max memory:   %lu MB\n",   (unsigned long)j->usage.mem_mb);
        printf("  Max swap:     %lu MB\n",   (unsigned long)j->usage.swap_mb);
    }

    if (start == NULL && (j->state == JOB_DONE || j->state == JOB_EXITED))
        printf("  Never dispatched.\n");
}

/* -----------------------------------------------------------------------
 * Main
 * ----------------------------------------------------------------------- */

static void usage(void)
{
    fprintf(stderr,
            "Usage: bhist [options] [job_id]\n"
            "\n"
            "Display job history. Without arguments, shows compact"
            " summary for the current user.\n"
            "\n"
            "Options:\n"
            "  -u, --user USER  Show jobs for USER\n"
            "  -l               Full detail for all jobs\n"
            "  -h, --help       Display this help and exit\n"
            "  -V, --version    Output version information and exit\n"
            "\n"
            "Arguments:\n"
            "  job_id           Full detail for a specific job\n");
}

static struct option longopts[] = {
    { "help",    no_argument,       NULL, 'h' },
    { "version", no_argument,       NULL, 'V' },
    { "user",    required_argument, NULL, 'u' },
    { NULL, 0, NULL, 0 }
};

int main(int argc, char **argv)
{
    int64_t       job_id  = 0;
    int32_t       uid     = -1;
    int32_t       njobs   = 0;
    int32_t       flags   = 0;
    int           full    = 0;
    int           cc;
    struct passwd *pw;

    /* -l is short-only: no long equivalent */
    while ((cc = getopt_long(argc, argv, "hVu:l", longopts, NULL)) != EOF) {
        switch (cc) {
        case 'V':
            printf("%s\n", LAVALITE_VERSION_STR);
            return 0;
        case 'h':
            usage();
            return 0;
        case 'u':
            pw = getpwnam(optarg);
            if (pw == NULL) {
                fprintf(stderr, "bhist: unknown user '%s'\n", optarg);
                return 1;
            }
            uid = pw->pw_uid;
            break;
        case 'l':
            full = 1;
            break;
        default:
            usage();
            return 1;
        }
    }

    if (optind < argc) {
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

    /* default: current user */
    if (job_id <= 0 && uid == -1) {
        pw = getpwuid(getuid());
        if (pw == NULL) {
            fprintf(stderr, "bhist: cannot determine current user\n");
            return 1;
        }
        uid = pw->pw_uid;
    }

    struct job_hist_info *jobs = llb_hist_info(job_id, uid, flags, &njobs);
    if (jobs == NULL) {
        if (errno != 0) {
            fprintf(stderr, "bhist: %s\n", strerror(errno));
            return 1;
        }
        printf("No historical jobs found.\n");
        return 0;
    }

    struct col_widths w;

    if (job_id <= 0 && !full) {
        compute_widths(jobs, njobs, &w);
        print_compact_header(&w);
    }

    for (int i = 0; i < njobs; i++) {
        if (job_id > 0 || full) {
            print_job_full(&jobs[i]);
            if (i + 1 < njobs)
                printf("\n");
        } else {
            print_job_compact(&jobs[i], &w);
        }
    }

    llb_free_hist_info(jobs, njobs);
    return 0;
}
