/*
 * Copyright (C) LavaLite Contributors
 * GPL v2
 */

#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <syslog.h>
#include <fcntl.h>
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

static char manifest_path[PATH_MAX];
char jobs_dir[PATH_MAX];
static ino_t manifest_ino = 0;
static uint32_t manifest_seq = 0;
static int job_finish_threshold = 1000;
static int job_finish_period = 3600;

static FILE *open_manifest(void)
{
    int fd = open(manifest_path, O_CREAT | O_WRONLY | O_APPEND, 0640);
    if (fd < 0) {
        LL_ERR("open=%s", manifest_path);
        mbd_die(MBD_EXIT_EVENTS);
    }

    FILE *fp = fdopen(fd, "a");
    if (fp == NULL) {
        LL_ERR("fopen=%s", manifest_path);
        mbd_die(MBD_EXIT_EVENTS);
    }
    struct stat st;
    if (fstat(fileno(fp), &st) < 0 ||
        (manifest_ino != 0 && st.st_ino != manifest_ino)) {
        LL_ERRX("manifest inode changed or removed — integrity lost");
        fclose(fp);
        mbd_die(MBD_EXIT_EVENTS);
    }
    manifest_ino = st.st_ino;
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
        // Assume a host has less than 64 gpus but cap it
        for (int i = 0; i < h->res.gpu.count; i++)
            h->res.gpu.ids[i].in_use = 0;
    }
}

static void replay_charge_running_job(struct job_data *job)
{
    for (int i = 0; i < job->run_nhosts; i++) {
        struct mbd_host *h = job->run_hosts[i];

        /* only an orphan can run on a stale host, see job_is_orphan() */
        if (h->stale)
            continue;

        // TODO what to do if a cpu has been taken of line
        h->res.free_cpu -= job->res.num_cpus;
        h->res.free_mem_mb -= (int64_t) job->res.mem_mb;
        h->res.free_storage_mb -= (int64_t) job->res.storage_mb;
        h->num_jobs++;
        h->num_cpus_used += job->res.num_cpus;

        if (job->state == JOB_SUSPENDED)
            h->num_susp++;
        else
            h->num_run++;

        if (job->flags & JOB_FLAG_EXCLUSIVE)
            h->exclusive = 1;

        // TODO what to do if a gpu has been taken of line
        // Update the gpus that were assigned to the job at
        // runtime
        if (job->res.num_gpus > 0)
            gpu_ids_mark_inuse(&h->res.gpu, job->res.num_gpus);
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
            LL_ERRX("job=%ld in invalid state %d in pending list",
                    job->job_id, job->state);
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
            LL_ERRX("job=%ld in invalid state %d in running list",
                    job->job_id, job->state);
            assert(0);
        }
        replay_charge_running_job(job);
        token_alloc(job);
    }

    /*
     * Orphans hold what they hold. A frozen job is still pending in
     * its queue, or still running on its hosts with their cpus, memory
     * and tokens. Count it on every real object it references, by its
     * real state, so the scheduler never hands out what it holds and
     * the counters stay right when the config is fixed. Stale objects
     * are not counted, they are in no list.
     */
    for (e = orphan_jobs_list.head; e != NULL; e = e->next) {
        struct job_data *job = (struct job_data *) e;
        struct mbd_queue *q = job->queue;

        if (!q->stale)
            q->num_jobs++;

        switch (job->state) {
        case JOB_HELD:
            if (!q->stale)
                q->num_held++;
            break;
        case JOB_PENDING:
            if (!q->stale)
                q->num_pend++;
            break;
        case JOB_RUNNING:
        case JOB_SUSPENDED:
            if (!q->stale) {
                if (job->state == JOB_RUNNING)
                    q->num_run++;
                else
                    q->num_susp++;
                q->num_cpus_used += job->res.num_cpus * job->run_nhosts;
                q->num_hosts_used += job->run_nhosts;
            }
            replay_charge_running_job(job);
            token_alloc(job);
            break;
        default:
            LL_ERRX("job=%ld orphan in invalid state %d", job->job_id,
                    job->state);
            assert(0);
        }
    }

    /*
     * dep_refcnt is derived state, not persisted directly (see
     * job_replay_deps()). Rebuild it here, once every job_id from the
     * manifest is in job_id_hash, by walking every still-pending job's
     * dependency expression and re-establishing its holds.
     */
    for (e = pend_jobs_list.head; e != NULL; e = e->next) {
        struct job_data *job = (struct job_data *) e;

        if (job->deps.count > 0)
            job_deps_hold(job);
    }
    /* an orphan is a live job, frozen: its dependencies stay held so
     * the jobs it waits on survive compaction until it heals */
    for (e = orphan_jobs_list.head; e != NULL; e = e->next) {
        struct job_data *job = (struct job_data *) e;

        if (job->deps.count > 0)
            job_deps_hold(job);
    }

    /*
     * array_element_cnt is derived state too, same reasoning as
     * dep_refcnt above -- calloc left every head at 0. Rebuild by
     * counting, per array, how many elements are still not finished
     * (present in pend_jobs_list or run_jobs_list). Elements already
     * compacted away before this crash are gone from the manifest
     * entirely and cannot be counted -- same accepted limitation as
     * any other post-compaction history loss.
     */
    for (e = pend_jobs_list.head; e != NULL; e = e->next) {
        struct job_data *job = (struct job_data *) e;

        if (job->array_id == 0)
            continue;

        struct job_data *head = job_find(job->array_id);
        if (head != NULL)
            head->array_element_cnt++;
    }
    for (e = run_jobs_list.head; e != NULL; e = e->next) {
        struct job_data *job = (struct job_data *) e;

        if (job->array_id == 0)
            continue;

        struct job_data *head = job_find(job->array_id);
        if (head != NULL)
            head->array_element_cnt++;
    }
    for (e = orphan_jobs_list.head; e != NULL; e = e->next) {
        struct job_data *job = (struct job_data *) e;

        if (job->array_id == 0)
            continue;

        struct job_data *head = job_find(job->array_id);
        if (head != NULL)
            head->array_element_cnt++;
    }
}

