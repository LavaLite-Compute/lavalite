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

/* -----------------------------------------------------------
 * job signal
 * -----------------------------------------------------------
 */
static int finish_pending_job(struct job_data *job,
                              const struct wire_job_sig *ws)
{
    LL_INFO("finish_pending_job: job_id=%ld sig=%d -> EXIT", (long) job->job_id,
            ws->sig);
    job->signal_time = job->end_time = time(NULL);

    if (job->state == JOB_PENDING)
        job->queue->num_pend--;

    if (job->state == JOB_HELD)
        job->queue->num_held--;

    job->queue->num_jobs--;

    job->state = JOB_EXITED;
    event_job_signal(job, ws);
    event_job_finish(job);
    job_move_list(job, &pend_jobs_list, &finish_jobs_list, JOB_LIST_FINISH);

    return MBD_OK;
}

static int stop_pending_job(struct job_data *job, const struct wire_job_sig *ws)
{
    if (job->state == JOB_HELD)
        return MBD_OK;

    job->state = JOB_HELD;
    job->signal_time = time(NULL);
    LL_INFO("stop_pending_job: job_id=%ld sig=%d -> PSUSP", (long) job->job_id,
            ws->sig);
    event_job_signal(job, ws);
    event_job_pend_susp(job);

    job->queue->num_pend--;
    job->queue->num_held++;
    LL_DEBUG("queue=%s num_pend=%d num_run=%d num_susp=%d num_held=%d",
             job->queue->name, job->queue->num_pend, job->queue->num_run,
             job->queue->num_susp, job->queue->num_held);

    return MBD_OK;
}

static int resume_pending_job(struct job_data *job,
                              const struct wire_job_sig *ws)
{
    if (!(job->state == JOB_HELD))
        return MBD_OK;

    job->state = JOB_PENDING;
    job->signal_time = time(NULL);
    LL_INFO("resume_pending_job: job_id=%ld sig=%d -> PEND", (long) job->job_id,
            ws->sig);
    event_job_signal(job, ws);
    event_job_pend_resume(job);

    job->queue->num_held--;
    job->queue->num_pend++;
    LL_DEBUG("queue=%s num_pend=%d num_run=%d num_susp=%d num_held=%d",
             job->queue->name, job->queue->num_pend, job->queue->num_run,
             job->queue->num_susp, job->queue->num_held);

    return MBD_OK;
}

static int signal_pending_job(struct job_data *job,
                              const struct wire_job_sig *ws)
{
    switch (ws->sig) {
    case SIGTERM:
    case SIGINT:
    case SIGKILL:
        return finish_pending_job(job, ws);
    case SIGSTOP:
    case SIGTSTP:
        return stop_pending_job(job, ws);
    case SIGCONT:
        return resume_pending_job(job, ws);
    default:
        LL_DEBUG("signal_pending_job: job_id=%ld sig=%d unsupported",
                 (long) job->job_id, ws->sig);
        return EINVAL;
    }
}

static int signal_running_job(struct job_data *job,
                              const struct wire_job_sig *ws)
{
    struct protocol_header hdr;
    init_protocol_header(&hdr);
    hdr.operation = BATCH_SBD_JOB_SIGNAL;
    hdr.status = MBD_OK;

    if (auth_sign_header(&hdr) < 0) {
        LL_ERR("job=%ld failed to sign header for host=%s", job->job_id,
               job->run_hosts[0]->net.name);
        return EAGAIN;
    }

    if (enqueue_payload(job->run_hosts[0]->sbd_chan, &hdr, (void *) ws,
                        LL_BUFSIZ_1K, xdr_wire_job_sig) < 0) {
        LL_ERR("job=%ld enqueue_payload failed", job->job_id);
        return EAGAIN;
    }

    job->signal_time = time(NULL);
    event_job_signal(job, ws);

    LL_INFO("job=%ld sig=%d sent to sbd=%s", job->job_id, ws->sig,
            job->run_hosts[0]->net.name);

    return MBD_OK;
}

static int signal_all_jobs(uint32_t uid, struct wire_job_sig *req)
{
    struct ll_list_entry *e;
    struct ll_list_entry *next;
    struct job_data *job;

    for (e = pend_jobs_list.head; e != NULL; e = next) {
        next = e->next;
        job = (struct job_data *) e;
        assert(job->state == JOB_PENDING || job->state == JOB_HELD);
        if (job->uid != uid)
            continue;
        req->job_id = job->job_id;
        // Best effort, even if one failed keep going
        signal_pending_job(job, req);
    }
    for (e = run_jobs_list.head; e != NULL; e = e->next) {
        job = (struct job_data *) e;

        assert(job->run_hosts[0]);
        if (job->run_hosts[0]->sbd_chan < 0) {
            LL_DEBUG("sbd=%s is disconnected", job->run_hosts[0]->net.name);
            assert(job->state == JOB_UNKNOWN);
            continue;
        }

        assert(job->state == JOB_RUNNING || job->state == JOB_SUSPENDED);
        if (job->uid != uid)
            continue;
        req->job_id = job->job_id;
        // Best effort, even if one fails keep going
        signal_running_job(job, req);
    }

    return MBD_OK;
}

int jobs_signal(XDR *xdrs, int chan_id)
{
    struct wire_job_sig req;
    memset(&req, 0, sizeof(req));
    if (!xdr_wire_job_sig(xdrs, &req)) {
        LL_ERR("job_signal: xdr decode failed chan_id=%d", chan_id);
        return enqueue_header(chan_id, BATCH_JOB_SIGNAL_ACK, EPROTO);
    }

    LL_DEBUG("job_id=%ld by uid=%u sig=%d chan_id=%d", (long) req.job_id,
             req.uid, req.sig, chan_id);

    if (req.job_id == 0) {
        int cc = signal_all_jobs(req.uid, &req);
        return enqueue_header(chan_id, BATCH_JOB_SIGNAL_ACK, cc);
    }

    struct job_data *job = job_find(req.job_id);
    if (job == NULL) {
        LL_INFO("job_signal: job_id=%ld not found", (long) req.job_id);
        return enqueue_header(chan_id, BATCH_JOB_SIGNAL_ACK, ESRCH);
    }

    if (job->state == JOB_DONE || job->state == JOB_EXITED) {
        LL_DEBUG("job_signal: job_id=%ld already finished", (long) req.job_id);
        return enqueue_header(chan_id, BATCH_JOB_SIGNAL_ACK, EINVAL);
    }

    if ((req.sig == SIGSTOP || req.sig == SIGTSTP) &&
        (job->state == JOB_SUSPENDED || job->state == JOB_HELD)) {
        LL_DEBUG("job_signal: job_id=%ld already suspended", (long) req.job_id);
        return enqueue_header(chan_id, BATCH_JOB_SIGNAL_ACK, EINVAL);
    }

    if (req.sig == SIGCONT &&
        (job->state == JOB_PENDING || job->state == JOB_RUNNING)) {
        LL_DEBUG("job_signal: job_id=%ld SIGCONT no-op", (long) req.job_id);
        return enqueue_header(chan_id, BATCH_JOB_SIGNAL_ACK, MBD_OK);
    }

    int cc;
    if (job->state == JOB_PENDING || job->state == JOB_HELD)
        cc = signal_pending_job(job, &req);
    else
        cc = signal_running_job(job, &req);

    return enqueue_header(chan_id, BATCH_JOB_SIGNAL_ACK, cc);
}

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
