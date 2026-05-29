/* Copyright (C) LavaLite Contributors
 * GPL v2
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <getopt.h>
#include "llbatch.h"

static const char *queue_status_str(int32_t status)
{
    if (status == 0)
        return "open";
    return "closed";
}

struct col_widths {
    int name;
    int prio;
    int status;
    int max;
    int njobs;
    int pend;
    int held;
    int run;
    int susp;
    int used_cpus;
    int used_hosts;
};

static int imax(int a, int b)
{
    return a > b ? a : b;
}

static int ndigits(int32_t n)
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

static void compute_widths(struct queue_info *q, int32_t n,
                           struct col_widths *w)
{
    w->name = strlen("QUEUE_NAME");
    w->prio = strlen("PRIO");
    w->status = strlen("STATUS");
    w->max = strlen("MAX");
    w->njobs = strlen("NJOBS");
    w->pend = strlen("PEND");
    w->held = strlen("HELD");
    w->run = strlen("RUN");
    w->susp = strlen("SUSP");
    w->used_cpus = strlen("USED_CPUS");
    w->used_hosts = strlen("USED_HOSTS");

    for (int i = 0; i < n; i++) {
        int njobs =
            q[i].num_pend + q[i].num_held + q[i].num_run + q[i].num_susp;
        w->name = imax(w->name, strlen(q[i].name));
        w->prio = imax(w->prio, ndigits(q[i].priority));
        w->status = imax(w->status, strlen(queue_status_str(q[i].status)));
        w->max = imax(w->max, ndigits(q[i].max_jobs));
        w->njobs = imax(w->njobs, ndigits(njobs));
        w->pend = imax(w->pend, ndigits(q[i].num_pend));
        w->held = imax(w->held, ndigits(q[i].num_held));
        w->run = imax(w->run, ndigits(q[i].num_run));
        w->susp = imax(w->susp, ndigits(q[i].num_susp));
        w->used_cpus = imax(w->used_cpus, ndigits(q[i].num_cpus_used));
        w->used_hosts = imax(w->used_hosts, ndigits(q[i].num_hosts_used));
    }
}

static void print_wrapped(const char *label, char **items, int n,
                          const char *all_str)
{
    int col = printf("  %-12s", label);

    if (n == 0) {
        printf("%s\n", all_str);
        return;
    }
    for (int i = 0; i < n; i++) {
        int w = (int) strlen(items[i]) + 1;
        if (i > 0 && col + w > 79) {
            printf("\n  %-12s", "");
            col = 14;
        }
        col += printf(" %s", items[i]);
    }
    printf("\n");
}

static void print_queue_long(const struct queue_info *q)
{
    printf("QUEUE: %s\n", q->name);
    if (q->description[0] != '\0')
        printf("  Description: %s\n", q->description);
    printf("  Priority:    %d    Status: %s    Max jobs: ",
           q->priority, queue_status_str(q->status));
    if (q->max_jobs == 0)
        printf("unlimited\n");
    else
        printf("%d\n", q->max_jobs);
    print_wrapped("Users:", q->users, q->num_users, "all");
    print_wrapped("Hosts:", q->hosts, q->num_hosts, "all");
    printf("  Jobs:        run=%-4d pend=%-4d held=%-4d susp=%d\n",
           q->num_run, q->num_pend, q->num_held, q->num_susp);
    printf("  Resources:   cpus_used=%-4d hosts_used=%d\n",
           q->num_cpus_used, q->num_hosts_used);
}

static void print_queues_long(const struct queue_info *q, int n)
{
    for (int i = 0; i < n; i++) {
        if (i > 0)
            printf("\n");
        print_queue_long(&q[i]);
    }
}

static void usage(void)
{
    fprintf(stderr, "bqueues: --help display this help and exit\n"
                    "  -l, --long     display detailed queue information\n"
                    "  -c, --close    QUEUE close a queue\n"
                    "  -o, --open     QUEUE open a queue\n"
                    "  --version      output version information and exit\n");
}

static struct option longopts[] = {{"help", no_argument, NULL, 'h'},
                                   {"version", no_argument, NULL, 'V'},
                                   {"close", required_argument, NULL, 'c'},
                                   {"open", required_argument, NULL, 'o'},
                                   {"long", no_argument, NULL, 'l'},
                                   {NULL, 0, NULL, 0}};

int main(int argc, char **argv)
{
    const char *close_queue = NULL;
    const char *open_queue = NULL;

    int cc;
    int long_fmt = 0;
    while ((cc = getopt_long(argc, argv, "hVl", longopts, NULL)) != EOF) {
        switch (cc) {
        case 'c':
            close_queue = optarg;
            break;
        case 'o':
            open_queue = optarg;
            break;
        case 'V':
            fprintf(stderr, "%s\n", LAVALITE_VERSION_STR);
            return 0;
        case 'l':
            long_fmt = 1;
            break;
        case 'h':
        default:
            usage();
            return 0;
        }
    }

    if (close_queue) {
        int rc = llb_queue_admin(close_queue, QUEUE_CLOSED);
        if (rc != 0)
            fprintf(stderr, "bqueues: %s: %m\n", close_queue);
        else
            printf("queue %s closed\n", close_queue);
        return rc;
    }

    if (open_queue) {
        int rc = llb_queue_admin(open_queue, QUEUE_OPEN);
        if (rc != 0)
            fprintf(stderr, "bqueues: %s: %m\n", open_queue);
        else
            printf("queue %s opened\n", open_queue);
        return rc;
    }

    int32_t n;
    struct queue_info *q;
    struct col_widths w;

    q = llb_queue_info(&n);
    if (!q) {
        fprintf(stderr, "bqueues: failed\n");
        return -1;
    }

    if (long_fmt) {
        print_queues_long(q, n);
        llb_free_queue_info(q, n);
        return 0;
    }
    compute_widths(q, n, &w);

    printf("%-*s  %-*s  %-*s  %*s  %*s  %*s  %*s  %*s  %*s  %*s  %*s\n", w.name,
           "QUEUE_NAME", w.prio, "PRIO", w.status, "STATUS", w.max, "MAX",
           w.njobs, "NJOBS", w.pend, "PEND", w.held, "HELD", w.run, "RUN",
           w.susp, "SUSP", w.used_cpus, "USED_CPUS", w.used_hosts,
           "USED_HOSTS");

    for (int i = 0; i < n; i++) {
        printf("%-*s  %-*d  %-*s  %*d  %*d  %*d  %*d  %*d  %*d  %*d  %*d\n",
               w.name, q[i].name, w.prio, q[i].priority, w.status,
               queue_status_str(q[i].status), w.max, q[i].max_jobs, w.njobs,
               q[i].num_jobs, w.pend, q[i].num_pend, w.held, q[i].num_held,
               w.run, q[i].num_run, w.susp, q[i].num_susp, w.used_cpus,
               q[i].num_cpus_used, w.used_hosts, q[i].num_hosts_used);
    }

    llb_free_queue_info(q, n);
    return 0;
}
