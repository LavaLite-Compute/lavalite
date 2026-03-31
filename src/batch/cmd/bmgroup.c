/* Copyright (C) LavaLite Contributors
 * GPL v2
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <getopt.h>
#include "llbatch.h"

static const char *prog;

static void usage(void)
{
    fprintf(stderr, "bmgroup: %s [-h | --help]\n", prog);
}

static void compute_widths(const struct host_group *groups, int32_t ngroups,
                           int *w_name, int *w_members)
{
    int32_t i;
    int     n;

    *w_name    = (int)strlen("GROUP_NAME");
    *w_members = (int)strlen("GROUP_MEMBER");

    for (i = 0; i < ngroups; i++) {
        if (groups[i].name != NULL) {
            n = (int)strlen(groups[i].name);
            if (n > *w_name)
                *w_name = n;
        }
        if (groups[i].members != NULL) {
            n = (int)strlen(groups[i].members);
            if (n > *w_members)
                *w_members = n;
        }
    }
}

static void print_groups(const struct host_group *groups, int32_t ngroups)
{
    int32_t     i;
    int         w_name, w_members;
    const char *name, *members;

    compute_widths(groups, ngroups, &w_name, &w_members);

    printf("%-*s  %-*s\n", w_name, "GROUP_NAME", w_members, "GROUP_MEMBER");
    printf("%.*s  %.*s\n",
           w_name,    "----------------------------------------------",
           w_members, "----------------------------------------------");

    for (i = 0; i < ngroups; i++) {
        name = groups[i].name;
        if (name == NULL)
            name = "";
        members = groups[i].members;
        if (members == NULL)
            members = "";
        printf("%-*s  %-*s\n", w_name, name, w_members, members);
    }
}

static const struct option long_opts[] = {
    { "help", no_argument, NULL, 'h' },
    { NULL, 0, NULL, 0 }
};

int main(int argc, char **argv)
{
    struct host_group *groups;
    int32_t            ngroups;
    int                opt;

    while ((opt = getopt_long(argc, argv, "h", long_opts, NULL)) != -1) {
        switch (opt) {
        case 'h':
            usage();
            return 0;
        default:
            usage();
            return 1;
        }
    }

    if (optind < argc) {
        usage();
        return 1;
    }

    groups = llb_group_info(&ngroups);
    if (groups == NULL) {
        fprintf(stderr, "%s: llb_group_info failed\n", prog);
        return 1;
    }

    print_groups(groups, ngroups);
    llb_free_group_info(groups, ngroups);
    return 0;
}
