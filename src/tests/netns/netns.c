// Copyright (C) LavaLite Contributors
// GPL v2

#include <errno.h>
#include <getopt.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

#include "batch/sbd/sbd.h"
#include "batch/sbd/snamespace.h"

#define TEST_APP_PORT 8888

static void usage(FILE *stream, const char *prog)
{
    fprintf(stream,
            "Usage:\n"
            "  %s --job JOB_ID --port PORT\n"
            "  %s --delete --job JOB_ID\n"
            "  %s --list\n"
            "  %s --help\n"
            "\n"
            "Options:\n"
            "  -j, --job JOB_ID   Service job ID\n"
            "  -p, --port PORT     External TCP port\n"
            "  -d, --delete        Delete the service namespace\n"
            "  -l, --list          List network namespaces\n"
            "  -h, --help          Show this help\n"
            "\n"
            "The test service port is fixed at %d.\n",
            prog, prog, prog, prog, TEST_APP_PORT);
}

static int parse_int64(const char *arg, int64_t *value)
{
    char *end;
    long long v;

    errno = 0;
    v = strtoll(arg, &end, 10);
    if (errno != 0 || *arg == '\0' || *end != '\0' || v <= 0)
        return -1;

    *value = (int64_t)v;
    return 0;
}

static int parse_port(const char *arg, int *port)
{
    char *end;
    long v;

    errno = 0;
    v = strtol(arg, &end, 10);
    if (errno != 0 || *arg == '\0' || *end != '\0' || v < 1 || v > 65535)
        return -1;

    *port = (int)v;
    return 0;
}

static int list_namespaces(void)
{
    char *const argv[] = {"ip", "netns", "list", NULL};
    pid_t pid;
    int status;

    pid = fork();
    if (pid < 0) {
        perror("fork");
        return -1;
    }

    if (pid == 0) {
        execvp(argv[0], argv);
        perror("execvp");
        _exit(127);
    }

    if (waitpid(pid, &status, 0) < 0) {
        perror("waitpid");
        return -1;
    }

    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
        return -1;

    return 0;
}

int main(int argc, char **argv)
{
    static const struct option long_options[] = {
        {"job", required_argument, NULL, 'j'},
        {"port", required_argument, NULL, 'p'},
        {"delete", no_argument, NULL, 'd'},
        {"list", no_argument, NULL, 'l'},
        {"help", no_argument, NULL, 'h'},
        {NULL, 0, NULL, 0}
    };

    struct sbd_job job = {0};
    int ext_port = 0;
    int delete = 0;
    int list = 0;
    int have_job = 0;
    int c;

    while ((c = getopt_long(argc, argv, "j:p:dlh", long_options, NULL)) != -1) {
        switch (c) {
        case 'j':
            if (parse_int64(optarg, &job.job_id) < 0) {
                fprintf(stderr, "invalid job ID: %s\n", optarg);
                return EXIT_FAILURE;
            }
            have_job = 1;
            break;

        case 'p':
            if (parse_port(optarg, &ext_port) < 0) {
                fprintf(stderr, "invalid port: %s\n", optarg);
                return EXIT_FAILURE;
            }
            break;

        case 'd':
            delete = 1;
            break;

        case 'l':
            list = 1;
            break;

        case 'h':
            usage(stdout, argv[0]);
            return EXIT_SUCCESS;

        default:
            usage(stderr, argv[0]);
            return EXIT_FAILURE;
        }
    }

    if (optind != argc) {
        usage(stderr, argv[0]);
        return EXIT_FAILURE;
    }

    if (list) {
        if (delete || have_job || ext_port != 0) {
            usage(stderr, argv[0]);
            return EXIT_FAILURE;
        }

        return list_namespaces() == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    if (!have_job) {
        usage(stderr, argv[0]);
        return EXIT_FAILURE;
    }

    if (delete) {
        if (ext_port != 0) {
            usage(stderr, argv[0]);
            return EXIT_FAILURE;
        }

        if (snamespace_destroy_job(&job) < 0) {
            perror("snamespace_destroy_job");
            return EXIT_FAILURE;
        }

        printf("destroyed namespace svc%ld\n", job.job_id);
        return EXIT_SUCCESS;
    }

    if (ext_port == 0) {
        usage(stderr, argv[0]);
        return EXIT_FAILURE;
    }

    job.ext_port = ext_port;
    job.app_port = TEST_APP_PORT;

    if (snamespace_setup(&job) < 0) {
        perror("snamespace_setup");
        return EXIT_FAILURE;
    }

    printf("created namespace svc%ld\n", job.job_id);
    printf("  external port: %d\n", job.ext_port);
    printf("  service port:  %d\n", job.app_port);
    printf("\n");
    printf("start the test service with:\n");
    printf("  sudo ip netns exec svc%ld python3 -m http.server %d\n",
           job.job_id, job.app_port);
    printf("\n");
    printf("delete with:\n");
    printf("  sudo %s --delete --job %ld\n", argv[0], job.job_id);

    return EXIT_SUCCESS;
}
