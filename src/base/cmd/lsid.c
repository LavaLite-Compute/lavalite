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

    char *name = ll_clustername();
    if (name == NULL) {
        fprintf(stderr, "lsid: ls_getclustername failed\n");
        return 1;
    }
    printf("My cluster name is %s\n", name);

    name = ll_mastername();
    if (name == NULL) {
        fprintf(stderr, "lsid: ls_getmastername failed\n");
        return 1;
    }
    printf("My master name is %s\n", name);

    return 0;
}
