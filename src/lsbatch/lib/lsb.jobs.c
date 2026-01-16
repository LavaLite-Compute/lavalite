/* ------------------------------------------------------------------------
 * LavaLite — High-Performance Job Scheduling Infrastructure
 *
 * Copyright (C) LavaLite Contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 * ------------------------------------------------------------------------ */

#include "lsbatch/lib/lsb.h"

extern void copyJUsage(struct jRusage *to, struct jRusage *from);

static int mbd_sock = -1;

/* Allocated once, reused across reads */
static struct jobInfoReply jobInfoReply;
static struct jobInfoEnt jobInfo;
static struct submitReq submitReq;
static int buffers_initialized = 0;

/* Rusage state preserved across calls */
static int npids = 0;
static struct pidInfo *pidInfo = NULL;
static int npgids = 0;
static int *pgid = NULL;

/* init_jobinfo_buffers() - Allocate reusable buffers
 * Returns 0 on success, -1 on failure
 */
static int init_jobinfo_buffers(struct jobInfoReply *reply,
                                struct submitReq *submit)
{
    if (buffers_initialized)
        return 0;

    submit->fromHost = malloc(MAXHOSTNAMELEN);
    submit->jobFile = malloc(MAXFILENAMELEN);
    submit->inFile = malloc(MAXFILENAMELEN);
    submit->outFile = malloc(MAXFILENAMELEN);
    submit->errFile = malloc(MAXFILENAMELEN);
    submit->inFileSpool = malloc(MAXFILENAMELEN);
    submit->commandSpool = malloc(MAXFILENAMELEN);
    submit->hostSpec = malloc(MAXHOSTNAMELEN);
    submit->chkpntDir = malloc(MAXFILENAMELEN);
    submit->subHomeDir = malloc(MAXFILENAMELEN);
    submit->cwd = malloc(MAXFILENAMELEN);
    reply->userName = malloc(MAXLSFNAMELEN);

    if (!submit->fromHost || !submit->jobFile || !submit->inFile ||
        !submit->outFile || !submit->errFile || !submit->inFileSpool ||
        !submit->commandSpool || !submit->hostSpec || !submit->chkpntDir ||
        !submit->subHomeDir || !submit->cwd || !reply->userName) {
        return -1;
    }

    submit->xf = NULL;
    submit->nxf = 0;
    submit->numAskedHosts = 0;
    reply->numToHosts = 0;
    reply->toHosts = NULL;
    reply->jobBill = submit;

    buffers_initialized = 1;

    return 0;
}

/* free_previous_job() - Release per-job allocations before reading next
 */

static void free_previous_job(void)
{
    int i;

    if (jobInfoReply.numToHosts > 0) {
        for (i = 0; i < jobInfoReply.numToHosts; i++)
            FREEUP(jobInfoReply.toHosts[i]);
        FREEUP(jobInfoReply.toHosts);
        jobInfoReply.numToHosts = 0;
    }

    if (submitReq.xf) {
        free(submitReq.xf);
        submitReq.xf = NULL;
    }

    free(jobInfoReply.execHome);
    free(jobInfoReply.execCwd);
    free(jobInfoReply.execUsername);
    free(jobInfoReply.parentGroup);
    free(jobInfoReply.jName);
}

/* ========================================================================
 * copy_job_info() - Map jobInfoReply to jobInfoEnt
 *
 * Direct field copy where names match, plus field renames:
 *   numToHosts -> numExHosts, toHosts -> exHosts, userName -> user
 * ======================================================================== */

