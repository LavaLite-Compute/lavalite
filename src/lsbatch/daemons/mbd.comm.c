/*
 * Copyright (C) 2007 Platform Computing Inc
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

extern int connTimeout;

// enqueue a message to sbatchd
static sbdReplyType call_sbd(struct hData *, char *, int);

extern sbdReplyType start_ajob(struct jData *jDataPtr, struct qData *qp,
                               struct jobReply *jobReply);

// Inititlize the sbd node list
struct sbdNode sbdNodeList = {.forw = &sbdNodeList, .back = &sbdNodeList};

sbdReplyType start_job(struct jData *job,
                       struct qData *qp,
                       struct jobReply *jobReply)
{
    // This is the node where sbd lives and where we are
    // going to send this job
    struct hData *host_node = job->hPtr[0];

    // Bail if we dont have a connection to this sbd
    // in this case we should not be here to begin with
    // but that is a scheduling issue
    if (host_node->sbd_node == NULL) {
        LS_ERRX("sbd on the node %s is not connected cannot schedule",
               host_node->host);
        return ERR_UNREACH_SBD;

    }

    LS_DEBUG("job %s", lsb_jobid2str(job->jobId));

    struct jobSpecs jobSpecs;
    memset(&jobSpecs, 0, sizeof(jobSpecs));
    packJobSpecs(job, &jobSpecs);

    if (mbd_read_job_file(&jobSpecs, job) == -1) {
        LS_ERR("failed to read job file for %s", lsb_jobid2str(job->jobId));
        freeJobSpecs(&jobSpecs);
        return ERR_NO_FILE;
    }

    struct packet_header hdr;
    hdr.operation = MBD_NEW_JOB;
    // Quoqe tu, Brute
    size_t buflen = LL_BUFSIZ_64K;
    for (int i = 0; i < jobSpecs.numEnv; i++)
        buflen += strlen(jobSpecs.env[i]);

    buflen += jobSpecs.job_file_data.len;
    buflen += jobSpecs.eexec.len;

    /*
     * Guardrail: the total job payload should never exceed
     * base + maximum allowed environment size.
     * If this trips, either the jobfile is corrupted or future code
     * introduced a regresssion in sizing — both worth catching early.
     */
    if (buflen > (size_t)(LL_ENVVAR_MAX + LL_BUFSIZ_64K)) {
        LS_ERR("%s: jobspec too large (%zu bytes), refusing to send to sbatchd",
               __func__, buflen);
        return ERR_JOB_TOO_LARGE;
    }

    XDR xdrs;
    char *request_buf = calloc(buflen, sizeof(char));
    if (request_buf == NULL) {
        LS_ERR("calloc(%d) for job %s failed",
               buflen, lsb_jobid2str(job->jobId));
        freeJobSpecs(&jobSpecs);
        return ERR_MEM;
    }

    xdrmem_create(&xdrs, request_buf, buflen, XDR_ENCODE);

    // No auth no because the sbd already authenticated during its
    // registration with mbd
    if (!xdr_encodeMsg(&xdrs,
                       (char *)&jobSpecs,
                       &hdr,
                       xdr_jobSpecs,
                       0,
                       NULL)) {
        LS_ERR("xdr_encodeMsg failed for job %s", lsb_jobid2str(job->jobId));
        xdr_destroy(&xdrs);
        free(request_buf);
        freeJobSpecs(&jobSpecs);
        return ERR_FAIL;
    }

    // call_sbd on the host node selected by the scheduler
    // note this node must be connected to mbd as it its sbd
    // must have registered previously with mbd. otherwise
    // we must fail.
    sbdReplyType reply = call_sbd(host_node, request_buf, xdr_getpos(&xdrs));
    xdr_destroy(&xdrs);
    free(request_buf);
    freeJobSpecs(&jobSpecs);

    if (reply == ERR_NULL || reply == ERR_FAIL || reply == ERR_UNREACH_SBD)
        return reply;

    jobReply->jobId = job->jobId;
    jobReply->jobPid = 0;
    jobReply->jobPGid = 0;
    jobReply->jStatus = jobSpecs.jStatus;
    jobReply->jStatus &= ~JOB_STAT_MIG;
    if (jobSpecs.options & SUB_PRE_EXEC)
        SET_STATE(jobReply->jStatus, JOB_STAT_RUN | JOB_STAT_PRE_EXEC);
    else
        SET_STATE(jobReply->jStatus, JOB_STAT_RUN);

    // Build the cache pointer to jData
    struct mbd_sbd_job *sj;
    sj = calloc(1, sizeof(struct mbd_sbd_job));
    if (!sj) {
        errno = ENOMEM;
        LS_ERR("failed to allocate mbd_sbd_job for %s",
               lsb_jobid2str(job->jobId));
        reply = ERR_MEM;
        return reply;
    }

    sj->job = job;
    ll_list_append(&host_node->sbd_job_list, &sj->next_job);

    return reply;
}

