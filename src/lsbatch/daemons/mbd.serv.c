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
#include "lsbatch/daemons/sbatchd.h"

static unsigned int msgcnt = 0;
extern int numLsbUsable;
extern char *env_dir;
extern int lsb_CheckMode;
extern int lsb_CheckError;

extern char *jgrpNodeParentPath(struct jgTreeNode *);
static int packJgrpInfo(struct jgTreeNode *, int, char **, int, int);
static int packJobInfo(struct jData *, int, char **, int, int, int);
static void initSubmit(int *, struct submitReq *, struct submitMbdReply *);
static int sendBack(int, struct submitReq *, struct submitMbdReply *, int);
static void addPendSigEvent(struct sbdNode *sbdPtr);
static void freeJobHead(struct jobInfoHead *);
static void freeJobInfoReply(struct jobInfoReply *);
static void freeShareResourceInfoReply(struct lsbShareResourceInfoReply *);
static int xdrsize_QueueInfoReply(struct queueInfoReply *);

extern void closeSession(int);

int do_submitReq(XDR *xdrs, int chfd, struct sockaddr_in *from, char *hostName,
                 struct packet_header *reqHdr, struct sockaddr_in *laddr,
                 struct lsfAuth *auth, int *schedule, int dispatch,
                 struct jData **jobData)
{
    static char fname[] = "do_submitReq";
    static struct submitMbdReply submitReply;
    static int first = TRUE;
    static struct submitReq subReq;
    int reply;

    if (logclass & (LC_TRACE | LC_EXEC | LC_COMM))
        ls_syslog(LOG_DEBUG, "%s: Entering this routine...; host=%s, socket=%d",
                  fname, hostName, chan_sock(chfd));

    initSubmit(&first, &subReq, &submitReply);

    if (!xdr_submitReq(xdrs, &subReq, reqHdr)) {
        reply = LSBE_XDR;
        ls_syslog(LOG_ERR, "%s", __func__, "xdr_submitReq");
        goto sendback;
    }

    if (!(subReq.options & SUB_RLIMIT_UNIT_IS_KB)) {
        convertRLimit(subReq.rLimits, 1);
    }

    reply =
        newJob(&subReq, &submitReply, chfd, auth, schedule, dispatch, jobData);
sendback:
    if (reply != 0 || submitReply.jobId <= 0) {
        if (logclass & (LC_TRACE | LC_EXEC)) {
            ls_syslog(
                LOG_DEBUG,
                "Job submission Failed due reason <%d> job <%d > JobName<%s> "
                "queue <%s> reqReq <%s> hostSpec <%s>  subHomeDir <%s> inFile "
                "<%s>  outFile <%s> errFile<%s> command <%s> inFileSpool <%s> "
                "commandSpool <%s> chkpntDir <%s> jobFile <%s> fromHost <%s>  "
                "cwd <%s> preExecCmd <%s>  mailUser <%s> projectName <%s> "
                "loginShell <%s> schedHostType <%s> numAskedHosts <%d> nxf "
                "<%d>  ",
                reply, (nextJobId - 1), subReq.jobName, subReq.queue,
                subReq.resReq, subReq.hostSpec, subReq.subHomeDir,
                subReq.inFile, subReq.outFile, subReq.errFile, subReq.command,
                subReq.inFileSpool, subReq.commandSpool, subReq.chkpntDir,
                subReq.jobFile, subReq.fromHost, subReq.cwd, subReq.preExecCmd,
                subReq.mailUser, subReq.projectName, subReq.loginShell,
                subReq.schedHostType, subReq.numAskedHosts, subReq.nxf);
        }
    }
    if (logclass & (LC_TRACE | LC_EXEC)) {
        ls_syslog(
            LOG_DEBUG,
            "Job submission before sendBack reply <%d> job <%d > JobName <%s> "
            "queue <%s> reqReq <%s> hostSpec <%s> subHomeDir <%s> inFile <%s>  "
            "outFile <%s> errFile<%s> command <%s> inFileSpool <%s> "
            "commandSpool <%s> chkpntDir <%s>  jobFile <%s> fromHost <%s>  cwd "
            "<%s> preExecCmd <%s>  mailUser <%s> projectName <%s> loginShell "
            "<%s> schedHostType <%s>  numAskedHosts <%d> nxf <%d>  ",
            reply, (nextJobId - 1), subReq.jobName, subReq.queue, subReq.resReq,
            subReq.hostSpec, subReq.subHomeDir, subReq.inFile, subReq.outFile,
            subReq.errFile, subReq.command, subReq.inFileSpool,
            subReq.commandSpool, subReq.chkpntDir, subReq.jobFile,
            subReq.fromHost, subReq.cwd, subReq.preExecCmd, subReq.mailUser,
            subReq.projectName, subReq.loginShell, subReq.schedHostType,
            subReq.numAskedHosts, subReq.nxf);
    }

    if (sendBack(reply, &subReq, &submitReply, chfd) < 0) {
        return -1;
    }
    return 0;
}

int checkUseSelectJgrps(struct packet_header *reqHdr, struct jobInfoReq *req)
{
    if (req->jobId != 0 && !(req->options & JGRP_ARRAY_INFO))
        return FALSE;

    if ((req->jobName[0] == '\0') && !(req->options & JGRP_ARRAY_INFO))
        return FALSE;

    return TRUE;
}

int do_jobInfoReq(XDR *xdrs, int chfd, struct sockaddr_in *from,
                  struct packet_header *reqHdr, int schedule)
{
    static char fname[] = "do_jobInfoReq";
    char *reply_buf = NULL;
    char *buf = NULL;
    XDR xdrs2;
    struct jobInfoReq jobInfoReq;
    struct jobInfoHead jobInfoHead;
    int reply = 0;
    int i, len, listSize = 0;
    struct packet_header replyHdr;
    struct nodeList *jgrplist = NULL;
    struct jData **joblist = NULL;
    int selectJgrpsFlag = FALSE;

    if (logclass & (LC_TRACE | LC_COMM))
        ls_syslog(LOG_DEBUG, "%s: Entering this routine...; channel=%d", fname,
                  chfd);

    jobInfoHead.hostNames = NULL;
    jobInfoHead.jobIds = NULL;

    memset(&jobInfoReq, 0, sizeof(struct jobInfoReq));
    if (!xdr_jobInfoReq(xdrs, &jobInfoReq, reqHdr)) {
        reply = LSBE_XDR;
        ls_syslog(LOG_ERR, "%s", __func__, "xdr_jobInfoReq");
    }
    if (jobInfoReq.host[0] != '\0' && getHGrpData(jobInfoReq.host) == NULL &&
        !is_valid_host(jobInfoReq.host) &&
        (strcmp(jobInfoReq.host, LOST_AND_FOUND) != 0))
        reply = LSBE_BAD_HOST;
    else {
        if ((selectJgrpsFlag = checkUseSelectJgrps(reqHdr, &jobInfoReq)) ==
            TRUE) {
            reply = selectJgrps(&jobInfoReq, (void **) &jgrplist, &listSize);
        } else {
            reply = selectJobs(&jobInfoReq, &joblist, &listSize);

            jgrplist =
                (struct nodeList *) calloc(listSize, sizeof(struct nodeList));
            for (i = 0; i < listSize; i++) {
                jgrplist[i].info = (void *) joblist[i];
                jgrplist[i].isJData = TRUE;
            }
            FREEUP(joblist);
        }
    }

    xdr_lsffree(xdr_jobInfoReq, (char *) &jobInfoReq, reqHdr);

    jobInfoHead.numJobs = listSize;

    if (jobInfoHead.numJobs > 0)
        jobInfoHead.jobIds =
            (int64_t *) my_calloc(listSize, sizeof(int64_t), fname);
    for (i = 0; i < listSize; i++) {
        if (!jgrplist[i].isJData)
            jobInfoHead.jobIds[i] = 0;
        else
            jobInfoHead.jobIds[i] = ((struct jData *) jgrplist[i].info)->jobId;
    }

    jobInfoHead.numHosts = 0;
    if (jobInfoReq.options & HOST_NAME) {
        jobInfoHead.hostNames =
            (char **) my_calloc(numofhosts, sizeof(char *), fname);
        for (i = 1; i <= numofhosts; i++)
            jobInfoHead.hostNames[i - 1] = hDataPtrTb[i]->host;

        jobInfoHead.numHosts = numofhosts;
    }

    len = sizeof(struct jobInfoHead) + jobInfoHead.numJobs * sizeof(int64_t) +
          jobInfoHead.numHosts * (sizeof(char *) + MAXHOSTNAMELEN) + 100;

    reply_buf = (char *) my_malloc(len, fname);
    xdrmem_create(&xdrs2, reply_buf, len, XDR_ENCODE);
    replyHdr.operation = reply;

    if (!xdr_encodeMsg(&xdrs2, (char *) &jobInfoHead, &replyHdr,
                       xdr_jobInfoHead, 0, NULL)) {
        FREEUP(reply_buf);
        freeJobHead(&jobInfoHead);
        ls_syslog(LOG_ERR, "%s", __func__, "xdr_encodeMsg", "jobInfoHead");
        xdr_destroy(&xdrs2);
        FREEUP(jgrplist);
        return -1;
    }
    len = XDR_GETPOS(&xdrs2);

    {
        if (chan_write(chfd, reply_buf, len) != len) {
            ls_syslog(LOG_ERR, "%s", __func__, "chan_write");
            FREEUP(reply_buf);
            freeJobHead(&jobInfoHead);
            FREEUP(jgrplist);
            xdr_destroy(&xdrs2);
            return -1;
        }
    }
    FREEUP(reply_buf);

    xdr_destroy(&xdrs2);
    freeJobHead(&jobInfoHead);
    if (reply != LSBE_NO_ERROR ||
        (jobInfoReq.options & (JOBID_ONLY | JOBID_ONLY_ALL))) {
        FREEUP(jgrplist);
        return 0;
    }

    for (i = 0; i < listSize; i++) {
        if (jgrplist[i].isJData &&
            ((len = packJobInfo((struct jData *) jgrplist[i].info,
                                listSize - 1 - i, &buf, schedule,
                                jobInfoReq.options, reqHdr->version)) < 0)) {
            ls_syslog(LOG_ERR, "%s", __func__, "packJobInfo");
            FREEUP(jgrplist);
            return -1;
        }
        if (!jgrplist[i].isJData &&
            ((len = packJgrpInfo((struct jgTreeNode *) jgrplist[i].info,
                                 listSize - 1 - i, &buf, schedule,
                                 reqHdr->version)) < 0)) {
            ls_syslog(LOG_ERR, "%s", __func__, "packJgrpInfo");
            FREEUP(jgrplist);
            return -1;
        }

        {
            if (chan_write(chfd, buf, len) != len) {
                ls_syslog(LOG_ERR, "%s", __func__, "chan_write");
                FREEUP(buf);
                FREEUP(jgrplist);
                return -1;
            }
        }
        FREEUP(buf);
    }
    FREEUP(jgrplist);

    chan_close(chfd);
    return 0;
}

