/* Copyright (C) LavaLite Contributors
 * GPL v2
 */
#include "base/lib/ll.conf.h"

struct ll_kv ll_params[PARAMS_COUNT] = {
    [LL_CONFDIR]          = {"LL_CONFDIR",          NULL},
    [LL_SERVERDIR]        = {"LL_SERVERDIR",         NULL},
    [LL_BINDIR]           = {"LL_BINDIR",            NULL},
    [LL_SHAREDIR]         = {"LL_SHAREDIR",          NULL},
    [LL_ENVDIR]           = {"LL_ENVDIR",            NULL},
    [LL_LOGDIR]           = {"LL_LOGDIR",            NULL},
    [LL_LOG_MASK]         = {"LL_LOG_MASK",          "LOG_WARNING"},
    [LL_CLUSTER_NAME]     = {"LL_CLUSTER_NAME",      NULL},
    [LL_LIM_PORT]         = {"LL_LIM_PORT",          "33123"},
    [LL_MBD_PORT]         = {"LL_MBD_PORT",          "33124"},
    [LL_SBD_PORT]         = {"LL_SBD_PORT",          "33125"},
    [LL_EVENTS_MAX_SIZE]  = {"LL_EVENTS_MAX_SIZE",   "100M"},
    [LL_EVENTS_RETAIN]    = {"LL_EVENTS_RETAIN",     "24h"},
    [LL_API_CONNTIMEOUT]  = {"LL_API_CONNTIMEOUT",   "3"},
    [LL_API_RECVTIMEOUT]  = {"LL_API_RECVTIMEOUT",   "5"},
    [LL_SBD_CONNTIMEOUT]  = {"LL_SBD_CONNTIMEOUT",   NULL},
    [LL_SBD_READTIMEOUT]  = {"LL_SBD_READTIMEOUT",   NULL},
    [LL_MBD_HOST]         = {"LL_MBD_HOST",          NULL},
    [LL_MBD_USER]         = {"LL_MBD_USER",          "lavalite"}
};

static uint16_t initialized;

int ll_init(void)
{
    if (initialized)
        return 0;

    char *conf_dir = getenv("LL_ENVDIR");
    if (conf_dir == NULL)
        return -1;

    char path[PATH_MAX];
    int cc = snprintf(path, sizeof(path), "%s/ll.conf", conf_dir);
    if (cc < 0 || cc >= (int)sizeof(path))
        return -1;

    if (ll_conf_load(ll_params, PARAMS_COUNT, path) < 0)
        return -1;

    initialized = 1;
    return 0;
}

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
