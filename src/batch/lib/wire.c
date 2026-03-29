// Copyright (C) LavaLite Contributors
// GPL v2


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

bool_t xdr_wire_compact_notify(XDR *xdrs, struct wire_compact_notify *p)
{
    if (!xdr_int32_t(xdrs, &p->status))
        return false;

    if (!xdr_int64_t(xdrs, &p->compact_time))
        return false;

    return true;
}

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
    if (!xdr_vector(xdrs, (char *)p->hostname,
                    MAXHOSTNAMELEN, sizeof(char), (xdrproc_t)xdr_char))
        return false;

    if (!xdr_array(xdrs,
                   (char **)&p->jobs,
                   (u_int *)&p->num_jobs,
                   INT32_MAX,
                   sizeof(struct wire_sbd_job),
                   (xdrproc_t)xdr_wire_sbd_job))
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

bool_t xdr_wire_job_file(XDR *xdrs, struct wire_job_file *p)
{
    if (!xdr_u_int32_t(xdrs, &p->len))
        return false;

    if (!xdr_bytes(xdrs,
                   (char **)&p->data,
                   &p->len,
                   UINT_MAX))
        return false;

    return true;
}

bool_t xdr_wire_job_sig(XDR *xdrs, struct wire_job_sig *p)
{
    if (!xdr_int64_t(xdrs, &p->job_id))
        return false;

    if (!xdr_int32_t(xdrs, &p->sig))
        return false;

    return true;
}

bool_t xdr_wire_job_submit(XDR *xdrs, struct wire_job_submit *p)
{
    if (!xdr_vector(xdrs, p->job_name,
                    LL_BUFSIZ_64, sizeof(char), (xdrproc_t)xdr_char))
        return false;

    if (!xdr_vector(xdrs, p->queue,
                    LL_BUFSIZ_64, sizeof(char), (xdrproc_t)xdr_char))
        return false;

    if (!xdr_vector(xdrs, p->hosts,
                    LL_BUFSIZ_256, sizeof(char), (xdrproc_t)xdr_char))
        return false;

    if (!xdr_vector(xdrs, p->command,
                    LL_BUFSIZ_1K, sizeof(char), (xdrproc_t)xdr_char))
        return false;

    if (!xdr_vector(xdrs, p->in_file,
                    PATH_MAX, sizeof(char), (xdrproc_t)xdr_char))
        return false;

    if (!xdr_vector(xdrs, p->out_file,
                    PATH_MAX, sizeof(char), (xdrproc_t)xdr_char))
        return false;

    if (!xdr_vector(xdrs, p->err_file,
                    PATH_MAX, sizeof(char), (xdrproc_t)xdr_char))
        return false;

    if (!xdr_vector(xdrs, p->project_name,
                    LL_BUFSIZ_64, sizeof(char), (xdrproc_t)xdr_char))
        return false;

    if (!xdr_int64_t(xdrs, &p->begin_time))
        return false;

    if (!xdr_int64_t(xdrs, &p->term_time))
        return false;

    if (!xdr_int32_t(xdrs, &p->num_cpu))
        return false;

    if (!xdr_int32_t(xdrs, &p->num_hosts))
        return false;

    if (!xdr_int32_t(xdrs, &p->num_gpu))
        return false;

    if (!xdr_u_int64_t(xdrs, &p->mem_mb))
        return false;

    return true;
}

bool_t xdr_wire_submit_reply(XDR *xdrs, struct wire_submit_reply *p)
{
    if (!xdr_int64_t(xdrs, &p->job_id))
        return false;

    if (!xdr_int32_t(xdrs, &p->error))
        return false;

    return true;
}

bool_t xdr_wire_job_resources(XDR *xdrs, struct wire_job_resources *p)
{
    if (!xdr_int32_t(xdrs, &p->job_pid))
        return false;

    return true;
}

