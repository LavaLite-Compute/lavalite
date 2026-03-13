/* Copyright (C) LavaLite Contributors
 * GPL v2
 */

#include "base/lim/lim.h"

struct ll_kv lim_params[LSF_PARAM_COUNT] = {
    [LSF_CONFDIR] = {"LSF_CONFDIR", NULL},
    [LSF_SERVERDIR] = {"LSF_SERVERDIR", NULL},
    [LSF_LOGDIR] = {"LSF_LOGDIR", NULL},
    [LSF_LIM_DEBUG] = {"LSF_LIM_DEBUG", NULL},
    [LSF_LIM_PORT] = {"LSF_LIM_PORT", NULL},
    [LSF_LOG_MASK] = {"LSF_LOG_MASK", NULL},
    [LSF_DEBUG_LIM] = {"LSF_DEBUG_LIM", NULL},
    [LSF_TIME_LIM] = {"LSF_TIME_LIM", NULL},
    [LSF_API_CONNTIMEOUT] = {"LSF_API_CONNTIMEOUT", NULL},
    [LSF_API_RECVTIMEOUT] = {"LSF_API_RECVTIMEOUT", NULL},
};

static int parse_cluster_section(FILE *f)
{
    char line[LL_BUFSIZ_1K];

    while (fgets(line, sizeof(line), f) != NULL) {

        rtrim(line);
        char *p = ltrim(line);

        if (*p == 0 || *p == '#')
            continue;

        if (ll_conf_parse_end(p))
            return 0;

        if (strcmp(p, "ClusterName") == 0)
            continue;

        char cluster[LL_BUFSIZ_64];
        int n = sscanf(p, "%64s", cluster);
        if (n < 1) {
            LS_ERR("sscanf Cluster section failed");
            continue;
        }

        lim_cluster.name = strdup(p);
    }

    return 0;
}

static int parse_admins_section(FILE *f)
{
    char line[LL_BUFSIZ_1K];

    while (fgets(line, sizeof(line), f) != NULL) {

        rtrim(line);
        char *p = ltrim(line);

        if (*p == 0 || *p == '#')
            continue;

        if (ll_conf_parse_end(p))
            return 0;

        if (strcasecmp(p, "Administrators") == 0)
            continue;

        // We support only one for now, verify lim is
        // running as the admin
        lim_cluster.admin = strdup(p);
        break;
    }
    LS_ERR("ClusterAdmin section: unexpected EOF");
    return -1;
}
static struct lim_node *node_make(const char *hostname)
{
    struct lim_node *n = calloc(1, sizeof(struct lim_node));
    if (n == NULL)
        return NULL;

    struct ll_host *h = calloc(1, sizeof(struct ll_host));
    if (h == NULL) {
        free(n);
        return NULL;
    }

    if (get_host_by_name(hostname, h) < 0) {
        LS_WARNING("cannot resolve %s", hostname);
        strncpy(h->name, hostname, MAXHOSTNAMELEN - 1);
        h->name[MAXHOSTNAMELEN - 1] = 0;
    }

    n->host = h;
    n->host_state = LIM_STAT_OK;
    struct utsname u;
    if (uname(&u) < 0) {
        LS_ERR("uname failed");
        n->machine = strdup("unknown");
    } else {
        n->machine = strdup(u.machine);
    }

    char buf[MAXHOSTNAMELEN];
    gethostname(buf, sizeof(buf));
    if (strcmp(buf, n->host->name) == 0) {
        me = n;
    }
    return n;
}
static char *parse_resources(const char *line)
{
    const char *op = strchr(line, '(');
    if (op == NULL)
        return NULL;

    op++;

    const char *cl = strchr(op, ')');
    if (cl == NULL)
        return NULL;

    return strndup(op, cl - op);
}
static int parse_host_section(FILE *f)
{
    char line[LL_BUFSIZ_1K];

    n_master_candidates = 0;
    while (fgets(line, sizeof(line), f) != NULL) {
        rtrim(line);
        char *p = ltrim(line);
        if (*p == 0 || *p == '#')
            continue;
        if (ll_conf_parse_end(p))
            return 0;

        char hostname[MAXHOSTNAMELEN];
        char resources[LL_BUFSIZ_64 * 4];
        char master[4];
        int n = sscanf(p, "%255s %255s %3s", hostname, resources, master);
        if (n != 3) {
            LS_ERR("Host section: expected 'hostname resources master' got='%s'",
                   p);
            continue;
        }

        if (strcasecmp(hostname, "Hostname") == 0)
            continue;

        if (strcasecmp(master, "Y") != 0 && strcmp(master, "-") != 0) {
            LS_ERR("Host section: invalid master='%s' host=%s", master, hostname);
            return -1;
        }

        if (ll_hash_search(&node_name_hash, hostname)) {
            LS_ERRX("Host section: duplicate host=%s skipped", hostname);
            continue;
        }
        struct lim_node *node = node_make(hostname);
        if (node == NULL) {
            LS_ERR("Host section: cannot create node=%s", hostname);
            return -1;
        }

        node->resources = parse_resources(resources);

        node->is_candidate = 0;
        if (strcasecmp(master, "Y") == 0) {
            node->is_candidate = 1;
            ++n_master_candidates;
        }
        ll_list_append(&node_list, &node->list);
        ll_hash_insert(&node_name_hash, hostname, node, 0);
        ll_hash_insert(&node_addr_hash, node->host->addr, node, 0);
    }
    LS_ERR("Host section: unexpected EOF");
    return -1;
}

