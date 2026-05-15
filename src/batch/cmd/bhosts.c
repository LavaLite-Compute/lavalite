/* Copyright (C) LavaLite Contributors
 * GPL v2
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include "llbatch.h"

static const char *host_state_str(int32_t status)
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
        state = "unknown";
        break;
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
    int state;
    int max;
    int total_cpu;
    int used_cpu;
    int mem;
    int used_mem;
    int storage;
    int used_storage;
    int total_gpu;
    int used_gpu;
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

    w->name        = strlen("HOST_NAME");
    w->state       = strlen("STATE");
    w->max         = strlen("MAX");
    w->total_cpu   = strlen("NCPU");
    w->used_cpu    = strlen("USED_CPU");
    w->mem         = strlen("MEM");
    w->used_mem    = strlen("USED_MEM");
    w->storage     = strlen("STOR");
    w->used_storage = strlen("USED_STOR");
    w->total_gpu   = strlen("NGPU");
    w->used_gpu    = strlen("USED_GPU");
    w->njobs       = strlen("NJOBS");
    w->run         = strlen("RUN");
    w->susp        = strlen("SUSP");

    for (int i = 0; i < n; i++) {
        uint64_t used_mem  = h[i].total_mem_mb - h[i].free_mem_mb;
        uint64_t used_stor = h[i].total_storage_mb - h[i].free_storage_mb;
        int32_t  used_cpu  = h[i].total_cpu - h[i].free_cpu;
        int32_t  used_gpu  = h[i].total_gpu - h[i].free_gpu;

        fmt_mb(h[i].total_mem_mb, tmp, sizeof(tmp));
        w->mem          = imax(w->mem,          (int)strlen(tmp));
        fmt_mb(used_mem, tmp, sizeof(tmp));
        w->used_mem     = imax(w->used_mem,     (int)strlen(tmp));
        fmt_mb(h[i].total_storage_mb, tmp, sizeof(tmp));
        w->storage      = imax(w->storage,      (int)strlen(tmp));
        fmt_mb(used_stor, tmp, sizeof(tmp));
        w->used_storage = imax(w->used_storage, (int)strlen(tmp));
        w->name         = imax(w->name,         (int)strlen(h[i].name));
        w->state        = imax(w->state,        (int)strlen(host_state_str(h[i].state)));
        w->max          = imax(w->max,          ndigits(h[i].max_jobs));
        w->total_cpu    = imax(w->total_cpu,    ndigits(h[i].total_cpu));
        w->used_cpu     = imax(w->used_cpu,     ndigits(used_cpu));
        w->total_gpu    = imax(w->total_gpu,    ndigits(h[i].total_gpu));
        w->used_gpu     = imax(w->used_gpu,     ndigits(used_gpu));
        w->njobs        = imax(w->njobs,        ndigits(h[i].num_jobs));
        w->run          = imax(w->run,          ndigits(h[i].num_run));
        w->susp         = imax(w->susp,         ndigits(h[i].num_susp));
    }
}

static void usage(void)
{
    fprintf(stderr, "bhosts: --help display this help and exit\n"
            "--version output version information and exit\n");
}

static struct option longopts[] = {
    {"help",    no_argument, NULL, 'h'},
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

    printf("%-*s  %-*s  %*s  %*s  %*s  %*s  %*s  %*s  %*s  %*s  %*s  %*s  %*s  %*s\n",
           w.name,         "HOST_NAME",
           w.state,        "STATE",
           w.max,          "MAX",
           w.total_cpu,    "NCPU",
           w.mem,          "MEM",
           w.storage,      "STOR",
           w.total_gpu,    "NGPU",
           w.njobs,        "NJOBS",
           w.run,          "RUN",
           w.susp,         "SUSP",
           w.used_cpu,     "USED_CPU",
           w.used_mem,     "USED_MEM",
           w.used_storage, "USED_STOR",
           w.used_gpu,     "USED_GPU");

    char mem_buf[FMT_BUF_LEN];
    char used_mem_buf[FMT_BUF_LEN];
    char stor_buf[FMT_BUF_LEN];
    char used_stor_buf[FMT_BUF_LEN];

    for (int i = 0; i < nhosts; i++) {
        uint64_t used_mem  = hosts[i].total_mem_mb  - hosts[i].free_mem_mb;
        uint64_t used_stor = hosts[i].total_storage_mb - hosts[i].free_storage_mb;
        int32_t  used_cpu  = hosts[i].total_cpu - hosts[i].free_cpu;
        int32_t  used_gpu  = hosts[i].total_gpu - hosts[i].free_gpu;

        fmt_mb(hosts[i].total_mem_mb,  mem_buf,       sizeof(mem_buf));
        fmt_mb(used_mem,               used_mem_buf,  sizeof(used_mem_buf));
        fmt_mb(hosts[i].total_storage_mb, stor_buf,   sizeof(stor_buf));
        fmt_mb(used_stor,              used_stor_buf, sizeof(used_stor_buf));

        printf("%-*s  %-*s  %*d  %*d  %*s  %*s  %*d  %*d  %*d  %*d  %*d  %*s  %*s  %*d\n",
               w.name,         hosts[i].name,
               w.state,        host_state_str(hosts[i].state),
               w.max,          hosts[i].max_jobs,
               w.total_cpu,    hosts[i].total_cpu,
               w.mem,          mem_buf,
               w.storage,      stor_buf,
               w.total_gpu,    hosts[i].total_gpu,
               w.njobs,        hosts[i].num_jobs,
               w.run,          hosts[i].num_run,
               w.susp,         hosts[i].num_susp,
               w.used_cpu,     used_cpu,
               w.used_mem,     used_mem_buf,
               w.used_storage, used_stor_buf,
               w.used_gpu,     used_gpu);
    }

    llb_free_host_info(hosts, nhosts);
    return 0;
}