void event_job_new(const struct job_data *job, const struct wire_job_submit *ws)
{
    struct log_job_new e;
    memset(&e, 0, sizeof(e));

    e.job_id = job->job_id;
    e.array_id = job->array_id;
    e.array_index = job->array_index;
    e.array_start = job->array_start;
    e.array_end = job->array_end;
    e.array_stride = job->array_stride;
    e.uid = (uid_t) job->uid;
    e.gid = (gid_t) job->gid;
    e.state = job->state;
    e.priority = job->priority;
    e.submit_time = job->submit_time;
    e.begin_time = (time_t) ws->begin_time;
    e.term_time = (time_t) ws->term_time;
    e.num_cpu = ws->num_cpus;
    /* Preserve the submitted host count in the event log.
     * Host groups are expanded during replay.
     */
    e.num_hosts = ws->num_hosts;
    e.num_gpus = ws->num_gpus;
    e.mem_mb = ws->mem_mb;
    e.storage_mb = ws->storage_mb;
    ll_strlcpy(e.gpu_model, ws->gpu_model, sizeof(e.gpu_model));
    ll_strlcpy(e.machines, ws->machines, sizeof(e.machines));

    e.flags = ws->flags;
    ll_strlcpy(e.username, ws->username, sizeof(e.username));
    ll_strlcpy(e.job_name, ws->name, sizeof(e.job_name));
    ll_strlcpy(e.queue, ws->queue, sizeof(e.queue));
    ll_strlcpy(e.project_name, ws->project, sizeof(e.project_name));
    ll_strlcpy(e.tokenpool, ws->tokenpool, sizeof(e.tokenpool));
    ll_strlcpy(e.depend_cond, ws->depend_cond, sizeof(e.depend_cond));
    e.run_limit = job->run_limit;

    if (job_is_service(job)) {
        ll_strlcpy(e.service_name, job->svc_inst->svc->name,
                   sizeof(e.service_name));
        ll_strlcpy(e.service_token, job->svc_inst->token,
                   sizeof(e.service_token));
    }

    FILE *fp = open_manifest();
    if (log_write_job_new(fp, &e) < 0) {
        fclose(fp);
        LL_ERR("log_write_job_new failed job=%ld", job->job_id);
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

    ll_strlcpy(e.gpu_model, job->res.gpu_model, sizeof(e.gpu_model));
    ll_strlcpy(e.gpu_assigned, job->gpu_assigned, sizeof(e.gpu_assigned));

    /* build space-separated hostname list */
    for (int i = 0; i < job->run_nhosts; i++) {
        if (i > 0)
            ll_strlcat(e.run_hosts, " ", sizeof(e.run_hosts));
        ll_strlcat(e.run_hosts, job->run_hosts[i]->net.name, sizeof(e.run_hosts));
    }

    FILE *fp = open_manifest();
    if (log_write_job_start(fp, &e) < 0) {
        fclose(fp);
        LL_ERR("log_write_job_start failed job=%ld", job->job_id);
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
    if (job->svc_inst != NULL)
        e.service_port = job->svc_inst->port;

    FILE *fp = open_manifest();
    if (log_write_job_fork(fp, &e) < 0) {
        fclose(fp);
        LL_ERR("log_write_job_fork failed job=%ld", job->job_id);
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
    e.job_flags = job->flags;

    FILE *fp = open_manifest();
    if (log_write_job_signal(fp, &e) < 0) {
        fclose(fp);
        LL_ERR("log_write_job_signal failed job=%ld", job->job_id);
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

    FILE *fp = open_manifest();
    if (log_write_job_finish(fp, &e) < 0) {
        fclose(fp);
        LL_ERR("log_write_job_finish failed job=%ld", job->job_id);
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

    FILE *fp = open_manifest();
    if (log_write_job_pend_susp(fp, &e) < 0) {
        fclose(fp);
        LL_ERR("log_write_job_pend_susp failed job=%ld", job->job_id);
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

    FILE *fp = open_manifest();
    if (log_write_job_pend_resume(fp, &e) < 0) {
        fclose(fp);
        LL_ERR("log_write_job_resume failed job=%ld", job->job_id);
        mbd_die(MBD_EXIT_EVENTS);
    }
    fclose(fp);
}

/* a running job suspended, event_time is job->susp_time: compaction
 * writes it again with the same time, see compact_write_job_susp() */
void event_job_susp(const struct job_data *job)
{
    struct log_job_susp e;
    memset(&e, 0, sizeof(e));
    e.job_id = job->job_id;
    e.event_time = job->susp_time;

    FILE *fp = open_manifest();
    if (log_write_job_susp(fp, &e) < 0) {
        fclose(fp);
        LL_ERR("log_write_job_susp failed job=%ld", job->job_id);
        mbd_die(MBD_EXIT_EVENTS);
    }
    fclose(fp);
}

/* a suspended running job resumed */
void event_job_resume(const struct job_data *job)
{
    struct log_job_resume e;
    memset(&e, 0, sizeof(e));
    e.job_id = job->job_id;
    e.event_time = time(NULL);

    FILE *fp = open_manifest();
    if (log_write_job_resume(fp, &e) < 0) {
        fclose(fp);
        LL_ERR("log_write_job_resume failed job=%ld", job->job_id);
        mbd_die(MBD_EXIT_EVENTS);
    }
    fclose(fp);
}

/* -----------------------------------------------------------------------
 * stale objects
 *
 * A queue, service or host the manifest references but the config no
 * longer has. Replay builds a stale object carrying only the name, so
 * the history of every job replays to the end and nothing is ever
 * NULL. A token pool needs no object, tokens keep the pool name. Stale objects live only in these hashes, never in queue_list,
 * service_list, host_list or host_name_hash: they are never scheduled,
 * never counted, and an sbd can never register as a stale host.
 * Jobs that reference them become orphans, see replay_mark_orphans().
 * ----------------------------------------------------------------------- */
static struct ll_hash stale_queue_hash;
static struct ll_hash stale_service_hash;
static struct ll_hash stale_host_hash;

static struct mbd_queue *replay_queue(const char *name)
{
    struct mbd_queue *q = ll_hash_search(&queue_name_hash, name);
    if (q != NULL)
        return q;

    q = ll_hash_search(&stale_queue_hash, name);
    if (q != NULL)
        return q;

    q = calloc(1, sizeof(*q));
    if (q == NULL) {
        LL_ERR("calloc stale queue=%s failed", name);
        mbd_die(MBD_EXIT_MEM);
    }
    ll_strlcpy(q->name, name, sizeof(q->name));
    ll_hash_init(&q->host_hash, 0);
    ll_hash_init(&q->user_hash, 0);
    q->state = QUEUE_CLOSED;
    q->stale = 1;
    ll_hash_insert(&stale_queue_hash, q->name, q, 0);

    LL_ERRX("queue=%s not in the config, stale", name);
    return q;
}

static struct service_data *replay_service(const char *name)
{
    struct service_data *s = svc_find_by_name(name);
    if (s != NULL)
        return s;

    s = ll_hash_search(&stale_service_hash, name);
    if (s != NULL)
        return s;

    s = calloc(1, sizeof(*s));
    if (s == NULL) {
        LL_ERR("calloc stale service=%s failed", name);
        mbd_die(MBD_EXIT_MEM);
    }
    ll_strlcpy(s->name, name, sizeof(s->name));
    ll_list_init(&s->instances);
    s->stale = 1;
    ll_hash_insert(&stale_service_hash, s->name, s, 0);

    LL_ERRX("service=%s not in the config, stale", name);
    return s;
}

/* Run hosts are looked up in the whole config, not in the job's queue:
 * a host dropped from a queue still exists and still runs the job.
 */
static struct mbd_host *replay_host(const char *name)
{
    struct mbd_host *h = ll_hash_search(&host_name_hash, name);
    if (h != NULL)
        return h;

    h = ll_hash_search(&stale_host_hash, name);
    if (h != NULL)
        return h;

    h = calloc(1, sizeof(*h));
    if (h == NULL) {
        LL_ERR("calloc stale host=%s failed", name);
        mbd_die(MBD_EXIT_MEM);
    }
    ll_strlcpy(h->net.name, name, sizeof(h->net.name));
    h->state = HOST_UNAVAIL;
    h->sbd_chan = -1;
    h->stale = 1;
    ll_hash_insert(&stale_host_hash, h->net.name, h, 0);

    LL_ERRX("host=%s not in the config, stale", name);
    return h;
}

static int job_is_orphan(const struct job_data *job)
{
    if (job->queue->stale)
        return 1;

    if (job->svc_inst != NULL && job->svc_inst->svc->stale)
        return 1;

    for (int i = 0; i < job->run_nhosts; i++) {
        if (job->run_hosts[i]->stale)
            return 1;
    }

    /* tokens keep the pool name, see job_parse_tokens() */
    for (struct ll_list_entry *e = job->res.tokens.head; e != NULL;
         e = e->next) {
        struct job_token *t = (struct job_token *) e;
        if (ll_hash_search(&token_pool_name_hash, t->name) == NULL)
            return 1;
    }

    return 0;
}

static void replay_orphan_list(struct ll_list *list)
{
    struct ll_list_entry *e;
    struct ll_list_entry *next;

    for (e = list->head; e != NULL; e = next) {
        struct job_data *job = (struct job_data *) e;
        next = e->next;

        if (!job_is_orphan(job))
            continue;

        job_move_list(job, list, &orphan_jobs_list, JOB_LIST_ORPHAN);

        LL_ERRX("job=%ld state=%s queue=%s is orphan: it references a "
                "queue, service, host or token pool not in the config, "
                "frozen until the config is fixed", job->job_id,
                llb_job_state_str(job->state), job->queue->name);
    }
}

/* After the whole history has replayed, a live job referencing a stale
 * object, or a token pool not in the config, is an orphan: it moves to orphan_jobs_list and is frozen.
 * Its state, dispatch_time, fork_time, pid and run hosts stay exactly
 * as replay built them, compaction writes them back as they are, so
 * once the config is fixed the next replay rebuilds the job as it was.
 * Finished jobs need nothing, they keep their stale names for display.
 */
static void replay_mark_orphans(void)
{
    replay_orphan_list(&pend_jobs_list);
    replay_orphan_list(&run_jobs_list);
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
    job->array_id = e->array_id;
    job->array_index = e->array_index;
    job->array_start = e->array_start;
    job->array_end = e->array_end;
    job->array_stride = e->array_stride;
    job->uid = e->uid;
    job->gid = e->gid;
    job->state = e->state;
    job->priority = e->priority;
    job->submit_time = e->submit_time;
    job->begin_time = e->begin_time;
    job->term_time = e->term_time;
    job->run_limit = e->run_limit;
    job->run_start = e->run_start;

    job->res.num_cpus = e->num_cpu;
    job->res.num_hosts = e->num_hosts;
    job->res.num_gpus = e->num_gpus;
    job->res.mem_mb = e->mem_mb;
    job->res.storage_mb = e->storage_mb;
    ll_strlcpy(job->res.gpu_model, e->gpu_model, sizeof(job->res.gpu_model));

    machines_hash_populate(&job->res.machines, e->machines);
    if (job->res.machines.nentries > 0)
        job->res.num_hosts = (int32_t) job->res.machines.nentries;
    /* compaction writes these back, see compact_write_job_new() */
    ll_strlcpy(job->res.machines_str, e->machines,
               sizeof(job->res.machines_str));
    ll_strlcpy(job->res.tokenpool_str, e->tokenpool,
               sizeof(job->res.tokenpool_str));
    /* a pool missing from the config is kept by name, the job becomes
     * an orphan, see job_is_orphan() */
    if (job_parse_tokens(job, e->tokenpool, 0) < 0)
        LL_ERRX("job=%ld invalid token spec=%s", e->job_id, e->tokenpool);

    job->flags = e->flags;
    ll_strlcpy(job->name, e->job_name, sizeof(job->name));
    ll_strlcpy(job->user, e->username, sizeof(job->user));
    ll_strlcpy(job->project, e->project_name, sizeof(job->project));

    job->queue = replay_queue(e->queue);

    // job owned storage for hosts pointers
    job->run_hosts = calloc(job->res.num_hosts, sizeof(struct mbd_host *));
    if (job->run_hosts == NULL) {
        LL_ERR("calloc run_hosts with num_hosts=%d failed", job->res.num_hosts);
        job->res.num_hosts = 0;
        job_free(job);
        return NULL;
    }

    job_replay_deps(job, e->depend_cond);

    return job;
}

static int replay_insert(struct job_data *job)
{
    char key[LL_BUFSIZ_32];
    snprintf(key, sizeof(key), "%ld", job->job_id);
    if (ll_hash_insert(&job_id_hash, key, job, 0) < 0) {
        LL_ERR("job=%ld hash insert failed", job->job_id);
        job_free(job);
        return 0;
    }
    job_set_list(job, &pend_jobs_list, JOB_LIST_PEND);
    return 1;
}

static int replay_service_job_new(struct job_data *job,
                                  const struct log_job_new *e)
{
    struct service_data *svc = replay_service(e->service_name);

    struct service_instance *inst;
    inst = calloc(1, sizeof(*inst));
    if (inst == NULL) {
        LL_ERR("job=%ld cannot allocate service instance, dying",
               job->job_id);
        mbd_die(MBD_EXIT_EVENTS);
    }

    inst->svc = svc;
    inst->job_id = job->job_id;
    inst->uid = job->uid;
    /* no client is waiting on a replayed instance, calloc's 0 is a
     * valid chan_id */
    inst->chan_id = -1;
    ll_strlcpy(inst->token, e->service_token, sizeof(inst->token));

    job->svc_inst = inst;
    inst->status = SVC_PENDING;
    ll_list_append(&svc->instances, &inst->ent);

    LL_DEBUG("job=%ld uid=%u", job->job_id, job->uid);

    return 0;
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

    int rc = replay_insert(job);
    if (rc == 0) {
        LL_ERR("failed insert job=%ld", e.job_id);
        return 0;
    }

    LL_DEBUG("JOB_NEW job=%ld array_id=%ld array_index=%d depend=%s "
             "flags=0x%x", e.job_id, e.array_id, e.array_index,
             (e.depend_cond[0] != 0) ? e.depend_cond : "none",
             e.flags);

    if (job->flags & JOB_FLAG_SERVICE)
        replay_service_job_new(job, &e);

    return rc;
}

static int replay_set_run_hosts(struct job_data *job,
                                const struct log_job_start *e)
{
    char hosts[LL_BUFSIZ_4K];

    ll_strlcpy(hosts, e->run_hosts, sizeof(hosts));

    char *tok = strtok(hosts, " \t,");
    while (tok != NULL) {
        struct mbd_host *h = replay_host(tok);

        if (job->run_nhosts >= e->nhosts) {
            LL_ERRX("job=%ld too many hosts run=%d e=%d", job->job_id,
                    job->run_nhosts, e->nhosts);
            break;
        }

        // run_hosts array storage allocated when replay job_new
        job->run_hosts[job->run_nhosts] = h;
        job->run_nhosts++;

        tok = strtok(NULL, " \t,");
    }

    if (job->run_nhosts != e->nhosts) {
        LL_ERRX("JOB_START job=%ld expected_nhosts=%d got=%d", job->job_id,
                e->nhosts, job->run_nhosts);
        assert(0);
        return -1;
    }

    return 0;
}

static void replay_service_job_start(struct job_data *job)
{
    /* port arrives with JOB_FORK, picked by sbd */

    ll_strlcpy(job->svc_inst->run_host,
               job->run_hosts[0]->net.name,
               sizeof(job->svc_inst->run_host));
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
        LL_ERR("JOB_START job=%ld not found", e.job_id);
        return;
    }

    if (replay_set_run_hosts(job, &e) < 0) {
        LL_ERRX("job=%ld JOB_START run hosts do not match nhosts=%d",
                e.job_id, e.nhosts);
        return;
    }

    job->state = JOB_RUNNING;
    job->dispatch_time = e.dispatch_time;
    ll_strlcpy(job->gpu_assigned, e.gpu_assigned, sizeof(job->gpu_assigned));
    job_move_list(job, &pend_jobs_list, &run_jobs_list, JOB_LIST_RUN);

    LL_DEBUG("JOB_START job=%ld nhosts=%d cpus=%d gpus=%d gpu_assigned=%s",
             e.job_id, e.nhosts, e.cpus_per_host, e.gpus_per_host,
             (e.gpu_assigned[0] != 0) ? e.gpu_assigned : "none");

    if (job_is_service(job))
        replay_service_job_start(job);
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
        LL_ERR("JOB_FORK job=%ld not found", e.job_id);
        return;
    }
    job->pid = (pid_t) e.job_pid;
    job->fork_time = e.fork_time;
    /* first fork, unless compaction already restored it in JOB_NEW */
    if (job->run_start == 0)
        job->run_start = e.fork_time;
    if (job_is_service(job)) {
        job->svc_inst->port = e.service_port;
        job->svc_inst->status = SVC_RUNNING;
    }

    LL_DEBUG("JOB_FORK job=%ld pid=%d port=%d", e.job_id, e.job_pid,
             e.service_port);
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
        LL_ERR("JOB_SIGNAL job=%ld not found", e.job_id);
        return;
    }
    job->signal_time = e.signal_time;
    /* snapshot of the flags at signal time, e.g. JOB_FLAG_RELOCATABLE
     * cleared by bservices -d */
    job->flags = e.job_flags;
    LL_DEBUG("JOB_SIGNAL job=%ld sig=%d uid=%u flags=0x%x", e.job_id,
             e.signal_num, e.uid, e.job_flags);
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
        LL_ERR("JOB_FINISH job=%ld not found", e.job_id);
        return;
    }

    job->state = e.state;
    job->exit_status = e.exit_status;
    job->uid = e.uid;
    job->end_time = e.end_time;

    // The job was running, or pending and then bkilled
    struct ll_list *from = &pend_jobs_list;
    if (job->list_id == JOB_LIST_RUN)
        from = &run_jobs_list;
    job_move_list(job, from, &finish_jobs_list, JOB_LIST_FINISH);
    job_array_element_finished(job);

    LL_DEBUG("JOB_FINISH job=%ld", e.job_id);
    /* No counter updates. These updates are performed only after
     * the full replay and only for jobs in pending or running lists.
     */
    if (job_is_service(job))
        job->svc_inst->status = SVC_FINISH;
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
        LL_ERRX("JOB_PEND_SUSP job=%ld not found", e.job_id);
        return;
    }
    if (!(job->state == JOB_PENDING)) {
        LL_ERRX("JOB_PEND_SUSP job=%ld not in PEND", e.job_id);
        assert(0);
        return;
    }
    job->state = JOB_HELD;
    LL_DEBUG("JOB_PEND_SUSP job=%ld", e.job_id);
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
        LL_ERRX("JOB_RESUME job=%ld not found", e.job_id);
        return;
    }
    if (!(job->state == JOB_HELD)) {
        LL_ERRX("JOB_PENDING_RESUME job=%ld not in PEND_SUSP", e.job_id);
        assert(0);
        return;
    }
    job->state = JOB_PENDING;
    job->flags &= ~JOB_FLAG_HOLD;
    LL_DEBUG("JOB_RESUME job=%ld", e.job_id);
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
        LL_ERRX("JOB_SUSP job=%ld not found", e.job_id);
        return;
    }
    job->state = JOB_SUSPENDED;
    job->susp_time = e.event_time;
    LL_DEBUG("JOB_SUSP job=%ld", e.job_id);
}

