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

#define HIST_JOB_BUCKETS 10

struct job_hist {
    int64_t              job_id;
    uid_t                uid;
    int                  all;
    struct job_hist_info *jobs;
    int32_t              num_jobs;
    int32_t              max_jobs;
};

static char *hist_strdup(const char *s)
{
    if (s == NULL)
        s = "";
    return strdup(s);
}

static char *hist_trim(char *s)
{
    char *e;

    while (*s == ' ' || *s == '\t')
        s++;

    e = s + strlen(s);
    while (e > s) {
        if (e[-1] != ' ' && e[-1] != '\t' && e[-1] != '\r' && e[-1] != '\n')
            break;
        e--;
    }
    *e = 0;

    return s;
}

/* -----------------------------------------------------------------------
 * job_event array
 * ----------------------------------------------------------------------- */

static void event_free(struct job_event *e)
{
    free(e->from_queue);
    free(e->to_queue);
    free(e->run_hosts);
}

static struct job_event *event_add(struct job_hist_info *j)
{
    struct job_event *n;

    n = realloc(j->events, (j->num_events + 1) * sizeof(struct job_event));
    if (n == NULL)
        return NULL;

    j->events = n;
    memset(&j->events[j->num_events], 0, sizeof(struct job_event));
    j->num_events++;

    return &j->events[j->num_events - 1];
}

/* -----------------------------------------------------------------------
 * job_hist_info
 * ----------------------------------------------------------------------- */

