/*
 * Copyright (C) LavaLite Contributors
 * GPL v2
 */

#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <syslog.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <assert.h>

#include "batch/lib/wire.h"
#include "batch/lib/log.h"
#include "base/lib/ll.conf.h"
#include "base/lib/ll.syslog.h"
#include "base/lib/ll.hash.h"
#include "batch/mbd/mbd.h"

static char events_path[PATH_MAX];
char jobs_dir[PATH_MAX];
static ino_t events_ino = 0;
static uint32_t compact_seq = 0;
static int job_finish_retain = 100;

static FILE *open_events(void)
{
    FILE *fp = fopen(events_path, "a");
    if (fp == NULL) {
        LL_ERR("fopen=%s", events_path);
        mbd_die(MBD_EXIT_EVENTS);
    }
    struct stat st;
    if (fstat(fileno(fp), &st) < 0 ||
        (events_ino != 0 && st.st_ino != events_ino)) {
        LL_ERRX("eventlog inode changed or removed — integrity lost");
        fclose(fp);
        mbd_die(MBD_EXIT_EVENTS);
    }
    events_ino = st.st_ino;
    return fp;
}

static void replay_reset_counters(void)
{
    struct ll_list_entry *e;

    for (e = queue_list.head; e != NULL; e = e->next) {
        struct mbd_queue *q = (struct mbd_queue *) e;

        q->num_jobs = 0;
        q->num_pend = 0;
        q->num_run = 0;
        q->num_susp = 0;
        q->num_held = 0;
        q->num_cpus_used = 0;
        q->num_hosts_used = 0;
    }

    for (e = host_list.head; e != NULL; e = e->next) {
        struct mbd_host *h = (struct mbd_host *) e;

        h->num_jobs = 0;
        h->num_run = 0;
        h->num_susp = 0;
        h->num_cpus_used = 0;
        h->exclusive = 0;
    }
}

static void replay_charge_running_job(struct job_data *job)
{
    for (int i = 0; i < job->run_nhosts; i++) {
        struct mbd_host *h = job->run_hosts[i];

        h->res.free_cpu -= job->res.num_cpus;
        h->res.free_mem_mb -= job->res.mem_mb;
        h->res.free_storage_mb -= job->res.storage_mb;
        h->num_jobs++;
        h->num_cpus_used += job->res.num_cpus;

        if (job->state == JOB_SUSPENDED)
            h->num_susp++;
        else
            h->num_run++;

        if (job->flags & JOB_FLAG_EXCLUSIVE)
            h->exclusive = 1;

        if (job->res.num_gpus > 0)
            h->res.free_gpu -= job->res.num_gpus;

        if (job->res.gpu_type[0] != 0) {
            struct mbd_gpu *g;
            assert(job->res.num_gpus > 0);
            g = ll_hash_search(&h->res.gpu_type_hash, job->res.gpu_type);
            if (g == NULL) {
                LL_ERRX("job=%ld host=%s gpu_type=%s not found", job->job_id,
                        h->net.name, job->res.gpu_type);
                continue;
            }
            g->free -= job->res.num_gpus;
        }
    }
}

static void replay_rebuild_counters(void)
{
    replay_reset_counters();

    struct ll_list_entry *e;

    for (e = pend_jobs_list.head; e != NULL; e = e->next) {
        struct job_data *job = (struct job_data *) e;

        assert(job->queue);
        if (job->queue == NULL)
            continue;

        job->queue->num_jobs++;
        if (job->state == JOB_HELD)
            job->queue->num_held++;
        else if (job->state == JOB_PENDING)
            job->queue->num_pend++;
        else {
            LL_ERRX("job=%ld in invalid state %d in pending list", job->job_id,
                    job->state);
            assert(0);
        }
    }

    for (e = run_jobs_list.head; e != NULL; e = e->next) {
        struct job_data *job = (struct job_data *) e;

        assert(job->queue);
        if (job->queue == NULL)
            continue;

        job->queue->num_jobs++;
        if (job->state == JOB_RUNNING) {
            job->queue->num_run++;
            job->queue->num_cpus_used += job->res.num_cpus * job->run_nhosts;
            job->queue->num_hosts_used += job->run_nhosts;
        } else if (job->state == JOB_SUSPENDED) {
            job->queue->num_susp++;
            job->queue->num_cpus_used += job->res.num_cpus * job->run_nhosts;
            job->queue->num_hosts_used += job->run_nhosts;
        } else {
            LL_ERRX("job=%ld in invalid state %d in running list", job->job_id,
                    job->state);
            assert(0);
        }
        replay_charge_running_job(job);
        token_alloc(job);
    }
}

