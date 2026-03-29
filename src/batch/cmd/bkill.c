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
    if (isdigit((unsigned char)s[0])) {
        char *end = NULL;
        long v;

        errno = 0;
        v = strtol(s, &end, 10);
        if (errno != 0 || end == s || *end != '\0')
            return -1;
        if (v <= 0 || v >= NSIG)
            return -1;
        // Note that we don't map numberical signals so
        // SIGSTOP is not mapped to SIGTSTP
        *out = (int)v;
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
        *out = SIGTSTP;
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
    // Unsupported for now
    return -1;
}

static void
usage(void)
{
    fprintf(stderr, "bkill: -s SIGNAL jobid [jobid ...]\n");
    fprintf(stderr, " --signal SIGNAL jobid [jobid ...]\n");
    fprintf(stderr, "SIGNAL: kill | term | stop | cont | hup|  <number>\n");
}

int main(int argc, char **argv)
{
    static struct option longopts[] = {
        { "signal", required_argument, 0, 's' },
        { "help",   no_argument,       0, 'h' },
        { "version",no_argument,       0, 'V' },
        { 0, 0, 0, 0 }
    };

    int sig = SIGTERM;
    int cc;

    while ((cc = getopt_long(argc, argv, "s:hV", longopts, NULL)) != -1) {
        switch (cc) {
        case 's':
            if (parse_signal(optarg, &sig) < 0) {
                fprintf(stderr, "%s: invalid signal '%s'\n", argv[0], optarg);
                usage();
                return -1;
            }
            break;
        case 'V':
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
        char *end;

        strtoll(argv[i], &end, 10);
        if (end == argv[i] || *end != 0) {
            fprintf(stderr, "bkill: invalid jobid '%s'\n", argv[i]);
            return -1;
        }
    }

    int signaled = 0;
    for (; optind < argc; optind++) {
        char *end;

        int64_t jobid = strtoll(argv[optind], &end, 10);
        if (end == argv[optind] || *end != 0) {
            fprintf(stderr, "bkill: invalid jobid '%s'\n", argv[optind]);
            return -1;
        }

        if (llb_signal_job(jobid, sig) < 0) {
            fprintf(stderr, "failed to signal job <%ld>\n", (long)jobid);
            continue;
        }

        if (jobid == 0)
            printf("All your jobs are being signaled\n");
        else
            printf("Job <%ld> is being signaled\n", (long)jobid);

        signaled = 1;
    }
    if (!signaled)
        return -1;

    return 0;
}
