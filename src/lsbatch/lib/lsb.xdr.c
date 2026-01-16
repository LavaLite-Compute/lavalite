/* $Id: lsb.xdr.c,v 1.22 2007/08/15 22:18:48 tmizan Exp $
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

#include "lsbatch/lib/lsb.h"

static int allocLoadIdx(float **sched, float **stop, int *outSize, int size);

int lsbSharedResConfigured_ = false;

bool_t xdr_submitReq(XDR *xdrs, struct submitReq *submitReq, void *)
{
    int i, nLimits;
    static int numAskedHosts = 0;
    static char **askedHosts = NULL;

    if (xdrs->x_op == XDR_DECODE) {
        submitReq->fromHost[0] = '\0';
        submitReq->jobFile[0] = '\0';
        submitReq->inFile[0] = '\0';
        submitReq->outFile[0] = '\0';
        submitReq->errFile[0] = '\0';
        submitReq->inFileSpool[0] = '\0';
        submitReq->commandSpool[0] = '\0';
        submitReq->chkpntDir[0] = '\0';
        submitReq->hostSpec[0] = '\0';
        submitReq->cwd[0] = '\0';
        submitReq->subHomeDir[0] = '\0';

        FREEUP(submitReq->queue);
        FREEUP(submitReq->command);
        FREEUP(submitReq->jobName);
        FREEUP(submitReq->preExecCmd);
        FREEUP(submitReq->dependCond);
        FREEUP(submitReq->resReq);
        FREEUP(submitReq->mailUser);
        FREEUP(submitReq->projectName);
        FREEUP(submitReq->loginShell);
        FREEUP(submitReq->schedHostType);

        if (numAskedHosts > 0) {
            for (i = 0; i < numAskedHosts; i++)
                FREEUP(askedHosts[i]);
            FREEUP(askedHosts);
        }
        numAskedHosts = 0;
    }

    if (!(xdr_u_int(xdrs, (unsigned int *) &submitReq->options) &&
          xdr_var_string(xdrs, &submitReq->queue) &&
          xdr_var_string(xdrs, &submitReq->resReq) &&
          xdr_int(xdrs, &submitReq->numProcessors))) {
        return false;
    }

    if (!(xdr_string(xdrs, &submitReq->fromHost, MAXHOSTNAMELEN) &&
          xdr_var_string(xdrs, &submitReq->dependCond) &&
          xdr_var_string(xdrs, &submitReq->jobName) &&
          xdr_var_string(xdrs, &submitReq->command) &&
          xdr_string(xdrs, &submitReq->jobFile, MAXFILENAMELEN) &&
          xdr_string(xdrs, &submitReq->inFile, MAXFILENAMELEN) &&
          xdr_string(xdrs, &submitReq->outFile, MAXFILENAMELEN) &&
          xdr_string(xdrs, &submitReq->errFile, MAXFILENAMELEN) &&
          xdr_var_string(xdrs, &submitReq->preExecCmd) &&
          xdr_string(xdrs, &submitReq->hostSpec, MAXHOSTNAMELEN))) {
        return false;
    }

    if (!xdr_int(xdrs, &submitReq->numAskedHosts))
        return false;

    if (xdrs->x_op == XDR_DECODE && submitReq->numAskedHosts > 0) {
        submitReq->askedHosts =
            (char **) calloc(submitReq->numAskedHosts, sizeof(char *));
        if (submitReq->askedHosts == NULL)
            return false;
    }

    for (i = 0; i < submitReq->numAskedHosts; i++) {
        if (!xdr_var_string(xdrs, &submitReq->askedHosts[i])) {
            submitReq->numAskedHosts = i;
            goto Error0;
        }
    }

    nLimits = LSF_RLIM_NLIMITS;
    if (!xdr_int(xdrs, &nLimits))
        goto Error0;

    for (i = 0; i < nLimits; i++) {
        if (i < LSF_RLIM_NLIMITS) {
            if (!xdr_int(xdrs, &submitReq->rLimits[i]))
                goto Error0;
        } else {
            int j;

            if (!xdr_int(xdrs, &j))
                goto Error0;
        }
    }

    if (!(xdr_time_t(xdrs, &submitReq->submitTime) &&
          xdr_time_t(xdrs, &submitReq->beginTime) &&
          xdr_time_t(xdrs, &submitReq->termTime) &&
          xdr_int(xdrs, &submitReq->umask) &&
          xdr_int(xdrs, &submitReq->sigValue) &&
          xdr_int(xdrs, &submitReq->restartPid) &&
          xdr_time_t(xdrs, &submitReq->chkpntPeriod) &&
          xdr_string(xdrs, &submitReq->chkpntDir, MAXFILENAMELEN) &&
          xdr_string(xdrs, &submitReq->subHomeDir, MAXFILENAMELEN) &&
          xdr_string(xdrs, &submitReq->cwd, MAXFILENAMELEN))) {
        goto Error0;
    }

    if (!xdr_int(xdrs, &submitReq->nxf))
        goto Error0;

    if (xdrs->x_op == XDR_DECODE && submitReq->nxf > 0) {
        if ((submitReq->xf = (struct xFile *) calloc(
                 submitReq->nxf, sizeof(struct xFile))) == NULL)
            goto Error0;
    }

    for (i = 0; i < submitReq->nxf; i++) {
        if (!xdr_array_element(xdrs, &(submitReq->xf[i]), NULL, xdr_xFile))
            goto Error1;
    }

    if (!xdr_var_string(xdrs, &submitReq->mailUser))
        goto Error1;

    if (!xdr_var_string(xdrs, &submitReq->projectName))
        goto Error1;

    if (!xdr_int(xdrs, &submitReq->niosPort))
        goto Error1;
    if (!xdr_int(xdrs, &submitReq->maxNumProcessors))
        goto Error1;
    if (!xdr_var_string(xdrs, &submitReq->loginShell))
        goto Error1;
    if (!xdr_var_string(xdrs, &submitReq->schedHostType))
        goto Error1;

    if (xdrs->x_op == XDR_DECODE) {
        numAskedHosts = submitReq->numAskedHosts;
        askedHosts = submitReq->askedHosts;
    }

    if (!xdr_int(xdrs, &submitReq->options2))
        goto Error1;

    if (!(xdr_string(xdrs, &submitReq->inFileSpool, MAXFILENAMELEN) &&
          xdr_string(xdrs, &submitReq->commandSpool, MAXFILENAMELEN))) {
        return false;
    }

    if (!xdr_int(xdrs, &submitReq->userPriority))
        goto Error1;

    return true;

Error1:
    if (xdrs->x_op == XDR_DECODE) {
        FREEUP(submitReq->xf);
        submitReq->xf = NULL;
    }

Error0:
    if (xdrs->x_op == XDR_DECODE) {
        for (i = 0; i < submitReq->numAskedHosts; i++)
            FREEUP(submitReq->askedHosts[i]);
        FREEUP(submitReq->askedHosts);
        submitReq->askedHosts = NULL;
        submitReq->numAskedHosts = 0;
    }

    return false;
}

bool_t xdr_modifyReq(XDR *xdrs, struct modifyReq *modifyReq, void *)
{
    int jobArrId, jobArrElemId;

    if (xdrs->x_op == XDR_DECODE) {
        FREEUP(modifyReq->jobIdStr);
    }
    if (xdrs->x_op == XDR_ENCODE) {
        jobId64To32(modifyReq->jobId, &jobArrId, &jobArrElemId);
    }
    if (!(xdr_int(xdrs, &jobArrId) && xdr_int(xdrs, &(modifyReq->delOptions))))
        return false;

    if (!xdr_array_element(xdrs, &modifyReq->submitReq, NULL, xdr_submitReq))
        return false;

    if (!xdr_var_string(xdrs, &(modifyReq->jobIdStr)))
        return false;

    if (!(xdr_int(xdrs, &(modifyReq->delOptions2)))) {
        return false;
    }
    if (!xdr_int(xdrs, &jobArrElemId)) {
        return false;
    }
    if (xdrs->x_op == XDR_DECODE) {
        jobId32To64(&modifyReq->jobId, jobArrId, jobArrElemId);
    }

    return true;
}

bool_t xdr_jobInfoReq(XDR *xdrs, struct jobInfoReq *req, void *unused)
{
    (void)unused;

    // options
    if (!xdr_int(xdrs, &req->options))
        return FALSE;

    // userName
    if (!xdr_string(xdrs, &req->userName, LL_BUFSIZ_32))
        return FALSE;

    // jobId as true 64-bit
    if (!xdr_int64_t(xdrs, &req->jobId))
        return FALSE;

    // jobName
    if (!xdr_string(xdrs, &req->jobName, LL_BUFSIZ_256))
        return FALSE;

    // queue
    if (!xdr_string(xdrs, &req->queue, LL_BUFSIZ_32))
        return FALSE;

    // host
    if (!xdr_string(xdrs, &req->host, MAXHOSTNAMELEN))
        return FALSE;

    return TRUE;
}

bool_t
xdr_signalReq(XDR *xdrs, struct signalReq *req, void *hdr)
{
    int job_arr_id = 0;
    int job_arr_elem_id = 0;

    (void)hdr;

    if (xdrs == NULL || req == NULL)
        return FALSE;

    if (xdrs->x_op == XDR_ENCODE)
        jobId64To32(req->jobId, &job_arr_id, &job_arr_elem_id);

    if (!xdr_int(xdrs, &req->sigValue))
        return FALSE;

    if (!xdr_int(xdrs, &job_arr_id))
        return FALSE;

    /*
     * MVP: only SIG_ARRAY_REQUEUE carries extra fields.
     * (Checkpointing and legacy delete are not supported.)
     */
    if (req->sigValue == SIG_ARRAY_REQUEUE) {
        if (!xdr_time_t(xdrs, &req->chkPeriod))
            return FALSE;
        if (!xdr_int(xdrs, &req->actFlags))
            return FALSE;
    } else {
        if (xdrs->x_op == XDR_DECODE) {
            req->chkPeriod = 0;
            req->actFlags = 0;
        }
    }

    if (!xdr_int(xdrs, &job_arr_elem_id))
        return FALSE;

    if (xdrs->x_op == XDR_DECODE)
        jobId32To64(&req->jobId, job_arr_id, job_arr_elem_id);

    return TRUE;
}

