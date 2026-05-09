/*
 * Copyright (C) LavaLite Contributors
 * GPL v2
 */

#include <stdlib.h>
#include <time.h>
#include <string.h>

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

        if (h->status != HOST_OK)
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
    if (!(job->status & JOB_STAT_PEND))
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

#define SCHED_PLAN_MAX 1024
static struct mbd_host *host_plan[SCHED_PLAN_MAX];
static struct sched_plan plan;

static int build_host_plan(struct job_data *job)
{
    struct ll_list_entry *e;
    int n = 0;

    memset(host_plan, 0, sizeof(host_plan));
    for (e = host_list.head; e; e = e->next) {
        struct mbd_host *h = (struct mbd_host *)e;

        LS_DEBUG("host=%s total_mem_mb=%lu free_mem_mb=%lu "
                 "total_storage_mb=%lu free_storage_mb=%lu "
                 " total_cpu=%d free_cpu=%d candidate=%d fits job=%ld",
                 h->net.name,
                 h->res.total_mem_mb, h->res.free_mem_mb,
                 h->res.total_storage_mb, h->res.free_storage_mb,
                 h->res.total_cpu, h->res.free_cpu,
                 h->candidate, job->job_id);

        // not candidate
        if (!h->candidate)
            continue;
        // already exclusively allocated
        if (h->exclusive)
            continue;
        // job wants exclusive but job alreayd has runnign jobs
        if ((job->flags & JOB_FLAG_EXCLUSIVE) && h->num_jobs > 0)
            continue;
        // host is memeber of the queue
        if (!host_in_queue_group(h, job))
            continue;
        if (h->res.free_cpu < job->res.num_cpus)
            continue;
        if (h->res.free_mem_mb < job->res.mem_mb)
            continue;
        if (h->res.free_storage_mb < job->res.storage_mb)
            continue;
        if (job->res.num_gpus > 0) {
            if (!host_has_gpu(h, job))
                continue;
        }
        if (job->res.machines.nentries > 0 &&
            !ll_hash_contains(&job->res.machines, h->net.name))
            continue;

        LS_DEBUG("host=%s is candidate for job=%ld", h->net.name, job->job_id);
        host_plan[n] = h;
        ++n;
    }

    if (n < job->res.num_hosts) {
        LS_DEBUG("job=%ld need=%d found=%d hosts", job->job_id,
                 job->res.num_hosts, n);
        return 0;
    }

    qsort(host_plan, n, sizeof(struct mbd_host *), host_plan_cmp);

    memset(&plan, 0, sizeof(plan));
    int i;
    for (i = 0; i < job->res.num_hosts; i++)
        job->run_hosts[i] = host_plan[i];

    LS_INFO("job=%ld exec_host=%s nhosts=%d cpus_per_host=%d gpus_per_host=%d",
            job->job_id, job->run_hosts[0]->net.name, job->res.num_hosts,
            job->res.num_cpus, job->res.num_gpus);

    return 1;
}
static void host_update_resources(const struct job_data *job)
{
    int i;

    for (i = 0; i < plan.nhosts; i++) {
        struct mbd_host *h = plan.hosts[i];

        h->res.free_cpu        -= job->res.num_cpus;
        h->res.free_mem_mb     -= job->res.mem_mb;
        h->res.free_storage_mb -= job->res.storage_mb;
        h->num_jobs++;

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

        LS_DEBUG("host=%s free_cpu=%d free_mem_mb=%lu free_storage_mb=%lu "
                 "free_gpu=%d num_jobs=%d",
                 h->net.name, h->res.free_cpu, h->res.free_mem_mb,
                 h->res.free_storage_mb, h->res.free_gpu, h->num_jobs);
    }
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
        if (!job_is_ready(job))
            continue;

        LS_DEBUG("job=%ld is ready for scheduling", job->job_id);
        if (build_host_plan(job) == 0) {
            LS_INFO("job=%ld not enough hosts found", job->job_id);
            continue;
        }

        if (mbd_dispatch_job(job) < 0) {
            LS_ERRX("job=%ld dispatch failed", job->job_id);
            continue;
        }

        // udpate host and queue counters and resources
        host_update_resources(job);

        job->queue->num_run++;
        job->queue->num_pend--;
        LS_DEBUG("queue=%s num_pend=%d num_run=%d num_susp=%d",
                 job->queue->name, job->queue->num_pend,
                 job->queue->num_run, job->queue->num_susp);
    }
}