static void replay_job_resume(const struct event_rec *rec)
{
    struct log_job_resume e;
    if (log_parse_job_resume(rec, &e) < 0) {
        LL_ERR("parse JOB_RESUME failed");
        return;
    }
    struct job_data *job = job_find(e.job_id);
    if (job == NULL) {
        LL_ERRX("JOB_RESUME job=%ld not found", e.job_id);
        return;
    }
    job->state = JOB_RUNNING;
    job->susp_time = 0;
    LL_DEBUG("JOB_RESUME job=%ld", e.job_id);
}

/*
 * manifest_seq_scan - derive the current sequence number by scanning the
 * mbd directory for existing manifest.NNNNNN archives.
 *
 * No external sequence file is needed. Admins may delete old archives
 * freely without having to update any state file. The next compact will
 * simply use the highest sequence number found + 1.
 */
static void manifest_seq_scan(void)
{
    char dir[PATH_MAX];
    snprintf(dir, sizeof(dir), "%s/mbd", ll_params[LL_STATE_DIR].val);

    DIR *dp = opendir(dir);
    if (dp == NULL)
        return; /* first run, seq stays 0 */

    struct dirent *de;
    while ((de = readdir(dp)) != NULL) {
        if (strncmp(de->d_name, "manifest.", 9) != 0)
            continue;
        const char *p = de->d_name + 9;
        if (*p == '\0')
            continue;
        /* must be all digits */
        const char *q = p;
        while (*q >= '0' && *q <= '9')
            q++;
        if (*q != '\0')
            continue;
        uint32_t n = (uint32_t) atol(p);
        if (n > manifest_seq)
            manifest_seq = n;
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
        LL_ERR("mkdir(%s) failed", dir);
        mbd_die(MBD_EXIT_FATAL);
    }
    LL_INFO("working dir initialized %s", dir);

    /* our own directory, also on a shared LL_STATE_DIR: a core
     * dump lands here when kernel.core_pattern writes to the cwd */
    if (chdir(dir) < 0)
        LL_ERR("chdir(%s) failed", dir);

    n = snprintf(manifest_path, sizeof(manifest_path), "%s/manifest", dir);
    if (n < 0 || n >= (int) sizeof(manifest_path))
        mbd_die(MBD_EXIT_EVENTS);
    LL_INFO("job events initialized %s", manifest_path);

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

    if (!ll_atoi(ll_params[LL_MBD_JOB_FINISH_THRESHOLD].val,
                 &job_finish_threshold)) {
        LL_ERRX("failed parsing LL_MBD_JOB_FINISH_THRESHOLD=%s using "
                "default=1000 jobs",
                ll_params[LL_MBD_JOB_FINISH_THRESHOLD].val);
    }

    if (job_finish_threshold < 0) {
        LL_ERRX(
            "LL_MBD_JOB_FINISH_THRESHOLD=%d must be greater than or equal 0 "
            "using default job_finish_threshold = 1000",
            job_finish_threshold);
        job_finish_threshold = 1000;
    }

    if (!ll_atoi(ll_params[LL_MBD_JOB_FINISH_PERIOD].val,
                 &job_finish_period) || job_finish_period < 0) {
        LL_ERRX("invalid LL_MBD_JOB_FINISH_PERIOD=%s using default=3600 "
                "seconds", ll_params[LL_MBD_JOB_FINISH_PERIOD].val);
        job_finish_period = 3600;
    }
    LL_INFO("job_finish_threshold=%d job_finish_period=%d",
            job_finish_threshold, job_finish_period);

    manifest_seq_scan();

    LL_INFO("manifest seq initialized seq=%u", manifest_seq);

    return 0;
}

