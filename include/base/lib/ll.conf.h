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
    LSF_CONFDIR,
    LSF_SERVERDIR,
    LSF_LOGDIR,
    LSF_LIM_DEBUG,
    LSF_LIM_PORT,
    LSF_LOG_MASK,

    /* LIM */
    LSF_DEBUG_LIM,
    LSF_TIME_LIM,

    /* lib.common */
    LSF_API_CONNTIMEOUT,
    LSF_API_RECVTIMEOUT,

    /* SBD */
    LSB_SBD_PORT,
    LSB_DEBUG_SBD,
    LSB_TIME_SBD,
    LSB_SBD_CONNTIMEOUT,
    LSB_SBD_READTIMEOUT,

    /* MBD */
    LSB_MBD_PORT,

    LSF_PARAM_COUNT  /* array sizing sentinel */
};

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
