/* $Id: mbd.serv.c,v 1.29 2007/08/15 22:18:46 tmizan Exp $
 * Copyright (C) 2007 Platform Computing Inc
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 *
 */

#include <stdlib.h>
#include "mbd.h"

#include <dirent.h>
#include <malloc.h>

#define NL_SETN		10	

static unsigned int     msgcnt = 0;
extern int numLsbUsable;
extern char *env_dir;
extern int lsb_CheckMode;
extern int lsb_CheckError;
extern  sbdReplyType callSbdDebug(struct debugReq *pdebug);
extern int getXdrStrlen(char *);
extern  bool_t xdr_resourceInfoReply (XDR *, struct resourceInfoReply *, 
                                                       struct LSFHeader *);
extern bool_t xdr_resourceInfoReq(XDR *, struct resourceInfoReq *, 
						       struct LSFHeader *);

extern char *jgrpNodeParentPath(struct jgTreeNode *);
static int packJgrpInfo(struct jgTreeNode *, int, char **, int, int);
static int packJobInfo(struct jData *, int, char **, int, int, int);
static void initSubmit(int *, struct submitReq *, struct submitMbdReply *);
static int sendBack(int, struct submitReq *, struct submitMbdReply *, int);
static void addPendSigEvent(struct sbdNode *sbdPtr);
static void freeJobHead (struct jobInfoHead *);
static void freeJobInfoReply (struct jobInfoReply *);
static void freeShareResourceInfoReply (struct  lsbShareResourceInfoReply *);
static int xdrsize_QueueInfoReply(struct queueInfoReply * );
#ifdef INTER_DAEMON_AUTH
static int authSbdRequest(struct sbdNode *, XDR *,
			  struct LSFHeader *, struct sockaddr_in *);
#endif

extern void closeSession(int);
extern int getHdrReserved(struct LSFHeader *);
extern void setHdrReserved(struct LSFHeader *, unsigned int);
extern int cpHostent(struct hostent *, const struct hostent *);
extern void freeHp(struct hostent *);



int
do_submitReq (XDR *xdrs, int chfd, struct sockaddr_in *from, char *hostName,
	      struct LSFHeader *reqHdr, struct sockaddr_in *laddr,
	      struct lsfAuth *auth, int *schedule, int dispatch, 
              struct jData **jobData)
{
    static char             fname[] = "do_submitReq";
    static struct submitMbdReply submitReply;
    static int              first = TRUE;
    static struct submitReq subReq;
    int                     reply;

    if (logclass & (LC_TRACE | LC_EXEC | LC_COMM))
        ls_syslog(LOG_DEBUG, "%s: Entering this routine...; host=%s, socket=%d", fname, hostName, chanSock_(chfd));

    
    initSubmit(&first, &subReq, &submitReply);

    
    if (!xdr_submitReq(xdrs, &subReq, reqHdr)) {
	reply = LSBE_XDR;
	ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "xdr_submitReq");
	goto sendback;
    }

    
    if (!(subReq.options & SUB_RLIMIT_UNIT_IS_KB)) {
	convertRLimit(subReq.rLimits, 1);
    }

    reply = newJob (&subReq, &submitReply, chfd, auth, schedule, dispatch, 
                    jobData);
sendback:
    if (reply != 0 || submitReply.jobId <= 0 ) {
       if (logclass & (LC_TRACE | LC_EXEC )) {
          ls_syslog(LOG_DEBUG, "Job submission Failed due reason <%d> job <%d > JobName<%s> queue <%s> reqReq <%s> hostSpec <%s>  subHomeDir <%s> inFile <%s>  outFile <%s> errFile<%s> command <%s> inFileSpool <%s> commandSpool <%s> chkpntDir <%s> jobFile <%s> fromHost <%s>  cwd <%s> preExecCmd <%s>  mailUser <%s> projectName <%s> loginShell <%s> schedHostType <%s> numAskedHosts <%d> nxf <%d>  ",
                            reply,(nextJobId -1),
                            subReq.jobName,subReq.queue,
                            subReq.resReq,subReq.hostSpec,
                            subReq.subHomeDir,subReq.inFile,
                            subReq.outFile,subReq.errFile,
                            subReq.command,subReq.inFileSpool,
                            subReq.commandSpool,subReq.chkpntDir,
                            subReq.jobFile,subReq.fromHost,
                            subReq.cwd, subReq.preExecCmd,
                            subReq.mailUser,subReq.projectName,
                            subReq.loginShell,subReq.schedHostType,
                            subReq.numAskedHosts,subReq.nxf);
       }
    }
    if (logclass & (LC_TRACE | LC_EXEC )) {
        ls_syslog(LOG_DEBUG, "Job submission before sendBack reply <%d> job <%d > JobName <%s> queue <%s> reqReq <%s> hostSpec <%s> subHomeDir <%s> inFile <%s>  outFile <%s> errFile<%s> command <%s> inFileSpool <%s> commandSpool <%s> chkpntDir <%s>  jobFile <%s> fromHost <%s>  cwd <%s> preExecCmd <%s>  mailUser <%s> projectName <%s> loginShell <%s> schedHostType <%s>  numAskedHosts <%d> nxf <%d>  ",
                            reply,(nextJobId -1),
                            subReq.jobName,subReq.queue,
                            subReq.resReq,subReq.hostSpec,
                            subReq.subHomeDir,subReq.inFile,
                            subReq.outFile,subReq.errFile,
                            subReq.command,subReq.inFileSpool,
                            subReq.commandSpool,subReq.chkpntDir,
                            subReq.jobFile,subReq.fromHost,
                            subReq.cwd, subReq.preExecCmd,
                            subReq.mailUser,subReq.projectName,
                            subReq.loginShell,subReq.schedHostType,
                            subReq.numAskedHosts,subReq.nxf);
        }

    if (sendBack (reply, &subReq, &submitReply, chfd) < 0) {
        return (-1);
    }
    return (0);

} 

int
checkUseSelectJgrps(struct LSFHeader *reqHdr, struct jobInfoReq *req)
{
    
    if (req->jobId != 0 && !(req->options & JGRP_ARRAY_INFO))
        return(FALSE);
 
    
    if ((req->jobName[0] == '\0') &&  
         !(req->options & JGRP_ARRAY_INFO))
	return(FALSE);

    return(TRUE);
}


int
do_jobInfoReq (XDR *xdrs, int chfd, struct sockaddr_in *from,
               struct LSFHeader *reqHdr, int schedule)
{
    static char             fname[] = "do_jobInfoReq";
    char                    *reply_buf = NULL;	
    char                   *buf = NULL;
    XDR                     xdrs2;
    struct jobInfoReq       jobInfoReq;
    struct jobInfoHead      jobInfoHead;
    int                     reply = 0;
    int                     i, len, listSize = 0;
    struct LSFHeader        replyHdr;
    struct nodeList        *jgrplist = NULL;
    struct jData          **joblist = NULL; 
    int                     selectJgrpsFlag = FALSE;

    if (logclass & (LC_TRACE | LC_COMM))
        ls_syslog(LOG_DEBUG, "%s: Entering this routine...; channel=%d", fname,chfd);

    jobInfoHead.hostNames = NULL;
    jobInfoHead.jobIds  = NULL;

    if (!xdr_jobInfoReq(xdrs, &jobInfoReq, reqHdr)) {
	reply = LSBE_XDR;
	ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "xdr_jobInfoReq");
    }
    if (jobInfoReq.host[0] != '\0'      
        && getHGrpData(jobInfoReq.host) == NULL
        && !isValidHost_(jobInfoReq.host)
        && (strcmp(jobInfoReq.host, LOST_AND_FOUND) != 0))
        reply = LSBE_BAD_HOST;
    else {
	if ((selectJgrpsFlag = checkUseSelectJgrps(reqHdr, &jobInfoReq))
	    == TRUE) {
            reply = selectJgrps(&jobInfoReq, (void **)&jgrplist, &listSize);
        }
        else {
	    reply = selectJobs(&jobInfoReq, &joblist, &listSize);

	    
            jgrplist = (struct nodeList *) calloc(listSize,
                                                  sizeof(struct nodeList));
            for (i = 0; i < listSize; i++){
                jgrplist[i].info = (void *) joblist[i];
                jgrplist[i].isJData = TRUE;
            }
            FREEUP(joblist);
	}
    }


    

    
    xdr_lsffree(xdr_jobInfoReq, (char *) &jobInfoReq, reqHdr);

    jobInfoHead.numJobs = listSize;

    if (jobInfoHead.numJobs > 0)
        jobInfoHead.jobIds = (LS_LONG_INT *)my_calloc (listSize, sizeof(LS_LONG_INT), fname); 
    for (i = 0; i < listSize; i++){
	if (!jgrplist[i].isJData)
            jobInfoHead.jobIds[i] = 0;
        else
            jobInfoHead.jobIds[i] = ((struct jData *)jgrplist[i].info)->jobId;
    }

    jobInfoHead.numHosts = 0;
    if (jobInfoReq.options & HOST_NAME) {
        jobInfoHead.hostNames = (char **) my_calloc (numofhosts, 
                                          sizeof(char *), fname);
        for (i = 1; i <= numofhosts; i++)
            jobInfoHead.hostNames[i-1] = hDataPtrTb[i]->host;

        jobInfoHead.numHosts = numofhosts;
    }

    len = sizeof(struct jobInfoHead) 
	  + jobInfoHead.numJobs * sizeof(LS_LONG_INT)
          + jobInfoHead.numHosts * (sizeof(char *) + MAXHOSTNAMELEN)
          + 100;
       
    reply_buf = (char *)my_malloc (len, fname);
    xdrmem_create(&xdrs2, reply_buf, len, XDR_ENCODE);
    replyHdr.opCode = reply;

    if (!xdr_encodeMsg(&xdrs2, (char *) &jobInfoHead, &replyHdr, 
                       xdr_jobInfoHead, 0, NULL)) {
        FREEUP (reply_buf);
        freeJobHead (&jobInfoHead);
	ls_syslog(LOG_ERR, I18N_FUNC_S_FAIL, fname, 
	    "xdr_encodeMsg", "jobInfoHead");
        xdr_destroy(&xdrs2);
	FREEUP (jgrplist);
	return(-1);
    }
    len = XDR_GETPOS(&xdrs2);
    
    {
        if (chanWrite_(chfd, reply_buf, len) != len) {
            ls_syslog(LOG_ERR, I18N_FUNC_FAIL_M, fname, "chanWrite_");
            FREEUP (reply_buf);
	    freeJobHead (&jobInfoHead);
	    FREEUP (jgrplist);
       	    xdr_destroy(&xdrs2);
	    return(-1);
	}
    }
    FREEUP (reply_buf);

    xdr_destroy(&xdrs2);
    freeJobHead (&jobInfoHead);
    if (reply != LSBE_NO_ERROR || 
        (jobInfoReq.options & (JOBID_ONLY|JOBID_ONLY_ALL))) {
	FREEUP (jgrplist);
	return(0);
    }

    for (i = 0; i < listSize; i++) {
        if (jgrplist[i].isJData && 
            ((len = packJobInfo ((struct jData *)jgrplist[i].info, 
                                    listSize - 1 - i,  &buf, schedule,
				 jobInfoReq.options, reqHdr->version)) < 0)) {
            ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "packJobInfo");
            FREEUP (jgrplist);
            return(-1);
        }
        if ( !jgrplist[i].isJData && 
             ((len = packJgrpInfo ((struct jgTreeNode *)jgrplist[i].info, 
                                    listSize - 1 - i, &buf, schedule, reqHdr->version)) < 0)) {
            ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "packJgrpInfo");
            FREEUP (jgrplist);
            return(-1);
        }
	
	{
            if (chanWrite_(chfd, buf, len) != len) {
                ls_syslog(LOG_ERR, I18N_FUNC_FAIL_M, fname, "chanWrite_");
                    FREEUP(buf);
	            FREEUP (jgrplist);
            	    return(-1);
	    }
	}
        FREEUP(buf);
    }
    FREEUP (jgrplist);

    chanClose_(chfd);
    return(0);           
} 

