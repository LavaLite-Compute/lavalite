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

/* -----------------------------------------------------------------------
 * print_job_compact: 5-liner overview
 * ----------------------------------------------------------------------- */

static void print_job_compact(const struct job_hist_info *j)
{
    const struct job_event *start  = find_event(j, EVENT_JOB_START);
    const struct job_event *fork   = find_event(j, EVENT_JOB_FORK);
    const struct job_event *finish = find_event(j, EVENT_JOB_FINISH);

    printf("Job <%ld>  User <%s>  Queue <%s>  Status <%s>\n",
           j->job_id,
           str_or_dash(j->username),
           str_or_dash(j->queue),
           job_state_str(j->state));

    printf("  Submitted: %s  CWD: %s\n",
           fmt_time(j->submit_time),
           str_or_dash(j->cwd));

    printf("  Resources: %d host(s)  %d cpu(s)/host  %d gpu(s)/host"
           "  %lu MB mem\n",
           j->num_hosts, j->num_cpus, j->num_gpus,
           (unsigned long)j->mem_mb);

    printf("  Command:   %s\n", str_or_dash(j->command));

    if (start != NULL) {
        printf("  Dispatched: %s  Host: %s  PID: %d",
               fmt_time(start->event_time),
               str_or_dash(start->from_host),
               fork ? (int)fork->pid : 0);

        if (finish != NULL)
            printf("  Ended: %s  Exit: %d",
                   fmt_time(finish->event_time),
                   finish->exit_status);

        printf("\n");
    } else {
        printf("  Never dispatched.\n");
    }
}

/* -----------------------------------------------------------------------
 * print_job_full: sidecar + event sequence
 * used by both bhist <job_id> and bhist -l
 * ----------------------------------------------------------------------- */

static void print_job_full(const struct job_hist_info *j)
{
    const struct job_event *start  = find_event(j, EVENT_JOB_START);

    printf("Job <%ld>  User <%s>  Queue <%s>  Status <%s>\n",
           j->job_id,
           str_or_dash(j->username),
           str_or_dash(j->queue),
           job_state_str(j->state));

    printf("  Submitted:  %s\n", fmt_time(j->submit_time));
    printf("  CWD:        %s\n", str_or_dash(j->cwd));
    printf("  Command:    %s\n", str_or_dash(j->command));

    if (j->in_file  != NULL && j->in_file[0]  != '\0')
        printf("  stdin:      %s\n", j->in_file);
    if (j->out_file != NULL && j->out_file[0] != '\0')
        printf("  stdout:     %s\n", j->out_file);
    if (j->err_file != NULL && j->err_file[0] != '\0')
        printf("  stderr:     %s\n", j->err_file);

    printf("  Requested resources:  %d host(s)  %d cpu(s)/host  %d gpu(s)/host"
           "  %lu MB mem\n",
           j->num_hosts, j->num_cpus, j->num_gpus,
           (unsigned long)j->mem_mb);

    if (start != NULL && start->exec_hosts != NULL && start->exec_hosts[0] != '\0')
        printf("  Hosts: %s", start->exec_hosts);
    printf("\n");

    for (int i = 0; i < j->num_events; i++) {
        const struct job_event *e = &j->events[i];

        printf("  %s  %s", fmt_time(e->event_time), event_type_str(e->type));

        switch (e->type) {
        case EVENT_JOB_START:
            printf("  host: %s", str_or_dash(e->from_host));
            break;
        case EVENT_JOB_FORK:
            printf("  pid: %d", (int)e->pid);
            break;
        case EVENT_JOB_SIGNAL:
            printf("  signal: %d", e->signal);
            break;
        case EVENT_JOB_FINISH:
            printf("  exit: %d  state: %s",
                   e->exit_status, job_state_str(e->state));
            break;
        case EVENT_JOB_MOVE:
            printf("  %s -> %s", str_or_dash(e->from_queue), str_or_dash(e->to_queue));
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
        printf("  CPU time:   %.2f sec\n", j->usage.cpu_time);
        printf("  Max memory: %lu MB\n",  (unsigned long)j->usage.mem_mb);
        printf("  Max swap:   %lu MB\n",  (unsigned long)j->usage.swap_mb);
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
    uid_t         uid     = (uid_t)-1;
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
    if (job_id <= 0 && uid == (uid_t)-1) {
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

    for (int i = 0; i < njobs; i++) {
        if (job_id > 0 || full)
            print_job_full(&jobs[i]);
        else
            print_job_compact(&jobs[i]);

        if (i + 1 < njobs)
            printf("\n");
    }

    llb_free_hist_info(jobs, njobs);
    return 0;
}
