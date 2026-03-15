/* Copyright (C) LavaLite Contributors
 * GPL v2
 */
#include "include/ll.h"

static void usage(const char *cmd)
{
    fprintf(stderr, "Usage: %s [-h] [-V]\n", cmd);
}

int main(int argc, char **argv)
{
    int opt;
    while ((opt = getopt(argc, argv, "hV")) != -1) {
        switch (opt) {
        case 'h':
            usage(argv[0]);
            return 0;
        case 'V':
            puts(LAVALITE_VERSION_STR);
            return 0;
        default:
            usage(argv[0]);
            return 1;
        }
    }

    struct ll_cluster_info *cl = ll_clusterinfo();
    if (cl == NULL) {
        fprintf(stderr, "lsclusters: ls_clusterinfo failed\n");
        return 1;
    }

    printf("%-20s %-20s %-20s\n", "CLUSTER_NAME", "MASTER_HOST", "ADMIN");
    printf("%-20s %-20s %-20s\n", cl->cluster_name, cl->master_name,
           cl->manager_name);

    free(cl);
    return 0;
}
