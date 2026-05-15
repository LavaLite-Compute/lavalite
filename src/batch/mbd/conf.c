// Copyright (C) LavaLite Contributors
// GPL v2

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <syslog.h>
#include <pwd.h>

#include "base/lib/ll.syslog.h"
#include "base/lib/ll.conf.h"
#include "batch/mbd/mbd.h"

static uint64_t parse_mem(const char *s)
{
    char *end;
    uint64_t v;

    if (s == NULL || *s == 0)
        return 0;

    v = strtoull(s, &end, 10);
    if (end == s)
        return 0;

    if (*end == 0)
        return v;

    if ((end[1] != 0))
        return 0;

    if (*end == 'G' || *end == 'g')
        return v * 1024;

    if (*end == 'T' || *end == 't')
        return v * 1024 * 1024;

    return 0;
}

static struct mbd_host *find_host_by_name(const char *name)
{
    return (struct mbd_host *)ll_hash_search(&host_name_hash, name);
}

static struct mbd_gpu *make_gpu(const char *p)
{
    struct mbd_gpu *g;
    char hostname[MAXHOSTNAMELEN];
    char model[LL_BUFSIZ_64];
    char gpu_type[LL_BUFSIZ_64];
    int gpu_id;
    int count;

    g = calloc(1, sizeof(*g));
    if (g == NULL) {
        LS_ERR("calloc failed");
        return NULL;
    }

    /* HOST_NAME  GPU_ID  GPU_MODEL  GPU_TYPE  COUNT */
    int n = sscanf(p, "%255s %d %63s %63s %d",
                   hostname, &gpu_id, model, gpu_type, &count);
    if (n != 5) {
        LS_ERRX("bad gpu line: %s", p);
        free(g);
        return NULL;
    }

    struct mbd_host *h = find_host_by_name(hostname);
    if (h == NULL) {
        LS_ERRX("gpu references unknown host=%s", hostname);
        free(g);
        return NULL;
    }

    g->gpu_id = gpu_id;
    g->count  = count;
    g->free   = count;
    ll_strlcpy(g->model,    model,    sizeof(g->model));
    ll_strlcpy(g->gpu_type, gpu_type, sizeof(g->gpu_type));

    /* aggregate totals on host */
    h->res.total_gpu += count;
    h->res.free_gpu  += count;

    ll_list_append(&h->res.gpu_list, &g->ent);
    ll_hash_insert(&h->res.gpu_hash, g->gpu_type, g, 0);

    return g;
}

static int parse_gpus(const char *path)
{
    FILE *f;
    char line[LL_BUFSIZ_1K];
    int in_section;
    int header_skipped;

    f = fopen(path, "r");
    if (f == NULL) {
        LS_ERR("fopen=%s failed", path);
        return -1;
    }

    in_section     = 0;
    header_skipped = 0;

    while (fgets(line, sizeof(line), f) != NULL) {
        char *p = ltrim(line);
        rtrim(p);

        if (*p == 0 || *p == '#')
            continue;

        char *section = ll_conf_parse_begin(p);
        if (section != NULL) {
            if (strncmp(section, "Gpu", 3) == 0) {
                in_section     = 1;
                header_skipped = 0;
            }
            continue;
        }

        if (!in_section)
            continue;

        if (!header_skipped) {
            header_skipped = 1;
            continue;
        }

        if (ll_conf_parse_end(p)) {
            fclose(f);
            return 0;
        }

        if (make_gpu(p) == NULL) {
            LS_ERRX("make_gpu failed line=%s", p);
            fclose(f);
            return -1;
        }
    }

    fclose(f);
    LS_ERRX("missing End Gpu in %s", path);
    return -1;
}