static int
packJgrpInfo(struct jgTreeNode * jgNode, int remain, char **replyBuf, int schedule, int version)
{
    struct jobInfoReply jobInfoReply;
    struct submitReq jobBill;
    struct LSFHeader hdr;
    char  *request_buf = NULL;
    static char fname[] = "packJgrpInfo";
    XDR xdrs;
    int i, len;

    jobInfoReply.jobId        = 0;
    jobInfoReply.numReasons   = 0;
    jobInfoReply.reasons      = 0; 
    jobInfoReply.subreasons   = 0;

    jobInfoReply.startTime    = 0;
    jobInfoReply.predictedStartTime = 0;
    jobInfoReply.endTime      = 0;

    jobInfoReply.cpuTime      = 0;
    jobInfoReply.numToHosts   = 0;
    jobInfoReply.jobBill      = &jobBill;

    jobInfoReply.jType             = jgNode->nodeType;
    jobInfoReply.parentGroup       = jgrpNodeParentPath(jgNode);
    jobInfoReply.jName             = jgNode->name;
    jobInfoReply.jobBill->jobName  = jgNode->name;

    jobInfoReply.nIdx = 0;
    len = 4 * MAX_LSB_NAME_LEN + 3 * MAXLINELEN + 2 * MAXHOSTNAMELEN
	+ 7 * MAXFILENAMELEN + sizeof(struct submitReq)
	+ sizeof(struct jobInfoReply)
	+ MAX_LSB_NAME_LEN + 2 * MAXFILENAMELEN
        + sizeof(time_t)
        + strlen(jobInfoReply.jobBill->dependCond)
        + 100
	+ sizeof(int)
	+ strlen(jobInfoReply.parentGroup)
	+ strlen(jobInfoReply.jName)
	+ (NUM_JGRP_COUNTERS + 1) * sizeof(int);

    len = (len * 4) / 4;

    request_buf = (char *) my_malloc(len, "packJgrpInfo");
    xdrmem_create(&xdrs, request_buf, len, XDR_ENCODE);
    
    setHdrReserved(&hdr, remain);
    

    hdr.version = version;
    if (!xdr_encodeMsg(&xdrs, (char *) &jobInfoReply,
		       &hdr, xdr_jobInfoReply, 0, NULL)) {
        ls_syslog(LOG_ERR, I18N_FUNC_S_FAIL, fname, 
	    "xdr_encodeMsg", "jobInfoReply");
	xdr_destroy(&xdrs);
	FREEUP (request_buf);
	return -1;
    }
    i = XDR_GETPOS(&xdrs);
    *replyBuf = request_buf;
    xdr_destroy(&xdrs);
    return (i);

} 

static int 
jobInfoReplyXdrBufLen(struct jobInfoReply *jobInfoReplyPtr)
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

    

    len += jobInfoReply.runRusage.npids * (NET_INTSIZE_ + sizeof(struct pidInfo));
 
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

    return(len);
}	

static int
packJobInfo(struct jData * jobData, int remain, char **replyBuf, int schedule,
	    int options, int version)
{
    static char              fname[] = "packJobInfo";
    struct jobInfoReply      jobInfoReply;
    struct submitReq         jobBill;
    struct LSFHeader         hdr;
    char                     *request_buf = NULL;
    int                      *reasonTb = NULL;
    int                      *jReasonTb;
    XDR                      xdrs;
    int                      i, 
	                     k, 
	                     len;
    int                      svReason, 
	                     *pkHReasonTb, 
                             *pkQReasonTb,  
                             *pkUReasonTb;
    float                    *loadSched = NULL,
                             *loadStop = NULL;
    float                    *cpuFactor, 
                             one = 1.0;
    float		     cpuF;
    char		     fullName[MAXPATHLEN];
    struct jgTreeNode        *jgNode;
    int			     job_numReasons;
    int			    *job_reasonTb;

    job_numReasons = jobData->numReasons;
    job_reasonTb = jobData->reasonTb;

    if (reasonTb == NULL) {
       reasonTb = (int *) my_calloc (numofhosts + 1, sizeof(int), fname); 
       jReasonTb = (int *) my_calloc (numofhosts + 1, sizeof(int), fname); 
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

    if (IS_SUSP (jobData->jStatus))
	jobInfoReply.reasons = (~SUSP_MBD_LOCK & jobData->newReason);
    else
        jobInfoReply.reasons = jobData->newReason; 
    jobInfoReply.subreasons = jobData->subreasons;
    jobInfoReply.reasonTb = reasonTb;           

    if (logclass & LC_PEND)
        ls_syslog(LOG_DEBUG3, "%s: job=%s, rs=%d, nrs=%d srs=%d, qNrs=%d, jNrs=%d nLsb=%d nQU=%d mStage=%d", fname, lsb_jobid2str(jobData->jobId), jobData->oldReason, jobData->newReason, jobData->subreasons, jobData->qPtr->numReasons, job_numReasons, numLsbUsable, jobData->qPtr->numUsable, mSchedStage);

    
    if (IS_PEND (jobData->jStatus) && !(options & NO_PEND_REASONS)) {
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
            for (i = 0; i <= numofhosts; i ++) {
                if (i > 0 && jobData->numAskedPtr > 0 
                    && jobData->askedOthPrio < 0)     
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
                    if (k > numofhosts 
                        || jReasonTb[k] == PEND_HOST_USR_SPEC)
                        continue;
                    jReasonTb[k] = job_reasonTb[i];
                }
            }
            if (svReason == 0) {
		if (logclass & LC_PEND)
		    ls_syslog(LOG_DEBUG2, "%s: Get h/u/q reasons", fname);
                k = 0;
		for (i = 1; i <= numofhosts; i ++) {
                    
                    if (jReasonTb[i] == PEND_HOST_USR_SPEC) {
                        continue;       
                    }
		    if (pkQReasonTb[i] == PEND_HOST_QUE_MEMB)
                        continue;          
                    if (!isHostQMember(hDataPtrTb[i], jobData->qPtr)){
                        continue;           
                    }
		    if (pkHReasonTb[i]) {
			jobInfoReply.reasonTb[k] = pkHReasonTb[i];
                        PUT_HIGH(jobInfoReply.reasonTb[k], i);
                        k++;               
			if (debug && (logclass & LC_PEND))
			    ls_syslog(LOG_DEBUG2, "%s: hReasonTb[%d]=%d", 
					    fname, i, pkHReasonTb[i]); 
                        continue;
                    }
		    if (pkQReasonTb[i]) {
			jobInfoReply.reasonTb[k] = pkQReasonTb[i];
			PUT_HIGH(jobInfoReply.reasonTb[k], i);
                        k++;               
			if (debug && (logclass & LC_PEND))
			    ls_syslog(LOG_DEBUG2, "%s: qReason[%d]=%d", 
			          fname, i, pkQReasonTb[i]); 
                        continue;
		    }
		    if (pkUReasonTb[i]) {
			jobInfoReply.reasonTb[k] = pkUReasonTb[i];
			PUT_HIGH(jobInfoReply.reasonTb[k], i);
                        k++;               
			if (debug && (logclass & LC_PEND))
			    ls_syslog(LOG_DEBUG2, "%s: uReason[%d]=%d", 
			          fname, i, pkUReasonTb[i]); 
                        continue;
		    }
		    if (jReasonTb[i]) {
			jobInfoReply.reasonTb[k++] = jReasonTb[i];
			if (debug && (logclass & LC_PEND)) {
                            int rs;
			    GET_LOW(rs, jReasonTb[i]);
			    ls_syslog(LOG_DEBUG2, "%s: jReason[%d]=%d", 
						    fname, i, rs); 
                        }
                        continue;
		    }
		}
                
                if (jReasonTb[0] != 0) {
		    
		    jobInfoReply.reasonTb[k++] = jReasonTb[0];        
		    if (debug && (logclass & LC_PEND)) {
			ls_syslog(LOG_DEBUG2, "%s: jReason[0]=%d", 
						    fname, jReasonTb[0]); 
		    }
                }
            }
            jobInfoReply.numReasons = k;
        }
    } else
	jobInfoReply.numReasons = 0;


    if (jobData->numHostPtr > 0) {
	jobInfoReply.toHosts = (char **) my_calloc(jobData->numHostPtr,
						     sizeof(char *), fname);
	for (i = 0; i < jobData->numHostPtr; i++) {
	    jobInfoReply.toHosts[i] = safeSave(jobData->hPtr[i]->host);
	}
    }
    jobInfoReply.nIdx = allLsInfo->numIndx;
    if (!loadSched) {
        loadSched=(float *)malloc(allLsInfo->numIndx*sizeof(float *));
        loadStop=(float *)malloc(allLsInfo->numIndx*sizeof(float *));
        if ((!loadSched) || (!loadStop)) {
            ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "malloc");
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
    copyJobBill (&jobData->shared->jobBill, jobInfoReply.jobBill, FALSE);
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
    if ((jobData->numHostPtr > 0) && (jobData->hPtr[0] != NULL)
	  && !IS_PEND (jobData->jStatus)
          && strcmp(jobData->hPtr[0]->host, LOST_AND_FOUND)) {
	    cpuFactor = &jobData->hPtr[0]->cpuFactor;
	FREEUP (jobInfoReply.jobBill->hostSpec);
	jobInfoReply.jobBill->hostSpec = safeSave (jobData->hPtr[0]->host);
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
		    ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 7809,
		        "%s: Cannot find cpu factor for hostSpec  <%s>; cpuFactor is set to 1.0"), fname, jobInfoReply.jobBill->fromHost); /* catgets 7809 */
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
    jobInfoReply.jType    = jobData->nodeType;
    jobInfoReply.parentGroup = jgrpNodeParentPath(jgNode);
    jobInfoReply.jName        = jgNode->name;

    
    if ( jgNode->nodeType == JGRP_NODE_ARRAY ) {
        for (i=0; i<NUM_JGRP_COUNTERS; i++)
            jobInfoReply.counter[i] = ARRAY_DATA(jgNode)->counts[i];
    }

    

    jobInfoReply.jRusageUpdateTime = jobData->jRusageUpdateTime;

    memcpy((char *) &jobInfoReply.runRusage, 
           (char *) &jobData->runRusage, sizeof(struct jRusage));

    len = jobInfoReplyXdrBufLen(&jobInfoReply);
    len += 1024; 

    FREEUP (request_buf);
    request_buf = (char *) my_malloc(len, "packJobInfo");
    xdrmem_create(&xdrs, request_buf, len, XDR_ENCODE);
     
    setHdrReserved(&hdr, remain);
     
    
    hdr.version = version; 

    if (!xdr_encodeMsg(&xdrs, (char *) &jobInfoReply,
		       &hdr, xdr_jobInfoReply, 0, NULL)) {
        ls_syslog(LOG_ERR, I18N_FUNC_S_FAIL, fname, 
	    "xdr_encodeMsg", "jobInfoReply");
	xdr_destroy(&xdrs);
	freeJobInfoReply (&jobInfoReply);
        FREEUP(reasonTb);
        FREEUP(jReasonTb);
        FREEUP(loadSched);
        FREEUP(loadStop);
	FREEUP (request_buf);
	return -1;
    }
    FREEUP(reasonTb);
    FREEUP(jReasonTb);
    FREEUP(loadSched);
    FREEUP(loadStop);
    freeJobInfoReply (&jobInfoReply);
    i = XDR_GETPOS(&xdrs);
    *replyBuf = request_buf;
    xdr_destroy(&xdrs);
    return (i);

} 

int
do_jobPeekReq (XDR *xdrs, int chfd, struct sockaddr_in *from, char *hostName,
               struct LSFHeader *reqHdr, struct lsfAuth *auth)
{

    static char             fname[] = "do_jobPeekReq";
    char                    reply_buf[MSGSIZE];
    XDR                     xdrs2;
    struct jobPeekReq       jobPeekReq;
    struct jobPeekReply     jobPeekReply;
    int                     reply;
    int                     cc;
    struct LSFHeader        replyHdr;
    char                   *replyStruct;

    jobPeekReply.outFile = NULL;
    if (!xdr_jobPeekReq(xdrs, &jobPeekReq, reqHdr)) {
	reply = LSBE_XDR;
	ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "xdr_jobPeekReq");
    } else {
	reply = peekJob(&jobPeekReq, &jobPeekReply, auth);
    }

    xdrmem_create(&xdrs2, reply_buf, MSGSIZE, XDR_ENCODE);
    replyHdr.opCode = reply;
    if (reply == LSBE_NO_ERROR)
	replyStruct = (char *) &jobPeekReply;
    else
	replyStruct = (char *) 0;
    if (!xdr_encodeMsg(&xdrs2, replyStruct, &replyHdr, xdr_jobPeekReply, 0,
		       NULL)) {
	ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "xdr_encodeMsg");
	xdr_destroy(&xdrs2);
	FREEUP(jobPeekReply.outFile);
	return -1;
    }
    cc = XDR_GETPOS (&xdrs2);
    if ((chanWrite_(chfd, reply_buf, cc)) != cc) {
	ls_syslog(LOG_ERR, I18N_FUNC_FAIL_M, fname, "chanWrite_");
	xdr_destroy(&xdrs2);
	FREEUP(jobPeekReply.outFile);
	return -1;
    }
    xdr_destroy(&xdrs2);
    FREEUP(jobPeekReply.outFile);
    return 0;

}				