static void copy_job_info(struct jobInfoEnt *dst, struct jobInfoReply *src)
{
    int i;

    dst->jobId = src->jobId;
    dst->status = src->status;
    dst->reasonTb = src->reasonTb;
    dst->numReasons = src->numReasons;
    dst->reasons = src->reasons;
    dst->subreasons = src->subreasons;
    dst->startTime = src->startTime;
    dst->predictedStartTime = src->predictedStartTime;
    dst->endTime = src->endTime;
    dst->cpuTime = src->cpuTime;
    dst->reserveTime = src->reserveTime;
    dst->jobPid = src->jobPid;
    dst->port = src->port;
    dst->jobPriority = src->jobPriority;
    dst->nIdx = src->nIdx;
    dst->loadSched = src->loadSched;
    dst->loadStop = src->loadStop;
    dst->exitStatus = src->exitStatus;
    dst->execUid = src->execUid;
    dst->jType = src->jType;
    dst->jRusageUpdateTime = src->jRusageUpdateTime;

    /* Field renames */
    dst->numExHosts = src->numToHosts;
    dst->exHosts = src->toHosts;
    dst->user = src->userName;

    /* String pointers - shared, not copied */
    dst->execHome = src->execHome;
    dst->execCwd = src->execCwd;
    dst->execUsername = src->execUsername;
    dst->parentGroup = src->parentGroup;
    dst->jName = src->jName;

    /* Counters */
    for (i = 0; i < NUM_JGRP_COUNTERS; i++)
        dst->counter[i] = src->counter[i];

    /* Fields from jobBill (submitReq) */
    dst->submitTime = src->jobBill->submitTime;
    dst->umask = src->jobBill->umask;
    dst->cwd = src->jobBill->cwd;
    dst->subHomeDir = src->jobBill->subHomeDir;
    dst->fromHost = src->jobBill->fromHost;

    /* Submit struct */
    dst->submit.options = src->jobBill->options;
    dst->submit.options2 = src->jobBill->options2;
    dst->submit.numProcessors = src->jobBill->numProcessors;
    dst->submit.maxNumProcessors = src->jobBill->maxNumProcessors;
    dst->submit.jobName = src->jobBill->jobName;
    dst->submit.command = src->jobBill->command;
    dst->submit.resReq = src->jobBill->resReq;
    dst->submit.queue = src->jobBill->queue;
    dst->submit.inFile = src->jobBill->inFile;
    dst->submit.outFile = src->jobBill->outFile;
    dst->submit.errFile = src->jobBill->errFile;
    dst->submit.beginTime = src->jobBill->beginTime;
    dst->submit.termTime = src->jobBill->termTime;
    dst->submit.userPriority = src->jobBill->userPriority;
    dst->submit.hostSpec = src->jobBill->hostSpec;
    dst->submit.sigValue = src->jobBill->sigValue;
    dst->submit.chkpntDir = src->jobBill->chkpntDir;
    dst->submit.dependCond = src->jobBill->dependCond;
    dst->submit.preExecCmd = src->jobBill->preExecCmd;
    dst->submit.chkpntPeriod = src->jobBill->chkpntPeriod;
    dst->submit.numAskedHosts = src->jobBill->numAskedHosts;
    dst->submit.askedHosts = src->jobBill->askedHosts;
    dst->submit.projectName = src->jobBill->projectName;
    dst->submit.mailUser = src->jobBill->mailUser;
    dst->submit.loginShell = src->jobBill->loginShell;
    dst->submit.nxf = src->jobBill->nxf;
    dst->submit.xf = src->jobBill->xf;

    for (i = 0; i < LSF_RLIM_NLIMITS; i++)
        dst->submit.rLimits[i] = src->jobBill->rLimits[i];
}

/*
 * lsb_openjobinfo() - Open streaming job query
 */
