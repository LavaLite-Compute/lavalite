/*
 * Copyright (C) LavaLite Contributors
 * GPL v2
 */

#include <sys/param.h>
#include <rpc/types.h>
#include <rpc/xdr.h>
#include <sys/epoll.h>

#include "base/lib/ll.protocol.h"
#include "base/lib/ll.bufsiz.h"
#include "base/lib/ll.conf.h"
#include "base/lib/ll.list.h"
#include "base/lib/ll.channel.h"

#include "batch/lib/wire.h"

/* -----------------------------------------------------------------------
 * control
 * ----------------------------------------------------------------------- */

bool_t xdr_wire_sbd_job(XDR *xdrs, struct wire_sbd_job *p)
{
    if (!xdr_int64_t(xdrs, &p->job_id))
        return false;
    if (!xdr_int32_t(xdrs, &p->pid))
        return false;
    return true;
}

bool_t xdr_wire_sbd_register(XDR *xdrs, struct wire_sbd_register *p)
{
    if (!xdr_opaque(xdrs, (char *) p->hostname, MAXHOSTNAMELEN))
        return false;
    if (!xdr_array(xdrs, (char **) &p->jobs, (u_int *) &p->num_jobs, INT32_MAX,
                   sizeof(struct wire_sbd_job), (xdrproc_t) xdr_wire_sbd_job))
        return false;
    return true;
}

/* -----------------------------------------------------------------------
 * job
 * ----------------------------------------------------------------------- */

bool_t xdr_wire_job_state(XDR *xdrs, struct wire_job_state *p)
{
    if (!xdr_int64_t(xdrs, &p->job_id))
        return false;
    if (!xdr_int32_t(xdrs, &p->state))
        return false;
    return true;
}

bool_t xdr_wire_job_finish(XDR *xdr, struct wire_job_finish *f)
{
    if (!xdr_int64_t(xdr, &f->job_id))
        return false;
    if (!xdr_int32_t(xdr, &f->state))
        return false;
    if (!xdr_int32_t(xdr, &f->exit_status))
        return false;
    if (!xdr_int32_t(xdr, &f->pid))
        return false;
    if (!xdr_uint64_t(xdr, &f->mem_mb))
        return false;
    if (!xdr_uint64_t(xdr, &f->swap_mb))
        return false;
    if (!xdr_double(xdr, &f->cpu_time))
        return false;

    return true;
}

bool_t xdr_wire_job_script(XDR *xdrs, struct wire_job_script *p)
{
    if (!xdr_bytes(xdrs, &p->data, &p->len, UINT32_MAX))
        return false;
    return true;
}

bool_t xdr_wire_job_sig(XDR *xdrs, struct wire_job_sig *p)
{
    if (!xdr_int64_t(xdrs, &p->job_id))
        return false;
    if (!xdr_int32_t(xdrs, &p->sig))
        return false;
    if (!xdr_uint32_t(xdrs, &p->uid))
        return false;
    return true;
}

