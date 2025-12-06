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
static sbdReplyType call_sbd(char *, char *, int, struct sbdNode *);

extern sbdReplyType start_ajob(struct jData *jDataPtr, struct qData *qp,
                               struct jobReply *jobReply);

// Inititlize the sbd node list
struct sbdNode sbdNodeList = {.forw = &sbdNodeList, .back = &sbdNodeList};

sbdReplyType start_job(struct jData *jDataPtr, struct qData *qp,
                       struct jobReply *jobReply)
{
    static char fname[] = "start_job";
    struct jobSpecs jobSpecs;
    char *request_buf = NULL;
    struct packet_header hdr;
    char *reply_buf = NULL;
    XDR xdrs;
    sbdReplyType reply;
    int buflen, i;
    struct hData *hostData = jDataPtr->hPtr[0];
    char *toHost = jDataPtr->hPtr[0]->host;
    struct lenData jf;
    struct lenData aux_auth_data;
    struct sbdNode sbdNode;
    struct lsfAuth *auth = NULL;

    if (logclass & (LC_SCHED | LC_EXEC))
        ls_syslog(LOG_DEBUG2, "%s: job=%s", fname,
                  lsb_jobid2str(jDataPtr->jobId));

    packJobSpecs(jDataPtr, &jobSpecs);

    if (pjobSpoolDir != NULL) {
        if (jDataPtr->jobSpoolDir == NULL) {
            strcpy(jobSpecs.jobSpoolDir, pjobSpoolDir);
            jDataPtr->jobSpoolDir = safeSave(pjobSpoolDir);

        } else if (strcmp(pjobSpoolDir, jDataPtr->jobSpoolDir) != 0) {
            strcpy(jobSpecs.jobSpoolDir, pjobSpoolDir);
            FREEUP(jDataPtr->jobSpoolDir);
            jDataPtr->jobSpoolDir = safeSave(pjobSpoolDir);
        }
    } else {
        if (jDataPtr->jobSpoolDir != NULL) {
            jobSpecs.jobSpoolDir[0] = '\0';
            FREEUP(jDataPtr->jobSpoolDir);
        }
    }

    if (readLogJobInfo(&jobSpecs, jDataPtr, &jf, &aux_auth_data) == -1) {
        ls_syslog(LOG_ERR, "%s", __func__, lsb_jobid2str(jDataPtr->jobId),
                  "readLogJobInfo");
        freeJobSpecs(&jobSpecs);
        return ERR_NO_FILE;
    }

    if (jobSpecs.options & SUB_RESTART) {
        char *tmp;
        if (strchr(jobSpecs.jobFile, '.') != strrchr(jobSpecs.jobFile, '.')) {
            tmp = strstr(jobSpecs.jobFile, lsb_jobid2str(jobSpecs.jobId));
            if (tmp != NULL) {
                --tmp;
                *tmp = '\0';
            }
        }
    }

    hdr.operation = MBD_NEW_JOB;
    buflen = sizeof(struct jobSpecs) + sizeof(struct sbdPackage) + 100 +
             jobSpecs.numToHosts * MAXHOSTNAMELEN +
             jobSpecs.thresholds.nThresholds * jobSpecs.thresholds.nIdx * 2 *
                 sizeof(float) +
             jobSpecs.nxf * sizeof(struct xFile) + jobSpecs.eexec.len;
    for (i = 0; i < jobSpecs.numEnv; i++)
        buflen += strlen(jobSpecs.env[i]);

    request_buf = (char *) my_malloc(buflen, fname);
    xdrmem_create(&xdrs, request_buf, buflen, XDR_ENCODE);
    if (!xdr_encodeMsg(&xdrs, (char *) &jobSpecs, &hdr, xdr_jobSpecs, 0,
                       auth)) {
        ls_syslog(LOG_ERR, "%s", __func__, "xdr_encodeMsg");
        xdr_destroy(&xdrs);
        free(request_buf);
        freeJobSpecs(&jobSpecs);
        free(jf.data);
        return ERR_FAIL;
    }

    sbdNode.jData = jDataPtr;
    sbdNode.hData = hostData;
    sbdNode.reqCode = MBD_NEW_JOB;

    reply = call_sbd(toHost, request_buf, XDR_GETPOS(&xdrs), &sbdNode);

    xdr_destroy(&xdrs);
    free(request_buf);
    freeJobSpecs(&jobSpecs);
    free(jf.data);

    if (reply == ERR_NULL || reply == ERR_FAIL || reply == ERR_UNREACH_SBD)
        return reply;

    if (reply == ERR_NO_ERROR) {
        jobReply->jobId = jDataPtr->jobId;
        jobReply->jobPid = 0;
        jobReply->jobPGid = 0;
        jobReply->jStatus = jobSpecs.jStatus;
        jobReply->jStatus &= ~JOB_STAT_MIG;
        if (jobSpecs.options & SUB_PRE_EXEC)
            SET_STATE(jobReply->jStatus, JOB_STAT_RUN | JOB_STAT_PRE_EXEC);
        else
            SET_STATE(jobReply->jStatus, JOB_STAT_RUN);
    }

    if (reply_buf)
        free(reply_buf);

    return reply;
}

