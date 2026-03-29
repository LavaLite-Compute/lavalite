/* Copyright (C) LavaLite Contributors
 * GPL v2
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "llbatch.h"

static const char *
host_status_str(int32_t status)
{
    switch (status) {
    case 0: return "ok";
    case 1: return "closed";
    case 2: return "unavail";
    default: return "unknown";
    }
}

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

struct col_widths {
    int name;
    int status;
    int max;
    int njobs;
    int run;
    int susp;
};

static void
compute_widths(struct host_info *h, int n, struct col_widths *w)
{
    w->name   = strlen("HOST_NAME");
    w->status = strlen("STATUS");
    w->max    = strlen("MAX");
    w->njobs  = strlen("NJOBS");
    w->run    = strlen("RUN");
    w->susp   = strlen("SUSP");

    for (int i = 0; i < n; i++) {
        w->name   = imax(w->name,   strlen(h[i].host));
        w->status = imax(w->status, strlen(host_status_str(h[i].status)));
        w->max    = imax(w->max,    ndigits(h[i].max_jobs));
        w->njobs  = imax(w->njobs,  ndigits(h[i].num_jobs));
        w->run    = imax(w->run,    ndigits(h[i].num_run));
        w->susp   = imax(w->susp,   ndigits(h[i].num_susp));
    }
}

int
main(void)
{
    int nhosts;
    struct host_info *hosts;
    struct col_widths w;

    hosts = llb_host_info(&nhosts);
    if (!hosts) {
        fprintf(stderr, "bhosts: failed\n");
        return -1;
    }

    compute_widths(hosts, nhosts, &w);

    printf("%-*s  %-*s  %*s  %*s  %*s  %*s\n",
           w.name,   "HOST_NAME",
           w.status, "STATUS",
           w.max,    "MAX",
           w.njobs,  "NJOBS",
           w.run,    "RUN",
           w.susp,   "SUSP");

    for (int i = 0; i < nhosts; i++) {
        printf("%-*s  %-*s  %*d  %*d  %*d  %*d\n",
               w.name,   hosts[i].host,
               w.status, host_status_str(hosts[i].status),
               w.max,    hosts[i].max_jobs,
               w.njobs,  hosts[i].num_jobs,
               w.run,    hosts[i].num_run,
               w.susp,   hosts[i].num_susp);
    }

    free(hosts);
    return 0;
}
