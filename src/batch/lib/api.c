/* Copyright (C) LavaLite Contributors
 * GPL v2
 */

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/epoll.h>

#include "base/lib/ll.protocol.h"
#include "base/lib/ll.sys.h"
#include "batch/lib/proto.h"
#include "batch/lib/wire.h"

#include "llbatch.h"

__thread enum llb_error llberrno = LLBE_NONE;

struct queue_info *llb_queue_info(int32_t *nqueues)
{
    char buf[256];
    XDR xdrs;

    /* -------------------------
     * encode request
     * ------------------------- */
    xdrmem_create(&xdrs, buf, sizeof(buf), XDR_ENCODE);

    struct protocol_header hdr;
    init_protocol_header(&hdr);
    hdr.operation = BATCH_QUEUE_INFO;

    if (!xdr_pack_hdr(&xdrs, &hdr)) {
        xdr_destroy(&xdrs);
        return NULL;
    }

    size_t len = xdr_getpos(&xdrs);
    xdr_destroy(&xdrs);

    /* -------------------------
     * RPC
     * ------------------------- */
    void *rep = NULL;
    struct protocol_header rhdr;

    if (call_mbd(buf, len, &rep, &rhdr) < 0)
        return NULL;

    if (rhdr.status != MBD_OK) {
        llberrno = LLBE_QUEUE;
        return NULL;
    }

    /* -------------------------
     * decode reply
     * ------------------------- */
    xdrmem_create(&xdrs, rep, rhdr.length, XDR_DECODE);

    struct wire_queue_info_array w;
    memset(&w, 0, sizeof(w));

    if (!xdr_wire_queue_info_array(&xdrs, &w)) {
        xdr_destroy(&xdrs);
        free(rep);
        return NULL;
    }

    xdr_destroy(&xdrs);
    free(rep);

    /* -------------------------
     * convert wire → API
     * ------------------------- */
    struct queue_info *out;
    out = calloc(w.nqueues, sizeof(*out));
    if (!out) {
        xdr_free((xdrproc_t)xdr_wire_queue_info_array, (char *)&w);
        return NULL;
    }

    for (int i = 0; i < w.nqueues; i++) {
        struct wire_queue_info *src = &w.queues[i];
        struct queue_info *dst = &out[i];

        dst->name        = strdup(src->name);
        dst->description = strdup(src->description);
        dst->hosts       = strdup(src->hosts);

        dst->priority = src->priority;
        dst->max_jobs = src->max_jobs;

        dst->num_pend = src->num_pend;
        dst->num_run  = src->num_run;
        dst->num_susp = src->num_susp;
    }

    *nqueues = w.nqueues;

    xdr_free((xdrproc_t)xdr_wire_queue_info_array, (char *)&w);

    return out;
}

void llb_free_queue_info(struct queue_info *q, int n)
{
    for (int i = 0; i < n; i++) {
        free(q[i].name);
        free(q[i].description);
        free(q[i].hosts);
    }
    free(q);
}

struct host_group *llb_group_info(int32_t *ngroups)
{
    char buf[256];
    XDR xdrs;

    xdrmem_create(&xdrs, buf, sizeof(buf), XDR_ENCODE);

    struct protocol_header hdr;
    init_protocol_header(&hdr);
    hdr.operation = BATCH_GROUP_INFO;

    if (!xdr_pack_hdr(&xdrs, &hdr)) {
        xdr_destroy(&xdrs);
        return NULL;
    }

    size_t len = xdr_getpos(&xdrs);
    xdr_destroy(&xdrs);

    void *rep = NULL;
    struct protocol_header rhdr;

    if (call_mbd(buf, len, &rep, &rhdr) < 0)
        return NULL;

    if (rhdr.status != MBD_OK) {
        llberrno = LLBE_HOST_GROUP;
        return NULL;
    }

    xdrmem_create(&xdrs, rep, rhdr.length, XDR_DECODE);

    struct wire_group_info_array w;
    memset(&w, 0, sizeof(w));

    if (!xdr_wire_group_info_array(&xdrs, &w)) {
        xdr_destroy(&xdrs);
        free(rep);
        return NULL;
    }

    xdr_destroy(&xdrs);
    free(rep);

    if (w.ngroups == 0) {
        llberrno = LLBE_HOST_GROUP;
        return NULL;
    }

