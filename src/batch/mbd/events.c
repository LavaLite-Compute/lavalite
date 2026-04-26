/*
 * Copyright (C) LavaLite Contributors
 * GPL v2
 */

#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <syslog.h>
#include <sys/stat.h>
#include <assert.h>

#include "batch/lib/wire.h"
#include "batch/lib/log.h"
#include "base/lib/ll.conf.h"
#include "base/lib/ll.syslog.h"
#include "batch/mbd/mbd.h"

static char events_path[PATH_MAX];
static char acct_path[PATH_MAX];
char jobs_dir[PATH_MAX];

/* -----------------------------------------------------------------------
 * event writers -- called by mbd to append to job.events
 * ----------------------------------------------------------------------- */

static FILE *open_events(void)
{
    FILE *fp = fopen(events_path, "a");
    if (fp == NULL) {
        LS_ERR("fopen=%s", events_path);
        mbd_die(MBD_EXIT_EVENTS);
    }
    return fp;
}

void event_job_new(const struct job_data *job, const struct wire_job_submit *ws)
{
    struct log_job_new e;
    memset(&e, 0, sizeof(e));

    e.job_id      = job->job_id;
    e.uid         = (uid_t)ws->uid;
    e.gid         = (gid_t)ws->gid;
    e.status      = job->status;
    e.submit_time = job->submit_time;
    e.begin_time  = (time_t)ws->begin_time;
    e.term_time   = (time_t)ws->term_time;
    e.num_cpu     = ws->num_cpus;
    e.num_hosts   = ws->num_nhosts;
    e.num_gpus    = ws->num_gpus;
    e.mem_mb      = ws->mem_mb;
    e.flags       = ws->flags;

    ll_strlcpy(e.username,     ws->username,  sizeof(e.username));
    ll_strlcpy(e.job_name,     ws->name,      sizeof(e.job_name));
    ll_strlcpy(e.queue,        ws->queue,     sizeof(e.queue));
    ll_strlcpy(e.project_name, ws->project,   sizeof(e.project_name));
    ll_strlcpy(e.gpu_type,     ws->gpu_type,  sizeof(e.gpu_type));
    ll_strlcpy(e.from_host,    ws->from_host, sizeof(e.from_host));
    ll_strlcpy(e.hosts,        ws->machines,  sizeof(e.hosts));
    ll_strlcpy(e.comment,      ws->comment,   sizeof(e.comment));

    FILE *fp = open_events();
    if (log_write_job_new(fp, &e) < 0) {
        fclose(fp);
        LS_ERRX("log_write_job_new failed job_id=%ld", job->job_id);
        mbd_die(MBD_EXIT_EVENTS);
    }
    fclose(fp);
}

void event_job_start(const struct job_data *job)
{
    struct log_job_start e;
    memset(&e, 0, sizeof(e));

    e.job_id = job->job_id;
    ll_strlcpy(e.exec_host, job->exec_host, sizeof(e.exec_host));

    FILE *fp = open_events();
    if (log_write_job_start(fp, &e) < 0) {
        fclose(fp);
        LS_ERRX("log_write_job_start failed job_id=%ld", job->job_id);
        mbd_die(MBD_EXIT_EVENTS);
    }
    fclose(fp);
}

void event_job_accept(const struct job_data *job)
{
    struct log_job_accept e;
    memset(&e, 0, sizeof(e));

    e.job_id  = job->job_id;
    e.job_pid = job->res.pid;

    FILE *fp = open_events();
    if (log_write_job_accept(fp, &e) < 0) {
        fclose(fp);
        LS_ERRX("log_write_job_accept failed job_id=%ld", job->job_id);
        mbd_die(MBD_EXIT_EVENTS);
    }
    fclose(fp);
}

void event_job_execute(const struct job_data *job, const char *cwd)
{
    struct log_job_execute e;
    memset(&e, 0, sizeof(e));

    e.job_id  = job->job_id;
    e.job_pid = job->res.pid;
    ll_strlcpy(e.cwd, cwd, sizeof(e.cwd));

    FILE *fp = open_events();
    if (log_write_job_execute(fp, &e) < 0) {
        fclose(fp);
        LS_ERRX("log_write_job_execute failed job_id=%ld", job->job_id);
        mbd_die(MBD_EXIT_EVENTS);
    }
    fclose(fp);
}

