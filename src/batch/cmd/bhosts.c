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
    int cpu;
    int mem;
    int storage;
    int gpu;
    int njobs;
    int run;
    int susp;
};

#define FMT_BUF_LEN 16

static void fmt_mb(uint64_t mb, char *buf, size_t len)
{
    if (mb >= (uint64_t)1024 * 1024)
        snprintf(buf, len, "%luT", (unsigned long)(mb / (1024 * 1024)));
    else if (mb >= 1024)
        snprintf(buf, len, "%luG", (unsigned long)(mb / 1024));
    else
        snprintf(buf, len, "%luM", (unsigned long)mb);
}

static void
compute_widths(struct host_info *h, int n, struct col_widths *w)
{
    char tmp[FMT_BUF_LEN];

    w->name    = strlen("HOST_NAME");
    w->status  = strlen("STATUS");
    w->max     = strlen("MAX");
    w->cpu     = strlen("CPU");
    w->mem     = strlen("MEM");
    w->storage = strlen("STOR");
    w->gpu     = strlen("GPU");
    w->njobs   = strlen("NJOBS");
    w->run     = strlen("RUN");
    w->susp    = strlen("SUSP");

    for (int i = 0; i < n; i++) {
        fmt_mb(h[i].total_mem_mb, tmp, sizeof(tmp));
        w->mem     = imax(w->mem,     (int)strlen(tmp));
        fmt_mb(h[i].total_storage_mb, tmp, sizeof(tmp));
        w->storage = imax(w->storage, (int)strlen(tmp));
        w->name    = imax(w->name,    (int)strlen(h[i].name));
        w->status  = imax(w->status,  (int)strlen(host_status_str(h[i].status)));
        w->max     = imax(w->max,     ndigits(h[i].max_jobs));
        w->cpu     = imax(w->cpu,     ndigits(h[i].total_cpu));
        w->gpu     = imax(w->gpu,     ndigits(h[i].total_gpu));
        w->njobs   = imax(w->njobs,   ndigits(h[i].num_jobs));
        w->run     = imax(w->run,     ndigits(h[i].num_run));
        w->susp    = imax(w->susp,    ndigits(h[i].num_susp));
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

    printf("%-*s  %-*s  %*s  %*s  %*s  %*s  %*s  %*s  %*s  %*s\n",
           w.name,    "HOST_NAME",
           w.status,  "STATUS",
           w.max,     "MAX",
           w.cpu,     "CPU",
           w.mem,     "MEM",
           w.storage, "STOR",
           w.gpu,     "GPU",
           w.njobs,   "NJOBS",
           w.run,     "RUN",
           w.susp,    "SUSP");

    char mem_buf[FMT_BUF_LEN];
    char stor_buf[FMT_BUF_LEN];
    for (int i = 0; i < nhosts; i++) {
        fmt_mb(hosts[i].total_mem_mb,     mem_buf,  sizeof(mem_buf));
        fmt_mb(hosts[i].total_storage_mb, stor_buf, sizeof(stor_buf));
        printf("%-*s  %-*s  %*d  %*d  %*s  %*s  %*d  %*d  %*d  %*d\n",
               w.name,    hosts[i].name,
               w.status,  host_status_str(hosts[i].status),
               w.max,     hosts[i].max_jobs,
               w.cpu,     hosts[i].total_cpu,
               w.mem,     mem_buf,
               w.storage, stor_buf,
               w.gpu,     hosts[i].total_gpu,
               w.njobs,   hosts[i].num_jobs,
               w.run,     hosts[i].num_run,
               w.susp,    hosts[i].num_susp);
    }

    free(hosts);
    return 0;
}