bool_t xdr_lsbMsg(XDR *xdrs, struct lsbMsg *m, void *)
{
    int xdrrc;
    int jobArrId, jobArrElemId;

    if (xdrs->x_op == XDR_DECODE) {
        m->header->src[0] = '\0';
        m->header->dest[0] = '\0';
    }

    if (xdrs->x_op == XDR_FREE) {
        FREEUP(m->header->src);
        FREEUP(m->header->dest);
        FREEUP(m->msg);
        return true;
    }

    xdrrc = xdr_int(xdrs, &m->header->msgId);
    if (!xdrrc)
        goto Failure;

    xdrrc = xdr_int(xdrs, &m->header->usrId);
    if (!xdrrc)
        goto Failure;

    if (xdrs->x_op == XDR_ENCODE) {
        jobId64To32(m->header->jobId, &jobArrId, &jobArrElemId);
    }
    xdrrc = xdr_int(xdrs, &jobArrId);
    if (!xdrrc)
        goto Failure;

    xdrrc = xdr_int(xdrs, &m->header->type);
    if (!xdrrc)
        goto Failure;

    xdrrc = xdr_string(xdrs, &m->header->src, LSB_MAX_SD_LENGTH);
    if (!xdrrc)
        goto Failure;

    xdrrc = xdr_string(xdrs, &m->header->dest, LSB_MAX_SD_LENGTH);
    if (!xdrrc)
        goto Failure;

    xdrrc = xdr_var_string(xdrs, &m->msg);
    if (!xdrrc)
        goto Failure;

    xdrrc = xdr_int(xdrs, &jobArrElemId);
    if (!xdrrc)
        goto Failure;

    if (xdrs->x_op == XDR_DECODE) {
        jobId32To64(&m->header->jobId, jobArrId, jobArrElemId);
    }

    return true;

Failure:
    return false;
}

bool_t xdr_submitMbdReply(XDR *xdrs, struct submitMbdReply *reply, void *)
{
    static char queueName[LL_BUFSIZ_32];
    static char jobName[MAX_CMD_DESC_LEN];
    int jobArrId, jobArrElemId;

    if (xdrs->x_op == XDR_DECODE) {
        queueName[0] = '\0';
        jobName[0] = '\0';
        reply->queue = queueName;
        reply->badJobName = jobName;
    }

    if (xdrs->x_op == XDR_ENCODE) {
        jobId64To32(reply->jobId, &jobArrId, &jobArrElemId);
    }
    if (!(xdr_int(xdrs, &(jobArrId)) && xdr_int(xdrs, &(reply->badReqIndx)) &&
          xdr_string(xdrs, &reply->queue, LL_BUFSIZ_32) &&
          xdr_string(xdrs, &reply->badJobName, MAX_CMD_DESC_LEN)))
        return false;

    if (!xdr_int(xdrs, &jobArrElemId)) {
        return false;
    }

    if (xdrs->x_op == XDR_DECODE) {
        jobId32To64(&reply->jobId, jobArrId, jobArrElemId);
    }

    return true;
}

bool_t xdr_parameterInfo(XDR *xdrs, struct parameterInfo *paramInfo, void *)
{
    if (xdrs->x_op == XDR_DECODE) {
        FREEUP(paramInfo->defaultQueues);
        FREEUP(paramInfo->defaultHostSpec);
        FREEUP(paramInfo->defaultProject);
        FREEUP(paramInfo->pjobSpoolDir);
    }

    if (!(xdr_var_string(xdrs, &paramInfo->defaultQueues) &&
          xdr_var_string(xdrs, &paramInfo->defaultHostSpec)))
        return false;

    if (!(xdr_int(xdrs, &(paramInfo->mbatchdInterval)) &&
          xdr_int(xdrs, &(paramInfo->sbatchdInterval)) &&
          xdr_int(xdrs, &(paramInfo->jobAcceptInterval)) &&
          xdr_int(xdrs, &(paramInfo->maxDispRetries)) &&
          xdr_int(xdrs, &(paramInfo->maxSbdRetries)) &&
          xdr_int(xdrs, &(paramInfo->cleanPeriod)) &&
          xdr_int(xdrs, &(paramInfo->maxNumJobs)) &&
          xdr_int(xdrs, &(paramInfo->pgSuspendIt))))
        return false;

    if (!(xdr_var_string(xdrs, &paramInfo->defaultProject)))
        return false;

    if (!(xdr_int(xdrs, &(paramInfo->maxJobArraySize)))) {
        return false;
    }

    if (!xdr_int(xdrs, &(paramInfo->jobTerminateInterval)))
        return false;

    if (!xdr_bool(xdrs, &paramInfo->disableUAcctMap))
        return false;

    if (!(xdr_int(xdrs, &(paramInfo->maxJobArraySize))))
        return false;

    if (!(xdr_var_string(xdrs, &paramInfo->pjobSpoolDir))) {
        return false;
    }

    if (!(xdr_int(xdrs, &paramInfo->maxUserPriority) &&
          xdr_int(xdrs, &paramInfo->jobPriorityValue) &&
          xdr_int(xdrs, &paramInfo->jobPriorityTime))) {
        return false;
    }

    if (!xdr_int(xdrs, &(paramInfo->maxJobId))) {
        return false;
    }

    if (!(xdr_int(xdrs, &(paramInfo->maxAcctArchiveNum)) &&
          xdr_int(xdrs, &(paramInfo->acctArchiveInDays)) &&
          xdr_int(xdrs, &(paramInfo->acctArchiveInSize)))) {
        return false;
    }

    if (!(xdr_int(xdrs, &(paramInfo->jobDepLastSub)) &&
          xdr_int(xdrs, &(paramInfo->sharedResourceUpdFactor)))) {
        return false;
    }

    return true;
}