sbdReplyType switch_job(struct jData *job, int options)
{
    static char fname[] = "switch_job";
    struct jobSpecs jobSpecs;
    char *request_buf = NULL;
    struct packet_header hdr;
    char *reply_buf = NULL;
    XDR xdrs;
    sbdReplyType reply;
    int buflen;
    struct hData *HostData = job->hPtr[0];
    char *toHost = job->hPtr[0]->host;
    struct sbdNode sbdNode;
    struct lsfAuth *auth = NULL;

    if (logclass & (LC_SIGNAL | LC_EXEC))
        ls_syslog(LOG_DEBUG2, "%s: job=%s", fname,
                  lsb_jobid2str(job->jobId));

    packJobSpecs(job, &jobSpecs);

    if (options == TRUE) {
        hdr.operation = MBD_SWIT_JOB;
    } else {
        hdr.operation = MBD_MODIFY_JOB;
    }
    buflen = LL_BUFSIZ_32K;
    request_buf = calloc(buflen, sizeof(char));
    xdrmem_create(&xdrs, request_buf, buflen, XDR_ENCODE);
    if (!xdr_encodeMsg(&xdrs, (char *) &jobSpecs, &hdr, xdr_jobSpecs, 0,
                       auth)) {
        ls_syslog(LOG_ERR, "%s", __func__, "xdr_encodeMsg");
        xdr_destroy(&xdrs);
        free(request_buf);
        freeJobSpecs(&jobSpecs);
        return ERR_FAIL;
    }

    sbdNode.jData = job;
    sbdNode.hData = HostData;
    if (options == TRUE) {
        sbdNode.reqCode = MBD_SWIT_JOB;
    } else {
        sbdNode.reqCode = MBD_MODIFY_JOB;
    }

    reply = call_sbd(sbdNode.hData, request_buf, XDR_GETPOS(&xdrs));

    xdr_destroy(&xdrs);
    free(request_buf);
    freeJobSpecs(&jobSpecs);

    if (reply == ERR_NULL || reply == ERR_FAIL || reply == ERR_UNREACH_SBD)
        return reply;

    if (reply != ERR_NO_ERROR) {
        ls_syslog(LOG_ERR,
                  "%s: Job <%s>: Illegal reply code <%d> from host <%s>, "
                  "switch job failed",
                  fname, lsb_jobid2str(job->jobId), reply, toHost);
    }

    if (reply_buf)
        free(reply_buf);

    return reply;
}

sbdReplyType signal_job(struct jData *jp, struct jobSig *sendReq,
                        struct jobReply *jobReply)
{
    static char fname[] = "signal_job";
    struct packet_header hdr;
    char request_buf[MSGSIZE];
    XDR xdrs;
    sbdReqType reqCode;
    sbdReplyType reply;
    struct jobSig jobSig;
    struct hData *hostData = jp->hPtr[0];
    char *toHost = jp->hPtr[0]->host;
    struct sbdNode sbdNode;
    struct lsfAuth *auth = NULL;

    jobSig.jobId = jp->jobId;
    jobSig.sigValue = sig_encode(sendReq->sigValue);
    jobSig.actFlags = sendReq->actFlags;
    jobSig.chkPeriod = sendReq->chkPeriod;
    jobSig.actCmd = sendReq->actCmd;
    jobSig.reasons = sendReq->reasons;
    jobSig.subReasons = sendReq->subReasons;

