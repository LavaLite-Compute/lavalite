/*
 * Copyright (C) LavaLite Contributors
 * GPL v2
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>

#include "llbatch.h"

static void usage(FILE *f)
{
    fprintf(f,
            "Usage: bpriority --priority N job_id\n"
            "\n"
            "Change the priority of a pending or held job.\n"
            "Priority cannot exceed the queue priority unless admin.\n"
            "\n"
            "Options:\n"
            "  -p, --priority N   New priority value\n"
            "  -h, --help         Display this help and exit\n"
            "  -V, --version      Output version information and exit\n");
}

static struct option longopts[] = {
    { "priority", required_argument, NULL, 'p' },
    { "help",     no_argument,       NULL, 'h' },
    { "version",  no_argument,       NULL, 'V' },
    { NULL, 0, NULL, 0 }
};

int main(int argc, char **argv)
{
    int32_t priority = -1;
    int priority_set = 0;
    int c;

    while ((c = getopt_long(argc, argv, "p:hV", longopts, NULL)) != -1) {
        switch (c) {
        case 'p': {
            char *end;
            long v = strtol(optarg, &end, 10);
            if (*end != '\0' || v < 0) {
                fprintf(stderr, "bpriority: invalid priority '%s'\n", optarg);
                return 1;
            }
            priority = (int32_t) v;
            priority_set = 1;
            break;
        }
        case 'h':
            usage(stdout);
            return 0;
        case 'V':
            printf("%s\n", LAVALITE_VERSION_STR);
            return 0;
        default:
            usage(stderr);
            return 1;
        }
    }

    if (!priority_set) {
        fprintf(stderr, "bpriority: --priority is required\n");
        usage(stderr);
        return 1;
    }

    if (optind >= argc) {
        fprintf(stderr, "bpriority: job_id is required\n");
        usage(stderr);
        return 1;
    }

    int64_t job_id;
    char *end;
    job_id = strtoll(argv[optind], &end, 10);
    if (end == argv[optind] || *end != '\0' || job_id <= 0) {
        fprintf(stderr, "bpriority: invalid job_id '%s'\n", argv[optind]);
        return 1;
    }

    if (llb_priority_job(job_id, priority) < 0) {
        fprintf(stderr, "bpriority: job <%ld> priority change failed: %s\n",
                job_id, strerror(errno));
        return 1;
    }

    printf("Job <%ld> priority set to %d.\n", job_id, priority);
    return 0;
}