bool_t xdr_jobInfoHead(XDR *xdrs, struct jobInfoHead *jobInfoHead, void *)
{
    static char **hostNames = NULL;
    static int numJobs = 0, numHosts = 0;
    static int64_t *jobIds = NULL;
    char *sp;
    int *jobArrIds = NULL, *jobArrElemIds = NULL;
    int i;

    if (!(xdr_int(xdrs, &(jobInfoHead->numJobs)) &&
          xdr_int(xdrs, &(jobInfoHead->numHosts)))) {
        return false;
    }
    if (jobInfoHead->numJobs > 0) {
        if ((jobArrIds = (int *) calloc(jobInfoHead->numJobs, sizeof(int))) ==
            NULL) {
            lsberrno = LSBE_NO_MEM;
            return false;
        }
        if ((jobArrElemIds =
                 (int *) calloc(jobInfoHead->numJobs, sizeof(int))) == NULL) {
            lsberrno = LSBE_NO_MEM;
            FREEUP(jobArrIds);
            return false;
        }
    }
    if (xdrs->x_op == XDR_DECODE) {
        if (jobInfoHead->numJobs > numJobs) {
            FREEUP(jobIds);
            numJobs = 0;
            if ((jobIds = (int64_t *) calloc(jobInfoHead->numJobs,
                                             sizeof(int64_t))) == NULL) {
                lsberrno = LSBE_NO_MEM;
                return false;
            }
            numJobs = jobInfoHead->numJobs;
        }

        if (jobInfoHead->numHosts > numHosts) {
            for (i = 0; i < numHosts; i++)
                FREEUP(hostNames[i]);
            numHosts = 0;
            FREEUP(hostNames);
            if ((hostNames = (char **) calloc(jobInfoHead->numHosts,
                                              sizeof(char *))) == NULL) {
                lsberrno = LSBE_NO_MEM;
                return false;
            }
            for (i = 0; i < jobInfoHead->numHosts; i++) {
                hostNames[i] = malloc(MAXHOSTNAMELEN);
                if (!hostNames[i]) {
                    numHosts = i;
                    lsberrno = LSBE_NO_MEM;
                    return false;
                }
            }
            numHosts = jobInfoHead->numHosts;
        }
        jobInfoHead->jobIds = jobIds;
        jobInfoHead->hostNames = hostNames;
    }

    for (i = 0; i < jobInfoHead->numJobs; i++) {
        if (xdrs->x_op == XDR_ENCODE) {
            jobId64To32(jobInfoHead->jobIds[i], &jobArrIds[i],
                        &jobArrElemIds[i]);
        }
        if (!xdr_int(xdrs, &(jobArrIds[i])))
            return false;
    }

    for (i = 0; i < jobInfoHead->numHosts; i++) {
        sp = jobInfoHead->hostNames[i];
        if (xdrs->x_op == XDR_DECODE)
            sp[0] = '\0';
        if (!(xdr_string(xdrs, &sp, MAXHOSTNAMELEN)))
            return false;
    }

    for (i = 0; i < jobInfoHead->numJobs; i++) {
        if (!xdr_int(xdrs, &jobArrElemIds[i])) {
            return false;
        }
        if (xdrs->x_op == XDR_DECODE) {
            jobId32To64(&jobInfoHead->jobIds[i], jobArrIds[i],
                        jobArrElemIds[i]);
        }
    }

    FREEUP(jobArrIds);
    FREEUP(jobArrElemIds);

    return true;
}

