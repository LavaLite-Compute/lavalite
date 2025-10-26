#define _GNU_SOURCE
#include "accel_flux.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* ---- Optional: your minimal syslog wrapper (replace if you already have logging.h) ---- */
#include <syslog.h>
static void log_init_once(void) {
    static int inited;
    if (!inited) { openlog("lavalite-accel-flux", LOG_PID, LOG_DAEMON); inited = 1; }
}
#define LOGI(fmt, ...) do { log_init_once(); syslog(LOG_INFO,    fmt, ##__VA_ARGS__); } while (0)
#define LOGW(fmt, ...) do { log_init_once(); syslog(LOG_WARNING, fmt, ##__VA_ARGS__); } while (0)
#define LOGE(fmt, ...) do { log_init_once(); syslog(LOG_ERR,     fmt, ##__VA_ARGS__); } while (0)
/* --------------------------------------------------------------------------------------- */

/* Build-time switch: prefer libflux if present; else fallback to CLI. */
#if defined(HAVE_LIBFLUX)

#include <flux/core.h>
#include <flux/job.h>

struct accel_flux {
    flux_t *h;
    int     use_cli_fallback; /* set to 0 when libflux path is active */
};

struct accel_flux *accel_flux_init(const char *endpoint)
{
    struct accel_flux *ctx = calloc(1, sizeof(*ctx));
    if (!ctx) { LOGE("calloc accel_flux: %m"); return NULL; }

    /* endpoint may be NULL. flux_open(NULL, 0) picks default instance. */
    ctx->h = flux_open(endpoint, 0);
    if (!ctx->h) {
        LOGE("flux_open failed (endpoint=%s): %m", endpoint ? endpoint : "(default)");
        free(ctx);
        return NULL;
    }
    ctx->use_cli_fallback = 0;
    LOGI("Flux accelerator initialized (libflux), endpoint=%s",
         endpoint ? endpoint : "(default)");
    return ctx;
}

void accel_flux_fini(struct accel_flux *ctx)
{
    if (!ctx) return;
    if (ctx->h) flux_close(ctx->h);
    free(ctx);
}

/* Small helper: build a minimal RFC-39 jobspec (JSON). */
static char *build_jobspec(char *const argv[], int ntasks, int cores_pt,
                           char *const env_kv[], const char *workdir)
{
    /* Command array → JSON array of strings. */
    char cmd[2048] = {0};
    {
        size_t off = 0;
        off += snprintf(cmd + off, sizeof(cmd) - off, "[");
        for (int i = 0; argv && argv[i]; i++) {
            const char *s = argv[i];
            /* naive JSON string escaper for quotes/backslashes */
            char esc[1024]; size_t eoff = 0;
            for (const unsigned char *p=(const unsigned char*)s; *p && eoff < sizeof(esc)-2; ++p) {
                if (*p == '\"' || *p == '\\') esc[eoff++]='\\', esc[eoff++]=*p;
                else esc[eoff++]=*p;
            }
            esc[eoff] = 0;
            off += snprintf(cmd + off, sizeof(cmd) - off, "%s\"%s\"", (i? ",":""),
                            esc);
        }
        off += snprintf(cmd + off, sizeof(cmd) - off, "]");
    }

    /* Env → JSON object */
    char env[2048] = {0};
    {
        size_t off = 0;
        off += snprintf(env + off, sizeof(env) - off, "{");
        for (int i = 0; env_kv && env_kv[i]; i++) {
            const char *kv = env_kv[i];
            const char *eq = strchr(kv, '=');
            if (!eq) continue;
            size_t klen = (size_t)(eq - kv);
            char key[256]; if (klen >= sizeof(key)) klen = sizeof(key) - 1;
            memcpy(key, kv, klen); key[klen] = 0;
            const char *val = eq + 1;

            /* escape quotes/backslashes in key & val */
            char kesc[512]={0}, vesc[1024]={0};
            size_t ko=0, vo=0;
            for (const unsigned char *p=(const unsigned char*)key; *p && ko<sizeof(kesc)-2; ++p) {
                if (*p == '\"' || *p == '\\') kesc[ko++]='\\', kesc[ko++]=*p;
                else kesc[ko++]=*p;
            }
            kesc[ko]=0;
            for (const unsigned char *p=(const unsigned char*)val; *p && vo<sizeof(vesc)-2; ++p) {
                if (*p == '\"' || *p == '\\') vesc[vo++]='\\', vesc[vo++]=*p;
                else vesc[vo++]=*p;
            }
            vesc[vo]=0;

            off += snprintf(env + off, sizeof(env) - off, "%s\"%s\":\"%s\"",
                            (i? ",":""), kesc, vesc);
        }
        off += snprintf(env + off, sizeof(env) - off, "}");
    }

    /* jobspec JSON (Flux RFC-39/40-ish minimal subset) */
    char *json = NULL;
    int nt = ntasks > 0 ? ntasks : 1;
    int cp = cores_pt > 0 ? cores_pt : 1;
    const char *wd = (workdir && *workdir) ? workdir : NULL;

    if (wd) {
        if (asprintf(&json,
            "{"
              "\"tasks\": %d,"
              "\"cores_per_task\": %d,"
              "\"command\": %s,"
              "\"cwd\":\"%s\","
              "\"environment\": %s"
            "}",
            nt, cp, cmd, wd, env) < 0) json = NULL;
    } else {
        if (asprintf(&json,
            "{"
              "\"tasks\": %d,"
              "\"cores_per_task\": %d,"
              "\"command\": %s,"
              "\"environment\": %s"
            "}",
            nt, cp, cmd, env) < 0) json = NULL;
    }
    return json;
}