static int parse_token_pools(const char *path)
{
    FILE *f;
    char line[LL_BUFSIZ_1K];
    int in_section;
    int header_skipped;

    f = fopen(path, "r");
    if (f == NULL) {
        LS_ERR("fopen=%s failed", path);
        return -1;
    }

    in_section     = 0;
    header_skipped = 0;

    while (fgets(line, sizeof(line), f) != NULL) {
        char *p = ltrim(line);
        rtrim(p);

        if (*p == 0 || *p == '#')
            continue;

        char *section = ll_conf_parse_begin(p);
        if (section != NULL) {
            if (strncmp(section, "TokenPool", 9) == 0) {
                in_section     = 1;
                header_skipped = 0;
            }
            continue;
        }

        if (!in_section)
            continue;

        if (!header_skipped) {
            header_skipped = 1;
            continue;
        }

        if (ll_conf_parse_end(p)) {
            fclose(f);
            return 0;
        }

        char name[LL_BUFSIZ_64];
        int  total;

        if (sscanf(p, "%63s %d", name, &total) != 2) {
            LS_ERRX("bad tokenpool line: %s", p);
            fclose(f);
            return -1;
        }

        if (total <= 0) {
            LS_ERRX("tokenpool=%s invalid total=%d", name, total);
            fclose(f);
            return -1;
        }

        struct mbd_token_pool *tp = calloc(1, sizeof(*tp));
        if (tp == NULL) {
            LS_ERR("calloc failed");
            fclose(f);
            return -1;
        }

        ll_strlcpy(tp->name, name, sizeof(tp->name));
        tp->total = total;
        tp->free  = total;

        ll_list_append(&token_pool_list, &tp->ent);
        ll_hash_insert(&token_pool_name_hash, tp->name, tp, 0);

        LS_INFO("tokenpool name=%s total=%d", tp->name, tp->total);
    }

    fclose(f);

    /* TokenPool section is optional */
    return 0;
}

static struct mbd_host *make_host(const char *p)
{
    struct mbd_host *h = calloc(1, sizeof(*h));
    if (h == NULL) {
        LS_ERR("calloc failed");
        return NULL;
    }

    /* HOST_NAME  MXJ  CPU  MEM  STORAGE */
    int n;
    char mem_str[LL_BUFSIZ_32];
    char storage_str[LL_BUFSIZ_32];
    char hostname[LL_BUFSIZ_64];
    n = sscanf(p, "%63s %d %d %31s %31s",
               hostname,
               &h->res.max_jobs,
               &h->res.total_cpu,
               mem_str,
               storage_str);
    if (n != 5) {
        LS_ERRX("bad line: %s", p);
        free(h);
        return NULL;
    }

    if (get_host_by_name(hostname, &h->net) < 0) {
        LS_ERR("get_host_by_name failed host=%s", hostname);
        free(h);
        return NULL;
    }

    h->res.total_mem_mb = parse_mem(mem_str);
    if (h->res.total_mem_mb == 0) {
        LS_ERRX("bad memory value host=%s mem=%s", hostname, mem_str);
        free(h);
        return NULL;
    }

    h->res.total_storage_mb = parse_mem(storage_str);
    if (h->res.total_storage_mb == 0) {
        LS_ERRX("bad storage value host=%s storage=%s", hostname, storage_str);
        free(h);
        return NULL;
    }

    ll_list_init(&h->res.gpu_list);
    ll_hash_init(&h->res.gpu_hash, 101);

    h->res.free_cpu        = h->res.total_cpu;
    h->res.free_mem_mb     = h->res.total_mem_mb;
    h->res.free_storage_mb = h->res.total_storage_mb;
    h->sbd_chan = -1;
    h->state   = HOST_UNAVAIL;

    return h;
}

