/* Copyright (C) LavaLite Contributors
 * GPL v2
 */

#include <sys/utsname.h>
#include <pwd.h>

#include "base/lim/lim.h"

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
            LL_ERR("sscanf Cluster section failed");
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
    }
    LL_ERR("ClusterAdmin section: unexpected EOF");
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
        LL_WARNING("cannot resolve %s", hostname);
        strncpy(h->name, hostname, MAXHOSTNAMELEN - 1);
        h->name[MAXHOSTNAMELEN - 1] = 0;
    }

    n->host = h;
    n->status = LIM_STAT_CLOSED;
    struct utsname u;
    if (uname(&u) < 0) {
        LL_ERR("uname failed");
        n->machine = strdup("unknown");
    } else {
        n->machine = strdup(u.machine);
    }

    char buf[MAXHOSTNAMELEN];
    gethostname(buf, sizeof(buf));
    if (strcmp(buf, n->host->name) == 0) {
        me = n;
        n->status = LIM_STAT_OK;
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

    /* skip header line: "Hostname Resources Master" */
    while (fgets(line, sizeof(line), f) != NULL) {
        rtrim(line);
        char *p = ltrim(line);
        if (*p == '\0' || *p == '#')
            continue;
        if (strncasecmp(p, "Hostname", 8) == 0)
            break;
        LL_ERR("Host section: expected header 'Hostname Resources Master' "
               "got='%s'",
               p);
        return -1;
    }

    while (fgets(line, sizeof(line), f) != NULL) {
        rtrim(line);
        char *p = ltrim(line);

        if (*p == 0 || *p == '#')
            continue;
        if (ll_conf_parse_end(p))
            return 0;

        /* hostname */
        char *hostname = p;
        char *q = strpbrk(p, " \t");
        if (q == NULL) {
            LL_ERR("Host section: bad format got='%s'", p);
            continue;
        }
        *q = 0;
        p = ltrim(q + 1);

        /* resources: must have ( ... ) */
        char *op = strchr(p, '(');
        char *cl = strchr(p, ')');
        if (op == NULL || cl == NULL || cl < op) {
            LL_ERR("Host section: missing () for host=%s got='%s'", hostname,
                   p);
            continue;
        }
        p = ltrim(cl + 1);

        /* master flag */
        char *master = p;
        q = strpbrk(p, " \t\r\n");
        if (q != NULL)
            *q = 0;

        if (*master == 0) {
            LL_ERR("Host section: missing master flag for host=%s", hostname);
            return -1;
        }

        if (strcasecmp(master, "Y") != 0 && strcmp(master, "-") != 0) {
            LL_ERR("Host section: invalid master='%s' host=%s", master,
                   hostname);
            return -1;
        }

        if (ll_hash_search(&node_name_hash, hostname)) {
            LL_ERRX("Host section: duplicate host=%s skipped", hostname);
            continue;
        }

        struct lim_node *node = node_make(hostname);
        if (node == NULL) {
            LL_ERR("Host section: cannot create node=%s", hostname);
            return -1;
        }

        node->resources = parse_resources(op);
        node->is_candidate = 0;
        if (strcasecmp(master, "Y") == 0) {
            node->is_candidate = 1;
            ++n_master_candidates;
        }

        ll_list_append(&node_list, &node->list);
        ll_hash_insert(&node_name_hash, hostname, node, 0);
        ll_hash_insert(&node_addr_hash, node->host->addr, node, 0);
    }

    LL_ERR("Host section: unexpected EOF");
    return -1;
}

static void set_host_no(void)
{
    struct ll_list_entry *e;

    int num = 0;
    for (e = node_list.head; e; e = e->next) {
        struct lim_node *n = (struct lim_node *) e;
        n->host_no = num;
        LL_DEBUG("host=%s machine=%s master=%d host_no=%d resources='%s'",
                 n->host->name, n->machine, n->is_candidate, n->host_no,
                 n->resources);
        ++num;
    }
}

static int make_master_candidates(void)
{
    master_candidates = calloc(n_master_candidates, sizeof(struct lim_node *));
    if (master_candidates == NULL) {
        LL_ERR("calloc failed");
        return -1;
    }
    struct ll_list_entry *e;
    int i = 0;
    for (e = node_list.head; e; e = e->next) {
        struct lim_node *n = (struct lim_node *) e;
        if (!n->is_candidate)
            continue;
        master_candidates[i] = n;
        ++i;
    }
    assert(i == n_master_candidates);

    return 0;
}

int load_conf(const char *path)
{
    if (ll_conf_load(ll_params, PARAMS_COUNT, path) < 0)
        return -1;

    if (ll_conf_param_missing("LL_LOG_DIR", ll_params[LL_LOG_DIR].val)) {
        LL_ERR("missing mandatory parameter LL_LOG_DIR");
        return -1;
    }

    if (ll_conf_param_missing("LL_LIM_PORT", ll_params[LL_LIM_PORT].val)) {
        LL_ERR("missing mandatory parameter LL_LIM_PORT");
        return -1;
    }
    return 0;
}

static int is_admin(void)
{
    struct passwd *pw = getpwuid(getuid());
    if (pw == NULL)
        return 0;

    if (strcmp(pw->pw_name, lim_cluster.admin) == 0)
        return 1;

    LL_ERRX("the user=%s is not lavalite admin=%s", pw->pw_name,
            lim_cluster.admin);

    return 0;
}

int make_cluster(const char *path)
{
    char line[LL_BUFSIZ_1K];

    FILE *f = fopen(path, "r");
    if (f == NULL) {
        LL_ERR("cannot open %s", path);
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
            LL_WARNING("unknown section %s", kw);
            fclose(f);
            return -1;
        }
        if (rc < 0) {
            LL_ERRX("parsing cluster file failed");
            fclose(f);
            return -1;
        }
    }

    fclose(f);

    if (me == NULL) {
        LL_ERRX("this host is not in the cluster configuration");
        return -1;
    }

    if (lim_cluster.name == NULL) {
        LL_ERRX("missing ClusterName in %s", path);
        return -1;
    }

    if (lim_cluster.admin == NULL) {
        LL_ERRX("missing Administrators in %s", path);
        return -1;
    }

    if (ll_list_is_empty(&node_list)) {
        LL_ERRX("no hosts defined in %s", path);
        return -1;
    }

    if (!is_admin()) {
        LL_ERRX("user is not admin");
        return -1;
    }

    LL_INFO("cluster=%s admin=%s hosts=%d", lim_cluster.name, lim_cluster.admin,
            ll_list_count(&node_list));

    set_host_no();
    make_master_candidates();

    return 0;
}
