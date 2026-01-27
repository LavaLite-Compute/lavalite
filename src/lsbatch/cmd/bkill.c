/*
 * Copyright (C) 2007 Platform Computing Inc
 * Copyright (C) LavaLite Contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 USA
 *
 */

#include "lsbatch/cmd/cmd.h"

static void usage(const char *cmd);
static int parse_signal(const char *s, int *out);
static int parse_jobid(const char *s, int64_t *out);
extern int  _lsb_recvtimeout;

static void
usage(const char *cmd)
{
    fprintf(stderr, "Usage: %s -s SIGNAL jobid [jobid ...]\n", cmd);
    fprintf(stderr, "       %s --signal SIGNAL jobid [jobid ...]\n", cmd);
    fprintf(stderr, "\n");
    fprintf(stderr, "SIGNAL: kill | term | stop | cont | hup|  <number>\n");
}

int
main(int argc, char **argv)
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
                usage(argv[0]);
                return -1;
            }
            break;
        case 'V':
            fprintf(stderr, "%s\n", LAVALITE_VERSION_STR);
            return 0;
        case 'h':
        default:
            usage(argv[0]);
            return -1;
        }
    }

    if (sig < 0) {
        usage(argv[0]);
        return -1;
    }

    if (optind >= argc) {
        usage(argv[0]);
        return -1;
    }

    // Validate all jobids first (tight parsing; no partial execution).
    for (int i = optind; i < argc; i++) {
        int64_t tmp;

        if (ll_validate_jobid(argv[i], &tmp) < 0) {
            fprintf(stderr, "%s: invalid jobid '%s'\n", argv[0], argv[i]);
            usage(argv[0]);
            return -1;
        }
    }

    if (lsb_init(argv[0]) < 0) {
        lsb_perror("lsb_init");
        return -1;
    }

    bool_t signaled = false;

    for (; optind < argc; optind++) {
        long long tmp;
        int64_t jobid;

        if (ll_validate_jobid(argv[optind], &tmp) < 0) {
            // Should not happen due to pre-validation above.
            fprintf(stderr, "%s: invalid jobid '%s'\n", argv[0], argv[optind]);
            return -1;
        }

        jobid = (int64_t)tmp;

        if (lsb_signaljob(jobid, sig) < 0) {
            char msg[128];

            snprintf(msg, sizeof(msg), "Job <%s>", lsb_jobid2str(jobid));
            lsb_perror(msg);
            continue;
        }

        if (jobid == 0)
            printf("All your jobs are being signaled\n");
        else
            printf("Job <%s> is being signaled\n", lsb_jobid2str(jobid));

        signaled = true;
    }

    if (!signaled)
        return -1;

    return 0;
}

static int
parse_signal(const char *s, int *out)
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

static int
parse_jobid(const char *s, int64_t *out)
{
    if (! ll_atoll(s, out))
        return false;

    return 0;
}