static int parse_hosts(const char *path)
{
    FILE *f;
    char line[LL_BUFSIZ_1K];
    int in_section;
    int header_skipped;

    f = fopen(path, "r");
    if (f == NULL) {
        LS_ERR("fopen=%s failed", path);
        return -1;
    }

    in_section = 0;
    header_skipped = 0;

    while (fgets(line, sizeof(line), f) != NULL) {
        char *p;
        char *section;
        struct mbd_host *h;

        p = ltrim(line);
        rtrim(p);

        if (*p == 0 || *p == '#')
            continue;

        section = ll_conf_parse_begin(p);
        if (section != NULL) {
            if (strncmp(section, "Host", 4) == 0) {
                in_section = 1;
                header_skipped = 0;
            }
            continue;
        }

        if (!in_section)
            continue;

        if (!header_skipped) {
            header_skipped = 1;
            continue;
        }

        if (ll_conf_parse_end(p)) {
            fclose(f);
            return 0;
        }

        h = make_host(p);
        if (h == NULL) {
            LS_ERRX("make_host failed line=%s", p);
            fclose(f);
            return -1;
        }

        ll_list_append(&host_list, &h->ent);
        ll_hash_insert(&host_name_hash, h->net.name, h, 0);
        ll_hash_insert(&host_addr_hash, h->net.addr, h, 0);

        LS_INFO("host=%s cpu=%d mem=%luMB storage=%luMB",
                h->net.name, h->res.total_cpu,
                h->res.total_mem_mb, h->res.total_storage_mb);
    }

    fclose(f);
    LS_ERRX("missing End Host in %s", path);
    return -1;
}

static struct mbd_group *make_group(char *p)
{
    struct mbd_group *g;
    char *open;
    char *close;
    char *name;
    char *members;
    char tmp[LL_BUFSIZ_1K];
    char *tok;

    open = strchr(p, '(');
    close = strrchr(p, ')');
    if (open == NULL || close == NULL || close <= open) {
        LS_ERRX("bad line=%s", p);
        return NULL;
    }

    *open = 0;
    *close = 0;

    name = ltrim(p);
    members = ltrim(open + 1);
    rtrim(name);
    rtrim(members);

    g = calloc(1, sizeof(*g));
    if (g == NULL) {
        LS_ERR("calloc failed");
        return NULL;
    }

    ll_strlcpy(g->name, name, LL_BUFSIZ_64);
    ll_strlcpy(g->members, members, LL_BUFSIZ_1K);
    ll_strlcpy(tmp, members, LL_BUFSIZ_1K);

    tok = strtok(tmp, " \t");
    while (tok != NULL) {
        if (!ll_hash_contains(&host_name_hash, tok)) {
            LS_ERRX("host=%s unknown by configuration", tok);
            free(g);
            return NULL;
        }
        g->num_members++;
        tok = strtok(NULL, " \t");
    }

    return g;
}

static int parse_groups(const char *path)
{
    FILE *f;
    char line[LL_BUFSIZ_1K];
    int in_section;
    int header_skipped;

    f = fopen(path, "r");
    if (f == NULL) {
        LS_ERR("fopen=%s failed", path);
        return -1;
    }

    in_section = 0;
    header_skipped = 0;

    while (fgets(line, sizeof(line), f) != NULL) {
        char *p;
        char *section;
        struct mbd_group *g;

        p = ltrim(line);
        rtrim(p);

        if (*p == 0 || *p == '#')
            continue;

        section = ll_conf_parse_begin(p);
        if (section != NULL) {
            if (strncmp(section, "HostGroup", 9) == 0) {
                in_section = 1;
                header_skipped = 0;
            }
            continue;
        }

        if (!in_section)
            continue;

        if (!header_skipped) {
            header_skipped = 1;
            continue;
        }

        if (ll_conf_parse_end(p)) {
            fclose(f);
            return 0;
        }

        g = make_group(p);
        if (g == NULL) {
            LS_ERRX("make_group failed line=%s", p);
            fclose(f);
            return -1;
        }

        ll_list_append(&group_list, &g->ent);
        ll_hash_insert(&group_name_hash, g->name, g, 0);
    }

    fclose(f);
    if (in_section) {
        LS_ERRX("missing End HostGroup in %s", path);
        return -1;
    }
    return 0;
}

