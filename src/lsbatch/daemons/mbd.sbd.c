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
#include "lsbatch/daemons/sbatchd.h"

static bool_t mbd_status_reply_means_committed(int);
static void mbd_reset_sbd_job_list(struct hData *);


// LavaLite
int
do_sbd_register(XDR *xdrs, struct mbd_client_node *client,
                struct packet_header *hdr)
{
    (void)hdr;

    struct wire_sbd_register req;
    memset(&req, 0, sizeof(req));

    if (!xdr_wire_sbd_register(xdrs, &req)) {
        LS_ERR("SBD_REGISTER decode failed");
        return enqueue_header_reply(mbd_efd, client->chanfd, LSBE_XDR);
    }

    char hostname[MAXHOSTNAMELEN];
    memcpy(hostname, req.hostname, sizeof(hostname));
    hostname[sizeof(hostname) - 1] = 0;

    struct hData *host_data = getHostData(hostname);
    if (host_data == NULL) {
        LS_ERR("SBD_REGISTER from unknown host %s", hostname);
        return enqueue_header_reply(mbd_efd, client->chanfd, LSBE_BAD_HOST);
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

    return enqueue_header_reply(mbd_efd,
                                client->chanfd,
                                BATCH_SBD_REGISTER_REPLY);
}

int sbd_handle_new_job_reply(struct mbd_client_node *client,
                             XDR *xdrs,
                             struct packet_header *hdr)
{
    struct jobReply jobReply;

    struct hData *host_node = client->host_node;
    if (host_node == NULL) {
        LS_ERR("SBD NEW_JOB reply with NULL host_node (chanfd=%d)",
               client->chanfd);
        // Hard invariant violation: this should never happen.
        abort();
    }

    const char *host_name = host_node->host;
    /*
     * hdr->operation is an sbdReplyType:
     *   ERR_NO_ERROR, ERR_BAD_REQ
     *
     * In the current protocol, a jobReply payload is sent only on
     * ERR_NO_ERROR. For error codes we log and return for now.
     */

    if (hdr->operation != ERR_NO_ERROR) {
        LS_ERR("SBD NEW_JOB failed on host %s, reply_code=%s",
               host_name, mbd_op_str(hdr->operation));

        /*
         * TODO:
         *   Extend the protocol to include jobId also on error, or
         *   maintain a pending-job queue per SBD so we can map this
         *   error back to a specific job and update its state.
         */
        return -1;
    }

    memset(&jobReply, 0, sizeof(jobReply));

    if (!xdr_jobReply(xdrs, &jobReply, hdr)) {
        LS_ERR("xdr_jobReply decode failed for SBD NEW_JOB reply from host %s",
               host_name);
        //jStatusChange(jData, JOB_STAT_PEND, LOG_IT, fname);
        // maybe close the connection with sbd?
        return -1;
    }

    LS_INFO("mbd job=%"PRId64" operation %s from %s", jobReply.jobId,
            mbd_op_str(hdr->operation), host_name);

    // Map jobId -> job descriptor.
    struct jData *job;
    job = getJobData((int64_t)jobReply.jobId);
    if (job == NULL) {
        LS_ERR("SBD NEW_JOB reply for unknown jobId=%u from host %s",
               jobReply.jobId, host_name);
        return -1;
    }

    // Ignore replies for jobs that are no longer in START state.
    if (!IS_START(job->jStatus)) {
        return 0;
    }

    job->jobPid  = jobReply.jobPid;
    job->jobPGid = jobReply.jobPGid;
    job->jStatus = jobReply.jStatus; // typically JOB_STAT_RUN

    log_startjobaccept(job);

    /*
     * Acknowledge back to sbatchd so it can unblock.
     * We only send a small packet_header over the LavaLite channel.
     * No more blocking send path, everything is queued via chan_enqueue().
     */
    struct job_status_ack ack;
    memset(&ack, 0, sizeof(struct job_status_ack));
    ack.job_id = job->jobId;
    ack.acked_op = ERR_NO_ERROR;

