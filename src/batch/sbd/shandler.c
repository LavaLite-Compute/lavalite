/*
 * Copyright (C) LavaLite Contributors
 * GPL v2
 */

#include <syslog.h>

#include "base/lib/auth.h"
#include "base/lib/ll.syslog.h"
#include "base/lib/ll.channel.h"
#include "batch/sbd/sbd.h"
#include "batch/lib/rpc.h"

// the chan_id in input is the channel we have opened with mbatchd
//
int sbd_mbd_handle(int chan_id)
{
    struct chan_data *chan = &channels[chan_id];

    if (chan->chan_events == CHAN_EPOLLERR) {
        LS_ERRX("lost connection with mbd on channel=%d socket err=%d",
                chan_id, chan_sock_error(chan_id));
        sbd_mbd_link_down();
        return -1;
    }

    if (chan->chan_events != CHAN_EPOLLIN) {
        // channel is not ready
        return 0;
    }

    // Get the packet header from the channel first
    struct chan_buffer *buf;
    if (chan_dequeue(chan_id, &buf) < 0) {
        LS_ERR("chan_dequeue() failed");
        return -1;
    }

    if (!buf || buf->len < PACKET_HEADER_SIZE) {
        LS_ERR("short header from mbd on channel=%d: len=%d",
               chan_id, buf ? buf->len : 0);
        return -1;
    }

    XDR xdrs;
    struct protocol_header hdr;
    // Allocate the buffer data based on what was sent
    xdrmem_create(&xdrs, buf->data, buf->len, XDR_DECODE);
    if (!xdr_pack_hdr(&xdrs, &hdr)) {
        LS_ERR("xdr_pack_hdr failed");
        xdr_destroy(&xdrs);
        return -1;
    }

    if (auth_verify_header(&hdr) < 0) {
        LS_ERR("failed validate header opcode=%s from=%s",
               batch_op_str(hdr.operation), chan_addr_str(chan_id));
        xdr_destroy(&xdrs);
        chan_free_buf(buf);
        sbd_chan_shutdown(chan_id);
        return -1;
    }

    LS_DEBUG("mbd requesting operation=%s", batch_op_str(hdr.operation));

    // sbd handler
    switch (hdr.operation) {
    case BATCH_NEW_JOB:
        // a new job from mbd has arrived
        sbd_job_new(chan_id, &xdrs);
        break;
    case BATCH_NEW_JOB_ACK:
        // this indicate the ack of the previous job_reply
        // has reached the mbd who logged in the events
        // we can send a new event sbd_enqueue_execute
        sbd_job_new_ack(chan_id, &xdrs);
        break;
    case BATCH_JOB_EXECUTE_ACK:
        sbd_job_execute_ack(chan_id, &xdrs);
        break;
    case BATCH_JOB_FINISH_ACK:
        sbd_job_finish_ack(chan_id, &xdrs);
        break;
    case BATCH_SBD_JOB_SIGNAL:
        sbd_job_signal(chan_id, &xdrs);
        break;
    case BATCH_SBD_REGISTER_ACK:
        sbd_register_ack(chan_id, &xdrs);
        break;
    default:
        break;
    }

    xdr_destroy(&xdrs);
    chan_free_buf(buf);

    return 0;
}

void sbd_job_new(int chan_id, XDR *xdrs)
{
    (void)chan_id;
    (void)xdrs;
}

void sbd_job_new_ack(int chan_id, XDR *xdrs)
{
    (void)chan_id;
    (void)xdrs;
}

void sbd_job_execute_ack(int chan_id, XDR *xdrs)
{
    (void)chan_id;
    (void)xdrs;
}

void sbd_job_finish_ack(int chan_id, XDR *xdrs)
{
    (void)chan_id;
    (void)xdrs;
}

void sbd_register_ack(int chan_id, XDR *xdrs)
{
    (void)chan_id;
    (void)xdrs;
}

int sbd_job_signal(int chan_id, XDR *xdrs)
{
    (void)chan_id;
    (void)xdrs;
    return 0;
}