static int commit_queue(struct queue_conf *qc)
{
    struct mbd_queue *q;

    if (qc->name[0] == 0) {
        LS_ERRX("queue missing NAME");
        return -1;
    }

    if (qc->hosts_spec[0] == 0) {
        LS_ERRX("queue=%s missing HOSTS", qc->name);
        return -1;
    }

    q = calloc(1, sizeof(struct mbd_queue));
    if (q == NULL) {
        LS_ERRX("calloc failed name=%s", qc->name);
        return -1;
    }

    ll_strlcpy(q->name,       qc->name,       LL_BUFSIZ_64);
    ll_strlcpy(q->description, qc->desc,       LL_BUFSIZ_256);
    ll_strlcpy(q->hosts_spec, qc->hosts_spec,  LL_BUFSIZ_256);
    ll_strlcpy(q->users,      qc->users,       LL_BUFSIZ_256);

    q->priority = qc->priority;
    q->state = QUEUE_OPEN;

    ll_list_append(&queue_list, &q->ent);
    ll_hash_insert(&queue_name_hash, q->name, q, 0);

    return 0;
}

static int parse_queue_conf(struct queue_conf *qc,
                            const char *key,
                            const char *val)
{
    if (strcmp(key, "QUEUE_NAME") == 0)
        return ll_strlcpy(qc->name, val, LL_BUFSIZ_64);

    if (strcmp(key, "PRIORITY") == 0)
        return ll_atoi(val, &qc->priority);

    if (strcmp(key, "DESCRIPTION") == 0)
        return ll_strlcpy(qc->desc, val, LL_BUFSIZ_256);

    if (strcmp(key, "HOSTS") == 0)
        return ll_strlcpy(qc->hosts_spec, val, LL_BUFSIZ_256);

    if (strcmp(key, "USERS") == 0)
        return ll_strlcpy(qc->users, val, LL_BUFSIZ_256);

    LS_ERRX("unknown queue key=%s", key);
    return -1;
}

static int parse_queues(const char *path)
{
    FILE *f;
    struct queue_conf qc;
    char line[LL_BUFSIZ_1K];
    int in_section;

    f = fopen(path, "r");
    if (f == NULL) {
        LS_ERR("fopen=%s failed", path);
        return -1;
    }

    memset(&qc, 0, sizeof(qc));
    in_section = 0;

    while (fgets(line, sizeof(line), f) != NULL) {
        char *p;
        char *section;

        p = ltrim(line);
        rtrim(p);

        if (*p == 0 || *p == '#')
            continue;

        section = ll_conf_parse_begin(p);
        if (section != NULL) {
            if (strncmp(section, "Queue", 5) == 0) {
                in_section = 1;
                memset(&qc, 0, sizeof(qc));
            }
            continue;
        }

        if (!in_section)
            continue;

        if (ll_conf_parse_end(p)) {
            if (commit_queue(&qc) < 0) {
                fclose(f);
                return -1;
            }
            in_section = 0;
            memset(&qc, 0, sizeof(qc));
            continue;
        }

        {
            char *eq;
            char *key;
            char *val;

            eq = strchr(p, '=');
            if (eq == NULL) {
                LS_ERRX("parse_queues: bad line: %s", p);
                fclose(f);
                return -1;
            }

            *eq = 0;
            key = p;
            val = ltrim(eq + 1);
            rtrim(key);
            rtrim(val);

            if (parse_queue_conf(&qc, key, val) < 0) {
                fclose(f);
                return -1;
            }
        }
    }

    fclose(f);

    if (in_section) {
        LS_ERRX("missing End Queue in %s", path);
        return -1;
    }

    return 0;
}