void event_job_new(const struct job_data *job, const struct wire_job_submit *ws)
{
    struct log_job_new e;
    memset(&e, 0, sizeof(e));

    e.job_id = job->job_id;
    e.uid = (uid_t)job->uid;
    e.gid = (gid_t)job->gid;
    e.state = job->state;
    e.priority = job->priority;
    e.submit_time = job->submit_time;
    e.begin_time = (time_t) ws->begin_time;
    e.term_time = (time_t) ws->term_time;

    e.num_cpu = ws->num_cpus;
    e.num_hosts = ws->num_hosts;
    e.num_gpus = ws->num_gpus;
    e.mem_mb = ws->mem_mb;
    e.storage_mb = ws->storage_mb;
    ll_strlcpy(e.gpu_type, ws->gpu_type, sizeof(e.gpu_type));
    ll_strlcpy(e.machines, ws->machines, sizeof(e.machines));

    e.flags = ws->flags;
    ll_strlcpy(e.username, ws->username, sizeof(e.username));
    ll_strlcpy(e.job_name, ws->name, sizeof(e.job_name));
    ll_strlcpy(e.queue, ws->queue, sizeof(e.queue));
    ll_strlcpy(e.project_name, ws->project, sizeof(e.project_name));
    ll_strlcpy(e.tokenpool, ws->tokenpool, sizeof(e.tokenpool));

    FILE *fp = open_events();
    if (log_write_job_new(fp, &e) < 0) {
        fclose(fp);
        LL_ERR("log_write_job_new failed job_id=%ld", job->job_id);
        mbd_die(MBD_EXIT_EVENTS);
    }
    fclose(fp);
}

/*
 * event_job_start -- called at dispatch time, plan is still valid.
 * Builds the space-separated hosts string from plan->hosts[].
 */
void event_job_start(const struct job_data *job)
{
    struct log_job_start e;
    memset(&e, 0, sizeof(e));

    e.job_id = job->job_id;
    e.dispatch_time = job->dispatch_time;
    e.nhosts = job->res.num_hosts;
    e.cpus_per_host = job->res.num_cpus;
    e.gpus_per_host = job->res.num_gpus;

    ll_strlcpy(e.exec_host, job->run_hosts[0]->net.name, sizeof(e.exec_host));
    ll_strlcpy(e.gpu_type, job->res.gpu_type, sizeof(e.gpu_type));

    /* build space-separated hostname list */
    for (int i = 0; i < job->run_nhosts; i++) {
        if (i > 0)
            ll_strlcat(e.hosts, " ", sizeof(e.hosts));
        ll_strlcat(e.hosts, job->run_hosts[i]->net.name, sizeof(e.hosts));
    }

    FILE *fp = open_events();
    if (log_write_job_start(fp, &e) < 0) {
        fclose(fp);
        LL_ERR("log_write_job_start failed job_id=%ld", job->job_id);
        mbd_die(MBD_EXIT_EVENTS);
    }
    fclose(fp);
}

void event_job_fork(const struct job_data *job)
{
    struct log_job_fork e;
    memset(&e, 0, sizeof(e));

    e.job_id = job->job_id;
    e.fork_time = job->fork_time;
    e.job_pid = job->pid;

    FILE *fp = open_events();
    if (log_write_job_fork(fp, &e) < 0) {
        fclose(fp);
        LL_ERR("log_write_job_fork failed job_id=%ld", job->job_id);
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
        LL_ERR("log_write_job_signal failed job_id=%ld", job->job_id);
        mbd_die(MBD_EXIT_EVENTS);
    }
    fclose(fp);
}

void event_job_finish(const struct job_data *job)
{
    struct log_job_finish e;
    memset(&e, 0, sizeof(e));

    e.job_id = job->job_id;
    e.uid = job->uid;
    e.state = job->state;
    e.exit_status = job->exit_status;
    e.end_time = job->end_time;

    FILE *fp = open_events();
    if (log_write_job_finish(fp, &e) < 0) {
        fclose(fp);
        LL_ERR("log_write_job_finish failed job_id=%ld", job->job_id);
        mbd_die(MBD_EXIT_EVENTS);
    }
    fclose(fp);
}

void event_job_pend_susp(const struct job_data *job)
{
    struct log_job_pend_susp e;
    memset(&e, 0, sizeof(e));
    e.job_id = job->job_id;
    e.event_time = time(NULL);

    FILE *fp = open_events();
    if (log_write_job_pend_susp(fp, &e) < 0) {
        fclose(fp);
        LL_ERR("log_write_job_pend_susp failed job_id=%ld", job->job_id);
        mbd_die(MBD_EXIT_EVENTS);
    }
    fclose(fp);
}