int
do_signalReq (XDR *xdrs, int chfd, struct sockaddr_in *from, char *hostName,
	struct LSFHeader *reqHdr, struct lsfAuth *auth)
{
    static char             fname[] = "do_signalReq";
    char                    reply_buf[MSGSIZE];
    XDR                     xdrs2;
    static struct signalReq signalReq;
    int                     reply;
    struct LSFHeader        replyHdr;
    struct jData *jpbw;

    if (!xdr_signalReq(xdrs, &signalReq, reqHdr)) {
	reply = LSBE_XDR;
	ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "xdr_signalReq");
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
    xdrmem_create(&xdrs2, reply_buf, MSGSIZE, XDR_ENCODE);
    replyHdr.opCode = reply;

    if (!xdr_encodeMsg(&xdrs2, (char *) NULL, &replyHdr, xdr_int, 0, NULL)) {
	ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "xdr_encodeMsg");
	xdr_destroy(&xdrs2);
	return -1;
    }
    if (chanWrite_(chfd, reply_buf, XDR_GETPOS(&xdrs2)) <= 0) {
	ls_syslog(LOG_ERR, I18N_FUNC_FAIL_M, fname, "b_write_fix");
	xdr_destroy(&xdrs2);
	return -1;
    }
    xdr_destroy(&xdrs2);
    return 0;

}				

int
do_jobMsg(struct bucket *bucket,
	  XDR *xdrs, 
	  int chfd, 
	  struct sockaddr_in * from, 
	  char *hostName,
	  struct LSFHeader * reqHdr, 
	  struct lsfAuth * auth)
{
    static char             fname[] = "do_jobMsg";
    char                    reply_buf[MSGSIZE];
    XDR                     xdrs2;
    int                     reply;
    struct LSFHeader        sndhdr;
    struct LSFHeader        replyHdr;
    struct jData           *jpbw;
    struct bucket          *msgq;
    struct Buffer          *buf = bucket->storage;
    LSBMSG_DECL(header, jmsg);

    LSBMSG_INIT(header, jmsg);

    if (logclass & (LC_TRACE | LC_SIGNAL))
        ls_syslog(LOG_DEBUG1, "%s: Entering this routine ...", fname);

    if (!xdr_lsbMsg(xdrs, &jmsg, reqHdr)) {
	reply = LSBE_XDR;
	ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "xdr_lsbMsg");
	goto Reply;
    }

    
    jmsg.header->msgId = msgcnt;
    msgcnt++;

    
    LSBMSG_CACHE_BUFFER(bucket, jmsg);
    
    sndhdr.opCode = BATCH_JOB_MSG;
    xdrmem_create(&bucket->xdrs, buf->data, buf->len, XDR_ENCODE);

    if (! xdr_encodeMsg(&bucket->xdrs, (char *)&jmsg,
			&sndhdr, xdr_lsbMsg, 0, NULL)) {

	reply = LSBE_XDR;
	ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "xdr_encodeMsg");
	goto Reply;
    }
    xdr_destroy(&bucket->xdrs);
    xdrmem_create(&bucket->xdrs, buf->data, buf->len, XDR_DECODE);
    
    
    if ((jpbw = getJobData(jmsg.header->jobId)) == NULL) {
	reply = LSBE_NO_JOB;	
	goto Reply;
    }

    if (IS_PEND (jpbw->jStatus)) {
        reply = LSBE_NOT_STARTED;
        goto Reply;
    }

    log_jobmsg(jpbw, &jmsg, jmsg.header->usrId);

    
    msgq = jpbw->hPtr[0]->msgq[MSG_STAT_QUEUED];
    eventPending = TRUE;

    if (logclass & (LC_SIGNAL))
        ls_syslog(LOG_DEBUG2, "%s: A message for job %s; eP=%d", 
		  fname, lsb_jobid2str(jpbw->jobId), eventPending);

    QUEUE_APPEND(bucket, msgq);
    bucket->storage->stashed = TRUE;
    reply = LSBE_NO_ERROR;

Reply:
    xdrmem_create(&xdrs2, reply_buf, MSGSIZE, XDR_ENCODE);
    replyHdr.opCode = reply;

    if (!xdr_encodeMsg(&xdrs2, (char *) NULL, &replyHdr, xdr_int, 0, NULL)) {
	ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "xdr_encodeMsg");
	xdr_destroy(&xdrs2);
	return -1;
    }
    if (chanWrite_(chfd, reply_buf, XDR_GETPOS(&xdrs2)) <= 0) {
	ls_syslog(LOG_ERR, I18N_FUNC_FAIL_M, fname, "b_write_fix");
	xdr_destroy(&xdrs2);
	return -1;
    }
    xdr_destroy(&xdrs2);
    return 0;

}				


int
do_migReq (XDR *xdrs, int chfd, struct sockaddr_in *from, char *hostName,
	   struct LSFHeader *reqHdr, struct lsfAuth *auth)
{
    static char             fname[] = "do_migReq";
    char                    reply_buf[MSGSIZE];
    XDR                     xdrs2;
    struct migReq           migReq;
    struct submitMbdReply   migReply;
    int                     reply;
    struct LSFHeader        replyHdr;
    char                   *replyStruct;
    int                     i;

    migReply.jobId = 0;
    migReply.badReqIndx = 0;
    migReply.queue = "";
    migReply.badJobName = "";

    if (!xdr_migReq(xdrs, &migReq, reqHdr)) {
	reply = LSBE_XDR;
	ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "xdr_migReq");
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
    replyHdr.opCode = reply;
    if (reply != LSBE_NO_ERROR)
	replyStruct = (char *) &migReply;
    else {
	replyStruct = (char *) 0;
    }
    if (!xdr_encodeMsg(&xdrs2, replyStruct, &replyHdr, xdr_submitMbdReply, 0,
		       NULL)) {
	ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "xdr_encodeMsg");
	xdr_destroy(&xdrs2);
	return -1;
    }
    if (chanWrite_(chfd, reply_buf, XDR_GETPOS(&xdrs2)) <= 0) {
	ls_syslog(LOG_ERR, I18N_FUNC_FAIL_M, fname, "b_write_fix");
	xdr_destroy(&xdrs2);
	return -1;
    }
    xdr_destroy(&xdrs2);
    return 0;

} 

int
do_statusReq(XDR * xdrs, int chfd, struct sockaddr_in * from, int *schedule,
	     struct LSFHeader * reqHdr)
{
    static char             fname[] = "do_statusReq()";
    char                    reply_buf[MSGSIZE];
    XDR                     xdrs2;
    struct statusReq        statusReq;
    int                     reply;
    struct hData           *hData;
    struct hostent         *hp;
    struct hostent          hpBuf;
    struct LSFHeader        replyHdr;

#ifdef INTER_DAEMON_AUTH
    if (authSbdRequest(NULL, xdrs, reqHdr, from) != LSBE_NO_ERROR) {
	ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd, NL_SETN, 7825,
					 "%s: Received status report from unauthenticated host <%s>"),
		  fname, sockAdd2Str_(from));
	if (reqHdr->opCode != BATCH_RUSAGE_JOB)
	    errorBack(chfd, LSBE_PERMISSION, from);
	return -1;
    }
#endif 

    if (!portok(from)) {
	ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 7824,
	    "%s: Received status report from bad port <%s>"), /* catgets 7824 */
	    fname, 
	    sockAdd2Str_(from)); 
	if (reqHdr->opCode != BATCH_RUSAGE_JOB)
	    errorBack(chfd, LSBE_PORT, from);
	return -1;		
    }
    hp = (struct hostent *)getHostEntryByAddr_(&(from->sin_addr));
    if (hp == NULL) {
	ls_syslog(LOG_ERR, I18N_FUNC_S_FAIL_MM, fname, "getHostEntryByAddr_", 
	    sockAdd2Str_(from));
	if (reqHdr->opCode != BATCH_RUSAGE_JOB)	
	    errorBack(chfd, LSBE_BAD_HOST, from);
	return (-1);
    }

    cpHostent(&hpBuf, hp);
    
    if (!xdr_statusReq(xdrs, &statusReq, reqHdr)) {
	reply = LSBE_XDR;
	ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "xdr_statusReq");
    } else {
	switch(reqHdr->opCode) {
	case BATCH_STATUS_MSG_ACK:
	    reply = statusMsgAck(&statusReq);
	    break;
	case BATCH_STATUS_JOB:
	    reply = statusJob(&statusReq, &hpBuf, schedule);
	    break;
	case BATCH_RUSAGE_JOB:
	    reply = rusageJob(&statusReq, &hpBuf);
	    break;
	default:
	    ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 7827,
		"%s: Unknown request %d"),  /* catgets 7827 */
		fname, 
		reqHdr->opCode);
	    reply = LSBE_PROTOCOL;
	}
    }

    xdr_lsffree(xdr_statusReq, (char *) &statusReq, reqHdr);

    if (reqHdr->opCode == BATCH_RUSAGE_JOB) {
        freeHp(&hpBuf);
	if (reply == LSBE_NO_ERROR)
	    return 0;
	return (-1);
    }

    xdrmem_create(&xdrs2, reply_buf, MSGSIZE, XDR_ENCODE);
    replyHdr.opCode = reply;
    replyHdr.length = 0;
    if (!xdr_LSFHeader(&xdrs2, &replyHdr)) {
	ls_syslog(LOG_ERR, I18N_FUNC_D_FAIL, fname, "xdr_LSFHeader",
	    reply);
	xdr_destroy(&xdrs2);
        freeHp(&hpBuf);
	return -1;
    }
    if (chanWrite_(chfd, reply_buf, XDR_GETPOS(&xdrs2)) <= 0) {
        ls_syslog(LOG_ERR, I18N_FUNC_FAIL_M, fname, "b_write_fix");
	xdr_destroy(&xdrs2);
        freeHp(&hpBuf);
	return -1;
    }
    xdr_destroy(&xdrs2);
   
    if ((hData = getHostData(hpBuf.h_name)) != NULL)
	hStatChange(hData, 0);	

    freeHp(&hpBuf);
    return 0;

}				

int
do_chunkStatusReq(XDR * xdrs, int chfd, struct sockaddr_in * from, 
		  int *schedule, struct LSFHeader * reqHdr)
{
    static char             fname[] = "do_chunkStatusReq()";
    char                    reply_buf[MSGSIZE];
    XDR                     xdrs2;
    struct chunkStatusReq   chunkStatusReq;
    int                     reply;
    struct hData           *hData;
    struct hostent         *hp;
    struct hostent          hpBuf;
    struct LSFHeader        replyHdr;
    int i = 0;

#ifdef INTER_DAEMON_AUTH
    if (authSbdRequest(NULL, xdrs, reqHdr, from) != LSBE_NO_ERROR) {
	ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd, NL_SETN, 7825,
					 "%s: Received status report from unauthenticated host <%s>"),
		  fname, sockAdd2Str_(from));
	errorBack(chfd, LSBE_PERMISSION, from);
	return -1;
    }
