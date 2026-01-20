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
    fprintf(stderr, "SIGNAL: kill | term | stop | cont | <number>\n");
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

    if (lsb_init(argv[0]) < 0) {
        lsb_perror("lsb_init");
        return -1;
    }

    bool_t signaled = 0;
    for (; optind < argc; optind++) {
        int64_t jobid;

        if (parse_jobid(argv[optind], &jobid) < 0) {
            fprintf(stderr, "%s: invalid jobid '%s'\n", argv[0], argv[optind]);
            continue;
        }

        if (lsb_signaljob(jobid, sig) < 0) {
            char msg[128];

            snprintf(msg, sizeof(msg), "Job <%s>", lsb_jobid2str(jobid));
            lsb_perror(msg);
            continue;
        }

        printf("Job <%s> is being signaled\n", lsb_jobid2str(jobid));
        signaled = true;
    }

    if (! signaled)
        return -1;
    return 0;
}

static int
parse_signal(const char *s, int *out)
{
    char buf[32];
    size_t i, n;

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

        *out = (int)v;
        return 0;
    }

    // normalize to lowercase
    n = strlen(s);
    if (n >= sizeof(buf))
        return -1;

    for (i = 0; i < n; i++)
        buf[i] = (char)tolower((unsigned char)s[i]);
    buf[n] = '\0';

    if (strcasecmp(buf, "kill") == 0) {
        *out = SIGKILL;
        return 0;
    }
    if (strcasecmp(buf, "term") == 0 || strcasecmp(buf, "terminate") == 0) {
        *out = SIGTERM;
        return 0;
    }
    if (strcasecmp(buf, "stop") == 0) {
        *out = SIGSTOP;
        return 0;
    }
    if (strcasecmp(buf, "cont") == 0 || strcasecmp(buf, "continue") == 0) {
        *out = SIGCONT;
        return 0;
    }

    return -1;
}

static int
parse_jobid(const char *s, int64_t *out)
{
    char *end = NULL;
    long long v;

    if (s == NULL || *s == '\0')
        return -1;

    errno = 0;
    v = strtoll(s, &end, 10);
    if (errno != 0 || end == s || *end != '\0')
        return -1;
    if (v <= 0)
        return -1;

    *out = (int64_t)v;
    return 0;
}