void event_job_pend_resume(const struct job_data *job)
{
    struct log_job_pend_resume e;
    memset(&e, 0, sizeof(e));
    e.job_id = job->job_id;
    e.event_time = time(NULL);

    FILE *fp = open_events();
    if (log_write_job_pend_resume(fp, &e) < 0) {
        fclose(fp);
        LL_ERR("log_write_job_resume failed job_id=%ld", job->job_id);
        mbd_die(MBD_EXIT_EVENTS);
    }
    fclose(fp);
}

void event_job_susp(const struct job_data *job)
{
    struct log_job_susp e;
    memset(&e, 0, sizeof(e));
    e.job_id = job->job_id;
    e.event_time = time(NULL);

    FILE *fp = open_events();
    if (log_write_job_susp(fp, &e) < 0) {
        fclose(fp);
        LL_ERR("log_write_job_susp failed job_id=%ld", job->job_id);
        mbd_die(MBD_EXIT_EVENTS);
    }
    fclose(fp);
}

/* -----------------------------------------------------------------------
 * replay
 * ----------------------------------------------------------------------- */
static struct job_data *replay_alloc(const struct log_job_new *e)
{
    struct job_data *job = calloc(1, sizeof(struct job_data));
    if (job == NULL) {
        LL_ERR("calloc failed");
        return NULL;
    }

    job->job_id = e->job_id;
    job->uid = e->uid;
    job->gid = e->gid;
    job->state = e->state;
    job->priority = e->priority;
    job->submit_time = e->submit_time;
    job->begin_time = e->begin_time;
    job->term_time = e->term_time;

    job->res.num_cpus = e->num_cpu;
    job->res.num_hosts = e->num_hosts;
    job->res.num_gpus = e->num_gpus;
    job->res.mem_mb = e->mem_mb;
    job->res.storage_mb = e->storage_mb;
    ll_strlcpy(job->res.gpu_type, e->gpu_type, sizeof(job->res.gpu_type));

    machines_hash_populate(&job->res.machines, e->machines);

    job->flags = e->flags;
    ll_strlcpy(job->name, e->job_name, sizeof(job->name));
    ll_strlcpy(job->user, e->username, sizeof(job->user));
    ll_strlcpy(job->project, e->project_name, sizeof(job->project));

    job->queue = ll_hash_search(&queue_name_hash, e->queue);
    if (job->queue == NULL) {
        LL_ERR("job_id=%ld queue='%s' not found, orphaned", e->job_id,
               e->queue);
        job->state = JOB_ORPHAN;
    }

    job->run_hosts = calloc(job->res.num_hosts, sizeof(struct mbd_host *));
    if (job->run_hosts == NULL) {
        LL_ERR("calloc run_hosts with num_hosts=%d failed", job->res.num_hosts);
        job->res.num_hosts = 0;
        free(job);
        return NULL;
    }

    return job;
}

static int replay_insert(struct job_data *job)
{
    char key[LL_BUFSIZ_32];
    snprintf(key, sizeof(key), "%ld", job->job_id);
    if (ll_hash_insert(&job_id_hash, key, job, 0) < 0) {
        LL_ERR("job_id=%ld hash insert failed", job->job_id);
        free(job);
        return 0;
    }
    job_set_list(job, &pend_jobs_list, JOB_LIST_PEND);
    return 1;
}

static int replay_job_new(const struct event_rec *rec, int64_t *max_id)
{
    struct log_job_new e;
    memset(&e, 0, sizeof(e));
    if (log_parse_job_new(rec, &e) < 0) {
        LL_ERR("parse JOB_NEW failed");
        return 0;
    }
    if (e.job_id > *max_id)
        *max_id = e.job_id;

    struct job_data *job = replay_alloc(&e);
    if (job == NULL) {
        LL_ERR("failed replay job=%ld", e.job_id);
        return 0;
    }
    return replay_insert(job);
}

static int replay_set_run_hosts(struct job_data *job,
                                const struct log_job_start *e)
{
    char hosts[LL_BUFSIZ_4K];

    ll_strlcpy(hosts, e->hosts, sizeof(hosts));

    job->run_hosts = calloc(e->nhosts, sizeof(struct mbd_host *));
    if (job->run_hosts == NULL) {
        LL_ERR("calloc failed job=%ld nhosts=%d", job->job_id, e->nhosts);
        return -1;
    }

    char *tok = strtok(hosts, " \t,");
    while (tok != NULL) {
        struct mbd_host *h = ll_hash_search(&host_name_hash, tok);
        if (h == NULL) {
            LL_ERRX("JOB_START job_id=%ld host=%s not found", job->job_id, tok);
            return -1;
        }

        if (job->run_nhosts >= e->nhosts) {
            LL_ERRX("job_id=%ld too many hosts run=%d e=%d", job->job_id,
                    job->run_nhosts, e->nhosts);
            return -1;
        }

        job->run_hosts[job->run_nhosts] = h;
        job->run_nhosts++;

        tok = strtok(NULL, " \t,");
    }

    if (job->run_nhosts != e->nhosts) {
        LL_ERRX("JOB_START job_id=%ld expected_nhosts=%d got=%d", job->job_id,
                e->nhosts, job->run_nhosts);
        assert(0);
        return -1;
    }

    return 0;
}