#endif 

    if (!portok(from)) {
	ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 7824,
	    "%s: Received status report from bad port <%s>"), /* catgets 7824 */
	    fname, 
	    sockAdd2Str_(from)); 
	errorBack(chfd, LSBE_PORT, from);
	return -1;		
    }
    hp = (struct hostent *)getHostEntryByAddr_(&(from->sin_addr));
    if (hp == NULL) {
	ls_syslog(LOG_ERR, I18N_FUNC_S_FAIL_MM, fname, "getHostEntryByAddr_", 
	    sockAdd2Str_(from));
	errorBack(chfd, LSBE_BAD_HOST, from);
	return (-1);
    }
 
    cpHostent(&hpBuf, hp);
    
    if (!xdr_chunkStatusReq(xdrs, &chunkStatusReq, reqHdr)) {
	reply = LSBE_XDR;
	ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "xdr_chunkStatusReq");
    } else {
	
	for (i=0; i<chunkStatusReq.numStatusReqs; i++) {
	    
	    statusJob(chunkStatusReq.statusReqs[i], &hpBuf, schedule);
	}
	
	reply = LSBE_NO_ERROR;
    }

    xdr_lsffree(xdr_chunkStatusReq, (char *) &chunkStatusReq, reqHdr);

    xdrmem_create(&xdrs2, reply_buf, MSGSIZE, XDR_ENCODE);
    replyHdr.opCode = reply;
    replyHdr.length = 0;
    if (!xdr_LSFHeader(&xdrs2, &replyHdr)) {
	ls_syslog(LOG_ERR, I18N_FUNC_D_FAIL, fname, "xdr_LSFHeader",
	    reply);
	xdr_destroy(&xdrs2);
        freeHp(&hpBuf);
	return -1;
    }
    if (chanWrite_(chfd, reply_buf, XDR_GETPOS(&xdrs2)) <= 0) {
        ls_syslog(LOG_ERR, I18N_FUNC_FAIL_M, fname, "b_write_fix");
	xdr_destroy(&xdrs2);
        freeHp(&hpBuf);
	return -1;
    }
    xdr_destroy(&xdrs2);

    if ((hData = getHostData(hpBuf.h_name)) != NULL)
	hStatChange(hData, 0);	

    freeHp(&hpBuf);
    return 0;

}				


int
do_restartReq(XDR * xdrs, int chfd, struct sockaddr_in * from,
	      struct LSFHeader * reqHdr)
{
    static char             fname[] = "do_restartReq()";
    char                   *reply_buf;
    XDR                     xdrs2;
    int                     buflen;
    struct LSFHeader        replyHdr;
    int                     reply;
    struct sbdPackage       sbdPackage;
    int                     cc;
    const char             *officialName;
    char                    officialNameBuf[MAXHOSTNAMELEN];
    struct hData           *hData;
    int			   i;

    if (!portok(from)) {
	ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 7824,
	    "%s: Received status report from bad port <%s>"),
	    fname,
	    sockAdd2Str_(from));
	errorBack(chfd, LSBE_PORT, from);
	return -1;		
    }

    officialName = getHostOfficialByAddr_(&(from->sin_addr));
    if (officialName == NULL) {
	ls_syslog(LOG_ERR, I18N_FUNC_S_FAIL_MM, fname, "getHostOfficialByAddr_",
	    sockAdd2Str_(from));
	errorBack(chfd, LSBE_BAD_HOST, from);
	return (-1);
    }
    strcpy( officialNameBuf, officialName);

    if ((hData = getHostData((char*)officialNameBuf)) == NULL) {
	ls_syslog(LOG_ERR, I18N_FUNC_S_FAIL, fname, "getHostData",
	    officialNameBuf);
	errorBack(chfd, LSBE_BAD_HOST, from);
	return (-1); 
    }
    hStatChange(hData, 0);	

    if ((sbdPackage.numJobs = countNumSpecs(hData)) > 0)
	sbdPackage.jobs = (struct jobSpecs *) my_calloc(sbdPackage.numJobs,
				  sizeof(struct jobSpecs), "do_restartReq");
    else
	sbdPackage.jobs = NULL;
    buflen = sbatchdJobs(&sbdPackage, hData);
    reply = LSBE_NO_ERROR;

    reply_buf = (char *) my_malloc(buflen, "do_restartReq");
    xdrmem_create(&xdrs2, reply_buf, buflen, XDR_ENCODE);
    replyHdr.opCode = reply;
    if (!xdr_encodeMsg(&xdrs2, (char *) &sbdPackage, &replyHdr,
		       xdr_sbdPackage, 0, NULL)) {
	ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "xdr_encodeMsg");
	xdr_destroy(&xdrs2);
	free(reply_buf);
	for (i = 0; i < sbdPackage.nAdmins; i++)
	    FREEUP(sbdPackage.admins[i]);
	FREEUP(sbdPackage.admins);
	for (cc = 0; cc < sbdPackage.numJobs; cc++)
	    freeJobSpecs(&sbdPackage.jobs[cc]);
	if (sbdPackage.jobs)
	    free(sbdPackage.jobs);
	return -1;
    }
    cc = XDR_GETPOS(&xdrs2);
    if (chanWrite_(chfd, reply_buf, cc) <= 0)
        ls_syslog(LOG_ERR, I18N_FUNC_D_FAIL_M, fname, "chanWrite_",
	    cc);

    free(reply_buf);
    xdr_destroy(&xdrs2);
    for (i = 0; i < sbdPackage.nAdmins; i++)
	FREEUP(sbdPackage.admins[i]);
    FREEUP(sbdPackage.admins);
    for (cc = 0; cc < sbdPackage.numJobs; cc++)
	freeJobSpecs(&sbdPackage.jobs[cc]);
    if (sbdPackage.jobs)
	free(sbdPackage.jobs);
    return 0;

}				

int
do_hostInfoReq(XDR * xdrs, int chfd, struct sockaddr_in * from, struct LSFHeader * reqHdr)
{
    static char             fname[] = "do_hostInfoReq()";
    char                   *reply_buf;
    XDR                     xdrs2;
    struct LSFHeader        replyHdr;
    char                   *replyStruct;
    int                     count;
    int                     reply;
    struct infoReq          hostsReq;
    struct hostDataReply    hostsReply;


    
    hostsReply.numHosts = 0;
    hostsReply.hosts = NULL;
    if (!xdr_infoReq(xdrs, &hostsReq, reqHdr)) {
	reply = LSBE_XDR;
	ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "xdr_infoReq");
    } else {
	reply = checkHosts(&hostsReq, &hostsReply);
    }

    if (logclass & (LC_EXEC)) {
	int i;
	for (i=0; i < hostsReply.numHosts; i++)
	   ls_syslog(LOG_DEBUG, "%s, host[%d]'s name is %s", fname, i, 
		     hostsReply.hosts[i].host);
    }
    count = hostsReply.numHosts * (sizeof(struct hostInfoEnt)
			   + MAXLINELEN + MAXHOSTNAMELEN
                           + hostsReply.nIdx*4*sizeof(float)) + 100;
    reply_buf = (char *) my_malloc(count, "do_hostInfoReq");
    xdrmem_create(&xdrs2, reply_buf, count, XDR_ENCODE);
    replyHdr.opCode = reply;
    if (reply == LSBE_NO_ERROR || reply == LSBE_BAD_HOST)
	replyStruct = (char *) &hostsReply;
    else
	replyStruct = (char *) 0;
    if (!xdr_encodeMsg (&xdrs2, replyStruct, &replyHdr, xdr_hostDataReply, 0,
			NULL)) {
        ls_syslog(LOG_ERR,  I18N_FUNC_FAIL, fname, "xdr_encodeMsg");
	FREEUP ( hostsReply.hosts);
	FREEUP (reply_buf);
        xdr_destroy(&xdrs2);
        return(-1);
    }
    if (chanWrite_(chfd, reply_buf, XDR_GETPOS(&xdrs2)) <= 0) {
        ls_syslog(LOG_ERR, I18N_FUNC_D_FAIL_M, fname, "b_write_fix",
	    XDR_GETPOS(&xdrs2));
        xdr_destroy(&xdrs2);
	FREEUP ( hostsReply.hosts);
	FREEUP (reply_buf);
        return(-1);
    }
    FREEUP ( hostsReply.hosts);
    xdr_destroy(&xdrs2);
    FREEUP (reply_buf);
    return(0);
}				

int
do_userInfoReq(XDR * xdrs, int chfd, struct sockaddr_in * from, struct LSFHeader * reqHdr)
{
    static char             fname[] = "do_userInfoReq()";
    char                   *reply_buf;
    XDR                     xdrs2;
    int                     reply;
    struct LSFHeader        replyHdr;
    char                   *replyStruct;
    int                     count;
    struct infoReq          userInfoReq;
    struct userInfoReply    userInfoReply;

    userInfoReply.users = NULL;
    if (!xdr_infoReq(xdrs, &userInfoReq, reqHdr)) {
	reply = LSBE_XDR;
	ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "xdr_infoReq");
    } else {
	count = userInfoReq.numNames == 0 ? uDataList.numEnts	
	    : userInfoReq.numNames;
	userInfoReply.users = (struct userInfoEnt *) my_calloc(count,
			      sizeof(struct userInfoEnt), "do_userInfoReq");
	reply = checkUsers(&userInfoReq, &userInfoReply);
    }
    count = userInfoReply.numUsers * (sizeof(struct userInfoEnt)
				      + MAX_LSB_NAME_LEN) + 100;
    reply_buf = (char *) my_malloc(count, "do_userInfoReq");
    xdrmem_create(&xdrs2, reply_buf, count, XDR_ENCODE);
    replyHdr.opCode = reply;
    if (reply == LSBE_NO_ERROR || reply == LSBE_BAD_USER)
	replyStruct = (char *) &userInfoReply;
    else
	replyStruct = (char *) 0;
    if (!xdr_encodeMsg(&xdrs2, replyStruct, &replyHdr, xdr_userInfoReply, 0,
		       NULL)) {
	ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "xdr_encodeMsg");
	xdr_destroy(&xdrs2);
	FREEUP(reply_buf);
	FREEUP (userInfoReply.users );
	if (userInfoReply.users)
	    free(userInfoReply.users);
	return -1;
    }
    if (chanWrite_(chfd, reply_buf, XDR_GETPOS(&xdrs2)) <= 0) {
        ls_syslog(LOG_ERR, I18N_FUNC_D_FAIL_M, fname, "b_write_fix",
	    XDR_GETPOS(&xdrs2));
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


int
xdrsize_QueueInfoReply(struct queueInfoReply * qInfoReply)
{
   int len;
   int i;

   len = 0;

   for (i = 0; i < qInfoReply->numQueues; i++) {
        len += getXdrStrlen(qInfoReply->queues[i].description)
            + getXdrStrlen(qInfoReply->queues[i].windows)
            + getXdrStrlen(qInfoReply->queues[i].userList)
            + getXdrStrlen(qInfoReply->queues[i].hostList)
            + getXdrStrlen(qInfoReply->queues[i].defaultHostSpec)
            + getXdrStrlen(qInfoReply->queues[i].hostSpec)
            + getXdrStrlen(qInfoReply->queues[i].windowsD)
            + getXdrStrlen(qInfoReply->queues[i].admins)
            + getXdrStrlen(qInfoReply->queues[i].preCmd)
            + getXdrStrlen(qInfoReply->queues[i].postCmd)
            + getXdrStrlen(qInfoReply->queues[i].requeueEValues)
            + getXdrStrlen(qInfoReply->queues[i].resReq)
            + getXdrStrlen(qInfoReply->queues[i].resumeCond)
            + getXdrStrlen(qInfoReply->queues[i].stopCond)
            + getXdrStrlen(qInfoReply->queues[i].jobStarter)
            + getXdrStrlen(qInfoReply->queues[i].suspendActCmd)
            + getXdrStrlen(qInfoReply->queues[i].resumeActCmd)
            + getXdrStrlen(qInfoReply->queues[i].terminateActCmd)
            + getXdrStrlen(qInfoReply->queues[i].chkpntDir);
    }
    len += ALIGNWORD_(sizeof(struct queueInfoReply)
                      + qInfoReply->numQueues * (sizeof(struct queueInfoEnt)+ MAX_LSB_NAME_LEN + qInfoReply->nIdx*2*sizeof(float))
                      + qInfoReply->numQueues * NET_INTSIZE_);

    return len;
}

int
do_queueInfoReq (XDR* xdrs, 
		 int chfd, 
		 struct sockaddr_in* from,
                 struct LSFHeader* reqHdr)
{
    static char             fname[] = "do_queueInfoReq()";
    XDR                     xdrs2;
    struct infoReq          qInfoReq;
    struct queueInfoReply   qInfoReply;
    int                     reply;
    char                   *reply_buf;
    int                     len = 0;
    struct LSFHeader        replyHdr;
    char                   *replyStruct;


    qInfoReply.numQueues = 0;
    qInfoReply.queues = (struct queueInfoEnt *) my_calloc (numofqueues,
			     sizeof(struct queueInfoEnt), "do_queueInfoReq");

    if (!xdr_infoReq (xdrs, &qInfoReq, reqHdr)) {
        reply = LSBE_XDR;
        ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "xdr_infoReq");
        len =  100;
    } else {
	reply = checkQueues(&qInfoReq, &qInfoReply);

	
        
        len = sizeof(struct LSFHeader);
        len += xdrsize_QueueInfoReply(&qInfoReply);
    }

    reply_buf = (char *) my_malloc(len, "do_queueInfoReq");
    xdrmem_create(&xdrs2, reply_buf, len, XDR_ENCODE);
    replyHdr.opCode = reply;
    if (reply == LSBE_NO_ERROR || reply == LSBE_BAD_QUEUE)
	replyStruct = (char *) &qInfoReply;
    else
	replyStruct = (char *) 0;
    if (!xdr_encodeMsg (&xdrs2, 
			replyStruct, 
			&replyHdr, 
			xdr_queueInfoReply, 
			0,
			NULL)) {
        ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "xdr_encodeMsg");
	freeQueueInfoReply(&qInfoReply, replyStruct);
	FREEUP (reply_buf);
	xdr_destroy(&xdrs2);
	return(-1);
    }
    if (chanWrite_(chfd, reply_buf, XDR_GETPOS(&xdrs2)) <= 0) {
        ls_syslog(LOG_ERR, I18N_FUNC_D_FAIL_M, fname, "b_write_fix",
	    XDR_GETPOS(&xdrs2));
	freeQueueInfoReply(&qInfoReply, replyStruct);
	FREEUP (reply_buf);
	xdr_destroy(&xdrs2);
	return(-1);
    }
    freeQueueInfoReply(&qInfoReply, replyStruct);
    FREEUP (reply_buf);
    xdr_destroy(&xdrs2);
    return(0);

} 

