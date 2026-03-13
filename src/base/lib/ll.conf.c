/* Copyright (C) LavaLite Contributors
 * GPL v2
 */
#include "base/lib/ll.conf.h"

struct ll_kv ll_params[LL_PARAMS_COUNT] = {
    [LSF_CONFDIR]          = {"LSF_CONFDIR",          NULL},
    [LSF_SERVERDIR]        = {"LSF_SERVERDIR",         NULL},
    [LSF_LOGDIR]           = {"LSF_LOGDIR",            NULL},
    [LSF_LIM_DEBUG]        = {"LSF_LIM_DEBUG",         NULL},
    [LSF_LIM_PORT]         = {"LSF_LIM_PORT",          NULL},
    [LSF_LOG_MASK]         = {"LSF_LOG_MASK",          NULL},
    [LSF_DEBUG_LIM]        = {"LSF_DEBUG_LIM",         NULL},
    [LSF_TIME_LIM]         = {"LSF_TIME_LIM",          NULL},
    [LSF_API_CONNTIMEOUT]  = {"LSF_API_CONNTIMEOUT",   NULL},
    [LSF_API_RECVTIMEOUT]  = {"LSF_API_RECVTIMEOUT",   NULL},
    [LSB_SBD_PORT]         = {"LSB_SBD_PORT",          NULL},
    [LSB_DEBUG_SBD]        = {"LSB_DEBUG_SBD",         NULL},
    [LSB_TIME_SBD]         = {"LSB_TIME_SBD",          NULL},
    [LSB_SBD_CONNTIMEOUT]  = {"LSB_SBD_CONNTIMEOUT",   NULL},
    [LSB_SBD_READTIMEOUT]  = {"LSB_SBD_READTIMEOUT",   NULL},
    [LSB_MBD_PORT]         = {"LSB_MBD_PORT",          NULL},
};

char *ltrim(char *s)
{
    while (*s == ' ' || *s == '\t')
        s++;
    return s;
}

void rtrim(char *s)
{
    char *p = s + strlen(s);
    while (p > s && (p[-1] == '\n' || p[-1] == '\r' ||
                     p[-1] == ' '  || p[-1] == '\t'))
        p--;
    *p = '\0';
}

static int kv_find(struct ll_kv *items, int nitems, const char *key)
{
    for (int i = 0; i < nitems; i++) {
        if (items[i].key == NULL)
            continue;
        if (strcmp(items[i].key, key) == 0)
            return i;
    }

    return -1;
}

char *ll_conf_parse_begin(char *line)
{
    char *p = ltrim(line);
    if (strncmp(p, "Begin", 5) != 0)
        return NULL;
    p += 5;
    if (*p != ' ' && *p != '\t')
        return NULL;
    return ltrim(p);
}

char *ll_conf_parse_end(char *line)
{
    char *p = ltrim(line);
    if (strncmp(p, "End", 3) != 0)
        return NULL;
    p += 3;
    if (*p != ' ' && *p != '\t')
        return NULL;
    return ltrim(p);
}

char *ll_conf_kv_get(struct ll_kv *items, int nitems, const char *key)
{
    if (items == NULL || key == NULL)
        return NULL;

    int idx = kv_find(items, nitems, key);
    if (idx < 0)
        return NULL;

    return items[idx].val;
}

int ll_conf_load(struct ll_kv *items, int nitems, const char *path)
{

    char *p;
    char *eq;
    char *key;
    char *val;
    int idx;

    FILE *f = fopen(path, "r");
    if (f == NULL)
        return -1;

    char line[LL_BUFSIZ_1K];
    while (fgets(line, sizeof(line), f) != NULL) {
        rtrim(line);
        p = ltrim(line);

        if (*p == 0 || *p == '#')
            continue;

        eq = strchr(p, '=');
        if (eq == NULL)
            continue;

        *eq = '\0';
        key = p;
        val = eq + 1;

        rtrim(key);
        key = ltrim(key);
        val = ltrim(val);
        rtrim(val);

        idx = kv_find(items, nitems, key);
        if (idx < 0)
            continue;  /* unknown key, skip */

        free(items[idx].val);
        items[idx].val = strdup(val);
        if (items[idx].val == NULL) {
            fclose(f);
            return -1;
        }
    }

    if (ferror(f)) {
        fclose(f);
        return -1;
    }

    fclose(f);
    return 0;
}

void ll_conf_free(struct ll_kv *items, int nitems)
{
    int i;

    if (items == NULL)
        return;

    for (i = 0; i < nitems; i++) {
        free(items[i].val);
        items[i].val = NULL;
    }
}

int ll_conf_param_missing(const char *name, const char *val)
{
    if (val == NULL || val[0] == 0) {
        errno = EINVAL;
        return 1;
    }
    return 0;
}