static int packJgrpInfo(struct jgTreeNode *jgNode, int remain, char **replyBuf,
                        int schedule, int version)
{
    struct jobInfoReply jobInfoReply;
    struct submitReq jobBill;
    struct packet_header hdr;
    char *request_buf = NULL;
    XDR xdrs;
    int i, len;

    jobInfoReply.jobId = 0;
    jobInfoReply.numReasons = 0;
    jobInfoReply.reasons = 0;
    jobInfoReply.subreasons = 0;

    jobInfoReply.startTime = 0;
    jobInfoReply.predictedStartTime = 0;
    jobInfoReply.endTime = 0;

    jobInfoReply.cpuTime = 0;
    jobInfoReply.numToHosts = 0;
    jobInfoReply.jobBill = &jobBill;

    jobInfoReply.jType = jgNode->nodeType;
    jobInfoReply.parentGroup = jgrpNodeParentPath(jgNode);
    jobInfoReply.jName = jgNode->name;
    jobInfoReply.jobBill->jobName = jgNode->name;

    jobInfoReply.nIdx = 0;
    len = 4 * MAXLSFNAMELEN + 3 * MAXLINELEN + 2 * MAXHOSTNAMELEN +
          7 * MAXFILENAMELEN + sizeof(struct submitReq) +
          sizeof(struct jobInfoReply) + MAXLSFNAMELEN + 2 * MAXFILENAMELEN +
          sizeof(time_t) + strlen(jobInfoReply.jobBill->dependCond) + 100 +
          sizeof(int) + strlen(jobInfoReply.parentGroup) +
          strlen(jobInfoReply.jName) + (NUM_JGRP_COUNTERS + 1) * sizeof(int);

    len = (len * 4) / 4;

    request_buf = (char *) my_malloc(len, "packJgrpInfo");
    xdrmem_create(&xdrs, request_buf, len, XDR_ENCODE);

    hdr.version = version;
    if (!xdr_encodeMsg(&xdrs, (char *) &jobInfoReply, &hdr, xdr_jobInfoReply, 0,
                       NULL)) {
        ls_syslog(LOG_ERR, "%s", __func__, "xdr_encodeMsg", "jobInfoReply");
        xdr_destroy(&xdrs);
        FREEUP(request_buf);
        return -1;
    }
    i = XDR_GETPOS(&xdrs);
    *replyBuf = request_buf;
    xdr_destroy(&xdrs);
    return i;
}

static int jobInfoReplyXdrBufLen(struct jobInfoReply *jobInfoReplyPtr)
{
    struct jobInfoReply jobInfoReply = *jobInfoReplyPtr;
    int len = 0;
    int i;

    len = sizeof(struct jobInfoReply);
    len += jobInfoReply.numReasons * sizeof(int);
    len += jobInfoReply.numToHosts * ALIGNWORD_(MAXHOSTNAMELEN);
    len += 2 * jobInfoReply.nIdx * sizeof(float);
    len += ALIGNWORD_(MAXLSFNAMELEN);
    len += ALIGNWORD_(strlen(jobInfoReply.execUsername) + 1);
    len += ALIGNWORD_(strlen(jobInfoReply.execHome) + 1);
    len += ALIGNWORD_(strlen(jobInfoReply.execCwd) + 1);
    len += ALIGNWORD_(strlen(jobInfoReply.parentGroup) + 1);
    len += ALIGNWORD_(strlen(jobInfoReply.jName) + 1);

    len += sizeof(jobInfoReply.runRusage);
    len += jobInfoReply.runRusage.npgids * sizeof(int);

    len +=
        jobInfoReply.runRusage.npids * (NET_INTSIZE_ + sizeof(struct pidInfo));

    len += sizeof(struct submitReq);
    len += ALIGNWORD_(strlen(jobInfoReply.jobBill->jobName) + 1);
    len += ALIGNWORD_(strlen(jobInfoReply.jobBill->queue) + 1);
    for (i = 0; i < jobInfoReply.jobBill->numAskedHosts; i++)
        len += ALIGNWORD_(strlen(jobInfoReply.jobBill->askedHosts[i]) + 1);
    len += ALIGNWORD_(strlen(jobInfoReply.jobBill->resReq) + 1);
    len += ALIGNWORD_(MAXHOSTNAMELEN);
    len += ALIGNWORD_(strlen(jobInfoReply.jobBill->dependCond) + 1);
    len += ALIGNWORD_(MAXFILENAMELEN);
    len += ALIGNWORD_(MAXFILENAMELEN);
    len += ALIGNWORD_(MAXFILENAMELEN);
    len += ALIGNWORD_(MAXFILENAMELEN);
    len += ALIGNWORD_(strlen(jobInfoReply.jobBill->command) + 1);
    len += ALIGNWORD_(MAXFILENAMELEN);
    len += jobInfoReply.jobBill->nxf * sizeof(struct xFile);
    len += ALIGNWORD_(MAXFILENAMELEN);
    len += ALIGNWORD_(MAXHOSTNAMELEN);
    len += ALIGNWORD_(MAXFILENAMELEN);
    len += ALIGNWORD_(strlen(jobInfoReply.jobBill->preExecCmd) + 1);
    len += ALIGNWORD_(strlen(jobInfoReply.jobBill->mailUser) + 1);
    len += ALIGNWORD_(strlen(jobInfoReply.jobBill->projectName) + 1);
    len += ALIGNWORD_(strlen(jobInfoReply.jobBill->loginShell) + 1);
    len += ALIGNWORD_(strlen(jobInfoReply.jobBill->schedHostType) + 1);

    return len;
}

static int packJobInfo(struct jData *jobData, int remain, char **replyBuf,
                       int schedule, int options, int version)
{
    static char fname[] = "packJobInfo";
    struct jobInfoReply jobInfoReply;
    struct submitReq jobBill;
    struct packet_header hdr;
    char *request_buf = NULL;
    int *reasonTb = NULL;
    int *jReasonTb;
    XDR xdrs;
    int i, k, len;
    int svReason, *pkHReasonTb, *pkQReasonTb, *pkUReasonTb;
    float *loadSched = NULL, *loadStop = NULL;
    float *cpuFactor, one = 1.0;
    float cpuF;
    char fullName[MAXPATHLEN];
    struct jgTreeNode *jgNode;
    int job_numReasons;
    int *job_reasonTb;

    job_numReasons = jobData->numReasons;
    job_reasonTb = jobData->reasonTb;

    if (reasonTb == NULL) {
        reasonTb = (int *) my_calloc(numofhosts + 1, sizeof(int), fname);
        jReasonTb = (int *) my_calloc(numofhosts + 1, sizeof(int), fname);
    }

    jobInfoReply.jobId = jobData->jobId;
    jobInfoReply.startTime = jobData->startTime;
    jobInfoReply.predictedStartTime = jobData->predictedStartTime;
    jobInfoReply.endTime = jobData->endTime;
    jobInfoReply.cpuTime = jobData->cpuTime;
    jobInfoReply.numToHosts = jobData->numHostPtr;

    if (jobData->jStatus & JOB_STAT_UNKWN)
        jobInfoReply.status = JOB_STAT_UNKWN;
    else
        jobInfoReply.status = jobData->jStatus;
    jobInfoReply.status &= MASK_INT_JOB_STAT;

    if (IS_SUSP(jobData->jStatus))
        jobInfoReply.reasons = (~SUSP_MBD_LOCK & jobData->newReason);
    else
        jobInfoReply.reasons = jobData->newReason;
    jobInfoReply.subreasons = jobData->subreasons;
    jobInfoReply.reasonTb = reasonTb;

    if (logclass & LC_PEND)
        ls_syslog(LOG_DEBUG3,
                  "%s: job=%s, rs=%d, nrs=%d srs=%d, qNrs=%d, jNrs=%d nLsb=%d "
                  "nQU=%d mStage=%d",
                  fname, lsb_jobid2str(jobData->jobId), jobData->oldReason,
                  jobData->newReason, jobData->subreasons,
                  jobData->qPtr->numReasons, job_numReasons, numLsbUsable,
                  jobData->qPtr->numUsable, mSchedStage);

    if (IS_PEND(jobData->jStatus) && !(options & NO_PEND_REASONS)) {
        int useNewRs = TRUE;

        if (mSchedStage != 0 && !(jobData->processed & JOB_STAGE_DISP))
            useNewRs = FALSE;
        if (useNewRs) {
            svReason = jobData->newReason;
        } else {
            svReason = jobData->oldReason;
        }

        jobInfoReply.numReasons = 0;
        if (svReason) {
            jobInfoReply.reasonTb[jobInfoReply.numReasons++] = svReason;
        } else {
            pkHReasonTb = hReasonTb[0];
            pkQReasonTb = jobData->qPtr->reasonTb[0];
            pkUReasonTb = jobData->uPtr->reasonTb[0];
            for (i = 0; i <= numofhosts; i++) {
                if (i > 0 && jobData->numAskedPtr > 0 &&
                    jobData->askedOthPrio < 0)
                    jReasonTb[i] = PEND_HOST_USR_SPEC;
                else
                    jReasonTb[i] = 0;
            }

            for (i = 0; i < jobData->numAskedPtr; i++) {
                k = jobData->askedPtr[i].hData->hostId;
                jReasonTb[k] = 0;
            }
            for (i = 0; i < job_numReasons; i++) {
                if (job_reasonTb[i]) {
                    GET_HIGH(k, job_reasonTb[i]);
                    if (k > numofhosts || jReasonTb[k] == PEND_HOST_USR_SPEC)
                        continue;
                    jReasonTb[k] = job_reasonTb[i];
                }
            }
            if (svReason == 0) {
                if (logclass & LC_PEND)
                    ls_syslog(LOG_DEBUG2, "%s: Get h/u/q reasons", fname);
                k = 0;
                for (i = 1; i <= numofhosts; i++) {
                    if (jReasonTb[i] == PEND_HOST_USR_SPEC) {
                        continue;
                    }
                    if (pkQReasonTb[i] == PEND_HOST_QUE_MEMB)
                        continue;
                    if (!isHostQMember(hDataPtrTb[i], jobData->qPtr)) {
                        continue;
                    }
                    if (pkHReasonTb[i]) {
                        jobInfoReply.reasonTb[k] = pkHReasonTb[i];
                        PUT_HIGH(jobInfoReply.reasonTb[k], i);
                        k++;
                        if (mbd_debug && (logclass & LC_PEND))
                            ls_syslog(LOG_DEBUG2, "%s: hReasonTb[%d]=%d", fname,
                                      i, pkHReasonTb[i]);
                        continue;
                    }
                    if (pkQReasonTb[i]) {
                        jobInfoReply.reasonTb[k] = pkQReasonTb[i];
                        PUT_HIGH(jobInfoReply.reasonTb[k], i);
                        k++;
                        if (mbd_debug && (logclass & LC_PEND))
                            ls_syslog(LOG_DEBUG2, "%s: qReason[%d]=%d", fname,
                                      i, pkQReasonTb[i]);
                        continue;
                    }
                    if (pkUReasonTb[i]) {
                        jobInfoReply.reasonTb[k] = pkUReasonTb[i];
                        PUT_HIGH(jobInfoReply.reasonTb[k], i);
                        k++;
                        if (mbd_debug && (logclass & LC_PEND))
                            ls_syslog(LOG_DEBUG2, "%s: uReason[%d]=%d", fname,
                                      i, pkUReasonTb[i]);
                        continue;
                    }
                    if (jReasonTb[i]) {
                        jobInfoReply.reasonTb[k++] = jReasonTb[i];
                        if (mbd_debug && (logclass & LC_PEND)) {
                            int rs;
                            GET_LOW(rs, jReasonTb[i]);
                            ls_syslog(LOG_DEBUG2, "%s: jReason[%d]=%d", fname,
                                      i, rs);
                        }
                        continue;
                    }
                }

                if (jReasonTb[0] != 0) {
                    jobInfoReply.reasonTb[k++] = jReasonTb[0];
                    if (mbd_debug && (logclass & LC_PEND)) {
                        ls_syslog(LOG_DEBUG2, "%s: jReason[0]=%d", fname,
                                  jReasonTb[0]);
                    }
                }
            }
            jobInfoReply.numReasons = k;
        }
    } else
        jobInfoReply.numReasons = 0;

    if (jobData->numHostPtr > 0) {
        jobInfoReply.toHosts =
            (char **) my_calloc(jobData->numHostPtr, sizeof(char *), fname);
        for (i = 0; i < jobData->numHostPtr; i++) {
            jobInfoReply.toHosts[i] = safeSave(jobData->hPtr[i]->host);
        }
    }
    jobInfoReply.nIdx = allLsInfo->numIndx;
    if (!loadSched) {
        loadSched = (float *) malloc(allLsInfo->numIndx * sizeof(float *));
        loadStop = (float *) malloc(allLsInfo->numIndx * sizeof(float *));
        if ((!loadSched) || (!loadStop)) {
            ls_syslog(LOG_ERR, "%s", __func__, "malloc");
            mbdDie(MASTER_FATAL);
        }
    }

    if ((jobData->numHostPtr > 0) && (jobData->hPtr[0] != NULL)) {
        jobInfoReply.loadSched = loadSched;
        jobInfoReply.loadStop = loadStop;

        assignLoad(loadSched, loadStop, jobData->qPtr, jobData->hPtr[0]);
    } else {
        jobInfoReply.loadSched = jobData->qPtr->loadSched;
        jobInfoReply.loadStop = jobData->qPtr->loadStop;
    }

    jobInfoReply.userName = jobData->userName;
    jobInfoReply.userId = jobData->userId;

    jobInfoReply.exitStatus = jobData->exitStatus;
    jobInfoReply.execUid = jobData->execUid;
    if (!jobData->execHome)
        jobInfoReply.execHome = "";
    else
        jobInfoReply.execHome = jobData->execHome;

    if (!jobData->execCwd)
        jobInfoReply.execCwd = "";
    else
        jobInfoReply.execCwd = jobData->execCwd;

    if (!jobData->execUsername)
        jobInfoReply.execUsername = "";
    else
        jobInfoReply.execUsername = jobData->execUsername;

    jobInfoReply.reserveTime = jobData->reserveTime;
    jobInfoReply.jobPid = jobData->jobPid;
    jobInfoReply.port = jobData->port;
    jobInfoReply.jobPriority = jobData->jobPriority;

    jobInfoReply.jobBill = &jobBill;
    copyJobBill(&jobData->shared->jobBill, jobInfoReply.jobBill, FALSE);
    if (jobInfoReply.jobBill->options2 & SUB2_USE_DEF_PROCLIMIT) {
        jobInfoReply.jobBill->numProcessors = 1;
        jobInfoReply.jobBill->maxNumProcessors = 1;
    }
    if (jobInfoReply.jobBill->jobName)
        FREEUP(jobInfoReply.jobBill->jobName);
    fullJobName_r(jobData, fullName);
    jobInfoReply.jobBill->jobName = safeSave(fullName);
    if (jobInfoReply.jobBill->queue)
        FREEUP(jobInfoReply.jobBill->queue);
    jobInfoReply.jobBill->queue = safeSave(jobData->qPtr->queue);

    cpuFactor = NULL;
    if ((jobData->numHostPtr > 0) && (jobData->hPtr[0] != NULL) &&
        !IS_PEND(jobData->jStatus) &&
        strcmp(jobData->hPtr[0]->host, LOST_AND_FOUND)) {
        cpuFactor = &jobData->hPtr[0]->cpuFactor;
        FREEUP(jobInfoReply.jobBill->hostSpec);
        jobInfoReply.jobBill->hostSpec = safeSave(jobData->hPtr[0]->host);
    } else {
        if (getModelFactor_r(jobInfoReply.jobBill->hostSpec, &cpuF) < 0) {
            cpuFactor = getHostFactor(jobInfoReply.jobBill->hostSpec);
            if (cpuFactor == NULL) {
                cpuFactor = getHostFactor(jobInfoReply.jobBill->fromHost);
                if (cpuFactor != NULL) {
                    cpuF = *cpuFactor;
                    cpuFactor = &cpuF;
                }
                if (cpuFactor == NULL) {
                    ls_syslog(LOG_ERR,
                              "%s: Cannot find cpu factor for hostSpec  <%s>; "
                              "cpuFactor is set to 1.0",
                              fname, jobInfoReply.jobBill->fromHost);
                    cpuFactor = &one;
                }
            } else {
                cpuF = *cpuFactor;
                cpuFactor = &cpuF;
            }
        } else {
            cpuFactor = &cpuF;
        }
    }
    if (cpuFactor != NULL && *cpuFactor > 0) {
        if (jobInfoReply.jobBill->rLimits[LSF_RLIMIT_CPU] > 0)
            jobInfoReply.jobBill->rLimits[LSF_RLIMIT_CPU] /= *cpuFactor;
        if (jobInfoReply.jobBill->rLimits[LSF_RLIMIT_RUN] > 0)
            jobInfoReply.jobBill->rLimits[LSF_RLIMIT_RUN] /= *cpuFactor;
    }

    jgNode = jobData->jgrpNode;
    jobInfoReply.jType = jobData->nodeType;
    jobInfoReply.parentGroup = jgrpNodeParentPath(jgNode);
    jobInfoReply.jName = jgNode->name;

    if (jgNode->nodeType == JGRP_NODE_ARRAY) {
        for (i = 0; i < NUM_JGRP_COUNTERS; i++)
            jobInfoReply.counter[i] = ARRAY_DATA(jgNode)->counts[i];
    }

    jobInfoReply.jRusageUpdateTime = jobData->jRusageUpdateTime;

    memcpy((char *) &jobInfoReply.runRusage, (char *) &jobData->runRusage,
           sizeof(struct jRusage));

    len = jobInfoReplyXdrBufLen(&jobInfoReply);
    len += 1024;

    FREEUP(request_buf);
    request_buf = (char *) my_malloc(len, "packJobInfo");
    xdrmem_create(&xdrs, request_buf, len, XDR_ENCODE);

    hdr.version = version;

    if (!xdr_encodeMsg(&xdrs, (char *) &jobInfoReply, &hdr, xdr_jobInfoReply, 0,
                       NULL)) {
        ls_syslog(LOG_ERR, "%s", __func__, "xdr_encodeMsg", "jobInfoReply");
        xdr_destroy(&xdrs);
        freeJobInfoReply(&jobInfoReply);
        FREEUP(reasonTb);
        FREEUP(jReasonTb);
        FREEUP(loadSched);
        FREEUP(loadStop);
        FREEUP(request_buf);
        return -1;
    }
    FREEUP(reasonTb);
    FREEUP(jReasonTb);
    FREEUP(loadSched);
    FREEUP(loadStop);
    freeJobInfoReply(&jobInfoReply);
    i = XDR_GETPOS(&xdrs);
    *replyBuf = request_buf;
    xdr_destroy(&xdrs);
    return i;
}

