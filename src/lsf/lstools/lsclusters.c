/* lsclusters.c - display cluster information
 *
 * Copyright (C) 2024-2025 LavaLite Contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 */

#include <lsf.h>

static void usage(const char *);
static const char *cluster_status_to_str(int);

int main(int argc, char **argv)
{
    struct clusterInfo *info;
    int num_clusters;
    int cc;
    int i;

    if (ls_initdebug(argv[0]) < 0) {
        ls_perror("ls_initdebug");
        return -1;
    }

    while ((cc = getopt(argc, argv, "Vh")) != -1) {
        switch (cc) {
        case 'V':
            fprintf(stderr, "%s\n", LAVALITE_VERSION_STR);
            return 0;
        case 'h':
        default:
            usage(argv[0]);
            return -1;
        }
    }

    if (optind < argc) {
        usage(argv[0]);
        return -1;
    }

    num_clusters = 0;
    info = ls_clusterinfo(NULL, &num_clusters, NULL, 0, 0);
    if (info == NULL) {
        ls_perror("ls_clusterinfo");
        return -1;
    }

    if (num_clusters <= 0) {
        ls_perror("No clusters configured.\n");
        return 0;
    }

    /* Header */
    printf("%-13s %-7s %-13s %-12s %7s %9s\n", "CLUSTER_NAME", "STATUS",
           "MASTER_HOST", "ADMIN", "HOSTS", "SERVERS");

    for (i = 0; i < num_clusters; i++) {
        const char *status;
        const char *admin;

        status = cluster_status_to_str(info[i].status);

        admin = "-";
        if (info[i].nAdmins > 0 && info[i].admins != NULL &&
            info[i].admins[0] != NULL) {
            admin = info[i].admins[0];
        }

        printf("%-13s %-7s %-13s %-12s %7d %9d\n", info[i].clusterName, status,
               info[i].masterName, admin, info[i].numServers,
               info[i].numServers);
    }

    return 0;
}

static void usage(const char *cmd)
{
    fprintf(stderr, "Usage: %s [-h] [-V]\n", cmd);
}

static const char *cluster_status_to_str(int status)
{
    switch (status) {
#ifdef CLUST_STAT_OK
    case CLUST_STAT_OK:
        return "ok";
#endif
#ifdef CLUST_STAT_UNAVAIL
    case CLUST_STAT_UNAVAIL:
        return "unavail";
#endif
    default:
        return "unknown";
    }
}