    if (logclass & LC_SIGNAL)
        ls_syslog(LOG_DEBUG, "%s: Job %s encoded sigValue %d actFlags %x",
                  fname, lsb_jobid2str(jobSig.jobId), jobSig.sigValue,
                  jobSig.actFlags);

    reqCode = MBD_SIG_JOB;
    xdrmem_create(&xdrs, request_buf, MSGSIZE / 2, XDR_ENCODE);
    hdr.operation = reqCode;
    if (!xdr_encodeMsg(&xdrs, (char *) &jobSig, &hdr, xdr_jobSig, 0, auth)) {
        ls_syslog(LOG_ERR, "%s", __func__, "xdr_encodeMsg");
        xdr_destroy(&xdrs);
        return ERR_FAIL;
    }

    sbdNode.jData = jp;
    sbdNode.hData = hostData;
    sbdNode.reqCode = MBD_SIG_JOB;
    sbdNode.sigVal = sendReq->sigValue;
    sbdNode.sigFlags = sendReq->actFlags;

    reply = call_sbd(sbdNode.hData, request_buf, XDR_GETPOS(&xdrs));
    xdr_destroy(&xdrs);

    if (reply == ERR_NULL || reply == ERR_FAIL || reply == ERR_UNREACH_SBD)
        return reply;

    switch (reply) {
    case ERR_NO_ERROR:
        break;

    case ERR_FORK_FAIL:
    case ERR_BAD_REQ:
    case ERR_NO_JOB:
    case ERR_SIG_RETRY:
        break;

    default:
        ls_syslog(
            LOG_ERR,
            "%s: Job <%s>: Illegal reply code <%d> from sbatchd on host <%s>",
            fname, lsb_jobid2str(jp->jobId), reply, toHost);
        reply = ERR_BAD_REPLY;
    }

    xdr_destroy(&xdrs);

    return reply;
}

// Bug this function does not make any sense
sbdReplyType msg_job(struct jData *jp, struct Buffer *mbuf,
                     struct jobReply *jobReply)
{
    char *toHost = jp->hPtr[0]->host;

    sbdReplyType reply = call_sbd(jp->hPtr[0], mbuf->data, mbuf->len);
    if (reply == ERR_NULL || reply == ERR_FAIL || reply == ERR_UNREACH_SBD)
        return reply;

    switch (reply) {
    case ERR_NO_ERROR:
        break;

    case ERR_BAD_REQ:
    case ERR_NO_JOB:
        break;

    default:
        ls_syslog(LOG_ERR,
                  "%s: job <%s>: illegal reply code <%d> from host <%s>",
                  __func__, lsb_jobid2str(jp->jobId), reply, toHost);
        reply = ERR_BAD_REPLY;
    }

    return reply;
}

