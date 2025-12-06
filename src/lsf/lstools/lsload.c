/* lsload.c - display static host information
 * Copyright (C) 2024-2025 LavaLite Contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 */

#include <lsf.h>

#define LOAD_COL_WIDTH 8 // width for each numeric/load column

static void print_load_value(int index, const float *li, int unavailable)
{
    char buf[32];

    if (unavailable) {
        snprintf(buf, sizeof(buf), "-");
        printf(" %*s", LOAD_COL_WIDTH, buf);
        return;
    }

    switch (index) {
    case 0: /* r15s */
    case 1: /* r1m */
    case 2: /* r15m */
        snprintf(buf, sizeof(buf), "%.1f", li[index]);
        break;

    case 3: /* ut CPU % */
        snprintf(buf, sizeof(buf), "%.0f%%", li[index]);
        break;

    case 4: /* pg */
    case 5: /* io */
    case 6: /* ls */
        snprintf(buf, sizeof(buf), "%.1f", li[index]);
        break;

    case 7: /* it % */
        snprintf(buf, sizeof(buf), "%.0f%%", li[index]);
        break;

    case 8:  /* tmp GB */
    case 9:  /* swp GB */
    case 10: /* mem GB */
        snprintf(buf, sizeof(buf), "%.0fG", li[index]);
        break;

    default:
        snprintf(buf, sizeof(buf), "-");
        break;
    }

    printf(" %*s", LOAD_COL_WIDTH, buf);
}

static void format_status(const int *status, char *buf, size_t buflen)
{
    if (!status) {
        snprintf(buf, buflen, "-");
        return;
    }

    if (LS_ISUNAVAIL(status)) {
        snprintf(buf, buflen, "unavail");
        return;
    }

    snprintf(buf, buflen, "ok");
}

int main(int argc, char **argv)
{
    struct hostLoad *hload;
    int num_hosts = 0;
    int options = 0;
    char *resreq = NULL;
    char *fromhost = NULL;
    char status_buf[32];
    int i, j;

    static const char *load_headers[] = {"r15s", "r1m", "r15m", "ut",
                                         "pg",   "io",  "ls",   "it",
                                         "tmp",  "swp", "mem"};

    hload = ls_load(resreq, &num_hosts, options, fromhost);
    if (!hload) {
        ls_perror("ls_load");
        return 1;
    }

    /* Header line */
    printf("%-16s %-10s", "HOST_NAME", "status");
    for (i = 0; i < 11; i++) {
        printf(" %*s", LOAD_COL_WIDTH, load_headers[i]);
    }
    printf("\n");

    /* Data lines */
    for (i = 0; i < num_hosts; i++) {
        int unavailable;

        format_status(hload[i].status, status_buf, sizeof(status_buf));
        unavailable = (status_buf[0] != 'o'); /* not 'ok' */

        printf("%-16s %-10s", hload[i].hostName, status_buf);

        for (j = 0; j < 11 /* NBUILTINDEX-1 */; j++) {
            print_load_value(j, hload[i].li, unavailable);
        }

        printf("\n");
    }

    return 0;
}
