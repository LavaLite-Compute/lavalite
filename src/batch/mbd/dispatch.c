/* Copyright (C) LavaLite Contributors
 * GPL v2
 */

#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <errno.h>
#include <assert.h>

#include "base/lib/auth.h"
#include "batch/lib/wire.h"
#include "batch/mbd/mbd.h"

static void job_data_to_wire(const struct job_data *job,
                             struct wire_job_info *w)
{
    memset(w, 0, sizeof(*w));
    w->job_id = job->job_id;
    w->uid = (uint32_t) job->uid;
    w->pid = job->pid;
    w->state = job->state;
    /*
     * UNKNOWN is a public/reporting state. Internally the job keeps its last
     * lifecycle state. If the execution host is disconnected, clients cannot
     * know whether the process is still running, suspended, or gone.
     */
    if ((w->state == JOB_RUNNING || w->state == JOB_SUSPENDED) &&
        job->run_hosts[0]->sbd_chan < 0)
        w->state = JOB_UNKNOWN;
    w->exit_status = job->exit_status;
    w->priority = job->priority;
    w->pend_reason = job->pend_reason;
    w->submit_time = (int64_t) job->submit_time;
    w->dispatch_time = (int64_t) job->dispatch_time;
    w->end_time = (int64_t) job->end_time;
    w->susp_time = (int64_t) job->susp_time;

    ll_strlcpy(w->name, job->name, sizeof(w->name));
    ll_strlcpy(w->queue, job->queue->name, sizeof(w->queue));

    for (int i = 0; i < job->run_nhosts; i++) {
        char entry[MAXHOSTNAMELEN + LL_BUFSIZ_64];

        if (i > 0)
            ll_strlcat(w->exec_hosts, " ", sizeof(w->exec_hosts));
        snprintf(entry, sizeof(entry), "%d@%s", job->res.num_cpus,
                 job->run_hosts[i]->net.name);
        ll_strlcat(w->exec_hosts, entry, sizeof(w->exec_hosts));
    }
}

/*
 * Append matching jobs from list into dst starting at dst[count].
 * uid == 0 means all users.
 * Returns updated count.
 */
static int collect_list(struct ll_list *list, struct wire_job_info *dst,
                        int count, uid_t uid)
{
    for (struct ll_list_entry *e = list->head; e != NULL; e = e->next) {
        struct job_data *job = (struct job_data *) e;

        if (uid != 0 && job->uid != uid && !is_manager(uid))
            continue;
        job_data_to_wire(job, &dst[count]);
        count++;
    }
    return count;
}

int jobs_info(XDR *xdrs, int chan_id)
{
    struct wire_job_info_req req;
    memset(&req, 0, sizeof(req));
    if (!xdr_wire_job_info_req(xdrs, &req)) {
        LL_ERRX("xdr_wire_job_info_req failed chan_id=%d", chan_id);
        return -1;
    }

    int ntotal = ll_list_count(&pend_jobs_list) +
                 ll_list_count(&run_jobs_list) +
                 ll_list_count(&finish_jobs_list);

    int n = 0;
    struct wire_job_info *jobs = NULL;
    if (ntotal == 0) {
        n = 0;
        jobs = NULL;
        goto send;
    }

    jobs = calloc(ntotal, sizeof(struct wire_job_info));
    if (jobs == NULL) {
        LL_ERR("calloc failed");
        return -1;
    }

    if (req.job_id != -1) {
        struct job_data *job = job_find(req.job_id);
        if (job != NULL) {
            job_data_to_wire(job, &jobs[0]);
            n = 1;
        }
        goto send;
    }

    uid_t uid = (uid_t) req.uid;

    if (req.flags == 0) {
        n = collect_list(&pend_jobs_list, jobs, n, uid);
        n = collect_list(&run_jobs_list, jobs, n, uid);
        goto send;
    }

    if (req.flags & LLB_JOB_PEND)
        n = collect_list(&pend_jobs_list, jobs, n, uid);
    if (req.flags & LLB_JOB_RUN)
        n = collect_list(&run_jobs_list, jobs, n, uid);
    if (req.flags & LLB_JOB_DONE)
        n = collect_list(&finish_jobs_list, jobs, n, uid);
send:
    /* enqueue_payload ... */

    struct wire_job_info_array reply;
    reply.njobs = n;
    reply.jobs = jobs;

    size_t siz = sizeof(struct wire_job_info) * n +
                 sizeof(struct wire_job_info_array) + PACKET_HEADER_SIZE +
                 LL_BUFSIZ_64;

    struct protocol_header hdr;
    init_protocol_header(&hdr);
    hdr.operation = BATCH_JOB_INFO_ACK;
    hdr.status = MBD_OK;

    if (enqueue_payload(chan_id, &hdr, &reply, siz, xdr_wire_job_info_array) <
        0) {
        LL_ERR("enqueue_payload failed");
        free(jobs);
        return -1;
    }

    free(jobs);
    return 0;
}

/* -----------------------------------------------------------
 * queue info
 * ----------------------------------------------------------- */
