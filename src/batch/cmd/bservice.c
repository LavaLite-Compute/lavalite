/* Copyright (C) LavaLite Contributors
 * GPL v2
 */
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <getopt.h>
#include <pwd.h>
#include <sys/param.h>

#include "llbatch.h"

struct svc_col_widths {
    int name;
    int queue;
};

struct inst_col_widths {
    int user;
    int port;
    int job_id;
    int run_host;
    int status;
};

static int imax(int a, int b)
{
    return a > b ? a : b;
}

static int ndigits(int64_t n)
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

static const char *uid_name(uid_t uid, char *buf, size_t bufsz)
{
    struct passwd *pw = getpwuid(uid);
    if (pw != NULL)
        return pw->pw_name;

    snprintf(buf, bufsz, "%u", (unsigned int) uid);
    return buf;
}

static void compute_service_widths(const struct svc_info *s, int32_t n,
                                   struct svc_col_widths *w)
{
    w->name = strlen("NAME");
    w->queue = strlen("QUEUE");

    for (int32_t i = 0; i < n; i++) {
        w->name = imax(w->name, strlen(s[i].name));
        w->queue = imax(w->queue, strlen(s[i].queue));
    }
}

static void compute_instance_widths(const struct svc_info *s,
                                    struct inst_col_widths *w)
{
    w->user = strlen("USER");
    w->port = strlen("PORT");
    w->job_id = strlen("JOB_ID");
    w->run_host = strlen("RUN_HOST");
    w->status = strlen("STATUS");

    for (uint32_t i = 0; i < s->ninstances; i++) {
        const struct svc_instance_info *inst = &s->instances[i];
        char uidbuf[32];
        const char *user = uid_name(inst->uid, uidbuf, sizeof(uidbuf));

        w->user = imax(w->user, strlen(user));
        w->port = imax(w->port, ndigits(inst->port));
        w->job_id = imax(w->job_id, ndigits(inst->job_id));
        w->run_host = imax(w->run_host,
                           strlen(inst->run_host ? inst->run_host : "-"));
        w->status = imax(w->status, strlen(llb_svc_status_str(inst->status)));
    }
}

static void print_services(const struct svc_info *s, int32_t n)
{
    struct svc_col_widths sw;
    compute_service_widths(s, n, &sw);

    printf("%-*s  %-*s\n", sw.name, "NAME", sw.queue, "QUEUE");

    for (int32_t i = 0; i < n; i++) {
        printf("%-*s  %-*s\n", sw.name, s[i].name, sw.queue, s[i].queue);

        if (s[i].ninstances == 0)
            continue;

        struct inst_col_widths iw;
        compute_instance_widths(&s[i], &iw);

        printf("  %-*s  %*s  %*s  %-*s  %-*s\n",
               iw.user, "USER", iw.port, "PORT", iw.job_id, "JOB_ID",
               iw.run_host, "RUN_HOST", iw.status, "STATUS");

        for (uint32_t j = 0; j < s[i].ninstances; j++) {
            const struct svc_instance_info *inst = &s[i].instances[j];
            char uidbuf[32];
            const char *user = uid_name(inst->uid, uidbuf, sizeof(uidbuf));

            printf("  %-*s  %*d  %*ld  %-*s  %-*s\n",
                   iw.user, user,
                   iw.port, inst->port,
                   iw.job_id, (long) inst->job_id,
                   iw.run_host, inst->run_host ? inst->run_host : "-",
                   iw.status, llb_svc_status_str(inst->status));
        }
    }
}

static int parse_service_endpoint(const char *s, char *host, size_t hostsz,
                                  int *port)
{
    const char *p = s;
    const char *prefix = "http://";
    size_t prefix_len = strlen(prefix);

    if (strncmp(p, prefix, prefix_len) == 0)
        p += prefix_len;

    const char *colon = strchr(p, ':');
    if (colon == NULL || colon == p)
        return -1;

    size_t hostlen = (size_t) (colon - p);
    if (hostlen >= hostsz)
        return -1;

    memcpy(host, p, hostlen);
    host[hostlen] = '\0';

    char *end;
    long n = strtol(colon + 1, &end, 10);
    if (*end != '\0' || n < 1 || n > 65535)
        return -1;

    *port = (int) n;
    return 0;
}

static void usage(void)
{
    fprintf(stderr, "bservice: --help display this help and exit\n"
                    "  bservice NAME  start a service defined in llb.services\n"
                    "  -l, --list list configured services and their instances\n"
                    "  -d, --delete URL delete a running service instance\n"
                    "  --version output version information and exit\n");
}

static struct option longopts[] = {
    {"help", no_argument, NULL, 'h'},
    {"version", no_argument, NULL, 'v'},
    {"list", no_argument, NULL, 'l'},
    {"delete", required_argument, NULL, 'd'},
    {NULL, 0, NULL, 0}
};

int main(int argc, char **argv)
{
    const char *delete_url = NULL;
    int list_fmt = 0;

    int cc;
    while ((cc = getopt_long(argc, argv, "hvld:", longopts, NULL)) != EOF) {
        switch (cc) {
        case 'd':
            delete_url = optarg;
            break;
        case 'l':
            list_fmt = 1;
            break;
        case 'v':
            fprintf(stderr, "%s\n", LAVALITE_VERSION_STR);
            return 0;
        case 'h':
        default:
            usage();
            return 0;
        }
    }

    if (delete_url != NULL) {
        char host[MAXHOSTNAMELEN];
        int port;

        if (parse_service_endpoint(delete_url, host, sizeof(host), &port) < 0) {
            fprintf(stderr, "bservice: invalid service URL: %s\n", delete_url);
            return -1;
        }

        int rc = llb_service_delete(host, port);
        if (rc != 0)
            fprintf(stderr, "bservice: %s: %m\n", delete_url);
        else
            printf("service %s deleted\n", delete_url);

        return rc;
    }

    if (list_fmt) {
        int32_t nsvc = 0;
        struct svc_info *s = llb_service_info(&nsvc);

        if (s == NULL) {
            if (nsvc == 0) {
                printf("No services: %m\n");
                return 0;
            }
            fprintf(stderr, "bservice: failed\n");
            return -1;
        }

        print_services(s, nsvc);
        llb_free_service_info(s, nsvc);
        return 0;
    }

    if (optind >= argc) {
        usage();
        return -1;
    }

    const char *name = argv[optind];
    struct svc_instance_info out;
    memset(&out, 0, sizeof(out));

    /* blocks until the backing job reaches RUNNING -- see
     * llb_service_start()/call_mbd_timeout()
     */
    int rc = llb_service_start(name, &out);
    if (rc != 0) {
        fprintf(stderr, "bservice: %s: %m\n", name);
        return rc;
    }

    printf("http://%s:%d\n", out.run_host, out.port);

    free(out.service);
    free(out.run_host);

    return 0;
}