int do_jobPeekReq(XDR *xdrs, int chfd, struct sockaddr_in *from, char *hostName,
                  struct packet_header *reqHdr, struct lsfAuth *auth)
{
    char reply_buf[MSGSIZE];
    XDR xdrs2;
    struct jobPeekReq jobPeekReq;
    struct jobPeekReply jobPeekReply;
    int reply;
    int cc;
    struct packet_header replyHdr;
    char *replyStruct;

    jobPeekReply.outFile = NULL;
    if (!xdr_jobPeekReq(xdrs, &jobPeekReq, reqHdr)) {
        reply = LSBE_XDR;
        ls_syslog(LOG_ERR, "%s", __func__, "xdr_jobPeekReq");
    } else {
        reply = peekJob(&jobPeekReq, &jobPeekReply, auth);
    }

    xdrmem_create(&xdrs2, reply_buf, MSGSIZE, XDR_ENCODE);
    replyHdr.operation = reply;
    if (reply == LSBE_NO_ERROR)
        replyStruct = (char *) &jobPeekReply;
    else
        replyStruct = (char *) 0;
    if (!xdr_encodeMsg(&xdrs2, replyStruct, &replyHdr, xdr_jobPeekReply, 0,
                       NULL)) {
        ls_syslog(LOG_ERR, "%s", __func__, "xdr_encodeMsg");
        xdr_destroy(&xdrs2);
        FREEUP(jobPeekReply.outFile);
        return -1;
    }
    cc = XDR_GETPOS(&xdrs2);
    if ((chan_write(chfd, reply_buf, cc)) != cc) {
        ls_syslog(LOG_ERR, "%s", __func__, "chan_write");
        xdr_destroy(&xdrs2);
        FREEUP(jobPeekReply.outFile);
        return -1;
    }
    xdr_destroy(&xdrs2);
    FREEUP(jobPeekReply.outFile);
    return 0;
}

int do_signalReq(XDR *xdrs, int chfd, struct sockaddr_in *from, char *hostName,
                 struct packet_header *reqHdr, struct lsfAuth *auth)
{
    XDR xdrs2;
    static struct signalReq signalReq;
    int reply;
    struct packet_header replyHdr;
    struct jData *jpbw;

    if (!xdr_signalReq(xdrs, &signalReq, reqHdr)) {
        reply = LSBE_XDR;
        ls_syslog(LOG_ERR, "%s", __func__, "xdr_signalReq");
        goto Reply;
    }
    signalReq.sigValue = sig_decode(signalReq.sigValue);
    if (signalReq.sigValue == SIG_CHKPNT) {
        if ((jpbw = getJobData(signalReq.jobId)) == NULL) {
            reply = LSBE_NO_JOB;
        } else {
            reply = signalJob(&signalReq, auth);
        }
    } else {
        reply = signalJob(&signalReq, auth);
    }
Reply:
    char reply_buf[LL_BUFSIZ_16];
    xdrmem_create(&xdrs2, reply_buf, MSGSIZE, XDR_ENCODE);
    replyHdr.operation = reply;

    if (!xdr_encodeMsg(&xdrs2, (char *) NULL, &replyHdr, xdr_int, 0, NULL)) {
        ls_syslog(LOG_ERR, "%s", __func__, "xdr_encodeMsg");
        xdr_destroy(&xdrs2);
        return -1;
    }
    if (chan_write(chfd, reply_buf, XDR_GETPOS(&xdrs2)) <= 0) {
        ls_syslog(LOG_ERR, "%s", __func__, "b_write_fix");
        xdr_destroy(&xdrs2);
        return -1;
    }
    xdr_destroy(&xdrs2);
    return 0;
}

int do_jobMsg(struct bucket *bucket, XDR *xdrs, int chfd,
              struct sockaddr_in *from, char *hostName,
              struct packet_header *reqHdr, struct lsfAuth *auth)
{
    static char fname[] = "do_jobMsg";
    char reply_buf[MSGSIZE];
    XDR xdrs2;
    int reply;
    struct packet_header sndhdr;
    struct packet_header replyHdr;
    struct jData *jpbw;
    struct bucket *msgq;
    struct Buffer *buf = bucket->storage;
    LSBMSG_DECL(header, jmsg);

    LSBMSG_INIT(header, jmsg);

    if (logclass & (LC_TRACE | LC_SIGNAL))
        ls_syslog(LOG_DEBUG1, "%s: Entering this routine ...", fname);

    if (!xdr_lsbMsg(xdrs, &jmsg, reqHdr)) {
        reply = LSBE_XDR;
        ls_syslog(LOG_ERR, "%s", __func__, "xdr_lsbMsg");
        goto Reply;
    }

    jmsg.header->msgId = msgcnt;
    msgcnt++;

    LSBMSG_CACHE_BUFFER(bucket, jmsg);

    sndhdr.operation = BATCH_JOB_MSG;
    xdrmem_create(&bucket->xdrs, buf->data, buf->len, XDR_ENCODE);

    if (!xdr_encodeMsg(&bucket->xdrs, (char *) &jmsg, &sndhdr, xdr_lsbMsg, 0,
                       NULL)) {
        reply = LSBE_XDR;
        ls_syslog(LOG_ERR, "%s", __func__, "xdr_encodeMsg");
        goto Reply;
    }
    xdr_destroy(&bucket->xdrs);
    xdrmem_create(&bucket->xdrs, buf->data, buf->len, XDR_DECODE);

    if ((jpbw = getJobData(jmsg.header->jobId)) == NULL) {
        reply = LSBE_NO_JOB;
        goto Reply;
    }

    if (IS_PEND(jpbw->jStatus)) {
        reply = LSBE_NOT_STARTED;
        goto Reply;
    }

    log_jobmsg(jpbw, &jmsg, jmsg.header->usrId);

    msgq = jpbw->hPtr[0]->msgq[MSG_STAT_QUEUED];
    eventPending = TRUE;

    if (logclass & (LC_SIGNAL))
        ls_syslog(LOG_DEBUG2, "%s: A message for job %s; eP=%d", fname,
                  lsb_jobid2str(jpbw->jobId), eventPending);

    QUEUE_APPEND(bucket, msgq);
    reply = LSBE_NO_ERROR;

Reply:
    xdrmem_create(&xdrs2, reply_buf, MSGSIZE, XDR_ENCODE);
    replyHdr.operation = reply;

