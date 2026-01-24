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

/*
 * sbatchd → mbatchd status protocol:
 *
 *   - BATCH_STATUS_JOB    : generic status update (legacy path)
 *   - BATCH_JOB_EXECUTE   : pipeline milestone (EXECUTE)
 *   - BATCH_JOB_FINISH    : pipeline milestone (FINISH)
 *   - BATCH_RUSAGE_JOB    : periodic resource usage
 *   - BATCH_JOB_SIGNAL_REPLY
 *
 * All three carry the same statusReq payload.
 * The opcode identifies the pipeline stage; statusReq.newStatus
 * drives the core state transition.
 * The fourth is just the job resource usage.
 *
 * mbatchd ACKs each message with ack.acked_op = hdr->operation
 * after the event is committed to lsb.events.
 */
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

    struct packet_header sbd_hdr;
    if (!xdr_pack_hdr(&xdrs, &sbd_hdr)) {
        LS_ERR("xdr_pack_hdr failed for SBD chanfd=%d", ch_id);
        xdr_destroy(&xdrs);
        chan_free_buf(buf);
        mbd_sbd_disconnect(client);
        return -1;
    }
    LS_INFO("sbd %s operation %s", host_node->host,
            mbd_op_str(sbd_hdr.operation));
    /*
     * replyHdr.operation is sbdReplyType:
     *   ERR_NO_ERROR, ERR_BAD_REQ, ERR_START_FAIL, ...
     *
     * Right now, we only care about NEW_JOB replies.
     * Later we can add more cases for other SBD RPCs.
     */
    switch (sbd_hdr.operation) {
    case BATCH_NEW_JOB_REPLY:
        mbd_new_job_reply(client, &xdrs, &sbd_hdr);
        break;
    case BATCH_STATUS_JOB:
    case BATCH_JOB_EXECUTE:
    case BATCH_JOB_FINISH:
    case BATCH_RUSAGE_JOB:
        // could be from sbd_enqueue_execute or from
        // sbd_enqueue_finish the jobSpecs.jStatus will tell
        mbd_job_status(client, &xdrs, &sbd_hdr);
        break;
    case BATCH_JOB_SIGNAL_REPLY:
        mbd_job_signal_reply(client, &xdrs, &sbd_hdr);
        break;
    default:
        LS_ERR("unexpected SBD reply code=%d from host %s",
               sbd_hdr.operation, client->host_node->host);
        break;
    }

    xdr_destroy(&xdrs);
    chan_free_buf(buf);

    return 0;
}

// LavaLite
// this is still a client-like request coming through the client handler
// after this call the connection becomes a permanent sbd connection.
int
mbd_sbd_register(XDR *xdrs, struct mbd_client_node *client,
                 struct packet_header *hdr)
{
    (void)hdr;

    struct wire_sbd_register req;
    memset(&req, 0, sizeof(req));

    if (!xdr_wire_sbd_register(xdrs, &req)) {
        LS_ERR("SBD_REGISTER decode failed");
        return enqueue_header_reply(client->chanfd, LSBE_XDR);
    }

    char hostname[MAXHOSTNAMELEN];
    memcpy(hostname, req.hostname, sizeof(hostname));
    hostname[sizeof(hostname) - 1] = 0;

    struct hData *host_data = getHostData(hostname);
    if (host_data == NULL) {
        LS_ERR("SBD_REGISTER from unknown host %s", hostname);
        return enqueue_header_reply(client->chanfd, LSBE_BAD_HOST);
    }

    // offlist the client and adopt it in the hData
    offList((struct listEntry *)client);
    host_data->sbd_node = client;
    // a back pointer to the hData using the current client connection
    host_data->sbd_node->host_node = host_data;

    // now insert into hash based on chanfd for fast retrieval
    char key[LL_BUFSIZ_32];

    snprintf(key, sizeof(key), "%d", client->chanfd);
    ll_hash_insert(&hdata_by_chan, key, host_data, 1);

    LS_INFO("sbatchd register hostname=%s canon=%s addr=%s ch_id=%d",
            hostname, host_data->sbd_node->host.name,
            host_data->sbd_node->host.addr, host_data->sbd_node->chanfd);

    return enqueue_header_reply(client->chanfd, BATCH_SBD_REGISTER_REPLY);
}

int mbd_handle_signal_req(XDR *xdrs,
                          struct mbd_client_node *client,
                          struct packet_header *hdr,
                          struct lsfAuth *auth)
{
    struct signalReq req;

    if (!xdr_signalReq(xdrs, &req, hdr))
        return enqueue_header_reply(client->chanfd, LSBE_XDR);

    if (req.jobId == 0) {
        int rc = mbd_signal_all_jobs(client->chanfd, &req, auth);
        return enqueue_header_reply(client->chanfd, rc);
    }

    struct jData *job = getJobData(req.jobId);
    if (!job) {
        LS_INFO("job %s unknown to mbd", lsb_jobid2str(req.jobId));
        return enqueue_header_reply(client->chanfd, LSBE_NO_JOB);
    }

    int rc = mbd_signal_job(client->chanfd, job, &req, auth);
    if (rc != LSBE_NO_ERROR) {
        LS_ERR("failed enqueue signal %d for job %s rc=%d",
               req.sigValue, lsb_jobid2str(job->jobId), rc);
    }

    return enqueue_header_reply(client->chanfd, rc);
}