    struct host_group *out;
    out = calloc(w.ngroups, sizeof(*out));
    if (!out) {
        xdr_free((xdrproc_t)xdr_wire_group_info_array, (char *)&w);
        return NULL;
    }

    for (int i = 0; i < w.ngroups; i++) {
        out[i].name = strdup(w.groups[i].name);
        out[i].members = strdup(w.groups[i].members);
    }

    *ngroups = w.ngroups;

    xdr_free((xdrproc_t)xdr_wire_group_info_array, (char *)&w);

    return out;
}

void llb_free_group_info(struct host_group *g, int32_t n)
{
    for (int i = 0; i < n; i++) {
        free(g[i].name);
        free(g[i].members);
    }
    free(g);
}

int32_t llb_signal_job(int64_t jobid, int32_t sig)
{
    char buf[256];
    XDR xdrs;

    /* -------------------------
     * encode request
     * ------------------------- */
    xdrmem_create(&xdrs, buf, sizeof(buf), XDR_ENCODE);

    struct protocol_header hdr;
    init_protocol_header(&hdr);
    hdr.operation = BATCH_JOB_SIG;

    if (!xdr_pack_hdr(&xdrs, &hdr)) {
        xdr_destroy(&xdrs);
        return -1;
    }

    struct wire_job_sig req;
    req.job_id = jobid;
    req.sig   = sig;

    if (!xdr_wire_job_sig(&xdrs, &req)) {
        xdr_destroy(&xdrs);
        return -1;
    }

    size_t len = xdr_getpos(&xdrs);
    xdr_destroy(&xdrs);

    /* -------------------------
     * RPC
     * ------------------------- */
    void *rep = NULL;
    struct protocol_header rhdr;

    if (call_mbd(buf, len, &rep, &rhdr) < 0)
        return -1;

    if (rhdr.status != MBD_OK) {
        llberrno = LLBE_SIGNAL;
        return -1;
    }

    free(rep);

    return 0;
}

struct host_info *llb_host_info(int32_t *nhosts)
{
    char reqbuf[256];
    XDR xdrs;

    /* -------------------------
     * build request
     * ------------------------- */
    xdrmem_create(&xdrs, reqbuf, sizeof(reqbuf), XDR_ENCODE);

    struct protocol_header hdr;
    init_protocol_header(&hdr);
    hdr.operation = BATCH_HOST_INFO;

    if (!xdr_pack_hdr(&xdrs, &hdr)) {
        xdr_destroy(&xdrs);
        return NULL;
    }

    size_t req_len = xdr_getpos(&xdrs);
    xdr_destroy(&xdrs);

    /* -------------------------
     * RPC
     * ------------------------- */
    void *rep = NULL;
    struct protocol_header rhdr;

    if (call_mbd(reqbuf, req_len, &rep, &rhdr) < 0)
        return NULL;

    if (rhdr.status != MBD_OK) {
        llberrno = LLBE_HOST;
        return NULL;
    }

    /* -------------------------
     * decode wire
     * ------------------------- */
    xdrmem_create(&xdrs, rep, rhdr.length, XDR_DECODE);

    struct wire_host_info_array w;
    memset(&w, 0, sizeof(w));

    if (!xdr_wire_host_info_array(&xdrs, &w)) {
        xdr_destroy(&xdrs);
        free(rep);
        return NULL;
    }

    xdr_destroy(&xdrs);
    free(rep);

    /* -------------------------
     * convert wire → public API
     * ------------------------- */

    struct host_info *out;
    out = calloc(w.nhosts, sizeof(*out));
    if (!out) {
        xdr_free((xdrproc_t)xdr_wire_host_info_array, (char *)&w);
        return NULL;
    }

    for (int i = 0; i < w.nhosts; i++) {

        out[i].name = strdup(w.hosts[i].name);
        out[i].status   = w.hosts[i].status;
        out[i].max_jobs = w.hosts[i].max_jobs;
        out[i].num_jobs = w.hosts[i].num_jobs;
        out[i].num_run  = w.hosts[i].num_run;
        out[i].num_susp = w.hosts[i].num_susp;
    }

    *nhosts = w.nhosts;

    xdr_free((xdrproc_t)xdr_wire_host_info_array, (char *)&w);

    return out;
}

