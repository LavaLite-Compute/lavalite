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
    /* Ports */
    LL_LIM_PORT,
    LL_MBD_PORT,
    LL_SBD_PORT,
    /* Events */
    LL_EVENTS_MAX_SIZE,
    LL_EVENTS_RETAIN,
    /* lib.common */
    LL_API_CONNTIMEOUT,
    LL_API_RECVTIMEOUT,
    /* SBD */
    LL_SBD_CONNTIMEOUT,
    LL_SBD_READTIMEOUT,
    PARAMS_COUNT,
};

extern struct ll_kv ll_params[];

/* Load KEY=VALUE pairs from path into items[0..nitems-1].
 * Skips blank lines and # comments.
 * Returns 0 on success, -1 on error.
 */
int ll_conf_load(struct ll_kv *, int, const char *);
void ll_conf_free(struct ll_kv *, int);
char *ll_conf_parse_begin(char *);
char *ll_conf_parse_end(char *);
char *ll_conf_kv_get(struct ll_kv *, int, const char *);
void rtrim(char *);
char *ltrim(char *);
int ll_conf_param_missing(const char *, const char *);