int accel_flux_submit(struct accel_flux *ctx,
                      char *const cmd_argv[],
                      int ntasks, int cores_pt,
                      char *const env_kv[],
                      const char *workdir,
                      accel_jobid_t *out_jobid)
{
    if (!ctx || !ctx->h || !cmd_argv || !cmd_argv[0] || !out_jobid) {
        errno = EINVAL; return -1;
    }

    char *jobspec = build_jobspec(cmd_argv, ntasks, cores_pt, env_kv, workdir);
    if (!jobspec) { errno = ENOMEM; return -1; }

    flux_future_t *f = flux_job_submit(ctx->h, jobspec, 0);
    free(jobspec);
    if (!f) {
        LOGE("flux_job_submit failed: %m");
        return -1;
    }

    flux_jobid_t jid = 0;
    if (flux_job_submit_get_id(f, &jid) < 0) {
        LOGE("flux_job_submit_get_id failed: %m");
        flux_future_destroy(f);
        return -1;
    }
    flux_future_destroy(f);
    *out_jobid = (accel_jobid_t)jid;
    LOGI("submitted Flux job %" PRIu64, (uint64_t)*out_jobid);
    return 0;
}

int accel_flux_status(struct accel_flux *ctx,
                      accel_jobid_t jobid,
                      char *buf, size_t bufsz)
{
    if (!ctx || !ctx->h || !buf || bufsz == 0) { errno = EINVAL; return -1; }

    flux_future_t *fi = flux_job_info(ctx->h, (flux_jobid_t)jobid, 0);
    if (!fi) { LOGE("flux_job_info(%" PRIu64 ") failed: %m", (uint64_t)jobid); return -1; }

    const char *state = NULL;
    if (flux_job_info_get_state(fi, &state) < 0 || !state) {
        LOGE("flux_job_info_get_state failed: %m");
        flux_future_destroy(fi);
        return -1;
    }
    /* Copy safely */
    snprintf(buf, bufsz, "%s", state);
    flux_future_destroy(fi);
    return 0;
}

int accel_flux_cancel(struct accel_flux *ctx, accel_jobid_t jobid)
{
    if (!ctx || !ctx->h) { errno = EINVAL; return -1; }
    flux_future_t *fk = flux_job_kill(ctx->h, (flux_jobid_t)jobid, SIGTERM, 0);
    if (!fk) { LOGE("flux_job_kill(%" PRIu64 ") failed: %m", (uint64_t)jobid); return -1; }
    flux_future_destroy(fk);
    LOGI("killed Flux job %" PRIu64, (uint64_t)jobid);
    return 0;
}

#else /* ---- CLI fallback (no libflux) ------------------------------------- */

#include <sys/wait.h>

struct accel_flux {
    char endpoint_env[256];  /* e.g., FLUX_URI=local:///run/flux */
    int  use_cli_fallback;   /* always 1 in this branch */
};

static int run_cmd_capture(char *const argv[], char *out, size_t outsz)
{
    int fds[2];
    if (pipe(fds) != 0) return -1;
    pid_t pid = fork();
    if (pid < 0) { close(fds[0]); close(fds[1]); return -1; }
    if (pid == 0) {
        /* child */
        dup2(fds[1], STDOUT_FILENO);
        dup2(fds[1], STDERR_FILENO);
        close(fds[0]); close(fds[1]);
        execvp(argv[0], argv);
        _exit(127);
    }
    /* parent */
    close(fds[1]);
    ssize_t n = read(fds[0], out, outsz - 1);
    if (n < 0) n = 0;
    out[n] = 0;
    close(fds[0]);
    int status = 0;
    (void)waitpid(pid, &status, 0);
    return (WIFEXITED(status) && WEXITSTATUS(status) == 0) ? 0 : -1;
}