bool_t xdr_wire_job_info(XDR *xdrs, struct wire_job_info *p)
{
    if (!xdr_int64_t(xdrs, &p->job_id))
        return false;

    if (!xdr_u_int32_t(xdrs, &p->uid))
        return false;

    if (!xdr_int32_t(xdrs, &p->status))
        return false;

    if (!xdr_int32_t(xdrs, &p->exit_status))
        return false;

    if (!xdr_int64_t(xdrs, &p->submit_time))
        return false;

    if (!xdr_int64_t(xdrs, &p->start_time))
        return false;

    if (!xdr_int64_t(xdrs, &p->end_time))
        return false;

    if (!xdr_float(xdrs, &p->cpu_time))
        return false;

    if (!xdr_vector(xdrs, p->from_host,
                    MAXHOSTNAMELEN, sizeof(char), (xdrproc_t)xdr_char))
        return false;

    if (!xdr_int32_t(xdrs, &p->num_exec_hosts))
        return false;

    if (!xdr_vector(xdrs, p->exec_host,
                    MAXHOSTNAMELEN, sizeof(char), (xdrproc_t)xdr_char))
        return false;

    if (!xdr_vector(xdrs, p->job_name,
                    LL_BUFSIZ_64, sizeof(char), (xdrproc_t)xdr_char))
        return false;

    if (!xdr_vector(xdrs, p->queue,
                    LL_BUFSIZ_64, sizeof(char), (xdrproc_t)xdr_char))
        return false;

    if (!xdr_int32_t(xdrs, &p->num_cpu))
        return false;

    if (!xdr_int32_t(xdrs, &p->num_hosts))
        return false;

    if (!xdr_int32_t(xdrs, &p->num_gpu))
        return false;

    if (!xdr_u_int64_t(xdrs, &p->mem_mb))
        return false;

    if (!xdr_int64_t(xdrs, &p->begin_time))
        return false;

    if (!xdr_int64_t(xdrs, &p->term_time))
        return false;

    if (!xdr_wire_job_resources(xdrs, &p->resources))
        return false;

    return true;
}

bool_t xdr_wire_job_info_array(XDR *xdrs, struct wire_job_info_array *p)
{
    if (!xdr_array(xdrs,
                   (char **)&p->jobs,
                   (u_int *)&p->njobs,
                   INT32_MAX,
                   sizeof(struct wire_job_info),
                   (xdrproc_t)xdr_wire_job_info))
        return false;

    return true;
}

/* -----------------------------------------------------------------------
 * host
 * ----------------------------------------------------------------------- */

bool_t xdr_wire_host_info(XDR *xdrs, struct wire_host_info *p)
{
    if (!xdr_vector(xdrs, p->host,
                    MAXHOSTNAMELEN, sizeof(char), (xdrproc_t)xdr_char))
        return false;

    if (!xdr_int32_t(xdrs, &p->status))
        return false;

    if (!xdr_int32_t(xdrs, &p->max_jobs))
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
    if (!xdr_array(xdrs,
                   (char **)&p->hosts,
                   (u_int *)&p->nhosts,
                   INT32_MAX,
                   sizeof(struct wire_host_info),
                   (xdrproc_t)xdr_wire_host_info))
        return false;

    return true;
}

/* -----------------------------------------------------------------------
 * queue
 * ----------------------------------------------------------------------- */


bool_t xdr_wire_queue_info_array(XDR *xdrs, struct wire_queue_info_array *p)
{
    if (!xdr_array(xdrs,
                   (char **)&p->queues,
                   (u_int *)&p->nqueues,
                   INT32_MAX,
                   sizeof(struct wire_queue_info),
                   (xdrproc_t)xdr_wire_queue_info))
        return false;

    return true;
}

bool_t xdr_wire_queue_info(XDR *xdrs, struct wire_queue_info *q)
{
    if (!xdr_vector(xdrs,
                    q->name,
                    LL_BUFSIZ_64,
                    sizeof(char),
                    (xdrproc_t)xdr_char))
        return false;

    if (!xdr_vector(xdrs,
                    q->description,
                    LL_BUFSIZ_256,
                    sizeof(char),
                    (xdrproc_t)xdr_char))
        return false;

    if (!xdr_vector(xdrs,
                    q->hosts,
                    LL_BUFSIZ_256,
                    sizeof(char),
                    (xdrproc_t)xdr_char))
        return false;

    if (!xdr_int32_t(xdrs, &q->priority))
        return false;

    if (!xdr_int32_t(xdrs, &q->max_jobs))
        return false;

    if (!xdr_int32_t(xdrs, &q->num_pend))
        return false;

    if (!xdr_int32_t(xdrs, &q->num_run))
        return false;

    if (!xdr_int32_t(xdrs, &q->num_susp))
        return false;

    return true;
}

/* -----------------------------------------------------------------------
 * group
 * ----------------------------------------------------------------------- */

bool_t xdr_wire_group_info(XDR *xdrs, struct wire_group_info *p)
{
    if (!xdr_vector(xdrs, p->name,
                    LL_BUFSIZ_64, sizeof(char), (xdrproc_t)xdr_char))
        return false;

    if (!xdr_int32_t(xdrs, &p->num_members))
        return false;

    if (!xdr_vector(xdrs, p->members,
                    LL_BUFSIZ_1K, sizeof(char), (xdrproc_t)xdr_char))
        return false;

    return true;
}

bool_t xdr_wire_group_info_array(XDR *xdrs, struct wire_group_info_array *p)
{
    if (!xdr_array(xdrs,
                   (char **)&p->groups,
                   (u_int *)&p->ngroups,
                   INT32_MAX,
                   sizeof(struct wire_group_info),
                   (xdrproc_t)xdr_wire_group_info))
        return false;

    return true;
}
