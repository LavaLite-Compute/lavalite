/* lshosts.c - display static host information
 * Copyright (C) 2024-2025 LavaLite Contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 */

#include <lsf.h>

static void usage(void)
{
    fprintf(stderr, "Usage: lshosts [-h] [-V] [host_name ...]\n");
}

int main(int argc, char **argv)
{
    struct hostInfo *hosts;
    int numhosts = 0;
    int opt;

    if (ls_initdebug(argv[0]) < 0) {
        ls_perror("ls_initdebug");
        exit(-1);
    }

    while ((opt = getopt(argc, argv, "hV")) != EOF) {
        switch (opt) {
        case 'h':
            usage();
            return 0;
        case 'V':
            fprintf(stderr, "%s\n", LAVALITE_VERSION_STR);
            return 0;
        default:
            usage();
            return -1;
        }
    }

    // Remaining args are hostnames (if any)
    char **hostnames = NULL;
    int nhosts = argc - optind;
    if (nhosts > 0) {
        hostnames = &argv[optind];
    }

    hosts = ls_gethostinfo(NULL, &numhosts, hostnames, nhosts, 0);
    if (hosts == NULL) {
        ls_perror("ls_get_host_info");
        return -1;
    }
    // Print header
    printf("%-15s %-13s %-15s %6s %6s %8s %8s %6s\n", "HOST_NAME", "type",
           "model", "cpuf", "ncpus", "maxmem", "maxswp", "server");

    // Print each host
    for (int i = 0; i < numhosts; i++) {
        struct hostInfo *h = &hosts[i];

        const char *hostName = (h->hostName[0] != '\0') ? h->hostName : "-";
        const char *hostType =
            (h->hostType && h->hostType[0]) ? h->hostType : "-";
        const char *hostModel =
            (h->hostModel && h->hostModel[0]) ? h->hostModel : "-";
        const char *server = h->isServer ? "Yes" : "No";

        printf("%-15s %-13s %-15s %6.1f %6d %8d %8d %6s\n", hostName, hostType,
               hostModel, h->cpuFactor, h->maxCpus > 0 ? h->maxCpus : 0,
               h->maxMem, h->maxSwap, server);
    }

    return 0;
}
