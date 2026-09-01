/*
 * Copyright (C) LavaLite Contributors
 * GPL v2
 */

#include <stdio.h>
#include <stdlib.h>

#include "batch/sbd/snamespace.h"

int main(void)
{
    struct snamespace ns;

    if (snamespace_create(123, &ns) < 0) {
        perror("snamespace_create");
        return EXIT_FAILURE;
    }

    printf("created namespace %s\n", ns.name);

    printf("hit enter to terminate:....");
    /* inspect with: ip netns list
     */
    getchar();

    if (snamespace_destroy(&ns) < 0) {
        perror("snamespace_destroy");
        return EXIT_FAILURE;
    }

    printf("destroyed namespace %s\n", ns.name);

    return EXIT_SUCCESS;
}