int queues_info(XDR *xdrs, int chan_id)
{
    (void) xdrs;

    int nqueues = ll_list_count(&queue_list);
    struct wire_queue_info *queues =
        calloc(nqueues, sizeof(struct wire_queue_info));
    if (queues == NULL) {
        LL_ERR("queue_info: calloc failed");
        return -1;
    }

    int i = 0;
    for (struct ll_list_entry *e = queue_list.head; e != NULL; e = e->next) {
        struct mbd_queue *q = (struct mbd_queue *) e;

        ll_strlcpy(queues[i].name, q->name, sizeof(queues[i].name));
        ll_strlcpy(queues[i].description, q->description,
                   sizeof(queues[i].description));
        ll_strlcpy(queues[i].hosts, q->hosts_spec, sizeof(queues[i].hosts));
        queues[i].status = q->state;
        queues[i].priority = q->priority;
        queues[i].max_jobs = q->max_jobs;
        queues[i].num_jobs = q->num_jobs;
        queues[i].num_pend = q->num_pend;
        queues[i].num_run = q->num_run;
        queues[i].num_susp = q->num_susp;
        queues[i].num_held = q->num_held;
        queues[i].num_cpus_used = q->num_cpus_used;
        queues[i].num_hosts_used = q->num_hosts_used;

        i++;
    }

    struct wire_queue_info_array reply;
    reply.nqueues = nqueues;
    reply.queues = queues;

    size_t siz = sizeof(struct wire_queue_info) * nqueues +
                 sizeof(struct wire_queue_info_array) + PACKET_HEADER_SIZE +
                 LL_BUFSIZ_64;

    struct protocol_header hdr;
    init_protocol_header(&hdr);
    hdr.operation = BATCH_QUEUE_INFO_ACK;
    hdr.status = MBD_OK;

    if (enqueue_payload(chan_id, &hdr, &reply, siz, xdr_wire_queue_info_array) <
        0) {
        LL_ERR("queue_info: enqueue_payload failed");
        free(queues);
        return -1;
    }

    free(queues);
    return 0;
}

/* -----------------------------------------------------------
 * host group info
 * ----------------------------------------------------------- */
int host_group_info(XDR *xdrs, int chan_id)
{
    (void) xdrs;

    int ngroups = ll_list_count(&group_list);
    struct wire_group_info *groups =
        calloc(ngroups, sizeof(struct wire_group_info));
    if (groups == NULL) {
        LL_ERR("host_group_info: calloc failed");
        return -1;
    }

    int i = 0;
    for (struct ll_list_entry *e = group_list.head; e != NULL; e = e->next) {
        struct mbd_group *g = (struct mbd_group *) e;

        ll_strlcpy(groups[i].name, g->name, sizeof(groups[i].name));
        ll_strlcpy(groups[i].members, g->members, sizeof(groups[i].members));
        groups[i].num_members = g->num_members;
        i++;
    }

    struct wire_group_info_array reply;
    reply.ngroups = ngroups;
    reply.groups = groups;

    size_t siz = sizeof(struct wire_group_info) * ngroups +
                 sizeof(struct wire_group_info_array) + PACKET_HEADER_SIZE +
                 LL_BUFSIZ_64;

    struct protocol_header hdr;
    init_protocol_header(&hdr);
    hdr.operation = BATCH_GROUP_INFO_ACK;
    hdr.status = MBD_OK;

    if (enqueue_payload(chan_id, &hdr, &reply, siz, xdr_wire_group_info_array) <
        0) {
        LL_ERR("host_group_info: enqueue_payload failed");
        free(groups);
        return -1;
    }

    free(groups);
    return 0;
}

/* -----------------------------------------------------------
 * host info
 * ----------------------------------------------------------- */
int hosts_info(XDR *xdrs, int chan_id)
{
    (void) xdrs;

    int nhosts = ll_list_count(&host_list);
    struct wire_host_info *hosts =
        calloc(nhosts ? nhosts : 1, sizeof(struct wire_host_info));
    if (hosts == NULL) {
        LL_ERR("host_info: calloc failed");
        return -1;
    }

    int i = 0;
    for (struct ll_list_entry *e = host_list.head; e != NULL; e = e->next) {
        struct mbd_host *h = (struct mbd_host *) e;

        ll_strlcpy(hosts[i].name, h->net.name, sizeof(hosts[i].name));
        hosts[i].state = h->state;
        hosts[i].max_jobs = h->res.max_jobs;
        hosts[i].total_cpu = h->res.total_cpu;
        hosts[i].free_cpu = h->res.free_cpu;
        hosts[i].total_gpu = h->res.total_gpu;
        hosts[i].free_gpu = h->res.free_gpu;
        hosts[i].total_mem_mb = h->res.total_mem_mb;
        hosts[i].free_mem_mb = h->res.free_mem_mb;
        hosts[i].total_storage_mb = h->res.total_storage_mb;
        hosts[i].free_storage_mb = h->res.free_storage_mb;
        hosts[i].num_jobs = h->num_jobs;
        hosts[i].num_run = h->num_run;
        hosts[i].num_susp = h->num_susp;

        i++;
    }

    struct wire_host_info_array reply;
    reply.nhosts = nhosts;
    reply.hosts = hosts;

    size_t siz = sizeof(struct wire_host_info) * nhosts +
                 sizeof(struct wire_host_info_array) + PACKET_HEADER_SIZE +
                 LL_BUFSIZ_64;

    struct protocol_header hdr;
    init_protocol_header(&hdr);
    hdr.operation = BATCH_HOST_INFO_ACK;
    hdr.status = MBD_OK;

    if (enqueue_payload(chan_id, &hdr, &reply, siz, xdr_wire_host_info_array) <
        0) {
        LL_ERR("host_info: enqueue_payload failed");
        free(hosts);
        return -1;
    }

    free(hosts);
    return 0;
}

