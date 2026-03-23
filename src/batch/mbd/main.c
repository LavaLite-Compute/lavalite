// Copyright (C) LavaLite Contributors
// GPL V2

#include "batch/mbd/mbd.h"

static void usage(void)
{
    fprintf(stderr, "mbd: --help show this help message\n"
            "--version show version information and exit\n"
            "-e, set environment variable LL_ENVDIR\n");
}
static struct option opts[] = {{"help", no_argument, NULL, 'h'},
   {"version", no_argument, NULL, 'V'},
   {"envdir", required_argument, NULL, 'e'},
   {NULL, 0, NULL, 0}};

int main(int argc, char **argv)
{
    int cc;
    char *env_dir = NULL;
    while ((cc = getopt_long(argc, argv, "hVe:", opts, NULL)) != EOF) {
        switch (cc) {
            case 'e':
                env_dir = optarg;
                break;
            case 'V':
                fprintf(stderr, "%s\n", LAVALITE_VERSION_STR);
                return -1;
            case 'h':
            default:
                usage();
                return -1;
        }
    }

    return 0;
}
