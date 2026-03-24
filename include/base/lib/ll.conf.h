/* Copyright (C) LavaLite Contributors
 * GPL v2
 */
#pragma once

#include "base/lib/ll.sys.h"

struct ll_kv {
    const char *key;
    char *val;
};

enum ll_params {
    /* Common */
    LL_CONFDIR,
    LL_SERVERDIR,
    LL_BINDIR,
    LL_SHAREDIR,
    LL_ENVDIR,
    LL_LOGDIR,
    LL_LOG_MASK,
    LL_CLUSTER_NAME,
    LL_LIM_PORT,
    LL_MBD_PORT,
    LL_SBD_PORT,
    LL_EVENTS_MAX_SIZE,
    LL_EVENTS_RETAIN,
    LL_API_CONNTIMEOUT,
    LL_API_RECVTIMEOUT,
    LL_SBD_CONNTIMEOUT,
    LL_SBD_READTIMEOUT,
    LL_MBD_HOST,
    LL_MBD_USER,
    PARAMS_COUNT,
};

extern struct ll_kv ll_params[];

/* Load KEY=VALUE pairs from path into items[0..nitems-1].
 * Skips blank lines and # comments.
 * Returns 0 on success, -1 on error.
 */
int ll_init(void);
int ll_conf_load(struct ll_kv *, int, const char *);
void ll_conf_free(struct ll_kv *, int);
char *ll_conf_parse_begin(char *);
char *ll_conf_parse_end(char *);
char *ll_conf_kv_get(struct ll_kv *, int, const char *);
void rtrim(char *);
char *ltrim(char *);
int ll_conf_param_missing(const char *, const char *);