/*
 * job_id_seq_read - read the persisted job_id_seq at startup.
 *
 * Sets job_id_seq to max(current, persisted) so the event log replay
 * value and the persisted value are both respected. If the file does
 * not exist (first run) the value stays at whatever replay set it to.
 *
 * Resist the temptation to delete this file as compact may leave a
 * manifest empty file so the job_id would go backwards.
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
        LL_INFO("job_id_seq: restoring from file seq=%ld (was %ld)", seq,
                job_id_seq);
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
        LL_ERRX("JOB_MOVE job=%ld not found", e.job_id);
        return;
    }
    job->queue = replay_queue(e.to_queue);
    LL_DEBUG("JOB_MOVE job=%ld from=%s to=%s", e.job_id, e.from_queue,
             e.to_queue);
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
        LL_ERRX("JOB_PRIORITY job=%ld not found", e.job_id);
        return;
    }
    job->priority = e.new_priority;
    LL_DEBUG("JOB_PRIORITY job=%ld old=%d new=%d", e.job_id, e.old_priority,
             e.new_priority);
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
        LL_ERRX("JOB_PEND job=%ld not found", e.job_id);
        return;
    }
    assert(job->list_id == JOB_LIST_RUN
           || job->list_id == JOB_LIST_FINISH);

    if (job->list_id == JOB_LIST_RUN)
        job_move_list(job, &run_jobs_list, &pend_jobs_list, JOB_LIST_PEND);
    else
        job_move_list(job, &finish_jobs_list, &pend_jobs_list, JOB_LIST_PEND);

    job->pid = 0;
    job->fork_time = 0;
    job->dispatch_time = 0;
    job->pid_wait_time = 0;
    job->end_time = 0;
    job->exit_status = 0;
    job->state = JOB_PENDING;
    job->run_nhosts = 0;

    /* service restart: the endpoint belonged to the old incarnation */
    if (job_is_service(job)) {
        job->svc_inst->status = SVC_PENDING;
        job->svc_inst->port = 0;
        job->svc_inst->run_host[0] = 0;
    }

    LL_DEBUG("JOB_PEND job=%ld", e.job_id);
}

