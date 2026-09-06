// Copyright (C) LavaLite Contributors
// GPL v2

#include <stdio.h>
#include <stdlib.h>

#include "batch/sbd/sbd.h"
#include "batch/sbd/snamespace.h"

int main(void)
{
    struct sbd_job job = {0};

    job.job_id = 123;

    if (snamespace_setup(&job) < 0) {
        perror("snamespace_setup");
        return EXIT_FAILURE;
    }

    printf("created namespace svc%ld\n", job.job_id);
    printf("inspect with:\n");
    printf("  ip addr show\n");
    printf("  ip netns exec svc%ld ip addr show\n", job.job_id);
    printf("  ip netns exec svc%ld ping -c 3 10.200.0.1\n", job.job_id);
    printf("hit enter to terminate:....");
    fflush(stdout);

    (void)getchar();

    if (snamespace_destroy_job(&job) < 0) {
        perror("snamespace_destroy_job");
        return EXIT_FAILURE;
    }

    printf("destroyed namespace svc%ld\n", job.job_id);
    return EXIT_SUCCESS;
}