    if (!xdr_encodeMsg(&xdrs2, (char *) NULL, &replyHdr, xdr_int, 0, NULL)) {
        ls_syslog(LOG_ERR, "%s", __func__, "xdr_encodeMsg");
        xdr_destroy(&xdrs2);
        return -1;
    }
    if (chan_write(chfd, reply_buf, XDR_GETPOS(&xdrs2)) <= 0) {
        ls_syslog(LOG_ERR, "%s", __func__, "b_write_fix");
        xdr_destroy(&xdrs2);
        return -1;
    }
    xdr_destroy(&xdrs2);
    return 0;
}

int do_migReq(XDR *xdrs, int chfd, struct sockaddr_in *from, char *hostName,
              struct packet_header *reqHdr, struct lsfAuth *auth)
{
    char reply_buf[MSGSIZE];
    XDR xdrs2;
    struct migReq migReq;
    struct submitMbdReply migReply;
    int reply;
    struct packet_header replyHdr;
    char *replyStruct;
    int i;

    migReply.jobId = 0;
    migReply.badReqIndx = 0;
    migReply.queue = "";
    migReply.badJobName = "";

    if (!xdr_migReq(xdrs, &migReq, reqHdr)) {
        reply = LSBE_XDR;
        ls_syslog(LOG_ERR, "%s", __func__, "xdr_migReq");
        migReq.numAskedHosts = 0;
        goto Reply;
    }
    reply = migJob(&migReq, &migReply, auth);

Reply:
    if (migReq.numAskedHosts) {
        for (i = 0; i < migReq.numAskedHosts; i++)
            free(migReq.askedHosts[i]);
        free(migReq.askedHosts);
    }
    xdrmem_create(&xdrs2, reply_buf, MSGSIZE, XDR_ENCODE);
    replyHdr.operation = reply;
    if (reply != LSBE_NO_ERROR)
        replyStruct = (char *) &migReply;
    else {
        replyStruct = (char *) 0;
    }
    if (!xdr_encodeMsg(&xdrs2, replyStruct, &replyHdr, xdr_submitMbdReply, 0,
                       NULL)) {
        ls_syslog(LOG_ERR, "%s", __func__, "xdr_encodeMsg");
        xdr_destroy(&xdrs2);
        return -1;
    }
    if (chan_write(chfd, reply_buf, XDR_GETPOS(&xdrs2)) <= 0) {
        ls_syslog(LOG_ERR, "%s", __func__, "b_write_fix");
        xdr_destroy(&xdrs2);
        return -1;
    }
    xdr_destroy(&xdrs2);
    return 0;
}

int do_statusReq(XDR *xdrs, int chfd, struct sockaddr_in *from, int *schedule,
                 struct packet_header *reqHdr)
{
    char reply_buf[MSGSIZE];
    XDR xdrs2;
    struct statusReq statusReq;
    int reply;
    struct hData *hData;
    struct packet_header replyHdr;

    if (!portok(from)) {
        ls_syslog(LOG_ERR, "%s: Received status report from bad port <%s>",
                  __func__, sockAdd2Str_(from));
        if (reqHdr->operation != BATCH_RUSAGE_JOB)
            errorBack(chfd, LSBE_PORT, from);
        return -1;
    }

    struct ll_host hs;
    get_host_by_sockaddr_in(from, &hs);
    if (hs.name[0] == 0) {
        ls_syslog(LOG_ERR, "%s", __func__, "getHostEntryByAddr_",
                  sockAdd2Str_(from));
        if (reqHdr->operation != BATCH_RUSAGE_JOB)
            errorBack(chfd, LSBE_BAD_HOST, from);
        return -1;
    }

    // hostent and change the signature of the service
    // routines later
    struct hostent hp;
    memset(&hp, 0, sizeof(struct hostent));
    hp.h_name = strdup(hs.name);

    if (!xdr_statusReq(xdrs, &statusReq, reqHdr)) {
        reply = LSBE_XDR;
        ls_syslog(LOG_ERR, "%s", __func__, "xdr_statusReq");
    } else {
        switch (reqHdr->operation) {
        case BATCH_STATUS_MSG_ACK:
            reply = statusMsgAck(&statusReq);
            break;
        case BATCH_STATUS_JOB:
            reply = statusJob(&statusReq, &hp, schedule);
            break;
        case BATCH_RUSAGE_JOB:
            reply = rusageJob(&statusReq, &hp);
            break;
        default:
            ls_syslog(LOG_ERR, "%s: Unknown request %d", __func__,
                      reqHdr->operation);
            reply = LSBE_PROTOCOL;
        }
    }

    xdr_lsffree(xdr_statusReq, (char *) &statusReq, reqHdr);

    if (reqHdr->operation == BATCH_RUSAGE_JOB) {
        free(hp.h_name);
        if (reply == LSBE_NO_ERROR)
            return 0;
        return -1;
    }

    xdrmem_create(&xdrs2, reply_buf, MSGSIZE, XDR_ENCODE);
    replyHdr.operation = reply;
    replyHdr.length = 0;
    if (!xdr_pack_hdr(&xdrs2, &replyHdr)) {
        ls_syslog(LOG_ERR, "%s", __func__, "xdr_pack_hdr", reply);
        xdr_destroy(&xdrs2);
        free(hp.h_name);
        return -1;
    }
    if (chan_write(chfd, reply_buf, XDR_GETPOS(&xdrs2)) <= 0) {
        ls_syslog(LOG_ERR, "%s", __func__, "b_write_fix");
        xdr_destroy(&xdrs2);
        free(hp.h_name);
        return -1;
    }
    xdr_destroy(&xdrs2);

    if ((hData = getHostData(hs.name)) != NULL)
        hStatChange(hData, 0);

    free(hp.h_name);
    return 0;
}

// LavaLite does not support chunk jobs
int do_chunkStatusReq(XDR *xdrs, int chfd, struct sockaddr_in *from,
                      int *schedule, struct packet_header *reqHdr)
{
#if 0
    static char             fname[] = "do_chunkStatusReq()";
    char                    reply_buf[MSGSIZE];
    XDR                     xdrs2;
    struct chunkStatusReq   chunkStatusReq;
    int                     reply;
    struct hData           *hData;
    struct hostent         *hp;
    struct hostent          hpBuf;
    struct packet_header        replyHdr;
    int i = 0;

#ifdef INTER_DAEMON_AUTH
    if (authSbdRequest(NULL, xdrs, reqHdr, from) != LSBE_NO_ERROR) {
        ls_syslog(LOG_ERR, "%s: Received status report from unauthenticated host <%s>",
                  fname, sockAdd2Str_(from));
        errorBack(chfd, LSBE_PERMISSION, from);
        return -1;
    }
#endif

    if (!portok(from)) {
        ls_syslog(LOG_ERR, "%s: Received status report from bad port <%s>",
                  fname,
                  sockAdd2Str_(from));
        errorBack(chfd, LSBE_PORT, from);
        return -1;
    }

    struct ll_host hs;
    get_host_by_sockaddr_in(from, &hs);
    if (hs.name[0] == 0) {
        ls_syslog(LOG_ERR, "%s: reverse DNS failed from %s", __func__,
                  sockAdd2Str_(from));
        errorBack(chfd, LSBE_BAD_HOST, from);
        return -1;
    }

    if (!xdr_chunkStatusReq(xdrs, &chunkStatusReq, reqHdr)) {
        reply = LSBE_XDR;
        ls_syslog(LOG_ERR, "%s", __func__, "xdr_chunkStatusReq");
    } else {

        for (i=0; i<chunkStatusReq.numStatusReqs; i++) {

            statusJob(chunkStatusReq.statusReqs[i], &hpBuf, schedule);
        }

        reply = LSBE_NO_ERROR;
    }

    xdr_lsffree(xdr_chunkStatusReq, (char *) &chunkStatusReq, reqHdr);

    xdrmem_create(&xdrs2, reply_buf, MSGSIZE, XDR_ENCODE);
    replyHdr.operation = reply;
    replyHdr.length = 0;
    if (!xdr_pack_hdr(&xdrs2, &replyHdr)) {
        ls_syslog(LOG_ERR, "%s", __func__, "xdr_pack_hdr",
                  reply);
        xdr_destroy(&xdrs2);
        free(hp.h_name);
        return -1;
    }
    if (chan_write(chfd, reply_buf, XDR_GETPOS(&xdrs2)) <= 0) {
        ls_syslog(LOG_ERR, "%s", __func__, "b_write_fix");
        xdr_destroy(&xdrs2);
        free(hp.h_name);
        return -1;
    }
    xdr_destroy(&xdrs2);

    if ((hData = getHostData(hpBuf.h_name)) != NULL)
        hStatChange(hData, 0);
#endif
    return 0;
}

int do_restartReq(XDR *xdrs, int chfd, struct sockaddr_in *from,
                  struct packet_header *reqHdr)
{
    char *reply_buf;
    XDR xdrs2;
    int buflen;
    struct packet_header replyHdr;
    int reply;
    struct sbdPackage sbdPackage;
    int cc;
    struct hData *hData;

    if (!portok(from)) {
        ls_syslog(LOG_ERR, "%s: Received status report from bad port <%s>",
                  __func__, sockAdd2Str_(from));
        errorBack(chfd, LSBE_PORT, from);
        return -1;
    }

    struct ll_host hs;
    get_host_by_sockaddr_in(from, &hs);
    if (hs.name[0] == 0) {
        ls_syslog(LOG_ERR, "%s get_host_by_sockaddr_in failed ", __func__,
                  sockAdd2Str_(from));
        errorBack(chfd, LSBE_BAD_HOST, from);
        return -1;
    }

    if ((hData = getHostData(hs.name)) == NULL) {
        ls_syslog(LOG_ERR, "%s: getHostDatas() failed", __func__, hs.name);
        errorBack(chfd, LSBE_BAD_HOST, from);
        return -1;
    }
    hStatChange(hData, 0);

    sbdPackage.jobs = NULL;
    if ((sbdPackage.numJobs = countNumSpecs(hData)) > 0)
        sbdPackage.jobs = calloc(sbdPackage.numJobs, sizeof(struct jobSpecs));

    buflen = sbatchdJobs(&sbdPackage, hData);
    reply = LSBE_NO_ERROR;

    reply_buf = (char *) my_malloc(buflen, "do_restartReq");
    xdrmem_create(&xdrs2, reply_buf, buflen, XDR_ENCODE);
    replyHdr.operation = reply;
    if (!xdr_encodeMsg(&xdrs2, (char *) &sbdPackage, &replyHdr, xdr_sbdPackage,
                       0, NULL)) {
        ls_syslog(LOG_ERR, "%s", __func__, "xdr_encodeMsg");
        xdr_destroy(&xdrs2);
        free(reply_buf);
        for (cc = 0; cc < sbdPackage.numJobs; cc++)
            freeJobSpecs(&sbdPackage.jobs[cc]);
        if (sbdPackage.jobs)
            free(sbdPackage.jobs);
        return -1;
    }
    cc = XDR_GETPOS(&xdrs2);
    if (chan_write(chfd, reply_buf, cc) <= 0)
        ls_syslog(LOG_ERR, "%s", __func__, "chan_write", cc);

    free(reply_buf);
    xdr_destroy(&xdrs2);
    for (cc = 0; cc < sbdPackage.numJobs; cc++)
        freeJobSpecs(&sbdPackage.jobs[cc]);
    if (sbdPackage.jobs)
        free(sbdPackage.jobs);
    return 0;
}

int do_hostInfoReq(XDR *xdrs, int chfd, struct sockaddr_in *from,
                   struct packet_header *reqHdr)
{
    static char fname[] = "do_hostInfoReq()";
    char *reply_buf;
    XDR xdrs2;
    struct packet_header replyHdr;
    char *replyStruct;
    int count;
    int reply;
    struct hostDataReply hostsReply;

    struct infoReq hostsReq = {0};

    hostsReply.numHosts = 0;
    hostsReply.hosts = NULL;
    if (!xdr_infoReq(xdrs, &hostsReq, reqHdr)) {
        reply = LSBE_XDR;
        ls_syslog(LOG_ERR, "%s", __func__, "xdr_infoReq");
    } else {
        reply = checkHosts(&hostsReq, &hostsReply);
    }

