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
    snprintf(buf, len, "%lu", (unsigned long)mb);
}

/* storage: stored in MB, displayed in GB */
static void fmt_stor(uint64_t mb, char *buf, size_t len)
{
    snprintf(buf, len, "%lu", (unsigned long)(mb / 1024));
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
    int gpu_model;
    int gpu_ids;
};

#define FMT_BUF_LEN 32

static void compute_widths(struct host_info *h, int n, struct col_widths *w)
{
    char tmp[FMT_BUF_LEN];

    w->name         = strlen("HOST_NAME");
    w->state        = strlen("STATE");
    w->max          = strlen("MAX");
    w->total_cpu    = strlen("NCPU");
    w->used_cpu     = strlen("USED_CPU");
    w->mem          = strlen("MEM_MB");
    w->used_mem     = strlen("USED_MB");
    w->storage      = strlen("STOR_GB");
    w->used_storage = strlen("USED_GB");
    w->total_gpu    = strlen("NGPU");
    w->used_gpu     = strlen("USED_GPU");
    w->njobs        = strlen("NJOBS");
    w->run          = strlen("RUN");
    w->susp         = strlen("SUSP");
    w->gpu_model     = strlen("GPU_MODEL");
    w->gpu_ids      = strlen("GPU_IDS");

    for (int i = 0; i < n; i++) {
        uint64_t used_mem  = h[i].total_mem_mb - h[i].free_mem_mb;
        uint64_t used_stor = h[i].total_storage_mb - h[i].free_storage_mb;
        int32_t  used_cpu  = h[i].total_cpu - h[i].free_cpu;
        int32_t  used_gpu  = h[i].total_gpu - h[i].free_gpu;

        fmt_mem(h[i].total_mem_mb, tmp, sizeof(tmp));
        w->mem = imax(w->mem, (int)strlen(tmp));
        fmt_mem(used_mem, tmp, sizeof(tmp));
        w->used_mem = imax(w->used_mem, (int)strlen(tmp));
        fmt_stor(h[i].total_storage_mb, tmp, sizeof(tmp));
        w->storage = imax(w->storage, (int)strlen(tmp));
        fmt_stor(used_stor, tmp, sizeof(tmp));
        w->used_storage = imax(w->used_storage, (int)strlen(tmp));

        w->name      = imax(w->name,      (int)strlen(h[i].name));
        w->state     = imax(w->state,     (int)strlen(host_state_str(h[i].state)));
        w->max       = imax(w->max,       ndigits(h[i].max_jobs));
        w->total_cpu = imax(w->total_cpu, ndigits(h[i].total_cpu));
        w->used_cpu  = imax(w->used_cpu,  ndigits(used_cpu));
        w->total_gpu = imax(w->total_gpu, ndigits(h[i].total_gpu));
        w->used_gpu  = imax(w->used_gpu,  ndigits(used_gpu));
        w->njobs     = imax(w->njobs,     ndigits(h[i].num_jobs));
        w->run       = imax(w->run,       ndigits(h[i].num_run));
        w->susp      = imax(w->susp,      ndigits(h[i].num_susp));
        if (h[i].gpu_model != NULL)
            w->gpu_model  = imax(w->gpu_model, (int)strlen(h[i].gpu_model));
        if (h[i].gpu_ids != NULL)
            w->gpu_ids   = imax(w->gpu_ids,  (int)strlen(h[i].gpu_ids));
    }
}

static void usage(void)
{
    fprintf(stderr, "bhosts: --help display this help and exit\n"
                    "  -c, --close    HOST close a host\n"
                    "  -o, --open     HOST open a host\n"
                    "  --version      output version information and exit\n");
}

static struct option longopts[] = {
    { "help",    no_argument,       NULL, 'h' },
    { "version", no_argument,       NULL, 'v' },
    { "close",   required_argument, NULL, 'c' },
    { "open",    required_argument, NULL, 'o' },
    { NULL, 0, NULL, 0 }
};

int main(int argc, char **argv)
{
    int cc;
    const char *close_host = NULL;
    const char *open_host  = NULL;

    while ((cc = getopt_long(argc, argv, "c:o:hv", longopts, NULL)) != EOF) {
        switch (cc) {
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

    struct col_widths w;
    char mem_buf[FMT_BUF_LEN];
    char used_mem_buf[FMT_BUF_LEN];
    char stor_buf[FMT_BUF_LEN];
    char used_stor_buf[FMT_BUF_LEN];

    compute_widths(hosts, nhosts, &w);

    printf("%-*s  %-*s  %*s  %*s  %*s  %*s  %*s  %*s  %*s  %*s  %*s  %*s  %*s  %*s  %-*s  %-*s\n",
           w.name,         "HOST_NAME",
           w.state,        "STATE",
           w.max,          "MAX",
           w.total_cpu,    "NCPU",
           w.mem,          "MEM_MB",
           w.storage,      "STOR_GB",
           w.total_gpu,    "NGPU",
           w.njobs,        "NJOBS",
           w.run,          "RUN",
           w.susp,         "SUSP",
           w.used_cpu,     "USED_CPU",
           w.used_mem,     "USED_MB",
           w.used_storage, "USED_GB",
           w.used_gpu,     "USED_GPU",
           w.gpu_model,     "GPU_MODEL",
           w.gpu_ids,      "GPU_IDS");

    for (int i = 0; i < nhosts; i++) {
        uint64_t used_mem  = hosts[i].total_mem_mb - hosts[i].free_mem_mb;
        uint64_t used_stor = hosts[i].total_storage_mb - hosts[i].free_storage_mb;
        int32_t  used_cpu  = hosts[i].total_cpu - hosts[i].free_cpu;
        int32_t  used_gpu  = hosts[i].total_gpu - hosts[i].free_gpu;

        fmt_mem(hosts[i].total_mem_mb,    mem_buf,      sizeof(mem_buf));
        fmt_mem(used_mem,                 used_mem_buf, sizeof(used_mem_buf));
        fmt_stor(hosts[i].total_storage_mb, stor_buf,   sizeof(stor_buf));
        fmt_stor(used_stor,               used_stor_buf, sizeof(used_stor_buf));

        printf("%-*s  %-*s  %*d  %*d  %*s  %*s  %*d  %*d  %*d  %*d  %*d  %*s  %*s  %*d  %-*s  %-*s\n",
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
               w.used_gpu,     used_gpu,
               w.gpu_model,     hosts[i].gpu_model ? hosts[i].gpu_model : "-",
               w.gpu_ids,      hosts[i].gpu_ids  ? hosts[i].gpu_ids  : "-");
    }

    llb_free_host_info(hosts, nhosts);
    return 0;
}
