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

static bool_t mbd_status_reply_means_committed(int);
static void mbd_reset_sbd_job_list(struct hData *);
static int pending_sig_allowed(int);
static int enqueue_sig_buckets(struct ll_hash *, int32_t);
static int signal_sbd_jobs(struct sig_host_bucket *, int32_t, int32_t);
static void free_sig_bucket_table(struct ll_hash *);
static int bucket_add_jobid(struct sig_host_bucket *, int64_t);
static void mbd_jstatus_change_by_signal_reply(struct jData *,
                                               const struct wire_job_sig_reply *);

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

int mbd_job_status(struct mbd_client_node *client, XDR *xdrs,
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
        break;
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
    case  BATCH_NEW_JOB_REPLY:
        return "BATCH_NEW_JOB_REPLY";
    case BATCH_JOB_SIGNAL:
        return "BATCH_JOB_SIGNAL";
    case BATCH_JOB_SIGNAL_REPLY:
        return "BATCH_JOB_SIGNAL_REPLY";

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

int mbd_signal_pending_job(int ch_id, struct jData *job, struct signalReq *req,
                           struct lsfAuth *auth)
{
    if (! pending_sig_allowed(req->sigValue)) {
        return LSBE_BAD_SIGNAL;
    }

    switch (req->sigValue) {
    case SIG_TERM_USER:
    case SIGTERM:
    case SIGINT:
        // This matches the model intent log first.
        log_signaljob(job, req, auth->uid, auth->lsfUserName);
        job->newReason = EXIT_NORMAL;
        jStatusChange(job, JOB_STAT_EXIT, LOG_IT, (char *)__func__);
        return LSBE_NO_ERROR;
        break;
    default:
        LS_INFO("signal %d to job %ld not supported", req->sigValue,
                job->jobId);
        return LSBE_BAD_SIGNAL;
        break;
    }

    return LSBE_NO_ERROR;
}

int mbd_signal_running_job(int ch_id, struct jData *job, struct signalReq *req,
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

// Lavalite
int mbd_signal_job(int ch_id,
                   struct jData *job,
                   struct signalReq *req,
                   struct lsfAuth *auth)
{
    if (req == NULL || job == NULL)
        return LSBE_BAD_ARG;

    LS_DEBUG("signal <%d> job <%s>", req->sigValue, lsb_jobid2str(req->jobId));

    if (auth) {
        if (auth->uid != 0
            && auth->uid != mbd_mgr->uid
            && auth->uid != job->userId) {
            return LSBE_PERMISSION;
        }
    }

    int st = MASK_STATUS(job->jStatus);

    if (st == JOB_STAT_DONE || st == JOB_STAT_EXIT)
        return LSBE_JOB_FINISH;

    if (st == JOB_STAT_PEND || st == JOB_STAT_PSUSP) {
        return mbd_signal_pending_job(ch_id, job, req, auth);
    }

    assert(IS_START(st));

    return mbd_signal_running_job(ch_id, job, req, auth);
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
                int cc = mbd_signal_pending_job(ch_id, job, req, auth);
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

    return LSBE_NO_ERROR;
}


int mbd_job_signal_reply(struct mbd_client_node *client, XDR *xdrs,
                         struct packet_header *sbd_hdr)
{
    struct wire_job_sig_reply rep;
    struct jData *job;

    (void)client;
    (void)sbd_hdr;

    if (!xdrs) {
        lserrno = LSBE_BAD_ARG;
        return -1;
    }

    memset(&rep, 0, sizeof(rep));

    if (!xdr_wire_job_sig_reply(xdrs, &rep)) {
        LS_ERR("decode BATCH_JOB_SIGNAL_REPLY failed");
        lserrno = LSBE_XDR;
        return -1;
    }

    job = getJobData(rep.job_id);
    if (!job) {
        LS_INFO("signal reply for unknown job_id=%"PRId64" sig=%d rc=%d errno=%d",
                rep.job_id, rep.sig, rep.rc, rep.detail_errno);
        return 0;
    }

    if (IS_FINISH(job->jStatus))
        return 0;

    // Pending: old mbd did sigPFjob() for “non checkpoint”. If you still want
    // that behavior, keep it here; otherwise keep it dead simple and return.
    if (IS_PEND(job->jStatus)) {
        if (rep.rc == LSBE_NO_ERROR) {
            // Optional legacy behavior: record “signal delivered while pending”.
            // If you do not want it, remove this block entirely.
            if (rep.sig != SIG_CHKPNT && rep.sig != SIG_CHKPNT_COPY) {
                sigPFjob(job, rep.sig, 0, time(NULL));
            }
        }
        return 0;
    }

    // mbd change job status based on signal
    mbd_jstatus_change_by_signal_reply(job, &rep);

    return 0;
}

static void
mbd_jstatus_change_by_signal_reply(struct jData *job,
                                   const struct wire_job_sig_reply *rep)
{
    if (!job || !rep)
        return;

    if (rep->rc == LSBE_NO_ERROR) {
        LS_INFO("signal delivered job_id=%"PRId64" sig=%d",
                rep->job_id, rep->sig);
    } else {
        LS_ERR("signal failed job_id=%"PRId64" sig=%d rc=%d errno=%d",
               rep->job_id, rep->sig, rep->rc, rep->detail_errno);
    }

    if (rep->rc == LSBE_NO_JOB) {
        job->newReason = EXIT_NORMAL;
        mbd_job_status_change(job, JOB_STAT_EXIT, LOG_IT, "signal_no_job");
        return;
    }

    if (rep->rc != LSBE_NO_ERROR)
        return;

    switch (rep->sig) {
    case SIGSTOP:
        if (job->jStatus & JOB_STAT_USUSP) {
            LS_INFO("job %s signal SIGSTOP duplicate rejected (already USUSP)",
                    lsb_jobid2str(job->jobId));
            return;
        }
        job->newReason |= SUSP_USER_STOP;
        mbd_job_status_change(job, JOB_STAT_USUSP, LOG_IT, "signal_stop");
        return;

    case SIGCONT:
        if (!(job->jStatus & (JOB_STAT_USUSP | JOB_STAT_SSUSP | JOB_STAT_PSUSP))) {
            LS_INFO("job %s signal SIGCONT duplicate rejected (not suspended)",
                    lsb_jobid2str(job->jobId));
            return;
        }
        job->newReason &= ~SUSP_USER_STOP;
        job->newReason |= SUSP_USER_RESUME;
        mbd_job_status_change(job, JOB_STAT_RUN, LOG_IT, "signal_cont");
        return;

    default:
        // TERM/KILL handled by finish path.
        return;
    }
}

static void
mbd_handle_finish_job(struct jData *job, int old_status, time_t event_time)
{
    int listno;

    (void)event_time;

    // Minimal cleanup (keep the “finish choke point” semantics).
    job->jStatus &= ~JOB_STAT_MIG;
    job->pendEvent.sig1 = SIG_NULL;
    job->pendEvent.sig = SIG_NULL;
    job->pendEvent.notSwitched = FALSE;
    job->pendEvent.notModified = FALSE;

    // Remove from whichever list we were in.
    if (IS_START(old_status))
        listno = SJL;
    else
        listno = PJL;

    offJobList(job, listno);

    // Insert into FJL (use the old idiom you showed).
    inList((struct listEntry *)jDataList[FJL]->forw,
           (struct listEntry *)job);
}

void
mbd_job_status_change(struct jData *job, int new_status, time_t event_time,
                      const char *why)
{
    int old_status;

    if (!job)
        return;

    assert(why);
    assert(event_time > 0);

    old_status = job->jStatus;

    if (MASK_STATUS(new_status & ~JOB_STAT_UNKWN) ==
        MASK_STATUS(old_status & ~JOB_STAT_UNKWN)) {

        LS_INFO("job %s %s duplicate rejected (status 0x%x)",
                lsb_jobid2str(job->jobId), why, old_status);
        return;
    }

    new_status = MASK_STATUS(new_status);

    // ---- PEND -> START ----
    if (IS_START(new_status) && IS_PEND(old_status)) {
        job->newReason = 0;
        job->subreasons = 0;

        if (job->startTime == 0)
            job->startTime = event_time;

        SET_STATE(job->jStatus, new_status);

        offJobList(job, PJL);
        inStartJobList(job);

        log_newstatus(job);

        LS_INFO("job %s %s transition PEND->START old=0x%x new=0x%x",
                lsb_jobid2str(job->jobId), why, old_status, job->jStatus);
        return;
    }

    // ---- START -> PEND ----
    if ((new_status & JOB_STAT_PEND) && IS_START(old_status)) {
        SET_STATE(job->jStatus, new_status);

        offJobList(job, SJL);

        listInsertEntryBefore((LIST_T *)jDataList[PJL],
                              (LIST_ENTRY_T *)jDataList[PJL],
                              (LIST_ENTRY_T *)job);

        log_newstatus(job);

        LS_INFO("job %s %s transition START->PEND old=0x%x new=0x%x reason=%d",
                lsb_jobid2str(job->jobId), why, old_status, job->jStatus,
                job->newReason);
        return;
    }

    // ---- * -> FINISH ----
    if (IS_FINISH(new_status)) {
        if (job->endTime == 0)
            job->endTime = event_time;

        if (job->newReason == 0)
            job->newReason = EXIT_NORMAL;

        SET_STATE(job->jStatus, new_status);

        mbd_handle_finish_job(job, old_status, event_time);

        log_newstatus(job);

        LS_INFO("job %s %s transition ->FINISH old=0x%x new=0x%x reason=%d",
                lsb_jobid2str(job->jobId), why, old_status, job->jStatus,
                job->newReason);
        return;
    }

    // ---- START <-> SUSP family ----
    if (IS_START(old_status) && IS_START(new_status) &&
        (IS_SUSP(old_status) || IS_SUSP(new_status))) {

        if (IS_SUSP(new_status))
            job->ssuspTime = event_time;

        SET_STATE(job->jStatus, new_status);

        log_newstatus(job);

        LS_INFO("job %s %s transition SUSP old=0x%x new=0x%x",
                lsb_jobid2str(job->jobId), why, old_status, job->jStatus);
        return;
    }

    // ---- fallback ----
    SET_STATE(job->jStatus, new_status);

    log_newstatus(job);

    LS_INFO("job %s %s transition FALLBACK old=0x%x new=0x%x",
            lsb_jobid2str(job->jobId), why, old_status, job->jStatus);
}

// ---------- The space of static functions

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

static int pending_sig_allowed(int sig)
{
    switch (sig) {
    case SIGTERM:
    case SIGINT:
    case SIGKILL:
    case SIGSTOP:
        return 1;
    default:
        return 0;
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
