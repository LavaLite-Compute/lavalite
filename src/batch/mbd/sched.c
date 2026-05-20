/*
 * Copyright (C) LavaLite Contributors
 * GPL v2
 */

#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <assert.h>

#include "base/lib/ll.list.h"
#include "base/lib/ll.hash.h"
#include "base/lib/ll.syslog.h"
#include "batch/mbd/mbd.h"

#define SCHED_SORT_BUF_MAX 131072   /* 1M of pointers — no alloc in hot path */
static struct ll_list_entry *sort_buf[SCHED_SORT_BUF_MAX];


static int pend_job_cmp(const void *a, const void *b)
{
    const struct job_data *ja = *(const struct job_data **)a;
    const struct job_data *jb = *(const struct job_data **)b;
    int r;

    /* 1. queue priority (higher = first) */
    r = jb->queue->priority - ja->queue->priority;
    if (r != 0)
        return r;

    /* 2. job priority */
    r = jb->priority - ja->priority;
    if (r != 0)
        return r;

    /* 3. job_id: lower = older = first */
    if (ja->job_id < jb->job_id)
        return -1;

    if (ja->job_id > jb->job_id)
        return 1;

    return 0;
}

static int host_plan_cmp(const void *a, const void *b)
{
    const struct mbd_host *ha = *(const struct mbd_host **)a;
    const struct mbd_host *hb = *(const struct mbd_host **)b;

    return ha->res.free_cpu - hb->res.free_cpu;
}

static int mark_candidates(void)
{
    struct ll_list_entry *e;
    int n = 0;

    for (e = host_list.head; e; e = e->next) {
        struct mbd_host *h = (struct mbd_host *)e;

        h->candidate = 0;

        if (h->state != HOST_OK)
            continue;
        if (h->sbd_chan < 0)
            continue;
        if (h->num_jobs >= h->res.max_jobs)
            continue;

        h->candidate = 1;
        ++n;
    }

    return n;
}

static int is_depend_ok(const struct job_data *job)
{
    (void)job;
    // for dependencies are ok
    return 1;
}


static int job_is_ready(const struct job_data *job)
{
    if (!(job->state == JOB_PENDING))
        return 0;
    if (job->begin_time > 0 && job->begin_time > time(NULL))
        return 0;
    if (! is_depend_ok(job))
        return 0;

    return 1;
}

static int host_in_queue_group(const struct mbd_host *h,
                               const struct job_data *job)
{
    LS_DEBUG("job=%ld is host=%s member of queue=%s", job->job_id,
             h->net.name, job->queue->name);
    return ll_hash_contains(&job->queue->host_hash, h->net.name);
}

static int host_has_gpu(const struct mbd_host *h, const struct job_data *job)
{
    struct mbd_gpu *g = ll_hash_search(&h->res.gpu_hash, job->res.gpu_type);
    if (g == NULL)
        return 0;
    LS_DEBUG("job=%ld host=%s has gpu type=%s", job->job_id,
                 h->net.name, job->res.gpu_type);

    return g->free >= job->res.num_gpus;
}

static enum pend_reason diag_reason(struct pend_diag *diag)
{
    if (diag->exclusive)
        return PEND_HOST_EXCLUSIVE;
    if (diag->gpu_type)
        return PEND_GPU_TYPE;
    if (diag->no_gpus)
        return PEND_NOT_ENOUGH_GPUS;
    if (diag->no_mem)
        return PEND_NOT_ENOUGH_MEM;
    if (diag->no_storage)
        return PEND_NOT_ENOUGH_STORAGE;
    if (diag->no_cpus)
        return PEND_NOT_ENOUGH_CPUS;
    if (diag->not_in_queue)
        return PEND_NO_HOSTS;
    return PEND_NO_HOSTS;
}

#define SCHED_PLAN_MAX 1024
static struct mbd_host *host_plan[SCHED_PLAN_MAX];

static int build_host_plan(struct job_data *job, struct pend_diag *diag)
{
    struct ll_list_entry *e;
    int n = 0;

    memset(host_plan, 0, sizeof(host_plan));
    memset(diag, 0, sizeof(*diag));

    for (e = host_list.head; e; e = e->next) {
        if (n >= SCHED_PLAN_MAX) {
            LS_ERRX("candidate host overflow max=%d", SCHED_PLAN_MAX);
            break;
        }
        struct mbd_host *h = (struct mbd_host *)e;

        if (!h->candidate)
            continue;
        if (h->exclusive) {
            diag->exclusive++;
            continue;
        }
        if ((job->flags & JOB_FLAG_EXCLUSIVE) && h->num_jobs > 0) {
            diag->exclusive++;
            continue;
        }
        if (!host_in_queue_group(h, job)) {
            diag->not_in_queue++;
            continue;
        }
        if (h->res.free_cpu < job->res.num_cpus) {
            diag->no_cpus++;
            continue;
        }
        if (h->res.free_mem_mb < job->res.mem_mb) {
            diag->no_mem++;
            continue;
        }
        if (h->res.free_storage_mb < job->res.storage_mb) {
            diag->no_storage++;
            continue;
        }
        if (job->res.num_gpus > 0 && !host_has_gpu(h, job)) {
            diag->no_gpus++;
            continue;
        }
        if (job->res.machines.nentries > 0 &&
            !ll_hash_contains(&job->res.machines, h->net.name)) {
            diag->not_in_queue++;
            continue;
        }
        host_plan[n] = h;
        ++n;
    }

    if (n < job->res.num_hosts) {
        LS_DEBUG("job=%ld need=%d found=%d hosts", job->job_id,
                 job->res.num_hosts, n);
        return 0;
    }

    qsort(host_plan, n, sizeof(struct mbd_host *), host_plan_cmp);
    job->run_nhosts = 0;
    for (int i = 0; i < job->res.num_hosts; i++) {
        job->run_hosts[i] = host_plan[i];
        job->run_nhosts++;
    }
    assert(job->run_nhosts == job->res.num_hosts);
    LS_INFO("job=%ld exec_host=%s nhosts=%d cpus_per_host=%d gpus_per_host=%d",
            job->job_id, job->run_hosts[0]->net.name, job->res.num_hosts,
            job->res.num_cpus, job->res.num_gpus);
    return 1;
}

