/*
 * Copyright (C) LavaLite Contributors
 *
 * bsub - submit a batch job to LavaLite.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <getopt.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <pwd.h>
#include <sys/types.h>

#include "llbatch.h"

static void usage(FILE *f)
{
    fprintf(
        f,
        "Usage: bsub [options] command [arguments]\n"
        "\n"
        "Job identity:\n"
        "  --queue  queue     Queue name (default: system default)\n"
        "  --name   name      Job name\n"
        "  --project name     Project name\n"
        "  --comment text     User comment, ignored by scheduler\n"
        "\n"
        "Resources (per host):\n"
        "  --cpus   n         CPU per host (default: 1)\n"
        "  --nhosts n         number of execution hosts (default: 1)\n"
        "  --mem    size      Memory per host: n[K|M|G] (cgroup enforced)\n"
        "  --storage size     Local scratch storage per host: n[K|M|G]\n"
        "  --gpus   n         GPUs per host (default: 0)\n"
        "  --gpu-type name    Required GPU type (requires --gpus)\n"
        "  --exclusive        Exclusive host, no job sharing\n"
        "\n"
        "Cluster resources:\n"
        "  --pool   name=N    Request N tokens from named pool (repeatable)\n"
        "Placement:\n"
        "  --machines \"h...\"  Restrict to listed hosts or host groups\n"
        "\n"
        "I/O:\n"
        "  --stdout path      Standard output (default: stdout.<jobid>)\n"
        "  --stderr path      Standard error  (default: stderr.<jobid>)\n"
        "  --stdin  path      Standard input  (default: /dev/null)\n"
        "\n"
        "Scheduling:\n"
        "  --hold             Submit in PSUSP state\n"
        "  --begin  [day:]h:m Do not dispatch before this time\n"
        "  --terminate [d:]h:m Terminate at deadline (SIGUSR2 + kill)\n"
        "\n"
        "  --help             Print this message and exit\n"
        "  --version          Print version and exit\n");
}

/*
 * Parse [[day:]hour:]minute into an absolute time_t.
 * At least hour:minute must be given.
 */
static int parse_time_arg(const char *arg, time_t *out)
{
    int fields[4];
    int nf = 0;
    char buf[64];
    char *p;

    if (strlen(arg) >= sizeof(buf)) {
        return -1;
    }
    strcpy(buf, arg);

    for (p = buf; *p; p++) {
        if (*p == ':') {
            nf++;
        }
    }
    nf++;

    if (nf < 2 || nf > 4) {
        return -1;
    }

    int i = 0;
    p = buf;
    while (p != NULL && i < nf) {
        char *colon = strchr(p, ':');
        if (colon) {
            *colon = '\0';
        }
        char *end;
        long v = strtol(p, &end, 10);
        if (*end != '\0' || v < 0) {
            return -1;
        }
        fields[i++] = (int) v;
        p = colon ? colon + 1 : NULL;
    }

    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    tm->tm_sec = 0;
    tm->tm_min = fields[nf - 1];
    tm->tm_hour = fields[nf - 2];
    if (nf >= 3) {
        tm->tm_mday = fields[nf - 3];
    }
    if (nf == 4) {
        tm->tm_mon = fields[0] - 1;
    }
    tm->tm_isdst = -1;

    time_t t = mktime(tm);
    if (t < 0) {
        return -1;
    }

    if (t <= now) {
        if (nf == 2) {
            t += 86400;
        } else if (nf == 3) {
            tm->tm_mon++;
            t = mktime(tm);
        } else {
            tm->tm_year++;
            t = mktime(tm);
        }
    }

    *out = t;
    return 0;
}

/*
 * Parse memory string: n[K|M|G] -> MB.
 * Plain integer is MB.
 */
