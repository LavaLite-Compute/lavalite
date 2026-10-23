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
#include <errno.h>
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

/* the endpoint exists only while the instance runs */
static void instance_endpoint(const struct svc_instance_info *inst,
                              char *buf, size_t bufsz)
{
    const char *host = inst->run_host;

    if (inst->status != SVC_RUNNING || host == NULL || host[0] == 0
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

static void print_instances(const struct svc_info *s, int all)
{
    struct inst_col_widths iw;
    if (compute_instance_widths(s, &iw, all) == 0)
        return;

    printf("  %-*s  %-*s  %*s  %-*s  %s\n",
           iw.user, "USER", iw.endpoint, "HOST:PORT",
           iw.job_id, "JOB_ID", iw.status, "STATUS", "TOKEN");

    for (uint32_t j = 0; j < s->ninstances; j++) {
        const struct svc_instance_info *inst = &s->instances[j];

        if (!all && inst->status == SVC_FINISH)
            continue;

        char uidbuf[32];
        char endpoint[MAXHOSTNAMELEN + 16];
        const char *user = uid_name(inst->uid, uidbuf, sizeof(uidbuf));

        instance_endpoint(inst, endpoint, sizeof(endpoint));

        printf("  %-*s  %-*s  %*ld  %-*s  %s\n",
               iw.user, user,
               iw.endpoint, endpoint,
               iw.job_id, inst->job_id,
               iw.status, llb_svc_status_str(inst->status),
               inst->token != NULL ? inst->token : "-");
    }
}

static void print_services(const struct svc_info *s, int32_t n, int all)
{
    struct svc_col_widths sw;
    compute_service_widths(s, n, &sw);

    printf("%-*s  %-*s  RESTART\n", sw.name, "NAME", sw.queue, "QUEUE");

    for (int32_t i = 0; i < n; i++) {
        printf("%-*s  %-*s  %s\n", sw.name, s[i].name, sw.queue, s[i].queue,
               s[i].restart_never ? "never" : "always");
        print_instances(&s[i], all);
    }
}

/* one block per service, like bqueues -l: the definition from
 * llb.services and the instance counts of all users
 */
static void print_services_long(const struct svc_info *s, int32_t n)
{
    for (int32_t i = 0; i < n; i++) {
        if (i > 0)
            printf("\n");
        printf("SERVICE: %s\n", s[i].name);
        printf("  Queue:       %s\n", s[i].queue);
        printf("  Type:        %s\n", s[i].daemon ? "daemon" : "user");
        if (s[i].port > 0)
            printf("  Port:        %d\n", s[i].port);
        else
            printf("  Port:        -\n");
        printf("  Restart:     %s\n", s[i].restart_never ? "never" : "always");
        if (s[i].run_limit > 0)
            printf("  Run limit:   %d:%02d\n", s[i].run_limit / 3600,
                   (s[i].run_limit % 3600) / 60);
        else
            printf("  Run limit:   -\n");
        printf("  Token env:   %s\n", s[i].token_env ? s[i].token_env : "-");
        printf("  Instances:   running: %d    pending: %d    finished: %d\n",
               s[i].num_run, s[i].num_pend, s[i].num_finish);
        /* last: the longest line, it would shadow the counts */
        printf("  Command:     %s\n", s[i].command);
    }
}

/* Move the services named on the command line to the front, in the
 * order given, and return how many. The array keeps all its entries,
 * so llb_free_service_info() still frees them all.
 */
static int select_services(struct svc_info *s, int n, char **names,
                           int nnames, int *bad)
{
    int m = 0;

    for (int k = 0; k < nnames; k++) {
        int i = 0;
        while (i < n && strcmp(s[i].name, names[k]) != 0)
            i++;
        if (i == n) {
            fprintf(stderr, "bservices: %s: no such service\n", names[k]);
            *bad = 1;
            continue;
        }
        if (i < m)
            continue;
        struct svc_info tmp = s[m];
        s[m] = s[i];
        s[i] = tmp;
        m++;
    }

    return m;
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
    fprintf(stderr,
            "Usage: bservices [-a] [-l] [NAME...]\n"
            "       bservices -s NAME [-t [h:]m]\n"
            "       bservices -d JOB_ID|HOST:PORT\n"
            "  -a, --all       include finished instances\n"
            "  -l, --long      one block per service, its definition\n"
            "  -s, --start     start an instance of service NAME\n"
            "  -t, --time      run limit, at most the service RUNLIMIT\n"
            "  -d, --delete    delete a service instance\n"
            "  -h, --help      display this help and exit\n"
            "  -v, --version   output version information and exit\n");
}

static struct option longopts[] = {
    {"help", no_argument, NULL, 'h'},
    {"version", no_argument, NULL, 'v'},
    {"all", no_argument, NULL, 'a'},
    {"long", no_argument, NULL, 'l'},
    {"start", required_argument, NULL, 's'},
    {"delete", required_argument, NULL, 'd'},
    {"time", required_argument, NULL, 't'},
    {NULL, 0, NULL, 0}
};

static int start_service(const char *name, int32_t run_limit)
{
    struct svc_instance_info out;
    memset(&out, 0, sizeof(out));

    /* blocks until the backing job reaches RUNNING -- see
     * llb_service_start()/call_mbd_timeout()
     */
    int rc = llb_service_start(name, run_limit, &out);
    if (rc != 0) {
        fprintf(stderr, "bservices: %s: %m\n", name);
        return rc;
    }

    if (out.token != NULL)
        printf("Job <%ld> http://%s:%d/?token=%s\n",
               out.job_id, out.run_host, out.port, out.token);
    else
        printf("Job <%ld> http://%s:%d\n",
               out.job_id, out.run_host, out.port);

    free(out.service);
    free(out.run_host);
    free(out.token);

    return 0;
}

static int delete_service(const char *arg)
{
    char host[MAXHOSTNAMELEN];
    int64_t job_id = 0;
    int port = 0;

    host[0] = 0;
    if (parse_job_id(arg, &job_id) < 0
        && parse_service_endpoint(arg, host, sizeof(host), &port) < 0) {
        fprintf(stderr, "bservices: invalid job id or endpoint: %s\n", arg);
        return -1;
    }

    int rc = llb_service_delete(job_id, host, port);
    if (rc != 0)
        fprintf(stderr, "bservices: %s: %m\n", arg);
    else
        printf("Service <%s> is being deleted\n", arg);

    return rc;
}

int main(int argc, char **argv)
{
    const char *delete_arg = NULL;
    const char *start_arg = NULL;
    int all = 0;
    int cc;
    int long_fmt = 0;
    int32_t run_limit = 0;

    while ((cc = getopt_long(argc, argv, "hvals:d:t:", longopts, NULL))
           != EOF) {
        switch (cc) {
        case 'l':
            long_fmt = 1;
            break;
        case 's':
            start_arg = optarg;
            break;
        case 't':
            if (llb_parse_time_limit(optarg, &run_limit) < 0) {
                fprintf(stderr, "bservices: --time: invalid limit '%s'\n",
                        optarg);
                return -1;
            }
            break;
        case 'd':
            delete_arg = optarg;
            break;
        case 'a':
            all = 1;
            break;
        case 'v':
            fprintf(stderr, "%s\n", LAVALITE_VERSION_STR);
            return 0;
        case 'h':
            usage();
            return 0;
        default:
            usage();
            return -1;
        }
    }

    /* one action at a time: start, delete or view */
    if ((start_arg && delete_arg)
        || ((start_arg || delete_arg)
            && (optind < argc || all || long_fmt))
        || (run_limit > 0 && start_arg == NULL)) {
        usage();
        return -1;
    }

    if (start_arg != NULL)
        return start_service(start_arg, run_limit);

    if (delete_arg != NULL)
        return delete_service(delete_arg);

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

    /* names select services, as bqueues and bhosts */
    int bad = 0;
    int nprint = nsvc;
    if (optind < argc)
        nprint = select_services(s, nsvc, &argv[optind], argc - optind, &bad);

    if (nprint > 0 && long_fmt)
        print_services_long(s, nprint);
    else if (nprint > 0)
        print_services(s, nprint, all);
    llb_free_service_info(s, nsvc);
    return bad;
}