    if (logclass & (LC_EXEC)) {
        int i;
        for (i = 0; i < hostsReply.numHosts; i++)
            ls_syslog(LOG_DEBUG, "%s, host[%d]'s name is %s", fname, i,
                      hostsReply.hosts[i].host);
    }
    count = hostsReply.numHosts *
                (sizeof(struct hostInfoEnt) + MAXLINELEN + MAXHOSTNAMELEN +
                 hostsReply.nIdx * 4 * sizeof(float)) +
            100;
    reply_buf = (char *) my_malloc(count, "do_hostInfoReq");
    xdrmem_create(&xdrs2, reply_buf, count, XDR_ENCODE);
    replyHdr.operation = reply;
    if (reply == LSBE_NO_ERROR || reply == LSBE_BAD_HOST)
        replyStruct = (char *) &hostsReply;
    else
        replyStruct = (char *) 0;
    if (!xdr_encodeMsg(&xdrs2, replyStruct, &replyHdr, xdr_hostDataReply, 0,
                       NULL)) {
        ls_syslog(LOG_ERR, "%s", __func__, "xdr_encodeMsg");
        FREEUP(hostsReply.hosts);
        FREEUP(reply_buf);
        xdr_destroy(&xdrs2);
        return -1;
    }
    if (chan_write(chfd, reply_buf, XDR_GETPOS(&xdrs2)) <= 0) {
        ls_syslog(LOG_ERR, "%s", __func__, "b_write_fix", XDR_GETPOS(&xdrs2));
        xdr_destroy(&xdrs2);
        FREEUP(hostsReply.hosts);
        FREEUP(reply_buf);
        return -1;
    }
    FREEUP(hostsReply.hosts);
    xdr_destroy(&xdrs2);
    FREEUP(reply_buf);
    return 0;
}

int do_userInfoReq(XDR *xdrs, int chfd, struct sockaddr_in *from,
                   struct packet_header *reqHdr)
{
    char *reply_buf;
    XDR xdrs2;
    int reply;
    struct packet_header replyHdr;
    char *replyStruct;
    int count;
    struct infoReq userInfoReq;
    struct userInfoReply userInfoReply;

    userInfoReply.users = NULL;
    if (!xdr_infoReq(xdrs, &userInfoReq, reqHdr)) {
        reply = LSBE_XDR;
        ls_syslog(LOG_ERR, "%s", __func__, "xdr_infoReq");
    } else {
        count = userInfoReq.numNames == 0 ? uDataList.numEnts
                                          : userInfoReq.numNames;
        userInfoReply.users = (struct userInfoEnt *) my_calloc(
            count, sizeof(struct userInfoEnt), "do_userInfoReq");
        reply = checkUsers(&userInfoReq, &userInfoReply);
    }
    count =
        userInfoReply.numUsers * (sizeof(struct userInfoEnt) + MAXLSFNAMELEN) +
        100;
    reply_buf = (char *) my_malloc(count, "do_userInfoReq");
    xdrmem_create(&xdrs2, reply_buf, count, XDR_ENCODE);
    replyHdr.operation = reply;
    if (reply == LSBE_NO_ERROR || reply == LSBE_BAD_USER)
        replyStruct = (char *) &userInfoReply;
    else
        replyStruct = (char *) 0;
    if (!xdr_encodeMsg(&xdrs2, replyStruct, &replyHdr, xdr_userInfoReply, 0,
                       NULL)) {
        ls_syslog(LOG_ERR, "%s", __func__, "xdr_encodeMsg");
        xdr_destroy(&xdrs2);
        FREEUP(reply_buf);
        FREEUP(userInfoReply.users);
        if (userInfoReply.users)
            free(userInfoReply.users);
        return -1;
    }
    if (chan_write(chfd, reply_buf, XDR_GETPOS(&xdrs2)) <= 0) {
        ls_syslog(LOG_ERR, "%s", __func__, "b_write_fix", XDR_GETPOS(&xdrs2));
        xdr_destroy(&xdrs2);
        FREEUP(reply_buf);
        FREEUP(userInfoReply.users);
        return -1;
    }
    xdr_destroy(&xdrs2);
    FREEUP(reply_buf);
    FREEUP(userInfoReply.users);
    return 0;
}

int xdrsize_QueueInfoReply(struct queueInfoReply *qInfoReply)
{
    int len;
    int i;

    len = 0;

    for (i = 0; i < qInfoReply->numQueues; i++) {
        len += XDR_STRLEN(strlen(qInfoReply->queues[i].description)) +
               XDR_STRLEN(strlen(qInfoReply->queues[i].windows)) +
               XDR_STRLEN(strlen(qInfoReply->queues[i].userList)) +
               XDR_STRLEN(strlen(qInfoReply->queues[i].hostList)) +
               XDR_STRLEN(strlen(qInfoReply->queues[i].defaultHostSpec)) +
               XDR_STRLEN(strlen(qInfoReply->queues[i].hostSpec)) +
               XDR_STRLEN(strlen(qInfoReply->queues[i].windowsD)) +
               XDR_STRLEN(strlen(qInfoReply->queues[i].admins)) +
               XDR_STRLEN(strlen(qInfoReply->queues[i].preCmd)) +
               XDR_STRLEN(strlen(qInfoReply->queues[i].postCmd)) +
               XDR_STRLEN(strlen(qInfoReply->queues[i].requeueEValues)) +
               XDR_STRLEN(strlen(qInfoReply->queues[i].resReq)) +
               XDR_STRLEN(strlen(qInfoReply->queues[i].resumeCond)) +
               XDR_STRLEN(strlen(qInfoReply->queues[i].stopCond)) +
               XDR_STRLEN(strlen(qInfoReply->queues[i].jobStarter)) +
               XDR_STRLEN(strlen(qInfoReply->queues[i].suspendActCmd)) +
               XDR_STRLEN(strlen(qInfoReply->queues[i].resumeActCmd)) +
               XDR_STRLEN(strlen(qInfoReply->queues[i].terminateActCmd)) +
               XDR_STRLEN(strlen(qInfoReply->queues[i].chkpntDir));
    }
    len += ALIGNWORD_(sizeof(struct queueInfoReply) +
                      qInfoReply->numQueues *
                          (sizeof(struct queueInfoEnt) + MAXLSFNAMELEN +
                           qInfoReply->nIdx * 2 * sizeof(float)) +
                      qInfoReply->numQueues * NET_INTSIZE_);

    return len;
}

int do_queueInfoReq(XDR *xdrs, int chfd, struct sockaddr_in *from,
                    struct packet_header *req_hdr)
{
    (void)from;

    XDR xdrs2;
    struct packet_header reply_hdr;
    char *reply_buf = NULL;
    char *reply_struct = NULL;
    int reply = LSBE_NO_ERROR;
    int len = 0;
    int rc = 0;

    /* Preallocate queue array; we expect checkQueues() to fill this */
    struct queueInfoReply qInfoReply = {0};
    qInfoReply.numQueues = 0;
    qInfoReply.queues = calloc(numofqueues,
                               sizeof(struct queueInfoEnt));
    if (qInfoReply.queues == NULL) {
        ls_syslog(LOG_ERR, "%s: my_calloc() failed", __func__);
        return -1;
    }

    // Decode request
    struct infoReq qInfoReq = {0};
    if (!xdr_infoReq(xdrs, &qInfoReq, req_hdr)) {
        reply = LSBE_XDR;
        ls_syslog(LOG_ERR, "%s: xdr_infoReq() failed", __func__);

        /* Header-only reply, no body */
        len = (int)sizeof(struct packet_header);
    } else {
        /* Build reply payload */
        reply = checkQueues(&qInfoReq, &qInfoReply);

        if (reply == LSBE_NO_ERROR || reply == LSBE_BAD_QUEUE) {
            len = (int)sizeof(struct packet_header);
            len += xdrsize_QueueInfoReply(&qInfoReply);
        } else {
            /* Errors: header only */
            len = (int)sizeof(struct packet_header);
        }
    }

    reply_buf = calloc((size_t)len, 1);
    if (reply_buf == NULL) {
        ls_syslog(LOG_ERR, "%s: calloc(%d) failed", __func__, len);
        freeQueueInfoReply(&qInfoReply, NULL);
        return -1;
    }

    xdrmem_create(&xdrs2, reply_buf, (u_int)len, XDR_ENCODE);
    init_pack_hdr(&reply_hdr);
    reply_hdr.operation = reply;

    if (reply == LSBE_NO_ERROR || reply == LSBE_BAD_QUEUE)
        reply_struct = (char *)&qInfoReply;
    else
        reply_struct = NULL;

    if (!xdr_encodeMsg(&xdrs2,
                       reply_struct,
                       &reply_hdr,
                       xdr_queueInfoReply,
                       0,
                       NULL)) {
        ls_syslog(LOG_ERR, "%s: xdr_encodeMsg() failed", __func__);
        rc = -1;
        goto out;
    }

    if (chan_write(chfd, reply_buf, XDR_GETPOS(&xdrs2)) <= 0) {
        ls_syslog(LOG_ERR,
                  "%s: chan_write(%d) failed",
                  __func__,
                  (int)XDR_GETPOS(&xdrs2));
        rc = -1;
        goto out;
    }

out:
    freeQueueInfoReply(&qInfoReply, reply_struct);
    FREEUP(reply_buf);
    xdr_destroy(&xdrs2);

    return rc;
}


int do_groupInfoReq(XDR *xdrs, int chfd, struct sockaddr_in *from,
                    struct packet_header *reqHdr)
{
    struct infoReq groupInfoReq;
    char *reply_buf;
    XDR xdrs2;
    int reply;
    int len;
    struct packet_header replyHdr;
    char *replyStruct;
    struct groupInfoReply groupInfoReply;

    memset((struct groupInfoReply *) &groupInfoReply, 0,
           sizeof(struct groupInfoReply));

    if (!xdr_infoReq(xdrs, &groupInfoReq, reqHdr)) {
        reply = LSBE_XDR;
        ls_syslog(LOG_ERR, "%s", __func__, "xdr_infoReq");
    }

    if (groupInfoReq.options & HOST_GRP) {
        if (numofhgroups == 0) {
            reply = LSBE_NO_HOST_GROUP;
        } else {
            groupInfoReply.groups = (struct groupInfoEnt *) my_calloc(
                numofhgroups, sizeof(struct groupInfoEnt), "do_groupInfoReq");

            reply = checkGroups(&groupInfoReq, &groupInfoReply);
        }
    } else {
        if (numofugroups == 0) {
            reply = LSBE_NO_USER_GROUP;
        } else {
            reply = checkGroups(&groupInfoReq, &groupInfoReply);
        }
    }

    len = sizeofGroupInfoReply(&groupInfoReply);

    len += ALIGNWORD_(sizeof(struct packet_header));

    reply_buf = (char *) my_malloc(len, "do_groupInfoReq");

    xdrmem_create(&xdrs2, reply_buf, len, XDR_ENCODE);

    replyHdr.operation = reply;

    if (reply == LSBE_NO_ERROR || reply == LSBE_BAD_GROUP)
        replyStruct = (char *) &groupInfoReply;
    else
        replyStruct = (char *) NULL;

    if (!xdr_encodeMsg(&xdrs2, replyStruct, &replyHdr, xdr_groupInfoReply, 0,
                       NULL)) {
        ls_syslog(LOG_ERR, "%s", __func__, "xdr_encodeMsg");
        FREEUP(reply_buf);
        xdr_destroy(&xdrs2);
        freeGroupInfoReply(&groupInfoReply);
        return -1;
    }

    if ((chan_write(chfd, reply_buf, XDR_GETPOS(&xdrs2))) <= 0) {
        ls_syslog(LOG_ERR, "%s", __func__, "chan_write", XDR_GETPOS(&xdrs2));
        FREEUP(reply_buf);
        xdr_destroy(&xdrs2);
        freeGroupInfoReply(&groupInfoReply);
        return -1;
    }

    FREEUP(reply_buf);
    xdr_destroy(&xdrs2);
    freeGroupInfoReply(&groupInfoReply);

    return 0;
}

int do_paramInfoReq(XDR *xdrs, int chfd, struct sockaddr_in *from,
                    struct packet_header *reqHdr)
{
    char *reply_buf;
    XDR xdrs2;
    int reply;
    struct packet_header replyHdr;
    char *replyStruct;
    int count, jobSpoolDirLen;
    struct infoReq infoReq;
    struct parameterInfo paramInfo;

