/*
 * Copyright (C) LavaLite Contributors
 * GPL v2
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
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
    int64_t         job_id;
    const char     *user;
    struct job_hist_info *jobs;
    int32_t         num_jobs;
    int32_t         max_jobs;
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
 * job_run
 * ----------------------------------------------------------------------- */

static void run_free(struct job_run *r)
{
    free(r->from_host);
    free(r->exec_hosts);
}

static struct job_run *run_add(struct job_hist_info *j)
{
    struct job_run *n;
    struct job_run *r;
    int32_t new_max;

    if (j->num_runs == j->max_runs) {
        new_max = j->max_runs + 4;
        n = realloc(j->runs, new_max * sizeof(struct job_run));
        if (n == NULL)
            return NULL;
        memset(&n[j->max_runs], 0, 4 * sizeof(struct job_run));
        j->runs = n;
        j->max_runs = new_max;
    }

    r = &j->runs[j->num_runs];
    memset(r, 0, sizeof(*r));
    r->run_seq = j->num_runs + 1;
    j->num_runs++;

    return r;
}

static struct job_run *run_current(struct job_hist_info *j)
{
    if (j->num_runs == 0)
        return NULL;
    return &j->runs[j->num_runs - 1];
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
    free(j->cwd);
    free(j->command);
    free(j->in_file);
    free(j->out_file);
    free(j->err_file);
    free(j->comment);

    for (i = 0; i < j->num_runs; i++)
        run_free(&j->runs[i]);

    free(j->runs);
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
 * submit sidecar
 *
 * Field ownership is strict:
 *   JOB_NEW event owns: username, name, queue, project
 *   sidecar owns:       command, cwd, in_file, out_file, err_file, comment
 *
 * hist_apply_submit_field handles sidecar-only fields.
 * Fields already set from JOB_NEW are silently skipped -- they are
 * present in the sidecar too but the event is the authoritative source.
 * ----------------------------------------------------------------------- */

static int hist_job_submit_path(char *path, size_t size, int64_t job_id)
{
    int n;

    n = snprintf(path, size, "%s/mbd/jobs/%d/%ld/submit",
                 ll_params[LL_STATE_DIR].val,
                 (int)(job_id % HIST_JOB_BUCKETS), job_id);
    if (n < 0 || n >= (int)size)
        return -1;

    return 0;
}

static void hist_apply_submit_field(struct job_hist_info *j,
                                    const char *key, const char *val)
{
    /* sidecar-only fields: not present in JOB_NEW */
    if (strcasecmp(key, "command") == 0) {
        assert(j->command == NULL);
        j->command = hist_strdup(val);
        return;
    }
    if (strcasecmp(key, "cwd") == 0) {
        assert(j->cwd == NULL);
        j->cwd = hist_strdup(val);
        return;
    }
    if (strcasecmp(key, "in_file") == 0) {
        assert(j->in_file == NULL);
        j->in_file = hist_strdup(val);
        return;
    }
    if (strcasecmp(key, "out_file") == 0) {
        assert(j->out_file == NULL);
        j->out_file = hist_strdup(val);
        return;
    }
    if (strcasecmp(key, "err_file") == 0) {
        assert(j->err_file == NULL);
        j->err_file = hist_strdup(val);
        return;
    }
    if (strcasecmp(key, "comment") == 0) {
        assert(j->comment == NULL);
        j->comment = hist_strdup(val);
        return;
    }
    /* name, queue, project, username: owned by JOB_NEW -- skip */
}

static void hist_load_submit_sidecar(struct job_hist_info *j)
{
    char path[PATH_MAX];
    char line[4096];
    FILE *fp;
    char *eq, *key, *val;

    if (hist_job_submit_path(path, sizeof(path), j->job_id) < 0)
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

/* -----------------------------------------------------------------------
 * hist_add: new job_hist_info slot.
 *
 * Scheduling/identity fields come from JOB_NEW (event log).
 * Display-only fields (command, cwd, files, comment) come from sidecar.
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
    j->submit_time = e->submit_time;
    j->num_cpus    = e->num_cpu;
    j->num_hosts   = e->num_hosts;
    j->num_gpus    = e->num_gpus;
    j->mem_mb      = e->mem_mb;
    j->storage_mb  = e->storage_mb;

    /* JOB_NEW owns these -- set here, never touched by sidecar loader */
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

    /* sidecar fills command, cwd, in_file, out_file, err_file, comment */
    hist_load_submit_sidecar(j);

    jh->num_jobs++;

    return j;
}

/* -----------------------------------------------------------------------
 * Event handlers
 * ----------------------------------------------------------------------- */

static int hist_match_new(struct job_hist *jh, const struct log_job_new *e)
{
    if (jh->job_id > 0)
        return e->job_id == jh->job_id;

    if (jh->user != NULL && jh->user[0] != '\0')
        return strcmp(e->username, jh->user) == 0;

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
        /* requeue: same job_id, new submission -- update state only */
        j->state = JOB_PENDING;
        j->submit_time = e.submit_time;
        return;
    }

    hist_add(jh, &e);
}