struct jobInfoHead *lsb_openjobinfo(int64_t jobId, const char *jobName,
                                    const char *userName, const char *queueName,
                                    const char *hostName, int options)
{
    static __thread struct jobInfoReq jobInfoReq;
    static __thread struct jobInfoHead jobInfoHead;
    static int req_initialized = 0;
    XDR xdrs;
    XDR xdrs2;
    char request_buf[LL_BUFSIZ_1K];
    char *reply_buf;
    struct packet_header hdr;

    /* Allocate request buffers once */
    if (!req_initialized) {
        jobInfoReq.userName = calloc(LL_BUFSIZ_32, sizeof(char));
        jobInfoReq.jobName = calloc(LL_BUFSIZ_256, sizeof(char));
        jobInfoReq.queue = calloc(LL_BUFSIZ_32, sizeof(char));
        jobInfoReq.host = calloc(MAXHOSTNAMELEN, sizeof(char));

        if (!jobInfoReq.jobName || !jobInfoReq.queue || !jobInfoReq.userName ||
            !jobInfoReq.host) {
            lsberrno = LSBE_SYS_CALL;
            return NULL;
        }
        req_initialized = 1;
    }

    /* Build request - queue */
    if (queueName == NULL) {
        jobInfoReq.queue[0] = 0;
    } else if (strlen(queueName) >= LL_BUFSIZ_32 - 1) {
        lsberrno = LSBE_BAD_QUEUE;
        return NULL;
    } else {
        strcpy(jobInfoReq.queue, queueName);
    }

    /* Host */
    if (hostName == NULL)
        jobInfoReq.host[0] = 0;
    else
        strcpy(jobInfoReq.host, hostName);

    /* Job name */
    if (jobName == NULL) {
        jobInfoReq.jobName[0] = 0;
    } else if (strlen(jobName) >= MAX_CMD_DESC_LEN - 1) {
        lsberrno = LSBE_BAD_JOB;
        return NULL;
    } else {
        strcpy(jobInfoReq.jobName, jobName);
    }

    /* User name */
    if (userName == NULL) {
        struct passwd *pwd = getpwuid(getuid());
        strcpy(jobInfoReq.userName, pwd->pw_name);
    } else if (strlen(userName) >= LL_BUFSIZ_32 - 1) {
        lsberrno = LSBE_BAD_USER;
        return NULL;
    } else {
        strcpy(jobInfoReq.userName, userName);
    }

    /* Options */
    if ((options &
         ~(JOBID_ONLY | JOBID_ONLY_ALL | HOST_NAME | NO_PEND_REASONS)) == 0)
        jobInfoReq.options = CUR_JOB;
    else
        jobInfoReq.options = options;

    if (jobId < 0) {
        lsberrno = LSBE_BAD_ARG;
        return NULL;
    }
    jobInfoReq.jobId = jobId;

    /* Encode */
    xdrmem_create(&xdrs, request_buf, MSGSIZE, XDR_ENCODE);
    init_pack_hdr(&hdr);
    hdr.operation = BATCH_JOB_INFO;

    if (!xdr_encodeMsg(&xdrs, (char *) &jobInfoReq, &hdr, xdr_jobInfoReq, 0,
                       NULL)) {
        lsberrno = LSBE_XDR;
        xdr_destroy(&xdrs);
        return NULL;
    }

    /* Open stream */
    mbd_sock =
        open_mbd_stream(request_buf, XDR_GETPOS(&xdrs), &reply_buf, &hdr);
    xdr_destroy(&xdrs);

    if (mbd_sock < 0)
        return NULL;

    lsberrno = hdr.operation;
    if (lsberrno != LSBE_NO_ERROR) {
        free(reply_buf);
        close_mbd_stream(mbd_sock);
        mbd_sock = -1;
        return NULL;
    }

    /* Decode header
     */
    xdrmem_create(&xdrs2, reply_buf, hdr.length, XDR_DECODE);
    if (!xdr_jobInfoHead(&xdrs2, &jobInfoHead, &hdr)) {
        lsberrno = LSBE_XDR;
        xdr_destroy(&xdrs2);
        free(reply_buf);
        close_mbd_stream(mbd_sock);
        mbd_sock = -1;
        return NULL;
    }
    xdr_destroy(&xdrs2);
    free(reply_buf);

    return &jobInfoHead;
}

/* ========================================================================
 * lsb_readjobinfo() - Read next job from stream
 * ======================================================================== */

struct jobInfoEnt *lsb_readjobinfo(void)
{
    XDR xdrs;
    struct packet_header hdr;
    char *buffer = NULL;
    int num;
    static __thread struct jobInfoReply jobInfoReply;
    static __thread struct jobInfoEnt jobInfo;
    static __thread struct submitReq submitReq;
    static __thread int buffers_initialized = 0;

    /* Init buffers on first use
     */
    if (init_jobinfo_buffers(&jobInfoReply, &submitReq) < 0) {
        lsberrno = LSBE_NO_MEM;
        return NULL;
    }

    /* Read next packet */
    num = read_mbd_stream(mbd_sock, &buffer, &hdr);
    if (num < 0) {
        close_mbd_stream(mbd_sock);
        mbd_sock = -1;
        lsberrno = LSBE_EOF;
        return NULL;
    }

    /* Free previous job's dynamic data */
    free_previous_job();

