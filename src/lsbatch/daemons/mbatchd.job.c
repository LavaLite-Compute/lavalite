/*
 * Copyright (C) LavaLite Contributors
 * GPL v2
 */

#include "lsbatch/daemons/mbd.h"
#include "lsbatch/daemons/mbatchd.h"

static int finish_pend_job(struct jData *);
static int stop_pend_job(struct jData *);
static int resume_pend_job(struct jData *);
static void requeue_start_failed(struct jData *, struct hData *);
static void clear_exec_context(struct jData *);
static void build_sbd_run_list(struct hData *, struct wire_sbd_register *);
static void purge_finished_job(struct jData *);
static void purge_job_info_file(struct jData *);
static void free_job_data(struct jData *);

// LavaLite
// this is still a client-like request coming through the client handler
// after this call the connection becomes a permanent sbd connection.
int mbd_sbd_register(XDR *xdrs, struct mbd_client_node *client,
                     struct packet_header *hdr)
{
    (void)hdr;

    struct wire_sbd_register reg;
    memset(&reg, 0, sizeof(struct wire_sbd_register));

    if (!xdr_wire_sbd_register(xdrs, &reg)) {
        LS_ERR("SBD_REGISTER decode failed");
        enqueue_header_reply(client->chanfd, LSBE_XDR);
        free(reg.jobs);
        return -1;
    }

    char hostname[MAXHOSTNAMELEN];
    memcpy(hostname, reg.hostname, sizeof(hostname));
    hostname[sizeof(hostname) - 1] = 0;

    struct hData *host_data = getHostData(hostname);
    if (host_data == NULL) {
        LS_ERR("SBD_REGISTER from unknown host %s", hostname);
        free(reg.jobs);
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

    // build an authoritative RUN list for that host, including whether
    // mbd already has the pid for each run job
    struct wire_sbd_register reg_ack;
    memset(&reg_ack, 0, sizeof(struct wire_sbd_register));
    build_sbd_run_list(host_data, &reg_ack);

    // good bye bits
    host_data->hStatus = HOST_STAT_OK;

    LS_INFO("hostname=%s canon=%s addr=%s chan_fd=%d status=%s",
            hostname, host_data->sbd_node->host.name,
            host_data->sbd_node->host.addr,
            host_data->sbd_node->chanfd, hstat_to_str(host_data->hStatus));

    enqueue_payload(client->chanfd,
                    BATCH_SBD_REGISTER_ACK,
                    &reg_ack,
                    xdr_wire_sbd_register);

    chan_set_write_interest(client->chanfd, true);
    free(reg.jobs);

    return 0;
}
/*
 * Protocol rule: after we successfully decode a reply that includes jobId,
 * we must always enqueue the matching *_ACK to stop SBD retries.
 *
 * ACK is delivery control, not state approval: duplicates/late/out-of-state
 * replies are ACKed and then ignored. If decode fails, we cannot ACK because
 * jobId is unknown (until jobId/req_id is carried in the header).
 */

int mbd_new_job_reply(struct mbd_client_node *client,
                      XDR *xdrs,
                      struct packet_header *hdr)
{
    struct jobReply jobReply;

    struct hData *host_node = client->host_node;
    if (host_node == NULL) {
        LS_ERR("operation=%s with NULL host_node (chanfd=%d)",
               mbd_op_str(hdr->operation), client->chanfd);
        // Hard invariant violation: this should never happen.
        abort();
    }

    const char *host_name = host_node->host;

    if (hdr->operation != BATCH_NEW_JOB_REPLY) {
        LS_ERR("operation=%s failed on host %s", mbd_op_str(hdr->operation),
               host_name);
        return -1;
    }

    memset(&jobReply, 0, sizeof(jobReply));
    if (!xdr_jobReply(xdrs, &jobReply, hdr)) {
        LS_ERR("operation=%s xdr_jobReply decode failed from host %s",
               mbd_op_str(hdr->operation), host_name);
        //jStatusChange(jData, JOB_STAT_PEND, LOG_IT, fname);
        // maybe close the connection with sbd?
        return -1;
    }

    // save the jobid we are acking and operating on
    int64_t ack_job_id = jobReply.jobId;

