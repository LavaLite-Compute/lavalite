/* Copyright (C) 2007 Platform Computing Inc
 * Copyright (C) LavaLite Contributors
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

    int nhosts = 0;
    struct ll_host_info *hosts = ll_hostinfo(&nhosts);
    if (hosts == NULL) {
        fprintf(stderr, "lshosts: ls_gethostinfo failed\n");
        return 1;
    }


    printf("%-20s %-16s %6s %8s %8s %8s %6s\n",
           "HOST_NAME", "type", "ncpus", "maxmem", "maxswp", "maxtmp", "master");

    for (int i = 0; i < nhosts; i++) {
        struct ll_host_info *h = &hosts[i];
        printf("%-20s %-12s %6lu %8lu %8lu %8lu %4s\n",
               h->host_name,
               h->host_type,
               h->num_cpus,
               h->max_mem,
               h->max_swap,
               h->max_tmp,
               h->is_master ? "yes" : "no");
    }

    free(hosts);
    return 0;
}