static void replay_job_start(const struct event_rec *rec)
{
    struct log_job_start e;
    memset(&e, 0, sizeof(e));
    if (log_parse_job_start(rec, &e) < 0) {
        LL_ERR("parse JOB_START failed");
        return;
    }
    struct job_data *job = job_find(e.job_id);
    if (job == NULL) {
        LL_ERR("JOB_START job_id=%ld not found", e.job_id);
        return;
    }
    if (replay_set_run_hosts(job, &e) < 0) {
        LL_ERR("job_id=%ld orphaned replay_set_run_hosts failed", e.job_id);
        job->state = JOB_ORPHAN;
        // make sure later on we dont reply this job
        job->run_nhosts = 0;
        return;
    }

    job->state = JOB_RUNNING;
    job->dispatch_time = e.dispatch_time;
    job_move_list(job, &pend_jobs_list, &run_jobs_list, JOB_LIST_RUN);

    LL_DEBUG("JOB_START job_id=%ld exec_host=%s nhosts=%d cpus=%d gpus=%d",
             e.job_id, e.exec_host, e.nhosts, e.cpus_per_host, e.gpus_per_host);
}

static void replay_job_fork(const struct event_rec *rec)
{
    struct log_job_fork e;

    if (log_parse_job_fork(rec, &e) < 0) {
        LL_ERR("parse JOB_FORK failed");
        return;
    }
    struct job_data *job = job_find(e.job_id);
    if (job == NULL) {
        LL_ERR("JOB_FORK job_id=%ld not found", e.job_id);
        return;
    }
    job->pid = (pid_t) e.job_pid;
    job->fork_time = e.fork_time;

    LL_DEBUG("JOB_FORK job_id=%ld pid=%d", e.job_id, e.job_pid);
}

static void replay_job_signal(const struct event_rec *rec)
{
    struct log_job_signal e;
    if (log_parse_job_signal(rec, &e) < 0) {
        LL_ERR("parse JOB_SIGNAL failed");
        return;
    }
    struct job_data *job = job_find(e.job_id);
    if (job == NULL) {
        LL_ERR("JOB_SIGNAL job_id=%ld not found", e.job_id);
        return;
    }
    job->signal_time = e.signal_time;
    LL_DEBUG("JOB_SIGNAL job_id=%ld sig=%d uid=%u", e.job_id, e.signal_num,
             e.uid);
}

static void replay_job_finish(const struct event_rec *rec)
{
    struct log_job_finish e;
    memset(&e, 0, sizeof(e));
    if (log_parse_job_finish(rec, &e) < 0) {
        LL_ERR("parse JOB_FINISH failed");
        return;
    }
    struct job_data *job = job_find(e.job_id);
    if (job == NULL) {
        LL_ERR("JOB_FINISH job_id=%ld not found", e.job_id);
        return;
    }

    job->state = e.state;
    job->exit_status = e.exit_status;
    job->uid = e.uid;
    job->end_time = e.end_time;

    struct ll_list *from = &pend_jobs_list;
    if (job->list_id == JOB_LIST_RUN)
        from = &run_jobs_list;
    job_move_list(job, from, &finish_jobs_list, JOB_LIST_FINISH);
    LL_DEBUG("JOB_FINISH job_id=%ld", e.job_id);
}

static void replay_job_pend_susp(const struct event_rec *rec)
{
    struct log_job_pend_susp e;
    if (log_parse_job_pend_susp(rec, &e) < 0) {
        LL_ERR("parse JOB_PEND_SUSP failed");
        return;
    }
    struct job_data *job = job_find(e.job_id);
    if (job == NULL) {
        LL_ERRX("JOB_PEND_SUSP job_id=%ld not found", e.job_id);
        return;
    }
    if (!(job->state == JOB_PENDING)) {
        LL_ERRX("JOB_PEND_SUSP job_id=%ld not in PEND", e.job_id);
        assert(0);
        return;
    }
    job->state = JOB_HELD;
    LL_DEBUG("JOB_PEND_SUSP job_id=%ld", e.job_id);
}

