/* Copyright (C) LavaLite Contributors
 * GPL v2
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "llbatch.h"

static int
imax(int a, int b)
{
    return a > b ? a : b;
}

struct col_widths {
    int name;
    int hosts;
};

static int
hosts_width(struct hgroup_info *g)
{
    int w = 0;
    for (int i = 0; i < g->nhosts; i++) {
        if (i > 0)
            w++;                 /* space separator */
        w += strlen(g->hosts[i]);
    }
    return w;
}

static void
compute_widths(struct hgroup_info *g, int n, struct col_widths *w)
{
    w->name  = strlen("GROUP_NAME");
    w->hosts = strlen("HOSTS");

    for (int i = 0; i < n; i++) {
        w->name  = imax(w->name,  strlen(g[i].name));
        w->hosts = imax(w->hosts, hosts_width(&g[i]));
    }
}

static void
print_hosts(struct hgroup_info *g, int width)
{
    int written = 0;
    for (int i = 0; i < g->nhosts; i++) {
        if (i > 0) {
            putchar(' ');
            written++;
        }
        fputs(g->hosts[i], stdout);
        written += strlen(g->hosts[i]);
    }
    /* pad to column width */
    while (written < width) {
        putchar(' ');
        written++;
    }
}

int
main(void)
{
    int ngroups;
    struct hgroup_info *groups;
    struct col_widths w;

    groups = llb_hgroup_info(&ngroups);
    if (!groups) {
        fprintf(stderr, "bhgroups: failed\n");
        return -1;
    }

    compute_widths(groups, ngroups, &w);

    printf("%-*s  %-*s\n",
           w.name,  "GROUP_NAME",
           w.hosts, "HOSTS");

    for (int i = 0; i < ngroups; i++) {
        printf("%-*s  ", w.name, groups[i].name);
        print_hosts(&groups[i], w.hosts);
        putchar('\n');
    }

    llb_free_hgroup_info(groups, ngroups);
    return 0;
}
