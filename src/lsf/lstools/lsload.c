/* lsload.c - display static host information
 * Copyright (C) 2024-2025 LavaLite Contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 */

#include "lsf/lib/lib.h"



static void usage(void)
{
    fprintf(stderr, "Usage: lsload [-h] [-V] [host_name ...]\n");
    exit(-1);
}

static const char *status_str(int status)
{
    if (LS_ISUNAVAIL(status))
        return "unavail";
    if (LS_ISBUSY(status))
        return "busy";
    if (LS_ISLOCKED(status))
        return "locked";
    if (LS_ISOKNRES(status))
        return "ok";
    return "unknown";
}

int main(int argc, char **argv)
{
    struct wire_load_info *loads;
    int numhosts = 0;
    int i;
    int opt;

    if (ls_initdebug(argv[0]) < 0) {
        ls_perror("ls_initdebug");
        exit(-1);
    }

    while ((opt = getopt(argc, argv, "hV")) != -1) {
        switch (opt) {
        case 'h':
            usage();
            return 0;
        case 'V':
            fprintf(stderr, "%s\n", LAVALITE_VERSION_STR);
            return 0;
        default:
            usage();
        }
    }

    // Remaining args are hostnames (if any)
    char **hostnames = NULL;
    int nhosts = argc - optind;
    if (nhosts > 0) {
        hostnames = &argv[optind];
    }

    loads = ls_load(NULL, &numhosts, 0, NULL);
    if (loads == NULL) {
        ls_perror("ls_get_load_info");
        return -1;
    }

    // Print header
    printf("%-15s %8s %6s %6s %6s %5s %5s %4s %4s %4s %7s %7s %7s\n",
           "HOST_NAME", "status", "r15s", "r1m", "r15m", "ut",
           "pg", "io", "ls", "it", "tmp", "swp", "mem");

    // Print each host's load
    for (i = 0; i < numhosts; i++) {
        struct wire_load_info *l = &loads[i];
        float *li = l->loadIndices;

        printf("%-15s %8s %6.1f %6.1f %6.1f %4.0f%% %5.1f %4.0f %4.0f "
               "%4.0f %6.0fM %6.0fM %6.0fM\n",
               l->hostname,
               status_str(l->status),
               li[R15S],
               li[R1M],
               li[R15M],
               li[UT] * 100.0,
               li[PG],
               li[IO],
               li[LS],
               li[IT],
               li[TMP],
               li[SWP],
               li[MEM]);
    }

    return 0;
}
