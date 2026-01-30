/*
 * Copyright (C) LavaLite Contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#include "lsbatch/daemons/sbd.h"

extern int sbd_mbd_chan;      /* defined in sbatchd.main.c */

// the ch_id in input is the channel we have opened with mbatchd
//
int sbd_handle_mbd(int ch_id)
{
    struct chan_data *chan = &channels[ch_id];

    if (chan->chan_events == CHAN_EPOLLERR) {
        LS_ERRX("lost connection with mbd on channel=%d socket err=%d",
                ch_id, chan_sock_error(ch_id));
        sbd_mbd_link_down();
        return -1;
    }

    if (chan->chan_events != CHAN_EPOLLIN) {
        // channel is not ready
        return 0;
    }

    // Get the packet header from the channel first
    struct Buffer *buf;
    if (chan_dequeue(ch_id, &buf) < 0) {
        LS_ERR("chan_dequeue() failed");
        return -1;
    }

    if (!buf || buf->len < PACKET_HEADER_SIZE) {
        LS_ERR("short header from mbd on channel=%d: len=%zu",
               ch_id, buf ? buf->len : 0);
        return -1;
    }

    XDR xdrs;
    struct packet_header hdr;
    // Allocate the buffer data based on what was sent
    xdrmem_create(&xdrs, buf->data, buf->len, XDR_DECODE);
    if (!xdr_pack_hdr(&xdrs, &hdr)) {
        LS_ERR("xdr_pack_hdr failed");
        xdr_destroy(&xdrs);
        return -1;
    }

    LS_DEBUG("mbd requesting operation=%s", mbd_op_str(hdr.operation));

    // sbd handler
    switch (hdr.operation) {
    case BATCH_NEW_JOB:
        // a new job from mbd has arrived
        sbd_new_job(ch_id, &xdrs, &hdr);
        break;
    case BATCH_NEW_JOB_REPLY_ACK:
        // this indicate the ack of the previous job_reply
        // has reached the mbd who logged in the events
        // we can send a new event sbd_enqueue_execute
        sbd_new_job_reply_ack(ch_id, &xdrs, &hdr);
        break;
    case BATCH_JOB_EXECUTE_ACK:
        sbd_job_execute_ack(ch_id, &xdrs, &hdr);
        break;
    case BATCH_JOB_FINISH_ACK:
        sbd_job_finish_ack(ch_id, &xdrs, &hdr);
        break;
    case BATCH_JOB_SIGNAL:
        sbd_signal_job(ch_id, &xdrs, &hdr);
        break;
    case BATCH_SBD_REGISTER_ACK:
        // informational only; no action required
        LS_INFO("received ack=%s from mbd", mbd_op_str(hdr.operation));
        break;
    default:
        break;
    }

    xdr_destroy(&xdrs);
    return 0;
}