/*
 * Parse Begin Sim section.
 * Each line: SIM_NAME  REAL_HOST  PORT
 * Clones the real host entry with a new name and port override.
 * Sim hosts are registered in host_list and host_name_hash alongside
 * real hosts so the scheduler treats them uniformly.
 * Section is optional — no error if absent.
 */
static int parse_sim(const char *path)
{
    FILE *f = fopen(path, "r");
    if (f == NULL) {
        LS_ERR("fopen=%s failed", path);
        return -1;
    }

    char line[LL_BUFSIZ_1K];
    int in_section     = 0;
    int header_skipped = 0;

    while (fgets(line, sizeof(line), f) != NULL) {
        char *p = ltrim(line);
        rtrim(p);

        if (*p == 0 || *p == '#')
            continue;

        char *section = ll_conf_parse_begin(p);
        if (section != NULL) {
            if (strncmp(section, "Sim", 3) == 0) {
                in_section     = 1;
                header_skipped = 0;
            }
            continue;
        }

        if (!in_section)
            continue;

        if (!header_skipped) {
            header_skipped = 1;
            continue;
        }

        if (ll_conf_parse_end(p)) {
            fclose(f);
            return 0;
        }

        char sim_name[MAXHOSTNAMELEN];
        char real_host[MAXHOSTNAMELEN];
        char mem_str[LL_BUFSIZ_32];
        char storage_str[LL_BUFSIZ_32];
        int  port;
        int  max_jobs;
        int  total_cpu;

        /* NAME  REAL_HOST  PORT  MXJ  CPU  MEM  STORAGE */
        if (sscanf(p, "%255s %255s %d %d %d %31s %31s",
                   sim_name, real_host, &port,
                   &max_jobs, &total_cpu,
                   mem_str, storage_str) != 7) {
            LS_ERRX("bad sim line: %s", p);
            fclose(f);
            return -1;
        }

        if (port < 1 || port > 65535) {
            LS_ERRX("sim=%s invalid port=%d", sim_name, port);
            fclose(f);
            return -1;
        }

        struct mbd_host *real = find_host_by_name(real_host);
        if (real == NULL) {
            LS_ERRX("sim=%s references unknown host=%s", sim_name, real_host);
            fclose(f);
            return -1;
        }

        struct mbd_host *h = calloc(1, sizeof(*h));
        if (h == NULL) {
            LS_ERR("calloc failed");
            fclose(f);
            return -1;
        }

        /* use real host network identity, override name */
        h->net = real->net;
        ll_strlcpy(h->net.name, sim_name, sizeof(h->net.name));

        h->res.max_jobs  = max_jobs;
        h->res.total_cpu = total_cpu;
        h->res.free_cpu  = total_cpu;

        h->res.total_mem_mb = parse_mem(mem_str);
        if (h->res.total_mem_mb == 0) {
            LS_ERRX("sim=%s bad mem=%s", sim_name, mem_str);
            free(h);
            fclose(f);
            return -1;
        }
        h->res.free_mem_mb = h->res.total_mem_mb;

        h->res.total_storage_mb = parse_mem(storage_str);
        if (h->res.total_storage_mb == 0) {
            LS_ERRX("sim=%s bad storage=%s", sim_name, storage_str);
            free(h);
            fclose(f);
            return -1;
        }
        h->res.free_storage_mb = h->res.total_storage_mb;

        ll_list_init(&h->res.gpu_list);
        h->port     = (uint16_t)port;
        h->sbd_chan = -1;
        h->state   = HOST_UNAVAIL;

        ll_list_append(&host_list, &h->ent);
        ll_hash_insert(&host_name_hash, h->net.name, h, 0);

        LS_INFO("sim host=%s index=%d real=%s port=%d cpu=%d mem=%luMB "
                "storage=%luMB", sim_name, h->host_idx, real_host, port,
                total_cpu, h->res.total_mem_mb, h->res.total_storage_mb);
    }

    fclose(f);
    if (in_section) {
        LS_ERRX("missing End Sim in %s", path);
        return -1;
    }

    return 0;
}