void event_job_signal(const struct job_data *job, const struct wire_job_sig *ws)
{
    struct log_job_signal e;
    memset(&e, 0, sizeof(e));

    e.job_id = job->job_id;
    e.signal_time = job->signal_time;
    e.signal_num = ws->sig;
    e.uid = ws->uid;

    FILE *fp = open_events();
    if (log_write_job_signal(fp, &e) < 0) {
        fclose(fp);
        LS_ERRX("log_write_job_signal failed job_id=%ld", job->job_id);
        mbd_die(MBD_EXIT_EVENTS);
    }
    fclose(fp);
}

void event_job_finish(const struct job_data *job)
{
    struct log_job_finish e;
    memset(&e, 0, sizeof(e));

    e.job_id      = job->job_id;
    e.uid         = job->uid;
    e.status = job->status;
    e.exit_status = job->exit_status;
    e.submit_time = job->submit_time;
    e.start_time  = job->start_time;
    e.end_time    = job->end_time;
    e.cpu_time    = job->res.cpu_time;

    ll_strlcpy(e.job_name,  job->name,       sizeof(e.job_name));
    ll_strlcpy(e.queue,     job->queue->name, sizeof(e.queue));
    ll_strlcpy(e.from_host, job->from_host,  sizeof(e.from_host));
    ll_strlcpy(e.exec_host, job->exec_host,  sizeof(e.exec_host));

    FILE *fp = open_events();
    if (log_write_job_finish(fp, &e) < 0) {
        fclose(fp);
        LS_ERRX("log_write_job_finish failed job_id=%ld", job->job_id);
        mbd_die(MBD_EXIT_EVENTS);
    }
    fclose(fp);
}

void event_job_pend_susp(const struct job_data *job)
{
    struct log_job_pend_susp e;
    memset(&e, 0, sizeof(e));
    e.job_id = job->job_id;

    FILE *fp = open_events();
    if (log_write_job_pend_susp(fp, &e) < 0) {
        fclose(fp);
        LS_ERRX("log_write_job_pend_susp failed job_id=%ld", job->job_id);
        mbd_die(MBD_EXIT_EVENTS);
    }
    fclose(fp);
}

void event_job_pend_resume(const struct job_data *job)
{
    struct log_job_pend_resume e;
    memset(&e, 0, sizeof(e));
    e.job_id = job->job_id;

    FILE *fp = open_events();
    if (log_write_job_pend_resume(fp, &e) < 0) {
        fclose(fp);
        LS_ERRX("log_write_job_resume failed job_id=%ld", job->job_id);
        mbd_die(MBD_EXIT_EVENTS);
    }
    fclose(fp);
}

void event_job_susp(const struct job_data *job)
{
    struct log_job_susp e;
    memset(&e, 0, sizeof(e));
    e.job_id = job->job_id;

    FILE *fp = open_events();
    if (log_write_job_susp(fp, &e) < 0) {
        fclose(fp);
        LS_ERRX("log_write_job_susp failed job_id=%ld", job->job_id);
        mbd_die(MBD_EXIT_EVENTS);
    }
    fclose(fp);
}

/* -----------------------------------------------------------------------
 * replay -- restore job state from job.events at startup.
 *
 * Single pass through the event log. Each event applies the same state
 * transition that happened live:
 *
 *   EVENT_JOB_NEW     alloc job_data from event + sidecar, insert pend list
 *   EVENT_JOB_START   move pend -> run, set exec_host
 *   EVENT_JOB_ACCEPT  set res.pid (barrier crossed)
 *   EVENT_JOB_EXECUTE set confirmed cwd in sidecar (no job_data field)
 *   EVENT_JOB_SIGNAL  no state change, informational only
 *   EVENT_JOB_FINISH  move run -> finish, set exit fields
 *
 * job_id_seq is set to max(job_id) seen so next_job_id() never collides.
 * Returns number of jobs restored, or -1 on hard error.
 * ----------------------------------------------------------------------- */

static struct job_data *replay_alloc(const struct log_job_new *e)
{
    struct job_data *job = calloc(1, sizeof(struct job_data));
    if (job == NULL) {
        LS_ERR("calloc failed");
        return NULL;
    }