static int parse_mem(const char *arg, uint64_t *out)
{
    char *end;
    unsigned long long v = strtoull(arg, &end, 10);

    if (end == arg) {
        return -1;
    }

    if (*end == '\0' || strcmp(end, "M") == 0) {
        *out = (uint64_t) v;
    } else if (strcmp(end, "K") == 0) {
        *out = (uint64_t) v / 1024;
        if (*out == 0) {
            *out = 1;
        }
    } else if (strcmp(end, "G") == 0) {
        *out = (uint64_t) v * 1024;
    } else {
        return -1;
    }

    return 0;
}

/*
 * Build the command string from remaining argv.
 */
static char *build_command(int argc, char **argv, int idx)
{
    size_t len = 0;
    int i;

    for (i = idx; i < argc; i++) {
        len += strlen(argv[i]) + 1;
    }

    if (len == 0) {
        return NULL;
    }

    char *cmd = malloc(len + 1);
    if (cmd == NULL) {
        return NULL;
    }

    cmd[0] = '\0';
    for (i = idx; i < argc; i++) {
        if (i > idx) {
            strcat(cmd, " ");
        }
        strcat(cmd, argv[i]);
    }

    return cmd;
}

static int count_machines(const char *machines)
{
    int n = 0;
    const char *p = machines;

    while (*p != '\0') {
        while (*p == ',' || *p == ' ')
            p++;
        if (*p != '\0') {
            n++;
            while (*p != '\0' && *p != ',' && *p != ' ')
                p++;
        }
    }
    return n;
}