sbdReplyType probe_slave(struct hData *hData, char sendJobs)
{
    static char fname[] = "probe_slave";
    char *request_buf, *reply_buf = NULL;
    int buflen = 0;
    struct sbdPackage sbdPackage;
    XDR xdrs;
    int i;
    sbdReplyType reply;
    struct packet_header hdr;
    char *toHost = hData->host;
    struct packet_header hdrBuf;
    struct lsfAuth *auth = NULL;

    memset(&xdrs, 0, sizeof(XDR));
    if (logclass & LC_COMM)
        ls_syslog(LOG_DEBUG, "%s: Probing <%s> sendJobs %d", fname, toHost,
                  sendJobs);

    if (sendJobs) {
        if ((sbdPackage.numJobs = countNumSpecs(hData)) > 0)
            sbdPackage.jobs = (struct jobSpecs *) my_calloc(
                sbdPackage.numJobs, sizeof(struct jobSpecs), fname);
        else {
            sbdPackage.numJobs = 0;
            sbdPackage.jobs = NULL;
        }

        buflen = sbatchdJobs(&sbdPackage, hData);
        hdr.operation = MBD_PROBE;
        request_buf = (char *) my_malloc(buflen, fname);
        xdrmem_create(&xdrs, request_buf, buflen, XDR_ENCODE);
        if (!xdr_encodeMsg(&xdrs, (char *) &sbdPackage, &hdr, xdr_sbdPackage, 0,
                           auth)) {
            ls_syslog(LOG_ERR, "%s", __func__, "xdr_encodeMsg");
            xdr_destroy(&xdrs);
            free(request_buf);
            for (i = 0; i < sbdPackage.numJobs; i++)
                freeJobSpecs(&sbdPackage.jobs[i]);
            if (sbdPackage.jobs)
                free(sbdPackage.jobs);
            return ERR_FAIL;
        }
    } else {
        hdr.operation = MBD_PROBE;
        hdr.length = 0;
        request_buf = (char *) &hdrBuf;
        xdrmem_create(&xdrs, request_buf, sizeof(hdrBuf), XDR_ENCODE);

        if (!xdr_pack_hdr(&xdrs, &hdr)) {
            ls_syslog(LOG_ERR, "%s", __func__, "xdr_pack_hdr");
            xdr_destroy(&xdrs);
            return ERR_FAIL;
        }
    }

    struct sbdNode sbdNode;
    sbdNode.jData = NULL;
    sbdNode.hData = hData;
    sbdNode.reqCode = MBD_PROBE;

    reply = call_sbd(hData, request_buf, XDR_GETPOS(&xdrs));
    xdr_destroy(&xdrs);

    if (sendJobs) {
        free(request_buf);
        for (i = 0; i < sbdPackage.numJobs; i++)
            freeJobSpecs(&sbdPackage.jobs[i]);
        if (sbdPackage.jobs)
            free(sbdPackage.jobs);
    }

    if (reply == ERR_NULL || reply == ERR_FAIL || reply == ERR_UNREACH_SBD)
        return reply;

    switch (reply) {
    case ERR_NO_ERROR:
    case ERR_NO_LIM:
        break;
    default:
        ls_syslog(LOG_ERR,
                  "%s: Illegal reply code <%d> from sbatchd on host <%s>",
                  fname, reply, toHost);
        break;
    }

    if (reply_buf)
        free(reply_buf);
    return reply;
}

// LavaLite use existing connection to sbd
sbdReplyType call_sbd(struct hData *host_node, char *request_buf, int len)
{

    struct mbd_client_node *client;
    client = host_node->sbd_node;
    if (client == NULL) {
        // This should never happen if the scheduler did its job
        // The caller should have detected this situation already
        LS_CRIT("no sbatchd connection for host <%s>", host_node->host);
        abort();
    }

    int ch_id = client->chanfd;
    if (ch_id < 0) {
        LS_CRIT("invalid chanfd %d for host <%s>", ch_id, host_node->host);
        abort();
    }

    // From here down, behave like the original call_sbd,
    // just without serv_connect() / io_nonblock_.

    struct Buffer *buf;
    chan_alloc_buf(&buf, len);
    if (buf == NULL) {
        LS_ERR("chan_alloc_buf(%d) failed for host <%s>", len, host_node->host);
        lsberrno = LSBE_SYS_CALL;
        return ERR_FAIL;
    }

    memcpy(buf->data, request_buf, (size_t)len);
    buf->len = len;

    if (chan_enqueue(ch_id, buf) < 0) {
        LS_ERR("chan_enqueue(%d) failed for host <%s>", ch_id, host_node->host);
        chan_free_buf(buf);
        chan_close(ch_id);
        lsberrno = LSBE_SYS_CALL;
        return ERR_FAIL;
    }

    // Enable epollout so dowrite() will send the Buffer out
    // to connected sbd
    struct epoll_event ev;
    memset(&ev, 0, sizeof(struct epoll_event));
    ev.data.fd = ch_id;
    ev.events = EPOLLIN | EPOLLOUT | EPOLLERR;

    if (epoll_ctl(mbd_efd, EPOLL_CTL_MOD, chan_sock(ch_id), &ev) < 0) {
        LS_ERR("epoll_ctl(EPOLLOUT) failed for host=%s ch_id=%d",
               host_node->host, ch_id);
        // Bug here we have to cleanup at least the current buffer and
        // dequeue it from the channel
        return ERR_FAIL;
    }

    return ERR_NO_ERROR;
}