int
do_groupInfoReq (XDR *xdrs, int chfd, struct sockaddr_in *from,
                 struct LSFHeader *reqHdr)
{
    static char             fname[] = "do_groupInfoReq()";
    struct infoReq          groupInfoReq;
    char *                  reply_buf;
    XDR                     xdrs2;
    int                     reply;
    int                     len;
    struct gData **         gplist;
    struct LSFHeader        replyHdr;
    char *                  replyStruct;
    struct groupInfoReply   groupInfoReply;

    
    memset((struct groupInfoReply *)&groupInfoReply, 0, 
	   sizeof(struct groupInfoReply));
	   
    if (!xdr_infoReq(xdrs, &groupInfoReq, reqHdr)) {
	reply = LSBE_XDR;
	ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "xdr_infoReq");
	gplist  = NULL;
    } 
    
    if (groupInfoReq.options & HOST_GRP) {
	gplist = hostgroups;
	
	if (numofhgroups == 0) {
	    reply = LSBE_NO_HOST_GROUP;
	} else {
	    
	    groupInfoReply.groups = (struct groupInfoEnt *)
		my_calloc(numofhgroups,
			  sizeof(struct groupInfoEnt), 
			  "do_groupInfoReq");
	    
	    reply = checkGroups(&groupInfoReq, &groupInfoReply);
	    
	}
    } else {	
	if (numofugroups == 0) {
	    reply = LSBE_NO_USER_GROUP;
	} else {
	    gplist = usergroups;

	    reply = checkGroups(&groupInfoReq, &groupInfoReply);
	}
    }

    
    len =  sizeofGroupInfoReply(&groupInfoReply);

    
    len += ALIGNWORD_(sizeof(struct LSFHeader));

    reply_buf = (char *)my_malloc(len, "do_groupInfoReq");

    xdrmem_create(&xdrs2, reply_buf, len, XDR_ENCODE);

    replyHdr.opCode = reply;

    if (reply == LSBE_NO_ERROR || reply == LSBE_BAD_GROUP)
	replyStruct = (char *) &groupInfoReply;
    else
	replyStruct = (char *) NULL;

    if (!xdr_encodeMsg(&xdrs2, 
		       replyStruct, 
		       &replyHdr, 
		       xdr_groupInfoReply, 
		       0,
		       NULL)) {
        ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "xdr_encodeMsg");
	FREEUP (reply_buf);
	xdr_destroy (&xdrs2);
        freeGroupInfoReply(&groupInfoReply);
	return(-1);
    }

    if ((chanWrite_(chfd, 
		    reply_buf, 
		    XDR_GETPOS(&xdrs2))) <= 0) {
        ls_syslog(LOG_ERR, I18N_FUNC_D_FAIL_M, fname, "chanWrite_",
	    XDR_GETPOS(&xdrs2));
	FREEUP (reply_buf);
	xdr_destroy (&xdrs2);
        freeGroupInfoReply(&groupInfoReply);
	return(-1);
    }

    FREEUP (reply_buf);
    xdr_destroy (&xdrs2);
    freeGroupInfoReply(&groupInfoReply);

    return(0);

} 

int
do_paramInfoReq(XDR * xdrs, int chfd, struct sockaddr_in * from,
		struct LSFHeader * reqHdr)
{
    static char             fname[] = "do_paramInfoReq()";
    char                   *reply_buf;
    XDR                     xdrs2;
    int                     reply;
    struct LSFHeader        replyHdr;
    char                   *replyStruct;
    int                     count, jobSpoolDirLen;
    struct infoReq          infoReq;
    struct parameterInfo    paramInfo;

    if (!xdr_infoReq(xdrs, &infoReq, reqHdr)) {
	reply = LSBE_XDR;
	ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "xdr_infoReq");
    } else
	checkParams(&infoReq, &paramInfo);
    reply = LSBE_NO_ERROR;

    if (paramInfo.pjobSpoolDir != NULL) {
	jobSpoolDirLen = strlen(paramInfo.pjobSpoolDir);
    } else {
	jobSpoolDirLen = 4;
    }

    

    count = sizeof(struct parameterInfo) + strlen(paramInfo.defaultQueues)
	+ strlen(paramInfo.defaultHostSpec)
	+ strlen(paramInfo.defaultProject) + 100 + jobSpoolDirLen;

    reply_buf = (char *) my_malloc(count, "do_paramInfoReq");
    xdrmem_create(&xdrs2, reply_buf, count, XDR_ENCODE);
    
    initLSFHeader_(&replyHdr);
    replyHdr.opCode = reply;
    replyStruct = (char *) &paramInfo;
    if (!xdr_encodeMsg(&xdrs2, replyStruct, &replyHdr, xdr_parameterInfo,
		       0, NULL)) {
	ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "xdr_encodeMsg");
	xdr_destroy(&xdrs2);
	FREEUP(reply_buf);
	return -1;
    }

    if (chanWrite_(chfd, reply_buf, XDR_GETPOS(&xdrs2)) <= 0) {
        ls_syslog(LOG_ERR, I18N_FUNC_D_FAIL_M, fname, "chanWrite_",
	    XDR_GETPOS(&xdrs2));
        xdr_destroy(&xdrs2);
        FREEUP (reply_buf);
        return -1;
    }
    xdr_destroy(&xdrs2);
    FREEUP(reply_buf);
    return 0;
}				

int
do_queueControlReq (XDR *xdrs, int chfd, struct sockaddr_in *from, 
     char *hostName, struct LSFHeader *reqHdr, struct lsfAuth *auth)
{
    static char             fname[] = "do_queueControlReq()";
    struct controlReq       bqcReq;
    char                    reply_buf[MSGSIZE];
    XDR                     xdrs2;
    int                     reply;
    struct LSFHeader        replyHdr;

    if (!xdr_controlReq(xdrs, &bqcReq, reqHdr)) {
	reply = LSBE_XDR;
	ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "xdr_controlReq");
    } else {
	reply = ctrlQueue(&bqcReq, auth);
    }

    xdrmem_create(&xdrs2, reply_buf, MSGSIZE, XDR_ENCODE);
    replyHdr.opCode = reply;
    replyHdr.length = 0;
    if (!xdr_LSFHeader(&xdrs2, &replyHdr)) {
        ls_syslog(LOG_ERR, I18N_FUNC_D_FAIL_M, fname, "xdr_LSFHeader",
	    reply);
	xdr_destroy(&xdrs2);
	return -1;
    }
    if (chanWrite_(chfd, reply_buf, XDR_GETPOS(&xdrs2)) <= 0) {
        ls_syslog(LOG_ERR, I18N_FUNC_D_FAIL_M, fname, "chanWrite_",
	    XDR_GETPOS(&xdrs2));
	xdr_destroy(&xdrs2);
	return -1;
    }
    xdr_destroy(&xdrs2);
    return 0;

}				

int
do_mbdShutDown (XDR * xdrs, int s, struct sockaddr_in * from, char *hostName,
	       struct LSFHeader * reqHdr)
{
    static char             fname[] = "do_mbdShutDown()";
    char                    reply_buf[MSGSIZE];
    XDR                     xdrs2;
    struct LSFHeader        replyHdr;

    xdrmem_create(&xdrs2, reply_buf, MSGSIZE, XDR_ENCODE);
    replyHdr.opCode = LSBE_NO_ERROR;
    replyHdr.length = 0;
    if (!xdr_LSFHeader(&xdrs2, &replyHdr)) {
        ls_syslog(LOG_ERR, I18N_FUNC_D_FAIL_M, fname, "xdr_LSFHeader",
	    replyHdr.opCode);
    }
    if (b_write_fix(s, reply_buf, XDR_GETPOS(&xdrs2)) <= 0)
        ls_syslog(LOG_ERR, I18N_FUNC_D_FAIL_M, fname, "b_write_fix",
	    XDR_GETPOS(&xdrs2));
    xdr_destroy(&xdrs2);
    millisleep_(3000);		
    mbdDie(MASTER_RECONFIG);
    return 0;

}				

int
do_reconfigReq (XDR *xdrs, int chfd, struct sockaddr_in *from, char *hostName,
                struct LSFHeader *reqHdr)
{
    static char             fname[] = "do_reconfigReq()";
    char                    reply_buf[MSGSIZE];
    XDR                     xdrs2;
    struct LSFHeader        replyHdr;

    xdrmem_create(&xdrs2, reply_buf, MSGSIZE, XDR_ENCODE);
    replyHdr.opCode = LSBE_NO_ERROR;
    replyHdr.length = 0;
    if (!xdr_LSFHeader(&xdrs2, &replyHdr)) {
	ls_syslog(LOG_ERR, I18N_FUNC_D_FAIL_M, fname, "xdr_LSFHeader",
	    replyHdr.opCode);
    }
    if (chanWrite_(chfd, reply_buf, XDR_GETPOS(&xdrs2)) <= 0)
	ls_syslog(LOG_ERR, I18N_FUNC_D_FAIL_M, fname, "chanWrite_",
	    XDR_GETPOS(&xdrs2));
    xdr_destroy(&xdrs2);

    if ( getHdrReserved(reqHdr) == MBD_RESTART ) {
	ls_syslog(LOG_DEBUG, I18N(7914, 
		"do_reconfigReq: restart a new mbatchd")); /* catgets 7914 */
        millisleep_(3000);		
        mbdDie(MASTER_RECONFIG);
    } else { 
	ls_syslog(LOG_DEBUG, I18N(7915, 
		"do_reconfigReq: reread the configuration files")); /* catgets 7915 */
        if (mSchedStage != M_STAGE_REPLAY)
            mbdReConf(RECONFIG_CONF);
    }
    return 0;

}				

int
do_hostControlReq (XDR *xdrs, int chfd, struct sockaddr_in *from, char *hostName,
                   struct LSFHeader *reqHdr, struct lsfAuth *auth)
{
    static char             fname[] = "do_hostControlReq()";
    struct controlReq       hostControlReq;
    char                    reply_buf[MSGSIZE];
    XDR                     xdrs2;
    int                     reply;
    struct hData           *hData;
    struct LSFHeader        replyHdr;

    if (!xdr_controlReq(xdrs, &hostControlReq, reqHdr)) {
	reply = LSBE_XDR;
	ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "xdr_controlReq");
	goto checkout;
    }
    hData = getHostData(hostControlReq.name);
    if (hData == NULL)
	reply = LSBE_BAD_HOST;
    else
	reply = ctrlHost(&hostControlReq, hData, auth);

checkout:
    xdrmem_create(&xdrs2, reply_buf, MSGSIZE, XDR_ENCODE);
    replyHdr.opCode = reply;
    replyHdr.length = 0;
    if (!xdr_LSFHeader(&xdrs2, &replyHdr)) {
	ls_syslog(LOG_ERR, I18N_FUNC_D_FAIL_M, fname, "xdr_LSFHeader",
	    reply);
    }
    if (chanWrite_(chfd, reply_buf, XDR_GETPOS(&xdrs2)) <= 0)
	ls_syslog(LOG_ERR, I18N_FUNC_D_FAIL_M, fname, "chanWrite_",
	    XDR_GETPOS(&xdrs2));
    xdr_destroy(&xdrs2);
    return 0;

}				