    /* Decode */
    xdrmem_create(&xdrs, buffer, hdr.length, XDR_DECODE);
    if (!xdr_jobInfoReply(&xdrs, &jobInfoReply, &hdr)) {
        lsberrno = LSBE_XDR;
        xdr_destroy(&xdrs);
        free(buffer);
        return NULL;
    }
    xdr_destroy(&xdrs);
    free(buffer);

    /* Copy to output struct */
    copy_job_info(&jobInfo, &jobInfoReply);

    /* Handle rusage - preserve across calls */
    jobInfo.runRusage.npids = npids;
    jobInfo.runRusage.pidInfo = pidInfo;
    jobInfo.runRusage.npgids = npgids;
    jobInfo.runRusage.pgid = pgid;

    copyJUsage(&jobInfo.runRusage, &jobInfoReply.runRusage);

    npids = jobInfo.runRusage.npids;
    pidInfo = jobInfo.runRusage.pidInfo;
    npgids = jobInfo.runRusage.npgids;
    pgid = jobInfo.runRusage.pgid;

    /* Free reply's rusage copies */
    if (jobInfoReply.runRusage.npids > 0) {
        free(jobInfoReply.runRusage.pidInfo);
        jobInfoReply.runRusage.npids = 0;
    }
    if (jobInfoReply.runRusage.npgids > 0) {
        free(jobInfoReply.runRusage.pgid);
        jobInfoReply.runRusage.npgids = 0;
    }

    return &jobInfo;
}

/* ========================================================================
 * lsb_closejobinfo() - Close stream
 * ======================================================================== */

void lsb_closejobinfo(void)
{
    close_mbd_stream(mbd_sock);
    mbd_sock = -1;
}

/* ========================================================================
 * lsb_runjob() - Force run a job (simple RPC)
 * ======================================================================== */

int lsb_runjob(struct runJobRequest *req)
{
    XDR xdrs;
    struct packet_header hdr;
    struct lsfAuth auth;
    char request_buf[LL_BUFSIZ_1K];
    char *reply_buf;
    int cc;

    if (!req || req->numHosts == 0 || !req->hostname || req->jobId < 0) {
        lsberrno = LSBE_BAD_ARG;
        return -1;
    }

    if (req->options != 0 &&
        !(req->options & (RUNJOB_OPT_NORMAL | RUNJOB_OPT_NOSTOP))) {
        lsberrno = LSBE_BAD_ARG;
        return -1;
    }

    if (!(req->options & (RUNJOB_OPT_NORMAL | RUNJOB_OPT_NOSTOP)))
        req->options |= RUNJOB_OPT_NORMAL;

    if (authTicketTokens_(&auth, NULL) == -1) {
        lsberrno = LSBE_LSBLIB;
        return -1;
    }

    xdrmem_create(&xdrs, request_buf, MSGSIZE / 2, XDR_ENCODE);
    init_pack_hdr(&hdr);
    hdr.operation = BATCH_JOB_FORCE;

    if (!xdr_encodeMsg(&xdrs, (char *) req, &hdr, xdr_runJobReq, 0, &auth)) {
        lsberrno = LSBE_XDR;
        xdr_destroy(&xdrs);
        return -1;
    }

    cc = call_mbd(request_buf, XDR_GETPOS(&xdrs), &reply_buf, &hdr, NULL);
    xdr_destroy(&xdrs);
    if (cc == -1)
        return -1;

    lsberrno = hdr.operation;

    if (cc > 0)
        free(reply_buf);

    return (lsberrno == LSBE_NO_ERROR) ? 0 : -1;
}

/* ========================================================================
 * Job ID formatting helpers
 * ======================================================================== */

char *lsb_jobid2str(int64_t jobId)
{
    static char s[LL_BUFSIZ_32];

    if (LSB_ARRAY_IDX(jobId) == 0)
        sprintf(s, "%d", LSB_ARRAY_JOBID(jobId));
    else
        sprintf(s, "%d[%d]", LSB_ARRAY_JOBID(jobId), LSB_ARRAY_IDX(jobId));

    return s;
}

char *lsb_jobidinstr(int64_t jobId)
{
    static char s[LL_BUFSIZ_32];
    sprintf(s, "%ld", jobId);
    return s;
}
