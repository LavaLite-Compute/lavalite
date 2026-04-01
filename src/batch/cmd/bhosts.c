/* Copyright (C) LavaLite Contributors
 * GPL v2
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include "llbatch.h"

static const char *host_status_str(int32_t status)
{
    const char *state;
    static char buf[32];

    switch (status & ~HOST_CLOSED) {
    case HOST_OK:
        state = "ok";
        break;
    case HOST_UNAVAIL:
        state = "unavail";
        break;
    default:
        state = "unknown"; break;
    }
    if (status & HOST_CLOSED)
        snprintf(buf, sizeof(buf), "%s|closed", state);
    else
        snprintf(buf, sizeof(buf), "%s", state);
    return buf;
}

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
        w->name   = imax(w->name,   strlen(h[i].name));
        w->status = imax(w->status, strlen(host_status_str(h[i].status)));
        w->max    = imax(w->max,    ndigits(h[i].max_jobs));
        w->njobs  = imax(w->njobs,  ndigits(h[i].num_jobs));
        w->run    = imax(w->run,    ndigits(h[i].num_run));
        w->susp   = imax(w->susp,   ndigits(h[i].num_susp));
    }
}

static void usage(void)
{
    fprintf(stderr, "bhosts: --help display this help and exit\n"
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

    int nhosts;
    struct host_info *hosts = llb_host_info(&nhosts);
    if (!hosts) {
        fprintf(stderr, "bhosts: failed\n");
        return -1;
    }

    struct col_widths w;
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
               w.name,   hosts[i].name,
               w.status, host_status_str(hosts[i].status),
               w.max,    hosts[i].max_jobs,
               w.njobs,  hosts[i].num_jobs,
               w.run,    hosts[i].num_run,
               w.susp,   hosts[i].num_susp);
    }

    free(hosts);
    return 0;
}