bool_t xdr_jgrpInfoReply(XDR *xdrs, struct jobInfoReply *jobInfoReply, void *)
{
    char *sp;
    int jobArrId, jobArrElemId;

    sp = jobInfoReply->userName;
    if (xdrs->x_op == XDR_DECODE) {
        sp[0] = '\0';

        FREEUP(jobInfoReply->jobBill->dependCond);
        FREEUP(jobInfoReply->jobBill->jobName);
    }

    if (xdrs->x_op == XDR_ENCODE) {
        jobId64To32(jobInfoReply->jobId, &jobArrId, &jobArrElemId);
    }
    if (!(xdr_int(xdrs, &jobArrId) &&
          xdr_int(xdrs, (int *) &(jobInfoReply->status)) &&
          xdr_int(xdrs, &(jobInfoReply->reasons)) &&
          xdr_int(xdrs, &(jobInfoReply->subreasons)) &&
          xdr_time_t(xdrs, &(jobInfoReply->startTime)) &&
          xdr_time_t(xdrs, &(jobInfoReply->predictedStartTime)) &&
          xdr_time_t(xdrs, &(jobInfoReply->endTime)) &&
          xdr_float(xdrs, &(jobInfoReply->cpuTime)) &&
          xdr_int(xdrs, &(jobInfoReply->numToHosts)) &&
          xdr_int(xdrs, &jobInfoReply->userId) &&
          xdr_string(xdrs, &sp, MAXLSFNAMELEN))) {
        return false;
    }

    if (!(xdr_var_string(xdrs, &jobInfoReply->jobBill->dependCond) &&
          xdr_time_t(xdrs, &jobInfoReply->jobBill->submitTime) &&
          xdr_var_string(xdrs, &jobInfoReply->jobBill->jobName))) {
        FREEUP(jobInfoReply->jobBill->dependCond);
        FREEUP(jobInfoReply->jobBill->jobName);
        return false;
    }

    if (!xdr_int(xdrs, &jobArrElemId)) {
        return false;
    }

    if (xdrs->x_op == XDR_DECODE) {
        jobId32To64(&jobInfoReply->jobId, jobArrId, jobArrElemId);
    }

    return true;
}
bool_t xdr_jobInfoReply(XDR *xdrs, struct jobInfoReply *jobInfoReply, void *)
{
    char *sp;
    int i, j;
    static float *loadSched = NULL, *loadStop = NULL;
    static int nIdx = 0;
    static int *reasonTb = NULL;
    static int nReasons = 0;
    int jobArrId, jobArrElemId;

    if (!(xdr_int(xdrs, (int *) &jobInfoReply->jType) &&
          xdr_var_string(xdrs, &jobInfoReply->parentGroup) &&
          xdr_var_string(xdrs, &jobInfoReply->jName))) {
        FREEUP(jobInfoReply->parentGroup);
        FREEUP(jobInfoReply->jName);
        return false;
    }
    for (i = 0; i < NUM_JGRP_COUNTERS; i++) {
        if (!xdr_int(xdrs, (int *) &jobInfoReply->counter[i])) {
            FREEUP(jobInfoReply->parentGroup);
            FREEUP(jobInfoReply->jName);
            return false;
        }
    }
    if (jobInfoReply->jType == JGRP_NODE_GROUP) {
        return (xdr_jgrpInfoReply(xdrs, jobInfoReply, NULL));
    }

    sp = jobInfoReply->userName;
    if (xdrs->x_op == XDR_DECODE)
        sp[0] = '\0';

    if (xdrs->x_op == XDR_ENCODE) {
        jobId64To32(jobInfoReply->jobId, &jobArrId, &jobArrElemId);
    }
    if (!(xdr_int(xdrs, &jobArrId) &&
          xdr_int(xdrs, (int *) &(jobInfoReply->status)) &&
          xdr_int(xdrs, &(jobInfoReply->reasons)) &&
          xdr_int(xdrs, &(jobInfoReply->subreasons)) &&
          xdr_time_t(xdrs, &(jobInfoReply->startTime)) &&
          xdr_time_t(xdrs, &(jobInfoReply->endTime)) &&
          xdr_float(xdrs, &(jobInfoReply->cpuTime)) &&
          xdr_int(xdrs, &(jobInfoReply->numToHosts)) &&
          xdr_int(xdrs, &jobInfoReply->nIdx) &&
          xdr_int(xdrs, &jobInfoReply->numReasons) &&
          xdr_int(xdrs, &jobInfoReply->userId) &&
          xdr_string(xdrs, &sp, MAXLSFNAMELEN))) {
        return false;
    }

    if (xdrs->x_op == XDR_DECODE) {
        if (jobInfoReply->nIdx > nIdx) {
            if (allocLoadIdx(&loadSched, &loadStop, &nIdx,
                             jobInfoReply->nIdx) == -1)
                return false;
        }
        jobInfoReply->loadSched = loadSched;
        jobInfoReply->loadStop = loadStop;

        if (jobInfoReply->numReasons > nReasons) {
            FREEUP(reasonTb);
            nReasons = 0;
            reasonTb = calloc(jobInfoReply->numReasons, sizeof(int));
            if (!reasonTb)
                return false;
            jobInfoReply->reasonTb = reasonTb;
            nReasons = jobInfoReply->numReasons;
        }
    }

    for (i = 0; i < jobInfoReply->nIdx; i++) {
        if (!xdr_float(xdrs, &jobInfoReply->loadSched[i]) ||
            !xdr_float(xdrs, &jobInfoReply->loadStop[i]))
            return false;
    }

    for (i = 0; i < jobInfoReply->numReasons; i++)
        if (!xdr_int(xdrs, &jobInfoReply->reasonTb[i]))
            return false;

    if (xdrs->x_op == XDR_DECODE && jobInfoReply->numToHosts > 0) {
        if ((jobInfoReply->toHosts = (char **) calloc(
                 jobInfoReply->numToHosts, sizeof(char *))) == NULL) {
            jobInfoReply->numToHosts = 0;
            return false;
        }
    }
    for (i = 0; i < jobInfoReply->numToHosts; i++) {
        if (xdrs->x_op == XDR_DECODE) {
            jobInfoReply->toHosts[i] = calloc(1, MAXHOSTNAMELEN);
            if (jobInfoReply->toHosts[i] == NULL) {
                for (j = 0; j < i; j++)
                    free(jobInfoReply->toHosts[j]);
                free(jobInfoReply->toHosts);
                jobInfoReply->numToHosts = 0;
                jobInfoReply->toHosts = NULL;
                return false;
            }
        }
        sp = jobInfoReply->toHosts[i];
        if (!xdr_string(xdrs, &sp, MAXHOSTNAMELEN)) {
            if (xdrs->x_op == XDR_DECODE) {
                for (j = 0; j < i; j++)
                    free(jobInfoReply->toHosts[j]);
                free(jobInfoReply->toHosts);
                jobInfoReply->numToHosts = 0;
                jobInfoReply->toHosts = NULL;
            }
            return false;
        }
    }

    if (!xdr_array_element(xdrs, jobInfoReply->jobBill, NULL, xdr_submitReq)) {
        if (xdrs->x_op == XDR_DECODE) {
            for (j = 0; j < jobInfoReply->numToHosts; j++)
                free(jobInfoReply->toHosts[j]);
            free(jobInfoReply->toHosts);
            jobInfoReply->numToHosts = 0;
            jobInfoReply->toHosts = NULL;
        }
        return false;
    }

    if (!(xdr_int(xdrs, (int *) &jobInfoReply->exitStatus) &&
          xdr_int(xdrs, (int *) &jobInfoReply->execUid) &&
          xdr_var_string(xdrs, &jobInfoReply->execHome) &&
          xdr_var_string(xdrs, &jobInfoReply->execCwd) &&
          xdr_var_string(xdrs, &jobInfoReply->execUsername))) {
        FREEUP(jobInfoReply->execHome);
        FREEUP(jobInfoReply->execCwd);
        FREEUP(jobInfoReply->execUsername);
        return false;
    }

    if (!xdr_time_t(xdrs, &jobInfoReply->reserveTime) ||
        !xdr_int(xdrs, &jobInfoReply->jobPid))
        return false;

    if (!(xdr_time_t(xdrs, &(jobInfoReply->jRusageUpdateTime))))
        return false;
    if (!(xdr_jRusage(xdrs, &(jobInfoReply->runRusage), NULL)))
        return false;

    if (!xdr_time_t(xdrs, &(jobInfoReply->predictedStartTime))) {
        return false;
    }

    if (!xdr_u_short(xdrs, &jobInfoReply->port)) {
        return false;
    }

    if (!xdr_int(xdrs, &jobInfoReply->jobPriority)) {
        return false;
    }

    if (!xdr_int(xdrs, &jobArrElemId)) {
        return false;
    }

    if (xdrs->x_op == XDR_DECODE) {
        jobId32To64(&jobInfoReply->jobId, jobArrId, jobArrElemId);
    }

    return true;
}

bool_t xdr_queueInfoReply(XDR *xdrs, struct queueInfoReply *reply, void *unused)
{
    int i;

    (void)unused;

    // numQueues, badQueue, nIdx
    if (!xdr_int(xdrs, &reply->numQueues))
        return false;

    if (!xdr_int(xdrs, &reply->badQueue))
        return false;

    if (!xdr_int(xdrs, &reply->nIdx))
        return false;

    if (reply->numQueues < 0 || reply->nIdx < 0)
        return false;

    /*
     * DECODE: allocate array of queueInfoEnt if needed.
     *
     * KISS assumption:
     *  - Caller passes a zero-initialized struct for DECODE
     *  - Caller is responsible for freeing reply->queues and
     *    all nested fields later with a dedicated free helper.
     */
    if (xdrs->x_op == XDR_DECODE) {
        if (reply->numQueues == 0) {
            reply->queues = NULL;
        } else {
            reply->queues = (struct queueInfoEnt *)calloc(
                reply->numQueues, sizeof(struct queueInfoEnt));
            if (!reply->queues)
                return false;
        }
    }

    /* Encode/decode each queueInfoEnt */
    for (i = 0; i < reply->numQueues; i++) {
        struct queueInfoEnt *q = &reply->queues[i];

        if (xdrs->x_op == XDR_DECODE && reply->nIdx > 0) {
            /* Each queue gets its own loadSched/loadStop arrays */
            q->nIdx = reply->nIdx;

            q->loadSched = (float *)malloc(reply->nIdx * sizeof(float));
            if (!q->loadSched)
                return false;

            q->loadStop = (float *)malloc(reply->nIdx * sizeof(float));
            if (!q->loadStop)
                return false;
        }

        /* Pass nIdx down if xdr_queueInfoEnt needs it */
        if (!xdr_queueInfoEnt(xdrs, q, &reply->nIdx))
            return false;
    }

    return true;
}