static void host_update_resources(const struct job_data *job)
{
    int i;

    for (i = 0; i < job->run_nhosts; i++) {
        struct mbd_host *h = job->run_hosts[i];

        h->res.free_cpu        -= job->res.num_cpus;
        h->res.free_mem_mb     -= job->res.mem_mb;
        h->res.free_storage_mb -= job->res.storage_mb;
        h->num_jobs++;
        h->num_run++;
        h->num_cpus_used += job->res.num_cpus;

        if (job->flags & JOB_FLAG_EXCLUSIVE)
            h->exclusive = 1;

        if (job->res.num_gpus > 0) {
            struct mbd_gpu *g = ll_hash_search(&h->res.gpu_hash,
                                               job->res.gpu_type);
            if (g == NULL) {
                LS_ERRX("job=%ld host=%s gpu_type=%s not found",
                        job->job_id, h->net.name, job->res.gpu_type);
                continue;
            }
            g->free         -= job->res.num_gpus;
            h->res.free_gpu -= job->res.num_gpus;
        }

        LS_DEBUG("host=%s num_jobs=%d free_cpu=%d free_mem_mb=%lu "
                 "free_storage_mb=%lu free_gpu=%d",
                 h->net.name, h->num_jobs, h->res.free_cpu, h->res.free_mem_mb,
                 h->res.free_storage_mb, h->res.free_gpu);
    }
}

static int tokens_available(const struct job_data *job)
{
    struct ll_list_entry *e;

    for (e = job->res.tokens.head; e != NULL; e = e->next) {
        struct job_token *t = (struct job_token *)e;
        struct mbd_token_pool *p;

        p = ll_hash_search(&token_pool_name_hash, t->name);
        if (p == NULL) {
            LS_ERRX("job=%ld token pool=%s not found", job->job_id, t->name);
            return 0;
        }
        if (p->free < t->count) {
            LS_DEBUG("no tokens for job=%ld pool=%s need=%d free=%d",
                     job->job_id, t->name, t->count, p->free);
            return 0;
        }
    }
    return 1;
}

void schedule(void)
{
    LS_DEBUG("num_pend_jobs=%d", ll_list_count(&pend_jobs_list));
    if (ll_list_is_empty(&pend_jobs_list))
        return;

    int cc = mark_candidates();
    LS_DEBUG("got number=%d of candidates hosts", cc);
    if (cc == 0)
        return;

    ll_list_sort_buf(&pend_jobs_list, sort_buf, pend_job_cmp);

    struct ll_list_entry *e = pend_jobs_list.head;
    while (e) {
        struct job_data *job = (struct job_data *)e;
        e = e->next;

        LS_DEBUG("is job=%ld ready for scheduling", job->job_id);
        if (!job_is_ready(job)) {
            job->pend_reason = PEND_JOB_NOT_READY;
            continue;
        }

        if (! tokens_available(job)) {
            job->pend_reason = PEND_TOKENS;
            continue;
        }

        LS_DEBUG("job=%ld is ready for scheduling", job->job_id);
        struct pend_diag diag;
        if (build_host_plan(job, &diag) == 0) {
            job->pend_reason = diag_reason(&diag);
            LS_INFO("job=%ld not enough hosts found", job->job_id);
            continue;
        }

        job->pend_reason = PEND_NONE;
        if (mbd_dispatch_job(job) < 0) {
            LS_ERRX("job=%ld dispatch failed", job->job_id);
            continue;
        }

        // udpate host and queue counters and resources
        host_update_resources(job);
        token_alloc(job);

        job->queue->num_run++;
        job->queue->num_pend--;
        job->queue->num_cpus_used += job->res.num_cpus * job->run_nhosts;
        job->queue->num_hosts_used += job->run_nhosts;

        LS_DEBUG("queue=%s num_pend=%d num_run=%d num_susp=%d",
                 job->queue->name, job->queue->num_pend,
                 job->queue->num_run, job->queue->num_susp);
    }
    mbd_assert_counters();
}

void token_alloc(const struct job_data *job)
{
    struct ll_list_entry *e;

    for (e = job->res.tokens.head; e != NULL; e = e->next) {
        struct job_token *t = (struct job_token *)e;
        struct mbd_token_pool *p;

        p = ll_hash_search(&token_pool_name_hash, t->name);
        if (p == NULL) {
            LS_ERRX("job=%ld pool=%s not found", job->job_id, t->name);
            continue;
        }
        p->free -= t->count;
    }
}

void token_free(const struct job_data *job)
{
    struct ll_list_entry *e;

    for (e = job->res.tokens.head; e != NULL; e = e->next) {
        struct job_token *t = (struct job_token *)e;
        struct mbd_token_pool *p;

        p = ll_hash_search(&token_pool_name_hash, t->name);
        if (p == NULL) {
            LS_ERRX("job=%ld pool=%s not found", job->job_id, t->name);
            continue;
        }
        p->free += t->count;
    }
}