static void replay_job_pending_resume(const struct event_rec *rec)
{
    struct log_job_pend_resume e;
    if (log_parse_job_pend_resume(rec, &e) < 0) {
        LL_ERR("parse JOB_RESUME failed");
        return;
    }
    struct job_data *job = job_find(e.job_id);
    if (job == NULL) {
        LL_ERRX("JOB_RESUME job_id=%ld not found", e.job_id);
        return;
    }
    if (!(job->state == JOB_HELD)) {
        LL_ERRX("JOB_PENDING_RESUME job_id=%ld not in PEND_SUSP", e.job_id);
        assert(0);
        return;
    }
    job->state = JOB_PENDING;
    LL_DEBUG("JOB_RESUME job_id=%ld", e.job_id);
}

static void replay_job_susp(const struct event_rec *rec)
{
    struct log_job_susp e;
    if (log_parse_job_susp(rec, &e) < 0) {
        LL_ERR("parse JOB_SUSP failed");
        return;
    }
    struct job_data *job = job_find(e.job_id);
    if (job == NULL) {
        LL_ERRX("JOB_SUSP job_id=%ld not found", e.job_id);
        return;
    }
    job->state = JOB_SUSPENDED;
    LL_DEBUG("JOB_SUSP job_id=%ld", e.job_id);
}

/*
 * compact_seq_scan - derive the current sequence number by scanning the
 * mbd directory for existing eventlog.NNNNNN archives.
 *
 * No external sequence file is needed. Admins may delete old archives
 * freely without having to update any state file. The next compact will
 * simply use the highest sequence number found + 1.
 */
static void compact_seq_scan(void)
{
    char dir[PATH_MAX];
    snprintf(dir, sizeof(dir), "%s/mbd", ll_params[LL_STATE_DIR].val);

    DIR *dp = opendir(dir);
    if (dp == NULL)
        return; /* first run, seq stays 0 */

    struct dirent *de;
    while ((de = readdir(dp)) != NULL) {
        if (strncmp(de->d_name, "eventlog.", 10) != 0)
            continue;
        const char *p = de->d_name + 10;
        if (*p == '\0')
            continue;
        /* must be all digits */
        const char *q = p;
        while (*q >= '0' && *q <= '9')
            q++;
        if (*q != '\0')
            continue;
        uint32_t n = (uint32_t)atol(p);
        if (n > compact_seq)
            compact_seq = n;
    }
    closedir(dp);
}

int events_init(void)
{
    char dir[PATH_MAX];
    int n = snprintf(dir, sizeof(dir), "%s/mbd", ll_params[LL_STATE_DIR].val);
    if (n < 0 || n >= (int) sizeof(dir))
        mbd_die(MBD_EXIT_EVENTS);

    // bhist run by users need to read the events
    if (mkdir(dir, 0755) == -1 && errno != EEXIST) {
        syslog(LOG_ERR, "mkdir(%s) failed: %m", dir);
        mbd_die(MBD_EXIT_FATAL);
    }
    LL_INFO("working dir initialized %s", dir);

    n = snprintf(events_path, sizeof(events_path), "%s/eventlog", dir);
    if (n < 0 || n >= (int) sizeof(events_path))
        mbd_die(MBD_EXIT_EVENTS);
    LL_INFO("job events initialized %s", events_path);

    n = snprintf(jobs_dir, sizeof(jobs_dir), "%s/jobs", dir);
    if (n < 0 || n >= (int) sizeof(jobs_dir))
        mbd_die(MBD_EXIT_EVENTS);

    if (mkdir(jobs_dir, 0755) == -1 && errno != EEXIST) {
        LL_ERRX("mkdir(%s) failed", jobs_dir);
        mbd_die(MBD_EXIT_EVENTS);
    }
    LL_INFO("job working dir initialized %s", jobs_dir);

    for (int i = 0; i < JOB_BUCKETS; i++) {
        char bucket[PATH_MAX];
        int nb = snprintf(bucket, sizeof(bucket), "%s/%d", jobs_dir, i);
        if (nb < 0 || nb >= (int) sizeof(bucket))
            mbd_die(MBD_EXIT_EVENTS);
        if (mkdir(bucket, 0755) == -1 && errno != EEXIST) {
            LL_ERRX("mkdir(%s) failed", bucket);
            mbd_die(MBD_EXIT_EVENTS);
        }
    }

    LL_INFO("%d bucket dirs initialized", JOB_BUCKETS);

    if (!ll_atoi(ll_params[LL_MBD_JOB_FINISH_RETAIN].val, &job_finish_retain)) {
        LL_ERRX("failed parsing LL_JOB_FINISH_RETAIN=%s using default=100 jobs",
                ll_params[LL_MBD_JOB_FINISH_RETAIN].val);
    }

    compact_seq_scan();

    LL_INFO("events seq initialized seq=%u", compact_seq);

    return 0;
}