/*
 * Expand queue host membership after all hosts, groups and queues are parsed.
 * Resolves hosts_spec (group name or single hostname) into host_hash so the
 * scheduler can do a single hash lookup per host per job.
 */
static int conf_expand_queues(void)
{
    struct ll_list_entry *e;

    for (e = queue_list.head; e; e = e->next) {
        struct mbd_queue *q = (struct mbd_queue *)e;

        ll_hash_init(&q->host_hash, 251);

        struct mbd_group *g = (struct mbd_group *)ll_hash_search(&group_name_hash,
                                                                  q->hosts_spec);
        if (g != NULL) {
            char tmp[LL_BUFSIZ_1K];
            ll_strlcpy(tmp, g->members, sizeof(tmp));

            char *tok = strtok(tmp, " \t");
            while (tok != NULL) {
                struct mbd_host *h = find_host_by_name(tok);
                if (h == NULL) {
                    LS_ERRX("queue=%s group=%s host=%s not found",
                            q->name, g->name, tok);
                    return -1;
                }
                enum ll_hash_status st = ll_hash_insert(&q->host_hash,
                                                        h->net.name, h, 0);
                if (st == LL_HASH_EXISTS)
                    LS_WARNING("queue=%s host=%s already in host_hash",
                               q->name, h->net.name);
                tok = strtok(NULL, " \t");
            }
            continue;
        }

        /* hosts_spec is a single hostname */
        struct mbd_host *h = find_host_by_name(q->hosts_spec);
        if (h == NULL) {
            LS_ERRX("queue=%s HOSTS=%s not a group or host",
                    q->name, q->hosts_spec);
            return -1;
        }
        ll_hash_insert(&q->host_hash, h->net.name, h, 0);
    }

    return 0;
}

static void dump_config(void)
{
    struct ll_list_entry *e;
    struct ll_list_entry *ge;

    LS_DEBUG("--- hosts ---");
    for (e = host_list.head; e; e = e->next) {
        struct mbd_host *h = (struct mbd_host *)e;
        LS_DEBUG("host name=%s addr=%s port=%d cpu=%d mem=%luMB storage=%luMB gpu=%d",
                 h->net.name, h->net.addr, h->port ? h->port : 0,
                 h->res.total_cpu,
                 h->res.total_mem_mb,
                 h->res.total_storage_mb,
                 h->res.total_gpu);
        for (ge = h->res.gpu_list.head; ge; ge = ge->next) {
            struct mbd_gpu *g = (struct mbd_gpu *)ge;
            LS_DEBUG("  gpu id=%d model=%s type=%s count=%d",
                     g->gpu_id, g->model, g->gpu_type, g->count);
        }
    }

    LS_DEBUG("--- groups ---");
    for (e = group_list.head; e; e = e->next) {
        struct mbd_group *g = (struct mbd_group *)e;
        LS_DEBUG("group name=%s members=%s num=%d",
                 g->name, g->members, g->num_members);
    }

    LS_DEBUG("--- queues ---");
    for (e = queue_list.head; e; e = e->next) {
        struct mbd_queue *q = (struct mbd_queue *)e;
        LS_DEBUG("queue name=%s priority=%d hosts_spec=%s users=%s desc=%s",
                 q->name, q->priority, q->hosts_spec,
                 q->users[0] ? q->users : "*",
                 q->description);
    }

    LS_DEBUG("--- token pools ---");
    for (e = token_pool_list.head; e; e = e->next) {
        struct mbd_token_pool *tp = (struct mbd_token_pool *)e;
        LS_DEBUG("tokenpool name=%s total=%d", tp->name, tp->total);
    }
}