    if (mbd_send_job_ack(client, BATCH_NEW_JOB_ACK, &ack) < 0) {
        LS_ERR("mbd failed enqueue job "PRId64" operation %s from %s",
               ack.job_id, mbd_op_str(hdr->operation), host_name);
    }

    LS_INFO("mbd job=%"PRId64" op=%s rc=%s acked",
            ack.job_id, mbd_op_str(hdr->operation));

    return 0;
}
/*
 * Handle statusReq messages coming from sbatchd.
 *
 * This handler is used for both legacy status updates and
 * pipeline milestones (EXECUTE / FINISH).
 *
 * The semantic stage is identified by hdr->operation.
 * The core transition is driven by statusReq.newStatus.
 */

int
sbd_handle_job_status(struct mbd_client_node *client,
                      XDR *xdrs,
                      struct packet_header *hdr)
{
    int reply;
    int schedule;

    if (client == NULL || xdrs == NULL || hdr == NULL) {
        errno = EINVAL;
        return -1;
    }

    schedule = 0;
    reply = LSBE_NO_ERROR;

    struct statusReq status_req;
    if (!xdr_statusReq(xdrs, &status_req, hdr)) {
        LS_ERR("xdr_statusReq failed");
        return -1;
    }

    const char *host_name = client->host_node->host;
    LS_INFO("mbd job=%"PRId64" operation %s from %s", status_req.jobId,
            mbd_op_str(hdr->operation), host_name);

    // Temporary: reuse the old handlers to update MBD state.
    // IMPORTANT: do not do host/port checks here; trust
    // client->host/client->host_node.
    switch (hdr->operation) {
    case BATCH_STATUS_JOB:
    case BATCH_JOB_EXECUTE:
    case BATCH_JOB_FINISH: {
        struct hostent hp;
        memset(&hp, 0, sizeof(hp));
        // do NOT strdup/free here; borrow the stable string
        hp.h_name = client->host.name;
        reply = statusJob(&status_req, &hp, &schedule);
        break;
    }
    case BATCH_RUSAGE_JOB: {
        struct hostent hp;

        memset(&hp, 0, sizeof(hp));
        hp.h_name = client->host.name;
        reply = rusageJob(&status_req, &hp);

        // Historically rusage there is no reply
        // just free and return based on reply.
        xdr_lsffree(xdr_statusReq, (char *)&status_req, hdr);
        return (reply == LSBE_NO_ERROR) ? 0 : -1;
    }

    default:
        LS_ERR("Unknown status op %d", hdr->operation);
        reply = LSBE_PROTOCOL;
        break;
    }

    // Free request now that handlers have consumed it.
    xdr_lsffree(xdr_statusReq, (char *)&status_req, hdr);

    if (!mbd_status_reply_means_committed(reply)) {
        LS_ERR("status op %s failed (%d) for job %"PRId64" from %s",
               mbd_op_str(hdr->operation), reply, status_req.jobId, host_name);
        return -1;   // no ACK
    }

    // New protocol ACK: always ACK status messages that are part of
    // the strict ordering.
    // job_id: take it from status_req
    struct job_status_ack ack;
    ack.job_id = status_req.jobId;
    ack.seq = hdr->sequence;
    /*
     * ACK the committed pipeline stage.
     *
     * ack.acked_op echoes the incoming opcode so that sbatchd
     * can advance the correct pipeline step (PID / EXECUTE / FINISH).
     */

    ack.acked_op = hdr->operation;

    // Close the channel in case of error
    if (mbd_send_job_ack(client, hdr->operation, &ack) < 0) {
        LS_ERR("mbd failed enqueue job=%"PRId64" operation %s from %s",
               ack.job_id, mbd_op_str(hdr->operation), host_name);
        return -1;
    }

    LS_INFO("mbd job=%"PRId64" op=%s rc=%s acked",
            ack.job_id,
            mbd_op_str(hdr->operation),
            mbd_op_str(reply));