int
do_jobSwitchReq (XDR *xdrs, int chfd, struct sockaddr_in *from, char *hostName,
                 struct LSFHeader *reqHdr, struct lsfAuth *auth)
{
    static char             fname[] = "do_jobSwitchReq()";
    char                    reply_buf[MSGSIZE];
    XDR                     xdrs2;
    struct jobSwitchReq     jobSwitchReq;
    int                     reply;
    struct LSFHeader        replyHdr;

    if (!xdr_jobSwitchReq(xdrs, &jobSwitchReq, reqHdr)) {
	reply = LSBE_XDR;
	ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "xdr_jobSwitchReq");
    } else {
	reply = switchJobArray(&jobSwitchReq, auth);
    }
    xdrmem_create(&xdrs2, reply_buf, MSGSIZE, XDR_ENCODE);
    replyHdr.opCode = reply;
    replyHdr.length = 0;
    if (!xdr_LSFHeader(&xdrs2, &replyHdr)) {
	ls_syslog(LOG_ERR, I18N_FUNC_D_FAIL_M, fname, "xdr_LSFHeader",
	    reply);
	xdr_destroy(&xdrs2);
	return -1;
    }
    if (chanWrite_(chfd, reply_buf, XDR_GETPOS(&xdrs2)) <= 0) {
	ls_syslog(LOG_ERR, I18N_FUNC_D_FAIL_M, fname, "chanWrite_",
	    XDR_GETPOS(&xdrs2));
	xdr_destroy(&xdrs2);
	return -1;
    }
    xdr_destroy(&xdrs2);
    return 0;
}				

int
do_jobMoveReq (XDR *xdrs, int chfd, struct sockaddr_in *from, char *hostName,
               struct LSFHeader *reqHdr, struct lsfAuth *auth)
{
    static char             fname[] = "do_jobMoveReq()";
    char                    reply_buf[MSGSIZE];
    XDR                     xdrs2;
    struct jobMoveReq       jobMoveReq;
    int                     reply;
    struct LSFHeader        replyHdr;
    char                   *replyStruct;

    if (!xdr_jobMoveReq(xdrs, &jobMoveReq, reqHdr)) {
	reply = LSBE_XDR;
	ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "xdr_jobMoveReq");
    } else {
	reply = moveJobArray(&jobMoveReq, TRUE, auth);
    }
    xdrmem_create(&xdrs2, reply_buf, MSGSIZE, XDR_ENCODE);
    replyHdr.opCode = reply;
    if (reply == LSBE_NO_ERROR)
	replyStruct = (char *) &jobMoveReq;
    else {
	replyStruct = (char *) 0;
    }
    if (!xdr_encodeMsg(&xdrs2, replyStruct, &replyHdr, xdr_jobMoveReq, 0,
		       NULL)) {
	ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "xdr_encodeMsg");
	xdr_destroy(&xdrs2);
	return -1;
    }
    if (chanWrite_(chfd, reply_buf, XDR_GETPOS(&xdrs2)) <= 0) {
	ls_syslog(LOG_ERR, I18N_FUNC_D_FAIL_M, fname, "chanWrite_",
	    XDR_GETPOS(&xdrs2));
	xdr_destroy(&xdrs2);
	return -1;
    }
    xdr_destroy(&xdrs2);
    return 0;

}				

int
do_modifyReq(XDR * xdrs, int s, struct sockaddr_in * from, char *hostName,
	     struct LSFHeader * reqHdr, struct lsfAuth * auth)
{
    static char             fname[] = "do_modifyReq()";
    static struct submitMbdReply submitReply;
    static int              first = TRUE;
    static struct modifyReq modifyReq;
    int                     reply;

    
    initSubmit(&first, &(modifyReq.submitReq), &submitReply);

    
    if (!xdr_modifyReq(xdrs, &modifyReq, reqHdr)) {
	reply = LSBE_XDR;
	ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "xdr_modifyReq");
	goto sendback;
    }

    
    if (!(modifyReq.submitReq.options & SUB_RLIMIT_UNIT_IS_KB)) {
	convertRLimit(modifyReq.submitReq.rLimits, 1);
    }

    reply = modifyJob(&modifyReq, &submitReply, auth);
sendback:
    if (sendBack(reply, &modifyReq.submitReq, &submitReply, s) < 0)
	return (-1);
    return (0);

} 

static void
initSubmit(int *first, struct submitReq *subReq,
	   struct submitMbdReply *submitReply)
{
    static char             fname[] = "initSubmit";


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

    strcpy (submitReply->badJobName, "");	

}				

static int
sendBack (int reply, struct submitReq *submitReq,
                            struct submitMbdReply *submitReply, int chfd)
{
    static char             fname[] = "sendBack()";
    char                    reply_buf[MSGSIZE / 2];
    XDR                     xdrs2;
    struct LSFHeader        replyHdr;

    if (submitReq->nxf > 0)
	FREEUP(submitReq->xf);


    xdrmem_create(&xdrs2, reply_buf, MSGSIZE / 2, XDR_ENCODE);
    replyHdr.opCode = reply;
    if (!xdr_encodeMsg(&xdrs2, (char *) submitReply, &replyHdr,
		       xdr_submitMbdReply, 0, NULL)) {
	ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "xdr_encodeMsg");
	xdr_destroy(&xdrs2);
	return (-1);
    }
    if (chanWrite_(chfd, reply_buf, XDR_GETPOS(&xdrs2)) != XDR_GETPOS(&xdrs2))
	ls_syslog(LOG_ERR, I18N_FUNC_D_FAIL_M, fname, "chanWrite_",
	    XDR_GETPOS(&xdrs2));
    xdr_destroy(&xdrs2);
    return (0);

}				


void
doNewJobReply(struct sbdNode *sbdPtr, int exception)
{
    static char fname[] = "doNewJobReply";
    struct LSFHeader replyHdr;
    XDR xdrs;
    struct jData *jData = sbdPtr->jData;

    struct jobReply jobReply;
    struct Buffer *buf;
    struct LSFHeader hdr;
    int cc, s, svReason, replayReason;
    
    if (logclass & LC_COMM)
	ls_syslog(LOG_DEBUG, "%s: Entering ...", fname);

    
    if (jData->jobPid != 0)
	return;

    if (exception == TRUE || chanRecv_(sbdPtr->chanfd, &buf) < 0) {
        if (exception == TRUE)
            ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 7887,
		"%s: Exception bit of <%d> is set for job <%s>"), /* catgets 7887 */
                fname, sbdPtr->chanfd, lsb_jobid2str(jData->jobId)); 
        else
	    ls_syslog(LOG_ERR, I18N_JOB_FAIL_S, 
		fname,
		lsb_jobid2str(jData->jobId), 
		"chanRecv_");

	if (IS_START(jData->jStatus)) {
	    jData->newReason = PEND_JOB_START_FAIL;
    	    jStatusChange(jData, JOB_STAT_PEND, LOG_IT, fname);	
        }
	return;
    }

    xdrmem_create(&xdrs, buf->data, buf->len, XDR_DECODE);
    
    if (!xdr_LSFHeader(&xdrs, &replyHdr)) {
	ls_syslog(LOG_ERR, I18N_JOB_FAIL_S, 
	    fname,
	    lsb_jobid2str(jData->jobId), 
	    "xdr_LSFHeader");

	if (IS_START(jData->jStatus)) {
	    jData->newReason = PEND_JOB_START_FAIL;
    	    jStatusChange(jData, JOB_STAT_PEND, LOG_IT, fname);
        }
	goto Leave;
    }

#ifdef INTER_DAEMON_AUTH
    if (authSbdRequest(sbdPtr, &xdrs, &replyHdr, NULL) != LSBE_NO_ERROR) {
	ls_syslog(LOG_ERR, I18N_JOB_FAIL_S, fname,
		  lsb_jobid2str(jData->jobId), "authSbdRequest");
	if (IS_START(jData->jStatus)) {
	    jData->newReason = PEND_JOB_START_FAIL;
	    jStatusChange(jData, JOB_STAT_PEND, LOG_IT, fname);
	}
	goto Leave;
    }
#endif 
    
    if (replyHdr.opCode != ERR_NO_ERROR) {
	if (IS_START(jData->jStatus)) {
            
	    replayReason = jobStartError(jData, (sbdReplyType) replyHdr.opCode);
	    svReason = jData->newReason;
	    jData->newReason = replayReason;
	    jStatusChange(jData, JOB_STAT_PEND, LOG_IT, fname);
	    jData->newReason = svReason;
        }
	goto Leave;
    }	

    if(!xdr_jobReply(&xdrs, &jobReply, &replyHdr)) {
	ls_syslog(LOG_ERR, I18N_JOB_FAIL_S,
	    fname, lsb_jobid2str(jData->jobId), "xdr_jobReply");
        if (IS_START(jData->jStatus)) {
	    jData->newReason =  PEND_JOB_START_FAIL;
	    jStatusChange(jData, JOB_STAT_PEND, LOG_IT, fname);
        }
	goto Leave;
    }

    if (IS_START(jData->jStatus)) {
        jData->jobPid = jobReply.jobPid;
        jData->jobPGid = jobReply.jobPGid;

	log_startjobaccept(jData);
	
	if (daemonParams[LSB_MBD_BLOCK_SEND].paramValue == NULL) {
	    struct Buffer *replyBuf;
	    
	    if (chanAllocBuf_(&replyBuf, sizeof(struct LSFHeader)) < 0) {
		ls_syslog(LOG_ERR, I18N_FUNC_FAIL_M, fname, "chanAllocBuf_");
		goto Leave;
	    }

	    memcpy((char *) replyBuf->data, (char *) buf->data,
		   LSF_HEADER_LEN);

	    replyBuf->len = LSF_HEADER_LEN;
	    
	    if (chanEnqueue_(sbdPtr->chanfd, replyBuf) < 0) {
		ls_syslog(LOG_ERR, I18N_FUNC_FAIL_M, fname, "chanEnqueue");
		chanFreeBuf_(replyBuf);
	    } else {
			    
		sbdPtr->reqCode = MBD_NEW_JOB_KEEP_CHAN;
	    }
	} else {
		
	    hdr.opCode = LSBE_NO_ERROR;
		
	    s = chanSock_(sbdPtr->chanfd);
	    io_block_(s);

	    if ((cc = writeEncodeHdr_(sbdPtr->chanfd, &hdr, chanWrite_)) < 0) {
		ls_syslog(LOG_ERR, I18N_JOB_FAIL_S_MM, 
		    fname, lsb_jobid2str(jData->jobId), "writeEncodeHdr");
	    }
	}
    }
    
  Leave:

    xdr_destroy(&xdrs);
    chanFreeBuf_(buf);    

} 


void
doProbeReply(struct sbdNode *sbdPtr, int exception)
{
    static char fname[] = "doProbeReply";
    struct LSFHeader replyHdr;
    XDR xdrs;
    char *toHost = sbdPtr->hData->host;
    struct Buffer *buf;
    
    if (logclass & LC_COMM)
	ls_syslog(LOG_DEBUG, "%s: Entering ...", fname);

    if (exception == TRUE || chanRecv_(sbdPtr->chanfd, &buf) < 0) {
        if (exception == TRUE)
            ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 7894,
		"%s: Exception bit of <%d> is set for host <%s>"), /* catgets 7894 */
                fname, 
		sbdPtr->chanfd, 
		toHost); 
        else
	    ls_syslog(LOG_ERR, I18N_HOST_FAIL,
		fname,
		toHost,
		"chanRecv_");
	sbdPtr->hData->flags |= HOST_NEEDPOLL;
	return;
    }

    xdrmem_create(&xdrs, buf->data, buf->len, XDR_DECODE);
    
    if (!xdr_LSFHeader(&xdrs, &replyHdr)) {
        ls_syslog(LOG_ERR, I18N_HOST_FAIL,
	    fname,
	    toHost,
	    "xdr_LSFHeader");
	sbdPtr->hData->flags |= HOST_NEEDPOLL;
    } else {
#ifdef INTER_DAEMON_AUTH
	if (authSbdRequest(sbdPtr, &xdrs, &replyHdr, NULL) != LSBE_NO_ERROR) {
	    ls_syslog(LOG_ERR, I18N_HOST_FAIL, fname,
		      toHost, "authSbdRequest");
	    sbdPtr->hData->flags |= HOST_NEEDPOLL;
	} else {
#endif 
    
	    if (replyHdr.opCode != ERR_NO_ERROR) {
		ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 7897,
						 "%s: sbatchd replied <%d> for host <%s>"), /* catgets 7897 */
			  fname, replyHdr.opCode, toHost);
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
    chanFreeBuf_(buf);    

} 