static void hist_free_one(struct job_hist_info *j)
{
    int32_t i;

    free(j->username);
    free(j->name);
    free(j->queue);
    free(j->project);
    free(j->submit_host);
    free(j->machines);
    free(j->cwd);
    free(j->command);
    free(j->depend_cond);
    free(j->in_file);
    free(j->out_file);
    free(j->err_file);
    free(j->comment);
    free(j->gpu_type);
    free(j->tokenpool);

    for (i = 0; i < j->num_events; i++)
        event_free(&j->events[i]);

    free(j->events);
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

static struct job_hist_info *hist_find(struct job_hist *jh, int64_t job_id)
{
    int32_t i;

    for (i = 0; i < jh->num_jobs; i++) {
        if (jh->jobs[i].job_id == job_id)
            return &jh->jobs[i];
    }

    return NULL;
}

/* -----------------------------------------------------------------------
 * Submit sidecar
 *
 * Field ownership:
 *   JOB_NEW event owns: username, name, queue, project
 *   sidecar owns:       command, cwd, in_file, out_file, err_file, comment
 * ----------------------------------------------------------------------- */

static int hist_job_sidecar_path(char *path, size_t size,
                                 int64_t job_id, const char *file)
{
    int n;

    n = snprintf(path, size, "%s/mbd/jobs/%d/%ld/%s",
                 ll_params[LL_STATE_DIR].val,
                 (int)(job_id % HIST_JOB_BUCKETS), job_id, file);
    if (n < 0 || n >= (int)size)
        return -1;

    return 0;
}

static void hist_apply_submit_field(struct job_hist_info *j,
                                    const char *key, const char *val)
{
    if (strcasecmp(key, "command") == 0 && j->command == NULL) {
        j->command = hist_strdup(val);
        return;
    }
    if (strcasecmp(key, "cwd") == 0 && j->cwd == NULL) {
        j->cwd = hist_strdup(val);
        return;
    }
    if (strcasecmp(key, "submit_host") == 0 && j->submit_host == NULL) {
        j->submit_host = hist_strdup(val);
        return;
    }
    if (strcasecmp(key, "machines") == 0 && j->machines == NULL) {
        j->machines = hist_strdup(val);
        return;
    }
    if (strcasecmp(key, "depend_cond") == 0 && j->depend_cond == NULL) {
        j->depend_cond = hist_strdup(val);
        return;
    }
    if (strcasecmp(key, "in_file") == 0 && j->in_file == NULL) {
        j->in_file = hist_strdup(val);
        return;
    }
    if (strcasecmp(key, "out_file") == 0 && j->out_file == NULL) {
        j->out_file = hist_strdup(val);
        return;
    }
    if (strcasecmp(key, "err_file") == 0 && j->err_file == NULL) {
        j->err_file = hist_strdup(val);
        return;
    }
    if (strcasecmp(key, "comment") == 0 && j->comment == NULL) {
        j->comment = hist_strdup(val);
        return;
    }
    if (strcasecmp(key, "gpu_type") == 0 && j->gpu_type == NULL) {
        j->gpu_type = hist_strdup(val);
        return;
    }
    if (strcasecmp(key, "tokenpool") == 0 && j->tokenpool == NULL) {
        j->tokenpool = hist_strdup(val);
        return;
    }
    if (strcasecmp(key, "begin_time") == 0 && j->begin_time == 0) {
        j->begin_time = (time_t)strtoll(val, NULL, 10);
        return;
    }
    if (strcasecmp(key, "term_time") == 0 && j->term_time == 0) {
        j->term_time = (time_t)strtoll(val, NULL, 10);
        return;
    }
}

static void hist_load_sidecar(struct job_hist_info *j, const char *file)
{
    char path[PATH_MAX];
    char line[4096];
    FILE *fp;
    char *eq, *key, *val;

    if (hist_job_sidecar_path(path, sizeof(path), j->job_id, file) < 0)
        return;

    fp = fopen(path, "r");
    if (fp == NULL)
        return;

    while (fgets(line, sizeof(line), fp) != NULL) {
        eq = strchr(line, '=');
        if (eq == NULL)
            continue;
        *eq = 0;
        key = hist_trim(line);
        val = hist_trim(eq + 1);
        hist_apply_submit_field(j, key, val);
    }

    fclose(fp);
}

static void hist_load_usage_sidecar(struct job_hist_info *j)
{
    char path[PATH_MAX];
    char line[256];
    FILE *fp;
    char *eq, *key, *val;

    if (hist_job_sidecar_path(path, sizeof(path), j->job_id, "usage") < 0)
        return;

    fp = fopen(path, "r");
    if (fp == NULL)
        return;

    while (fgets(line, sizeof(line), fp) != NULL) {
        eq = strchr(line, '=');
        if (eq == NULL)
            continue;
        *eq = 0;
        key = hist_trim(line);
        val = hist_trim(eq + 1);

        if (strcasecmp(key, "mem_mb") == 0)
            j->usage.mem_mb = (uint64_t)strtoull(val, NULL, 10);
        else if (strcasecmp(key, "swap_mb") == 0)
            j->usage.swap_mb = (uint64_t)strtoull(val, NULL, 10);
        else if (strcasecmp(key, "cpu_time") == 0)
            j->usage.cpu_time = atof(val);
    }

    fclose(fp);
}

/* -----------------------------------------------------------------------
 * hist_add
 * ----------------------------------------------------------------------- */

static struct job_hist_info *hist_add(struct job_hist *jh,
                                      const struct log_job_new *e)
{
    struct job_hist_info *n;
    struct job_hist_info *j;

    if (jh->num_jobs == jh->max_jobs) {
        int32_t new_max = jh->max_jobs + 64;

        n = realloc(jh->jobs, new_max * sizeof(struct job_hist_info));
        if (n == NULL)
            return NULL;

        jh->jobs = n;
        memset(&jh->jobs[jh->max_jobs], 0, 64 * sizeof(struct job_hist_info));
        jh->max_jobs = new_max;
    }

    j = &jh->jobs[jh->num_jobs];
    memset(j, 0, sizeof(*j));

    j->job_id      = e->job_id;
    j->uid         = e->uid;
    j->state       = e->state;
    j->priority    = e->priority;
    j->submit_time = e->submit_time;
    j->num_cpus    = e->num_cpu;
    j->num_hosts   = e->num_hosts;
    j->num_gpus    = e->num_gpus;
    j->mem_mb      = e->mem_mb;
    j->storage_mb  = e->storage_mb;

    j->username = hist_strdup(e->username);
    j->name     = hist_strdup(e->job_name);
    j->queue    = hist_strdup(e->queue);
    j->project  = hist_strdup(e->project_name);

    if (j->username == NULL || j->name == NULL ||
        j->queue == NULL || j->project == NULL) {
        hist_free_one(j);
        memset(j, 0, sizeof(*j));
        return NULL;
    }

    hist_load_sidecar(j, "submit");
    hist_load_usage_sidecar(j);

    jh->num_jobs++;

    return j;
}

/* -----------------------------------------------------------------------
 * Event handlers.
 *
 * Events for a job that survives multiple compactions repeat identically
 * across archives -- same type, same timestamp. The handlers are
 * idempotent: applying the same event twice has no visible effect.
 * ----------------------------------------------------------------------- */

static int hist_match_new(struct job_hist *jh, const struct log_job_new *e)
{
    if (jh->all)
        return 1;

    if (jh->job_id > 0)
        return e->job_id == jh->job_id;

    return e->uid == jh->uid;
}

static int hist_event_exists(struct job_hist_info *j, int32_t type,
                             time_t event_time)
{
    int32_t i;

    for (i = 0; i < j->num_events; i++) {
        if (j->events[i].type == type &&
            j->events[i].event_time == event_time)
            return 1;
    }

    return 0;
}

static void hist_apply_new(struct job_hist *jh, const struct event_rec *rec)
{
    struct log_job_new e;
    struct job_hist_info *j;

    memset(&e, 0, sizeof(e));

    if (log_parse_job_new(rec, &e) < 0)
        return;

    if (!hist_match_new(jh, &e))
        return;

    j = hist_find(jh, e.job_id);
    if (j != NULL) {
       /* repeated JOB_NEW from compact checkpoint -- only update
         * state if job hasn't progressed beyond pending/held yet
         */
        if (j->state == JOB_PENDING || j->state == JOB_HELD)
            j->state = e.state;
        j->submit_time = e.submit_time;
        return;
    }

    hist_add(jh, &e);
}

static void hist_apply_start(struct job_hist *jh, const struct event_rec *rec)
{
    struct log_job_start e;
    struct job_hist_info *j;
    struct job_event *ev;

    memset(&e, 0, sizeof(e));

    if (log_parse_job_start(rec, &e) < 0)
        return;

    j = hist_find(jh, e.job_id);
    if (j == NULL)
        return;

    if (hist_event_exists(j, EVENT_JOB_START, rec->event_time))
        return;

    ev = event_add(j);
    if (ev == NULL)
        return;

    j->state          = JOB_RUNNING;
    ev->type          = EVENT_JOB_START;
    ev->event_time    = rec->event_time;
    ev->state         = JOB_RUNNING;
    ev->run_hosts    = hist_strdup(e.hosts);
}

static void hist_apply_fork(struct job_hist *jh, const struct event_rec *rec)
{
    struct log_job_fork e;
    struct job_hist_info *j;
    struct job_event *ev;

    memset(&e, 0, sizeof(e));

    if (log_parse_job_fork(rec, &e) < 0)
        return;

    j = hist_find(jh, e.job_id);
    if (j == NULL)
        return;

    if (hist_event_exists(j, EVENT_JOB_FORK, rec->event_time))
        return;

    ev = event_add(j);
    if (ev == NULL)
        return;

    ev->type       = EVENT_JOB_FORK;
    ev->event_time = rec->event_time;
    ev->pid        = (pid_t)e.job_pid;
}

static void hist_apply_signal(struct job_hist *jh, const struct event_rec *rec)
{
    struct log_job_signal e;
    struct job_hist_info *j;
    struct job_event *ev;

    memset(&e, 0, sizeof(e));

    if (log_parse_job_signal(rec, &e) < 0)
        return;

    j = hist_find(jh, e.job_id);
    if (j == NULL)
        return;

    if (hist_event_exists(j, EVENT_JOB_SIGNAL, rec->event_time))
        return;

    ev = event_add(j);
    if (ev == NULL)
        return;

    ev->type       = EVENT_JOB_SIGNAL;
    ev->event_time = rec->event_time;
    ev->signal     = e.signal_num;
}

static void hist_apply_finish(struct job_hist *jh, const struct event_rec *rec)
{
    struct log_job_finish e;
    struct job_hist_info *j;
    struct job_event *ev;

    memset(&e, 0, sizeof(e));

    if (log_parse_job_finish(rec, &e) < 0)
        return;

    j = hist_find(jh, e.job_id);
    if (j == NULL)
        return;

    if (hist_event_exists(j, EVENT_JOB_FINISH, rec->event_time))
        return;

    ev = event_add(j);
    if (ev == NULL)
        return;

    j->state       = e.state;
    j->uid         = e.uid;
    ev->type       = EVENT_JOB_FINISH;
    ev->event_time = rec->event_time;
    ev->state      = e.state;
    ev->exit_status = e.exit_status;
}

static void hist_apply_pend_susp(struct job_hist *jh,
                                 const struct event_rec *rec)
{
    struct log_job_pend_susp e;
    struct job_hist_info *j;
    struct job_event *ev;

    memset(&e, 0, sizeof(e));

    if (log_parse_job_pend_susp(rec, &e) < 0)
        return;

    j = hist_find(jh, e.job_id);
    if (j == NULL)
        return;

    if (hist_event_exists(j, EVENT_JOB_PEND_SUSP, rec->event_time))
        return;

    ev = event_add(j);
    if (ev == NULL)
        return;

    j->state       = JOB_HELD;
    ev->type       = EVENT_JOB_PEND_SUSP;
    ev->event_time = rec->event_time;
    ev->state      = JOB_HELD;
}

static void hist_apply_pend_resume(struct job_hist *jh,
                                   const struct event_rec *rec)
{
    struct log_job_pend_resume e;
    struct job_hist_info *j;
    struct job_event *ev;

    memset(&e, 0, sizeof(e));

    if (log_parse_job_pend_resume(rec, &e) < 0)
        return;

    j = hist_find(jh, e.job_id);
    if (j == NULL)
        return;

    if (hist_event_exists(j, EVENT_JOB_PEND_RESUME, rec->event_time))
        return;

    ev = event_add(j);
    if (ev == NULL)
        return;

    j->state       = JOB_PENDING;
    ev->type       = EVENT_JOB_PEND_RESUME;
    ev->event_time = rec->event_time;
    ev->state      = JOB_PENDING;
}

static void hist_apply_susp(struct job_hist *jh, const struct event_rec *rec)
{
    struct log_job_susp e;
    struct job_hist_info *j;
    struct job_event *ev;

    memset(&e, 0, sizeof(e));

    if (log_parse_job_susp(rec, &e) < 0)
        return;

    j = hist_find(jh, e.job_id);
    if (j == NULL) {
        return;
    }

    if (hist_event_exists(j, EVENT_JOB_SUSP, rec->event_time))
        return;

    ev = event_add(j);
    if (ev == NULL)
        return;

    j->state       = JOB_SUSPENDED;
    ev->type       = EVENT_JOB_SUSP;
    ev->event_time = rec->event_time;
    ev->state      = JOB_SUSPENDED;
}

static void hist_apply_move(struct job_hist *jh, const struct event_rec *rec)
{
    struct log_job_move e;
    struct job_hist_info *j;
    struct job_event *ev;

    memset(&e, 0, sizeof(e));

    if (log_parse_job_move(rec, &e) < 0)
        return;

    j = hist_find(jh, e.job_id);
    if (j == NULL)
        return;

    if (hist_event_exists(j, EVENT_JOB_MOVE, rec->event_time))
        return;

    ev = event_add(j);
    if (ev == NULL)
        return;

    free(j->queue);
    j->queue       = hist_strdup(e.to_queue);
    ev->type       = EVENT_JOB_MOVE;
    ev->event_time = rec->event_time;
    ev->from_queue = hist_strdup(e.from_queue);
    ev->to_queue   = hist_strdup(e.to_queue);
}

static void hist_apply_priority(struct job_hist *jh, const struct event_rec *rec)
{
    struct log_job_priority e;
    struct job_hist_info *j;
    struct job_event *ev;

    memset(&e, 0, sizeof(e));

    if (log_parse_job_priority(rec, &e) < 0)
        return;

    j = hist_find(jh, e.job_id);
    if (j == NULL)
        return;

    if (hist_event_exists(j, EVENT_JOB_PRIORITY, rec->event_time))
        return;

    ev = event_add(j);
    if (ev == NULL)
        return;

    ev->type         = EVENT_JOB_PRIORITY;
    ev->event_time   = rec->event_time;
    ev->old_priority = e.old_priority;
    ev->new_priority = e.new_priority;
    j->priority = e.new_priority;
}


static void hist_apply_pend(struct job_hist *jh, const struct event_rec *rec)
{
    struct log_job_pend e;
    struct job_hist_info *j;
    struct job_event *ev;

    memset(&e, 0, sizeof(e));

    if (log_parse_job_pend(rec, &e) < 0)
        return;

    j = hist_find(jh, e.job_id);
    if (j == NULL)
        return;

    if (hist_event_exists(j, EVENT_JOB_PEND, rec->event_time))
        return;

    ev = event_add(j);
    if (ev == NULL)
        return;

    j->state       = JOB_PENDING;
    ev->type       = EVENT_JOB_PEND;
    ev->event_time = rec->event_time;
    ev->state      = JOB_PENDING;
}

static void hist_apply_event(struct job_hist *jh, const struct event_rec *rec)
{
    if (rec->type == EVENT_JOB_NEW) {
        hist_apply_new(jh, rec);
        return;
    }
    if (rec->type == EVENT_JOB_START) {
        hist_apply_start(jh, rec);
        return;
    }
    if (rec->type == EVENT_JOB_FORK) {
        hist_apply_fork(jh, rec);
        return;
    }
    if (rec->type == EVENT_JOB_SIGNAL) {
        hist_apply_signal(jh, rec);
        return;
    }
    if (rec->type == EVENT_JOB_FINISH) {
        hist_apply_finish(jh, rec);
        return;
    }
    if (rec->type == EVENT_JOB_PEND_SUSP) {
        hist_apply_pend_susp(jh, rec);
        return;
    }
    if (rec->type == EVENT_JOB_PEND_RESUME) {
        hist_apply_pend_resume(jh, rec);
        return;
    }
    if (rec->type == EVENT_JOB_SUSP) {
        hist_apply_susp(jh, rec);
        return;
    }
    if (rec->type == EVENT_JOB_MOVE) {
        hist_apply_move(jh, rec);
        return;
    }
    if (rec->type == EVENT_JOB_PRIORITY) {
        hist_apply_priority(jh, rec);
        return;
    }
    if (rec->type == EVENT_JOB_PEND) {
        hist_apply_pend(jh, rec);
        return;
    }
}

/* -----------------------------------------------------------------------
 * Event file scanning.
 *
 * All eventlog files live in the same directory. Scan them all in
 * readdir order; duplicate events across archives are deduplicated
 * by hist_event_exists() via type + timestamp.
 * ----------------------------------------------------------------------- */

static int hist_scan_file(struct job_hist *jh, const char *path)
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
        hist_apply_event(jh, &rec);
    }

    fclose(fp);
    return 0;
}

