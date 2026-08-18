/* Copyright (C) LavaLite Contributors
 * GPL v2
 */
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <getopt.h>
#include "llbatch.h"

struct col_widths {
    int svc_id;
    int name;
    int port;
    int job_id;
    int run_host;
    int state;
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

static void compute_widths(struct svc_info *s, int32_t n, struct col_widths *w)
{
    w->svc_id = strlen("SVC_ID");
    w->name = strlen("NAME");
    w->port = strlen("PORT");
    w->job_id = strlen("JOB_ID");
    w->run_host = strlen("RUN_HOST");
    w->state = strlen("STATE");

    for (int i = 0; i < n; i++) {
        w->svc_id = imax(w->svc_id, strlen(s[i].svc_id));
        w->name = imax(w->name, strlen(s[i].name));
        w->port = imax(w->port, ndigits(s[i].port));
        w->job_id = imax(w->job_id, ndigits(s[i].job_id));
        w->run_host = imax(w->run_host,
                           strlen(s[i].run_host ? s[i].run_host : "-"));
        w->state = imax(w->state, strlen(llb_svc_state_str(s[i].state)));
    }
}

static void print_services(struct svc_info *s, int32_t n)
{
    struct col_widths w;
    compute_widths(s, n, &w);

    printf("%-*s  %-*s  %*s  %*s  %-*s  %-*s\n",
           w.svc_id, "SVC_ID", w.name, "NAME", w.port, "PORT",
           w.job_id, "JOB_ID", w.run_host, "RUN_HOST", w.state, "STATE");

    for (int i = 0; i < n; i++) {
        printf("%-*s  %-*s  %*d  %*ld  %-*s  %-*s\n",
               w.svc_id, s[i].svc_id, w.name, s[i].name,
               w.port, s[i].port, w.job_id, (long) s[i].job_id,
               w.run_host, s[i].run_host ? s[i].run_host : "-",
               w.state, llb_svc_state_str(s[i].state));
    }
}

static void usage(void)
{
    fprintf(stderr, "bservice: --help display this help and exit\n"
                    "  bservice NAME  start a service defined in llb.services\n"
                    "  -l, --list list configured services and their instances\n"
                    "  -s, --stop SVC_ID stop a running service instance\n"
                    "  --version output version information and exit\n");
}

static struct option longopts[] = {{"help", no_argument, NULL, 'h'},
                                   {"version", no_argument, NULL, 'v'},
                                   {"list", no_argument, NULL, 'l'},
                                   {"stop", required_argument, NULL, 's'},
                                   {NULL, 0, NULL, 0}};

int main(int argc, char **argv)
{
    const char *stop_svc_id = NULL;
    int list_fmt = 0;

    int cc;
    while ((cc = getopt_long(argc, argv, "hvls:", longopts, NULL)) != EOF) {
        switch (cc) {
        case 's':
            stop_svc_id = optarg;
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

    if (stop_svc_id) {
        int rc = llb_service_stop(stop_svc_id);
        if (rc != 0)
            fprintf(stderr, "bservice: %s: %m\n", stop_svc_id);
        else
            printf("service %s stopped\n", stop_svc_id);
        return rc;
    }

    if (list_fmt) {
        int32_t n;
        struct svc_info *s = llb_service_info(&n);
        if (!s) {
            fprintf(stderr, "bservice: failed\n");
            return -1;
        }
        print_services(s, n);
        llb_free_service_info(s, n);
        return 0;
    }

    if (optind >= argc) {
        usage();
        return -1;
    }

    const char *name = argv[optind];
    struct svc_info out;

    /* blocks until the backing job reaches RUNNING -- see
     * llb_service_start()/call_mbd_timeout()
     */
    int rc = llb_service_start(name, &out);
    if (rc != 0) {
        fprintf(stderr, "bservice: %s: %m\n", name);
        return rc;
    }

    printf("http://%s:%d\n", out.run_host, out.port);
    return 0;
}