void llb_free_host_info(struct host_info *h, int32_t n)
{
    for (int i = 0; i < n; i++) {
        free(h[i].name);
    }
    free(h);
}

struct job_info *llb_job_info(int64_t jobid, int32_t *n, int32_t flags)
{
    char buf[512];
    XDR xdrs;
    /* -------------------------
     * encode request
     * ------------------------- */
    xdrmem_create(&xdrs, buf, sizeof(buf), XDR_ENCODE);
    struct protocol_header hdr;
    init_protocol_header(&hdr);
    hdr.operation = BATCH_JOB_INFO;
    if (!xdr_pack_hdr(&xdrs, &hdr)) {
        xdr_destroy(&xdrs);
        return NULL;
    }

    struct wire_job_info_req req;
    req.job_id = jobid;
    req.flags = flags;
    if (!xdr_wire_job_info_req(&xdrs, &req)) {
        xdr_destroy(&xdrs);
        return NULL;
    }
    size_t len = xdr_getpos(&xdrs);
    xdr_destroy(&xdrs);
    /* -------------------------
     * RPC
     * ------------------------- */
    void *rep = NULL;
    struct protocol_header rhdr;
    if (call_mbd(buf, len, &rep, &rhdr) < 0)
        return NULL;

    if (rhdr.status != MBD_OK) {
        llberrno = LLBE_NO_JOB;
        return NULL;
    }
    /* -------------------------
     * decode reply
     * ------------------------- */
    xdrmem_create(&xdrs, rep, rhdr.length, XDR_DECODE);
    struct wire_job_info_array w;
    memset(&w, 0, sizeof(w));
    if (!xdr_wire_job_info_array(&xdrs, &w)) {
        xdr_destroy(&xdrs);
        free(rep);
        return NULL;
    }
    xdr_destroy(&xdrs);
    free(rep);
    /* -------------------------
     * convert wire → API
     * ------------------------- */
    struct job_info *out;
    out = calloc(w.njobs, sizeof(*out));
    if (!out) {
        xdr_free((xdrproc_t)xdr_wire_job_info_array, (char *)&w);
        return NULL;
    }
    for (int i = 0; i < w.njobs; i++) {
        struct wire_job_info *src = &w.jobs[i];
        struct job_info      *dst = &out[i];
        dst->job_id      = src->job_id;
        dst->uid         = src->uid;
        dst->status      = src->status;
        dst->exit_status = src->exit_status;
        dst->priority    = src->priority;
        dst->submit_time = src->submit_time;
        dst->start_time  = src->start_time;
        dst->end_time    = src->end_time;
        dst->susp_time   = src->susp_time;
        dst->name      = strdup(src->name);
        dst->queue     = strdup(src->queue);
        dst->from_host = strdup(src->from_host);
        dst->exec_host = strdup(src->exec_host);
        dst->comment   = strdup(src->comment);
        dst->res.pid      = src->res.pid;
        dst->res.mem_mb   = src->res.mem_mb;
        dst->res.cpu_time = src->res.cpu_time;
    }
    *n = w.njobs;
    xdr_free((xdrproc_t)xdr_wire_job_info_array, (char *)&w);
    return out;
}

void llb_free_job_info(struct job_info *jobs, int32_t n)
{
    int i;

    if (!jobs)
        return;
    for (i = 0; i < n; i++) {
        free(jobs[i].name);
        free(jobs[i].queue);
        free(jobs[i].from_host);
        free(jobs[i].exec_host);
        free(jobs[i].comment);
    }
    free(jobs);
}

const char *llbe_str(enum llb_error e)
{
    static const char *msgs[] = {
        [LLBE_NONE]        = "no error",
        [LLBE_NO_JOB]      = "job not found",
        [LLBE_NOT_STARTED] = "job not yet started",
        [LLBE_JOB_STARTED] = "job already started",
        [LLBE_JOB_FINISH]  = "job already finished",
        [LLBE_HOST]       = "host info request failed",
        [LLBE_HOST_GROUP] = "host group request failed",
        [LLBE_QUEUE]      = "queue request failed",
        [LLBE_SIGNAL]      = "signal error",
        [LLBE_SYS_CALL]    = "system call failed",
        [LLBE_PROTOCOL]    = "protocol error",
    };

    if (e < 0 || e >= LLBE_NUM_ERR)
        return "unknown error";
    return msgs[e];
}