/*
 * job_id_seq_read - read the persisted job_id_seq at startup.
 *
 * Sets job_id_seq to max(current, persisted) so the event log replay
 * value and the persisted value are both respected. If the file does
 * not exist (first run) the value stays at whatever replay set it to.
 *
 * Resist the tempation to delete this file as compact may leave an
 * eventlog empty file so the job_id would go backwords.
 */
static void job_id_seq_read(void)
{
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/mbd/job_id_seq",
             ll_params[LL_STATE_DIR].val);

    FILE *fp = fopen(path, "r");
    if (fp == NULL)
        return; /* first run or no file yet */

    int64_t seq = 0;
    int n = fscanf(fp, "%ld", &seq);
    fclose(fp);

    if (n != 1) {
        LL_ERR("job_id_seq: parse failed, ignoring");
        return;
    }

    if (seq > job_id_seq) {
        LL_INFO("job_id_seq: restoring from file seq=%ld (was %ld)",
                seq, job_id_seq);
        job_id_seq = seq;
    }
}

static void replay_job_move(const struct event_rec *rec)
{
    struct log_job_move e;
    if (log_parse_job_move(rec, &e) < 0) {
        LL_ERR("parse JOB_MOVE failed");
        return;
    }
    struct job_data *job = job_find(e.job_id);
    if (job == NULL) {
        LL_ERRX("JOB_MOVE job_id=%ld not found", e.job_id);
        return;
    }
    struct mbd_queue *to = ll_hash_search(&queue_name_hash, e.to_queue);
    if (to == NULL) {
        LL_ERRX("JOB_MOVE job_id=%ld queue=%s not found, orphaned",
                e.job_id, e.to_queue);
        job->state = JOB_ORPHAN;
        return;
    }
    job->queue = to;
    LL_DEBUG("JOB_MOVE job_id=%ld from=%s to=%s", e.job_id,
             e.from_queue, e.to_queue);
}

static void replay_job_priority(const struct event_rec *rec)
{
    struct log_job_priority e;
    if (log_parse_job_priority(rec, &e) < 0) {
        LL_ERR("parse JOB_PRIORITY failed");
        return;
    }
    struct job_data *job = job_find(e.job_id);
    if (job == NULL) {
        LL_ERRX("JOB_PRIORITY job_id=%ld not found", e.job_id);
        return;
    }
    job->priority = e.new_priority;
    LL_DEBUG("JOB_PRIORITY job_id=%ld old=%d new=%d",
             e.job_id, e.old_priority, e.new_priority);
}

static void replay_job_pend(const struct event_rec *rec)
{
    struct log_job_pend e;

    if (log_parse_job_pend(rec, &e) < 0) {
        LL_ERR("parse JOB_PEND failed");
        return;
    }

    struct job_data *job = job_find(e.job_id);
    if (job == NULL) {
        LL_ERRX("JOB_PEND job_id=%ld not found", e.job_id);
        return;
    }

    assert(job->list_id == JOB_LIST_RUN);
    if (job->list_id == JOB_LIST_RUN)
        job_move_list(job, &run_jobs_list, &pend_jobs_list, JOB_LIST_PEND);

    job->pid = 0;
    job->fork_time = 0;
    job->dispatch_time = 0;
    job->state = JOB_PENDING;
    job->run_nhosts = 0;
    memset(job->run_hosts, 0, job->res.num_hosts * sizeof(job->run_hosts[0]));

    LL_DEBUG("JOB_PEND job_id=%ld", e.job_id);
}

