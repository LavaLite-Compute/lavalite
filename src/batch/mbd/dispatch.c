/* Copyright (C) LavaLite Contributors
 * GPL v2
 */

#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <errno.h>
#include <assert.h>

#include "batch/lib/wire.h"
#include "batch/mbd/mbd.h"

/* -----------------------------------------------------------
 * job signal
 * -----------------------------------------------------------
 */
static int finish_pending_job(struct job_data *job, const struct wire_job_sig *ws)
{
    LS_INFO("finish_pending_job: job_id=%ld sig=%d -> EXIT",
            (long)job->job_id, ws->sig);
    job->signal_time = job->end_time = time(NULL);
    job->status = JOB_STAT_EXIT;
    event_job_signal(job, ws);
    event_job_finish(job);
    job_move_list(job, &pend_jobs_list, &finish_jobs_list, JOB_LIST_FINISH);

    return MBD_OK;
}

static int stop_pending_job(struct job_data *job, const struct wire_job_sig *ws)
{
    if (job->status & JOB_STAT_PSUSP)
        return MBD_OK;
    job->status = JOB_STAT_PSUSP;
    job->signal_time = time(NULL);
    LS_INFO("stop_pending_job: job_id=%ld sig=%d -> PSUSP",
            (long)job->job_id, ws->sig);
    event_job_signal(job, ws);
    event_job_pend_susp(job);
    return MBD_OK;
}

static int resume_pending_job(struct job_data *job, const struct wire_job_sig *ws)
{
    if (!(job->status & JOB_STAT_PSUSP))
        return MBD_OK;
    job->status = JOB_STAT_PEND;
    job->signal_time = time(NULL);
    LS_INFO("resume_pending_job: job_id=%ld sig=%d -> PEND",
            (long)job->job_id, ws->sig);
    event_job_signal(job, ws);
    event_job_pend_resume(job);
    return MBD_OK;
}

static int signal_pending_job(struct job_data *job, const struct wire_job_sig *ws)
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
        LS_DEBUG("signal_pending_job: job_id=%ld sig=%d unsupported",
                 (long)job->job_id, ws->sig);
        return EINVAL;
    }
}

static int signal_running_job(struct job_data *job, const struct wire_job_sig *ws)
{
    /* TODO: send signal via cgroup/pid, log event */
    (void)job;
    (void)ws;
    return MBD_OK;
}

static void signal_all_jobs(uint32_t uid, struct wire_job_sig *req)
{
    struct ll_list_entry *e;
    struct ll_list_entry *next;
    struct job_data *job;

    for (e = pend_jobs_list.head; e != NULL; e = next) {
        next = e->next;
        job = (struct job_data *)e;
        assert(job->status & (JOB_STAT_PEND | JOB_STAT_PSUSP));
        if (job->uid != uid)
            continue;
        signal_pending_job(job, req);
    }
    for (e = run_jobs_list.head; e != NULL; e = next) {
        next = e->next;
        job = (struct job_data *)e;
        assert(job->status & (JOB_STAT_RUN | JOB_STAT_SUSP));
        if (job->uid != uid)
            continue;
        signal_running_job(job, req);
    }
}

int job_signal(XDR *xdrs, int chan_id)
{
    struct wire_job_sig req;
    memset(&req, 0, sizeof(req));
    if (!xdr_wire_job_sig(xdrs, &req)) {
        LS_ERR("job_signal: xdr decode failed chan_id=%d", chan_id);
        return enqueue_header(chan_id, BATCH_JOB_SIGNAL_ACK, EPROTO);
    }

    LS_DEBUG("job_id=%ld by uid=%u sig=%d chan_id=%d",
             (long)req.job_id, req.uid, req.sig, chan_id);

    if (req.job_id == 0) {
        signal_all_jobs(req.uid, &req);
        return enqueue_header(chan_id, BATCH_JOB_SIGNAL_ACK, MBD_OK);
    }

    struct job_data *job = job_find(req.job_id);
    if (job == NULL) {
        LS_INFO("job_signal: job_id=%ld not found", (long)req.job_id);
        return enqueue_header(chan_id, BATCH_JOB_SIGNAL_ACK, ESRCH);
    }

    if (job->status & (JOB_STAT_DONE | JOB_STAT_EXIT)) {
        LS_DEBUG("job_signal: job_id=%ld already finished", (long)req.job_id);
        return enqueue_header(chan_id, BATCH_JOB_SIGNAL_ACK, EINVAL);
    }

    if ((req.sig == SIGSTOP || req.sig == SIGTSTP)
        && (job->status & (JOB_STAT_SUSP | JOB_STAT_PSUSP))) {
        LS_DEBUG("job_signal: job_id=%ld already suspended", (long)req.job_id);
        return enqueue_header(chan_id, BATCH_JOB_SIGNAL_ACK, EINVAL);
    }

    if (req.sig == SIGCONT
        && (job->status & (JOB_STAT_PEND | JOB_STAT_RUN))) {
        LS_DEBUG("job_signal: job_id=%ld SIGCONT no-op", (long)req.job_id);
        return enqueue_header(chan_id, BATCH_JOB_SIGNAL_ACK, MBD_OK);
    }

    int cc;
    if (job->status & (JOB_STAT_PEND | JOB_STAT_PSUSP))
        cc = signal_pending_job(job, &req);
    else
        cc = signal_running_job(job, &req);

    return enqueue_header(chan_id, BATCH_JOB_SIGNAL_ACK, cc);
}


