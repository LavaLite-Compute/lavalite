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

static void parse_gpu_type(const char *s, char *out, size_t outlen)
{
    if (strcmp(s, "-") == 0) {
        out[0] = 0;
        return;
    }
    ll_strlcpy(out, s, outlen);
}

static struct mbd_host *make_host(const char *p)
{
    struct mbd_host *h;
    char mem_str[LL_BUFSIZ_32];
    char hostname[LL_BUFSIZ_64];
    char gpu_type_str[LL_BUFSIZ_64];
    int n;

    h = calloc(1, sizeof(*h));
    if (h == NULL) {
        LS_ERR("calloc failed");
        return NULL;
    }
    n = sscanf(p, "%63s %d %d %d %31s %63s",
               hostname,
               &h->max_jobs,
               &h->total_cpu,
               &h->total_gpu,
               mem_str,
               gpu_type_str);
    if (n != 6) {
        LS_ERRX("bad line: %s", p);
        free(h);
        return NULL;
    }
    if (get_host_by_name(hostname, &h->net) < 0) {
        LS_ERR("get_host_by_name failed host=%s", hostname);
        free(h);
        return NULL;
    }
    h->total_mem_mb = parse_mem(mem_str);
    if (h->total_mem_mb == 0) {
        LS_ERRX("bad memory value host=%s mem=%s", hostname, mem_str);
        free(h);
        return NULL;
    }
    parse_gpu_type(gpu_type_str, h->gpu_type, sizeof(h->gpu_type));
    if (h->total_gpu > 0 && h->gpu_type[0] == '\0') {
        LS_ERRX("gpu_type required when GPU > 0: %s", p);
        free(h);
        return NULL;
    }
    h->sbd_chan = -1;
    h->status = HOST_UNAVAIL;
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
    LS_ERRX("missing End HostGroup in %s", path);
    return -1;
}

static int commit_queue(struct queue_conf *qc)
{
    struct mbd_queue *q;

    if (qc->name[0] == 0) {
        LS_ERRX("queue missing NAME");
        return -1;
    }

    if (qc->hosts[0] == 0) {
        LS_ERRX("queue=%s missing HOSTS", qc->name);
        return -1;
    }

    q = calloc(1, sizeof(struct mbd_queue));
    if (q == NULL) {
        LS_ERRX("calloc failed name=%s", qc->name);
        return -1;
    }

    ll_strlcpy(q->name, qc->name, LL_BUFSIZ_64);
    ll_strlcpy(q->description, qc->desc, LL_BUFSIZ_256);

    ll_strlcpy(q->hosts, qc->hosts, LL_BUFSIZ_256);

    q->priority = qc->priority;
    q->status = QUEUE_OPEN;

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
        return ll_strlcpy(qc->hosts, val, LL_BUFSIZ_256);

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

static void dump_config(void)
{
    struct ll_list_entry *e;

    LS_DEBUG("--- queues ---");
    for (e = queue_list.head; e; e = e->next) {
        struct mbd_queue *q = (struct mbd_queue *)e;
        LS_DEBUG("queue name=%s priority=%d hosts=%s desc=%s",
                 q->name, q->priority, q->hosts, q->description);
    }

    LS_DEBUG("--- hosts ---");
    for (e = host_list.head; e; e = e->next) {
        struct mbd_host *h = (struct mbd_host *)e;
        LS_DEBUG("host name=%s addr=%s", h->net.name, h->net.addr);
    }

    LS_DEBUG("--- groups ---");
    for (e = group_list.head; e; e = e->next) {
        struct mbd_group *g = (struct mbd_group *)e;
        LS_DEBUG("group name=%s members=%s num=%d",
                 g->name, g->members, g->num_members);
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