bool_t xdr_queueInfoEnt(XDR *xdrs, struct queueInfoEnt *q, void *ctx)
{
    int i, j;
    int nIdx = 0;

    if (ctx != NULL) {
        nIdx = *(int *)ctx;
    } else {
        nIdx = q->nIdx;
    }

    if (nIdx < 0) {
        return false;
    }

    if (xdrs->x_op == XDR_DECODE) {
        q->nIdx = nIdx;
    }

    /* queue name + description */
    if (!xdr_string(xdrs, &q->queue, LL_BUFSIZ_32)) {
        return false;
    }

    if (!xdr_string(xdrs, &q->description, LL_BUFSIZ_256)) {
        return false;
    }

    /* basic string fields */
    if (!xdr_string(xdrs, &q->userList, LL_BUFSIZ_256)) {
        return false;
    }

    if (!xdr_string(xdrs, &q->hostList, LL_BUFSIZ_256)) {
        return false;
    }

    if (!xdr_string(xdrs, &q->windows, LL_BUFSIZ_256)) {
        return false;
    }

    /* hostSpec */
    if (!xdr_string(xdrs, &q->hostSpec, LL_BUFSIZ_32)) {
        return false;
    }

    /* rLimits */
    for (i = 0; i < LSF_RLIM_NLIMITS; i++) {
        if (!xdr_int(xdrs, &q->rLimits[i])) {
            return false;
        }
    }

    /* loadSched / loadStop: arrays of nIdx floats.
     * For DECODE, arrays are allocated by the caller (xdr_queueInfoReply). */
    if (nIdx > 0) {
        if (q->loadSched == NULL || q->loadStop == NULL) {
            return false;
        }

        for (i = 0; i < nIdx; i++) {
            if (!xdr_float(xdrs, &q->loadSched[i])) {
                return false;
            }
            if (!xdr_float(xdrs, &q->loadStop[i])) {
                return false;
            }
        }
    }

    /* scalar fields / counters */
    if (!xdr_int(xdrs, &q->priority)) {
        return false;
    }

    if (!xdr_short(xdrs, &q->nice)) {
        return false;
    }

    if (!xdr_int(xdrs, &q->userJobLimit)) {
        return false;
    }

    if (!xdr_float(xdrs, &q->procJobLimit)) {
        return false;
    }

    if (!xdr_int(xdrs, &q->qAttrib)) {
        return false;
    }

    if (!xdr_int(xdrs, &q->qStatus)) {
        return false;
    }

    if (!xdr_int(xdrs, &q->maxJobs)) {
        return false;
    }

    if (!xdr_int(xdrs, &q->numJobs)) {
        return false;
    }

    if (!xdr_int(xdrs, &q->numPEND)) {
        return false;
    }

    if (!xdr_int(xdrs, &q->numRUN)) {
        return false;
    }

    if (!xdr_int(xdrs, &q->numSSUSP)) {
        return false;
    }

    if (!xdr_int(xdrs, &q->numUSUSP)) {
        return false;
    }

    if (!xdr_int(xdrs, &q->mig)) {
        return false;
    }

    if (!xdr_int(xdrs, &q->acceptIntvl)) {
        return false;
    }

    if (!xdr_int(xdrs, &q->schedDelay)) {
        return false;
    }

    /* more strings */
    if (!xdr_string(xdrs, &q->windowsD, LL_BUFSIZ_256)) {
        return false;
    }

    if (!xdr_string(xdrs, &q->defaultHostSpec, LL_BUFSIZ_256)) {
        return false;
    }

    if (!xdr_int(xdrs, &q->procLimit)) {
        return false;
    }

    if (!xdr_string(xdrs, &q->admins, LL_BUFSIZ_256)) {
        return false;
    }

    if (!xdr_string(xdrs, &q->preCmd, LL_BUFSIZ_256)) {
        return false;
    }

    if (!xdr_string(xdrs, &q->postCmd, LL_BUFSIZ_256)) {
        return false;
    }

    if (!xdr_string(xdrs, &q->requeueEValues, LL_BUFSIZ_256)) {
        return false;
    }

    if (!xdr_int(xdrs, &q->hostJobLimit)) {
        return false;
    }

    if (!xdr_string(xdrs, &q->resReq, LL_BUFSIZ_512)) {
        return false;
    }

    if (!xdr_int(xdrs, &q->numRESERVE)) {
        return false;
    }

    if (!xdr_int(xdrs, &q->slotHoldTime)) {
        return false;
    }

    if (!xdr_string(xdrs, &q->resumeCond, LL_BUFSIZ_512)) {
        return false;
    }

    if (!xdr_string(xdrs, &q->stopCond, LL_BUFSIZ_512)) {
        return false;
    }

    if (!xdr_string(xdrs, &q->jobStarter, LL_BUFSIZ_256)) {
        return false;
    }

    if (!xdr_string(xdrs, &q->suspendActCmd, LL_BUFSIZ_256)) {
        return false;
    }

    if (!xdr_string(xdrs, &q->resumeActCmd, LL_BUFSIZ_256)) {
        return false;
    }

    if (!xdr_string(xdrs, &q->terminateActCmd, LL_BUFSIZ_256)) {
        return false;
    }

    /* signal map */
    for (j = 0; j < LSB_SIG_NUM; j++) {
        if (!xdr_int(xdrs, &q->sigMap[j])) {
            return false;
        }
    }

    /* checkpoint info */
    if (!xdr_string(xdrs, &q->chkpntDir, LL_BUFSIZ_256)) {
        return false;
    }

    if (!xdr_int(xdrs, &q->chkpntPeriod)) {
        return false;
    }

    /* default limits */
    for (i = 0; i < LSF_RLIM_NLIMITS; i++) {
        if (!xdr_int(xdrs, &q->defLimits[i])) {
            return false;
        }
    }

    if (!xdr_int(xdrs, &q->minProcLimit)) {
        return false;
    }

    if (!xdr_int(xdrs, &q->defProcLimit)) {
        return false;
    }

    return true;
}

bool_t xdr_infoReq(XDR *xdrs, struct infoReq *info_req, void *unused)
{
    (void)unused;

    if (!xdr_int(xdrs, &info_req->options))
        return false;

    if (!xdr_int(xdrs, &info_req->numNames))
        return false;

    if (info_req->numNames < 0)
        return false;

    switch (xdrs->x_op) {
    case XDR_ENCODE:
        for (int i = 0; i < info_req->numNames; i++) {
            if (!xdr_string(xdrs, &info_req->names[i], LL_BUFSIZ_64))
                return false;
        }
        break;

    case XDR_DECODE: {
        int n = info_req->numNames;

        info_req->names = calloc((size_t)n, sizeof(char *));
        if (info_req->names == NULL)
                return false;
        for (int i = 0; i < info_req->numNames; i++) {
            if (!xdr_string(xdrs, &info_req->names[i], LL_BUFSIZ_64))
                return false;
        }
        break;
    }

    case XDR_FREE:
        // do later
        return true;

    default:
        return false;
    }

    if (!xdr_string(xdrs, &info_req->resReq, LL_BUFSIZ_64))
        return false;

    return true;
}


