#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

static void usage(void)
{
    fprintf(
        stderr,
        "Usage: bmetric [options] <subcommand> [args]\n"
        "\n"
        "Subcommands:\n"
        "  add  <metric> <value> <host>   add a metric reading for a host\n"
        "  get  <metric> <host>           get latest metric value for a host\n"
        "  list [host]                    list all metrics, optionally for a "
        "host\n"
        "  del  <metric> <host>           delete metric for a host\n"
        "\n"
        "Options:\n"
        "  -h, --help                     show this help\n"
        "\n"
        "Examples:\n"
        "  bmetric add gpu_temp 72 worker1\n"
        "  bmetric get gpu_temp worker1\n"
        "  bmetric list worker1\n");
}

int main(int argc, char *argv[])
{
    static struct option opts[] = {{"help", no_argument, NULL, 'h'},
                                   {NULL, 0, NULL, 0}};
    int c;

    while ((c = getopt_long(argc, argv, "+h", opts, NULL)) != -1) {
        switch (c) {
        case 'h':
            usage();
            return 0;
        default:
            usage();
            return 1;
        }
    }

    argc -= optind;
    argv += optind;

    if (argc < 1) {
        usage();
        return 1;
    }

    if (strcmp(argv[0], "add") == 0) {
        if (argc != 4) {
            fprintf(stderr, "bmetric add: requires <metric> <value> <host>\n");
            return 1;
        }
        /* TODO: send metric to LIM/MBD */
        printf("add metric=%s value=%s host=%s\n", argv[1], argv[2], argv[3]);
        return 0;
    }

    if (strcmp(argv[0], "get") == 0) {
        if (argc != 3) {
            fprintf(stderr, "bmetric get: requires <metric> <host>\n");
            return 1;
        }
        /* TODO */
        printf("get metric=%s host=%s\n", argv[1], argv[2]);
        return 0;
    }

    if (strcmp(argv[0], "list") == 0) {
        /* TODO */
        if (argc == 2)
            printf("list host=%s\n", argv[1]);
        else
            printf("list all\n");
        return 0;
    }

    if (strcmp(argv[0], "del") == 0) {
        if (argc != 3) {
            fprintf(stderr, "bmetric del: requires <metric> <host>\n");
            return 1;
        }
        /* TODO */
        printf("del metric=%s host=%s\n", argv[1], argv[2]);
        return 0;
    }

    fprintf(stderr, "bmetric: unknown subcommand '%s'\n", argv[0]);
    usage();
    return 1;
}
