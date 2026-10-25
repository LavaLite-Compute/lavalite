/* Copyright (C) LavaLite Contributors
 * GPL v2
 */
#include "config.h"
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

    switch (status & ~(HOST_CLOSED | HOST_TAINTED)) {
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
    snprintf(buf, sizeof(buf), "%s%s%s", state,
             (status & HOST_CLOSED) ? "|closed" : "",
             (status & HOST_TAINTED) ? "|tainted" : "");
    return buf;
}

static int imax(int a, int b)
{
    if (a > b)
        return a;
    return b;
}

static int ndigits(int32_t n)
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

/* memory: stored and displayed in MB */
static void fmt_mem(uint64_t mb, char *buf, size_t len)
{
    snprintf(buf, len, "%lu", mb);
}

/* storage: stored in MB, displayed in GB */
static void fmt_stor(uint64_t mb, char *buf, size_t len)
{
    snprintf(buf, len, "%lu", (mb / 1024));
}

struct col_widths {
    int name;
    int state;
    int total_cpu;
    int mem;
    int storage;
    int total_gpu;
    int njobs;
    int run;
};

#define FMT_BUF_LEN 32

static void compute_widths(struct host_info *h, int n, struct col_widths *w)
{
    char tmp[FMT_BUF_LEN];

    w->name = strlen("HOST_NAME");
    w->state = strlen("STATE");
    w->total_cpu = strlen("NCPU");
    w->mem = strlen("MEM_MB");
    w->storage = strlen("STOR_GB");
    w->total_gpu = strlen("NGPU");
    w->njobs = strlen("NJOBS");
    w->run = strlen("RUN");

    for (int i = 0; i < n; i++) {
        fmt_mem(h[i].total_mem_mb, tmp, sizeof(tmp));
        w->mem = imax(w->mem, (int) strlen(tmp));
        fmt_stor(h[i].total_storage_mb, tmp, sizeof(tmp));
        w->storage = imax(w->storage, (int) strlen(tmp));

        w->name = imax(w->name, (int) strlen(h[i].name));
        w->state = imax(w->state, (int) strlen(host_state_str(h[i].state)));
        w->total_cpu = imax(w->total_cpu, ndigits(h[i].total_cpu));
        w->total_gpu = imax(w->total_gpu, ndigits(h[i].total_gpu));
        w->njobs = imax(w->njobs, ndigits(h[i].num_jobs));
        w->run = imax(w->run, ndigits(h[i].num_run));
    }
}

static void print_short(struct host_info *h, int n)
{
    struct col_widths w;
    char mem_buf[FMT_BUF_LEN];
    char stor_buf[FMT_BUF_LEN];

    compute_widths(h, n, &w);

    printf("%-*s  %-*s  %*s  %*s  %*s  %*s  %*s  %*s\n",
           w.name, "HOST_NAME", w.state, "STATE", w.total_cpu, "NCPU",
           w.mem, "MEM_MB", w.storage, "STOR_GB", w.total_gpu, "NGPU",
           w.njobs, "NJOBS", w.run, "RUN");

    for (int i = 0; i < n; i++) {
        fmt_mem(h[i].total_mem_mb, mem_buf, sizeof(mem_buf));
        fmt_stor(h[i].total_storage_mb, stor_buf, sizeof(stor_buf));

        printf("%-*s  %-*s  %*d  %*s  %*s  %*d  %*d  %*d\n",
               w.name, h[i].name, w.state, host_state_str(h[i].state),
               w.total_cpu, h[i].total_cpu, w.mem, mem_buf,
               w.storage, stor_buf, w.total_gpu, h[i].total_gpu,
               w.njobs, h[i].num_jobs, w.run, h[i].num_run);
    }
}