int jobs_replay(void)
{
    FILE *fp = fopen(events_path, "r");
    if (fp == NULL) {
        if (errno == ENOENT) {
            LL_INFO("replay: no job.events, nothing to replay");
            return 0;
        }
        LL_ERR("fopen %s", events_path);
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
        if (rec.type == EVENT_JOB_FORK) {
            replay_job_fork(&rec);
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
        if (rec.type == EVENT_JOB_MOVE) {
            replay_job_move(&rec);
            continue;
        }
        if (rec.type == EVENT_JOB_PRIORITY) {
            replay_job_priority(&rec);
            continue;
        }
        if (rec.type == EVENT_JOB_PEND) {
            replay_job_pend(&rec);
            continue;
        }
    }

    if (ferror(fp)) {
        fclose(fp);
        LL_ERR("I/O error reading job events=%s line=%d", events_path, lineno);
        mbd_die(MBD_EXIT_EVENTS);
    }

    fclose(fp);

    if (max_id > job_id_seq)
        job_id_seq = max_id;

    /* persisted seq may be higher than replay if eventlog was fully
     * compacted -- take the max so job_id never goes backwards
     */
    job_id_seq_read();

    replay_rebuild_counters();
    // debug
    mbd_assert_counters();

    LL_INFO("replay: done, %d jobs restored, job_id_seq=%ld", restored,
            job_id_seq);
    return restored;
}

static void compact_write_job_new(FILE *fp, const struct job_data *job)
{
    struct log_job_new e;
    memset(&e, 0, sizeof(e));

    e.job_id = job->job_id;
    e.uid = job->uid;
    e.gid = job->gid;
    e.state = job->state;
    e.priority = job->priority;
    e.submit_time = job->submit_time;
    e.begin_time = job->begin_time;
    e.term_time = job->term_time;
    e.num_cpu = job->res.num_cpus;
    e.num_hosts = job->res.num_hosts;
    e.num_gpus = job->res.num_gpus;
    e.mem_mb = job->res.mem_mb;
    e.storage_mb = job->res.storage_mb;
    e.flags = job->flags;
    ll_strlcpy(e.gpu_type, job->res.gpu_type, sizeof(e.gpu_type));
    ll_strlcpy(e.username, job->user, sizeof(e.username));
    ll_strlcpy(e.job_name, job->name, sizeof(e.job_name));
    ll_strlcpy(e.queue, job->queue->name, sizeof(e.queue));
    ll_strlcpy(e.project_name, job->project, sizeof(e.project_name));
    ll_strlcpy(e.tokenpool, job->res.tokenpool_str, sizeof(e.tokenpool));
    ll_strlcpy(e.machines, job->res.machines_str, sizeof(e.machines));

    if (log_write_job_new(fp, &e) < 0)
        mbd_die(MBD_EXIT_EVENTS);
}

static void compact_write_job_start(FILE *fp, const struct job_data *job)
{
    struct log_job_start e;
    memset(&e, 0, sizeof(e));

    e.job_id = job->job_id;
    e.dispatch_time = job->dispatch_time;
    e.nhosts = job->res.num_hosts;
    e.cpus_per_host = job->res.num_cpus;
    e.gpus_per_host = job->res.num_gpus;
    ll_strlcpy(e.exec_host, job->run_hosts[0]->net.name, sizeof(e.exec_host));
    ll_strlcpy(e.gpu_type, job->res.gpu_type, sizeof(e.gpu_type));

    for (int i = 0; i < job->res.num_hosts; i++) {
        if (i > 0)
            ll_strlcat(e.hosts, " ", sizeof(e.hosts));
        ll_strlcat(e.hosts, job->run_hosts[i]->net.name, sizeof(e.hosts));
    }

    if (log_write_job_start(fp, &e) < 0)
        mbd_die(MBD_EXIT_EVENTS);
}

static void compact_write_job_fork(FILE *fp, const struct job_data *job)
{
    struct log_job_fork e;
    memset(&e, 0, sizeof(e));

    e.job_id = job->job_id;
    e.fork_time = job->fork_time;
    e.job_pid = job->pid;

    if (log_write_job_fork(fp, &e) < 0)
        mbd_die(MBD_EXIT_EVENTS);
}


/*
 * job_id_seq_write - persist the current job_id_seq to disk.
 *
 * Called after every job submission so that mbd restart after a full
 * compaction (empty eventlog) still resumes from the correct sequence
 * number and never reuses a job_id.
 */
void job_id_seq_write(void)
{
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/mbd/job_id_seq",
             ll_params[LL_STATE_DIR].val);

    char tmp[PATH_MAX + LL_BUFSIZ_64];
    snprintf(tmp, sizeof(tmp), "%s.tmp", path);

    FILE *fp = fopen(tmp, "w");
    if (fp == NULL) {
        LL_ERR("fopen job_id_seq=%s: %m", tmp);
        mbd_die(MBD_EXIT_EVENTS);
    }
    fprintf(fp, "%ld\n", job_id_seq);
    if (fflush(fp) != 0 || fsync(fileno(fp)) != 0) {
        LL_ERR("fsync job_id_seq failed");
        fclose(fp);
        mbd_die(MBD_EXIT_EVENTS);
    }
    fclose(fp);

    if (rename(tmp, path) < 0) {
        LL_ERR("rename job_id_seq %s -> %s: %m", tmp, path);
        mbd_die(MBD_EXIT_EVENTS);
    }
}

