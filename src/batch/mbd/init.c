// Copyright (C) LavaLite Contributors
// GPL v2

static int parse_hosts(const char *path)
{
    FILE *f;
    char line[LL_BUFSIZ_1K];
    char *p;
    int in_section = 0;
    int header_skipped = 0;

    f = fopen(path, "r");
    if (f == NULL) {
        syslog(LOG_ERR, "parse_hosts: fopen(%s) failed: %m", path);
        return -1;
    }

    while (fgets(line, sizeof(line), f) != NULL) {
        rtrim(line);
        p = ltrim(line);

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

        if (ll_conf_parse_end(p) != NULL) {
            in_section = 0;
            continue;
        }

        if (!in_section)
            continue;

        if (!header_skipped) {
            header_skipped = 1;
            continue;
        }

        struct mbd_host h;
        char mem_str[32];
        memset(&h, 0, sizeof(h));

        int n = sscanf(p, "%63s %d %d %d %31s",
                       h.net.name,
                       &h.max_jobs,
                       &h.total_cpu,
                       &h.total_gpu,
                       mem_str);
        if (n != 5) {
            syslog(LOG_WARNING, "parse_hosts: bad line: %s", p);
            continue;
        }

        h.total_mem_mb = parse_mem(mem_str);
        h.sbd_chan = -1;
        h.status = HOST_CLOSED;

        if (ll_list_append(&host_list, &h) < 0) {
            fclose(f);
            return -1;
        }
    }

    fclose(f);
    return 0;
}

static int parse_groups(const char *path)
{
    FILE *f;
    char line[LL_BUFSIZ_1K];
    char *p;
    int in_section = 0;
    int header_skipped = 0;

    f = fopen(path, "r");
    if (f == NULL) {
        syslog(LOG_ERR, "parse_groups: fopen(%s) failed: %m", path);
        return -1;
    }

    while (fgets(line, sizeof(line), f) != NULL) {
        rtrim(line);
        p = ltrim(line);

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

        if (ll_conf_parse_end(p) != NULL) {
            in_section = 0;
            continue;
        }

        if (!in_section)
            continue;

        if (!header_skipped) {
            header_skipped = 1;
            continue;
        }

        char *open  = strchr(p, '(');
        char *close = strchr(p, ')');
        if (open == NULL || close == NULL || close <= open) {
            syslog(LOG_WARNING, "parse_groups: bad line: %s", p);
            continue;
        }

        *open  = 0;
        *close = 0;
        char *name    = ltrim(p);
        char *members = ltrim(open + 1);
        rtrim(name);
        rtrim(members);

        struct mbd_group g;
        memset(&g, 0, sizeof(g));

        strncpy(g.name,    name,    sizeof(g.name)    - 1);
        strncpy(g.members, members, sizeof(g.members) - 1);

        char tmp[LL_BUFSIZ_1K];
        strncpy(tmp, members, sizeof(tmp) - 1);
        char *tok = strtok(tmp, " \t");
        while (tok != NULL) {
            g.num_members++;
            tok = strtok(NULL, " \t");
        }

        if (ll_list_append(&group_list, &g) < 0) {
            fclose(f);
            return -1;
        }
    }

    fclose(f);
    return 0;
}

static uint64_t parse_mem(const char *s)
{
    char *end;
    uint64_t v = strtoull(s, &end, 10);

    if (*end == 'G' || *end == 'g')
        return v * 1024;
    if (*end == 'T' || *end == 't')
        return v * 1024 * 1024;
    return v;
}

int conf_init(void)
{
    if (ll_init() < 0) {
        syslog(LOG_ERR, "conf_init: ll_init failed");
        return -1;
    }

    char path[PATH_MAX];
    int n = snprintf(path, sizeof(path), "%s/llb.hosts",
                     ll_params[LL_CONFDIR].val);
    if (n < 0 || n >= (int)sizeof(path))
        return -1;

    if (parse_hosts(path) < 0) {
        syslog(LOG_ERR, "conf_init: parse_hosts failed");
        return -1;
    }

    if (parse_groups(path) < 0) {
        syslog(LOG_ERR, "conf_init: parse_groups failed");
        return -1;
    }

    return 0;
}