    // Map jobId -> job descriptor.
    struct jData *job;
    job = getJobData((int64_t)jobReply.jobId);
    if (job == NULL) {
        LS_ERR("operation %s reply for unknown jobId=%ld from host %s",
               mbd_op_str(hdr->operation), ack_job_id, host_name);
        goto send_ack;
    }

    // Ignore replies for jobs that are no longer in START state.
    if (!IS_START(job->jStatus)) {
        goto send_ack;
    }

    if (jobReply.jStatus == JOB_STAT_PEND) {
        requeue_start_failed(job, host_node);
        goto send_ack;
    }

    if (jobReply.jStatus == JOB_STAT_RUN) {
        job->jobPid  = jobReply.jobPid;
        job->jobPGid = jobReply.jobPGid;
        job->jStatus = JOB_STAT_RUN;
        log_startjobaccept(job);
        goto send_ack;
    }

    /*
     * Acknowledge back to sbatchd so it can unblock.
     * We only send a small packet_header over the LavaLite channel.
     * No more blocking send path, everything is queued via chan_enqueue().
     */
send_ack:
    struct job_status_ack ack;
    memset(&ack, 0, sizeof(struct job_status_ack));
    ack.job_id = ack_job_id;  // from decoded payload
    ack.acked_op = hdr->operation; // the op being acknowledged

    if (mbd_send_event_ack(client, BATCH_NEW_JOB_REPLY_ACK, &ack) < 0) {
        LS_ERR("mbd failed enqueue job %ld operation %s from %s",
               ack.job_id, mbd_op_str(hdr->operation), host_name);
    }

    LS_INFO("mbd job=%ld op=%s acked", ack.job_id, mbd_op_str(hdr->operation));

    return 0;
}

int mbd_set_status_execute(struct mbd_client_node *client, XDR *xdrs,
                           struct packet_header *hdr)
{
    //  decode payload
    struct statusReq status_req;
    memset(&status_req, 0, sizeof(status_req));
    if (!xdr_statusReq(xdrs, &status_req, hdr)) {
        LS_ERR("xdr_statusReq failed");
        return -1;
    }

    struct hData *host_node = client->host_node;
    const char *host_name = host_node->host;

    LS_INFO("operation=%s job=%ld from=%s (state=0x%x)",
            mbd_op_str(hdr->operation), status_req.jobId,
            host_name, status_req.newStatus);

    struct jData *job = getJobData(status_req.jobId);
    if (job == NULL) {
        LS_ERR("operation=%s reply for unknown jobId=%ld from host=%s",
               mbd_op_str(hdr->operation), status_req.jobId, host_name);
        goto send_ack;
    }

    // Handle duplicate job execute messages in theory should
    // not happened
    if (job->execute_time > 0) {
        LS_INFO("job %s EXECUTE duplicate from %s, first message at %s",
                lsb_jobid2str(job->jobId), client->host_node->host,
                ctime2(&job->execute_time));
        // send ack regadless
        goto send_ack;
    }

    int st = MASK_STATUS(job->jStatus & ~JOB_STAT_UNKNOWN);
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

send_ack:
    struct job_status_ack ack;
    memset(&ack, 0, sizeof(struct job_status_ack));
    ack.job_id = status_req.jobId;
    ack.acked_op = hdr->operation;

    if (mbd_send_event_ack(client, BATCH_JOB_EXECUTE_ACK, &ack) < 0) {
        LS_ERR("mbd failed enqueue ack job %ld operation %s from %s",
               ack.job_id, mbd_op_str(hdr->operation), host_name);
    }

    LS_INFO("operation=%s job=%ld acked", mbd_op_str(hdr->operation),
            ack.job_id);

    xdr_lsffree(xdr_statusReq, &status_req, hdr);

    return LSBE_NO_ERROR;
}

// form sbd
int mbd_set_status_finish(struct mbd_client_node *client, XDR *xdrs,
                          struct packet_header *hdr)
{
    struct statusReq status_req;

    memset(&status_req, 0, sizeof(struct statusReq));
    if (!xdr_statusReq(xdrs, &status_req, hdr)) {
        LS_ERR("xdr_statusReq failed");
        return -1;
    }

    struct hData *host_node = client->host_node;
    const char *host_name = host_node->host;

    struct jData *job = getJobData(status_req.jobId);
    if (job == NULL) {
        LS_ERR("operation=%s unknown jobId=%ld from host %s",
               mbd_op_str(hdr->operation), status_req.jobId, host_name);
        goto send_ack;
    }

