// Copyright (C) LavaLite Contributors
// GPL v2

#include "batch/mbd/mbd.h"

static uint64_t parse_mem(const char *s)
{
    char *end;
    uint64_t v = strtoull(s, &end, 10);
    if (*end == 'G' || *end == 'g') {
        return v * 1024;
    }
    if (*end == 'T' || *end == 't') {
        return v * 1024 * 1024;
    }
    return v;
}

static struct mbd_host *make_mbd_host(const char *p)
{
    struct mbd_host *h = calloc(1, sizeof(*h));
    if (h == NULL) {
        LS_ERR("calloc failed");
        return NULL;
    }

    char mem_str[LL_BUFSIZ_32];
    char hostname[LL_BUFSIZ_64];
    int n = sscanf(p, "%63s %d %d %d %31s",
                   hostname,
                   &h->max_jobs,
                   &h->total_cpu,
                   &h->total_gpu,
                   mem_str);
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

    h->total_mem_mb = parse_mem(mem_str);
    h->sbd_chan = -1;
    h->status = HOST_UNAVAIL;

    return h;
}

static int parse_hosts(const char *path)
{
    FILE *f = fopen(path, "r");
    if (f == NULL) {
        LS_ERR("fopen=%s failed", path);
        return -1;
    }

    char line[LL_BUFSIZ_1K];
    int in_section = 0;
    int header_skipped = 0;

    while (fgets(line, sizeof(line), f) != NULL) {
        char *p = ltrim(line);
        rtrim(p);

        if (*p == 0 || *p == '#')
            continue;

        char *section = ll_conf_parse_begin(p);
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

        struct mbd_host *h = make_mbd_host(p);
        if (h == NULL) {
            LS_ERRX("failed to make host from line=%s", p);
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

static struct mbd_group *make_mbd_group(char *p)
{
    char *open  = strchr(p, '(');
    char *close = strchr(p, ')');
    if (open == NULL || close == NULL || close <= open) {
        LS_ERRX("bad line=%s", p);
        return NULL;
    }

    *open  = 0;
    *close = 0;
    char *name    = ltrim(p);
    char *members = ltrim(open + 1);
    rtrim(name);
    rtrim(members);

    struct mbd_group *g = calloc(1, sizeof(*g));
    if (g == NULL) {
        LS_ERR("calloc failed");
        return NULL;
    }

    ll_strlcpy(g->name,    name,    sizeof(g->name));
    ll_strlcpy(g->members, members, sizeof(g->members));

    char tmp[LL_BUFSIZ_1K];
    ll_strlcpy(tmp, members, sizeof(tmp));
    char *tok = strtok(tmp, " \t");
    while (tok != NULL) {
        g->num_members++;
        tok = strtok(NULL, " \t");
    }

    return g;
}

static int parse_groups(const char *path)
{
    FILE *f = fopen(path, "r");
    if (f == NULL) {
        LS_ERR("fopen=%s failed", path);
        return -1;
    }

    char line[LL_BUFSIZ_1K];
    int in_section = 0;
    int header_skipped = 0;

    while (fgets(line, sizeof(line), f) != NULL) {
        char *p = ltrim(line);
        rtrim(p);

        if (*p == 0 || *p == '#')
            continue;

        char *section = ll_conf_parse_begin(p);
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

        struct mbd_group *g = make_mbd_group(p);
        if (g == NULL) {
            LS_ERRX("make_mbd_group failed line=%s", p);
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

static struct mbd_queue *make_mbd_queue(const char *name, const char *desc,
                                        const char *hosts_str, int priority)
{
    struct mbd_queue *q = calloc(1, sizeof(*q));
    if (q == NULL) {
        LS_ERR("calloc failed");
        return NULL;
    }

    ll_strlcpy(q->name, name, LL_BUFSIZ_64);
    ll_strlcpy(q->description, desc, LL_BUFSIZ_256);
    ll_strlcpy(q->hosts, hosts_str, LL_BUFSIZ_256);
    q->priority = priority;
    q->status   = QUEUE_OPEN;

    return q;
}

static int commit_queue(struct queue_conf *qc)
{
    struct mbd_queue *q = calloc(1, sizeof(struct mbd_queue));
    if (q == NULL) {
        LS_ERRX("calloc failed name=%s", qc->name);
        return -1;
    }

    strcpy(q->name, qc->name);
    strcpy(q->description, qc->desc);
    strcpy(q->hosts, qc->hosts);
    q->priority = qc->priority;

    ll_list_append(&queue_list, &q->ent);
    ll_hash_insert(&queue_name_hash, q->name, q, 0);

    return 0;
}

static int parse_queues(const char *path)
{
    FILE *f = fopen(path, "r");
    if (f == NULL) {
        LS_ERR("fopen=%s failed", path);
        return -1;
    }

    struct queue_conf qc;
    memset(&qc, 0, sizeof(struct queue_conf));
    char line[LL_BUFSIZ_1K];
    int in_section = 0;

    while (fgets(line, sizeof(line), f) != NULL) {
        char *p = ltrim(line);
        rtrim(p);
        if (*p == 0 || *p == '#')
            continue;
        char *section = ll_conf_parse_begin(p);
        if (section != NULL) {
            if (strncmp(section, "Queue", 5) == 0)
                in_section = 1;
            continue;
        }

        if (!in_section)
            continue;

        char *eq = strchr(p, '=');
        if (eq != NULL) {
            *eq = 0;
            char *key = p;
            char *val = ltrim(eq + 1);
            rtrim(key);
            rtrim(val);
            if (strcmp(key, "QUEUE_NAME") == 0)
                ll_strlcpy(qc.name, val, sizeof(qc.name));
            else if (strcmp(key, "PRIORITY") == 0)
                ll_atoi(val, &qc.priority);
            else if (strcmp(key, "DESCRIPTION") == 0)
                ll_strlcpy(qc.desc, val, sizeof(qc.desc));
            else if (strcmp(key, "HOSTS") == 0)
                ll_strlcpy(qc.hosts, val, sizeof(qc.hosts));
            continue;
        }

        if (ll_conf_parse_end(p)) {
            commit_queue(&qc);
            in_section = 0;
            memset(&qc, 0, sizeof(struct queue_conf));
            continue;
        }
        LS_ERRX("parse_queues: bad line: %s", p);
        fclose(f);
        return -1;
    }
    fclose(f);
    return 0;
}

static void dump_config(void)
{
    LS_DEBUG("--- queues ---");
    struct ll_list_entry *e;
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
    for (e = host_list.head; e; e = e->next) {
        struct mbd_group *g = (struct mbd_group *)e;
        LS_DEBUG("group name=%s members=%s num=%d", g->name, g->members,
               g->num_members);
    }
}


int conf_init()
{
    if (ll_init() < 0) {
        LS_ERRX("conf_init: ll_init failed");
        return -1;
    }

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

    char path[PATH_MAX];
    int n = snprintf(path, sizeof(path), "%s/llb.hosts",
                     ll_params[LL_CONF_DIR].val);
    if (n < 0 || n >= (int)sizeof(path)) {
        return -1;
    }

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
    if (n < 0 || n >= (int)sizeof(path)) {
        return -1;
    }

    if (parse_queues(path) < 0) {
        LS_ERRX("parse_queues failed path=%s", path);
        return -1;
    }

    return 0;
}

struct mbd_manager *mbd_init_manager(void)
{
    mbd_mgr = calloc(1, sizeof(struct mbd_manager));
    mbd_mgr->uid = getuid();
    mbd_mgr->gid = getgid();

    struct passwd *pw = getpwuid2(mbd_mgr->uid);
    if (!pw || !pw->pw_name) {
        LS_ERR("getpwuid2(%d) failed", mbd_mgr->uid);
        free(mbd_mgr);
        return NULL;
    }

    // check that mbd is LL_MBD_USER=lavalite
    LS_INFO("uid=%d gid=%d name=%s", mbd_mgr->uid, mbd_mgr->gid, pw->pw_name);

    return mbd_mgr;
}