    if (!xdr_infoReq(xdrs, &infoReq, reqHdr)) {
        reply = LSBE_XDR;
        ls_syslog(LOG_ERR, "%s", __func__, "xdr_infoReq");
    } else
        checkParams(&infoReq, &paramInfo);
    reply = LSBE_NO_ERROR;

    if (paramInfo.pjobSpoolDir != NULL) {
        jobSpoolDirLen = strlen(paramInfo.pjobSpoolDir);
    } else {
        jobSpoolDirLen = 4;
    }

    count = sizeof(struct parameterInfo) + strlen(paramInfo.defaultQueues) +
            strlen(paramInfo.defaultHostSpec) +
            strlen(paramInfo.defaultProject) + 100 + jobSpoolDirLen;

    reply_buf = (char *) my_malloc(count, "do_paramInfoReq");
    xdrmem_create(&xdrs2, reply_buf, count, XDR_ENCODE);

    init_pack_hdr(&replyHdr);
    replyHdr.operation = reply;
    replyStruct = (char *) &paramInfo;
    if (!xdr_encodeMsg(&xdrs2, replyStruct, &replyHdr, xdr_parameterInfo, 0,
                       NULL)) {
        ls_syslog(LOG_ERR, "%s", __func__, "xdr_encodeMsg");
        xdr_destroy(&xdrs2);
        FREEUP(reply_buf);
        return -1;
    }

    if (chan_write(chfd, reply_buf, XDR_GETPOS(&xdrs2)) <= 0) {
        ls_syslog(LOG_ERR, "%s", __func__, "chan_write", XDR_GETPOS(&xdrs2));
        xdr_destroy(&xdrs2);
        FREEUP(reply_buf);
        return -1;
    }
    xdr_destroy(&xdrs2);
    FREEUP(reply_buf);
    return 0;
}

int do_queueControlReq(XDR *xdrs, int chfd, struct sockaddr_in *from,
                       char *hostName, struct packet_header *reqHdr,
                       struct lsfAuth *auth)
{
    struct controlReq bqcReq;
    char reply_buf[MSGSIZE];
    XDR xdrs2;
    int reply;
    struct packet_header replyHdr;

    if (!xdr_controlReq(xdrs, &bqcReq, reqHdr)) {
        reply = LSBE_XDR;
        ls_syslog(LOG_ERR, "%s", __func__, "xdr_controlReq");
    } else {
        reply = ctrlQueue(&bqcReq, auth);
    }

    xdrmem_create(&xdrs2, reply_buf, MSGSIZE, XDR_ENCODE);
    replyHdr.operation = reply;
    replyHdr.length = 0;
    if (!xdr_pack_hdr(&xdrs2, &replyHdr)) {
        ls_syslog(LOG_ERR, "%s", __func__, "xdr_pack_hdr", reply);
        xdr_destroy(&xdrs2);
        return -1;
    }
    if (chan_write(chfd, reply_buf, XDR_GETPOS(&xdrs2)) <= 0) {
        ls_syslog(LOG_ERR, "%s", __func__, "chan_write", XDR_GETPOS(&xdrs2));
        xdr_destroy(&xdrs2);
        return -1;
    }
    xdr_destroy(&xdrs2);
    return 0;
}

int do_mbdShutDown(XDR *xdrs, int s, struct sockaddr_in *from, char *hostName,
                   struct packet_header *reqHdr)
{
    char reply_buf[MSGSIZE];
    XDR xdrs2;
    struct packet_header replyHdr;

    xdrmem_create(&xdrs2, reply_buf, MSGSIZE, XDR_ENCODE);
    replyHdr.operation = LSBE_NO_ERROR;
    replyHdr.length = 0;
    if (!xdr_pack_hdr(&xdrs2, &replyHdr)) {
        ls_syslog(LOG_ERR, "%s", __func__, "xdr_pack_hdr", replyHdr.operation);
    }
    if (b_write_fix(s, reply_buf, XDR_GETPOS(&xdrs2)) <= 0)
        ls_syslog(LOG_ERR, "%s", __func__, "b_write_fix", XDR_GETPOS(&xdrs2));
    xdr_destroy(&xdrs2);
    millisleep_(3000);
    mbdDie(MASTER_RECONFIG);
    return 0;
}

int do_reconfigReq(XDR *xdrs, int chfd, struct sockaddr_in *from,
                   char *hostName, struct packet_header *reqHdr)
{
    char reply_buf[MSGSIZE];
    XDR xdrs2;
    struct packet_header replyHdr;

    xdrmem_create(&xdrs2, reply_buf, MSGSIZE, XDR_ENCODE);
    replyHdr.operation = LSBE_NO_ERROR;
    replyHdr.length = 0;
    if (!xdr_pack_hdr(&xdrs2, &replyHdr)) {
        ls_syslog(LOG_ERR, "%s", __func__, "xdr_pack_hdr", replyHdr.operation);
    }
    if (chan_write(chfd, reply_buf, XDR_GETPOS(&xdrs2)) <= 0)
        ls_syslog(LOG_ERR, "%s", __func__, "chan_write", XDR_GETPOS(&xdrs2));
    xdr_destroy(&xdrs2);

    ls_syslog(LOG_DEBUG, "%s: restart a new mbatchd", __func__);
    mbdDie(MASTER_RECONFIG);

    return 0;
}

int do_hostControlReq(XDR *xdrs, int chfd, struct sockaddr_in *from,
                      char *hostName, struct packet_header *reqHdr,
                      struct lsfAuth *auth)
{
    struct controlReq hostControlReq;
    char reply_buf[MSGSIZE];
    XDR xdrs2;
    int reply;
    struct hData *hData;
    struct packet_header replyHdr;

    if (!xdr_controlReq(xdrs, &hostControlReq, reqHdr)) {
        reply = LSBE_XDR;
        ls_syslog(LOG_ERR, "%s", __func__, "xdr_controlReq");
        goto checkout;
    }
    hData = getHostData(hostControlReq.name);
    if (hData == NULL)
        reply = LSBE_BAD_HOST;
    else
        reply = ctrlHost(&hostControlReq, hData, auth);

checkout:
    xdrmem_create(&xdrs2, reply_buf, MSGSIZE, XDR_ENCODE);
    replyHdr.operation = reply;
    replyHdr.length = 0;
    if (!xdr_pack_hdr(&xdrs2, &replyHdr)) {
        ls_syslog(LOG_ERR, "%s", __func__, "xdr_pack_hdr", reply);
    }
    if (chan_write(chfd, reply_buf, XDR_GETPOS(&xdrs2)) <= 0)
        ls_syslog(LOG_ERR, "%s", __func__, "chan_write", XDR_GETPOS(&xdrs2));
    xdr_destroy(&xdrs2);
    return 0;
}

int do_jobSwitchReq(XDR *xdrs, int chfd, struct sockaddr_in *from,
                    char *hostName, struct packet_header *reqHdr,
                    struct lsfAuth *auth)
{
    char reply_buf[MSGSIZE];
    XDR xdrs2;
    struct jobSwitchReq jobSwitchReq;
    int reply;
    struct packet_header replyHdr;

    if (!xdr_jobSwitchReq(xdrs, &jobSwitchReq, reqHdr)) {
        reply = LSBE_XDR;
        ls_syslog(LOG_ERR, "%s", __func__, "xdr_jobSwitchReq");
    } else {
        reply = switchJobArray(&jobSwitchReq, auth);
    }
    xdrmem_create(&xdrs2, reply_buf, MSGSIZE, XDR_ENCODE);
    replyHdr.operation = reply;
    replyHdr.length = 0;
    if (!xdr_pack_hdr(&xdrs2, &replyHdr)) {
        ls_syslog(LOG_ERR, "%s", __func__, "xdr_pack_hdr", reply);
        xdr_destroy(&xdrs2);
        return -1;
    }
    if (chan_write(chfd, reply_buf, XDR_GETPOS(&xdrs2)) <= 0) {
        ls_syslog(LOG_ERR, "%s", __func__, "chan_write", XDR_GETPOS(&xdrs2));
        xdr_destroy(&xdrs2);
        return -1;
    }
    xdr_destroy(&xdrs2);
    return 0;
}

int do_jobMoveReq(XDR *xdrs, int chfd, struct sockaddr_in *from, char *hostName,
                  struct packet_header *reqHdr, struct lsfAuth *auth)
{
    char reply_buf[MSGSIZE];
    XDR xdrs2;
    struct jobMoveReq jobMoveReq;
    int reply;
    struct packet_header replyHdr;
    char *replyStruct;

    if (!xdr_jobMoveReq(xdrs, &jobMoveReq, reqHdr)) {
        reply = LSBE_XDR;
        ls_syslog(LOG_ERR, "%s", __func__, "xdr_jobMoveReq");
    } else {
        reply = moveJobArray(&jobMoveReq, TRUE, auth);
    }
    xdrmem_create(&xdrs2, reply_buf, MSGSIZE, XDR_ENCODE);
    replyHdr.operation = reply;
    if (reply == LSBE_NO_ERROR)
        replyStruct = (char *) &jobMoveReq;
    else {
        replyStruct = (char *) 0;
    }
    if (!xdr_encodeMsg(&xdrs2, replyStruct, &replyHdr, xdr_jobMoveReq, 0,
                       NULL)) {
        ls_syslog(LOG_ERR, "%s", __func__, "xdr_encodeMsg");
        xdr_destroy(&xdrs2);
        return -1;
    }
    if (chan_write(chfd, reply_buf, XDR_GETPOS(&xdrs2)) <= 0) {
        ls_syslog(LOG_ERR, "%s", __func__, "chan_write", XDR_GETPOS(&xdrs2));
        xdr_destroy(&xdrs2);
        return -1;
    }
    xdr_destroy(&xdrs2);
    return 0;
}

int do_modifyReq(XDR *xdrs, int s, struct sockaddr_in *from, char *hostName,
                 struct packet_header *reqHdr, struct lsfAuth *auth)
{
    static struct submitMbdReply submitReply;
    static int first = TRUE;
    static struct modifyReq modifyReq;
    int reply;

    initSubmit(&first, &(modifyReq.submitReq), &submitReply);

    if (!xdr_modifyReq(xdrs, &modifyReq, reqHdr)) {
        reply = LSBE_XDR;
        ls_syslog(LOG_ERR, "%s", __func__, "xdr_modifyReq");
        goto sendback;
    }

    if (!(modifyReq.submitReq.options & SUB_RLIMIT_UNIT_IS_KB)) {
        convertRLimit(modifyReq.submitReq.rLimits, 1);
    }

    reply = modifyJob(&modifyReq, &submitReply, auth);
sendback:
    if (sendBack(reply, &modifyReq.submitReq, &submitReply, s) < 0)
        return -1;
    return 0;
}

static void initSubmit(int *first, struct submitReq *subReq,
                       struct submitMbdReply *submitReply)
{
    static char fname[] = "initSubmit";

    if (*first == TRUE) {
        subReq->fromHost = (char *) my_malloc(MAXHOSTNAMELEN, fname);
        subReq->jobFile = (char *) my_malloc(MAXFILENAMELEN, fname);
        subReq->inFile = (char *) my_malloc(MAXFILENAMELEN, fname);
        subReq->outFile = (char *) my_malloc(MAXFILENAMELEN, fname);
        subReq->errFile = (char *) my_malloc(MAXFILENAMELEN, fname);
        subReq->inFileSpool = (char *) my_malloc(MAXFILENAMELEN, fname);
        subReq->commandSpool = (char *) my_malloc(MAXFILENAMELEN, fname);
        subReq->cwd = (char *) my_malloc(MAXFILENAMELEN, fname);
        subReq->subHomeDir = (char *) my_malloc(MAXFILENAMELEN, fname);
        subReq->chkpntDir = (char *) my_malloc(MAXFILENAMELEN, fname);
        subReq->hostSpec = (char *) my_malloc(MAXHOSTNAMELEN, fname);
        submitReply->badJobName = (char *) my_malloc(MAX_CMD_DESC_LEN, fname);
        *first = FALSE;
    } else {
        FREEUP(subReq->chkpntDir);
        FREEUP(submitReply->badJobName);
        subReq->chkpntDir = (char *) my_malloc(MAXFILENAMELEN, fname);
        submitReply->badJobName = (char *) my_malloc(MAX_CMD_DESC_LEN, fname);
    }

