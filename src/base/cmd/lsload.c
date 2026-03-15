/* Copyright (C) 2007 Platform Computing Inc
 * Copyright (C) LavaLite Contributors
 * GPL v2
 */
#include "include/ll.h"

static const char *load_status(uint32_t status)
{
    if (status == 0)
        return "ok";
    return "unavail";
}

static void usage(const char *cmd)
{
    fprintf(stderr, "Usage: %s [-h] [-V] [-l]\n", cmd);
}

int main(int argc, char **argv)
{
    int opt;
    int long_fmt = 0;

    while ((opt = getopt(argc, argv, "hVl")) != -1) {
        switch (opt) {
        case 'h':
            usage(argv[0]);
            return 0;
        case 'V':
            puts(LAVALITE_VERSION_STR);
            return 0;
        case 'l':
            long_fmt = 1;
            break;
        default:
            usage(argv[0]);
            return 1;
        }
    }

    int nhosts = 0;
    struct ll_host_load *loads = ll_hostload(&nhosts);
    if (loads == NULL) {
        fprintf(stderr, "lsload: ls_load failed\n");
        return 1;
    }

    if (long_fmt) {
        printf("%-16s %-8s %6s %6s %6s %4s %6s %6s %6s %6s %6s %6s %6s\n",
               "HOST_NAME", "status",
               "r15s", "r1m", "r15m", "ut",
               "pg", "io", "ls", "it",
               "mem", "swp", "tmp");
    } else {
        printf("%-16s %-8s %6s %6s %6s %4s %6s %6s %6s\n",
               "HOST_NAME", "status",
               "r15s", "r1m", "r15m", "ut",
               "mem", "swp", "tmp");
    }

    for (int i = 0; i < nhosts; i++) {
        struct ll_host_load *h = &loads[i];
        int unavail = (h->status != 0);

        if (unavail) {
            printf("%-16s %-8s %s\n", h->hostname, "unavail", "-");
            continue;
        }

        char ut[8], it[8];
        snprintf(ut, sizeof(ut), "%.0f%%", h->li[UT]);
        snprintf(it, sizeof(it), "%.0f%%", h->li[IT]);

        char mem[8], swp[8], tmp[8];
        snprintf(mem, sizeof(mem), "%.0fG", h->li[MEM]  / 1024.0);
        snprintf(swp, sizeof(swp), "%.0fG", h->li[SWP]  / 1024.0);
        snprintf(tmp, sizeof(tmp), "%.0fG", h->li[TMP]  / 1024.0);

        if (long_fmt) {
            printf("%-16s %-8s %6.1f %6.1f %6.1f %4s %6.1f %6.1f %6.1f %4s "
                   "%6s %6s %6s\n", h->hostname, load_status(h->status),
                   h->li[R15S], h->li[R1M], h->li[R15M], ut,
                   h->li[PG], h->li[IO], h->li[LS], it,
                   mem, swp, tmp);
        } else {
            printf("%-16s %-8s %6.1f %6.1f %6.1f %4s %6s %6s %6s\n",
                   h->hostname, load_status(h->status),
                   h->li[R15S], h->li[R1M], h->li[R15M], ut,
                   mem, swp, tmp);
        }
    }

    free(loads);
    return 0;
}