    int current_status = job->jStatus;

    LS_INFO("operation=%s job=%ld from=%s current=0x%x incoming=0x%x exit=%d",
            mbd_op_str(hdr->operation), status_req.jobId, host_name,
            current_status, status_req.newStatus, status_req.exitStatus);

    // Duplicate FINISH: normal (resend/replay). ACK always.
    if (job->endTime > 0
        || (current_status & (JOB_STAT_DONE | JOB_STAT_EXIT)) != 0) {
        LS_INFO("job %s FINISH duplicate ignored (state=0x%x endTime=%s)",
                lsb_jobid2str(status_req.jobId),
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
    job->jStatus &= ~JOB_STAT_UNKNOWN;

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

    LS_INFO("job %ld commit %s (current=0x%x new=0x%x)",
            status_req.jobId,
            (job->jStatus & JOB_STAT_DONE) ? "DONE" : "EXIT",
            current_status, job->jStatus);

send_ack:
    struct job_status_ack ack;

    memset(&ack, 0, sizeof(ack));
    ack.job_id = status_req.jobId;
    ack.seq = hdr->sequence;
    ack.acked_op = hdr->operation;

    if (mbd_send_event_ack(client, BATCH_JOB_FINISH_ACK, &ack) < 0) {
        LS_ERR("mbd failed enqueue FINISH ack job %ld op %s from %s",
                   ack.job_id, mbd_op_str(hdr->operation), host_name);
        return -1;
    }

    LS_INFO("mbd FINISH job=%ld op=%s acked",
            ack.job_id, mbd_op_str(hdr->operation));

    xdr_lsffree(xdr_statusReq, &status_req, hdr);

    return LSBE_NO_ERROR;
}

int
mbd_set_rusage_update(struct mbd_client_node *client,
                      XDR *xdrs, struct packet_header *hdr)
{
    struct statusReq status_req;
    memset(&status_req, 0, sizeof(status_req));

    if (!xdr_statusReq(xdrs, &status_req, hdr)) {
        LS_ERR("xdr_statusReq failed");
        return -1;
    }

    struct jData *job = getJobData(status_req.jobId);
    if (job == NULL)
        return enqueue_header_reply(client->chanfd, LSBE_NO_JOB);

    LS_DEBUG("job %s RUSAGE from %s", lsb_jobid2str(job->jobId),
             client->host_node->host);

    xdr_lsffree(xdr_jobSpecs, &status_req, hdr);
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
        LS_ERRX("mbd_handle_sbd_disconnect called with NULL host_node (chanfd=%d)",
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

    host_node->hStatus = HOST_STAT_UNREACH;
    LS_INFO("closing connection with host=%s addr=%s state=%s", host_node->host,
            host_node->sbd_node->host.addr, hstat_to_str(host_node->hStatus));

    struct jData *job;
    for (job = jDataList[SJL]->back; job != jDataList[SJL]; job = job->back) {

        if (job->hPtr[0] != host_node)
            continue;

         job->jStatus |= JOB_STAT_UNKNOWN;
         job->newReason = PEND_SBD_UNREACH;
         LS_DEBUG("job=%ld set to JOB_STAT_UNKNOW on addr=%s", job->jobId,
                  host_node->sbd_node->host.addr);
         log_newstatus(job);
    }
    // Break the association first, then reset pending jobs.
    client->host_node = NULL;
    // invalidate the pointer as the sbd is down
    host_node->sbd_node = NULL;

    char key[LL_BUFSIZ_32];
    snprintf(key, sizeof(key), "%d", client->chanfd);
    ll_hash_remove(&hdata_by_chan, key);

    // hose the client
    chan_close(client->chanfd);
    free(client);

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

    chan_set_write_interest(client->chanfd, true);

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
    memset(&signal_req, 0, sizeof(struct signalReq));

    if (!xdr_signalReq(xdrs, &signal_req, hdr))
        return enqueue_header_reply(client->chanfd, LSBE_XDR);

    // Special jobId for all jobs belonging to the caller
    // of bkill
    if (signal_req.jobId == 0) {
        LS_DEBUG("all signal=%d job=%s user=%d/%s", signal_req.sigValue,
                 lsb_jobid2str(signal_req.jobId), auth->uid, auth->lsfUserName);

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

    LS_INFO("job=%s state %s ", lsb_jobid2str(job->jobId),
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

    LS_INFO("job_id=%ld jStatus=%s sig=%d rc=%d errno=%d", rep.job_id,
            job_state_str(job->jStatus), rep.sig, rep.rc, rep.detail_errno);

    if (rep.rc == LSBE_NO_JOB) {
        LS_ERRX("failed: SBD reports LSBE_NO_JOB job_id=%ld sig=%d "
                "set JOB_STAT_UNKNOWN reason=PEND_SYS_UNABLE", rep.job_id, rep.sig);
        job->jStatus |= JOB_STAT_UNKNOWN;
        job->newReason = PEND_SYS_UNABLE;
        log_newstatus(job);

        return 0;
    }

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

int mbd_signal_all_jobs(int chan_id,
                        struct signalReq *signal_req,
                        struct lsfAuth *auth)
{
    (void)chan_id;

    LS_DEBUG("signal jobs for uid %d/%s sig=%d",
             auth->uid, auth->lsfUserName, signal_req->sigValue);

    int lists[] = {PJL, SJL};
    int nl = (int)(sizeof(lists) / sizeof(lists[0]));
    for (int i = 0; i < nl; i++) {
        int list = lists[i];
        struct jData *job;
        struct jData *next_job;
        for (job = jDataList[list]->back; job != jDataList[list]; job = next_job) {
            next_job = job->back;

            if (auth && auth->uid != 0 && auth->uid != mbd_mgr->uid) {
                if (auth->uid != job->userId)
                    continue;
            }

            // Skip finished
            int st = job->jStatus;
            if ((st & JOB_STAT_DONE) || (st & JOB_STAT_EXIT)) {
                LS_ERRX("job %s in state 0x%x should not be in list %d",
                        lsb_jobid2str(job->jobId), st, list);
                assert(0);
                continue;
            }

            // PEND/PSUSP: old per-job handling
            if ((st & JOB_STAT_PEND) || (st & JOB_STAT_PSUSP)) {

                struct signalReq tmp;
                memcpy(&tmp, signal_req, sizeof(struct signalReq));
                tmp.jobId = job->jobId;

                // Signal a pending job
                int cc = mbd_signal_pending_job(job, &tmp, auth);
                if (cc != LSBE_NO_ERROR) {
                    LS_ERR("failed to signal pending job %s",
                           lsb_jobid2str(job->jobId));
                }
                continue;
            }

            if (job->jStatus & JOB_STAT_UNKNOWN) {
                LS_INFO("job=%ld status=0x%x", job->jobId, job->jStatus);
                continue;
            }

            // RUN/USUSP/etc: bucket by host
            struct hData *host = NULL;
            if (job->hPtr)
                host = job->hPtr[0];

            assert(host != NULL && !(host->hStatus & HOST_STAT_UNREACH)
                   && ((st & JOB_STAT_RUN) || (st & JOB_STAT_SSUSP)
                       || (st & JOB_STAT_USUSP)));

            if (!host) {
                LS_ERR("bkill 0: job=%ld has no host? skip", job->jobId);
                continue;
            }

            if (!host->sbd_node || host->sbd_node->chanfd < 0) {
                LS_ERR("bkill 0: job=%ld host=%s sbd unreachable status=0x%x",
                       job->jobId, host->host, host->hStatus);
                continue;
            }

            struct signalReq tmp;
            memcpy(&tmp, signal_req, sizeof(struct signalReq));
            tmp.jobId = job->jobId;

            int cc = mbd_signal_running_job(job, &tmp, auth);
            if (cc != LSBE_NO_ERROR) {
                LS_ERRX("failed signal job=%ld error=%s", job->jobId,
                        lsb_sysmsg());
            }
        }
    }

    return LSBE_NO_ERROR;
}

void mbd_job_state_unknown(struct mbd_client_node *client, XDR *xdrs,
                           struct packet_header *sbd_hdr)
{
    struct wire_job_state wjs;
    memset(&wjs, 0, sizeof(struct wire_job_state));

    if (! xdr_wire_job_state(xdrs, &wjs)) {
        LS_ERR("xdr_wire_job_state failed");
        lserrno = LSBE_XDR;
        return;
    }

    struct jData *job = getJobData(wjs.job_id);
    if (!job) {
        // this is basically impossible because we just sent it
        LS_ERRX("job_id=%ld", wjs.job_id);
        return;
    }

    if (job->jobPid > 0) {
        job->jStatus |= JOB_STAT_UNKNOWN;
        job->newReason = PEND_SYS_UNABLE;
        log_newstatus(job);
        LS_ERRX("job=%ld pid=%d is missing set state=%s",
                job->jobId, job->jobPGid, job_state_str(job->jStatus));
        return;
    }

    int old_status = job->jStatus;
    job->jStatus = JOB_STAT_PEND;

    // Reason for requeue
    job->newReason = 0;
    job->subreasons = 0;

    // Move list membership: SJL -> PJL
    offJobList(job, SJL);
    inPendJobList(job, PJL, 0);

    // Bookkeeping
    job->requeueTime = time(NULL);
    log_newstatus(job);
    updCounters(job, old_status, now);

    clear_exec_context(job);

    LS_INFO("job=%ld unknown to sbd goes back to %s", job->jobId,
            job_state_str(job->jStatus));
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
    chan_epoll_register(mbd_efd);
    LS_INFO("chan_epoll_register mbd_efd=%d", mbd_efd);
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

// Requeue a START job back to PEND due to start failure on a host.
// Must be idempotent: duplicates should not double-move lists or double-update counters.
static void requeue_start_failed(struct jData *job, struct hData *host_node)
{
    int old_status;
    time_t now;

    host_node->hStatus |= HOST_STAT_DISABLED;
    host_node->last_disable_time = time(NULL);

    // If already pending, just stamp reason (duplicate/late reply) and return.
    if (IS_PEND(job->jStatus)) {
        job->newReason = PEND_JOB_START_FAIL;
        job->subreasons = 0;
        return;
    }

    old_status = job->jStatus;
    now = time(NULL);

    // Roll base status back to PEND (clear start/run/finish bits you don't want to keep)
    job->jStatus &= ~(JOB_STAT_RUN | JOB_STAT_SSUSP | JOB_STAT_USUSP |
                      JOB_STAT_PSUSP | JOB_STAT_DONE | JOB_STAT_EXIT);
    job->jStatus = JOB_STAT_PEND;

    // Reason for requeue
    job->newReason = PEND_JOB_START_FAIL;
    job->subreasons = 0;

    // Move list membership: SJL -> PJL
    offJobList(job, SJL);
    inPendJobList(job, PJL, 0);

    // Bookkeeping
    job->requeueTime = now;
    log_newstatus(job);
    updCounters(job, old_status, now);

    clear_exec_context(job);
}

// Bug
extern void cleanCandHosts(struct jData *job);
// Clear per-dispatch / per-attempt execution context.
// This must be safe to call even if the job is already partially cleared.
static void clear_exec_context(struct jData *job)
{
    // Runtime identity / sequencing
    job->jobPid = 0;
    job->jobPGid = 0;
    job->nextSeq = 1;

    // Execution metadata learned during the run attempt
    FREEUP(job->execCwd);
    FREEUP(job->execHome);
    FREEUP(job->execUsername);
    job->execUid = 0;

    // Queue hook expansions / per-dispatch strings
    FREEUP(job->queuePreCmd);
    FREEUP(job->queuePostCmd);

    // Assigned execution hosts for this attempt
    FREEUP(job->hPtr);
    job->numHostPtr = 0;

    FREEUP(job->execHosts);
    FREEUP(job->schedHost);

    // Pending-event / signal bookkeeping should not leak across attempts
    memset(&job->pendEvent, 0, sizeof(job->pendEvent));

    // Scheduler candidate state: clear "ready/processed" and free candidate arrays
    // (cleanCandHosts() is guarded by IS_PEND(job->jStatus) in your current code,
    // so caller should set JOB_STAT_PEND first, or you can remove that guard later.)
    cleanCandHosts(job);

    // Optional: if you decide these are per-attempt only, clear them here.
    // job->actPid = 0;
    // job->sigValue = 0;
    // job->execute_time = 0;
}

// sbd alive again
static void build_sbd_run_list(struct hData *host_node,
                               struct wire_sbd_register *reg)
{
    struct jData *job;


    int num_jobs = 0;
    for (job = jDataList[SJL]->back; job != jDataList[SJL]; job = job->back) {
        if (job->hPtr[0] == host_node)
            ++num_jobs;
    }

    reg->num_jobs = 0;
    reg->jobs = NULL;
    if (num_jobs == 0)
        return;

    reg->jobs = calloc(num_jobs, sizeof(struct wire_sbd_job));
    if (reg->jobs == NULL) {
        LS_ERR("calloc %ld failed", num_jobs * sizeof(struct wire_sbd_job));
        mbdDie(MASTER_FATAL);
    }

    reg->num_jobs = num_jobs;
    int i = 0;
    for (job = jDataList[SJL]->back; job != jDataList[SJL]; job = job->back) {

        if (job->hPtr[0] != host_node)
            continue;

        reg->jobs[i].job_id = job->jobId;
        reg->jobs[i].pid = job->jobPid;
        LS_INFO("job=%ld pid=%d sent to sbd",
                reg->jobs[i].job_id, reg->jobs[i].pid);

        if (job->jStatus & JOB_STAT_UNKNOWN) {
            job->jStatus |= JOB_STAT_RUN;
            job->jStatus &=~ JOB_STAT_UNKNOWN;
            job->newReason = 0;
            log_newstatus(job);
        }

        ++i;
    }
}

void clean_jobs(time_t compact_time)
{
    struct jData *head = jDataList[FJL];
    struct jData *job;
    struct jData *next;
    int removed = 0;

    if (compact_time <= 0)
        return;

    for (job = head->back; job != head; job = next) {
        next = job->back;

        int st = job->jStatus;
        assert((st & (JOB_STAT_DONE | JOB_STAT_EXIT)));
        if ((st & (JOB_STAT_DONE | JOB_STAT_EXIT)) == 0)
            continue;

        if (job->endTime <= 0)
            continue;

        if ((compact_time - job->endTime) <= clean_period)
            continue;

        // Remove from mbd memory (includes removing from FJL)
        purge_finished_job(job);

        removed++;
    }

    LS_INFO("compact clean_jobs removed=%d cutoff=%ld period=%d",
            removed,
            (long)(compact_time - clean_period),
            clean_period);
}

static void purge_finished_job(struct jData *jp)
{
    if (!jp)
        return;

    // only terminal jobs should be here
    if ((jp->jStatus & (JOB_STAT_DONE | JOB_STAT_EXIT)) == 0)
        return;

    // 1) remove from jobId hash
    remvMemb(&jobIdHT, jp->jobId);

    // 2) detach from finished list
    offJobList(jp, FJL);

    // 3) delete info file (best-effort)
    purge_job_info_file(jp);

    // no need to log this event nobody cares

    // 5) free
    free_job_data(jp);
}

static void purge_job_info_file(struct jData *jp)
{
    char job_file[PATH_MAX];
    const char *p = get_info_dir();

    int n = snprintf(job_file, sizeof(job_file), "%s/%s", p,
                     jp->shared->jobBill.jobFile);
    if (n < 0 || n >= (int)sizeof(job_file)) {
        LS_ERR("snprintf failed dir=%s file=%s", p, jp->shared->jobBill.jobFile);
        return;
    }

    if (unlink(job_file) < 0)
        LS_ERR("job=%ld unlink failed file=%s", jp->jobId, job_file);

    return;
}

static void free_job_data(struct jData *job)
{
    if (!job)
        return;

    FREEUP(job->userName);
    FREEUP(job->lsfRusage);
    FREEUP(job->reasonTb);
    FREEUP(job->hPtr);

    FREEUP(job->execHome);
    FREEUP(job->execCwd);
    FREEUP(job->execUsername);
    FREEUP(job->queuePreCmd);
    FREEUP(job->queuePostCmd);
    FREEUP(job->reqHistory);
    FREEUP(job->schedHost);
    if (job->runRusage.npids > 0)
        FREEUP(job->runRusage.pidInfo);
    if (job->runRusage.npgids > 0)
        FREEUP(job->runRusage.pgid);

    if (job->newSub) {
        freeSubmitReq(job->newSub);
        FREEUP(job->newSub);
    }

    freeSubmitReq(&(job->shared->jobBill));
    FREEUP(job->shared);
    FREEUP(job->askedPtr);

    FREEUP(job->execHosts);
    FREEUP(job->candPtr);
    FREEUP(job->jobSpoolDir);

    FREE_ALL_GRPS_CAND(job);
    FREEUP(job);
}