struct accel_flux *accel_flux_init(const char *endpoint)
{
    struct accel_flux *ctx = calloc(1, sizeof(*ctx));
    if (!ctx) { LOGE("calloc accel_flux: %m"); return NULL; }
    ctx->use_cli_fallback = 1;
    if (endpoint && *endpoint) {
        snprintf(ctx->endpoint_env, sizeof(ctx->endpoint_env), "FLUX_URI=%s", endpoint);
        putenv(ctx->endpoint_env);
    }
    LOGI("Flux accelerator initialized (CLI), endpoint=%s",
         endpoint ? endpoint : "(default)");
    return ctx;
}

void accel_flux_fini(struct accel_flux *ctx)
{
    free(ctx);
}

int accel_flux_submit(struct accel_flux *ctx,
                      char *const cmd_argv[],
                      int ntasks, int cores_pt,
                      char *const env_kv[],
                      const char *workdir,
                      accel_jobid_t *out_jobid)
{
    (void)env_kv;
    if (!ctx || !cmd_argv || !cmd_argv[0] || !out_jobid) { errno = EINVAL; return -1; }

    /* Build: flux submit --ntasks N --cores-per-task C --cwd <dir> -- <argv...> */
    char cmd[4096] = {0};
    size_t off = 0;
    off += snprintf(cmd+off, sizeof(cmd)-off, "flux submit ");
    if (ntasks > 0) off += snprintf(cmd+off, sizeof(cmd)-off, "--ntasks %d ", ntasks);
    if (cores_pt > 0) off += snprintf(cmd+off, sizeof(cmd)-off, "--cores-per-task %d ", cores_pt);
    if (workdir && *workdir) off += snprintf(cmd+off, sizeof(cmd)-off, "--cwd '%s' ", workdir);
    off += snprintf(cmd+off, sizeof(cmd)-off, "-- ");
    for (int i = 0; cmd_argv[i]; i++)
        off += snprintf(cmd+off, sizeof(cmd)-off, "%s%s", (i? " ":""), cmd_argv[i]);

    char *argv[] = { "/bin/sh", "-lc", cmd, NULL };
    char out[1024];
    if (run_cmd_capture(argv, out, sizeof(out)) != 0) {
        LOGE("flux submit failed: %s", out);
        return -1;
    }
    /* Expect output contains a jobid; typical is like "ƒ12345" or numeric; extract digits. */
    char *p = out; accel_jobid_t jid = 0;
    while (*p && (jid == 0)) {
        if (*p >= '0' && *p <= '9') {
            jid = strtoull(p, NULL, 10);
            break;
        }
        ++p;
    }
    if (jid == 0) { LOGE("could not parse jobid from: %s", out); errno = EPROTO; return -1; }
    *out_jobid = jid;
    LOGI("submitted Flux job %" PRIu64 " (CLI)", (uint64_t)jid);
    return 0;
}

int accel_flux_status(struct accel_flux *ctx,
                      accel_jobid_t jobid,
                      char *buf, size_t bufsz)
{
    if (!ctx || !buf || bufsz == 0) { errno = EINVAL; return -1; }
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "flux jobs -no %llu | awk 'NR==1{print $2}'",
             (unsigned long long)jobid);
    char *argv[] = { "/bin/sh", "-lc", cmd, NULL };
    char out[256];
    if (run_cmd_capture(argv, out, sizeof(out)) != 0) {
        LOGE("flux jobs failed: %s", out);
        return -1;
    }
    /* Trim */
    size_t len = strcspn(out, "\r\n\t ");
    if (len >= bufsz) len = bufsz - 1;
    memcpy(buf, out, len);
    buf[len] = 0;
    return 0;
}

int accel_flux_cancel(struct accel_flux *ctx, accel_jobid_t jobid)
{
    if (!ctx) { errno = EINVAL; return -1; }
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "flux cancel %llu", (unsigned long long)jobid);
    char *argv[] = { "/bin/sh", "-lc", cmd, NULL };
    char out[256];
    if (run_cmd_capture(argv, out, sizeof(out)) != 0) {
        LOGE("flux cancel failed: %s", out);
        return -1;
    }
    LOGI("killed Flux job %" PRIu64 " (CLI)", (uint64_t)jobid);
    return 0;
}
#endif /* HAVE_LIBFLUX */

gcc -Wall -Wextra -O2 accel_flux.c -o test_flux

/* test.c */
#include "accel_flux.h"
#include <stdio.h>

int main(void) {
    struct accel_flux *fx = accel_flux_init(NULL);
    if (!fx) { perror("init"); return 1; }

    char *argv[] = { "bash", "-lc", "echo hi && sleep 2", NULL };
    accel_jobid_t jid = 0;
    if (accel_flux_submit(fx, argv, 1, 1, NULL, NULL, &jid) != 0) {
        perror("submit"); return 1;
    }
    printf("submitted %" PRIu64 "\n", (uint64_t)jid);

    char st[32];
    if (accel_flux_status(fx, jid, st, sizeof(st)) == 0)
        printf("status: %s\n", st);

    /* optional: accel_flux_cancel(fx, jid); */
    accel_flux_fini(fx);
    return 0;
}