    job->job_id      = e->job_id;
    job->uid         = e->uid;
    job->gid = e->gid;
    job->status      = JOB_STAT_PEND;
    job->submit_time = e->submit_time;
    job->begin_time  = e->begin_time;
    job->term_time   = e->term_time;
    job->num_cpus    = e->num_cpu;
    job->num_nhosts  = e->num_hosts;
    job->num_gpus    = e->num_gpus;
    job->mem_mb      = e->mem_mb;
    job->flags       = e->flags;

    ll_strlcpy(job->name, e->job_name, sizeof(job->name));
    ll_strlcpy(job->user, e->username, sizeof(job->user));
    ll_strlcpy(job->project, e->project_name, sizeof(job->project));
    ll_strlcpy(job->gpu_type, e->gpu_type, sizeof(job->gpu_type));
    ll_strlcpy(job->from_host, e->from_host, sizeof(job->from_host));
    ll_strlcpy(job->machines, e->hosts, sizeof(job->machines));
    ll_strlcpy(job->comment, e->comment, sizeof(job->comment));

    job->queue = ll_hash_search(&queue_name_hash, e->queue);
    if (job->queue == NULL) {
        LS_ERR("job_id=%ld queue='%s' not found, orphaned",
               e->job_id, e->queue);
        job->status = JOB_STAT_ORPHAN;
    }

    return job;
}

static int replay_insert(struct job_data *job)
{
    char key[LL_BUFSIZ_32];
    sprintf(key, "%ld", job->job_id);
    enum ll_hash_status hs = ll_hash_insert(&job_id_hash, key, job, 0);
    if (hs != LL_HASH_INSERTED) {
        LS_ERR("hash insert failed job_id=%ld", job->job_id);
        free(job);
        return -1;
    }
    job_set_list(job, &pend_jobs_list, JOB_LIST_PEND);
    return 0;
}

static int replay_job_new(const struct event_rec *rec, int64_t *max_id)
{
    struct log_job_new e;
    memset(&e, 0, sizeof(struct log_job_new));
    if (log_parse_job_new(rec, &e) < 0) {
        LS_ERR("parse JOB_NEW failed");
        return 0;
    }
    if (e.job_id > *max_id)
        *max_id = e.job_id;
    struct job_data *job = replay_alloc(&e);
    if (job == NULL)
        return 0;
    if (replay_insert(job) < 0)
        return 0;
    LS_DEBUG("JOB_NEW job_id=%ld", e.job_id);
    return 1;
}

static void replay_job_start(const struct event_rec *rec)
{
    struct log_job_start e;
    if (log_parse_job_start(rec, &e) < 0) {
        LS_ERRX("parse JOB_START failed");
        return;
    }
    struct job_data *job = job_find(e.job_id);
    if (job == NULL) {
        LS_ERRX("JOB_START job_id=%ld not found", e.job_id);
        return;
    }
    struct mbd_host *host = ll_hash_search(&host_name_hash, e.exec_host);
    if (host == NULL) {
        LS_ERR("JOB_START job_id=%ld exec_host=%s not found, orphaned",
               e.job_id, e.exec_host);
        job->status = JOB_STAT_ORPHAN;
        return;
    }
    job->status = JOB_STAT_RUN;
    ll_strlcpy(job->exec_host, e.exec_host, sizeof(job->exec_host));
    job_move_list(job, &pend_jobs_list, &run_jobs_list, JOB_LIST_RUN);
    LS_DEBUG("JOB_START job_id=%ld", e.job_id);
}

static void replay_job_accept(const struct event_rec *rec)
{
    struct log_job_accept e;
    if (log_parse_job_accept(rec, &e) < 0) {
        LS_ERRX("parse JOB_ACCEPT failed");
        return;
    }
    struct job_data *job = job_find(e.job_id);
    if (job == NULL) {
        LS_ERRX("JOB_ACCEPT job_id=%ld not found", e.job_id);
        return;
    }
    job->res.pid = (pid_t)e.job_pid;
    LS_DEBUG("JOB_ACCEPT job_id=%ld pid=%d", e.job_id, e.job_pid);
}

