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
    int job_id;
    int endpoint;
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

static void instance_endpoint(const struct svc_instance_info *inst,
                              char *buf, size_t bufsz)
{
    const char *host = inst->run_host;

    if (host == NULL || host[0] == 0
        || strcmp(host, "-") == 0 || inst->port == 0)
        snprintf(buf, bufsz, "-");
    else
        snprintf(buf, bufsz, "%s:%d", host, inst->port);
}

static uint32_t compute_instance_widths(const struct svc_info *s,
                                       struct inst_col_widths *w, int all)
{
    uint32_t nvisible = 0;

    w->user = strlen("USER");
    w->endpoint = strlen("HOST:PORT");
    w->job_id = strlen("JOB_ID");
    w->status = strlen("STATUS");

    for (uint32_t i = 0; i < s->ninstances; i++) {
        const struct svc_instance_info *inst = &s->instances[i];

        if (!all && inst->status == SVC_FINISH)
            continue;

        char uidbuf[32];
        char endpoint[MAXHOSTNAMELEN + 16];
        const char *user = uid_name(inst->uid, uidbuf, sizeof(uidbuf));

        instance_endpoint(inst, endpoint, sizeof(endpoint));

        w->user = imax(w->user, strlen(user));
        w->endpoint = imax(w->endpoint, strlen(endpoint));
        w->job_id = imax(w->job_id, ndigits(inst->job_id));
        w->status = imax(w->status, strlen(llb_svc_status_str(inst->status)));
        nvisible++;
    }

    return nvisible;
}

static void print_services(const struct svc_info *s, int32_t n, int all)
{
    struct svc_col_widths sw;
    compute_service_widths(s, n, &sw);

    printf("%-*s  %-*s\n", sw.name, "NAME", sw.queue, "QUEUE");

    for (int32_t i = 0; i < n; i++) {
        printf("%-*s  %-*s\n", sw.name, s[i].name, sw.queue, s[i].queue);

        struct inst_col_widths iw;
        if (compute_instance_widths(&s[i], &iw, all) == 0)
            continue;

        printf("  %-*s  %-*s  %*s  %-*s\n",
               iw.user, "USER", iw.endpoint, "HOST:PORT",
               iw.job_id, "JOB_ID", iw.status, "STATUS");

        for (uint32_t j = 0; j < s[i].ninstances; j++) {
            const struct svc_instance_info *inst = &s[i].instances[j];

            if (!all && inst->status == SVC_FINISH)
                continue;

            char uidbuf[32];
            char endpoint[MAXHOSTNAMELEN + 16];
            const char *user = uid_name(inst->uid, uidbuf, sizeof(uidbuf));

            instance_endpoint(inst, endpoint, sizeof(endpoint));

            printf("  %-*s  %-*s  %*ld  %-*s\n",
                   iw.user, user,
                   iw.endpoint, endpoint,
                   iw.job_id, (long) inst->job_id,
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

/* bare digits: job_id, anything else: [http://]host:port */
static int parse_job_id(const char *s, int64_t *job_id)
{
    char *end;
    errno = 0;
    long long n = strtoll(s, &end, 10);
    if (errno == ERANGE || *end != '\0' || end == s || n <= 0)
        return -1;

    *job_id = (int64_t) n;
    return 0;
}

static void usage(void)
{
    fprintf(stderr, "bservices: --help display this help and exit\n"
            "  bservices NAME  start a service defined in llb.services\n"
            "  -a, --all list configured services and all retained instances\n"
            "  -d, --delete JOB_ID|HOST:PORT delete a service instance\n"
            "  --version output version information and exit\n");
}

static struct option longopts[] = {
    {"help", no_argument, NULL, 'h'},
    {"version", no_argument, NULL, 'v'},
    {"all", no_argument, NULL, 'a'},
    {"delete", required_argument, NULL, 'd'},
    {NULL, 0, NULL, 0}
};

int main(int argc, char **argv)
{
    const char *delete_arg = NULL;
    int all = 0;
    int cc;
    int list_fmt = 0;

    while ((cc = getopt_long(argc, argv, "hvad:", longopts, NULL)) != EOF) {
        switch (cc) {
        case 'd':
            delete_arg = optarg;
            break;
        case 'a':
            all = 1;
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

    if (delete_arg != NULL) {
        char host[MAXHOSTNAMELEN];
        int64_t job_id = 0;
        int port = 0;

        host[0] = 0;
        if (parse_job_id(delete_arg, &job_id) < 0
            && parse_service_endpoint(delete_arg, host, sizeof(host),
                                      &port) < 0) {
            fprintf(stderr, "bservices: invalid job id or endpoint: %s\n",
                    delete_arg);
            return -1;
        }

        int rc = llb_service_delete(job_id, host, port);
        if (rc != 0)
            fprintf(stderr, "bservices: %s: %m\n", delete_arg);
        else
            printf("service %s deleted\n", delete_arg);

        return rc;
    }

    if (list_fmt || optind >= argc) {
        int32_t nsvc = 0;
        struct svc_info *s = llb_service_info(&nsvc);
        if (s == NULL) {
            if (nsvc == 0) {
                printf("No services: %m\n");
                return 0;
            }
            fprintf(stderr, "bservices: failed\n");
            return -1;
        }

        print_services(s, nsvc, all);
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
        fprintf(stderr, "bservices: %s: %m\n", name);
        return rc;
    }

    printf("Job <%ld>: http://%s:%d\n",
           (long) out.job_id, out.run_host, out.port);

    free(out.service);
    free(out.run_host);

    return 0;
}