sbdReplyType switch_job(struct jData *jDataPtr, int options)
{
    static char fname[] = "switch_job";
    struct jobSpecs jobSpecs;
    char *request_buf = NULL;
    struct packet_header hdr;
    char *reply_buf = NULL;
    XDR xdrs;
    sbdReplyType reply;
    int buflen;
    struct hData *HostData = jDataPtr->hPtr[0];
    char *toHost = jDataPtr->hPtr[0]->host;
    struct sbdNode sbdNode;
    struct lsfAuth *auth = NULL;

    if (logclass & (LC_SIGNAL | LC_EXEC))
        ls_syslog(LOG_DEBUG2, "%s: job=%s", fname,
                  lsb_jobid2str(jDataPtr->jobId));

    packJobSpecs(jDataPtr, &jobSpecs);

    if (options == TRUE) {
        hdr.operation = MBD_SWIT_JOB;
    } else {
        hdr.operation = MBD_MODIFY_JOB;
    }
    buflen = sizeof(struct jobSpecs) + sizeof(struct sbdPackage) + 100 +
             jobSpecs.numToHosts * MAXHOSTNAMELEN +
             jobSpecs.thresholds.nThresholds * jobSpecs.thresholds.nIdx * 2 *
                 sizeof(float) +
             jobSpecs.nxf * sizeof(struct xFile);
    buflen = (buflen * 4) / 4;

    request_buf = (char *) my_malloc(buflen, fname);
    xdrmem_create(&xdrs, request_buf, buflen, XDR_ENCODE);
    if (!xdr_encodeMsg(&xdrs, (char *) &jobSpecs, &hdr, xdr_jobSpecs, 0,
                       auth)) {
        ls_syslog(LOG_ERR, "%s", __func__, "xdr_encodeMsg");
        xdr_destroy(&xdrs);
        free(request_buf);
        freeJobSpecs(&jobSpecs);
        return ERR_FAIL;
    }

    sbdNode.jData = jDataPtr;
    sbdNode.hData = HostData;
    if (options == TRUE) {
        sbdNode.reqCode = MBD_SWIT_JOB;
    } else {
        sbdNode.reqCode = MBD_MODIFY_JOB;
    }

    reply = call_sbd(toHost, request_buf, XDR_GETPOS(&xdrs), &sbdNode);

    xdr_destroy(&xdrs);
    free(request_buf);
    freeJobSpecs(&jobSpecs);

    if (reply == ERR_NULL || reply == ERR_FAIL || reply == ERR_UNREACH_SBD)
        return reply;

    if (reply != ERR_NO_ERROR) {
        ls_syslog(LOG_ERR,
                  "%s: Job <%s>: Illegal reply code <%d> from host <%s>, "
                  "switch job failed",
                  fname, lsb_jobid2str(jDataPtr->jobId), reply, toHost);
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

    reply = call_sbd(toHost, request_buf, XDR_GETPOS(&xdrs), &sbdNode);
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

    sbdReplyType reply = call_sbd(toHost, mbuf->data, mbuf->len, NULL);
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
    struct sbdNode sbdNode;
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
            for (i = 0; i < sbdPackage.nAdmins; i++)
                FREEUP(sbdPackage.admins[i]);
            FREEUP(sbdPackage.admins);
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

    sbdNode.jData = NULL;
    sbdNode.hData = hData;
    sbdNode.reqCode = MBD_PROBE;

    reply = call_sbd(toHost, request_buf, XDR_GETPOS(&xdrs), &sbdNode);
    xdr_destroy(&xdrs);

    if (sendJobs) {
        free(request_buf);
        for (i = 0; i < sbdPackage.nAdmins; i++)
            FREEUP(sbdPackage.admins[i]);
        FREEUP(sbdPackage.admins);
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

/* Bug for now return sbdReplyType
 */
sbdReplyType call_sbd(char *toHost, char *request_buf, int len,
                      struct sbdNode *sbdPtr)
{
    int ch_id = serv_connect(toHost, sbd_port, connTimeout);
    if (ch_id < 0)
        return ERR_UNREACH_SBD;

    // Make socket non-blocking for async I/O
    if (io_nonblock_(ch_id) < 0) {
        chan_close(ch_id);
        lsberrno = LSBE_SYS_CALL;
        return ERR_FAIL;
    }

    struct Buffer *buf;
    if (chan_alloc_buf(&buf, len) < 0) {
        chan_close(ch_id);
        lsberrno = LSBE_NO_MEM;
        return ERR_MEM;
    }

    memcpy(buf->data, request_buf, len);
    buf->len = len;

    if (chan_enqueue(ch_id, buf) < 0) {
        chan_free_buf(buf);
        chan_close(ch_id);
        lsberrno = LSBE_SYS_CALL;
        return ERR_FAIL;
    }

    struct sbdNode *node = calloc(1, sizeof(struct sbdNode));
    memcpy(node, sbdPtr, sizeof(struct sbdNode));
    node->chanfd = ch_id;
    node->lastTime = now;

    inList((struct listEntry *) &sbdNodeList, (struct listEntry *) node);
    nSbdConnections++;

    return ERR_NO_ERROR; // Success - reply comes async
}