static int hist_is_eventlog(const char *name)
{
    const char *p;

    if (strncmp(name, "eventlog", 8) != 0)
        return 0;

    p = name + 8;
    if (*p == '\0')
        return 1;

    if (*p != '.')
        return 0;

    p++;
    if (*p == '\0')
        return 0;

    while (*p != '\0') {
        if (*p < '0' || *p > '9')
            return 0;
        p++;
    }

    return 1;
}

static int hist_scan_events(struct job_hist *jh)
{
    char dir[PATH_MAX];
    char path[PATH_MAX];
    DIR *dp;
    struct dirent *de;
    int n;

    n = snprintf(dir, sizeof(dir), "%s/mbd", ll_params[LL_STATE_DIR].val);
    if (n < 0 || n >= (int)sizeof(dir))
        return -1;

    dp = opendir(dir);
    if (dp == NULL) {
        if (errno == ENOENT)
            return 0;
        return -1;
    }

    while ((de = readdir(dp)) != NULL) {
        if (!hist_is_eventlog(de->d_name))
            continue;

        n = snprintf(path, sizeof(path), "%s/%s", dir, de->d_name);
        if (n < 0 || n >= (int)sizeof(path)) {
            closedir(dp);
            return -1;
        }

        if (hist_scan_file(jh, path) < 0 && errno != ENOENT) {
            closedir(dp);
            return -1;
        }
    }

    closedir(dp);

    return 0;
}