/* -----------------------------------------------------------
 * compact done / failed
 * ----------------------------------------------------------- */
int compact_done(XDR *xdrs, int chan_id)
{
    (void) xdrs;

    LL_DEBUG("compact_done chan_id=%d", chan_id);
    return 0;
}

int tokens_info(XDR *xdrs, int chan_id)
{
    (void) xdrs;

    struct ll_list_entry *e;
    int n = 0;

    for (e = token_pool_list.head; e != NULL; e = e->next)
        n++;

    struct wire_token_info_array w;
    w.ntokens = n;
    w.tokens = calloc(n, sizeof(struct wire_token_info));
    if (w.tokens == NULL) {
        LL_ERR("calloc failed n=%d", n);
        enqueue_header(chan_id, BATCH_TOKEN_INFO_ACK, errno);
        return -1;
    }

    int i = 0;
    for (e = token_pool_list.head; e != NULL; e = e->next) {
        struct mbd_token_pool *t = (struct mbd_token_pool *) e;

        ll_strlcpy(w.tokens[i].name, t->name, sizeof(w.tokens[i].name));
        w.tokens[i].total = t->total;
        w.tokens[i].free = t->free;
        i++;
    }

    struct protocol_header hdr;
    init_protocol_header(&hdr);
    hdr.operation = BATCH_TOKEN_INFO_ACK;
    hdr.status = MBD_OK;

    size_t bufsz =
        PACKET_HEADER_SIZE + n * sizeof(struct wire_token_info) + LL_BUFSIZ_256;

    enqueue_payload(chan_id, &hdr, &w, bufsz,
                    (bool_t(*)()) xdr_wire_token_info_array);

    free(w.tokens);
    return 0;
}
/* -----------------------------------------------------------
 * queue admin (open/close)
 * ----------------------------------------------------------- */
int queue_admin(XDR *xdrs, int chan_id)
{
    struct wire_queue_admin req;
    struct mbd_queue *q;

    memset(&req, 0, sizeof(req));
    if (!xdr_wire_queue_admin(xdrs, &req)) {
        LL_ERR("queue_admin: xdr decode failed chan_id=%d", chan_id);
        return enqueue_header(chan_id, BATCH_QUEUE_ADMIN_ACK, EPROTO);
    }

    if (!is_manager(req.uid)) {
        LL_ERR("queue_admin: uid=%u not a manager", req.uid);
        return enqueue_header(chan_id, BATCH_QUEUE_ADMIN_ACK, EPERM);
    }

    q = ll_hash_search(&queue_name_hash, req.name);
    if (q == NULL) {
        LL_ERR("queue_admin: queue=%s not found", req.name);
        return enqueue_header(chan_id, BATCH_QUEUE_ADMIN_ACK, ESRCH);
    }

    q->state = req.op;
    LL_INFO("queue=%s set to %s by uid=%u", q->name,
            req.op == QUEUE_CLOSED ? "closed" : "open", req.uid);

    queue_state_write(q);

    return enqueue_header(chan_id, BATCH_QUEUE_ADMIN_ACK, 0);
}

int host_admin(XDR *xdrs, int chan_id)
{
    struct wire_host_admin req;
    struct mbd_host *h;

    memset(&req, 0, sizeof(req));
    if (!xdr_wire_host_admin(xdrs, &req)) {
        LL_ERR("host_admin: xdr decode failed chan_id=%d", chan_id);
        return enqueue_header(chan_id, BATCH_HOST_ADMIN_ACK, EPROTO);
    }

    if (!is_manager(req.uid)) {
        LL_ERR("host_admin: uid=%u not a manager", req.uid);
        return enqueue_header(chan_id, BATCH_HOST_ADMIN_ACK, EPERM);
    }

    h = ll_hash_search(&host_name_hash, req.name);
    if (h == NULL) {
        LL_ERR("host_admin: host=%s not found", req.name);
        return enqueue_header(chan_id, BATCH_HOST_ADMIN_ACK, ESRCH);
    }

    if (req.op == HOST_CLOSED)
        h->state |= HOST_CLOSED;
    else
        h->state &= ~HOST_CLOSED;

    host_state_write(h);
    LL_INFO("host=%s set to %s by uid=%u", h->net.name,
            req.op == HOST_CLOSED ? "closed" : "open", req.uid);

    return enqueue_header(chan_id, BATCH_HOST_ADMIN_ACK, 0);
}