int jobs_replay(void)
{
    ll_hash_init(&stale_queue_hash, 0);
    ll_hash_init(&stale_service_hash, 0);
    ll_hash_init(&stale_host_hash, 0);

    FILE *fp = fopen(manifest_path, "r");
    if (fp == NULL) {
        if (errno == ENOENT) {
            LL_INFO("replay: no manifest, nothing to replay");
            return 0;
        }
        LL_ERR("fopen %s", manifest_path);
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
        if (rec.type == EVENT_JOB_RESUME) {
            replay_job_resume(&rec);
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
        LL_ERR("I/O error reading job manifest=%s line=%d", manifest_path,
               lineno);
        mbd_die(MBD_EXIT_EVENTS);
    }

    fclose(fp);

    if (max_id > job_id_seq)
        job_id_seq = max_id;

    /* persisted seq may be higher than replay if manifest was fully
     * compacted -- take the max so job_id never goes backwards
     */
    job_id_seq_read();

    replay_mark_orphans();
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
    e.array_id = job->array_id;
    e.array_index = job->array_index;
    e.array_start = job->array_start;
    e.array_end = job->array_end;
    e.array_stride = job->array_stride;
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
    ll_strlcpy(e.gpu_model, job->res.gpu_model, sizeof(e.gpu_model));
    ll_strlcpy(e.username, job->user, sizeof(e.username));
    ll_strlcpy(e.job_name, job->name, sizeof(e.job_name));
    ll_strlcpy(e.queue, job->queue->name, sizeof(e.queue));
    ll_strlcpy(e.project_name, job->project, sizeof(e.project_name));
    ll_strlcpy(e.tokenpool, job->res.tokenpool_str, sizeof(e.tokenpool));
    ll_strlcpy(e.machines, job->res.machines_str, sizeof(e.machines));
    ll_strlcpy(e.depend_cond, job->depend_cond, sizeof(e.depend_cond));
    e.run_limit = job->run_limit;
    e.run_start = job->run_start;

    if (job_is_service(job)) {
        ll_strlcpy(e.service_name, job->svc_inst->svc->name,
                   sizeof(e.service_name));
        ll_strlcpy(e.service_token, job->svc_inst->token,
                   sizeof(e.service_token));
    }

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
    ll_strlcpy(e.gpu_model, job->res.gpu_model, sizeof(e.gpu_model));

    for (int i = 0; i < job->res.num_hosts; i++) {
        if (i > 0)
            ll_strlcat(e.run_hosts, " ", sizeof(e.run_hosts));
        ll_strlcat(e.run_hosts, job->run_hosts[i]->net.name, sizeof(e.run_hosts));
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
    if (job->svc_inst != NULL)
        e.service_port = job->svc_inst->port;

    if (log_write_job_fork(fp, &e) < 0)
        mbd_die(MBD_EXIT_EVENTS);
}

/* a suspended running job stays suspended across the rewrite */
static void compact_write_job_susp(FILE *fp, const struct job_data *job)
{
    if (job->state != JOB_SUSPENDED)
        return;

    struct log_job_susp e;
    memset(&e, 0, sizeof(e));
    e.job_id = job->job_id;
    e.event_time = job->susp_time;

    if (log_write_job_susp(fp, &e) < 0)
        mbd_die(MBD_EXIT_EVENTS);
}

static void compact_write_job_finish(FILE *fp, const struct job_data *job)
{
    struct log_job_finish e;
    memset(&e, 0, sizeof(e));

    e.job_id = job->job_id;
    e.uid = job->uid;
    e.state = job->state;
    e.exit_status = job->exit_status;
    e.end_time = job->end_time;

    if (log_write_job_finish(fp, &e) < 0)
        mbd_die(MBD_EXIT_EVENTS);
}

/*
 * compact_write_job_finished - re-log a finished job that compaction is
 * retaining in memory (dep_refcnt or array_element_cnt), so it survives
 * a restart. Without this its JOB_NEW only lives in the archived
 * manifest, which jobs_replay() never reads -- the job silently
 * vanishes from job_id_hash even though something still needs it.
 *
 * dispatch_time gates JOB_START: a job can reach finish_jobs_list
 * without ever starting (killed while still pending), and emitting a
 * JOB_START it never had would corrupt replay state.
 *
 * fork_time gates JOB_FORK independently of dispatch_time: a dispatched
 * job can be rejected by sbd and never fork at all, so a started job
 * with no fork is a real, expected case -- not a state to assert against.
 */
static void compact_write_job_finished(FILE *fp, const struct job_data *job)
{
    compact_write_job_new(fp, job);
    if (job->dispatch_time) {
        compact_write_job_start(fp, job);
        if (job->fork_time)
            compact_write_job_fork(fp, job);
    }
    compact_write_job_finish(fp, job);
}

/*
 * job_id_seq_write - persist the current job_id_seq to disk.
 *
 * Called after every job submission so that mbd restart after a full
 * compaction (empty manifest) still resumes from the correct sequence
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

static void manifest_rebuild(void)
{
    char archived[PATH_MAX + LL_BUFSIZ_32];
    /*
     * Sequence number is derived by manifest_seq_scan() at startup from
     * the filenames already present in the directory. No separate sequence
     * file is kept, so admins may delete old archives freely without
     * having to update any state.
     */
    manifest_seq++;
    snprintf(archived, sizeof(archived), "%s.%u", manifest_path, manifest_seq);

    if (rename(manifest_path, archived) < 0) {
        LL_ERR("rename(%s, %s)", manifest_path, archived);
        mbd_die(MBD_EXIT_EVENTS);
    }

    int fd = open(manifest_path, O_CREAT | O_WRONLY | O_APPEND, 0640);
    if (fd < 0) {
        LL_ERR("open=%s", manifest_path);
        mbd_die(MBD_EXIT_EVENTS);
    }

    FILE *fp = fdopen(fd, "a");
    if (!fp) {
        LL_ERR("fdopen=%s", manifest_path);
        close(fd);
        mbd_die(MBD_EXIT_EVENTS);
    }

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
        compact_write_job_susp(fp, job);
    }

    /* orphans are written back exactly as replay rebuilt them, with
     * their stale names, so a fixed config heals them on the next
     * replay. dispatch_time and fork_time gate START and FORK as in
     * compact_write_job_finished().
     */
    for (e = orphan_jobs_list.head; e; e = e->next) {
        struct job_data *job = (struct job_data *) e;
        compact_write_job_new(fp, job);
        if (job->dispatch_time) {
            compact_write_job_start(fp, job);
            if (job->fork_time)
                compact_write_job_fork(fp, job);
            compact_write_job_susp(fp, job);
        }
    }

    /*
     * Every finished job is dropped from memory on compaction unless
     * something still needs it: a pending job's dependency expression
     * still references it, or it's an array head with elements still
     * running/pending. No partial retain window -- either a job is
     * still referenced or it isn't.
     */
    /*
     * A finished job younger than job_finish_period is kept and written
     * back, so recent finished jobs survive a restart: replay never
     * reads the archived manifest. Older ones are dropped, unless
     * something still needs them. Every job is checked on its own, the
     * list is not strictly in end_time order: jobs retained by a
     * dependency or an array head stay where they were.
     */
    time_t now = time(NULL);
    int dropped = 0;
    int kept = 0;

    for (e = finish_jobs_list.head; e; e = next) {
        next = e->next;

        struct job_data *job = (struct job_data *) e;

        if (now - job->end_time < job_finish_period) {
            compact_write_job_finished(fp, job);
            kept++;
            continue;
        }

        /* still referenced by a pending job's dependency expression,
         * purging it now would leave that job unable to ever resolve */
        if (job->dep_refcnt > 0) {
            LL_DEBUG("job=%ld retained by compaction dep_refcnt=%d",
                     job->job_id, job->dep_refcnt);
            compact_write_job_finished(fp, job);
            kept++;
            continue;
        }

        /* array head: other elements may still be pending/running and
         * need job_find(array_id) to resolve array_start/end/stride */
        if (job->array_id == job->job_id && job->array_id != 0 &&
            job->array_element_cnt > 0) {
            LL_DEBUG("job=%ld retained by compaction array_element_cnt=%d",
                     job->job_id, job->array_element_cnt);
            compact_write_job_finished(fp, job);
            kept++;
            continue;
        }

        ll_list_remove(&finish_jobs_list, &job->ent);

        char key[LL_BUFSIZ_32];
        sprintf(key, "%ld", job->job_id);

        struct job_data *j2 = ll_hash_remove(&job_id_hash, key);
        assert(j2 == job);

        job_free(job);
        dropped++;
    }

    LL_INFO("compaction finished jobs dropped=%d kept=%d period=%d",
            dropped, kept, job_finish_period);

    if (fflush(fp) != 0 || fsync(fileno(fp)) != 0)
        mbd_die(MBD_EXIT_EVENTS);

    /* new manifest file has a new inode -- update tracking so
     * open_manifest() does not falsely detect integrity loss */
    struct stat st;
    if (fstat(fileno(fp), &st) < 0) {
        LL_ERR("fstat manifest after compact");
        mbd_die(MBD_EXIT_EVENTS);
    }
    manifest_ino = st.st_ino;

    fclose(fp);

    LL_INFO("manifest rebuild seq=%u archived=%s", manifest_seq, archived);
}