static int check_ll_config(void)
{
    if (ll_conf_param_missing("LL_CONF_DIR", ll_params[LL_CONF_DIR].val)) {
        LS_ERRX("LL_CONF_DIR missing from ll.conf");
        return -1;
    }
    if (ll_conf_param_missing("LL_STATE_DIR", ll_params[LL_STATE_DIR].val)) {
        LS_ERRX("LL_STATE_DIR missing from ll.conf");
        return -1;
    }
    if (ll_conf_param_missing("LL_MBD_PORT", ll_params[LL_MBD_PORT].val)) {
        LS_ERRX("LL_MBD_PORT missing from ll.conf");
        return -1;
    }
    if (ll_conf_param_missing("LL_MBD_HOST", ll_params[LL_MBD_HOST].val)) {
        LS_ERRX("LL_MBD_HOST missing from ll.conf");
        return -1;
    }
    if (ll_conf_param_missing("LL_MBD_USER", ll_params[LL_MBD_USER].val)) {
        LS_ERRX("LL_MBD_USER missing from ll.conf");
        return -1;
    }
    if (ll_conf_param_missing("LL_DEFAULT_QUEUE",
                              ll_params[LL_DEFAULT_QUEUE].val)) {
        LS_ERRX("LL_DEFAULT_QUEUE missing from ll.conf");
        return -1;
    }

    return 0;
}

int conf_init(void)
{
    char path[PATH_MAX];
    int n;

    if (ll_init() < 0) {
        LS_ERRX("ll_init failed");
        return -1;
    }

    if (check_ll_config() < 0) {
        LS_ERRX("check_ll_config failed");
        return -1;
    }

    n = snprintf(path, sizeof(path), "%s/llb.hosts",
                 ll_params[LL_CONF_DIR].val);
    if (n < 0 || n >= (int)sizeof(path))
        return -1;

    if (parse_hosts(path) < 0) {
        LS_ERRX("parse_hosts failed path=%s", path);
        return -1;
    }

    if (parse_gpus(path) < 0) {
        LS_ERRX("parse_gpus failed path=%s", path);
        return -1;
    }

    if (parse_token_pools(path) < 0) {
        LS_ERRX("parse_token_pools failed path=%s", path);
        return -1;
    }

    if (parse_sim(path) < 0) {
        LS_ERRX("parse_sim failed path=%s", path);
        return -1;
    }

    if (parse_groups(path) < 0) {
        LS_ERRX("parse_groups failed path=%s", path);
        return -1;
    }

    n = snprintf(path, sizeof(path), "%s/llb.queues",
                 ll_params[LL_CONF_DIR].val);
    if (n < 0 || n >= (int)sizeof(path))
        return -1;

    if (parse_queues(path) < 0) {
        LS_ERRX("parse_queues failed path=%s", path);
        return -1;
    }

    if (conf_expand_queues() < 0) {
        LS_ERRX("conf_expand_queues failed");
        return -1;
    }

    if (init_manager() < 0) {
        LS_ERR("init_manager failed");
        return -1;
    }

    if (! ll_hash_contains(&queue_name_hash, ll_params[LL_DEFAULT_QUEUE].val)) {
        LS_ERRX("LL_DEFAULT_QUEUE=%s not in queue configuration",
                ll_params[LL_DEFAULT_QUEUE].val);
        return -1;
    }

    dump_config();

    return 0;
}

int init_manager(void)
{
    mbd_mgr.uid = getuid();
    mbd_mgr.gid = getgid();

    struct passwd *pw = getpwuid2(mbd_mgr.uid);
    if (!pw || !pw->pw_name) {
        LS_ERR("getpwuid2(%d) failed", mbd_mgr.uid);
        return -1;
    }

    // check that mbd is LL_MBD_USER=lavalite
    LS_INFO("uid=%d gid=%d name=%s", mbd_mgr.uid, mbd_mgr.gid, pw->pw_name);

    return 0;
}

int is_manager(uid_t uid)
{
    if (uid == mbd_mgr.uid)
        return 1;
    return 0;
}
