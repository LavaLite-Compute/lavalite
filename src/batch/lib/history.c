/*
 * Copyright (C) LavaLite Contributors
 * GPL v2
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <dirent.h>
#include <limits.h>
#include <unistd.h>

#include "llbatch.h"
#include "batch/lib/log.h"
#include "base/lib/ll.conf.h"
#include "base/lib/ll.bufsiz.h"

struct hist_ctx {
    int64_t job_id;
    const char *user;
    struct job_hist_info *jobs;
    int32_t num_jobs;
    int32_t max_jobs;
};

static void hist_free_one(struct job_hist_info *j)
{
    free(j->username);
    free(j->name);
    free(j->queue);
    free(j->project);
    free(j->from_host);
    free(j->exec_hosts);
    free(j->cwd);
    free(j->comment);
}

void llb_free_hist_info(struct job_hist_info *jobs, int32_t num_jobs)
{
    int32_t i;

    if (jobs == NULL)
        return;

    for (i = 0; i < num_jobs; i++)
        hist_free_one(&jobs[i]);

    free(jobs);
}

static char *hist_strdup(const char *s)
{
    if (s == NULL)
        s = "";

    return strdup(s);
}

static struct job_hist_info *hist_find(struct hist_ctx *ctx, int64_t job_id)
{
    int32_t i;

    for (i = 0; i < ctx->num_jobs; i++) {
        if (ctx->jobs[i].job_id == job_id)
            return &ctx->jobs[i];
    }

    return NULL;
}

static struct job_hist_info *hist_add(struct hist_ctx *ctx,
                                      const struct log_job_new *e)
{
    struct job_hist_info *n;
    struct job_hist_info *j;

    if (ctx->num_jobs == ctx->max_jobs) {
        int32_t new_max = ctx->max_jobs + 64;

        n = realloc(ctx->jobs, new_max * sizeof(struct job_hist_info));
        if (n == NULL)
            return NULL;

        ctx->jobs = n;
        memset(&ctx->jobs[ctx->max_jobs], 0,
               64 * sizeof(struct job_hist_info));
        ctx->max_jobs = new_max;
    }

    j = &ctx->jobs[ctx->num_jobs];
    memset(j, 0, sizeof(*j));

    j->job_id = e->job_id;
    j->uid = e->uid;
    j->state = e->state;
    j->submit_time = e->submit_time;

    j->num_cpus = e->num_cpu;
    j->num_hosts = e->num_hosts;
    j->num_gpus = e->num_gpus;
    j->usage.mem_mb = e->mem_mb;
    j->storage_mb = e->storage_mb;

    j->username = hist_strdup(e->username);
    j->name = hist_strdup(e->job_name);
    j->queue = hist_strdup(e->queue);
    j->project = hist_strdup(e->project_name);
    j->exec_hosts = hist_strdup("");

    if (j->username == NULL || j->name == NULL || j->queue == NULL ||
        j->project == NULL || j->exec_hosts == NULL) {
        hist_free_one(j);
        memset(j, 0, sizeof(*j));
        return NULL;
    }

    ctx->num_jobs++;

    return j;
}

static int hist_match_new(struct hist_ctx *ctx, const struct log_job_new *e)
{
    if (ctx->job_id > 0)
        return e->job_id == ctx->job_id;

    if (ctx->user != NULL && ctx->user[0] != '\0')
        return strcmp(e->username, ctx->user) == 0;

    return 0;
}

static void hist_apply_new(struct hist_ctx *ctx, const struct event_rec *rec)
{
    struct log_job_new e;

    memset(&e, 0, sizeof(e));

    if (log_parse_job_new(rec, &e) < 0)
        return;

    if (!hist_match_new(ctx, &e))
        return;

    if (hist_find(ctx, e.job_id) != NULL)
        return;

    hist_add(ctx, &e);
}

static void hist_apply_start(struct hist_ctx *ctx, const struct event_rec *rec)
{
    struct log_job_start e;
    struct job_hist_info *j;

    memset(&e, 0, sizeof(e));

    if (log_parse_job_start(rec, &e) < 0)
        return;

    j = hist_find(ctx, e.job_id);
    if (j == NULL)
        return;

    j->state = JOB_RUNNING;
    j->dispatch_time = e.dispatch_time;

    free(j->from_host);
    free(j->exec_hosts);

    j->from_host = hist_strdup(e.exec_host);
    j->exec_hosts = hist_strdup(e.hosts);

    if (j->from_host == NULL || j->exec_hosts == NULL) {
        free(j->from_host);
        free(j->exec_hosts);
        j->from_host = NULL;
        j->exec_hosts = NULL;
    }
}

static void hist_apply_fork(struct hist_ctx *ctx, const struct event_rec *rec)
{
    struct log_job_fork e;
    struct job_hist_info *j;

    memset(&e, 0, sizeof(e));

    if (log_parse_job_fork(rec, &e) < 0)
        return;

    j = hist_find(ctx, e.job_id);
    if (j == NULL)
        return;

    j->pid = (pid_t)e.job_pid;
    j->fork_time = e.fork_time;
}

static void hist_apply_execute(struct hist_ctx *ctx, const struct event_rec *rec)
{
    struct log_job_execute e;
    struct job_hist_info *j;

    memset(&e, 0, sizeof(e));

    if (log_parse_job_execute(rec, &e) < 0)
        return;

    j = hist_find(ctx, e.job_id);
    if (j == NULL)
        return;

    j->execute_time = e.execute_time;

    free(j->cwd);
    j->cwd = hist_strdup(e.cwd);
}

static void hist_apply_finish(struct hist_ctx *ctx, const struct event_rec *rec)
{
    struct log_job_finish e;
    struct job_hist_info *j;

    memset(&e, 0, sizeof(e));

    if (log_parse_job_finish(rec, &e) < 0)
        return;

    j = hist_find(ctx, e.job_id);
    if (j == NULL)
        return;

    j->uid = e.uid;
    j->state = e.state;
    j->exit_status = e.exit_status;
    j->end_time = e.end_time;
}

static void hist_apply_pend_susp(struct hist_ctx *ctx,
                                 const struct event_rec *rec)
{
    struct log_job_pend_susp e;
    struct job_hist_info *j;

    memset(&e, 0, sizeof(e));

    if (log_parse_job_pend_susp(rec, &e) < 0)
        return;

    j = hist_find(ctx, e.job_id);
    if (j == NULL)
        return;

    j->state = JOB_HELD;
    j->susp_time = e.event_time;
}

static void hist_apply_pend_resume(struct hist_ctx *ctx,
                                   const struct event_rec *rec)
{
    struct log_job_pend_resume e;
    struct job_hist_info *j;

    memset(&e, 0, sizeof(e));

    if (log_parse_job_pend_resume(rec, &e) < 0)
        return;

    j = hist_find(ctx, e.job_id);
    if (j == NULL)
        return;

    j->state = JOB_PENDING;
}

static void hist_apply_susp(struct hist_ctx *ctx, const struct event_rec *rec)
{
    struct log_job_susp e;
    struct job_hist_info *j;

    memset(&e, 0, sizeof(e));

    if (log_parse_job_susp(rec, &e) < 0)
        return;

    j = hist_find(ctx, e.job_id);
    if (j == NULL)
        return;

    j->state = JOB_SUSPENDED;
    j->susp_time = e.event_time;
}

static void hist_apply_event(struct hist_ctx *ctx, const struct event_rec *rec)
{
    if (rec->type == EVENT_JOB_NEW) {
        hist_apply_new(ctx, rec);
        return;
    }

    if (rec->type == EVENT_JOB_START) {
        hist_apply_start(ctx, rec);
        return;
    }

    if (rec->type == EVENT_JOB_FORK) {
        hist_apply_fork(ctx, rec);
        return;
    }

    if (rec->type == EVENT_JOB_EXECUTE) {
        hist_apply_execute(ctx, rec);
        return;
    }

    if (rec->type == EVENT_JOB_FINISH) {
        hist_apply_finish(ctx, rec);
        return;
    }

    if (rec->type == EVENT_JOB_PEND_SUSP) {
        hist_apply_pend_susp(ctx, rec);
        return;
    }

    if (rec->type == EVENT_JOB_PEND_RESUME) {
        hist_apply_pend_resume(ctx, rec);
        return;
    }

    if (rec->type == EVENT_JOB_SUSP) {
        hist_apply_susp(ctx, rec);
        return;
    }
}

static int hist_scan_file(struct hist_ctx *ctx, const char *path)
{
    FILE *fp;
    struct event_rec rec;

    fp = fopen(path, "r");
    if (fp == NULL)
        return -1;

    while (1) {
        memset(&rec, 0, sizeof(rec));

        if (log_read_hdr(fp, &rec) < 0)
            break;

        hist_apply_event(ctx, &rec);
    }

    fclose(fp);
    return 0;
}
static int hist_is_archive(const char *name)
{
    const char *p;

    if (strncmp(name, "sysevents.", 10) != 0)
        return 0;

    p = name + 10;
    if (*p == '\0')
        return 0;

    while (*p != '\0') {
        if (*p < '0' || *p > '9')
            return 0;
        p++;
    }

    return 1;
}

static int hist_name_cmp(const void *a, const void *b)
{
    const char * const *aa = a;
    const char * const *bb = b;

    return strcmp(*aa, *bb);
}

static void hist_free_names(char **names, int count)
{
    int i;

    if (names == NULL)
        return;

    for (i = 0; i < count; i++)
        free(names[i]);

    free(names);
}

static int hist_add_name(char ***names, int *count, int *max, const char *name)
{
    char **n;

    if (*count == *max) {
        int new_max = *max + 64;

        n = realloc(*names, new_max * sizeof(char *));
        if (n == NULL)
            return -1;

        *names = n;
        *max = new_max;
    }

    (*names)[*count] = strdup(name);
    if ((*names)[*count] == NULL)
        return -1;

    (*count)++;

    return 0;
}

static int hist_scan_events(struct hist_ctx *ctx)
{
    char dir[PATH_MAX];
    char path[PATH_MAX];
    DIR *dp;
    struct dirent *de;
    char **names = NULL;
    int count = 0;
    int max = 0;
    int i;
    int n;

    n = snprintf(dir, sizeof(dir), "%s/mbd", ll_params[LL_STATE_DIR].val);
    if (n < 0 || n >= (int)sizeof(dir))
        return -1;

    dp = opendir(dir);
    if (dp == NULL)
        return -1;

    while ((de = readdir(dp)) != NULL) {
        if (!hist_is_archive(de->d_name))
            continue;

        if (hist_add_name(&names, &count, &max, de->d_name) < 0) {
            closedir(dp);
            hist_free_names(names, count);
            return -1;
        }
    }

    closedir(dp);

    qsort(names, count, sizeof(char *), hist_name_cmp);

    for (i = 0; i < count; i++) {
        n = snprintf(path, sizeof(path), "%s/%s", dir, names[i]);
        if (n < 0 || n >= (int)sizeof(path)) {
            hist_free_names(names, count);
            return -1;
        }

        hist_scan_file(ctx, path);
    }

    hist_free_names(names, count);

    n = snprintf(path, sizeof(path), "%s/sysevents", dir);
    if (n < 0 || n >= (int)sizeof(path))
        return -1;

    hist_scan_file(ctx, path);

    return 0;
}

struct job_hist_info *llb_hist_info(int64_t job_id, const char *user,
                                    int32_t *num)
{
    struct hist_ctx ctx;

    if (num == NULL)
        return NULL;

    *num = 0;

    if (job_id <= 0 && (user == NULL || user[0] == '\0'))
        return NULL;

    memset(&ctx, 0, sizeof(ctx));
    ctx.job_id = job_id;
    ctx.user = user;

    if (hist_scan_events(&ctx) < 0) {
        llb_free_hist_info(ctx.jobs, ctx.num_jobs);
        return NULL;
    }

    *num = ctx.num_jobs;

    return ctx.jobs;
}
