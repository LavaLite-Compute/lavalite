/* Copyright (C) LavaLite Contributors
 * GPL v2
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <getopt.h>
#include "llbatch.h"

struct col_widths {
    int name;
    int total;
    int used;
    int free;
};

static int imax(int a, int b)
{
    return a > b ? a : b;
}

static int ndigits(int32_t n)
{
    if (n <= 0)
        return 1;
    int d = 0;
    while (n > 0) {
        d++;
        n /= 10;
    }
    return d;
}

static void compute_widths(struct token_pool_info *t, int32_t n,
                           struct col_widths *w)
{
    w->name = strlen("POOL_NAME");
    w->total = strlen("TOTAL");
    w->used = strlen("USED");
    w->free = strlen("FREE");

    for (int i = 0; i < n; i++) {
        w->name = imax(w->name, (int) strlen(t[i].name));
        w->total = imax(w->total, ndigits(t[i].total));
        w->used = imax(w->used, ndigits(t[i].used));
        w->free = imax(w->free, ndigits(t[i].free));
    }
}

static void usage(void)
{
    fprintf(stderr, "btokens: --help display this help and exit\n"
                    "--version output version information and exit\n");
}

static struct option longopts[] = {{"help", no_argument, NULL, 'h'},
                                   {"version", no_argument, NULL, 'V'},
                                   {NULL, 0, NULL, 0}};

int main(int argc, char **argv)
{
    int cc;
    while ((cc = getopt_long(argc, argv, "hV", longopts, NULL)) != EOF) {
        switch (cc) {
        case 'V':
            fprintf(stderr, "%s\n", LAVALITE_VERSION_STR);
            return 0;
        case 'h':
        default:
            usage();
            return 0;
        }
    }

    int32_t n;
    struct token_pool_info *t = llb_token_info(&n);
    if (!t) {
        fprintf(stderr, "btokens: failed\n");
        return -1;
    }

    if (n == 0) {
        printf("No token pools configured.\n");
        llb_free_token_info(t, n);
        return 0;
    }

    struct col_widths w;
    compute_widths(t, n, &w);

    printf("%-*s  %*s  %*s  %*s\n", w.name, "POOL_NAME", w.total, "TOTAL",
           w.used, "USED", w.free, "FREE");

    for (int i = 0; i < n; i++) {
        printf("%-*s  %*d  %*d  %*d\n", w.name, t[i].name, w.total, t[i].total,
               w.used, t[i].used, w.free, t[i].free);
    }

    llb_free_token_info(t, n);
    return 0;
}
