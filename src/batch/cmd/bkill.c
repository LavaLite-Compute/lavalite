/* Copyright (C) LavaLite Contributors
 * GPL v2
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <ctype.h>
#include <errno.h>
#include <getopt.h>

#include "llbatch.h"

static int parse_signal(const char *s, int *out)
{
    if (s == NULL || *s == '\0')
        return -1;

    // numeric?
    if (isdigit((unsigned char) s[0])) {
        char *end = NULL;
        long v;

        errno = 0;
        v = strtol(s, &end, 10);
        if (errno != 0 || end == s || *end != '\0')
            return -1;
        if (v <= 0 || v >= NSIG)
            return -1;
        // Numeric signals are passed through unchanged.
        *out = (int) v;
        return 0;
    }

    if (strcasecmp(s, "kill") == 0) {
        *out = SIGKILL;
        return 0;
    }
    if (strcasecmp(s, "term") == 0 || strcasecmp(s, "terminate") == 0) {
        *out = SIGTERM;
        return 0;
    }
    if (strcasecmp(s, "stop") == 0) {
        *out = SIGSTOP;
        return 0;
    }
    if (strcasecmp(s, "cont") == 0 || strcasecmp(s, "continue") == 0) {
        *out = SIGCONT;
        return 0;
    }
    if (strcasecmp(s, "int") == 0) {
        *out = SIGINT;
        return 0;
    }
    if (strcasecmp(s, "hup") == 0) {
        *out = SIGHUP;
        return 0;
    }
    if (strcasecmp(s, "tstp") == 0) {
        *out = SIGTSTP;
        return 0;
    }

    // Unsupported for now
    return -1;
}

/*
 * Parse a bkill target: an ordinary job id "N" (array_index left at 0),
 * or a single array element "N[M]". The mbd side decides whether a
 * bare N is an ordinary job or an array_id — bkill just splits the
 * syntax, it doesn't need to know which.
 */
static int parse_jobid(const char *s, int64_t *job_id, int32_t *array_index)
{
    char *end;

    errno = 0;
    int64_t id = strtoll(s, &end, 10);
    if (end == s || errno != 0)
        return -1;

    if (*end == '\0') {
        *job_id = id;
        *array_index = 0;
        return 0;
    }

    if (*end != '[')
        return -1;

    char *idx_end;
    errno = 0;
    long idx = strtol(end + 1, &idx_end, 10);
    if (idx_end == end + 1 || errno != 0)
        return -1;
    if (*idx_end != ']' || idx_end[1] != '\0')
        return -1;
    if (idx <= 0)
        return -1;

    *job_id = id;
    *array_index = (int32_t) idx;
    return 0;
}

static void usage(void)
{
    fprintf(stderr, "bkill: -s SIGNAL jobid|jobid[idx] [...]\n");
    fprintf(stderr, " --signal SIGNAL jobid|jobid[idx] [...]\n");
    fprintf(stderr, "SIGNAL: kill | term | stop | tstp | "
            "cont | int | hup | <number>\n");
}

int main(int argc, char **argv)
{
    static struct option longopts[] = {{"signal", required_argument, 0, 's'},
                                       {"help", no_argument, 0, 'h'},
                                       {"version", no_argument, 0, 'v'},
                                       {0, 0, 0, 0}};

    int sig = SIGTERM;
    int cc;

    while ((cc = getopt_long(argc, argv, "s:hv", longopts, NULL)) != -1) {
        switch (cc) {
        case 's':
            if (parse_signal(optarg, &sig) < 0) {
                fprintf(stderr, "%s: invalid signal '%s'\n", argv[0], optarg);
                usage();
                return -1;
            }
            break;
        case 'v':
            fprintf(stderr, "%s\n", LAVALITE_VERSION_STR);
            return 0;
        case 'h':
        default:
            usage();
            return -1;
        }
    }

    if (sig < 0) {
        usage();
        return -1;
    }

    if (optind >= argc) {
        usage();
        return -1;
    }

    // Validate all jobids first (tight parsing; no partial execution).
    for (int i = optind; i < argc; i++) {
        int64_t jobid;
        int32_t array_index;

        if (parse_jobid(argv[i], &jobid, &array_index) < 0) {
            fprintf(stderr, "bkill: invalid jobid '%s'\n", argv[i]);
            return -1;
        }
    }

    int signaled = 0;
    for (; optind < argc; optind++) {
        int64_t jobid;
        int32_t array_index;

        if (parse_jobid(argv[optind], &jobid, &array_index) < 0) {
            fprintf(stderr, "bkill: invalid jobid '%s'\n", argv[optind]);
            return -1;
        }

        if (llb_signal_job(jobid, array_index, sig) < 0) {
            fprintf(stderr, "bkill: failed to signal job <%s>: %m\n",
                    argv[optind]);
            continue;
        }

        if (jobid == 0)
            printf("All your jobs are being signaled\n");
        else if (array_index != 0)
            printf("Job <%ld[%d]> is being signaled\n", (long) jobid,
                   array_index);
        else
            printf("Job <%ld> is being signaled\n", (long) jobid);

        signaled = 1;
    }
    if (!signaled)
        return -1;

    return 0;
}