static void hist_apply_start(struct job_hist *jh, const struct event_rec *rec)
{
    struct log_job_start e;
    struct job_hist_info *j;
    struct job_run *r;

    memset(&e, 0, sizeof(e));

    if (log_parse_job_start(rec, &e) < 0)
        return;

    j = hist_find(jh, e.job_id);
    if (j == NULL)
        return;

    /*
     * A job live at compact time appears in both the archived chronological
     * log and the new compact checkpoint -- dedup by dispatch_time.
     */
    for (int32_t i = 0; i < j->num_runs; i++) {
        if (j->runs[i].dispatch_time == e.dispatch_time)
            return;
    }

    r = run_add(j);
    if (r == NULL)
        return;

    j->state         = JOB_RUNNING;
    r->state         = JOB_RUNNING;
    r->dispatch_time = e.dispatch_time;
    r->from_host     = hist_strdup(e.exec_host);
    r->exec_hosts    = hist_strdup(e.hosts);
}

static void hist_apply_fork(struct job_hist *jh, const struct event_rec *rec)
{
    struct log_job_fork e;
    struct job_hist_info *j;
    struct job_run *r;

    memset(&e, 0, sizeof(e));

    if (log_parse_job_fork(rec, &e) < 0)
        return;

    j = hist_find(jh, e.job_id);
    if (j == NULL)
        return;

    r = run_current(j);
    if (r == NULL)
        return;

    /* dedup: compact checkpoint repeats JOB_FORK for live jobs */
    if (r->fork_time != 0)
        return;

    r->pid       = (pid_t)e.job_pid;
    r->fork_time = e.fork_time;
}

static void hist_apply_finish(struct job_hist *jh, const struct event_rec *rec)
{
    struct log_job_finish e;
    struct job_hist_info *j;
    struct job_run *r;

    memset(&e, 0, sizeof(e));

    if (log_parse_job_finish(rec, &e) < 0)
        return;

    j = hist_find(jh, e.job_id);
    if (j == NULL)
        return;

    r = run_current(j);
    if (r == NULL)
        return;

    assert(r->end_time == 0);
    j->state       = e.state;
    j->uid         = e.uid;
    r->state       = e.state;
    r->exit_status = e.exit_status;
    r->end_time    = e.end_time;
}

static void hist_apply_pend_susp(struct job_hist *jh,
                                 const struct event_rec *rec)
{
    struct log_job_pend_susp e;
    struct job_hist_info *j;

    memset(&e, 0, sizeof(e));

    if (log_parse_job_pend_susp(rec, &e) < 0)
        return;

    j = hist_find(jh, e.job_id);
    if (j == NULL)
        return;

    j->state = JOB_HELD;
}

static void hist_apply_pend_resume(struct job_hist *jh,
                                   const struct event_rec *rec)
{
    struct log_job_pend_resume e;
    struct job_hist_info *j;

    memset(&e, 0, sizeof(e));

    if (log_parse_job_pend_resume(rec, &e) < 0)
        return;

    j = hist_find(jh, e.job_id);
    if (j == NULL)
        return;

    j->state = JOB_PENDING;
}

static void hist_apply_susp(struct job_hist *jh, const struct event_rec *rec)
{
    struct log_job_susp e;
    struct job_hist_info *j;
    struct job_run *r;

    memset(&e, 0, sizeof(e));

    if (log_parse_job_susp(rec, &e) < 0)
        return;

    j = hist_find(jh, e.job_id);
    if (j == NULL)
        return;

    r = run_current(j);
    if (r == NULL)
        return;

    j->state     = JOB_SUSPENDED;
    r->susp_time = e.event_time;
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
}

/* -----------------------------------------------------------------------
 * Event file scanning.
 *
 * sysevents is the mbd replay checkpoint: live pending/running jobs only.
 * Archives (sysevents.NNNNNN) contain finished job history, one job per
 * archive, immutable once written. No ordering dependency between archives.
 *
 * Open sysevents first to capture live state before any compact renames it.
 * Then readdir archives and scan in any order -- they are immutable.
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

    /*
     * Open sysevents first -- before readdir -- so a compact that races
     * us renames the file we already have open, not one we haven't seen.
     * ENOENT is fine: mbd not yet started or between compacts.
     */
    n = snprintf(path, sizeof(path), "%s/sysevents", dir);
    if (n < 0 || n >= (int)sizeof(path))
        return -1;

    if (hist_scan_file(jh, path) < 0 && errno != ENOENT)
        return -1;

    dp = opendir(dir);
    if (dp == NULL) {
        if (errno == ENOENT)
            return 0;
        return -1;
    }

    while ((de = readdir(dp)) != NULL) {
        if (!hist_is_archive(de->d_name))
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

/* -----------------------------------------------------------------------
 * Public API
 * ----------------------------------------------------------------------- */

struct job_hist_info *llb_hist_info(int64_t job_id, const char *user,
                                    int32_t *num)
{
    struct job_hist jh;

    if (num == NULL)
        return NULL;

    *num = 0;

    if (job_id <= 0 && (user == NULL || user[0] == '\0'))
        return NULL;

    memset(&jh, 0, sizeof(jh));
    jh.job_id = job_id;
    jh.user   = user;

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
        errno = 0;
        return NULL;
    }

    *num = jh.num_jobs;

    return jh.jobs;
}