bool_t xdr_wire_job_submit(XDR *xdrs, struct wire_job_submit *s)
{
    if (!xdr_opaque(xdrs, s->name, sizeof(s->name)))
        return false;
    if (!xdr_opaque(xdrs, s->queue, sizeof(s->queue)))
        return false;
    if (!xdr_opaque(xdrs, s->project, sizeof(s->project)))
        return false;
    if (!xdr_opaque(xdrs, s->comment, sizeof(s->comment)))
        return false;
    if (!xdr_opaque(xdrs, s->machines, sizeof(s->machines)))
        return false;
    if (!xdr_opaque(xdrs, s->in_file, sizeof(s->in_file)))
        return false;
    if (!xdr_opaque(xdrs, s->out_file, sizeof(s->out_file)))
        return false;
    if (!xdr_opaque(xdrs, s->err_file, sizeof(s->err_file)))
        return false;
    if (!xdr_opaque(xdrs, s->cwd, sizeof(s->cwd)))
        return false;
    if (!xdr_opaque(xdrs, s->depend_cond, sizeof(s->depend_cond)))
        return false;
    if (!xdr_opaque(xdrs, s->command, sizeof(s->command)))
        return false;
    if (!xdr_opaque(xdrs, s->gpu_type, sizeof(s->gpu_type)))
        return false;
    if (!xdr_opaque(xdrs, s->from_host, sizeof(s->from_host)))
        return false;
    if (!xdr_opaque(xdrs, s->username, sizeof(s->username)))
        return false;
    if (!xdr_opaque(xdrs, s->home_dir, sizeof(s->home_dir)))
        return false;
    if (!xdr_opaque(xdrs, s->tokenpool, sizeof(s->tokenpool)))
        return false;
    if (!xdr_int32_t(xdrs, &s->num_cpus))
        return false;
    if (!xdr_int32_t(xdrs, &s->num_hosts))
        return false;
    if (!xdr_int32_t(xdrs, &s->num_gpus))
        return false;
    if (!xdr_uint64_t(xdrs, &s->mem_mb))
        return false;
    if (!xdr_uint32_t(xdrs, &s->uid))
        return false;
    if (!xdr_uint32_t(xdrs, &s->gid))
        return false;
    if (!xdr_uint32_t(xdrs, &s->umask))
        return false;
    if (!xdr_uint32_t(xdrs, &s->flags))
        return false;
    if (!xdr_int64_t(xdrs, &s->begin_time))
        return false;
    if (!xdr_int64_t(xdrs, &s->term_time))
        return false;
    if (!xdr_int64_t(xdrs, &s->susp_time))
        return false;
    if (!xdr_int64_t(xdrs, &s->resume_time))
        return false;
    return true;
}

bool_t xdr_wire_job_submit_reply(XDR *xdrs, struct wire_job_submit_reply *r)
{
    if (!xdr_int64_t(xdrs, &r->job_id))
        return false;
    return true;
}

bool_t xdr_wire_job_info_req(XDR *xdrs, struct wire_job_info_req *p)
{
    if (!xdr_int64_t(xdrs, &p->job_id))
        return false;
    if (!xdr_int32_t(xdrs, &p->flags))
        return false;
    if (!xdr_uint32_t(xdrs, &p->uid))
        return false;
    return true;
}

bool_t xdr_wire_job_info(XDR *xdrs, struct wire_job_info *p)
{
    if (!xdr_int64_t(xdrs, &p->job_id))
        return false;
    if (!xdr_uint32_t(xdrs, &p->uid))
        return false;
    if (!xdr_int32_t(xdrs, &p->pid))
        return false;
    if (!xdr_int32_t(xdrs, &p->state))
        return false;
    if (!xdr_int32_t(xdrs, &p->exit_status))
        return false;
    if (!xdr_int32_t(xdrs, &p->priority))
        return false;
    if (!xdr_int32_t(xdrs, &p->pend_reason))
        return false;
    if (!xdr_int64_t(xdrs, &p->submit_time))
        return false;
    if (!xdr_int64_t(xdrs, &p->dispatch_time))
        return false;
    if (!xdr_int64_t(xdrs, &p->end_time))
        return false;
    if (!xdr_int64_t(xdrs, &p->susp_time))
        return false;
    if (!xdr_opaque(xdrs, p->name, sizeof(p->name)))
        return false;
    if (!xdr_opaque(xdrs, p->queue, sizeof(p->queue)))
        return false;
    if (!xdr_opaque(xdrs, p->from_host, sizeof(p->from_host)))
        return false;
    if (!xdr_opaque(xdrs, p->exec_hosts, sizeof(p->exec_hosts)))
        return false;
    if (!xdr_opaque(xdrs, p->comment, sizeof(p->comment)))
        return false;
    return true;
}

bool_t xdr_wire_job_info_array(XDR *xdrs, struct wire_job_info_array *p)
{
    if (!xdr_array(xdrs, (char **) &p->jobs, (u_int *) &p->njobs, INT32_MAX,
                   sizeof(struct wire_job_info), (xdrproc_t) xdr_wire_job_info))
        return false;
    return true;
}