void
doSwitchJobReply(struct sbdNode *sbdPtr, int exception)
{
    static char fname[] = "doSwitchJobReply";
    struct LSFHeader replyHdr;
    XDR xdrs;
    struct jData *jData = sbdPtr->jData;
    char *toHost = sbdPtr->hData->host;
    struct jobReply jobReply;
    struct Buffer *buf;

    
    if (logclass & LC_COMM)
	ls_syslog(LOG_DEBUG, "%s: Entering ...", fname);

    if (!IS_START (jData->jStatus)) 
	
	return;

    if (exception == TRUE || chanRecv_(sbdPtr->chanfd, &buf) < 0) {
        if (exception == TRUE)
            ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 7898,
		"%s: Exception bit of <%d> is set for job <%s>"), /* catgets 7898 */
                fname, 
		sbdPtr->chanfd, 
		lsb_jobid2str(jData->jobId)); 
        else
	    ls_syslog(LOG_ERR, I18N_JOB_FAIL_S, 
		fname,
		lsb_jobid2str(jData->jobId),
		"chanRecv_");
	jData->pendEvent.notSwitched = TRUE;
	eventPending = TRUE;	
	return;
    }

    xdrmem_create(&xdrs, buf->data, buf->len, XDR_DECODE);
    
    if (!xdr_LSFHeader(&xdrs, &replyHdr)) {
        ls_syslog(LOG_ERR, I18N_JOB_FAIL_S,
	    fname, 
	    lsb_jobid2str(jData->jobId),
	    "xdr_LSFHeader");
	jData->pendEvent.notSwitched = TRUE;
	eventPending = TRUE;		
	goto Leave;
    }

#ifdef INTER_DAEMON_AUTH
    if (authSbdRequest(sbdPtr, &xdrs, &replyHdr, NULL) != LSBE_NO_ERROR) {
	ls_syslog(LOG_ERR, I18N_JOB_FAIL_S, fname,
		  lsb_jobid2str(jData->jobId), "authSbdRequest");
	jData->pendEvent.notSwitched = TRUE;
	eventPending = TRUE;
	goto Leave;
    }
#endif 
    
    switch (replyHdr.opCode) {
    case ERR_NO_ERROR:
        if (!xdr_jobReply (&xdrs, &jobReply, &replyHdr) 
              || jData->jobId != jobReply.jobId ) {
            if (jData->jobId != jobReply.jobId )
                ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 7901,
		    "%s: Got bad jobId <%s> for job <%s>"), /* catgets 7901 */
		    fname, 
		    lsb_jobid2str(jobReply.jobId), 
		    lsb_jobid2str(jData->jobId)); 
            else 
                ls_syslog(LOG_ERR, I18N_JOB_FAIL_S, 
		    fname, lsb_jobid2str(jData->jobId),
		    "xdr_jobReply");
        }
	break;
    case ERR_NO_JOB:        
        job_abort(jData, JOB_MISSING);  
        ls_syslog(LOG_INFO, (_i18n_msg_get(ls_catd , NL_SETN, 7913, "%s: Job <%s> was missing by sbatchd on host <%s>\n")), fname, lsb_jobid2str(jData->jobId), toHost);  
	/* catgets 7913 */
        break; 
    case ERR_JOB_FINISH:
        break;
    case ERR_BAD_REQ:             
	ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 7903,
	    "%s: Job <%s>: sbatchd on host <%s> complained of bad request"), fname, lsb_jobid2str(jData->jobId), toHost); /* catgets 7903 */
	break;
    default:
        ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 7904,
	    "%s: Job <%s>: Illegal reply code <%d> from sbatchd on host <%s>"), fname, lsb_jobid2str(jData->jobId), replyHdr.opCode, toHost); /* catgets 7904 */
    }

  Leave:

    xdr_destroy(&xdrs);
    chanFreeBuf_(buf);    

} 


void
doSignalJobReply(struct sbdNode *sbdPtr, int exception)
{
    static char fname[] = "doSignalJobReply";
    struct LSFHeader replyHdr;
    XDR xdrs;
    struct jData *jData = sbdPtr->jData;

    struct jobReply jobReply;
    struct Buffer *buf;
    
    if (logclass & LC_COMM)
	ls_syslog(LOG_DEBUG, "%s: Entering ...", fname);

    if (IS_FINISH(jData->jStatus)) {
	
	return;
    } else if (IS_PEND (jData->jStatus)) {
	
	if (sbdPtr->sigVal != SIG_CHKPNT && sbdPtr->sigVal != SIG_CHKPNT_COPY) {
	    sigPFjob (jData, sbdPtr->sigVal, 0, now); 
        }
	return;
    }       

    if (exception == TRUE || chanRecv_(sbdPtr->chanfd, &buf) < 0) {
        if (exception == TRUE)
            ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 7905,
		"%s: Exception bit of <%d> is set for job <%s>"), /* catgets 7905 */
                fname, sbdPtr->chanfd, lsb_jobid2str(jData->jobId)); 
        else
	    ls_syslog(LOG_ERR, I18N_JOB_FAIL_S_M,
		fname,
		lsb_jobid2str(jData->jobId),
		"chanRecv_");
	addPendSigEvent(sbdPtr);
	return;
    }


    xdrmem_create(&xdrs, buf->data, buf->len, XDR_DECODE);
    
    if (!xdr_LSFHeader(&xdrs, &replyHdr)) {
        ls_syslog(LOG_ERR, I18N_JOB_FAIL_S_M, 
	    fname, 
	    lsb_jobid2str(jData->jobId),
	    "xdr_LSFHeader");
        addPendSigEvent(sbdPtr);
	goto Leave;
    }

#ifdef INTER_DAEMON_AUTH
    if (authSbdRequest(sbdPtr, &xdrs, &replyHdr, NULL) != LSBE_NO_ERROR) {
	ls_syslog(LOG_ERR, I18N_JOB_FAIL_S, fname,
		  lsb_jobid2str(jData->jobId), "authSbdRequest");
	addPendSigEvent(sbdPtr);
	goto Leave;
    }
#endif 
    
    if (logclass & LC_SIGNAL)
	ls_syslog(LOG_DEBUG, "%s: Job <%s> sigVal %d got reply code=%d",
		  fname, lsb_jobid2str(jData->jobId),
		  sbdPtr->sigVal, replyHdr.opCode);

    signalReplyCode(replyHdr.opCode, jData, sbdPtr->sigVal, sbdPtr->sigFlags);

    switch (replyHdr.opCode) {
    case ERR_NO_ERROR:
	if (!xdr_jobReply (&xdrs, &jobReply, &replyHdr)
            || jData->jobId != jobReply.jobId ) {
            if (jData->jobId != jobReply.jobId )
                ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 7908,
		    "%s: Got bad jobId <%s> for job <%s>"),  /* catgets 7908 */
                    fname, lsb_jobid2str(jobReply.jobId), jData->jobId);
            else
                ls_syslog(LOG_ERR, I18N_JOB_FAIL_S_M,
		    fname, lsb_jobid2str(jData->jobId),
		    "xdr_jobReply");
	    addPendSigEvent(sbdPtr);
	} else {
	    jobStatusSignal(replyHdr.opCode, jData, sbdPtr->sigVal,
			    sbdPtr->sigFlags, &jobReply);
	}
	break;
	
    case ERR_NO_JOB:
        if (getJobData (jData->jobId) != NULL)
            addPendSigEvent(sbdPtr);
        break;
        
    default:
	addPendSigEvent(sbdPtr);	
    }

  Leave:

    xdr_destroy(&xdrs);
    chanFreeBuf_(buf);    

} 


 
static void
addPendSigEvent(struct sbdNode *sbdPtr)
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
    


int 
ctrlMbdDebug(struct debugReq *pdebug,  struct lsfAuth *auth)
{
    static char fname[]="ctrlMbdDebug";
    int opCode, level, newClass, options;
    char logFileName[MAXLSFNAMELEN];
    char lsfLogDir[MAXPATHLEN];
    char *dir=NULL;

    memset(logFileName, 0, sizeof(logFileName));
    memset(lsfLogDir, 0, sizeof(lsfLogDir));
 
    if (!isAuthManager(auth) && auth->uid != 0 ) {
        ls_syslog(LOG_CRIT, 
	  I18N(7916,"ctrlMbdDebug: uid <%d> not allowed to perform control operation"),/*catgets 7916*/ 
	  auth->uid);
        return (LSBE_PERMISSION);
     }

    opCode = pdebug->opCode;
    level = pdebug->level;
    newClass = pdebug->logClass;
    options  = pdebug->options;
 
    if (pdebug->logFileName[0] != '\0') {
	if (((dir=strrchr(pdebug->logFileName,'/')) != NULL) ||  
	    ((dir=strrchr(pdebug->logFileName,'\\')) != NULL)) {
	    dir++;
	    ls_strcat(logFileName, sizeof(logFileName), dir);
	    *(--dir)='\0';
	    ls_strcat(lsfLogDir, sizeof(lsfLogDir), pdebug->logFileName);
	}         
	else {
	    ls_strcat(logFileName, sizeof(logFileName), pdebug->logFileName);
            if (daemonParams[LSF_LOGDIR].paramValue
                && *(daemonParams[LSF_LOGDIR].paramValue)) {
		ls_strcat(lsfLogDir, sizeof(lsfLogDir),
			  daemonParams[LSF_LOGDIR].paramValue);
            }
            else {
                lsfLogDir[0] = '\0';
            }
	}
	ls_strcat(logFileName, sizeof(logFileName), ".mbatchd");
    }
    else {
	ls_strcat(logFileName, sizeof(logFileName), "mbatchd");
	if (daemonParams[LSF_LOGDIR].paramValue
	    && *(daemonParams[LSF_LOGDIR].paramValue)) {
	    ls_strcat(lsfLogDir, sizeof(lsfLogDir), 
		      daemonParams[LSF_LOGDIR].paramValue);
	} else {
	    lsfLogDir[0] = '\0';
	}
    }

    if (options==1) { 
  
        struct config_param *plp;

        for (plp = daemonParams; plp->paramName != NULL; plp++) {
	     if (plp->paramValue != NULL) 
		     FREEUP(plp->paramValue);
	 }

        if (initenv_(daemonParams, env_dir) < 0) {
	    ls_openlog("mbatchd", daemonParams[LSF_LOGDIR].paramValue,
                       (debug > 1 || lsb_CheckMode), 
		       daemonParams[LSF_LOG_MASK].paramValue);
            ls_syslog(LOG_ERR, I18N_FUNC_FAIL_MM, fname, "initenv_");

           if (!lsb_CheckMode)
		mbdDie(MASTER_FATAL);
	   else
	        lsb_CheckError = FATAL_ERR;
           return(lsb_CheckError);
         }

         
         getLogClass_(daemonParams[LSB_DEBUG_MBD].paramValue,
   		     daemonParams[LSB_TIME_MBD].paramValue);

         closelog();

         if (lsb_CheckMode)
             ls_openlog("mbatchd", daemonParams[LSF_LOGDIR].paramValue, TRUE,
			 "LOG_WARN");
         else if (debug > 1)
	     ls_openlog("mbatchd", daemonParams[LSF_LOGDIR].paramValue, TRUE,
	                  daemonParams[LSF_LOG_MASK].paramValue);
         else
	      ls_openlog("mbatchd", daemonParams[LSF_LOGDIR].paramValue, 
			 FALSE, daemonParams[LSF_LOG_MASK].paramValue);
         if (logclass & LC_TRACE)
	     ls_syslog(LOG_DEBUG, "%s: logclass=%x", fname, logclass);

         return(LSBE_NO_ERROR);
     }

    if (opCode==MBD_DEBUG) {

        if (logclass & LC_TRACE)
            ls_syslog(LOG_DEBUG, 
	         "ctrMbdDebug:filename= %s: newclass=%x, level = %d",
	         logFileName, newClass, level);
	putMaskLevel(level,  &(daemonParams[LSF_LOG_MASK].paramValue));

	if (logclass & LC_TRACE)
	   ls_syslog(LOG_DEBUG, "%s: LSF_LOG_MASK =%s", fname, daemonParams[LSF_LOG_MASK].paramValue); 

        if (newClass >= 0)
            logclass = newClass;

        if ( pdebug->level>=0 ){
	    

            closelog();

	    if (debug > 1)
	        ls_openlog(logFileName, lsfLogDir,
			   TRUE, daemonParams[LSF_LOG_MASK].paramValue);
	    else
	        ls_openlog(logFileName, lsfLogDir, 
	  		FALSE, daemonParams[LSF_LOG_MASK].paramValue);
         }

       
  }
  else if (opCode == MBD_TIMING) { 
       
      if (level>=0)
          timinglevel = level;
      if (pdebug->logFileName[0] != '\0') { 
	  closelog();
          if (debug > 1)
	      ls_openlog(logFileName, lsfLogDir, TRUE,
			 daemonParams[LSF_LOG_MASK].paramValue);
          else
	      ls_openlog(logFileName, lsfLogDir,FALSE,
			 daemonParams[LSF_LOG_MASK].paramValue);
       }
  }
  else {
      ls_perror("No this debug command!\n");
      return (-1);
  }

  return (LSBE_NO_ERROR);

} 