static int hist_job_cmp(const void *a, const void *b)
{
    const struct job_hist_info *ja = (const struct job_hist_info *)a;
    const struct job_hist_info *jb = (const struct job_hist_info *)b;

    if (ja->job_id < jb->job_id)
        return -1;
    if (ja->job_id > jb->job_id)
        return  1;
    return 0;
}

/* -----------------------------------------------------------------------
 * Public API
 * ----------------------------------------------------------------------- */

struct job_hist_info *llb_hist_info(int64_t job_id, uid_t uid,
                                    int32_t flags, int32_t *num)
{
    struct job_hist jh;

    if (num == NULL)
        return NULL;

    *num = 0;

    if (job_id <= 0 && !(flags & LLB_HIST_ALL) && uid == (uid_t)-1)
        return NULL;

    memset(&jh, 0, sizeof(jh));
    jh.job_id = job_id;
    jh.uid    = uid;
    if (flags & LLB_HIST_ALL)
        jh.all = 1;

    errno = 0;

    if (ll_init() < 0) {
        errno = EINVAL;
        return NULL;
    }

    if (hist_scan_events(&jh) < 0) {
        llb_free_hist_info(jh.jobs, jh.num_jobs);
        return NULL;
    }

    if (jh.num_jobs == 0) {
        free(jh.jobs);
        errno = 0;
        return NULL;
    }

    qsort(jh.jobs, jh.num_jobs, sizeof(struct job_hist_info), hist_job_cmp);

    *num = jh.num_jobs;
    return jh.jobs;
}