bool_t xdr_wire_job_start(XDR *xdrs, struct wire_job_start *p)
{
    if (!xdr_int64_t(xdrs, &p->job_id))
        return false;
    if (!xdr_uint32_t(xdrs, &p->uid))
        return false;
    if (!xdr_uint32_t(xdrs, &p->gid))
        return false;
    if (!xdr_uint32_t(xdrs, &p->umask))
        return false;
    if (!xdr_opaque(xdrs, p->job_name, sizeof(p->job_name)))
        return false;
    if (!xdr_opaque(xdrs, p->queue, sizeof(p->queue)))
        return false;
    if (!xdr_opaque(xdrs, p->username, sizeof(p->username)))
        return false;
    if (!xdr_opaque(xdrs, p->home_dir, sizeof(p->home_dir)))
        return false;
    if (!xdr_opaque(xdrs, p->cwd, sizeof(p->cwd)))
        return false;
    if (!xdr_opaque(xdrs, p->command, sizeof(p->command)))
        return false;
    if (!xdr_opaque(xdrs, p->in_file, sizeof(p->in_file)))
        return false;
    if (!xdr_opaque(xdrs, p->out_file, sizeof(p->out_file)))
        return false;
    if (!xdr_opaque(xdrs, p->err_file, sizeof(p->err_file)))
        return false;
    if (!xdr_opaque(xdrs, p->hosts, sizeof(p->hosts)))
        return false;
    if (!xdr_int64_t(xdrs, &p->term_time))
        return false;
    if (!xdr_int32_t(xdrs, &p->gpus_per_host))
        return false;
    if (!xdr_int32_t(xdrs, &p->ncpus))
        return false;
    if (!xdr_uint64_t(xdrs, &p->mem_mb))
        return false;
    if (!xdr_opaque(xdrs, p->gpu_type, sizeof(p->gpu_type)))
        return false;
    if (!xdr_wire_job_script(xdrs, &p->script))
        return false;
    return true;
}

bool_t xdr_wire_job_reply(XDR *xdrs, struct wire_job_reply *p)
{
    if (!xdr_int64_t(xdrs, &p->job_id))
        return false;
    if (!xdr_int32_t(xdrs, &p->pid))
        return false;
    if (!xdr_int32_t(xdrs, &p->pgid))
        return false;
    if (!xdr_int32_t(xdrs, &p->state))
        return false;
    return true;
}

/* -----------------------------------------------------------------------
 * host
 * ----------------------------------------------------------------------- */

bool_t xdr_wire_host_info(XDR *xdrs, struct wire_host_info *p)
{
    if (!xdr_opaque(xdrs, p->name, sizeof(p->name)))
        return false;
    if (!xdr_int32_t(xdrs, &p->state))
        return false;
    if (!xdr_int32_t(xdrs, &p->max_jobs))
        return false;
    if (!xdr_int32_t(xdrs, &p->total_cpu))
        return false;
    if (!xdr_int32_t(xdrs, &p->free_cpu))
        return false;
    if (!xdr_int32_t(xdrs, &p->total_gpu))
        return false;
    if (!xdr_int32_t(xdrs, &p->free_gpu))
        return false;
    if (!xdr_uint64_t(xdrs, &p->total_mem_mb))
        return false;
    if (!xdr_uint64_t(xdrs, &p->free_mem_mb))
        return false;
    if (!xdr_uint64_t(xdrs, &p->total_storage_mb))
        return false;
    if (!xdr_uint64_t(xdrs, &p->free_storage_mb))
        return false;
    if (!xdr_int32_t(xdrs, &p->num_jobs))
        return false;
    if (!xdr_int32_t(xdrs, &p->num_run))
        return false;
    if (!xdr_int32_t(xdrs, &p->num_susp))
        return false;
    return true;
}

bool_t xdr_wire_host_info_array(XDR *xdrs, struct wire_host_info_array *p)
{
    if (!xdr_array(xdrs, (char **) &p->hosts, (u_int *) &p->nhosts, INT32_MAX,
                   sizeof(struct wire_host_info),
                   (xdrproc_t) xdr_wire_host_info))
        return false;
    return true;
}