int
do_debugReq(XDR * xdrs, int chfd, struct sockaddr_in * from,
 	char *hostName, struct LSFHeader * reqHdr, struct lsfAuth * auth)
{
    static char fname[]="do_debugReq";
     struct debugReq   debugReq;
     char                reply_buf[MSGSIZE];
     XDR                 xdrs2;
     int                 reply; 
     struct LSFHeader        replyHdr;

     if (!xdr_debugReq(xdrs, &debugReq, reqHdr)) {
	 reply = LSBE_XDR;
	 ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "xdr_debugReq");
     } else {
         if ( debugReq.opCode == 1 || debugReq.opCode == 2)
              reply = ctrlMbdDebug(&debugReq, auth);
         else if ( debugReq.opCode == 3 || debugReq.opCode == 4){
                  
 	          reply = callSbdDebug(&debugReq);
		  if (reply == ERR_NO_ERROR)
		      reply = LSBE_NO_ERROR;
	          else
		      reply = LSBE_SBATCHD;
                }

         else 
	    {
	      ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 7912,
		  "%s: Bad opCode <%d>"), /* catgets 7912 */
		  fname,
		  debugReq.opCode);
	      return -1;
            }
     }

     xdrmem_create(&xdrs2, reply_buf, MSGSIZE, XDR_ENCODE);
     replyHdr.opCode = reply;

     replyHdr.length = 0;
     if (!xdr_LSFHeader(&xdrs2, &replyHdr)) {
          ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "xdr_LSFHeader");
          xdr_destroy(&xdrs2);
          return -1;
        }
  
     if (chanWrite_(chfd, reply_buf, XDR_GETPOS(&xdrs2)) <= 0) {
         ls_syslog(LOG_ERR, I18N_FUNC_D_FAIL_M,
	     fname, 
	     "chanWrite_",
	     XDR_GETPOS(&xdrs2));
         xdr_destroy(&xdrs2);
	 return -1;
     }
     xdr_destroy(&xdrs2);
     return 0;

 }      

int
do_resourceInfoReq (XDR *xdrs, int chfd, struct sockaddr_in *from,
                 struct LSFHeader *reqHdr)
{
    static char fname[] = "do_resourceInfoReq";
    XDR                     xdrs2;
    static struct resourceInfoReq  resInfoReq;
    struct lsbShareResourceInfoReply   resInfoReply;
    int                     reply;
    char                   *reply_buf;
    int                     len = 0, i, j;
    static struct LSFHeader        replyHdr;
    char                   *replyStruct;


    if (logclass & LC_TRACE)
	ls_syslog(LOG_DEBUG1, "%s: Entering this routine...", fname);
    
    resInfoReply.numResources = 0;
    resInfoReply.resources = NULL;

    if (resInfoReq.numResourceNames > 0)
	xdr_lsffree(xdr_resourceInfoReq, (char *)&resInfoReq, &replyHdr);

    if (!xdr_resourceInfoReq (xdrs, &resInfoReq, reqHdr)) {
	reply = LSBE_XDR;
	ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "xdr_resourceInfoReq");
	len = MSGSIZE;
    } else {
	if (numResources == 0) {
	    reply = LSBE_NO_RESOURCE;
	    len = MSGSIZE;
        } else {
            reply = checkResources (&resInfoReq, &resInfoReply);

	    len = ALIGNWORD_(resInfoReply.numResources 
		  * (sizeof(struct lsbSharedResourceInfo) + MAXLSFNAMELEN)) + 100;

	    for (i = 0; i < resInfoReply.numResources; i++) {
	        for (j = 0; j < resInfoReply.resources[i].nInstances; j++) {
		    len += ALIGNWORD_(sizeof (struct lsbSharedResourceInstance)
			   + MAXLSFNAMELEN 
			   + (resInfoReply.resources[i].instances[j].nHosts 
			       * MAXHOSTNAMELEN)) + 4;
                }
            }
        }
	len += 4*MSGSIZE;
    }
    reply_buf = (char *) my_malloc(len, fname);
    xdrmem_create(&xdrs2, reply_buf, len, XDR_ENCODE);
    replyHdr.opCode = reply;
    if (reply == LSBE_NO_ERROR || reply == LSBE_BAD_RESOURCE)
        replyStruct = (char *) &resInfoReply;
    else
        replyStruct = (char *) 0;
    if (!xdr_encodeMsg (&xdrs2, replyStruct, &replyHdr, 
        xdr_lsbShareResourceInfoReply, 0 , NULL))
    {
        ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "xdr_encodeMsg");
        xdr_destroy(&xdrs2);
        FREEUP(reply_buf);
	freeShareResourceInfoReply (&resInfoReply);
        return(-1);
    }
    if (chanWrite_(chfd, reply_buf, XDR_GETPOS(&xdrs2)) <= 0) {
        ls_syslog(LOG_ERR, I18N_FUNC_D_FAIL_M,
	    fname, 
	    "b_write_fix",
	    XDR_GETPOS(&xdrs2));
        xdr_destroy(&xdrs2);
        FREEUP(reply_buf);
	freeShareResourceInfoReply (&resInfoReply);
        return(-1);
    }
    xdr_destroy(&xdrs2);
    FREEUP(reply_buf);
    freeShareResourceInfoReply (&resInfoReply);
    return(0);

}		


static void
freeJobHead (struct jobInfoHead *jobInfoHead)
{
    if (jobInfoHead == NULL)
	return;

    FREEUP (jobInfoHead->jobIds);
    FREEUP (jobInfoHead->hostNames);

} 

static void
freeJobInfoReply (struct jobInfoReply *job)
{
    int i;

    if (job == NULL)
        return;

    
    freeSubmitReq (job->jobBill);
    if (job->numToHosts > 0) {
	for (i = 0; i < job->numToHosts; i++)
	    FREEUP(job->toHosts[i]);
	FREEUP(job->toHosts);
    }
} 

static void
freeShareResourceInfoReply (struct  lsbShareResourceInfoReply *reply)
{
    int i, j, k;

    if (reply == NULL)
        return;

    for (i = 0; i < reply->numResources; i++) {
	for (j = 0; j < reply->resources[i].nInstances; j++) {
	    FREEUP (reply->resources[i].instances[j].totalValue);
            FREEUP (reply->resources[i].instances[j].rsvValue);
	    for (k = 0; k < reply->resources[i].instances[j].nHosts; k++)
		FREEUP (reply->resources[i].instances[j].hostList[k]);
	    FREEUP(reply->resources[i].instances[j].hostList);
        }
	FREEUP(reply->resources[i].instances);
    }
    FREEUP (reply->resources);

} 

int
do_runJobReq(XDR*                 xdrs, 
	     int                  chfd, 
	     struct sockaddr_in*  from,
	     struct lsfAuth *     auth,
	     struct LSFHeader*    reqHeader)
{
    static char          fname[] = "do_runJobReq()";
    struct runJobRequest runJobRequest;
    XDR                  replyXdr;
    struct LSFHeader     lsfHeader;
    char                 reply_buf[MSGSIZE/2];
    int                  reply;

    memset((struct runJobRequest *)&runJobRequest, 0, 
	   sizeof(struct runJobRequest));

    
    if (!xdr_runJobReq(xdrs, &runJobRequest, reqHeader)) {
	reply = LSBE_XDR;
	ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "xdr_runJobReq");
	goto Reply;
    }
    
    
    reply = runJob(&runJobRequest, auth);

Reply:
    
    
    xdr_lsffree(xdr_runJobReq, (char *)&runJobRequest, reqHeader);

    
    xdrmem_create(&replyXdr, 
		  reply_buf, 
		  MSGSIZE/2, 
		  XDR_ENCODE);

    lsfHeader.opCode = reply;

    if (!xdr_encodeMsg(&replyXdr, 
		       NULL, 
		       &lsfHeader, 
		       xdr_int, 
		       0, 
		       NULL)) {
	ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "xdr_encodeMsg");
	xdr_destroy(&replyXdr);
	return(-1);
    }

    
    if (chanWrite_(chfd, 
		   reply_buf, 
		   XDR_GETPOS(&replyXdr)) <= 0) {
	ls_syslog(LOG_ERR, I18N_FUNC_FAIL_MM, fname, "chanWrite_");
	xdr_destroy(&replyXdr);
	return(-1);
    }

    xdr_destroy(&replyXdr);
    return(0);
}

int
do_setJobAttr(XDR * xdrs, int s, struct sockaddr_in * from, char *hostName,
	 struct LSFHeader * reqHdr, struct lsfAuth * auth)
{
    static char          fname[] = "do_setJobAttr()";
    struct jobAttrInfoEnt jobAttr;
    struct jData *job;
    XDR xdrs2;
    char reply_buf[MSGSIZE];
    struct LSFHeader replyHdr;
    int reply;

    if (!xdr_jobAttrReq(xdrs, &jobAttr, reqHdr)) {
        reply = LSBE_XDR;
        ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "xdr_jobAttrReq");
    } else {  

   	
        if ((job = getJobData (jobAttr.jobId)) == NULL) {
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
    replyHdr.opCode = reply;
    if (!xdr_encodeMsg(&xdrs2, (char *) &jobAttr, &replyHdr, xdr_jobAttrReq,
                       0, NULL)) {
        ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "xdr_encodeMsg");
        xdr_destroy(&xdrs2);
        return (-1);
    }
    if (chanWrite_(s, reply_buf, XDR_GETPOS(&xdrs2)) != XDR_GETPOS(&xdrs2)) {
        ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "chanWrite_");
        xdr_destroy(&xdrs2);
        return (-1);
    }
    xdr_destroy(&xdrs2);
    return (0);
} 


#ifdef INTER_DAEMON_AUTH
static int
authSbdRequest(struct sbdNode *sbdPtr,
	       XDR *xdrs,
	       struct LSFHeader *reqHdr,
	       struct sockaddr_in *from_host)
{
    char buf[1024];

    sprintf(buf, "mbatchd@%s", clusterName);
    return authDaemonRequest(sbdPtr ? sbdPtr->chanfd : -1, xdrs, reqHdr, from_host,
                             "sbatchd", buf);
} 

int
authDaemonRequest(int chanfd,
                  XDR *xdrs,
                  struct LSFHeader *reqHdr,
                  struct sockaddr_in *from_host,
                  char *client,
                  char *server)
{
    static char fname[] = "authDaemonRequest"; 
    int s, rc, from_len;
    struct sockaddr_in from;
    struct lsfAuth auth;


    
    if (daemonParams[LSF_AUTH_DAEMONS].paramValue == NULL)
	return LSBE_NO_ERROR;

    if (from_host == NULL) {
	s = chanSock_(chanfd);
	from_len = sizeof(from);
	memset(&from, 0, from_len);
	if (getpeername(s, (struct sockaddr *)&from, &from_len) == -1) {
	    ls_syslog(LOG_ERR, I18N_FUNC_FAIL_M, fname, "getpeername");
	    return (LSBE_SYS_CALL);
	}
    }
    else {
	memcpy(&from, from_host, sizeof(from));
    }
    
    if (!xdr_lsfAuth(xdrs, &auth, reqHdr)) {
	ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "xdr_lsfAuth");
	return (LSBE_XDR);
    }

    
    putEauthClientEnvVar(client);
    putEauthServerEnvVar(server);
    rc = verifyEAuth_(&auth, &from);
    if (rc == -1) {
        ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "verifyEAuth");
        return (LSBE_PERMISSION);
    }
 
    return (LSBE_NO_ERROR);

} 

#endif 