/*
 * events_compact - archive eventlog, rewrite with live jobs only.
 * PEND:      JOB_NEW
 * RUN/SUSP:  JOB_NEW + JOB_START
 * FINISH:    discarded — full history in archived file for bhist.
 *            finish_jobs_list drained and freed here.
 *
 * After compaction, eventlog is a replay checkpoint, not a chronological
 * history file. Event timestamps are preserved, but record order across
 * different jobs may differ from original event arrival order.
 *
 * Replay only depends on per-job state reconstruction. Historical tools
 * such as bhist must read archived eventlog.* files for full chronological
 * job history.
 */
static void events_compact(void)
{
    char archived[PATH_MAX + LL_BUFSIZ_32];
    /*
     * Sequence number is derived by compact_seq_scan() at startup from
     * the filenames already present in the directory. No separate sequence
     * file is kept, so admins may delete old archives freely without
     * having to update any state.
     */
    compact_seq++;
    snprintf(archived, sizeof(archived), "%s.%u", events_path, compact_seq);

    if (rename(events_path, archived) < 0) {
        LL_ERR("rename(%s, %s)", events_path, archived);
        mbd_die(MBD_EXIT_EVENTS);
    }

    FILE *fp = fopen(events_path, "a");
    if (!fp)
        mbd_die(MBD_EXIT_EVENTS);

    struct ll_list_entry *e;
    struct ll_list_entry *next;

    for (e = pend_jobs_list.head; e; e = e->next)
        compact_write_job_new(fp, (struct job_data *) e);

    for (e = run_jobs_list.head; e; e = e->next) {
        struct job_data *job = (struct job_data *) e;
        compact_write_job_new(fp, job);
        compact_write_job_start(fp, job);
        if (job->fork_time)
            compact_write_job_fork(fp, job);
    }

    for (e = finish_jobs_list.head; e; e = next) {
        next = e->next;
        struct job_data *job = (struct job_data *) e;
        ll_list_remove(&finish_jobs_list, &job->ent);

        char key[LL_BUFSIZ_32];
        sprintf(key, "%ld", job->job_id);
        struct job_data *j2 = ll_hash_remove(&job_id_hash, key);
        assert(j2 == job);
        job_free(job);
    }

    if (fflush(fp) != 0 || fsync(fileno(fp)) != 0)
        mbd_die(MBD_EXIT_EVENTS);

    /* new eventlog file has a new inode -- update tracking so
     * open_events() does not falsely detect integrity loss */
    struct stat st;
    if (fstat(fileno(fp), &st) < 0) {
        LL_ERR("fstat eventlog after compact");
        mbd_die(MBD_EXIT_EVENTS);
    }
    events_ino = st.st_ino;

    fclose(fp);

    LL_INFO("events compacted seq=%u archived=%s", compact_seq, archived);
}

void maybe_compact_events(void)
{
    if (ll_list_count(&finish_jobs_list) < job_finish_retain)
        return;

    LL_DEBUG("compacting eventlog as finished job=%d",
             ll_list_count(&finish_jobs_list));

    events_compact();
}

void event_job_move(const struct job_data *job, const char *to_queue)
{
    struct log_job_move e;

    memset(&e, 0, sizeof(e));
    e.job_id = job->job_id;
    e.event_time = time(NULL);
    ll_strlcpy(e.from_queue, job->queue->name, sizeof(e.from_queue));
    ll_strlcpy(e.to_queue, to_queue, sizeof(e.to_queue));

    FILE *fp = open_events();
    if (log_write_job_move(fp, &e) < 0) {
        fclose(fp);
        LL_ERR("log_write_job_move failed job_id=%ld", job->job_id);
        mbd_die(MBD_EXIT_EVENTS);
    }
    fclose(fp);
}

void event_job_priority(const struct job_data *job, int32_t old_priority)
{
    struct log_job_priority e;

    memset(&e, 0, sizeof(e));
    e.job_id = job->job_id;
    e.event_time = time(NULL);
    e.old_priority = old_priority;
    e.new_priority = job->priority;

    FILE *fp = open_events();
    if (log_write_job_priority(fp, &e) < 0) {
        fclose(fp);
        LL_ERR("log_write_job_priority failed job_id=%ld", job->job_id);
        mbd_die(MBD_EXIT_EVENTS);
    }
    fclose(fp);
}

void event_job_pend(const struct job_data *job)
{
    struct log_job_pend e;
    memset(&e, 0, sizeof(e));

    e.job_id = job->job_id;
    e.event_time = time(NULL);

    FILE *fp = open_events();
    if (log_write_job_pend(fp, &e) < 0) {
        fclose(fp);
        LL_ERR("log_write_job_pend failed job_id=%ld", job->job_id);
        mbd_die(MBD_EXIT_EVENTS);
    }
    fclose(fp);
}
