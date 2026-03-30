// Copyright (C) LavaLite Contributors
// GPL v2

#include <string.h>
#include <assert.h>
#include <syslog.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "batch/lib/wire.h"
#include "batch/mbd/mbd.h"

/* -----------------------------------------------------------
 * job submit
 * ----------------------------------------------------------- */
void job_submit(XDR *xdrs, int ch_id)
{
    (void)xdrs;

    LS_DEBUG("job_submit ch_id=%d", ch_id);

    /* TODO: decode wire_job_submit + enqueue reply */
}

/* -----------------------------------------------------------
 * job signal
 * ----------------------------------------------------------- */
void job_signal(XDR *xdrs, int ch_id)
{
    (void)xdrs;

    LS_DEBUG("job_signal ch_id=%d", ch_id);

    /* TODO: decode wire_job_sig */
}

/* -----------------------------------------------------------
 * job info
 * ----------------------------------------------------------- */
void job_info(XDR *xdrs, int ch_id)
{
    (void)xdrs;

    LS_DEBUG("job_info ch_id=%d", ch_id);

    /* TODO: build wire_job_info_array + reply */
}

/* -----------------------------------------------------------
 * queue info
 * ----------------------------------------------------------- */
void
queue_info(XDR *xdrs, int ch_id)
{
    (void)xdrs;

    int nqueues = ll_list_count(&queue_list);
    struct wire_queue_info *queues = calloc(nqueues, sizeof(struct wire_queue_info));
    if (queues == NULL) {
        LS_ERR("queue_info: calloc failed");
        return;
    }

    int i = 0;
    for (struct ll_list_entry *e = queue_list.head; e != NULL; e = e->next) {
        struct mbd_queue *q = (struct mbd_queue *)e;
        ll_strlcpy(queues[i].name, q->name, sizeof(queues[i].name));
        ll_strlcpy(queues[i].description, q->description, sizeof(queues[i].description));
        ll_strlcpy(queues[i].hosts, q->hosts, sizeof(queues[i].hosts));
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

    struct protocol_header hdr;
    init_protocol_header(&hdr);
    hdr.operation = BATCH_QUEUE_INFO_ACK;
    hdr.status    = MBD_OK;

    if (enqueue_payload(ch_id, &hdr, &reply, sizeof(reply),
                        xdr_wire_queue_info_array) < 0)
        LS_ERR("queue_info: enqueue_payload failed");

    free(queues);
}

/* -----------------------------------------------------------
 * group info
 * ----------------------------------------------------------- */
void
host_group_info(XDR *xdrs, int ch_id)
{
    (void)xdrs;

    int ngroups = ll_list_count(&group_list);
    struct wire_group_info *groups = calloc(ngroups, sizeof(struct wire_group_info));
    if (groups == NULL) {
        LS_ERR("host_group_info: calloc failed");
        return;
    }

    int i = 0;
    for (struct ll_list_entry *e = group_list.head; e != NULL; e = e->next) {
        struct mbd_group *g = (struct mbd_group *)e;
        ll_strlcpy(groups[i].name, g->name, sizeof(groups[i].name));
        ll_strlcpy(groups[i].members, g->members, sizeof(groups[i].members));
        groups[i].num_members = g->num_members;
        i++;
    }

    struct wire_group_info_array reply;
    reply.ngroups = ngroups;
    reply.groups  = groups;

    struct protocol_header hdr;
    init_protocol_header(&hdr);
    hdr.operation = BATCH_GROUP_INFO_ACK;
    hdr.status    = MBD_OK;

    if (enqueue_payload(ch_id, &hdr, &reply, sizeof(reply),
                        xdr_wire_group_info_array) < 0)
        LS_ERR("host_group_info: enqueue_payload failed");

    free(groups);
}

/* -----------------------------------------------------------
 * sbd register
 * ----------------------------------------------------------- */
void sbd_register(XDR *xdrs, int ch_id)
{
    (void)xdrs;

    LS_DEBUG("sbd_register ch_id=%d", ch_id);

    /* TODO: decode wire_sbd_register + update state */
}

/* -----------------------------------------------------------
 * compact done / failed
 * ----------------------------------------------------------- */
void compact_done(XDR *xdrs, int ch_id)
{
    (void)xdrs;

    LS_DEBUG("compact_done ch_id=%d", ch_id);

    /* TODO: handle compactor notification */
}

void host_info(XDR *xdrs, int ch_id)
{
    (void)xdrs;
    int nhosts;
    nhosts = ll_list_count(&host_list);

    struct wire_host_info *hosts;
    hosts = calloc(nhosts, sizeof(struct wire_host_info));
    if (!hosts) {
        LS_ERR("host_info: calloc failed");
        return;
    }

    int i = 0;
    struct ll_list_entry *e;
    for (e = host_list.head; e; e = e->next) {
        struct mbd_host *h = (struct mbd_host *)e;

        ll_strlcpy(hosts[i].name, h->net.name, MAXHOSTNAMELEN);

        hosts[i].status   = h->status;
        hosts[i].max_jobs = h->max_jobs;
        hosts[i].num_jobs = h->num_jobs;
        hosts[i].num_run  = h->num_run;
        hosts[i].num_susp = h->num_susp;

        i++;
    }

    struct wire_host_info_array reply;
    reply.nhosts = nhosts;
    reply.hosts  = hosts;

    struct protocol_header hdr;
    init_protocol_header(&hdr);
    hdr.operation = BATCH_HOST_INFO_ACK;
    hdr.status = MBD_OK;

    if (enqueue_payload(ch_id, &hdr, &reply, sizeof(reply),
                        xdr_wire_host_info_array) < 0) {
        LS_ERR("host_info: enqueue_payload failed");
        free(hosts);
        return;
    }

    free(hosts);
}