static void replay_job_execute(const struct event_rec *rec)
{
    /* cwd is in the sidecar; no field to update in job_data.
     * Parse to validate the record. */
    struct log_job_execute e;
    if (log_parse_job_execute(rec, &e) < 0) {
        LS_ERR("parse JOB_EXECUTE failed");
        return;
    }
    LS_DEBUG("JOB_EXECUTE job_id=%ld pid=%d", e.job_id, e.job_pid);
}

static void replay_job_signal(const struct event_rec *rec)
{
    /* informational only, no state change */
    struct log_job_signal e;
    if (log_parse_job_signal(rec, &e) < 0) {
        LS_ERR("parse JOB_SIGNAL failed");
        return;
    }
    LS_DEBUG("JOB_SIGNAL job_id=%ld sig=%d uid=%u at=%s",
             e.job_id, e.signal_num, e.uid, ctime2(&e.signal_time));
}

static void replay_job_finish(const struct event_rec *rec)
{
    struct log_job_finish e;
    memset(&e, 0, sizeof(struct log_job_finish));
    if (log_parse_job_finish(rec, &e) < 0) {
        LS_ERR("parse JOB_FINISH failed");
        return;
    }
    struct job_data *job = job_find(e.job_id);
    if (job == NULL) {
        LS_ERRX("JOB_FINISH job_id=%ld not found", e.job_id);
        return;
    }

    job->status       = e.status;
    job->exit_status  = e.exit_status;
    job->uid          = e.uid;
    job->submit_time  = e.submit_time;
    job->start_time   = e.start_time;
    job->end_time     = e.end_time;
    job->res.cpu_time = e.cpu_time;

    struct ll_list *from = &pend_jobs_list;
    if (job->list_id == JOB_LIST_RUN)
        from = &run_jobs_list;
    job_move_list(job, from, &finish_jobs_list, JOB_LIST_FINISH);
    LS_DEBUG("JOB_FINISH job_id=%ld", e.job_id);
}

static void replay_job_pend_susp(const struct event_rec *rec)
{
    struct log_job_pend_susp e;
    if (log_parse_job_pend_susp(rec, &e) < 0) {
        LS_ERR("parse JOB_PEND_SUSP failed");
        return;
    }
    struct job_data *job = job_find(e.job_id);
    if (job == NULL) {
        LS_ERRX("JOB_PEND_SUSP job_id=%ld not found", e.job_id);
        return;
    }
    if (!(job->status & JOB_STAT_PEND)) {
        LS_ERRX("JOB_PEND_SUSP job_id=%ld not in PEND", e.job_id);
        assert(0);
        return;
    }
    job->status = JOB_STAT_PSUSP;
    LS_DEBUG("JOB_PEND_SUSP job_id=%ld", e.job_id);
}

static void replay_job_pending_resume(const struct event_rec *rec)
{
    struct log_job_pend_resume e;
    if (log_parse_job_pend_resume(rec, &e) < 0) {
        LS_ERR("parse JOB_RESUME failed");
        return;
    }
    struct job_data *job = job_find(e.job_id);
    if (job == NULL) {
        LS_ERRX("JOB_RESUME job_id=%ld not found", e.job_id);
        return;
    }
    if (!(job->status & JOB_STAT_PSUSP)) {
        LS_ERRX("JOB_PENDING_RESUME job_id=%ld not in PEND_SUSP", e.job_id);
        assert(0);
        return;
    }
    job->status = JOB_STAT_PEND;
    LS_DEBUG("JOB_RESUME job_id=%ld", e.job_id);
}

static void replay_job_susp(const struct event_rec *rec)
{
    struct log_job_susp e;
    if (log_parse_job_susp(rec, &e) < 0) {
        LS_ERR("parse JOB_SUSP failed");
        return;
    }
    struct job_data *job = job_find(e.job_id);
    if (job == NULL) {
        LS_ERRX("JOB_SUSP job_id=%ld not found", e.job_id);
        return;
    }
    job->status = JOB_STAT_SUSP;
    LS_DEBUG("JOB_SUSP job_id=%ld", e.job_id);
}