static void set_host_no(void)
{
    struct ll_list_entry *e;

    int num = 0;
    for (e = node_list.head; e; e = e->next) {
        struct lim_node *n = (struct lim_node *)e;
        n->host_no = num;
        LS_DEBUG("host=%s machine=%s master=%d host_no=%d resources='%s'",
                 n->host->name, n->machine, n->is_candidate,
                 n->host_no, n->resources);
        ++num;
    }

}

static int make_master_candidates(void)
{
    master_candidates = calloc(n_master_candidates, sizeof(struct lim_node *));
    if (master_candidates == NULL) {
        LS_ERR("calloc failed");
        return -1;
    }
    struct ll_list_entry *e;
    int i = 0;
    for (e = node_list.head; e; e = e->next) {
        struct lim_node *n = (struct lim_node *)e;
        if (! n->is_candidate)
            continue;
        master_candidates[i] = n;
        ++i;
    }
    assert(i == n_master_candidates);

    return 0;
}

int load_conf(const char *path)
{
    int nitems = sizeof(lim_params) / sizeof(lim_params[0]);

    if (ll_conf_load(lim_params, nitems, path) < 0)
        return -1;

    char *p = "LSF_CONFDIR";
    if (ll_conf_param_missing(p, lim_params[LSF_CONFDIR].val)) {
        LS_ERR("missing mandatory parameter %s", p);
        return -1;
    }

    p = "LSF_SERVERDIR";
    if (ll_conf_param_missing(p, lim_params[LSF_SERVERDIR].val)) {
        LS_ERR("missing mandatory parameter %s", p);
        return -1;
    }

    p = "LSF_LOGDIR";
    if (ll_conf_param_missing(p, lim_params[LSF_LOGDIR].val)) {
        LS_ERR("missing mandatory parameter %s", p);
        return -1;
    }

    p = "LSF_LIM_PORT";
    if (ll_conf_param_missing(p, lim_params[LSF_LIM_PORT].val)) {
        LS_ERR("missing mandatory parameter %s", p);
        return -1;
    }
    return 0;
}

int lim_make_cluster(const char *path)
{
    char line[LL_BUFSIZ_1K];

    FILE *f = fopen(path, "r");
    if (f == NULL) {
        LS_ERR("cannot open %s", path);
        return -1;
    }

    while (fgets(line, sizeof(line), f) != NULL) {

        rtrim(line);
        char *p = ltrim(line);

        if (*p == 0 || *p == '#')
            continue;

        char *kw = ll_conf_parse_begin(p);
        if (kw == NULL)
            continue;

        rtrim(kw);

        int rc = 0;
        if (strcmp(kw, "Cluster") == 0)
            rc = parse_cluster_section(f);
        else if (strcmp(kw, "ClusterAdmin") == 0)
            rc = parse_admins_section(f);
        else if (strcmp(kw, "Host") == 0)
            rc = parse_host_section(f);
        else {
            LS_WARNING("unknown section %s", kw);
            fclose(f);
            return -1;
        }
        if (rc < 0) {
            LS_ERRX("parsing cluster file failed");
            fclose(f);
            return -1;
        }
    }

    fclose(f);

    if (lim_cluster.name == NULL) {
        LS_ERRX("missing ClusterName in %s", path);
        return -1;
    }

    if (lim_cluster.admin == NULL) {
        LS_ERRX("missing Administrators in %s", path);
        return -1;
    }

    if (ll_list_is_empty(&node_list)) {
        LS_ERRX("no hosts defined in %s", path);
        return -1;
    }

    LS_INFO("cluster=%s admin=%s hosts=%d",
            lim_cluster.name,
            lim_cluster.admin,
            ll_list_count(&node_list));

    set_host_no();
    make_master_candidates();

    return 0;
}
