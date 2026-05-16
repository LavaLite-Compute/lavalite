/* Copyright (C) LavaLite Contributors
 * GPL v2
 */

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/epoll.h>

#include "base/lib/ll.protocol.h"
#include "base/lib/ll.sys.h"
#include "base/lib/auth.h"
#include "batch/lib/rpc.h"
#include "batch/lib/wire.h"

#include "llbatch.h"

struct queue_info *llb_queue_info(int32_t *nqueues)
{
    char buf[LL_BUFSIZ_256];
    XDR xdrs;

    /* -------------------------
     * encode request
     * ------------------------- */
    xdrmem_create(&xdrs, buf, sizeof(buf), XDR_ENCODE);

    struct protocol_header hdr;
    init_protocol_header(&hdr);
    hdr.operation = BATCH_QUEUE_INFO;

    if (auth_sign_header(&hdr) < 0) {
        xdr_destroy(&xdrs);
        return NULL;
    }
    if (!ll_encode_msg(&xdrs, NULL, NULL, &hdr)) {
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
        dst->num_jobs = src->num_jobs;
        dst->num_pend = src->num_pend;
        dst->num_run  = src->num_run;
        dst->num_susp = src->num_susp;
        dst->num_held = src->num_held;
        dst->num_cpus_used  = src->num_cpus_used;
        dst->num_hosts_used = src->num_hosts_used;
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

    if (auth_sign_header(&hdr) < 0) {
        xdr_destroy(&xdrs);
        return NULL;
    }
    if (!ll_encode_msg(&xdrs, NULL, NULL, &hdr)) {
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
    size_t bufsz = LL_BUFSIZ_1K;
    char *buf = malloc(bufsz);
    if (buf == NULL)
        return -1;

    struct wire_job_sig req;
    req.job_id = jobid;
    req.sig    = sig;
    req.uid = getuid();

    struct protocol_header hdr;
    init_protocol_header(&hdr);
    hdr.operation = BATCH_JOB_SIGNAL;
    hdr.status    = MBD_OK;

    if (auth_sign_header(&hdr) < 0) {
        free(buf);
        return -1;
    }

    XDR xdrs;
    xdrmem_create(&xdrs, buf, (uint32_t)bufsz, XDR_ENCODE);
    if (!ll_encode_msg(&xdrs, (char *)&req,
                       xdr_wire_job_sig, &hdr)) {
        xdr_destroy(&xdrs);
        free(buf);
        errno = EPROTO;
        return -1;
    }
    size_t len = xdr_getpos(&xdrs);
    xdr_destroy(&xdrs);

    void *rep = NULL;
    struct protocol_header rhdr;
    if (call_mbd(buf, len, &rep, &rhdr) < 0) {
        free(buf);
        return -1;
    }
    free(buf);

    if (rhdr.status != MBD_OK) {
        errno = EPROTO;
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

    if (auth_sign_header(&hdr) < 0) {
        xdr_destroy(&xdrs);
        return NULL;
    }
    if (!ll_encode_msg(&xdrs, NULL, NULL, &hdr)) {
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
        out[i].name             = strdup(w.hosts[i].name);
        out[i].state            = w.hosts[i].state;
        out[i].max_jobs         = w.hosts[i].max_jobs;
        out[i].total_cpu        = w.hosts[i].total_cpu;
        out[i].free_cpu         = w.hosts[i].free_cpu;
        out[i].total_gpu        = w.hosts[i].total_gpu;
        out[i].free_gpu         = w.hosts[i].free_gpu;
        out[i].total_mem_mb     = w.hosts[i].total_mem_mb;
        out[i].free_mem_mb      = w.hosts[i].free_mem_mb;
        out[i].total_storage_mb = w.hosts[i].total_storage_mb;
        out[i].free_storage_mb  = w.hosts[i].free_storage_mb;
        out[i].num_jobs         = w.hosts[i].num_jobs;
        out[i].num_run          = w.hosts[i].num_run;
        out[i].num_susp         = w.hosts[i].num_susp;
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
    *n = -1;
    errno = 0;

    size_t bufsz = PACKET_HEADER_SIZE + sizeof(struct wire_job_info_req)
        + LL_BUFSIZ_64;
    char *buf = calloc(bufsz, 1);
    if (buf == NULL)
        return NULL;

    struct protocol_header hdr;
    init_protocol_header(&hdr);
    hdr.operation = BATCH_JOB_INFO;
    hdr.status    = MBD_OK;

    struct wire_job_info_req req;
    req.job_id = jobid;
    req.flags  = flags;
    req.uid = (uint32_t)getuid();

    if (auth_sign_header(&hdr) < 0) {
        free(buf);
        errno = EPROTO;
        return NULL;
    }

    XDR xdrs;
    xdrmem_create(&xdrs, buf, (uint32_t)bufsz, XDR_ENCODE);
    if (!ll_encode_msg(&xdrs, (char *)&req,
                       xdr_wire_job_info_req, &hdr)) {
        xdr_destroy(&xdrs);
        free(buf);
        errno = EPROTO;
        return NULL;
    }
    size_t len = xdr_getpos(&xdrs);
    xdr_destroy(&xdrs);

    void *rep = NULL;
    struct protocol_header rhdr;
    if (call_mbd(buf, len, &rep, &rhdr) < 0) {
        free(buf);
        return NULL;
    }
    free(buf);

    if (rhdr.status != 0) {
        errno = rhdr.status;
        free(rep);
        return NULL;
    }

    xdrmem_create(&xdrs, rep, rhdr.length, XDR_DECODE);
    struct wire_job_info_array w;
    memset(&w, 0, sizeof(w));
    if (!xdr_wire_job_info_array(&xdrs, &w)) {
        xdr_destroy(&xdrs);
        free(rep);
        errno = EPROTO;
        return NULL;
    }
    xdr_destroy(&xdrs);
    free(rep);

    if (w.njobs == 0) {
        *n = 0;
        return NULL;
    }

    struct job_info *out = calloc(w.njobs, sizeof(*out));
    if (out == NULL) {
        xdr_free((xdrproc_t)xdr_wire_job_info_array, (char *)&w);
        return NULL;
    }

    for (int i = 0; i < w.njobs; i++) {
        struct wire_job_info *src = &w.jobs[i];
        struct job_info      *dst = &out[i];

        dst->job_id      = src->job_id;
        dst->uid         = src->uid;
        dst->state      = src->state;
        dst->exit_status = src->exit_status;
        dst->priority    = src->priority;
        dst->submit_time = src->submit_time;
        dst->dispatch_time  = src->dispatch_time;
        dst->end_time    = src->end_time;
        dst->susp_time   = src->susp_time;
        dst->res.pid      = src->res.pid;
        dst->res.mem_mb   = src->res.mem_mb;
        dst->res.cpu_time = src->res.cpu_time;
        dst->name      = strdup(src->name);
        dst->queue     = strdup(src->queue);
        dst->from_host = strdup(src->from_host);
        dst->exec_hosts = strdup(src->exec_hosts);
        dst->comment   = strdup(src->comment);
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
        free(jobs[i].exec_hosts);
        free(jobs[i].comment);
    }
    free(jobs);
}

struct token_pool_info *llb_token_info(int32_t *ntokens)
{
    char buf[LL_BUFSIZ_256];
    XDR xdrs;

    xdrmem_create(&xdrs, buf, sizeof(buf), XDR_ENCODE);

    struct protocol_header hdr;
    init_protocol_header(&hdr);
    hdr.operation = BATCH_TOKEN_INFO;

    if (auth_sign_header(&hdr) < 0) {
        xdr_destroy(&xdrs);
        return NULL;
    }
    if (!ll_encode_msg(&xdrs, NULL, NULL, &hdr)) {
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
        free(rep);
        return NULL;
    }

    xdrmem_create(&xdrs, rep, rhdr.length, XDR_DECODE);

    struct wire_token_info_array w;
    memset(&w, 0, sizeof(w));

    if (!xdr_wire_token_info_array(&xdrs, &w)) {
        xdr_destroy(&xdrs);
        free(rep);
        return NULL;
    }

    xdr_destroy(&xdrs);
    free(rep);

    struct token_pool_info *out;
    out = calloc(w.ntokens, sizeof(*out));
    if (!out) {
        xdr_free((xdrproc_t)xdr_wire_token_info_array, (char *)&w);
        return NULL;
    }

    for (int i = 0; i < w.ntokens; i++) {
        out[i].name  = strdup(w.tokens[i].name);
        out[i].total = w.tokens[i].total;
        out[i].free  = w.tokens[i].free;
        out[i].used  = w.tokens[i].total - w.tokens[i].free;
    }

    *ntokens = w.ntokens;

    xdr_free((xdrproc_t)xdr_wire_token_info_array, (char *)&w);

    return out;
}

void llb_free_token_info(struct token_pool_info *t, int32_t n)
{
    for (int i = 0; i < n; i++)
        free(t[i].name);
    free(t);
}
