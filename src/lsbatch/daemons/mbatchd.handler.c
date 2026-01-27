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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 USA
 *
 */

#include "lsbatch/daemons/mbd.h"
#include "lsbatch/daemons/mbatchd.h"
#include "lsbatch/daemons/sbatchd.h"

// These are the top handlers of sbd events
int mbd_dispatch_sbd(struct mbd_client_node *client)
{
    struct hData *host_node = client->host_node;
    if (host_node == NULL) {
        LS_ERR("mbd_dispatch_sbd called with NULL host_node (chanfd=%d)",
               client->chanfd);
        abort();
    }

    int ch_id = client->chanfd;
    // handle the exception on the channel
    if (channels[ch_id].chan_events == CHAN_EPOLLERR) {
        // Use the X version as errno could have been changed
        LS_ERRX("epoll error on SBD channel for host %s (chanfd=%d)",
               host_node->host, ch_id);
        mbd_sbd_disconnect(client);
        return -1;
    }

    struct Buffer *buf;
    if (chan_dequeue(ch_id, &buf) < 0) {
        LS_ERR("%s: chan_dequeue failed for SBD chanfd=%d", __func__, ch_id);
        mbd_sbd_disconnect(client);
        return -1;
    }

    XDR xdrs;
    xdrmem_create(&xdrs, buf->data, buf->len, XDR_DECODE);

    // Dispatcher decode header
    struct packet_header sbd_hdr;
    if (!xdr_pack_hdr(&xdrs, &sbd_hdr)) {
        LS_ERR("xdr_pack_hdr failed for SBD chanfd=%d", ch_id);
        xdr_destroy(&xdrs);
        chan_free_buf(buf);
        mbd_sbd_disconnect(client);
        return -1;
    }

    LS_INFO("routing operation %s from %s", mbd_op_str(sbd_hdr.operation),
            host_node->host);

    // route to the appropriate handler
    switch (sbd_hdr.operation) {
    case BATCH_NEW_JOB_REPLY:
        mbd_new_job_reply(client, &xdrs, &sbd_hdr);
        break;
    case BATCH_JOB_EXECUTE:
        mbd_set_status_execute(client, &xdrs, &sbd_hdr);
        break;
    case BATCH_JOB_FINISH:
        mbd_set_status_finish(client, &xdrs, &sbd_hdr);
        break;
    case BATCH_RUSAGE_JOB:
        // No ack for rusage
        mbd_set_rusage_update(client, &xdrs, &sbd_hdr);
        break;
    case BATCH_JOB_SIGNAL_REPLY:
        mbd_job_signal_reply(client, &xdrs, &sbd_hdr);
    break;

    default:
        LS_ERR("unexpected SBD protocol code=%d from host %s",
               sbd_hdr.operation, client->host.name);
        break;
    }

    xdr_destroy(&xdrs);
    chan_free_buf(buf);

    return 0;
}