/* Rebuild the event log when the finished jobs a rebuild can drop,
 * those that ended more than job_finish_period ago, reach the
 * threshold. Younger ones are kept and written back: counting them
 * too would rewrite the same jobs at every check under steady load.
 * Checked every MANIFEST_CHECK_TIME seconds.
 */
void maybe_rebuild_manifest(void)
{
    static time_t last_check;
    time_t now = time(NULL);

    if (now - last_check < MANIFEST_CHECK_TIME)
        return;
    last_check = now;

    int old = 0;
    for (struct ll_list_entry *e = finish_jobs_list.head; e != NULL;
         e = e->next) {
        struct job_data *job = (struct job_data *) e;
        if (now - job->end_time >= job_finish_period)
            old++;
    }

    if (old < job_finish_threshold)
        return;

    LL_DEBUG("manifest rebuild triggered finished_jobs=%d old=%d "
             "threshold=%d", ll_list_count(&finish_jobs_list), old,
             job_finish_threshold);

    manifest_rebuild();
}

void event_job_move(const struct job_data *job, const char *to_queue)
{
    struct log_job_move e;

    memset(&e, 0, sizeof(e));
    e.job_id = job->job_id;
    e.event_time = time(NULL);
    ll_strlcpy(e.from_queue, job->queue->name, sizeof(e.from_queue));
    ll_strlcpy(e.to_queue, to_queue, sizeof(e.to_queue));

    FILE *fp = open_manifest();
    if (log_write_job_move(fp, &e) < 0) {
        fclose(fp);
        LL_ERR("log_write_job_move failed job=%ld", job->job_id);
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

    FILE *fp = open_manifest();
    if (log_write_job_priority(fp, &e) < 0) {
        fclose(fp);
        LL_ERR("log_write_job_priority failed job=%ld", job->job_id);
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

    FILE *fp = open_manifest();
    if (log_write_job_pend(fp, &e) < 0) {
        fclose(fp);
        LL_ERR("log_write_job_pend failed job=%ld", job->job_id);
        mbd_die(MBD_EXIT_EVENTS);
    }
    fclose(fp);
}
