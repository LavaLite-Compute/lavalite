/* Copyright (C) LavaLite Contributors
 * GPL v2
 */
#pragma once


// Explicit. Verbose. Clear. Implicit nothing.

#include <config.h>
// System headers
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <sys/param.h>

// 11 load indexes collected by lim on every cluster machine
enum lim_load_index {
    R15S = 0,
    R1M  = 1,
    R15M = 2,
    UT   = 3,
    PG   = 4,
    IO   = 5,
    LS   = 6,
    IT   = 7,
    TMP  = 8,
    SWP  = 9,
    MEM  = 10,
    NUM_METRICS = 11,   // number of built-in indices
};

extern __thread int lserrno;

enum ll_err {
    LL_OK           = 0,
    LL_ERR_LIM_DOWN = 1,   /* LIM not reachable */
    LL_ERR_NO_MASTER = 2,  /* master unknown */
    LL_ERR_CONF     = 3,   /* config error */
    LL_ERR_PROTO    = 4,   /* protocol/XDR error */
    LL_ERR_PERM     = 5,   /* permission denied */
};

enum ll_buf_siz {
    LL_BUFSIZE_32 = 32,
    LL_BUFSIZE_64 = 64,
    LL_BUFSIZE_256 = 256,
    LL_BUFSIZE_1K = 1024,
    LL_BUFSIZE_4K = 4096,
};

enum lim_err {
    LIM_OK = 0,
    LIM_ERROR = -1
};

enum lim_stat {
    LIM_STAT_OK,
    LIM_STAT_CLOSED
};

struct ll_cluster {
    char cluster_name[LL_BUFSIZE_64];
    char master_name[MAXHOSTNAMELEN];
    char manager_name[LL_BUFSIZE_32];
};

struct ll_cluster_host {
    char host_name[MAXHOSTNAMELEN];
    char host_type[LL_BUFSIZE_32];
    uint64_t num_cpus;
    uint64_t max_mem;
    uint64_t max_swap;
    uint64_t max_tmp;
    uint16_t is_master;
};

struct ll_load {
    char hostname[MAXHOSTNAMELEN];
    uint32_t status;
    uint32_t num_metrics;
    float li[NUM_METRICS];
};

char *ls_getmastername(void);
char *ls_getmyhostname(void);

struct ll_host *ls_gethostinfo(int *);
struct ll_load *ls_load(int *);
struct ll_cluster *ls_clusterinfo(void);