    subReq->askedHosts = NULL;
    subReq->numAskedHosts = 0;

    subReq->nxf = 0;
    subReq->xf = NULL;

    submitReply->jobId = 0;
    submitReply->queue = "";

    strcpy(submitReply->badJobName, "");
}

static int sendBack(int reply, struct submitReq *submitReq,
                    struct submitMbdReply *submitReply, int chfd)
{
    char reply_buf[MSGSIZE / 2];
    XDR xdrs2;
    struct packet_header replyHdr;

    if (submitReq->nxf > 0)
        FREEUP(submitReq->xf);

    xdrmem_create(&xdrs2, reply_buf, MSGSIZE / 2, XDR_ENCODE);
    replyHdr.operation = reply;
    if (!xdr_encodeMsg(&xdrs2, (char *) submitReply, &replyHdr,
                       xdr_submitMbdReply, 0, NULL)) {
        ls_syslog(LOG_ERR, "%s", __func__, "xdr_encodeMsg");
        xdr_destroy(&xdrs2);
        return -1;
    }
    if (chan_write(chfd, reply_buf, XDR_GETPOS(&xdrs2)) != XDR_GETPOS(&xdrs2))
        ls_syslog(LOG_ERR, "%s", __func__, "chan_write", XDR_GETPOS(&xdrs2));
    xdr_destroy(&xdrs2);
    return 0;
}

void doNewJobReply(struct sbdNode *sbdPtr, int exception)
{
    static char fname[] = "doNewJobReply";
    struct packet_header replyHdr;
    XDR xdrs;
    struct jData *jData = sbdPtr->jData;
    struct jobReply jobReply;
    struct Buffer *buf;
    struct packet_header hdr;
    int cc, s, svReason, replayReason;

    if (logclass & LC_COMM)
        ls_syslog(LOG_DEBUG, "%s: Entering ...", fname);

    if (jData->jobPid != 0)
        return;

    if (exception == TRUE || chan_dequeue(sbdPtr->chanfd, &buf) < 0) {
        if (exception == TRUE)
            ls_syslog(LOG_ERR, "%s: Exception bit of <%d> is set for job <%s>",
                      fname, sbdPtr->chanfd, lsb_jobid2str(jData->jobId));
        else
            ls_syslog(LOG_ERR, "%s", __func__, lsb_jobid2str(jData->jobId),
                      "chan_dequeue");

        if (IS_START(jData->jStatus)) {
            jData->newReason = PEND_JOB_START_FAIL;
            jStatusChange(jData, JOB_STAT_PEND, LOG_IT, fname);
        }
        return;
    }

    xdrmem_create(&xdrs, buf->data, buf->len, XDR_DECODE);

    if (!xdr_pack_hdr(&xdrs, &replyHdr)) {
        ls_syslog(LOG_ERR, "%s", __func__, lsb_jobid2str(jData->jobId),
                  "xdr_pack_hdr");

        if (IS_START(jData->jStatus)) {
            jData->newReason = PEND_JOB_START_FAIL;
            jStatusChange(jData, JOB_STAT_PEND, LOG_IT, fname);
        }
        goto Leave;
    }

    if (replyHdr.operation != ERR_NO_ERROR) {
        if (IS_START(jData->jStatus)) {
            replayReason =
                jobStartError(jData, (sbdReplyType) replyHdr.operation);
            svReason = jData->newReason;
            jData->newReason = replayReason;
            jStatusChange(jData, JOB_STAT_PEND, LOG_IT, fname);
            jData->newReason = svReason;
        }
        goto Leave;
    }

    if (!xdr_jobReply(&xdrs, &jobReply, &replyHdr)) {
        ls_syslog(LOG_ERR, "%s", __func__, lsb_jobid2str(jData->jobId),
                  "xdr_jobReply");
        if (IS_START(jData->jStatus)) {
            jData->newReason = PEND_JOB_START_FAIL;
            jStatusChange(jData, JOB_STAT_PEND, LOG_IT, fname);
        }
        goto Leave;
    }

    if (IS_START(jData->jStatus)) {
        jData->jobPid = jobReply.jobPid;
        jData->jobPGid = jobReply.jobPGid;

        log_startjobaccept(jData);

        if (lsbParams[LSB_MBD_BLOCK_SEND].paramValue == NULL) {
            struct Buffer *replyBuf;

            if (chan_alloc_buf(&replyBuf, sizeof(struct packet_header)) < 0) {
                ls_syslog(LOG_ERR, "%s", __func__, "chan_alloc_buf");
                goto Leave;
            }

            memcpy(replyBuf->data, buf->data, PACKET_HEADER_SIZE);

            replyBuf->len = PACKET_HEADER_SIZE;

            if (chan_enqueue(sbdPtr->chanfd, replyBuf) < 0) {
                ls_syslog(LOG_ERR, "%s chan_enqueue() failed", __func__);
                chan_free_buf(replyBuf);
            } else {
                sbdPtr->reqCode = MBD_NEW_JOB_KEEP_CHAN;
            }
        } else {
            hdr.operation = LSBE_NO_ERROR;

            s = chan_sock(sbdPtr->chanfd);
            io_block(s);

            cc = send_packet_header(sbdPtr->chanfd, &hdr);
            if (cc < 0) {
                ls_syslog(LOG_ERR, "%s: send_packet_header failed: %m",
                          __func__, lsb_jobid2str(jData->jobId));
            }
        }
    }

Leave:

    xdr_destroy(&xdrs);
    chan_free_buf(buf);
}

void doProbeReply(struct sbdNode *sbdPtr, int exception)
{
    static char fname[] = "doProbeReply";
    struct packet_header replyHdr;
    XDR xdrs;
    char *toHost = sbdPtr->hData->host;
    struct Buffer *buf;

    if (logclass & LC_COMM)
        ls_syslog(LOG_DEBUG, "%s: Entering ...", fname);

    if (exception == TRUE || chan_dequeue(sbdPtr->chanfd, &buf) < 0) {
        if (exception == TRUE)
            ls_syslog(LOG_ERR, "%s: Exception bit of <%d> is set for host <%s>",
                      fname, sbdPtr->chanfd, toHost);
        else
            ls_syslog(LOG_ERR, "%s", __func__, toHost, "chan_dequeue");
        sbdPtr->hData->flags |= HOST_NEEDPOLL;
        return;
    }

    xdrmem_create(&xdrs, buf->data, buf->len, XDR_DECODE);

    if (!xdr_pack_hdr(&xdrs, &replyHdr)) {
        ls_syslog(LOG_ERR, "%s", __func__, toHost, "xdr_pack_hdr");
        sbdPtr->hData->flags |= HOST_NEEDPOLL;
    } else {
#ifdef INTER_DAEMON_AUTH
        if (authSbdRequest(sbdPtr, &xdrs, &replyHdr, NULL) != LSBE_NO_ERROR) {
            ls_syslog(LOG_ERR, "%s", __func__, toHost, "authSbdRequest");
            sbdPtr->hData->flags |= HOST_NEEDPOLL;
        } else {
#endif

            if (replyHdr.operation != ERR_NO_ERROR) {
                ls_syslog(LOG_ERR, "%s: sbatchd replied <%d> for host <%s>",
                          fname, replyHdr.operation, toHost);
            } else {
                if (logclass & LC_COMM)
                    ls_syslog(LOG_DEBUG, "%s: Got ok probe reply from <%s>",
                              fname, toHost);
            }
#ifdef INTER_DAEMON_AUTH
        }
#endif
    }

    xdr_destroy(&xdrs);
    chan_free_buf(buf);
}

void doSwitchJobReply(struct sbdNode *sbdPtr, int exception)
{
    static char fname[] = "doSwitchJobReply";
    struct packet_header replyHdr;
    XDR xdrs;
    struct jData *jData = sbdPtr->jData;
    char *toHost = sbdPtr->hData->host;
    struct jobReply jobReply;
    struct Buffer *buf;

    if (logclass & LC_COMM)
        ls_syslog(LOG_DEBUG, "%s: Entering ...", fname);

    if (!IS_START(jData->jStatus))

        return;

    if (exception == TRUE || chan_dequeue(sbdPtr->chanfd, &buf) < 0) {
        if (exception == TRUE)
            ls_syslog(LOG_ERR, "%s: Exception bit of <%d> is set for job <%s>",
                      fname, sbdPtr->chanfd, lsb_jobid2str(jData->jobId));
        else
            ls_syslog(LOG_ERR, "%s", __func__, lsb_jobid2str(jData->jobId),
                      "chan_dequeue");
        jData->pendEvent.notSwitched = TRUE;
        eventPending = TRUE;
        return;
    }

    xdrmem_create(&xdrs, buf->data, buf->len, XDR_DECODE);

    if (!xdr_pack_hdr(&xdrs, &replyHdr)) {
        ls_syslog(LOG_ERR, "%s", __func__, lsb_jobid2str(jData->jobId),
                  "xdr_pack_hdr");
        jData->pendEvent.notSwitched = TRUE;
        eventPending = TRUE;
        goto Leave;
    }

#ifdef INTER_DAEMON_AUTH
    if (authSbdRequest(sbdPtr, &xdrs, &replyHdr, NULL) != LSBE_NO_ERROR) {
        ls_syslog(LOG_ERR, "%s", __func__, lsb_jobid2str(jData->jobId),
                  "authSbdRequest");
        jData->pendEvent.notSwitched = TRUE;
        eventPending = TRUE;
        goto Leave;
    }
#endif

    switch (replyHdr.operation) {
    case ERR_NO_ERROR:
        if (!xdr_jobReply(&xdrs, &jobReply, &replyHdr) ||
            jData->jobId != jobReply.jobId) {
            if (jData->jobId != jobReply.jobId)
                ls_syslog(LOG_ERR, "%s: Got bad jobId <%s> for job <%s>", fname,
                          lsb_jobid2str(jobReply.jobId),
                          lsb_jobid2str(jData->jobId));
            else
                ls_syslog(LOG_ERR, "%s", __func__, lsb_jobid2str(jData->jobId),
                          "xdr_jobReply");
        }
        break;
    case ERR_NO_JOB:
        job_abort(jData, JOB_MISSING);
        ls_syslog(LOG_INFO,
                  ("%s: Job <%s> was missing by sbatchd on host <%s>\n"), fname,
                  lsb_jobid2str(jData->jobId), toHost);

        break;
    case ERR_JOB_FINISH:
        break;
    case ERR_BAD_REQ:
        ls_syslog(
            LOG_ERR,
            "%s: Job <%s>: sbatchd on host <%s> complained of bad request",
            fname, lsb_jobid2str(jData->jobId), toHost);
        break;
    default:
        ls_syslog(
            LOG_ERR,
            "%s: Job <%s>: Illegal reply code <%d> from sbatchd on host <%s>",
            fname, lsb_jobid2str(jData->jobId), replyHdr.operation, toHost);
    }

Leave:

    xdr_destroy(&xdrs);
    chan_free_buf(buf);
}

