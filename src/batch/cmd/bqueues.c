/* Copyright (C) LavaLite Contributors
 * GPL v2
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <getopt.h>
#include "llbatch.h"

static const char *
queue_status_str(int32_t status)
{
    if (status == 0)
        return "open";
    return "closed";
}

/* column widths: floor at header length */
struct col_widths {
    int name;
    int prio;
    int status;
    int max;
    int njobs;
    int pend;
    int run;
    int susp;
};

static int
imax(int a, int b)
{
    return a > b ? a : b;
}

static int
ndigits(int32_t n)
{
    if (n <= 0)
        return 1;
    int d = 0;
    while (n > 0) { d++; n /= 10; }
    return d;
}

static void
compute_widths(struct queue_info *q, int32_t n, struct col_widths *w)
{
    w->name   = strlen("QUEUE_NAME");
    w->prio   = strlen("PRIO");
    w->status = strlen("STATUS");
    w->max    = strlen("MAX");
    w->njobs  = strlen("NJOBS");
    w->pend   = strlen("PEND");
    w->run    = strlen("RUN");
    w->susp   = strlen("SUSP");

    for (int i = 0; i < n; i++) {
        int njobs = q[i].num_pend + q[i].num_run + q[i].num_susp;
        w->name   = imax(w->name,   strlen(q[i].name));
        w->prio   = imax(w->prio,   ndigits(q[i].priority));
        w->status = imax(w->status, strlen(queue_status_str(q[i].status)));
        w->max    = imax(w->max,    ndigits(q[i].max_jobs));
        w->njobs  = imax(w->njobs,  ndigits(njobs));
        w->pend   = imax(w->pend,   ndigits(q[i].num_pend));
        w->run    = imax(w->run,    ndigits(q[i].num_run));
        w->susp   = imax(w->susp,   ndigits(q[i].num_susp));
    }
}

static void usage(void)
{
    fprintf(stderr, "bqueues: --help display this help and exit\n"
            "--version output version information and exit\n");
}

static struct option longopts[] = {
    {"help", no_argument, NULL, 'h'},
    {"version", no_argument, NULL, 'V'},
    {NULL, 0, NULL, 0}
};

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

    int32_t n;
    struct queue_info *q;
    struct col_widths w;

    q = llb_queue_info(&n);
    if (!q) {
        fprintf(stderr, "bqueues: failed\n");
        return -1;
    }

    compute_widths(q, n, &w);

    printf("%-*s  %-*s  %-*s  %*s  %*s  %*s  %*s  %*s\n",
           w.name,  "QUEUE_NAME",
           w.prio,  "PRIO",
           w.status,"STATUS",
           w.max,   "MAX",
           w.njobs, "NJOBS",
           w.pend,  "PEND",
           w.run,   "RUN",
           w.susp,  "SUSP");

    for (int i = 0; i < n; i++) {
        printf("%-*s  %-*d  %-*s  %*d  %*d  %*d  %*d  %*d\n",
               w.name,  q[i].name,
               w.prio,  q[i].priority,
               w.status,queue_status_str(q[i].status),
               w.max,   q[i].max_jobs,
               w.njobs, q[i].num_jobs,
               w.pend,  q[i].num_pend,
               w.run,   q[i].num_run,
               w.susp,  q[i].num_susp);
    }

    llb_free_queue_info(q, n);
    return 0;
}