    return 0;
}

// Return true only if the job status update was successfully committed in MBD.
// An ACK means "state applied and durable"; on errors we do not ACK and force
// sbatchd to reconnect to avoid advancing past a failed transition.
static bool_t
mbd_status_reply_means_committed(int reply)
{
    switch (reply) {
    case LSBE_NO_ERROR:
    case LSBE_STOP_JOB:
    case LSBE_LOCK_JOB:
        return true;
    default:
        return false;
    }
}

int sbd_handle_disconnect(struct mbd_client_node *client)
{
    struct hData *host_node = client->host_node;

    /*
     * Hard invariant:
     *     if this function is entered, this client MUST be the SBD
     *     connection for its host.
     *
     * Any violation indicates internal consistency failure, not
     * a runtime recoverable case.
     */
    if (host_node == NULL) {
        LS_ERR("sbd_handle_disconnect called with NULL host_node (chanfd=%d)",
               client->chanfd);
        abort();
    }

    if (host_node->sbd_node != client) {
        LS_ERR("sbd_handle_disconnect invariant violated: "
               "host->sbd_node (%p) != client (%p) for host %s",
               (void *)host_node->sbd_node,
               (void *)client,
               host_node->host);
        abort();
    }
    LS_DEBUG("closing connection with host %s", host_node->host);

    // Break the association first, then reset pending jobs.
    client->host_node = NULL;
    // invalidate the pointer as the sbd is down
    host_node->sbd_node = NULL;

    // dont reset the job list as jobs are still running
    // on the host they should probably go into UNKNWN state
    if (0)
        mbd_reset_sbd_job_list(host_node);
    char key[LL_BUFSIZ_32];

    snprintf(key, sizeof(key), "%d", client->chanfd);
    ll_hash_remove(&hdata_by_chan, key);

    // hose the client
    chan_close(client->chanfd);
    free(client);

    return 0;
}

int mbd_handle_slave_restart(struct mbd_client_node *client,
                             struct packet_header *reqHdr,
                             XDR *xdr_in)
{
    struct ll_host hs;
    struct hData *host_node;
    struct sbdPackage pkg;

    (void)hs;
    (void)host_node;
    (void)pkg;
    // 1) authenticate/portok + map host
    // 2) find host hData
    // 3) hStatChange(h, 0) etc.
    // 4) pkg.numJobs = countNumSpecs(h)
    // 5) pkg.jobs = calloc(...)
    // 6) fill pkg via sbatchdJobs(&pkg, h)
    // 7) reply using enqueue_payload(client->chanfd, BATCH_SLAVE_RESTART_REPLY, &pkg, xdr_sbdPackage)
    // 8) free pkg.jobs (and freeJobSpecs)

    return 0;
}

static void mbd_reset_sbd_job_list(struct hData *host_node)
{
    struct mbd_sbd_job *sj;

    struct ll_list *l = &host_node->sbd_job_list;
    while ((sj = (struct mbd_sbd_job *)ll_list_pop(l)) != NULL) {
        struct jData *job;

        if (sj == NULL) {
            LS_ERR("no sbd job in sbd_job_list?");
            //* Corrupt list, but do not crash the whole daemon.
            continue;
        }

        job = getJobData(sj->job->jobId);
        if (job != NULL && IS_START(job->jStatus)) {
            job->newReason = PEND_JOB_START_FAIL;
            jStatusChange(job, JOB_STAT_PEND, LOG_IT, (char *)__func__);
        }

        free(sj);
    }
}

int mbd_enqueue_hdr(struct mbd_client_node *client, int operation)
{
    int chfd = client->chanfd;
    struct Buffer *buf;
    if (chan_alloc_buf(&buf, sizeof(struct packet_header)) < 0) {
        LS_ERR("chan_alloc_buf failed for header reply on chanfd=%d", chfd);
        return -1;
    }

    XDR xdrs;
    xdrmem_create(&xdrs,
                  buf->data,
                  sizeof(struct packet_header),
                  XDR_ENCODE);

    struct packet_header hdr;
    init_pack_hdr(&hdr);
    hdr.operation = operation;

    if (!xdr_pack_hdr(&xdrs, &hdr)) {
        LS_ERR("xdr_pack_hdr failed for header reply on chanfd=%d", chfd);
        xdr_destroy(&xdrs);
        chan_free_buf(buf);
        return -1;
    }

    buf->len = XDR_GETPOS(&xdrs);
    xdr_destroy(&xdrs);

    if (chan_enqueue(chfd, buf) < 0) {
        LS_ERR("chan_enqueue failed for header reply on chanfd=%d", chfd);
        chan_free_buf(buf);
        return -1;
    }

    /*
     * Ensure the epoll loop is watching for writable events now that
     * there is pending output on this channel. This calls
     * into the channel/epoll layer to set EPOLLOUT.
     */
    struct epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.events = EPOLLIN | EPOLLOUT;   /* readable + writable */
    ev.data.u32 = (uint32_t)chfd;     /* or whatever you already use */

    if (epoll_ctl(mbd_efd, EPOLL_CTL_MOD, chan_sock(chfd), &ev) < 0) {
        LS_ERR("epoll_ctl(EPOLL_CTL_MOD, EPOLLIN|EPOLLOUT) failed for chanfd=%d",
               chfd);
        // Bug we should free what we have allocated
        return -1;
    }

    return 0;
}

// Send an explicit NEW_JOB ACK to sbatchd.
// This is a queued write (non-blocking at the call site).
//
// Wire:
//   hdr.operation = BATCH_NEW_JOB_ACK
//   payload       = struct new_job_ack (XDR)
//
// Notes:
// - new_job_ack carries jobId (and seq placeholder).
// - We keep this small and self-contained: header + payload in one buffer.
//
int
mbd_send_job_ack(struct mbd_client_node *client,
                 int operation,
                 const struct job_status_ack *ack)
{
    if (client == NULL || ack == NULL) {
        errno = EINVAL;
        return -1;
    }

    int cc = enqueue_payload(client->chanfd,
                             operation,
                             (void *)ack,
                             xdr_job_status_ack);
    if (cc < 0) {
        LS_ERR("enqueue_payload for job %"PRId64" ack failed",
               ack->job_id);
        return -1;
    }

    int chfd = client->chanfd;
    struct epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.events = EPOLLIN | EPOLLOUT;   /* readable + writable */
    ev.data.u32 = (uint32_t)chfd;     /* or whatever you already use */

    if (epoll_ctl(mbd_efd, EPOLL_CTL_MOD, chan_sock(chfd), &ev) < 0) {
        LS_ERR("epoll_ctl(EPOLL_CTL_MOD, EPOLLIN|EPOLLOUT) failed for chanfd=%d",
               chfd);
        // Bug we should free what we have allocated
        return -1;
    }

    return 0;
}

const char *
mbd_op_str(int op)
{
    switch (op) {
    /* sbatchd → mbatchd pipeline / status ops */
    case BATCH_JOB_EXECUTE:
        return "BATCH_JOB_EXECUTE";
    case BATCH_JOB_FINISH:
        return "BATCH_JOB_FINISH";
    case BATCH_STATUS_JOB:
        return "BATCH_STATUS_JOB";
    case BATCH_RUSAGE_JOB:
        return "BATCH_RUSAGE_JOB";

    /* ACK / control */
    case BATCH_NEW_JOB_ACK:
        return "BATCH_NEW_JOB_ACK";
    case BATCH_STATUS_MSG_ACK:
        return "BATCH_STATUS_MSG_ACK";

    /* protocol / reply codes */
    case LSBE_NO_ERROR:
        return "LSBE_NO_ERROR";
    case ERR_NO_ERROR:
        return "ERR_NO_ERROR";
    case ERR_BAD_REQ:
        return "ERR_BAD_REQ";
    case ERR_MEM:
        return "ERR_MEM";
    case ERR_FORK_FAIL:
        return "ERR_FORK_FAIL";

    default:
        return "UNKNOWN_OP";
    }
}

int mbd_send_signal_to_sbd(struct jData *job, struct signalReq *req)
{
    return 0;
}