void reopen_job_events(void)
{
}
int jobs_replay(void)
{
    FILE *fp = fopen(events_path, "r");
    if (fp == NULL) {
        if (errno == ENOENT) {
            // the very first start or admin wiped out
            LS_INFO("replay: no job.events, nothing to replay");
            return 0;
        }
        LS_ERR("fopen %s", events_path);
        mbd_die(MBD_EXIT_EVENTS);
    }

    int lineno = 0;
    int restored = 0;
    int64_t max_id = 0;

    for (;;) {
        ++lineno;
        struct event_rec rec;
        memset(&rec, 0, sizeof(struct event_rec));
        if (log_read_hdr(fp, &rec) < 0)
            break;
        if (rec.type == EVENT_JOB_NEW) {
            restored += replay_job_new(&rec, &max_id);
            continue;
        }
        if (rec.type == EVENT_JOB_START) {
            replay_job_start(&rec);
            continue;
        }
        if (rec.type == EVENT_JOB_ACCEPT) {
            replay_job_accept(&rec);
            continue;
        }
        if (rec.type == EVENT_JOB_EXECUTE) {
            replay_job_execute(&rec);
            continue;
        }
        if (rec.type == EVENT_JOB_SIGNAL) {
            replay_job_signal(&rec);
            continue;
        }
        if (rec.type == EVENT_JOB_FINISH) {
            replay_job_finish(&rec);
            continue;
        }
        if (rec.type == EVENT_JOB_PEND_SUSP) {
            replay_job_pend_susp(&rec);
            continue;
        }
        if (rec.type == EVENT_JOB_PEND_RESUME) {
            replay_job_pending_resume(&rec);
            continue;
        }
        if (rec.type == EVENT_JOB_SUSP) {
            replay_job_susp(&rec);
            continue;
        }
    }

    if (ferror(fp)) {
        fclose(fp);
        LS_ERR("I/O error reading job events=%s line=%d", events_path, lineno);
        mbd_die(MBD_EXIT_EVENTS);
    }

    fclose(fp);

    if (max_id > job_id_seq)
        job_id_seq = max_id;

    LS_INFO("replay: done, %d jobs restored, job_id_seq=%ld",
            restored, job_id_seq);
    return restored;
}
int events_init(void)
{
    char dir[PATH_MAX];
    int n = snprintf(dir, sizeof(dir), "%s/mbd",
                     ll_params[LL_STATE_DIR].val);
    if (n < 0 || n >= (int)sizeof(dir))
        mbd_die(MBD_EXIT_EVENTS);

    if (mkdir(dir, 0700) == -1 && errno != EEXIST) {
        syslog(LOG_ERR, "mkdir(%s) failed: %m", dir);
        mbd_die(MBD_EXIT_FATAL);
    }
    LS_INFO("working dir initialized %s", dir);

    n = snprintf(events_path, sizeof(events_path), "%s/sysevents", dir);
    if (n < 0 || n >= (int)sizeof(events_path))
        mbd_die(MBD_EXIT_EVENTS);
    LS_INFO("job events initialized %s", events_path);

    n = snprintf(acct_path, sizeof(acct_path), "%s/jobs.acct", dir);
    if (n < 0 || n >= (int)sizeof(acct_path))
        mbd_die(MBD_EXIT_EVENTS);
    LS_INFO("job accounts initialized %s", acct_path);

    n = snprintf(jobs_dir, sizeof(jobs_dir), "%s/jobs", dir);
    if (n < 0 || n >= (int)sizeof(jobs_dir))
        mbd_die(MBD_EXIT_EVENTS);

    if (mkdir(jobs_dir, 0700) == -1 && errno != EEXIST) {
        LS_ERRX("mkdir(%s) failed", jobs_dir);
        mbd_die(MBD_EXIT_EVENTS);
    }
    LS_INFO("job working dir initialized %s", jobs_dir);

    for (int i = 0; i < JOB_BUCKETS; i++) {
        char bucket[PATH_MAX];
        int nb = snprintf(bucket, sizeof(bucket), "%s/%d", jobs_dir, i);
        if (nb < 0 || nb >= (int)sizeof(bucket))
            mbd_die(MBD_EXIT_EVENTS);
        if (mkdir(bucket, 0700) == -1 && errno != EEXIST) {
            LS_ERRX("mkdir(%s) failed", bucket);
            mbd_die(MBD_EXIT_EVENTS);
        }
    }
    LS_INFO("%d bucket dirs initialized", JOB_BUCKETS);

    return 0;
}
