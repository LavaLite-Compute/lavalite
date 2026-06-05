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

#define SCHED_SORT_BUF_MAX 131072 /* 1M of pointers — no alloc in hot path */
static struct ll_list_entry *sort_buf[SCHED_SORT_BUF_MAX];

static int pend_job_cmp(const void *a, const void *b)
{
    const struct job_data *ja = *(const struct job_data **) a;
    const struct job_data *jb = *(const struct job_data **) b;
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
    const struct mbd_host *ha = *(const struct mbd_host **) a;
    const struct mbd_host *hb = *(const struct mbd_host **) b;

    return ha->res.free_cpu - hb->res.free_cpu;
}

static int mark_candidates(void)
{
    struct ll_list_entry *e;
    int n = 0;

    for (e = host_list.head; e; e = e->next) {
        struct mbd_host *h = (struct mbd_host *) e;

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
    (void) job;
    // for dependencies are ok
    return 1;
}

static int job_is_ready(const struct job_data *job)
{
    if (!(job->state == JOB_PENDING))
        return 0;
    if (job->begin_time > 0 && job->begin_time > time(NULL))
        return 0;
    if (!is_depend_ok(job))
        return 0;

    return 1;
}

static int host_in_queue_group(const struct mbd_host *h,
                               const struct job_data *job)
{
    int n = ll_hash_contains(&job->queue->host_hash, h->net.name);
    if (n == 1) {
        LL_DEBUG("job=%ld host=%s is member of queue=%s", job->job_id,
                 h->net.name, job->queue->name);
        return 1;
    }
    LL_DEBUG("job=%ld host=%s is not member of queue=%s", job->job_id,
             h->net.name, job->queue->name);
    return 0;
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

static struct mbd_host **host_plan;

static int build_plan_array(void)
{
    if (host_plan)
        return 0;

    int n = ll_list_count(&host_list);
    host_plan = calloc(n, sizeof(struct mbd_host *));
    if (host_plan == NULL) {
        LL_ERR("calloc n=%d *hosts failed", n);
        return -1;
    }
    return 0;
}

static int host_has_gpu_count(const struct mbd_host *h,
                               const struct job_data *job)
{
    int n = gpu_ids_count_free(&h->res.gpu);
    LL_DEBUG("job=%ld host=%s count=%d free=%d", job->job_id, h->net.name,
             h->res.gpu.count, n);
    if (n >= job->res.num_gpus)
        return n;
    return 0;
}

static int host_has_gpu_type_and_count(const struct mbd_host *h,
                                        const struct job_data *job)
{
    if (strcmp(h->res.gpu.gpu_type, job->res.gpu_type) != 0)
        return 0;
    int n = gpu_ids_count_free(&h->res.gpu);
    LL_DEBUG("job=%ld host=%s gpu_type=%s count=%d free=%d", job->job_id,
             h->net.name, job->res.gpu_type, h->res.gpu.count, n);
    if (n >= job->res.num_gpus)
        return n;
    return 0;
}

static int host_meets_requirements(struct mbd_host *h, struct job_data *job,
                                   struct pend_diag *diag)
{
    if (!h->candidate)
        return 0;
    if (h->exclusive) {
        diag->exclusive++;
        return 0;
    }
    if ((job->flags & JOB_FLAG_EXCLUSIVE) && h->num_jobs > 0) {
        diag->exclusive++;
        return 0;
    }
    if (h->res.free_cpu < job->res.num_cpus) {
        diag->no_cpus++;
        return 0;
    }
    if (h->res.free_mem_mb < job->res.mem_mb) {
        diag->no_mem++;
        return 0;
    }
    if (h->res.free_storage_mb < job->res.storage_mb) {
        diag->no_storage++;
        return 0;
    }
    if (job->res.num_gpus > 0 && !host_has_gpu_count(h, job)) {
        diag->no_gpus++;
        return 0;
    }
    if (job->res.gpu_type[0] != 0 && !host_has_gpu_type_and_count(h, job)) {
        diag->gpu_type++;
        return 0;
    }
    return 1;
}

static void log_run_hosts(const struct job_data *job)
{
    char buf[LL_BUFSIZ_1K];
    int pos = 0;
    int i;

    for (i = 0; i < job->run_nhosts; i++) {
        int n = snprintf(buf + pos, sizeof(buf) - pos, "%d@%s ",
                         job->res.num_cpus,
                         job->run_hosts[i]->net.name);
        if (n < 0 || pos + n >= (int) sizeof(buf))
            break;
        pos += n;
    }
    LL_INFO("job=%ld run_hosts=%s gpus_per_host=%d",
            job->job_id, buf, job->res.num_gpus);
}

// Build specific host plan given the job requested machines
static int build_host_plan_machines(struct job_data *job,
                                    struct pend_diag *diag)
{
    int n = 0;
    int need = job->res.machines.nentries;

    struct ll_hash_iter it;
    struct ll_hash_entry *e;

    ll_hash_iter_init(&it, &job->res.machines);
    while ((e = ll_hash_iter_next(&it)) != NULL) {

        struct mbd_host *h = ll_hash_search(&job->queue->host_hash, e->key);
        if (h == NULL) {
            diag->not_in_queue++;
            continue;
        }
        if (!host_meets_requirements(h, job, diag))
            continue;
        host_plan[n] = h;
        ++n;
    }

    if (n < need) {
        LL_DEBUG("job=%ld machines: need=%d found=%d", job->job_id, need, n);
        return 0;
    }

    job->run_nhosts = 0;
    for (int i = 0; i < need; i++) {
        job->run_hosts[i] = host_plan[i];
        job->run_nhosts++;
    }
    assert(job->run_nhosts == need);
    log_run_hosts(job);

    return 1;
}

static int build_host_plan(struct job_data *job, struct pend_diag *diag)
{
    if (build_plan_array() < 0)
        return -1;

    int num_hosts = ll_list_count(&host_list);
    memset(host_plan, 0, num_hosts * sizeof(struct mbd_host *));
    memset(diag, 0, sizeof(*diag));

    // the job asked for specific machines
    if (job->res.machines.nentries > 0) {
        return build_host_plan_machines(job, diag);
    }

    int n = 0;
    struct ll_list_entry *e;
    for (e = host_list.head; e; e = e->next) {
        struct mbd_host *h = (struct mbd_host *) e;

        if (!host_in_queue_group(h, job)) {
            diag->not_in_queue++;
            continue;
        }

        if (!host_meets_requirements(h, job, diag))
            continue;

        host_plan[n] = h;
        ++n;
    }

    if (n < job->res.num_hosts) {
        LL_DEBUG("job=%ld need=%d found=%d hosts", job->job_id,
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
    log_run_hosts(job);

    return 1;
}

static void host_update_resources(const struct job_data *job)
{
    int i;

    for (i = 0; i < job->run_nhosts; i++) {
        struct mbd_host *h = job->run_hosts[i];

        h->res.free_cpu -= job->res.num_cpus;
        h->res.free_mem_mb -= job->res.mem_mb;
        h->res.free_storage_mb -= job->res.storage_mb;
        h->num_jobs++;
        h->num_run++;
        h->num_cpus_used += job->res.num_cpus;

        // host has reached MXJ capacity
        if (h->num_jobs >= h->res.max_jobs)
            h->candidate = 0;

        if (job->flags & JOB_FLAG_EXCLUSIVE)
            h->exclusive = 1;

        // gpu ids were already updated during dispatch to sbd
        int n = gpu_ids_count_free(&h->res.gpu);
        LL_DEBUG("host=%s num_jobs=%d free_cpu=%d free_mem_mb=%lu "
                 "free_storage_mb=%lu free_gpu=%d maxjobsleft=%d",
                 h->net.name, h->num_jobs, h->res.free_cpu, h->res.free_mem_mb,
                 h->res.free_storage_mb, n,
                 h->res.max_jobs - h->num_jobs);
    }
}

static int tokens_available(const struct job_data *job)
{
    struct ll_list_entry *e;

    for (e = job->res.tokens.head; e != NULL; e = e->next) {
        struct job_token *t = (struct job_token *) e;
        struct mbd_token_pool *p;

        p = ll_hash_search(&token_pool_name_hash, t->name);
        if (p == NULL) {
            LL_ERRX("job=%ld token pool=%s not found", job->job_id, t->name);
            return 0;
        }
        if (p->free < t->count) {
            LL_DEBUG("no tokens for job=%ld pool=%s need=%d free=%d",
                     job->job_id, t->name, t->count, p->free);
            return 0;
        }
    }
    return 1;
}

void schedule(void)
{
    LL_DEBUG("num_pend_jobs=%d", ll_list_count(&pend_jobs_list));
    if (ll_list_is_empty(&pend_jobs_list))
        return;

    int cc = mark_candidates();
    if (cc == 0) {
        LL_DEBUG("no scheduling attempt possible %d candidate hosts", cc);
        return;
    }
    LL_DEBUG("scheduling with %d candidate hosts", cc);

    ll_list_sort_buf(&pend_jobs_list, sort_buf, pend_job_cmp);

    struct ll_list_entry *e = pend_jobs_list.head;
    while (e) {
        struct job_data *job = (struct job_data *) e;
        e = e->next;

        LL_DEBUG("is job=%ld ready for scheduling", job->job_id);
        if (!job_is_ready(job)) {
            job->pend_reason = PEND_JOB_NOT_READY;
            continue;
        }

        if (job->queue->state == QUEUE_CLOSED) {
            job->pend_reason = PEND_QUEUE_CLOSED;
            continue;
        }

        if (!tokens_available(job)) {
            job->pend_reason = PEND_TOKENS;
            continue;
        }

        LL_DEBUG("job=%ld is ready for scheduling", job->job_id);
        struct pend_diag diag;
        cc = build_host_plan(job, &diag);
        if (cc < 0) {
            LL_ERRX("job=%ld failed to build host plan, trying next cycle",
                    job->job_id);
            return;
        }
        if (cc == 0) {
            job->pend_reason = diag_reason(&diag);
            LL_INFO("job=%ld not enough hosts found to build a plan",
                    job->job_id);
            continue;
        }

        job->pend_reason = PEND_NONE;
        if (mbd_dispatch_job(job) < 0) {
            LL_ERRX("job=%ld dispatch failed", job->job_id);
            continue;
        }

        // udpate host and queue counters and resources
        host_update_resources(job);
        token_alloc(job);

        job->queue->num_run++;
        job->queue->num_pend--;
        job->queue->num_cpus_used += job->res.num_cpus * job->run_nhosts;
        job->queue->num_hosts_used += job->run_nhosts;

        LL_DEBUG("queue=%s num_pend=%d num_run=%d num_susp=%d",
                 job->queue->name, job->queue->num_pend, job->queue->num_run,
                 job->queue->num_susp);
    }
    mbd_assert_counters();
}

void token_alloc(const struct job_data *job)
{
    struct ll_list_entry *e;

    for (e = job->res.tokens.head; e != NULL; e = e->next) {
        struct job_token *t = (struct job_token *) e;
        struct mbd_token_pool *p;

        p = ll_hash_search(&token_pool_name_hash, t->name);
        if (p == NULL) {
            LL_ERRX("job=%ld pool=%s not found", job->job_id, t->name);
            continue;
        }
        p->free -= t->count;
    }
}

void token_free(const struct job_data *job)
{
    struct ll_list_entry *e;

    for (e = job->res.tokens.head; e != NULL; e = e->next) {
        struct job_token *t = (struct job_token *) e;
        struct mbd_token_pool *p;

        p = ll_hash_search(&token_pool_name_hash, t->name);
        if (p == NULL) {
            LL_ERRX("job=%ld pool=%s not found", job->job_id, t->name);
            continue;
        }
        p->free += t->count;
    }
}
