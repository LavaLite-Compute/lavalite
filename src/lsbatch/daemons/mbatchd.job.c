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
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#include "lsbatch/daemons/mbd.h"
#include "lsbatch/daemons/mbatchd.h"

static void mbd_reset_sbd_job_list(struct hData *);
static int enqueue_sig_buckets(struct ll_hash *, int32_t);
static int signal_sbd_jobs(struct sig_host_bucket *, int32_t, int32_t);
static void free_sig_bucket_table(struct ll_hash *);
static int bucket_add_jobid(struct sig_host_bucket *, int64_t);
static int finish_pend_job(struct jData *);
static int stop_pend_job(struct jData *);
static int resume_pend_job(struct jData *);

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

    return enqueue_header_reply(client->chanfd, BATCH_SBD_REGISTER_ACK);
}

int mbd_new_job_reply(struct mbd_client_node *client,
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

    if (hdr->operation != BATCH_NEW_JOB_REPLY) {
        LS_ERR("BATCH_NEW_JOB_REPLY failed on host %s, reply_code=%s",
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

    LS_INFO("mbd job=%ld operation %s from %s", jobReply.jobId,
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

    if (mbd_send_event_ack(client, BATCH_NEW_JOB_REPLY_ACK, &ack) < 0) {
        LS_ERR("mbd failed enqueue job %ld operation %s from %s",
               ack.job_id, mbd_op_str(hdr->operation), host_name);
    }

    LS_INFO("mbd job=%ld op=%s acked",
            ack.job_id, mbd_op_str(hdr->operation));

    return 0;
}

int mbd_set_status_execute(struct mbd_client_node *client, XDR *xdrs,
                           struct packet_header *hdr)
{
    //  decode payload
    struct statusReq status_req;
    if (!xdr_statusReq(xdrs, &status_req, hdr)) {
        LS_ERR("xdr_statusReq failed");
        return -1;
    }

    struct jData *job = getJobData(status_req.jobId);
    if (job == NULL) {
        return enqueue_header_reply(client->chanfd, LSBE_NO_JOB);
    }

    const char *host_name = client->host_node->host;

    LS_INFO("job %s EXECUTE from %s (state=0x%x)",
            lsb_jobid2str(job->jobId), host_name, job->jStatus);

    // Handle duplicate job execute messages in theory should
    // not happened
    if (job->execute_time > 0) {
        LS_INFO("job %s EXECUTE duplicate from %s, first message at %s",
                lsb_jobid2str(job->jobId), client->host_node->host,
                ctime2(&job->execute_time));
        assert(0);
        return LSBE_NO_ERROR;
    }

    int st = MASK_STATUS(job->jStatus & ~JOB_STAT_UNKWN);
    assert(st == JOB_STAT_RUN || st == JOB_STAT_USUSP || st == JOB_STAT_SSUSP);

    // Copy the execute cwd and home
    if (status_req.execCwd && status_req.execCwd[0] != 0) {
        free(job->execCwd);
        job->execCwd = strdup(status_req.execCwd);
        // Bug handle return coherently with other places and correctly
        // reply hosed memory
    }
    if (status_req.execHome && status_req.execHome[0] != 0) {
        free(job->execHome);
        job->execHome = strdup(status_req.execHome);
    }
    if (status_req.execUsername && status_req.execUsername[0] != 0) {
        free(job->execUsername);
        job->execUsername = strdup(status_req.execUsername);
    }

    // virtual stage: log only, we dont touch the state
    log_executejob(job);
    job->execute_time = time(NULL);

    struct job_status_ack ack;
    memset(&ack, 0, sizeof(struct job_status_ack));
    ack.job_id = job->jobId;
    ack.acked_op = hdr->operation;

    if (mbd_send_event_ack(client, BATCH_JOB_EXECUTE_ACK, &ack) < 0) {
        LS_ERR("mbd failed enqueue ack job %ld operation %s from %s",
               ack.job_id, mbd_op_str(hdr->operation), host_name);
    }

    LS_INFO("mbd job=%ld op=%s rc=%s acked", ack.job_id,
            mbd_op_str(hdr->operation));

    return LSBE_NO_ERROR;
}

// form sbd
int
mbd_set_status_finish(struct mbd_client_node *client, XDR *xdrs,
                      struct packet_header *hdr)
{
    struct statusReq status_req;
    if (!xdr_statusReq(xdrs, &status_req, hdr)) {
        LS_ERR("xdr_statusReq failed");
        return -1;
    }

    struct jData *job = getJobData(status_req.jobId);
    if (job == NULL)
        return enqueue_header_reply(client->chanfd, LSBE_NO_JOB);

    const char *host_name = client->host_node->host;
    int current_status = job->jStatus;

    LS_INFO("job %s FINISH from %s current=0x%x incoming=0x%x exit=%d",
            lsb_jobid2str(job->jobId),
            host_name,
            current_status,
            status_req.newStatus,
            status_req.exitStatus);

    // Duplicate FINISH: normal (resend/replay). ACK always.
    if (job->endTime > 0
        || (current_status & (JOB_STAT_DONE | JOB_STAT_EXIT)) != 0) {
        LS_INFO("job %s FINISH duplicate ignored (state=0x%x endTime=%s)",
                lsb_jobid2str(job->jobId),
                current_status,
                (char *)ctime2(&job->endTime));
        goto send_ack;
    }

    // FINISH from SBD must be for a started job (RUN/USUSP/SSUSP).
    assert((current_status
            & (JOB_STAT_RUN | JOB_STAT_USUSP | JOB_STAT_SSUSP)) != 0);

    // FINISH must be DONE or EXIT (terminal bit must be present).
    assert((status_req.newStatus & (JOB_STAT_DONE | JOB_STAT_EXIT)) != 0);

    // Commit terminal metadata.
    job->endTime = time(NULL);

    job->exitStatus = status_req.exitStatus;
    job->newReason = EXIT_NORMAL;
    job->subreasons = 0;

    // Clear transient flags.
    job->jStatus &= ~JOB_STAT_MIG;
    job->jStatus &= ~JOB_STAT_UNKWN;

    job->pendEvent.sig1 = SIG_NULL;
    job->pendEvent.sig = SIG_NULL;
    job->pendEvent.notSwitched = FALSE;
    job->pendEvent.notModified = FALSE;

    // Commit terminal state: clear base state bits then set DONE/EXIT.
    job->jStatus &= ~(JOB_STAT_PEND | JOB_STAT_PSUSP | JOB_STAT_RUN |
                      JOB_STAT_SSUSP | JOB_STAT_USUSP | JOB_STAT_EXIT |
                      JOB_STAT_DONE);

    if (status_req.newStatus & JOB_STAT_DONE)
        job->jStatus |= JOB_STAT_DONE;
    else
        job->jStatus |= JOB_STAT_EXIT;

    // Move job to finished list: SJL -> FJL.
    offJobList(job, SJL);
    inList((struct listEntry *)jDataList[FJL]->forw,
           (struct listEntry *)job);

    // Log terminal status change.
    log_newstatus(job);

    updCounters(job, current_status, job->endTime);

    LS_INFO("job %s FINISH commit %s (current=0x%x new=0x%x) from %s",
            lsb_jobid2str(job->jobId),
            (job->jStatus & JOB_STAT_DONE) ? "DONE" : "EXIT",
            current_status,
            job->jStatus,
            host_name);

send_ack:
        struct job_status_ack ack;

        memset(&ack, 0, sizeof(ack));
        ack.job_id = job->jobId;
        ack.seq = hdr->sequence;
        ack.acked_op = hdr->operation;

        if (mbd_send_event_ack(client, BATCH_JOB_FINISH_ACK, &ack) < 0) {
            LS_ERR("mbd failed enqueue FINISH ack job %ld op %s from %s",
                   ack.job_id, mbd_op_str(hdr->operation), host_name);
            return -1;
        }

        LS_INFO("mbd FINISH job=%ld op=%s acked",
                ack.job_id, mbd_op_str(hdr->operation));

        return LSBE_NO_ERROR;
}


int
mbd_set_rusage_update(struct mbd_client_node *client,
                      XDR *xdrs, struct packet_header *hdr)
{
    struct statusReq status_req;
    if (!xdr_statusReq(xdrs, &status_req, hdr)) {
        LS_ERR("xdr_statusReq failed");
        return -1;
    }

    struct jData *job = getJobData(status_req.jobId);
    if (job == NULL)
        return enqueue_header_reply(client->chanfd, LSBE_NO_JOB);

    LS_DEBUG("job %s RUSAGE from %s", lsb_jobid2str(job->jobId),
             client->host_node->host);

    // No ack for rusage
    return LSBE_NO_ERROR;
}


int mbd_sbd_disconnect(struct mbd_client_node *client)
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
        LS_ERR("mbd_handle_sbd_disconnect called with NULL host_node (chanfd=%d)",
               client->chanfd);
        abort();
    }

    if (host_node->sbd_node != client) {
        LS_ERR("mbd_handle_sbd_disconnect invariant violated: "
               "host->sbd_node (%p) != client (%p) for host %s",
               (void *)host_node->sbd_node,
               (void *)client,
               host_node->host);
        abort();
    }
    LS_INFO("closing connection with host %s", host_node->host);

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

int mbd_slave_restart(struct mbd_client_node *client,
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

int mbd_send_event_ack(struct mbd_client_node *client,
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

// this is a batch library request not inter daemon, the top signaling handler
// every hadler is responsible for sending back the reply to the library
int mbd_handle_signal_req(XDR *xdrs,
                          struct mbd_client_node *client,
                          struct packet_header *hdr,
                          struct lsfAuth *auth)
{
    struct signalReq signal_req;

    if (!xdr_signalReq(xdrs, &signal_req, hdr))
        return enqueue_header_reply(client->chanfd, LSBE_XDR);

    // Special jobId for all jobs belonging to the caller
    // of bkill
    if (signal_req.jobId == 0) {
        LS_DEBUG("all signal=%d job=%s user=%d/%s", signal_req.sigValue,
                 lsb_jobid2str(signal_req.jobId), auth->uid, auth->lsfUserName);
        // Bug We do this later
        mbd_signal_all_jobs(client->chanfd, &signal_req, auth);
        if (enqueue_header_reply(client->chanfd, LSBE_NO_ERROR) < 0)
            return -1;
        return 0;
    }

    struct jData *job = getJobData(signal_req.jobId);
    if (!job) {
        LS_INFO("job %s unknown to mbd", lsb_jobid2str(signal_req.jobId));
        if (enqueue_header_reply(client->chanfd, LSBE_NO_JOB) < 0)
            return -1;
        return 0;
    }

    LS_DEBUG("signal=%d job=%s user=%d/%s", signal_req.sigValue,
             lsb_jobid2str(signal_req.jobId), job->userId, job->userName);

    if (auth->uid != 0
        && auth->uid != mbd_mgr->uid
        && auth->uid != job->userId) {
        if (enqueue_header_reply(client->chanfd, LSBE_PERMISSION) < 0)
            return -1;
        return 0;
    }

    // State/signal semantic gates
    // Job is already done
    if ((job->jStatus & JOB_STAT_DONE) || (job->jStatus & JOB_STAT_EXIT)) {
        if (enqueue_header_reply(client->chanfd, LSBE_JOB_FINISH) < 0)
            return -1;
        return 0;
    }

    // Signal is STOP but the job is already stopped
    if ((signal_req.sigValue == SIGSTOP || signal_req.sigValue == SIGTSTP)
        && ((job->jStatus & JOB_STAT_USUSP)
            || (job->jStatus & JOB_STAT_SSUSP)
            || (job->jStatus & JOB_STAT_PSUSP))) {
        LS_DEBUG("job=%s state %s suspended already", lsb_jobid2str(job->jobId),
                 job_state_str(job->jStatus));
        if (enqueue_header_reply(client->chanfd, LSBE_JOB_SUSP) < 0)
            return -1;
        return 0;

    }
    // Signal is CONT but the job is PEND or RUN, nothing to continue
    if (signal_req.sigValue == SIGCONT
        && ((job->jStatus & JOB_STAT_PEND) || (job->jStatus & JOB_STAT_RUN))) {
        // SIGCONT on PEND or RUN is a semantic no-op:
        // - do not emit signaljob/newstatus noise
        // - return success to the library
        LS_DEBUG("job=%s state %s: SIGCONT no-op",
                 lsb_jobid2str(job->jobId),
                 job_state_str(job->jStatus));
        if (enqueue_header_reply(client->chanfd, LSBE_NO_ERROR) < 0)
            return -1;
        return 0;
    }

    int cc;
    if ((job->jStatus & JOB_STAT_PEND) || (job->jStatus & JOB_STAT_PSUSP)) {
        // MUST be pending list: do not guess
        cc = mbd_signal_pending_job(job, &signal_req, auth);
        if (enqueue_header_reply(client->chanfd, cc) < 0)
            return -1;
        return 0;
    }

    cc = mbd_signal_running_job(job, &signal_req, auth);
    if (enqueue_header_reply(client->chanfd, cc) < 0)
        return -1;

    return 0;
}

int mbd_signal_pending_job(struct jData *job, struct signalReq *signal_req,
                           struct lsfAuth *auth)
{
    switch (signal_req->sigValue) {
    case SIGTERM:
    case SIGINT:
    case SIGKILL:
        log_signaljob(job, signal_req, auth->uid, auth->lsfUserName);
        finish_pend_job(job);
        break;
    case SIGSTOP:
    case SIGTSTP:
        log_signaljob(job, signal_req, auth->uid, auth->lsfUserName);
        return stop_pend_job(job);
    case SIGCONT:
        log_signaljob(job, signal_req, auth->uid, auth->lsfUserName);
        return resume_pend_job(job);
    default:
        LS_DEBUG("job=%s signal=%d unsupported",
                 lsb_jobid2str(job->jobId), signal_req->sigValue);
        return LSBE_BAD_SIGNAL;
    }

    return LSBE_NO_ERROR;
}

static int finish_pend_job(struct jData *job)
{
    LS_INFO("job %s transition from status 0%x0 to status 0%x0",
            lsb_jobid2str(job->jobId), job->jStatus, JOB_STAT_EXIT);

    job->endTime = time(NULL);
    job->numReasons = job->newReason = job->subreasons = 0;
    SET_STATE(job->jStatus, JOB_STAT_EXIT);

    offJobList(job, PJL);
    inList((struct listEntry *)jDataList[FJL]->forw,
           (struct listEntry *)job);

    log_newstatus(job);

    return LSBE_NO_ERROR;
}

static int stop_pend_job(struct jData *job)
{
    if (job->jStatus & JOB_STAT_PSUSP)
        return LSBE_NO_ERROR; // is alread JOB_STAT_PSUSP no opp

    SET_STATE(job->jStatus, JOB_STAT_PSUSP);
    job->newReason = PEND_USER_STOP;
    job->subreasons = 0;
    job->numReasons = 1;
    LS_INFO("job %s new state JOB_STAT_PSUSP", lsb_jobid2str(job->jobId));

    log_newstatus(job);

    return LSBE_NO_ERROR;
}

static int resume_pend_job(struct jData *job)
{
    if (!(job->jStatus & JOB_STAT_PSUSP))
        return 0;   // no-op for plain PEND

    SET_STATE(job->jStatus, JOB_STAT_PEND);

    LS_INFO("job %s state %s ", lsb_jobid2str(job->jobId),
            job_state_str(job->jStatus));

    job->numReasons = job->newReason = job->subreasons = 0;
    log_newstatus(job);

    return 0;
}

int mbd_signal_running_job(struct jData *job, struct signalReq *req,
                           struct lsfAuth *auth)
{
    struct hData *host = job->hPtr[0];
    if (!host) {
        assert(0);
        return LSBE_BAD_HOST;
    }
    if (!host->sbd_node) {
        errno = EAGAIN;
        LS_ERR("sbd on node %s is disconnected", host->host);
        return LSBE_SBD_UNREACH;
    }

    // channel to connected sbd
    int chan = host->sbd_node->chanfd;
    if (chan < 0) {
        assert(0);
        return LSBE_SBD_UNREACH;
    }

    /*
     * Minimal in-memory bookkeeping: remember a signal is pending.
     * (No retry engine yet; reply handler will clear it.)
     */
    job->pendEvent.sig = req->sigValue;

    /*
     * Build and enqueue the request to sbatchd.
     */
    struct wire_job_sig_req wreq;
    memset(&wreq, 0, sizeof(wreq));
    wreq.job_id = job->jobId;
    wreq.sig = req->sigValue;
    wreq.flags = 0;

    LS_INFO("signal request job=%s user=%s sig=%d",
            lsb_jobid2str(job->jobId),
            auth->lsfUserName,
            req->sigValue);

    // This matches the model intent log first.
    log_signaljob(job, req, auth->uid, auth->lsfUserName);

    // This enqueue goes to sbd
    if (enqueue_payload(chan,
                        BATCH_JOB_SIGNAL,
                        &wreq,
                        xdr_wire_job_sig_req) < 0) {
        LS_ERR("enqueue BATCH_JOB_SIGNAL failed job=%s sig=%d",
               lsb_jobid2str(job->jobId), req->sigValue);
        return LSBE_PROTOCOL;
    }

    // Enable chan_epoll to send out the message
    chan_set_write_interest(chan, true);

    return LSBE_NO_ERROR;
}

int mbd_job_signal_reply(struct mbd_client_node *client, XDR *xdrs,
                         struct packet_header *sbd_hdr)
{
    struct wire_job_sig_reply rep;
    memset(&rep, 0, sizeof(rep));

    if (!xdr_wire_job_sig_reply(xdrs, &rep)) {
        LS_ERR("decode BATCH_JOB_SIGNAL_REPLY failed");
        lserrno = LSBE_XDR;
        return -1;
    }

    struct jData *job = getJobData(rep.job_id);
    if (!job) {
        LS_ERRX("signal reply for unknown job_id=%ld sig=%d rc=%d errno=%d",
                rep.job_id, rep.sig, rep.rc, rep.detail_errno);
        return 0;
    }

    LS_INFO("processing signal ack job_id=%ld jStatus=%s sig=%d rc=%d errno=%d",
            rep.job_id, job_state_str(job->jStatus),
            rep.sig, rep.rc, rep.detail_errno);

    if (rep.rc != LSBE_NO_ERROR) {
        // what is the status of the job?
        LS_ERR("failed to deliver signal to job_id=%ld sig=%d rc=%d errno=%d",
               rep.job_id, rep.sig, rep.rc, rep.detail_errno);
        return -1;
    }

    if (IS_FINISH(job->jStatus)) {
        LS_INFO("job=%s has finished already status=%s",
                lsb_jobid2str(job->jobId), job_state_str(job->jStatus));
        return 0;
    }

    if (IS_PEND(job->jStatus)) {
        LS_INFO("job=%s now in status=%s", lsb_jobid2str(job->jobId),
                job_state_str(job->jStatus));
        assert(0);
        return 0;
    }

    int current_status = job->jStatus;
    switch (rep.sig) {
        // We send TSTP as default for sigstop so the application can
        // catch it it wants
    case SIGSTOP:
    case SIGTSTP:
        if (IS_SUSP(job->jStatus)) {
            LS_INFO("job %s signal SIGSTOP duplicate rejected (already USUSP)",
                    lsb_jobid2str(job->jobId));
            return 0;
        }
        job->newReason |= SUSP_USER_STOP;
        SET_STATE(job->jStatus, JOB_STAT_USUSP);
        log_newstatus(job);
        updCounters(job, current_status, time(NULL));
        return 0;

    case SIGCONT:
        if (! IS_SUSP(job->jStatus)) {
            LS_INFO("job %s signal SIGCONT duplicate rejected (not suspended)",
                    lsb_jobid2str(job->jobId));
            return 0;
        }
        job->newReason &= ~SUSP_USER_STOP;
        SET_STATE(job->jStatus, JOB_STAT_RUN);
        log_newstatus(job);
        updCounters(job, current_status, time(NULL));
        return 0;

    default:
        // TERM/KILL handled by finish path.
        break;
    }

    return 0;
}

int mbd_signal_all_jobs(int ch_id, struct signalReq *req, struct lsfAuth *auth)
{
    if (!req)
        return LSBE_BAD_ARG;

    LS_DEBUG("signal jobs for uid %d/%s sig=%d",
             auth->uid, auth->lsfUserName, req->sigValue);

    struct ll_hash *ht = ll_hash_create(101);
    if (!ht)
        return LSBE_NO_MEM;

    int  matched = 0;
    int lists[] = {PJL, SJL};
    int nl = (int)(sizeof(lists) / sizeof(lists[0]));
    for (int i = 0; i < nl; i++) {
        int list = lists[i];
        struct jData *job;
        struct jData *next;

        for (job = jDataList[list]->back; job != jDataList[list]; job = next) {
            next = job->back;

            // permission filter
            if (auth && auth->uid != 0 && auth->uid != mbd_mgr->uid) {
                if (auth->uid != job->userId)
                    continue;
            }
            // at least one jonb belonging the uid found
            ++matched;

            // Skip finished
            int st = MASK_STATUS(job->jStatus);
            if (st == JOB_STAT_DONE || st == JOB_STAT_EXIT) {
                LS_ERRX("job %s in state 0x%x should not be in list %d",
                        lsb_jobid2str(job->jobId), st, list);
                assert(0);
                continue;
            }

            // PEND/PSUSP: old per-job handling
            if (st == JOB_STAT_PEND || st == JOB_STAT_PSUSP) {
                int64_t save = req->jobId;
                req->jobId = job->jobId;
                // Signal a pending job
                int cc = mbd_signal_pending_job(job, req, auth);
                if (cc != LSBE_NO_ERROR) {
                    LS_ERR("failed to signal pending job %s",
                           lsb_jobid2str(job->jobId));
                }
                req->jobId = save;
                continue;
            }

            // RUN/USUSP/etc: bucket by host
            struct hData *host = NULL;
            if (job->hPtr)
                host = job->hPtr[0];

            assert(host != NULL && IS_START(st));

            if (!host) {
                LS_ERR("bkill 0: job %s has no host, skip",
                       lsb_jobid2str(job->jobId));
                continue;
            }

            if (!host->sbd_node || host->sbd_node->chanfd < 0) {
                LS_ERR("bkill 0: job %s host %s sbd unreachable, skip",
                       lsb_jobid2str(job->jobId), host->host);
                continue;
            }

            struct sig_host_bucket *b = ll_hash_search(ht, host->host);
            if (!b) {
                b = calloc(1, sizeof(*b));
                if (!b) {
                    free_sig_bucket_table(ht);
                    return LSBE_NO_MEM;
                }
                b->host = host;

                enum ll_hash_status rc = ll_hash_insert(ht, host->host, b, 0);
                if (rc != LL_HASH_INSERTED) {
                    free(b);
                    free_sig_bucket_table(ht);
                    return LSBE_NO_MEM;
                }
            }

            if (bucket_add_jobid(b, job->jobId) < 0) {
                free_sig_bucket_table(ht);
                return LSBE_NO_MEM;
            }

            log_signaljob(job, req, auth->uid, auth->lsfUserName);
        }
    }

    // BUG(MVP): bkill 0 is a bulk operation but the API returns a single
    // lsberrno.
    // We currently return LSBE_NO_ERROR as long as we matched at least one job,
    // even if some jobs could not be signalled (no host, SBD unreachable,
    // race: job finishes while we signal, etc.).
    // For MVP the operator must run bjobs to verify results.
    // Later we must record per-job / per-host outcomes and expose
    // them via bkill -l and/or export to external monitoring.

    if (matched == 0) {
        free_sig_bucket_table(ht);
        return LSBE_NO_JOB;
    }

    if (ht->nentries > 0)
        enqueue_sig_buckets(ht, req->sigValue);

    free_sig_bucket_table(ht);
    // By design now returns LSBE_NO_ERROR as we cannot easily
    // match, without some assumptions, the potentially many
    // return codes to just one return number
    return LSBE_NO_ERROR;
}
// Make the batch system manager
// One user, fixed identity, zero UID gymnastics
struct mbd_manager *mbd_init_manager(void)
{
    mbd_mgr = calloc(1, sizeof(struct mbd_manager));
    mbd_mgr->uid = getuid();
    mbd_mgr->gid = getgid();

    struct passwd *pw = getpwuid2(mbd_mgr->uid);
    if (!pw || !pw->pw_name) {
        syslog(LOG_ERR, "%s: getpwuid2(%d) failed: %m", __func__, mbd_mgr->uid);
        free(mbd_mgr);
        return NULL;
    }

    // for now we just keep one manager
    mbd_mgr->name = strdup(pw->pw_name);

    // Optional badge log for audit clarity
    syslog(LOG_INFO, "%s initialized: uid %d, gid %d, name %s", __func__,
           mbd_mgr->uid, mbd_mgr->gid, mbd_mgr->name);

    return mbd_mgr;
}

int mbd_init_networking(void)
{
    long open_max = sysconf(_SC_OPEN_MAX);

    mbd_chan = chan_listen_socket(SOCK_STREAM,
                                  mbd_port,
                                  SOMAXCONN,
                                  CHAN_OP_SOREUSE);
    if (mbd_chan < 0) {
        LS_ERR("chan_listen_socket() failed");
        return -1;
    }

    mbd_efd = epoll_create1(EPOLL_CLOEXEC);
    if (mbd_efd < 0) {
        LS_ERR("epoll_create1() failed");
        chan_close(mbd_chan);
        return -1;
    }

    // Set the MBD listening channel into the event structure
    struct epoll_event ev = {.events = EPOLLIN, .data.u32 = mbd_chan};

    int cc = epoll_ctl(mbd_efd, EPOLL_CTL_ADD, chan_sock(mbd_chan), &ev);
    if (cc < 0) {
        LS_ERR("epoll_ctl() failed");
        chan_close(mbd_chan);
        close(mbd_efd);
        return -1;
    }

    mbd_max_events = (int)open_max;
    mbd_events = calloc(mbd_max_events, sizeof(struct epoll_event));
    if (mbd_events == NULL) {
        LS_ERR("calloc num_events %d failed", mbd_max_events);
        chan_close(mbd_chan);
        close(mbd_efd);
        return -1;
    }

    return 0;
}

int mbd_init_tables(void)
{
    if (ll_hash_init(&hdata_by_chan, 101) < 0) {
        LS_ERR("failed to init hdata_by_chan");
        return -1;
    }

    return 0;
}

// ---------- The space of static functions


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

static int enqueue_sig_buckets(struct ll_hash *ht, int32_t sig)
{
    int queued_hosts_ok = 0;

    for (size_t i = 0; i < ht->nbuckets; i++) {
        struct ll_hash_entry *e = ht->buckets[i];

        while (e) {
            struct sig_host_bucket *b = e->value;
            assert(b);

            if (b->n > 0) {
                if (signal_sbd_jobs(b, sig, 0) == 0)
                    ++queued_hosts_ok;
            }

            e = e->next;
        }
    }

    if (queued_hosts_ok == 0)
        return LSBE_SBD_UNREACH;

    return LSBE_NO_ERROR;
}

#define SIG_MANY_CHUNK 2048
static int signal_sbd_jobs(struct sig_host_bucket *b, int32_t sig, int32_t flags)
{
    uint32_t off = 0;
    int chan = b->host->sbd_node->chanfd;

    while (off < b->n) {
        uint32_t n = b->n - off;
        if (n > SIG_MANY_CHUNK)
            n = SIG_MANY_CHUNK;

        struct xdr_sig_sbd_jobs sj = {
            .sig = sig,
            .flags = flags,
            .n = n,
            .job_ids = &b->job_ids[off],
        };

        if (enqueue_payload_bufsiz(chan,
                                   BATCH_JOB_SIGNAL_MANY,
                                   &sj,
                                   xdr_sig_sbd_jobs,
                                   LL_BUFSIZ_64K) < 0)
            return -1;

        off += n;
    }

    chan_set_write_interest(chan, true);
    return 0;
}

static int bucket_add_jobid(struct sig_host_bucket *b, int64_t job_id)
{
    if (b->n == b->cap) {
        uint32_t newcap = b->cap ? b->cap * 2 : 32;
        int64_t *p = realloc(b->job_ids, (size_t)newcap * sizeof(*p));
        if (!p)
            return -1;
        b->job_ids = p;
        b->cap = newcap;
    }

    b->job_ids[b->n++] = job_id;
    return 0;
}

static void free_sig_bucket_table(struct ll_hash *ht)
{
    if (!ht)
        return;

    for (size_t i = 0; i < ht->nbuckets; i++) {
        struct ll_hash_entry *e = ht->buckets[i];

        while (e) {
            struct ll_hash_entry *next = e->next;
            struct sig_host_bucket *b = e->value;

            if (b) {
                free(b->job_ids);
                free(b);
            }

            e = next;
        }
    }

    ll_hash_free(ht, NULL, NULL);
}