bool_t xdr_hostDataReply(XDR *xdrs, struct hostDataReply *hostDataReply, void *)
{
    int i, hostCount;
    struct hostInfoEnt *hInfoTmp;
    char *sp;

    static struct hostInfoEnt *hInfo = NULL;
    static int curNumHInfo = 0;
    static char *mem = NULL;
    static float *loadSched = NULL, *loadStop = NULL, *load = NULL;
    static float *realLoad = NULL;
    static int nIdx = 0, *busySched = NULL, *busyStop = NULL;

    if (!(xdr_int(xdrs, &(hostDataReply->numHosts)) &&
          xdr_int(xdrs, &(hostDataReply->badHost)) &&
          xdr_int(xdrs, &(hostDataReply->nIdx))))
        return false;

    if (xdrs->x_op == XDR_DECODE) {
        hostCount = hostDataReply->numHosts;

        if (curNumHInfo < hostCount) {
            hInfoTmp = (struct hostInfoEnt *) calloc(
                hostCount, sizeof(struct hostInfoEnt));
            if (hInfoTmp == NULL) {
                lsberrno = LSBE_NO_MEM;
                return false;
            }
            if (!(sp = malloc(hostCount * (MAXHOSTNAMELEN + MAXLINELEN)))) {
                FREEUP(hInfoTmp);
                lsberrno = LSBE_NO_MEM;
                return false;
            }
            if (hInfo != NULL) {
                FREEUP(mem);
                FREEUP(hInfo);
            }
            hInfo = hInfoTmp;
            curNumHInfo = hostCount;
            mem = sp;
            for (i = 0; i < hostCount; i++) {
                hInfo[i].host = sp;
                sp += MAXHOSTNAMELEN;
                hInfo[i].windows = sp;
                sp += MAXLINELEN;
            }
        }
        hostDataReply->hosts = hInfo;

        if (hostDataReply->nIdx * hostDataReply->numHosts > nIdx) {
            if (allocLoadIdx(&loadSched, &loadStop, &nIdx,
                             hostDataReply->nIdx * hostDataReply->numHosts) ==
                -1)
                return false;
        }
        FREEUP(load);
        FREEUP(realLoad);
        FREEUP(busySched);
        FREEUP(busyStop);

        if (hostDataReply->numHosts > 0 &&
            (realLoad = (float *) malloc(hostDataReply->nIdx *
                                         hostDataReply->numHosts *
                                         sizeof(float))) == NULL)
            return false;

        if (hostDataReply->numHosts > 0 &&
            (load = (float *) malloc(hostDataReply->nIdx *
                                     hostDataReply->numHosts *
                                     sizeof(float))) == NULL)
            return false;
        if (hostDataReply->numHosts > 0 &&
            (busySched =
                 (int *) malloc(GET_INTNUM(hostDataReply->nIdx) *
                                hostDataReply->numHosts * sizeof(int))) == NULL)
            return false;
        if (hostDataReply->numHosts > 0 &&
            (busyStop =
                 (int *) malloc(GET_INTNUM(hostDataReply->nIdx) *
                                hostDataReply->numHosts * sizeof(int))) == NULL)
            return false;
    }

    for (i = 0; i < hostDataReply->numHosts; i++) {
        if (xdrs->x_op == XDR_DECODE) {
            hostDataReply->hosts[i].loadSched =
                loadSched + (i * hostDataReply->nIdx);
            hostDataReply->hosts[i].loadStop =
                loadStop + (i * hostDataReply->nIdx);
            hostDataReply->hosts[i].realLoad =
                realLoad + (i * hostDataReply->nIdx);
            hostDataReply->hosts[i].load = load + (i * hostDataReply->nIdx);
            hostDataReply->hosts[i].busySched =
                busySched + (i * GET_INTNUM(hostDataReply->nIdx));
            hostDataReply->hosts[i].busyStop =
                busyStop + (i * GET_INTNUM(hostDataReply->nIdx));
        }

        if (!xdr_array_element(xdrs, &(hostDataReply->hosts[i]),
                               &hostDataReply->nIdx, xdr_hostInfoEnt))
            return false;
    }

    if (!xdr_int(xdrs, &(hostDataReply->flag))) {
        return false;
    }

    if (xdrs->x_op == XDR_DECODE) {
        if (hostDataReply->flag & LOAD_REPLY_SHARED_RESOURCE)
            lsbSharedResConfigured_ = true;
    }

    return true;
}

bool_t xdr_hostInfoEnt(XDR *xdrs, struct hostInfoEnt *hostInfoEnt, void *ctx)
{
    char *sp = hostInfoEnt->host;
    char *wp = hostInfoEnt->windows;
    int i;

    if (xdrs->x_op == XDR_DECODE) {
        sp[0] = '\0';
        wp[0] = '\0';
    }

    if (!(xdr_string(xdrs, &sp, MAXHOSTNAMELEN) &&
          xdr_float(xdrs, &hostInfoEnt->cpuFactor) &&
          xdr_string(xdrs, &wp, MAXLINELEN) &&
          xdr_int(xdrs, &hostInfoEnt->userJobLimit) &&
          xdr_int(xdrs, &hostInfoEnt->maxJobs) &&
          xdr_int(xdrs, &hostInfoEnt->numJobs) &&
          xdr_int(xdrs, &hostInfoEnt->numRUN) &&
          xdr_int(xdrs, &hostInfoEnt->numSSUSP) &&
          xdr_int(xdrs, &hostInfoEnt->numUSUSP) &&
          xdr_int(xdrs, &hostInfoEnt->hStatus) &&
          xdr_int(xdrs, &hostInfoEnt->attr) &&
          xdr_int(xdrs, &hostInfoEnt->mig)))
        return false;

    int n = *(int *) ctx;
    hostInfoEnt->nIdx = n;

    for (i = 0; i < GET_INTNUM(n); i++) {
        if (!xdr_int(xdrs, &hostInfoEnt->busySched[i]) ||
            !xdr_int(xdrs, &hostInfoEnt->busyStop[i]))
            return false;
    }
    for (i = 0; i < n; i++) {
        if (!xdr_float(xdrs, &hostInfoEnt->loadSched[i]) ||
            !xdr_float(xdrs, &hostInfoEnt->loadStop[i]))
            return false;
    }

    for (i = 0; i < n; i++) {
        if (!xdr_float(xdrs, &hostInfoEnt->realLoad[i]))
            return false;
        if (!xdr_float(xdrs, &hostInfoEnt->load[i]))
            return false;
    }
    if (!xdr_int(xdrs, &hostInfoEnt->numRESERVE))
        return false;

    return true;
}

bool_t xdr_userInfoReply(XDR *xdrs, struct userInfoReply *userInfoReply, void *)
{
    int i;
    struct userInfoEnt *uInfoTmp;
    char *sp;

    static struct userInfoEnt *uInfo = NULL;
    static int curNumUInfo = 0;
    static char *mem = NULL;

    if (!(xdr_int(xdrs, &(userInfoReply->numUsers)) &&
          xdr_int(xdrs, &(userInfoReply->badUser))))
        return false;

    if (xdrs->x_op == XDR_DECODE) {
        if (curNumUInfo < userInfoReply->numUsers) {
            uInfoTmp = (struct userInfoEnt *) calloc(
                userInfoReply->numUsers, sizeof(struct userInfoEnt));
            if (uInfoTmp == NULL) {
                lsberrno = LSBE_NO_MEM;
                return false;
            }
            if (!(sp = malloc(userInfoReply->numUsers * LL_BUFSIZ_32))) {
                free(uInfoTmp);
                lsberrno = LSBE_NO_MEM;
                return false;
            }
            if (uInfo != NULL) {
                free(mem);
                free(uInfo);
            }
            uInfo = uInfoTmp;
            curNumUInfo = userInfoReply->numUsers;
            mem = sp;
            for (i = 0; i < userInfoReply->numUsers; i++) {
                uInfo[i].user = sp;
                sp += LL_BUFSIZ_32;
            }
        }
        userInfoReply->users = uInfo;
    }

    for (i = 0; i < userInfoReply->numUsers; i++) {
        if (!xdr_array_element(xdrs, &(userInfoReply->users[i]), NULL,
                               xdr_userInfoEnt)) {
            return false;
        }
    }

    return true;
}

bool_t xdr_userInfoEnt(XDR *xdrs, struct userInfoEnt *userInfoEnt, void *)
{
    char *sp;

    sp = userInfoEnt->user;
    if (xdrs->x_op == XDR_DECODE)
        sp[0] = '\0';

    if (!(xdr_string(xdrs, &sp, LL_BUFSIZ_32) &&
          xdr_float(xdrs, &userInfoEnt->procJobLimit) &&
          xdr_int(xdrs, &userInfoEnt->maxJobs) &&
          xdr_int(xdrs, &userInfoEnt->numStartJobs)))

        return false;

    if (!(xdr_int(xdrs, &userInfoEnt->numJobs) &&
          xdr_int(xdrs, &userInfoEnt->numPEND) &&
          xdr_int(xdrs, &userInfoEnt->numRUN) &&
          xdr_int(xdrs, &userInfoEnt->numSSUSP) &&
          xdr_int(xdrs, &userInfoEnt->numUSUSP)))
        return false;

    if (!xdr_int(xdrs, &userInfoEnt->numRESERVE))
        return false;

    return true;
}