/* -----------------------------------------------------------
 * job info
 * ----------------------------------------------------------- */
static void job_data_to_wire(const struct job_data *job, struct wire_job_info *w)
{
    memset(w, 0, sizeof(*w));
    w->job_id      = job->job_id;
    w->uid         = (uint32_t)job->uid;
    w->status      = job->status;
    w->exit_status = job->exit_status;
    w->priority    = job->priority;
    w->submit_time = (int64_t)job->submit_time;
    w->dispatch_time  = (int64_t)job->dispatch_time;
    w->end_time    = (int64_t)job->end_time;
    w->susp_time   = (int64_t)job->susp_time;
    w->res.pid      = job->usage.pid;
    w->res.mem_mb   = job->usage.mem_mb;
    w->res.cpu_time = job->usage.cpu_time;
    ll_strlcpy(w->name, job->name,  sizeof(w->name));
    ll_strlcpy(w->queue, job->queue->name, sizeof(w->queue));
    ll_strlcpy(w->exec_host, job->exec_host, sizeof(w->exec_host));
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
        struct job_data *job = (struct job_data *)e;

        if (uid != 0 && job->uid != uid && !is_manager(uid))
            continue;
        job_data_to_wire(job, &dst[count]);
        count++;
    }
    return count;
}

int job_info(XDR *xdrs, int chan_id)
{
    struct wire_job_info_req req;
    memset(&req, 0, sizeof(req));
    if (!xdr_wire_job_info_req(xdrs, &req)) {
        LS_ERRX("xdr_wire_job_info_req failed chan_id=%d", chan_id);
        return -1;
    }

    int ntotal = ll_list_count(&pend_jobs_list)
               + ll_list_count(&run_jobs_list)
               + ll_list_count(&finish_jobs_list);

    struct wire_job_info *jobs = calloc(ntotal ? ntotal : 1,
                                        sizeof(struct wire_job_info));
    if (jobs == NULL) {
        LS_ERR("calloc failed");
        return -1;
    }

    int n = 0;

    if (req.job_id != -1) {
        struct job_data *job = job_find(req.job_id);
        if (job != NULL) {
            job_data_to_wire(job, &jobs[0]);
            n = 1;
        }
        goto send;
    }

    uid_t uid = (uid_t)req.uid;

    if (req.flags == 0) {
        n = collect_list(&pend_jobs_list, jobs, n, uid);
        n = collect_list(&run_jobs_list,  jobs, n, uid);
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
    reply.jobs  = jobs;

    size_t siz = sizeof(struct wire_job_info) * ntotal
               + sizeof(struct wire_job_info_array)
               + PACKET_HEADER_SIZE
               + LL_BUFSIZ_64;

    struct protocol_header hdr;
    init_protocol_header(&hdr);
    hdr.operation = BATCH_JOB_INFO_ACK;
    hdr.status = MBD_OK;

    if (enqueue_payload(chan_id, &hdr, &reply, siz,
                        xdr_wire_job_info_array) < 0) {
        LS_ERR("enqueue_payload failed");
        free(jobs);
        return -1;
    }

    free(jobs);
    return 0;
}

/* -----------------------------------------------------------
 * queue info
 * ----------------------------------------------------------- */
int queue_info(XDR *xdrs, int chan_id)
{
    (void)xdrs;

    int nqueues = ll_list_count(&queue_list);
    struct wire_queue_info *queues = calloc(nqueues,
                                            sizeof(struct wire_queue_info));
    if (queues == NULL) {
        LS_ERR("queue_info: calloc failed");
        return -1;
    }

    int i = 0;
    for (struct ll_list_entry *e = queue_list.head; e != NULL; e = e->next) {
        struct mbd_queue *q = (struct mbd_queue *)e;

        ll_strlcpy(queues[i].name, q->name, sizeof(queues[i].name));
        ll_strlcpy(queues[i].description, q->description,
                   sizeof(queues[i].description));
        ll_strlcpy(queues[i].hosts, q->hosts_spec, sizeof(queues[i].hosts));
        queues[i].priority = q->priority;
        queues[i].max_jobs = q->max_jobs;
        queues[i].num_pend = q->num_pend;
        queues[i].num_run  = q->num_run;
        queues[i].num_susp = q->num_susp;
        i++;
    }

    struct wire_queue_info_array reply;
    reply.nqueues = nqueues;
    reply.queues  = queues;

    size_t siz = sizeof(struct wire_queue_info) * nqueues
               + sizeof(struct wire_queue_info_array)
               + PACKET_HEADER_SIZE
               + LL_BUFSIZ_64;

    struct protocol_header hdr;
    init_protocol_header(&hdr);
    hdr.operation = BATCH_QUEUE_INFO_ACK;
    hdr.status    = MBD_OK;

    if (enqueue_payload(chan_id, &hdr, &reply, siz,
                        xdr_wire_queue_info_array) < 0) {
        LS_ERR("queue_info: enqueue_payload failed");
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
    (void)xdrs;

    int ngroups = ll_list_count(&group_list);
    struct wire_group_info *groups = calloc(ngroups,
                                            sizeof(struct wire_group_info));
    if (groups == NULL) {
        LS_ERR("host_group_info: calloc failed");
        return -1;
    }

    int i = 0;
    for (struct ll_list_entry *e = group_list.head; e != NULL; e = e->next) {
        struct mbd_group *g = (struct mbd_group *)e;

        ll_strlcpy(groups[i].name,    g->name,    sizeof(groups[i].name));
        ll_strlcpy(groups[i].members, g->members, sizeof(groups[i].members));
        groups[i].num_members = g->num_members;
        i++;
    }

    struct wire_group_info_array reply;
    reply.ngroups = ngroups;
    reply.groups  = groups;

    size_t siz = sizeof(struct wire_group_info) * ngroups
               + sizeof(struct wire_group_info_array)
               + PACKET_HEADER_SIZE
               + LL_BUFSIZ_64;

    struct protocol_header hdr;
    init_protocol_header(&hdr);
    hdr.operation = BATCH_GROUP_INFO_ACK;
    hdr.status    = MBD_OK;

    if (enqueue_payload(chan_id, &hdr, &reply, siz,
                        xdr_wire_group_info_array) < 0) {
        LS_ERR("host_group_info: enqueue_payload failed");
        free(groups);
        return -1;
    }

    free(groups);
    return 0;
}

/* -----------------------------------------------------------
 * host info
 * ----------------------------------------------------------- */
int host_info(XDR *xdrs, int chan_id)
{
    (void)xdrs;

    int nhosts = ll_list_count(&host_list);
    struct wire_host_info *hosts = calloc(nhosts ? nhosts : 1,
                                          sizeof(struct wire_host_info));
    if (hosts == NULL) {
        LS_ERR("host_info: calloc failed");
        return -1;
    }

    int i = 0;
    for (struct ll_list_entry *e = host_list.head; e != NULL; e = e->next) {
        struct mbd_host *h = (struct mbd_host *)e;

        ll_strlcpy(hosts[i].name, h->net.name, sizeof(hosts[i].name));
        hosts[i].status          = h->status;
        hosts[i].max_jobs        = h->res.max_jobs;
        hosts[i].total_cpu       = h->res.total_cpu;
        hosts[i].total_gpu       = h->res.total_gpu;
        hosts[i].total_mem_mb    = h->res.total_mem_mb;
        hosts[i].total_storage_mb = h->res.total_storage_mb;
        hosts[i].num_jobs        = h->num_jobs;
        hosts[i].num_run         = h->num_run;
        hosts[i].num_susp        = h->num_susp;
        i++;
    }

    struct wire_host_info_array reply;
    reply.nhosts = nhosts;
    reply.hosts  = hosts;

    size_t siz = sizeof(struct wire_host_info) * nhosts
               + sizeof(struct wire_host_info_array)
               + PACKET_HEADER_SIZE
               + LL_BUFSIZ_64;

    struct protocol_header hdr;
    init_protocol_header(&hdr);
    hdr.operation = BATCH_HOST_INFO_ACK;
    hdr.status    = MBD_OK;

    if (enqueue_payload(chan_id, &hdr, &reply, siz,
                        xdr_wire_host_info_array) < 0) {
        LS_ERR("host_info: enqueue_payload failed");
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
    (void)xdrs;

    LS_DEBUG("compact_done chan_id=%d", chan_id);
    return 0;
}