void doSignalJobReply(struct sbdNode *sbdPtr, int exception)
{
    static char fname[] = "doSignalJobReply";
    struct packet_header replyHdr;
    XDR xdrs;
    struct jData *jData = sbdPtr->jData;

    struct jobReply jobReply;
    struct Buffer *buf;

    if (logclass & LC_COMM)
        ls_syslog(LOG_DEBUG, "%s: Entering ...", fname);

    if (IS_FINISH(jData->jStatus)) {
        return;
    } else if (IS_PEND(jData->jStatus)) {
        if (sbdPtr->sigVal != SIG_CHKPNT && sbdPtr->sigVal != SIG_CHKPNT_COPY) {
            sigPFjob(jData, sbdPtr->sigVal, 0, now);
        }
        return;
    }

    if (exception == TRUE || chan_dequeue(sbdPtr->chanfd, &buf) < 0) {
        if (exception == TRUE)
            ls_syslog(LOG_ERR, "%s: Exception bit of <%d> is set for job <%s>",
                      fname, sbdPtr->chanfd, lsb_jobid2str(jData->jobId));
        else
            ls_syslog(LOG_ERR, "%s", __func__, lsb_jobid2str(jData->jobId),
                      "chan_dequeue");
        addPendSigEvent(sbdPtr);
        return;
    }

    xdrmem_create(&xdrs, buf->data, buf->len, XDR_DECODE);

    if (!xdr_pack_hdr(&xdrs, &replyHdr)) {
        ls_syslog(LOG_ERR, "%s", __func__, lsb_jobid2str(jData->jobId),
                  "xdr_pack_hdr");
        addPendSigEvent(sbdPtr);
        goto Leave;
    }

#ifdef INTER_DAEMON_AUTH
    if (authSbdRequest(sbdPtr, &xdrs, &replyHdr, NULL) != LSBE_NO_ERROR) {
        ls_syslog(LOG_ERR, "%s", __func__, lsb_jobid2str(jData->jobId),
                  "authSbdRequest");
        addPendSigEvent(sbdPtr);
        goto Leave;
    }
#endif

    if (logclass & LC_SIGNAL)
        ls_syslog(LOG_DEBUG, "%s: Job <%s> sigVal %d got reply code=%d", fname,
                  lsb_jobid2str(jData->jobId), sbdPtr->sigVal,
                  replyHdr.operation);

    signalReplyCode(replyHdr.operation, jData, sbdPtr->sigVal,
                    sbdPtr->sigFlags);

    switch (replyHdr.operation) {
    case ERR_NO_ERROR:
        if (!xdr_jobReply(&xdrs, &jobReply, &replyHdr) ||
            jData->jobId != jobReply.jobId) {
            if (jData->jobId != jobReply.jobId)
                ls_syslog(LOG_ERR, "%s: Got bad jobId <%s> for job <%s>", fname,
                          lsb_jobid2str(jobReply.jobId), jData->jobId);
            else
                ls_syslog(LOG_ERR, "%s", __func__, lsb_jobid2str(jData->jobId),
                          "xdr_jobReply");
            addPendSigEvent(sbdPtr);
        } else {
            jobStatusSignal(replyHdr.operation, jData, sbdPtr->sigVal,
                            sbdPtr->sigFlags, &jobReply);
        }
        break;

    case ERR_NO_JOB:
        if (getJobData(jData->jobId) != NULL)
            addPendSigEvent(sbdPtr);
        break;

    default:
        addPendSigEvent(sbdPtr);
    }

Leave:

    xdr_destroy(&xdrs);
    chan_free_buf(buf);
}

static void addPendSigEvent(struct sbdNode *sbdPtr)
{
    struct jData *jData = sbdPtr->jData;

    if ((sbdPtr->sigVal == SIG_CHKPNT) || (sbdPtr->sigVal == SIG_CHKPNT_COPY)) {
        if (jData->pendEvent.sig1 == SIG_NULL) {
            jData->pendEvent.sig1 = sbdPtr->sigVal;
            jData->pendEvent.sig1Flags = sbdPtr->sigFlags;
        }
    } else {
        if (jData->pendEvent.sig == SIG_NULL)
            jData->pendEvent.sig = sbdPtr->sigVal;
    }

    eventPending = TRUE;
}
int do_resourceInfoReq(XDR *xdrs, int chfd, struct sockaddr_in *from,
                       struct packet_header *reqHdr)
{
    static char fname[] = "do_resourceInfoReq";
    XDR xdrs2;
    static struct resourceInfoReq resInfoReq;
    struct lsbShareResourceInfoReply resInfoReply;
    int reply;
    char *reply_buf;
    int len = 0, i, j;
    static struct packet_header replyHdr;
    char *replyStruct;

    if (logclass & LC_TRACE)
        ls_syslog(LOG_DEBUG1, "%s: Entering this routine...", fname);

    resInfoReply.numResources = 0;
    resInfoReply.resources = NULL;

    if (resInfoReq.numResourceNames > 0)
        xdr_lsffree(xdr_resourceInfoReq, (char *) &resInfoReq, &replyHdr);

    if (!xdr_resourceInfoReq(xdrs, &resInfoReq, reqHdr)) {
        reply = LSBE_XDR;
        ls_syslog(LOG_ERR, "%s", __func__, "xdr_resourceInfoReq");
        len = MSGSIZE;
    } else {
        if (numResources == 0) {
            reply = LSBE_NO_RESOURCE;
            len = MSGSIZE;
        } else {
            reply = checkResources(&resInfoReq, &resInfoReply);

            len = ALIGNWORD_(
                      resInfoReply.numResources *
                      (sizeof(struct lsbSharedResourceInfo) + MAXLSFNAMELEN)) +
                  100;

            for (i = 0; i < resInfoReply.numResources; i++) {
                for (j = 0; j < resInfoReply.resources[i].nInstances; j++) {
                    len += ALIGNWORD_(
                               sizeof(struct lsbSharedResourceInstance) +
                               MAXLSFNAMELEN +
                               (resInfoReply.resources[i].instances[j].nHosts *
                                MAXHOSTNAMELEN)) +
                           4;
                }
            }
        }
        len += 4 * MSGSIZE;
    }
    reply_buf = (char *) my_malloc(len, fname);
    xdrmem_create(&xdrs2, reply_buf, len, XDR_ENCODE);
    replyHdr.operation = reply;
    if (reply == LSBE_NO_ERROR || reply == LSBE_BAD_RESOURCE)
        replyStruct = (char *) &resInfoReply;
    else
        replyStruct = (char *) 0;
    if (!xdr_encodeMsg(&xdrs2, replyStruct, &replyHdr,
                       xdr_lsbShareResourceInfoReply, 0, NULL)) {
        ls_syslog(LOG_ERR, "%s", __func__, "xdr_encodeMsg");
        xdr_destroy(&xdrs2);
        FREEUP(reply_buf);
        freeShareResourceInfoReply(&resInfoReply);
        return -1;
    }
    if (chan_write(chfd, reply_buf, XDR_GETPOS(&xdrs2)) <= 0) {
        ls_syslog(LOG_ERR, "%s", __func__, "b_write_fix", XDR_GETPOS(&xdrs2));
        xdr_destroy(&xdrs2);
        FREEUP(reply_buf);
        freeShareResourceInfoReply(&resInfoReply);
        return -1;
    }
    xdr_destroy(&xdrs2);
    FREEUP(reply_buf);
    freeShareResourceInfoReply(&resInfoReply);
    return 0;
}

static void freeJobHead(struct jobInfoHead *jobInfoHead)
{
    if (jobInfoHead == NULL)
        return;

    FREEUP(jobInfoHead->jobIds);
    FREEUP(jobInfoHead->hostNames);
}

static void freeJobInfoReply(struct jobInfoReply *job)
{
    int i;

    if (job == NULL)
        return;

    freeSubmitReq(job->jobBill);
    if (job->numToHosts > 0) {
        for (i = 0; i < job->numToHosts; i++)
            FREEUP(job->toHosts[i]);
        FREEUP(job->toHosts);
    }
}

static void freeShareResourceInfoReply(struct lsbShareResourceInfoReply *reply)
{
    int i, j, k;

    if (reply == NULL)
        return;

    for (i = 0; i < reply->numResources; i++) {
        for (j = 0; j < reply->resources[i].nInstances; j++) {
            FREEUP(reply->resources[i].instances[j].totalValue);
            FREEUP(reply->resources[i].instances[j].rsvValue);
            for (k = 0; k < reply->resources[i].instances[j].nHosts; k++)
                FREEUP(reply->resources[i].instances[j].hostList[k]);
            FREEUP(reply->resources[i].instances[j].hostList);
        }
        FREEUP(reply->resources[i].instances);
    }
    FREEUP(reply->resources);
}

int do_runJobReq(XDR *xdrs, int chfd, struct sockaddr_in *from,
                 struct lsfAuth *auth, struct packet_header *reqHeader)
{
    struct runJobRequest runJobRequest;
    XDR replyXdr;
    struct packet_header lsfHeader;
    char reply_buf[MSGSIZE / 2];
    int reply;

    memset((struct runJobRequest *) &runJobRequest, 0,
           sizeof(struct runJobRequest));

    if (!xdr_runJobReq(xdrs, &runJobRequest, reqHeader)) {
        reply = LSBE_XDR;
        ls_syslog(LOG_ERR, "%s", __func__, "xdr_runJobReq");
        goto Reply;
    }

    reply = runJob(&runJobRequest, auth);

Reply:

    xdr_lsffree(xdr_runJobReq, (char *) &runJobRequest, reqHeader);

    xdrmem_create(&replyXdr, reply_buf, MSGSIZE / 2, XDR_ENCODE);

    lsfHeader.operation = reply;

    if (!xdr_encodeMsg(&replyXdr, NULL, &lsfHeader, xdr_int, 0, NULL)) {
        ls_syslog(LOG_ERR, "%s", __func__, "xdr_encodeMsg");
        xdr_destroy(&replyXdr);
        return -1;
    }

    if (chan_write(chfd, reply_buf, XDR_GETPOS(&replyXdr)) <= 0) {
        ls_syslog(LOG_ERR, "%s", __func__, "chan_write");
        xdr_destroy(&replyXdr);
        return -1;
    }

    xdr_destroy(&replyXdr);
    return 0;
}

int do_setJobAttr(XDR *xdrs, int s, struct sockaddr_in *from, char *hostName,
                  struct packet_header *reqHdr, struct lsfAuth *auth)
{
    struct jobAttrInfoEnt jobAttr;
    struct jData *job;
    XDR xdrs2;
    char reply_buf[MSGSIZE];
    struct packet_header replyHdr;
    int reply;

    if (!xdr_jobAttrReq(xdrs, &jobAttr, reqHdr)) {
        reply = LSBE_XDR;
        ls_syslog(LOG_ERR, "%s", __func__, "xdr_jobAttrReq");
    } else {
        if ((job = getJobData(jobAttr.jobId)) == NULL) {
            reply = LSBE_NO_JOB;
            ls_syslog(LOG_DEBUG, "do_setJobAttr: no such job (%s)",
                      lsb_jobid2str(jobAttr.jobId));
        } else if (IS_FINISH(job->jStatus)) {
            reply = LSBE_JOB_FINISH;
            ls_syslog(LOG_DEBUG, "do_setJobAttr: job (%s) finished already",
                      lsb_jobid2str(jobAttr.jobId));
        } else if (!isJobOwner(auth, job)) {
            reply = LSBE_PERMISSION;
            ls_syslog(LOG_DEBUG, "do_setJobAttr: no permission for job (%s)",
                      lsb_jobid2str(jobAttr.jobId));
        } else if (IS_START(job->jStatus)) {
            reply = LSBE_NO_ERROR;
            job->port = jobAttr.port;
            strcpy(jobAttr.hostname, job->hPtr[0]->host);
            log_jobattrset(&jobAttr, auth->uid);
        } else {
            reply = LSBE_JOB_MODIFY;
            ls_syslog(LOG_DEBUG, "do_setJobAttr: job (%s) is not modified",
                      lsb_jobid2str(jobAttr.jobId));
        }
    }

    xdrmem_create(&xdrs2, reply_buf, MSGSIZE, XDR_ENCODE);
    replyHdr.operation = reply;
    if (!xdr_encodeMsg(&xdrs2, (char *) &jobAttr, &replyHdr, xdr_jobAttrReq, 0,
                       NULL)) {
        ls_syslog(LOG_ERR, "%s", __func__, "xdr_encodeMsg");
        xdr_destroy(&xdrs2);
        return -1;
    }
    if (chan_write(s, reply_buf, XDR_GETPOS(&xdrs2)) != XDR_GETPOS(&xdrs2)) {
        ls_syslog(LOG_ERR, "%s", __func__, "chan_write");
        xdr_destroy(&xdrs2);
        return -1;
    }
    xdr_destroy(&xdrs2);
    return 0;
}

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

    LS_INFO("sbatchd register hostname=%s canon=%s addr=%s ch_id=%d",
            hostname, host_data->sbd_node->host.name,
            host_data->sbd_node->host.addr, host_data->sbd_node->chanfd);

    return enqueue_header_reply(mbd_efd, client->chanfd, SBD_REGISTER_REPLY);
}