bool_t xdr_jobPeekReq(XDR *xdrs, struct jobPeekReq *jobPeekReq, void *)
{
    int jobArrId, jobArrElemId;

    if (xdrs->x_op == XDR_ENCODE) {
        jobId64To32(jobPeekReq->jobId, &jobArrId, &jobArrElemId);
    }
    if (!xdr_int(xdrs, &jobArrId))
        return false;

    if (!xdr_int(xdrs, &jobArrElemId)) {
        return false;
    }

    if (xdrs->x_op == XDR_DECODE) {
        jobId32To64(&jobPeekReq->jobId, jobArrId, jobArrElemId);
    }

    return true;
}

bool_t xdr_jobPeekReply(XDR *xdrs, struct jobPeekReply *jobPeekReply, void *)
{
    static char outFile[MAXFILENAMELEN];

    if (xdrs->x_op == XDR_DECODE) {
        outFile[0] = '\0';
        jobPeekReply->outFile = outFile;
    }

    if (!xdr_string(xdrs, &(jobPeekReply->outFile), MAXFILENAMELEN))
        return false;

    if (!(xdr_var_string(xdrs, &jobPeekReply->pSpoolDir))) {
        return false;
    }

    return true;
}

bool_t xdr_groupInfoReply(XDR *xdrs, struct groupInfoReply *groupInfoReply,
                          void *)
{
    int i;

    if (xdrs->x_op == XDR_FREE) {
        for (i = 0; i < groupInfoReply->numGroups; i++) {
            FREEUP(groupInfoReply->groups[i].group);
            FREEUP(groupInfoReply->groups[i].memberList);
        }
        FREEUP(groupInfoReply->groups);
        groupInfoReply->numGroups = 0;
        return true;
    }

    if (xdrs->x_op == XDR_DECODE) {
        groupInfoReply->numGroups = 0;
        groupInfoReply->groups = NULL;
    }

    if (!xdr_int(xdrs, &(groupInfoReply->numGroups)))
        return false;

    if (xdrs->x_op == XDR_DECODE && groupInfoReply->numGroups != 0) {
        groupInfoReply->groups = (struct groupInfoEnt *) calloc(
            groupInfoReply->numGroups, sizeof(struct groupInfoEnt));
        if (groupInfoReply->groups == NULL)
            return false;
    }

    for (i = 0; i < groupInfoReply->numGroups; i++) {
        if (!xdr_array_element(xdrs, &(groupInfoReply->groups[i]), NULL,
                               xdr_groupInfoEnt))
            return false;
    }

    if (xdrs->x_op == XDR_FREE)
        FREEUP(groupInfoReply->groups);

    return true;
}

bool_t xdr_groupInfoEnt(XDR *xdrs, struct groupInfoEnt *gInfo, void *)
{
    if (xdrs->x_op == XDR_DECODE) {
        gInfo->group = NULL;
        gInfo->memberList = NULL;
    }

    if (!xdr_var_string(xdrs, &(gInfo->group)) ||
        !xdr_var_string(xdrs, &(gInfo->memberList)))
        return false;

    if (xdrs->x_op == XDR_FREE) {
        FREEUP(gInfo->group);
        FREEUP(gInfo->memberList);
    }

    return true;
}

bool_t xdr_controlReq(XDR *xdrs, struct controlReq *controlReq, void *)
{
    static char *sp = NULL;
    static int first = true;

    if (xdrs->x_op == XDR_DECODE) {
        if (first == true) {
            sp = (char *) malloc(MAXHOSTNAMELEN);
            if (sp == NULL)
                return false;
            first = false;
        }
        controlReq->name = sp;
        sp[0] = '\0';
    } else {
        sp = controlReq->name;
    }
    if (!(xdr_int(xdrs, &controlReq->opCode) &&
          xdr_string(xdrs, &sp, MAXHOSTNAMELEN)))
        return false;

    return true;
}

bool_t xdr_jobSwitchReq(XDR *xdrs, struct jobSwitchReq *jobSwitchReq, void *)
{
    char *sp;
    int jobArrId, jobArrElemId;

    sp = jobSwitchReq->queue;
    if (xdrs->x_op == XDR_DECODE)
        sp[0] = '\0';

    if (xdrs->x_op == XDR_ENCODE) {
        jobId64To32(jobSwitchReq->jobId, &jobArrId, &jobArrElemId);
    }
    if (!(xdr_int(xdrs, &jobArrId) && xdr_string(xdrs, &sp, LL_BUFSIZ_32)))
        return false;
    if (!xdr_int(xdrs, &jobArrElemId)) {
        return false;
    }

    if (xdrs->x_op == XDR_DECODE) {
        jobId32To64(&jobSwitchReq->jobId, jobArrId, jobArrElemId);
    }

    return true;
}

bool_t xdr_jobMoveReq(XDR *xdrs, struct jobMoveReq *jobMoveReq, void *)
{
    int jobArrId, jobArrElemId;

    if (xdrs->x_op == XDR_ENCODE) {
        jobId64To32(jobMoveReq->jobId, &jobArrId, &jobArrElemId);
    }
    if (!(xdr_int(xdrs, &(jobMoveReq->opCode)) && xdr_int(xdrs, &jobArrId) &&
          xdr_int(xdrs, &(jobMoveReq->position))))
        return false;
    if (!xdr_int(xdrs, &jobArrElemId)) {
        return false;
    }

    if (xdrs->x_op == XDR_DECODE) {
        jobId32To64(&jobMoveReq->jobId, jobArrId, jobArrElemId);
    }

    return true;
}

bool_t xdr_migReq(XDR *xdrs, struct migReq *req, void *)
{
    char *sp;
    int i;
    int jobArrId, jobArrElemId;

    if (xdrs->x_op == XDR_ENCODE) {
        jobId64To32(req->jobId, &jobArrId, &jobArrElemId);
    }
    if (!(xdr_int(xdrs, &jobArrId) && xdr_int(xdrs, &req->options)))
        return false;

    if (!xdr_int(xdrs, &req->numAskedHosts))
        return false;

    if (xdrs->x_op == XDR_DECODE && req->numAskedHosts > 0) {
        req->askedHosts = (char **) calloc(req->numAskedHosts, sizeof(char *));
        if (req->askedHosts == NULL)
            return false;
        for (i = 0; i < req->numAskedHosts; i++) {
            req->askedHosts[i] = calloc(1, MAXHOSTNAMELEN);
            if (req->askedHosts[i] == NULL) {
                while (i--)
                    free(req->askedHosts[i]);
                free(req->askedHosts);
                return false;
            }
        }
    }

    for (i = 0; i < req->numAskedHosts; i++) {
        sp = req->askedHosts[i];
        if (xdrs->x_op == XDR_DECODE)
            sp[0] = '\0';
        if (!xdr_string(xdrs, &sp, MAXHOSTNAMELEN)) {
            for (i = 0; i < req->numAskedHosts; i++)
                free(req->askedHosts[i]);
            free(req->askedHosts);
            return false;
        }
    }
    if (!xdr_int(xdrs, &jobArrElemId)) {
        return false;
    }

    if (xdrs->x_op == XDR_DECODE) {
        jobId32To64(&req->jobId, jobArrId, jobArrElemId);
    }

    return true;
}

bool_t xdr_xFile(XDR *xdrs, struct xFile *xf, void *)
{
    char *sp;

    if (xdrs->x_op == XDR_DECODE) {
        xf->subFn[0] = '\0';
        xf->execFn[0] = '\0';
    }
    sp = xf->subFn;

    if (!xdr_string(xdrs, &sp, MAXFILENAMELEN))
        return false;

    sp = xf->execFn;
    if (!xdr_string(xdrs, &sp, MAXFILENAMELEN))
        return false;

    if (!xdr_int(xdrs, &xf->options))
        return false;

    return true;
}

static int allocLoadIdx(float **loadSched, float **loadStop, int *outSize,
                        int size)
{
    if (*loadSched)
        free(*loadSched);
    if (*loadStop)
        free(*loadStop);

    *outSize = 0;

    if ((*loadSched = (float *) calloc(size, sizeof(float))) == NULL)
        return -1;

    if ((*loadStop = (float *) calloc(size, sizeof(float))) == NULL)
        return -1;

    *outSize = size;
    return 0;
}

