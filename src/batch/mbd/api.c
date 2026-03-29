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
void queue_info(XDR *xdrs, int ch_id)
{
    (void)xdrs;

    LS_DEBUG("queue_info ch_id=%d", ch_id);

    /* TODO: build wire_queue_info_array */
}

/* -----------------------------------------------------------
 * group info
 * ----------------------------------------------------------- */
void host_group_info(XDR *xdrs, int ch_id)
{
    (void)xdrs;

    LS_DEBUG("group_info ch_id=%d", ch_id);

    /* TODO: build wire_group_info_array */
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

        ll_strlcpy(hosts[i].host, h->net.name, sizeof(hosts[i].host));

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