int main(int argc, char **argv)
{
    struct job_submit js;
    int64_t job_id;
    int rc;

    memset(&js, 0, sizeof(js));

    static const struct option opts[] = {
        {"queue", required_argument, NULL, 'q'},
        {"name", required_argument, NULL, 'J'},
        {"project", required_argument, NULL, 'P'},
        {"comment", required_argument, NULL, 'C'},
        {"cpus", required_argument, NULL, 'n'},
        {"nhosts", required_argument, NULL, 'N'},
        {"mem", required_argument, NULL, 'M'},
        {"scratch", required_argument, NULL, 's'},
        {"gpus", required_argument, NULL, 'g'},
        {"gpu-type", required_argument, NULL, 'G'},
        {"pool", required_argument, NULL, 'L'},
        {"exclusive", no_argument, NULL, 'x'},
        {"machines", required_argument, NULL, 'm'},
        {"stdout", required_argument, NULL, 'o'},
        {"stderr", required_argument, NULL, 'e'},
        {"stdin", required_argument, NULL, 'i'},
        {"hold", no_argument, NULL, 'H'},
        {"begin", required_argument, NULL, 'b'},
        {"terminate", required_argument, NULL, 't'},
        {"dependency", required_argument, NULL, 'w'},
        {"help", no_argument, NULL, 'h'},
        {"version", no_argument, NULL, 'V'},
        {NULL, 0, NULL, 0}};

    int c;
    while (
        (c = getopt_long(argc, argv, "q:J:P:C:n:N:M:s:g:G:L:xm:o:e:i:Hb:t:W:hV",
                         opts, NULL)) != -1) {
        switch (c) {
        case 'q':
            js.queue = optarg;
            break;
        case 'J':
            js.name = optarg;
            break;
        case 'P':
            js.project = optarg;
            break;
        case 'C':
            js.comment = optarg;
            break;
        case 'n': {
            char *end;
            long v = strtol(optarg, &end, 10);
            if (*end != '\0' || v <= 0) {
                fprintf(stderr, "bsub: --cpus: invalid value '%s'\n", optarg);
                return 1;
            }
            js.num_cpus = (int32_t) v;
            break;
        }
        case 'N': {
            char *end;
            long v = strtol(optarg, &end, 10);
            if (*end != '\0' || v <= 0) {
                fprintf(stderr, "bsub: --nhosts: invalid value '%s'\n", optarg);
                return 1;
            }
            js.num_hosts = (int32_t) v;
            break;
        }
        case 'M':
            if (parse_mem(optarg, &js.mem_mb) < 0) {
                fprintf(stderr, "bsub: --mem: invalid value '%s'\n", optarg);
                return 1;
            }
            break;
        case 's':
            if (parse_mem(optarg, &js.storage_mb) < 0) {
                fprintf(stderr, "bsub: --scratch: invalid value '%s'\n",
                        optarg);
                return 1;
            }
            break;
        case 'g': {
            char *end;
            long v = strtol(optarg, &end, 10);
            if (*end != '\0' || v < 0) {
                fprintf(stderr, "bsub: --gpus: invalid value '%s'\n", optarg);
                return 1;
            }
            js.num_gpus = (int32_t) v;
            break;
        }
        case 'G':
            js.gpu_type = optarg;
            break;
        case 'L': {
            /* validate format: name=N */
            char *eq = strchr(optarg, '=');
            if (eq == NULL || eq == optarg) {
                fprintf(stderr,
                        "bsub: --pool: invalid format '%s', "
                        "expected name=N\n",
                        optarg);
                return 1;
            }
            char *end;
            long v = strtol(eq + 1, &end, 10);
            if (*end != '\0' || v <= 0) {
                fprintf(stderr, "bsub: --pool: invalid count in '%s'\n",
                        optarg);
                return 1;
            }
            /* append to tokenpool string: "existing,name=N" */
            if (js.tokenpool == NULL) {
                js.tokenpool = strdup(optarg);
            } else {
                char *prev = js.tokenpool;
                if (asprintf(&js.tokenpool, "%s,%s", prev, optarg) < 0) {
                    fprintf(stderr, "bsub: out of memory\n");
                    free(prev);
                    return 1;
                }
                free(prev);
            }
            break;
        }
        case 'x':
            js.flags |= JOB_FLAG_EXCLUSIVE;
            break;
        case 'm':
            js.machines = optarg;
            break;
        case 'o':
            js.out_file = optarg;
            break;
        case 'e':
            js.err_file = optarg;
            break;
        case 'i':
            js.in_file = optarg;
            break;
        case 'H':
            js.flags |= JOB_FLAG_HOLD;
            break;
        case 'b':
            if (parse_time_arg(optarg, &js.begin_time) < 0) {
                fprintf(stderr, "bsub: --begin: invalid time '%s'\n", optarg);
                return 1;
            }
            break;
        case 't':
            if (parse_time_arg(optarg, &js.term_time) < 0) {
                fprintf(stderr, "bsub: --terminate: invalid time '%s'\n",
                        optarg);
                return 1;
            }
            break;
        case 'w':
            // TODO
            js.depend_cond = optarg;
            break;
        case 'h':
            usage(stdout);
            return 0;
        case 'V':
            fprintf(stdout, "LavaLite %s\n", LAVALITE_VERSION_STR);
            return 0;
        default:
            usage(stderr);
            return 1;
        }
    }

    /* --nhosts and --machines are mutually exclusive */
    if (js.num_hosts > 0 && js.machines != NULL) {
        fprintf(stderr,
                "bsub: --nhosts and --machines are mutually exclusive\n");
        return 1;
    }

    if (js.machines != NULL)
        js.num_hosts = count_machines(js.machines);

    /* --gpu-type requires --gpus */
    if (js.gpu_type != NULL && js.num_gpus == 0) {
        fprintf(stderr, "bsub: --gpu-type requires --gpus\n");
        return 1;
    }

    /* begin must be before terminate */
    if (js.begin_time > 0 && js.term_time > 0) {
        if (js.begin_time >= js.term_time) {
            fprintf(stderr, "bsub: --begin must be before --terminate\n");
            return 1;
        }
    }

    js.command = build_command(argc, argv, optind);
    if (js.command == NULL) {
        fprintf(stderr, "bsub: no command specified\n");
        usage(stderr);
        return 1;
    }

    rc = llb_submit(&js, &job_id);
    if (rc != 0) {
        fprintf(stderr, "Job not submitted: %m\n");
        free(js.command);
        free(js.tokenpool);
        return 1;
    }

    fprintf(stdout, "Job <%ld> is submitted to queue <%s>.\n", (long) job_id,
            js.queue ? js.queue : "default");

    free(js.command);
    free(js.tokenpool);
    return 0;
}