static bool_t
xdr_lsbShareResourceInstance(XDR *xdrs,
                             struct lsbSharedResourceInstance *instance, void *)
{
    if (xdrs->x_op == XDR_DECODE) {
        instance->totalValue = NULL;
        instance->rsvValue = NULL;
        instance->hostList = NULL;
        instance->nHosts = 0;
    }
    if (!(xdr_var_string(xdrs, &instance->totalValue) &&
          xdr_var_string(xdrs, &instance->rsvValue) &&
          xdr_int(xdrs, &instance->nHosts)))
        return false;

    if (xdrs->x_op == XDR_DECODE && instance->nHosts > 0) {
        if ((instance->hostList =
                 (char **) malloc(instance->nHosts * sizeof(char *))) == NULL) {
            lserrno = LSE_MALLOC;
            return false;
        }
    }
    if (!xdr_array_string(xdrs, instance->hostList, MAXHOSTNAMELEN,
                          instance->nHosts)) {
        if (xdrs->x_op == XDR_DECODE) {
            FREEUP(instance->hostList);
            instance->nHosts = 0;
        }
        return false;
    }
    if (xdrs->x_op == XDR_FREE && instance->nHosts > 0) {
        FREEUP(instance->hostList);
        instance->nHosts = 0;
    }

    return true;
}

static bool_t xdr_lsbSharedResourceInfo(
    XDR *xdrs, struct lsbSharedResourceInfo *lsbSharedResourceInfo, void *)
{
    int i, status;

    if (xdrs->x_op == XDR_DECODE) {
        lsbSharedResourceInfo->resourceName = NULL;
        lsbSharedResourceInfo->instances = NULL;
        lsbSharedResourceInfo->nInstances = 0;
    }
    if (!(xdr_var_string(xdrs, &lsbSharedResourceInfo->resourceName) &&
          xdr_int(xdrs, &lsbSharedResourceInfo->nInstances)))
        return false;

    if (xdrs->x_op == XDR_DECODE && lsbSharedResourceInfo->nInstances > 0) {
        if ((lsbSharedResourceInfo->instances =
                 (struct lsbSharedResourceInstance *) malloc(
                     lsbSharedResourceInfo->nInstances *
                     sizeof(struct lsbSharedResourceInstance))) == NULL) {
            lserrno = LSE_MALLOC;
            return false;
        }
    }
    for (i = 0; i < lsbSharedResourceInfo->nInstances; i++) {
        status = xdr_array_element(xdrs, &lsbSharedResourceInfo->instances[i],
                                   NULL, xdr_lsbShareResourceInstance);
        if (!status) {
            lsbSharedResourceInfo->nInstances = i;
            return false;
        }
    }
    if (xdrs->x_op == XDR_FREE && lsbSharedResourceInfo->nInstances > 0) {
        FREEUP(lsbSharedResourceInfo->instances);
        lsbSharedResourceInfo->nInstances = 0;
    }
    return true;
}

bool_t xdr_lsbShareResourceInfoReply(
    XDR *xdrs, struct lsbShareResourceInfoReply *lsbShareResourceInfoReply,
    void *)
{
    int i, status;

    if (xdrs->x_op == XDR_DECODE) {
        lsbShareResourceInfoReply->numResources = 0;
        lsbShareResourceInfoReply->resources = NULL;
    }
    if (!(xdr_int(xdrs, &lsbShareResourceInfoReply->numResources) &&
          xdr_int(xdrs, &lsbShareResourceInfoReply->badResource)))
        return false;

    if (xdrs->x_op == XDR_DECODE &&
        lsbShareResourceInfoReply->numResources > 0) {
        if ((lsbShareResourceInfoReply->resources =
                 (struct lsbSharedResourceInfo *) malloc(
                     lsbShareResourceInfoReply->numResources *
                     sizeof(struct lsbSharedResourceInfo))) == NULL) {
            lserrno = LSE_MALLOC;
            return false;
        }
    }
    for (i = 0; i < lsbShareResourceInfoReply->numResources; i++) {
        status =
            xdr_array_element(xdrs, &lsbShareResourceInfoReply->resources[i],
                              NULL, xdr_lsbSharedResourceInfo);
        if (!status) {
            lsbShareResourceInfoReply->numResources = i;

            return false;
        }
    }
    if (xdrs->x_op == XDR_FREE && lsbShareResourceInfoReply->numResources > 0) {
        FREEUP(lsbShareResourceInfoReply->resources);
        lsbShareResourceInfoReply->numResources = 0;
    }
    return true;
}

bool_t xdr_runJobReq(XDR *xdrs, struct runJobRequest *runJobRequest, void *)
{
    int i;
    int jobArrId, jobArrElemId;

    if (xdrs->x_op == XDR_ENCODE) {
        jobId64To32(runJobRequest->jobId, &jobArrId, &jobArrElemId);
    }
    if (!xdr_int(xdrs, &(runJobRequest->numHosts)) ||
        !xdr_int(xdrs, &jobArrId) ||
        !xdr_int(xdrs, &(runJobRequest->options))) {
        return false;
    }

    if (xdrs->x_op == XDR_DECODE) {
        runJobRequest->hostname =
            (char **) calloc(runJobRequest->numHosts, sizeof(char *));
        if (runJobRequest->hostname == NULL)
            return false;
    }

    for (i = 0; i < runJobRequest->numHosts; i++) {
        if (!xdr_var_string(xdrs, &(runJobRequest->hostname[i]))) {
            return false;
        }
    }

    if (!xdr_int(xdrs, &jobArrElemId)) {
        return false;
    }

    if (xdrs->x_op == XDR_DECODE) {
        jobId32To64(&runJobRequest->jobId, jobArrId, jobArrElemId);
    }
    if (xdrs->x_op == XDR_FREE) {
        runJobRequest->numHosts = 0;
        FREEUP(runJobRequest->hostname);
    }

    return true;
}

bool_t xdr_jobAttrReq(XDR *xdrs, struct jobAttrInfoEnt *jobAttr, void *)
{
    char *sp = jobAttr->hostname;

    int jobArrId, jobArrElemId;

    if (xdrs->x_op == XDR_ENCODE) {
        jobId64To32(jobAttr->jobId, &jobArrId, &jobArrElemId);
    }
    if (!xdr_int(xdrs, &jobArrId)) {
        return false;
    }
    if (!(xdr_u_short(xdrs, &(jobAttr->port)) &&
          xdr_string(xdrs, &sp, MAXHOSTNAMELEN))) {
        return false;
    }

    if (!xdr_int(xdrs, &jobArrElemId)) {
        return false;
    }

    if (xdrs->x_op == XDR_DECODE) {
        jobId32To64(&jobAttr->jobId, jobArrId, jobArrElemId);
    }

    return true;
}


bool_t xdr_wire_sbd_register(XDR *xdrs, struct wire_sbd_register *msg)
{
    if (msg == NULL)
        return false;

    return xdr_opaque(xdrs, msg->hostname, sizeof(msg->hostname));
}

bool_t xdr_wire_job_sig_req(XDR *xdrs, struct wire_job_sig_req *p)
{
    if (!xdr_int64_t(xdrs, &p->job_id))
        return false;
    if (!xdr_int32_t(xdrs, &p->sig))
        return false;
    if (!xdr_int32_t(xdrs, &p->flags))
        return false;

    return true;
}

bool_t xdr_wire_job_sig_reply(XDR *xdrs, struct wire_job_sig_reply *p)
{
    if (!xdr_int64_t(xdrs, &p->job_id))
        return false;
    if (!xdr_int32_t(xdrs, &p->rc))
        return false;
    if (!xdr_int32_t(xdrs, &p->detail_errno))
        return false;

    return true;
}