/* one block per host, used/total per resource */
static void print_long(struct host_info *h, int n)
{
    for (int i = 0; i < n; i++) {
        uint64_t used_mem = h[i].total_mem_mb - h[i].free_mem_mb;
        uint64_t used_stor = h[i].total_storage_mb - h[i].free_storage_mb;

        if (i > 0)
            printf("\n");
        printf("HOST: %s\n", h[i].name);
        printf("  STATE:     %s\n", host_state_str(h[i].state));
        printf("  JOBS:      %d (run %d, susp %d)\n",
               h[i].num_jobs, h[i].num_run, h[i].num_susp);
        printf("  CPU:       %d/%d\n",
               h[i].total_cpu - h[i].free_cpu, h[i].total_cpu);
        printf("  MEM_MB:    %lu/%lu\n",
               used_mem, h[i].total_mem_mb);
        printf("  STOR_GB:   %lu/%lu\n",
               (used_stor / 1024),
               (h[i].total_storage_mb / 1024));
        printf("  GPU:       %d/%d\n",
               h[i].total_gpu - h[i].free_gpu, h[i].total_gpu);
        printf("  GPU_MODEL: %s\n", h[i].gpu_model ? h[i].gpu_model : "-");
        printf("  GPU_IDS:   %s\n", h[i].gpu_ids ? h[i].gpu_ids : "-");
    }
}

/* Move the hosts named on the command line to the front, in the
 * order given, and return how many. The array keeps all its entries,
 * so llb_free_host_info() still frees them all.
 */
static int select_hosts(struct host_info *h, int n, char **names,
                        int nnames, int *bad)
{
    int m = 0;

    for (int k = 0; k < nnames; k++) {
        int i = 0;
        while (i < n && strcmp(h[i].name, names[k]) != 0)
            i++;
        if (i == n) {
            fprintf(stderr, "bhosts: %s: no such host\n", names[k]);
            *bad = 1;
            continue;
        }
        if (i < m)
            continue;
        struct host_info tmp = h[m];
        h[m] = h[i];
        h[i] = tmp;
        m++;
    }

    return m;
}

static void usage(void)
{
    fprintf(stderr, "bhosts: [-l] [HOST...]\n"
                    "  --help         display this help and exit\n"
                    "  -l, --long     per-host detail\n"
                    "  -c, --close    HOST close a host\n"
                    "  -o, --open     HOST open a host\n"
                    "  --version      output version information and exit\n");
}

static struct option longopts[] = {{"help", no_argument, NULL, 'h'},
                                   {"version", no_argument, NULL, 'v'},
                                   {"long", no_argument, NULL, 'l'},
                                   {"close", required_argument, NULL, 'c'},
                                   {"open", required_argument, NULL, 'o'},
                                   {NULL, 0, NULL, 0}};

int main(int argc, char **argv)
{
    int cc;
    const char *close_host = NULL;
    const char *open_host = NULL;
    int long_fmt = 0;

    while ((cc = getopt_long(argc, argv, "lc:o:hv", longopts, NULL)) != EOF) {
        switch (cc) {
        case 'l':
            long_fmt = 1;
            break;
        case 'c':
            close_host = optarg;
            break;
        case 'o':
            open_host = optarg;
            break;
        case 'v':
            fprintf(stderr, "%s\n", LAVALITE_VERSION_STR);
            return 0;
        case 'h':
        default:
            usage();
            return 0;
        }
    }

    if (close_host) {
        int rc = llb_host_admin(close_host, HOST_CLOSED);
        if (rc != 0)
            fprintf(stderr, "bhosts: %s: %m\n", close_host);
        else
            printf("host %s closed\n", close_host);
        return rc;
    }

    if (open_host) {
        int rc = llb_host_admin(open_host, 0);
        if (rc != 0)
            fprintf(stderr, "bhosts: %s: %m\n", open_host);
        else
            printf("host %s opened\n", open_host);
        return rc;
    }

    int nhosts;
    struct host_info *hosts = llb_host_info(&nhosts);
    if (!hosts) {
        fprintf(stderr, "bhosts: failed\n");
        return -1;
    }

    int bad = 0;
    int nprint = nhosts;
    if (optind < argc)
        nprint = select_hosts(hosts, nhosts, &argv[optind], argc - optind,
                              &bad);

    if (long_fmt)
        print_long(hosts, nprint);
    else
        print_short(hosts, nprint);

    llb_free_host_info(hosts, nhosts);
    return bad;
}