bool_t xdr_wire_host_admin(XDR *xdrs, struct wire_host_admin *p)
{
    if (!xdr_opaque(xdrs, p->name, sizeof(p->name)))
        return false;
    if (!xdr_int32_t(xdrs, &p->op))
        return false;
    if (!xdr_uint32_t(xdrs, &p->uid))
        return false;
    return true;
}

/* -----------------------------------------------------------------------
 * queue
 * ----------------------------------------------------------------------- */

bool_t xdr_wire_queue_info(XDR *xdrs, struct wire_queue_info *p)
{
    if (!xdr_opaque(xdrs, p->name, sizeof(p->name)))
        return false;
    if (!xdr_opaque(xdrs, p->description, sizeof(p->description)))
        return false;
    if (!xdr_opaque(xdrs, p->hosts, sizeof(p->hosts)))
        return false;
    if (!xdr_int32_t(xdrs, &p->priority))
        return false;
    if (!xdr_int32_t(xdrs, &p->max_jobs))
        return false;
    if (!xdr_int32_t(xdrs, &p->num_jobs))
        return false;
    if (!xdr_int32_t(xdrs, &p->num_pend))
        return false;
    if (!xdr_int32_t(xdrs, &p->num_run))
        return false;
    if (!xdr_int32_t(xdrs, &p->num_susp))
        return false;
    if (!xdr_int32_t(xdrs, &p->num_held))
        return false;
    if (!xdr_int32_t(xdrs, &p->num_cpus_used))
        return false;
    if (!xdr_int32_t(xdrs, &p->num_hosts_used))
        return false;
    if (!xdr_int32_t(xdrs, &p->status))
        return false;
    return true;
}

bool_t xdr_wire_queue_admin(XDR *xdrs, struct wire_queue_admin *p)
{
    if (!xdr_opaque(xdrs, p->name, sizeof(p->name)))
        return false;
    if (!xdr_int32_t(xdrs, &p->op))
        return false;
    if (!xdr_uint32_t(xdrs, &p->uid))
        return false;
    return true;
}

bool_t xdr_wire_queue_info_array(XDR *xdrs, struct wire_queue_info_array *p)
{
    if (!xdr_array(xdrs, (char **) &p->queues, (u_int *) &p->nqueues, INT32_MAX,
                   sizeof(struct wire_queue_info),
                   (xdrproc_t) xdr_wire_queue_info))
        return false;
    return true;
}

/* -----------------------------------------------------------------------
 * group
 * ----------------------------------------------------------------------- */

bool_t xdr_wire_group_info(XDR *xdrs, struct wire_group_info *p)
{
    if (!xdr_opaque(xdrs, p->name, sizeof(p->name)))
        return false;
    if (!xdr_int32_t(xdrs, &p->num_members))
        return false;
    if (!xdr_opaque(xdrs, p->members, sizeof(p->members)))
        return false;
    return true;
}

bool_t xdr_wire_group_info_array(XDR *xdrs, struct wire_group_info_array *p)
{
    if (!xdr_array(xdrs, (char **) &p->groups, (u_int *) &p->ngroups, INT32_MAX,
                   sizeof(struct wire_group_info),
                   (xdrproc_t) xdr_wire_group_info))
        return false;
    return true;
}

bool_t xdr_wire_job_ack(XDR *xdrs, struct wire_job_ack *p)
{
    if (!xdr_int64_t(xdrs, &p->job_id))
        return false;
    if (!xdr_int32_t(xdrs, &p->ack_op))
        return false;
    return true;
}

bool_t xdr_wire_token_info(XDR *xdrs, struct wire_token_info *p)
{
    if (!xdr_opaque(xdrs, p->name, sizeof(p->name)))
        return false;
    if (!xdr_int32_t(xdrs, &p->total))
        return false;
    if (!xdr_int32_t(xdrs, &p->free))
        return false;
    return true;
}

bool_t xdr_wire_token_info_array(XDR *xdrs, struct wire_token_info_array *p)
{
    if (!xdr_array(xdrs, (char **) &p->tokens, (u_int *) &p->ntokens, INT32_MAX,
                   sizeof(struct wire_token_info),
                   (xdrproc_t) xdr_wire_token_info))
        return false;
    return true;
}
