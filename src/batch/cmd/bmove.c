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
            "Usage: bmove --to <queue> <job_id>\n"
            "\n"
            "Move a pending or held job to a different queue.\n"
            "\n"
            "Options:\n"
            "  --to queue     Destination queue\n"
            "  -h, --help     Display this help and exit\n"
            "  -V, --version  Output version information and exit\n");
}

static struct option longopts[] = {
    { "to",      required_argument, NULL, 't' },
    { "help",    no_argument,       NULL, 'h' },
    { "version", no_argument,       NULL, 'V' },
    { NULL, 0, NULL, 0 }
};

int main(int argc, char **argv)
{
    char *to_queue = NULL;
    int c;

    while ((c = getopt_long(argc, argv, "t:hV", longopts, NULL)) != -1) {
        switch (c) {
        case 't':
            to_queue = optarg;
            break;
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

    if (to_queue == NULL) {
        fprintf(stderr, "bmove: --to queue is required\n");
        usage(stderr);
        return 1;
    }

    if (optind >= argc) {
        fprintf(stderr, "bmove: job_id is required\n");
        usage(stderr);
        return 1;
    }

    char *end;
    int64_t job_id = strtoll(argv[optind], &end, 10);
    if (end == argv[optind] || *end != '\0' || job_id <= 0) {
        fprintf(stderr, "bmove: invalid job_id '%s'\n", argv[optind]);
        return 1;
    }

    if (llb_move_job((int64_t) job_id, to_queue) < 0) {
        fprintf(stderr, "bmove: job <%ld> move failed: %s\n",
                job_id, strerror(errno));
        return 1;
    }

    printf("Job <%ld> moved to queue <%s>.\n", job_id, to_queue);
    return 0;
}
