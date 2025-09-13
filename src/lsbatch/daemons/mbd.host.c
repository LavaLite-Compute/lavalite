/* $Id: mbd.host.c,v 1.15 2007/08/15 22:18:45 tmizan Exp $
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
#include <sys/types.h>
#include <sys/wait.h>
#include <math.h>
#include "mbd.h"
#include <stdlib.h>

#include "../../lsf/lib/lsi18n.h"
#define NL_SETN		10	

static void   initHostStat(void);
static void   hostJobs(struct hData *hp, int stateTransit);
static void   hostQueues(struct hData *hp, int stateTransit);
static void   copyHostInfo (struct hData *, struct hostInfoEnt *);
static int    getAllHostInfoEnt (struct hostDataReply *, struct hData **,
				 struct infoReq *);
static int    returnHostInfo(struct hostDataReply *, int, struct hData **, 
			     struct infoReq *); 

static struct resPair * getResPairs(struct hData *);
static void   setHostBLStatus(void);
static int    hasResReserve(struct resVal *);
static void getAHostInfo (struct hData *hp);

typedef enum {
        OK_UNREACH	= 0,
        UNREACH_OK	= 1,
        UNREACH_UNAVAIL = 2,
        UNAVAIL_OK	= 3
} hostChange;

int
checkHosts (struct infoReq *hostsReqPtr,
            struct hostDataReply *hostsReplyPtr)
{
    static char fname[] = "checkHosts";
    struct hData *hData; 
    struct hData **hDList = NULL;
    struct gData *gp;
    int i, k, numHosts = 0;

    if (logclass & LC_TRACE)
        ls_syslog(LOG_DEBUG, "%s: Entering this routine...", fname);

    hostsReplyPtr->numHosts = 0;                         
    hostsReplyPtr->nIdx = allLsInfo->numIndx;
    hostsReplyPtr->hosts = (struct hostInfoEnt *) my_calloc (numofhosts,
				     sizeof(struct hostInfoEnt), "checkHosts");

    for (i = 0; i < allLsInfo->nRes; i++) {
        if (allLsInfo->resTable[i].flags &  RESF_SHARED 
            && allLsInfo->resTable[i].valueType == LS_NUMERIC) {
            hostsReplyPtr->flag |= LOAD_REPLY_SHARED_RESOURCE;
            break;
        }
    }

    if (hDList == NULL)
        hDList = (struct hData **)my_calloc(numofhosts, sizeof (struct hData *), "checkHosts");    

    if (hostsReqPtr->numNames == 0) 
        
        return (getAllHostInfoEnt (hostsReplyPtr, hDList, hostsReqPtr));

    for(i = 0; i < hostsReqPtr->numNames; i++) {     
        
	if (strcmp (hostsReqPtr->names[i], LOST_AND_FOUND) == 0) {
	    if ((hData = getHostData(LOST_AND_FOUND)) != NULL
                && hData->numJobs != 0 && (!hostsReqPtr->resReq || 
				     hostsReqPtr->resReq[0] == '\0')) {
                hDList[numHosts++] = hData;
            }
            continue;
	}
	if ((gp = getHGrpData (hostsReqPtr->names[i])) == NULL) {
	    
            if ((hData = getHostData(hostsReqPtr->names[i])) == NULL
		|| (hData->hStatus & HOST_STAT_REMOTE)) {
                hostsReplyPtr->numHosts = 0;
                hostsReplyPtr->badHost = i;
                return (LSBE_BAD_HOST);
	    } 

            hDList[numHosts++] = hData;
            continue;
	}

        
        if (gp->memberTab.numEnts != 0 || gp->numGroups != 0) {  
	    
	    char *members;     
	    char *rest, *host;
	    char found = TRUE;
	    members = getGroupMembers (gp, TRUE); 
	    rest = members;
	    host = rest;
	    while (found) {
		found = FALSE;
		for (k = 0; k < strlen(rest); k++)   
		    if (rest[k] == ' ') {
			rest[k] = '\0';
			host = rest;            
			rest = &rest[k] + 1;    
			found = TRUE;
			break; 
		    }
		if (found) {
		    char duplicate = FALSE;
		    for (k = 0; k < numHosts; k++) {
			if (equalHost_(host, hDList[k]->host)) {
			    duplicate = TRUE;
			    break;
			}
		    }
		    if (duplicate)
			continue; 
		    hData = getHostData(host);
		    if (! hData) {
			continue; 
		    }
                    hDList[numHosts++] = hData;
		}
	    } 
	    free (members);
        } else {
            
            return (getAllHostInfoEnt (hostsReplyPtr, hDList, hostsReqPtr));
        }
    } 

    if (numHosts == 0) {
        FREEUP(hDList);
        return (LSBE_BAD_HOST);
    }

    return (returnHostInfo(hostsReplyPtr, numHosts, hDList, hostsReqPtr));

} 

static int
returnHostInfo (struct hostDataReply *hostsReplyPtr, int numHosts, 
           struct hData **hDList, struct infoReq *hostReq)
{
    int i;
    struct hostInfoEnt *hInfo;
    struct resVal *resVal = NULL;
    struct candHost *candHosts = NULL;
    static char fname[] = "returnHostInfo() ";

    if (logclass & LC_EXEC){
	ls_syslog(LOG_DEBUG, "%s: Entering this routine...", fname);
	ls_syslog(LOG_DEBUG, "%s: The numHosts parameter is %d", fname, numHosts);	
   	ls_syslog(LOG_DEBUG, "%s: The hostReq->resReq is %s", fname, hostReq->resReq);
   	ls_syslog(LOG_DEBUG, "%s: The hostReq->options is %d", fname, hostReq->options); 
    }

    if (hostReq->resReq[0] != '\0') {
        
	if ((resVal = checkResReq (hostReq->resReq, USE_LOCAL)) == NULL) 
	{
            return (LSBE_BAD_RESREQ);
        }
        if (resVal->selectStr) {  
            int noUse;

            
            struct hData* fromHost = NULL;

            if ((hostReq->numNames == 0) && (hostReq->names) && (hostReq->names[0])) {
                fromHost = getHostData(hostReq->names[0]);
            }

            getHostsByResReq(resVal, &numHosts, hDList, NULL, fromHost, &noUse);
            if (numHosts == 0) {
                return (LSBE_NO_ENOUGH_HOST);
            }
            if (hostReq->options & SORT_HOST) {
		if (candHosts == NULL)
                candHosts = (struct candHost *) 
			my_calloc (numofhosts, sizeof(struct candHost), "returnHostInfo");
                for (i = 0; i < numHosts; i++)
                    candHosts[i].hData = hDList[i];
                
                numHosts = findBestHosts (NULL, resVal, numHosts, numHosts, candHosts, FALSE);
		for ( i= 0; i< numHosts; i++ )
		    hDList[i] = candHosts[i].hData;
            }
        }
    }
    for (i = 0; i < numHosts; i++) {
	int k, lostandfound=0;
	if (logclass & (LC_EXEC)) {
  		ls_syslog(LOG_DEBUG, "%s, host[%d]'s name is %s", fname, i, 
			  hDList[i]->host);	
	}
        hInfo = &(hostsReplyPtr->hosts[hostsReplyPtr->numHosts]);

	
	for(k=0; k < hostsReplyPtr->numHosts; k++) {
	    lostandfound = 0;
	    if ( strcmp(hDList[i]->host, hDList[k]->host) == 0
		 &&  strcmp(hDList[i]->host, LOST_AND_FOUND) == 0) {
		
		struct hostInfoEnt *orgHInfo;
		orgHInfo =  &(hostsReplyPtr->hosts[k]);
		orgHInfo->numJobs  += hDList[i]->numJobs;
	        orgHInfo->numRUN   += hDList[i]->numRUN;
		orgHInfo->numSSUSP += hDList[i]->numSSUSP;
		orgHInfo->numUSUSP += hDList[i]->numUSUSP;
		lostandfound = 1;
		break;
	    }
	}
	if ( !lostandfound) {
	    copyHostInfo (hDList[i], hInfo);
	    hostsReplyPtr->numHosts++;
	} 
    } 

    return (LSBE_NO_ERROR);
} 

static void
copyHostInfo (struct hData *hData, struct hostInfoEnt *hInfo)
{
    hInfo->host = hData->host;
    hInfo->cpuFactor = hData->cpuFactor;
    hInfo->loadSched = hData->loadSched; 
    hInfo->loadStop = hData->loadStop;

    hInfo->windows = (hData->windows != NULL)? hData->windows : "";
    hInfo->hStatus = hData->hStatus;
    hInfo->userJobLimit = hData->uJobLimit;
    hInfo->maxJobs = hData->maxJobs;
    hInfo->numJobs = hData->numJobs;
    hInfo->numRUN = hData->numRUN;
    hInfo->numSSUSP = hData->numSSUSP;
    hInfo->numUSUSP = hData->numUSUSP;
    hInfo->numRESERVE = hData->numRESERVE;
    hInfo->busySched = hData->busySched;
    hInfo->busyStop = hData->busyStop;
    hInfo->realLoad = hData->lsfLoad;
    hInfo->load = hData->lsbLoad;

    hInfo->mig = (hData->mig != INFINIT_INT) ? hData->mig/60 : INFINIT_INT;
    switch (hData->chkSig) {
      case SIG_CHKPNT:
	hInfo->attr = H_ATTR_CHKPNTABLE;
	break;
      case SIG_CHKPNT_COPY:
	hInfo->attr = H_ATTR_CHKPNTABLE; 
	break;
      default:
	hInfo->attr = 0;
    }

} 

static int
getAllHostInfoEnt (struct hostDataReply *hostsReplyPtr, 
                   struct hData **hDList, struct infoReq *hostReq)
{
    sTab hashSearchPtr;   
    hEnt *hashEntryPtr;
    struct hData *hData;
    struct hostInfoEnt *hInfo;
    int numHosts = 0;

    hostsReplyPtr->numHosts = 0;

    hashEntryPtr = h_firstEnt_(&hDataList, &hashSearchPtr);
    while (hashEntryPtr) {
        hData = (struct hData *) hashEntryPtr->hData;
        hInfo = &(hostsReplyPtr->hosts[hostsReplyPtr->numHosts]);
	hashEntryPtr = h_nextEnt_(&hashSearchPtr);
        
        if (strcmp (hData->host, LOST_AND_FOUND) == 0 && (hData->numJobs == 0
                            || (hostReq->resReq && hostReq->resReq[0] != '\0')))
            continue;
        if (!(hData->hStatus & HOST_STAT_REMOTE)) {
            hDList[numHosts++] = hData;
        } 
    }
    if (numHosts == 0) {
        return (LSBE_BAD_HOST);
    }
    return (returnHostInfo(hostsReplyPtr, numHosts, hDList, hostReq));

} 

struct hData *
getHostData (char *host)
{
    hEnt   *hostEnt;
    const char *officialName;

    hostEnt = h_getEnt_(&hDataList, host);
    if (hostEnt != NULL)
	return ((struct hData *) hostEnt->hData);

    
    if (strcmp (host, LOST_AND_FOUND) == 0)
	return (NULL);  

    
    if ((officialName = getHostOfficialByName_(host)) == NULL)
	return NULL;                             

    hostEnt = h_getEnt_(&hDataList, (char*)officialName);
    if (!hostEnt)
        return ((struct hData *) NULL);

    return ((struct hData *) hostEnt->hData);

} 


struct hData *
getHostData2 (char *host)
{
    hEnt   *hostEnt;
    struct hData *hData;
    const char *officialName;
    char* pHostName;

    hData = getHostData(host);
    if (hData)
        return(hData);

    
    if ((officialName = getHostOfficialByName_(host)) == NULL) {
	
	pHostName = host;
    } else {
	pHostName = (char*)officialName;
    }

    hData = initHData(NULL);

    
    FREEUP(hData->loadSched);
    FREEUP(hData->loadStop);
    hData->host = safeSave(pHostName);
    hData->hStatus = HOST_STAT_REMOTE;

    
    hostEnt = h_addEnt_(&hDataList, pHostName, NULL);
    hostEnt->hData = (int *)hData;
    return(hData);
} 

float *
getHostFactor (char *host)
{
    struct hData *hD;
    static float cpuFactor;
    const char *officialName;
    struct hostInfo *hInfo;
    char   officialNameBuf[MAXHOSTNAMELEN];

    if (host == NULL || strlen(host) == 0) {
        if ((hD = getHostData (ls_getmyhostname())) == NULL)
            return NULL;
    } else {
	if ((hD = getHostData (host)) == NULL) {
	    
	    if ((officialName = getHostOfficialByName_(host)) == NULL)
		return NULL;                             
             
            strcpy(officialNameBuf, officialName);
	    if ((hD = getHostData ((char*)officialNameBuf)) == NULL) {
                if ((hInfo = getLsfHostData((char*)officialNameBuf)) != NULL) 
                    return (&hInfo->cpuFactor);
		return NULL;               
            }
	}
    }

    if (hD->hStatus & HOST_STAT_REMOTE)
	return(NULL);

    cpuFactor = hD->cpuFactor;
    return (&cpuFactor);

} 

float *
getModelFactor (char *hostModel)
{
    static float cpuFactor;
    int i;

    if (hostModel == NULL || strlen(hostModel) == 0)
        return NULL;

    for (i = 0; i < allLsInfo->nModels; i++)
        if (strcmp (hostModel, allLsInfo->hostModels[i]) == 0) {
            cpuFactor = allLsInfo->cpuFactor[i];
            return (&cpuFactor);
        }

    return (NULL);
} 

int
getModelFactor_r(char *hostModel, float *cpuFactor)
{
    int i;

    if (hostModel == NULL || strlen(hostModel) == 0 || cpuFactor == NULL)
        return -1;

    for (i = 0; i < allLsInfo->nModels; i++)
        if (strcmp (hostModel, allLsInfo->hostModels[i]) == 0) {
            *cpuFactor = allLsInfo->cpuFactor[i];
            return 0;
        }

    return (-1);
} 

hEnt *
findHost (char *hname)
{
    if (numofhosts == 0)        
        return NULL;

    return (h_getEnt_(&hDataList, hname));

} 

void
pollSbatchds (int mbdRunFlag)
{
    static char fname[]="pollSbatchds";
    hEnt *hashEntryPtr;
    struct hData *hData;
    int num, maxprobes;
    int result;
    static struct sTab nextHost;
    char sendJobs;

    

    if (mbdRunFlag != NORMAL_RUN) {     
        if ((hashEntryPtr = h_firstEnt_(&hDataList, &nextHost)) == NULL) {
	    ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 6000,
		"%s: No host to probe"), fname);  /* catgets 6000 */
            return;
	}
	if (mbdRunFlag == FIRST_START)
	    initHostStat ();
        maxprobes = 10;             
    } else 
        maxprobes = 1;

    for (num = 0; num < maxprobes && num < numofhosts; num++) {
        int oldStatus;

        hashEntryPtr = h_nextEnt_(&nextHost);
        if (hashEntryPtr == NULL) { 		
            hashEntryPtr = h_firstEnt_(&hDataList, &nextHost);
        }
        hData = (struct hData *) hashEntryPtr->hData;
        oldStatus = hData->hStatus;

        
        if (hData->hStatus & HOST_STAT_REMOTE)
            continue;

        if (strcmp (hData->host, LOST_AND_FOUND) == 0 
	    && strcmp(hashEntryPtr->keyname,LOST_AND_FOUND) == 0 ) {
	    
            if (countHostJobs (hData) == 0) {   
                freeHData (hData, TRUE); 
                numofhosts --;
            }
            continue;                    
        }
	
        if (now - hData->pollTime < retryIntvl * msleeptime)
            continue;
	
        if (mbdRunFlag != NORMAL_RUN) {
	    if (mbdRunFlag & FIRST_START) {
	        
	        TIMEIT(2, (result = probe_slave (hData, TRUE)), hData->host);
            } else {
		if ((mbdRunFlag & WINDOW_CONF) &&
		    (hData->flags & HOST_NEEDPOLL) 
		    && countHostJobs (hData) != 0) { 
	             
	             TIMEIT(2, (result = probe_slave (hData, TRUE)),
			    hData->host);
                 } else
		     continue;
	    }
        } else {
	    
	    
	    sendJobs = (hData->flags & HOST_NEEDPOLL);
	    
	    
	    if (LS_ISUNAVAIL(hData->limStatus)) {
		if (hData->sbdFail == 0) {
		    
		    TIMEIT(2, (result = probe_slave(hData, sendJobs)),
			   hData->host);
                    if (result == ERR_NO_ERROR)
			
                        result = ERR_NO_LIM;
		} else {
		    result = ERR_UNREACH_SBD;
		    lsberrno = LSBE_CONN_TIMEOUT;
		    
		    maxprobes++;
		}
	    } else if (LS_ISSBDDOWN(hData->limStatus)) {
		if (hData->sbdFail == 0) {
		    
		    TIMEIT(2, (result = probe_slave(hData, sendJobs)),
			   hData->host);
		} else {
		    result = ERR_UNREACH_SBD;
		    lsberrno = LSBE_CONN_REFUSED;
		    maxprobes++;
		}
	    } else {
		
		if (hData->sbdFail > 0 || sendJobs) {
                    
		    TIMEIT(2, (result = probe_slave(hData, sendJobs)),
			   hData->host);
		    
		} else {
		    result = ERR_NO_ERROR;
		    maxprobes++;			
		}
	    }
	}
	hData->pollTime = now;
        if (logclass & LC_COMM)
            ls_syslog (LOG_DEBUG3, "pollSbatchds: host <%s> status <%x> lim status<%x> result=%d sbdFail = %d", hData->host, hData->hStatus, *hData->limStatus, result, hData->sbdFail);
        if (result == ERR_NO_LIM)
            hData->hStatus |= HOST_STAT_NO_LIM;
        if (result == ERR_NO_ERROR)
            hData->hStatus &= ~HOST_STAT_NO_LIM;    

	if (result != ERR_UNREACH_SBD && result != ERR_FAIL) { 
            

	    if (hData->hStatus & HOST_STAT_UNAVAIL) {
		hostJobs (hData, UNAVAIL_OK);
		hData->hStatus &= ~HOST_STAT_UNAVAIL;
                
                getAHostInfo (hData);
	    }
	    if (hData->hStatus & HOST_STAT_UNREACH) {
		hostJobs (hData,  UNREACH_OK);
		hData->hStatus &= ~HOST_STAT_UNREACH;
                
                getAHostInfo (hData);
	    }
	    hData->sbdFail = 0;	   	        
	    hData->flags &= ~HOST_NEEDPOLL;
	} else {
	    
	    hData->sbdFail++;
            if (lsberrno == LSBE_CONN_REFUSED)
		hData->sbdFail++;                  
	    if (!(hData->hStatus & (HOST_STAT_UNREACH | HOST_STAT_UNAVAIL))
                && hData->sbdFail > 1) {
		hostJobs (hData, OK_UNREACH);
		if (logclass & LC_COMM)
		    ls_syslog(LOG_INFO, (_i18n_msg_get(ls_catd , NL_SETN, 6011, "%s: Declaring host %s unreachable. result=%d ")), "pollSbatchds", hData->host, result);   /* catgets 6011 */
		hData->hStatus |= HOST_STAT_UNREACH;
		
	    }

	    if (hData->sbdFail > 2) {      
		if (!LS_ISUNAVAIL(hData->limStatus)) {
		    if (logclass & LC_COMM)
			ls_syslog(LOG_INFO, (_i18n_msg_get(ls_catd , NL_SETN, 6012, "%s: The sbatchd on host <%s> is unreachable")), "pollSbatchds", hData->host);    /* catgets 6012 */
		    hData->hStatus |= HOST_STAT_UNREACH;
		    hData->hStatus &= ~HOST_STAT_UNAVAIL;
		    hData->sbdFail = 1;  
		} if ((hData->sbdFail >=  max_sbdFail)) {
		    hData->sbdFail = max_sbdFail;
		    if (! (hData->hStatus & HOST_STAT_UNAVAIL)) {
			if (logclass & LC_COMM)
			    ls_syslog(LOG_INFO, (_i18n_msg_get(ls_catd , NL_SETN, 6013, "%s: The sbatchd on host <%s> is unavailable")), "pollSbatchds", hData->host);         /* catgets 6013 */
			hostJobs (hData, UNREACH_UNAVAIL);
			hData->hStatus |= HOST_STAT_UNAVAIL;
			hData->hStatus &= ~HOST_STAT_UNREACH;   
		    }
		}
	    }
	}
    } 

    return;

} 

int
countHostJobs (struct hData *hData)
{
    struct jData *jp;
    int numJobs = 0, list, i;

    for (list = 0; list < NJLIST; list++) {
        if (list != SJL && list != FJL)
            continue;
	for (jp = jDataList[list]->back; jp != jDataList[list]; jp = jp->back) 
	    for (i = 0; i < jp->numHostPtr; i++)
		if (jp->hPtr && jp->hPtr[i] == hData)
		    numJobs++;       
    }

    return numJobs;
} 

static void
initHostStat (void)
{
    struct jData *jpbw;

    for (jpbw = jDataList[SJL]->back; jpbw != jDataList[SJL]; jpbw=jpbw->back) {
        if (jpbw->jStatus & JOB_STAT_UNKWN && jpbw->hPtr) {
            jpbw->hPtr[0]->hStatus |= HOST_STAT_UNREACH;
            jpbw->hPtr[0]->sbdFail = 1;
        }
    }
} 

void
hStatChange (struct hData *hp, int newStatus)
{
    if (logclass & (LC_TRACE | LC_COMM))
        ls_syslog(LOG_DEBUG,"hStatChange: host=%s newStatus=%d",hp->host, newStatus);
    hp->pollTime = now;

    if ((hp->hStatus & HOST_STAT_UNREACH) 
        && !(newStatus & (HOST_STAT_UNAVAIL | HOST_STAT_UNREACH)))
    {   
        hp->sbdFail = 0;                                
        hp->hStatus &= ~HOST_STAT_UNREACH;
        hostJobs (hp, UNREACH_OK);
        
        getAHostInfo (hp);
        goto Return;
    }
    if (!(hp->hStatus & (HOST_STAT_UNREACH | HOST_STAT_UNAVAIL))
	&& (newStatus & HOST_STAT_UNREACH)) {
        
        hp->sbdFail++;
        if (lsberrno == LSBE_CONN_REFUSED)
            hp->sbdFail++;                        
        if (hp->sbdFail > 1) {
	    hp->hStatus |= HOST_STAT_UNREACH;
	    hostJobs (hp, OK_UNREACH);
	    
        }
        goto Return;
    }

    if ((hp->hStatus & HOST_STAT_UNAVAIL) 
         && !(newStatus & (HOST_STAT_UNAVAIL | HOST_STAT_UNREACH)))
    {  
	ls_syslog(LOG_INFO, (_i18n_msg_get(ls_catd , NL_SETN, 6015, "%s: The sbatchd on host <%s> is up now")), "hStatChange", hp->host);  /* catgets 6015 */
       
        hp->sbdFail = 0;                                
        hp->hStatus &= ~HOST_STAT_UNAVAIL;
        hostJobs (hp, UNAVAIL_OK);      
        
        getAHostInfo (hp);
    }

    
Return:
    return;

} 

static void
hostJobs (struct hData *hp, int stateTransit)
{
    struct jData  *jpbw, *nextJobPtr;

    for (jpbw = jDataList[SJL]->back;
		jpbw != jDataList[SJL]; jpbw = nextJobPtr) {
	nextJobPtr = jpbw->back;  
	
	if (hp != jpbw->hPtr[0])
	    continue; 			      

	if ((stateTransit == UNREACH_OK || stateTransit == UNAVAIL_OK)
	    && (jpbw->jStatus & JOB_STAT_UNKWN)) {
	    jpbw->jStatus &= ~JOB_STAT_UNKWN;
	    log_newstatus(jpbw);              
	    continue;
	}
	if (stateTransit == OK_UNREACH && !(jpbw->jStatus & JOB_STAT_UNKWN)) {
	    jpbw->jStatus |= JOB_STAT_UNKWN;
	    log_newstatus(jpbw);             
	    continue;
	}
	if (stateTransit != UNREACH_UNAVAIL)
	    continue;

        
	if (jpbw->shared->jobBill.options & SUB_RERUNNABLE){
	    int sendMail;
	    if (jpbw->shared->jobBill.options & SUB_RERUNNABLE) {
		sendMail = TRUE;
	    } else {
		sendMail = FALSE;
	    }
	    jpbw->endTime = now;        
            jpbw->newReason = EXIT_ZOMBIE;      
            jpbw->jStatus |= JOB_STAT_ZOMBIE;
	    if ((jpbw->shared->jobBill.options & SUB_CHKPNTABLE) &&
		((jpbw->shared->jobBill.options & SUB_RESTART) ||
		 (jpbw->jStatus & JOB_STAT_CHKPNTED_ONCE))) {	    
		jpbw->newReason |= EXIT_RESTART;  
	    }
	    inZomJobList (jpbw, sendMail); 
	    jStatusChange (jpbw, JOB_STAT_EXIT, LOG_IT, "hostJobs");
	}
    } 

    hostQueues(hp, stateTransit);

} 

static void
hostQueues(struct hData *hp, int stateTransit)
{
    struct qData *qPtr;

    if (stateTransit != UNREACH_UNAVAIL && stateTransit != UNAVAIL_OK)
	return;

    for (qPtr = qDataList->forw; qPtr != qDataList; qPtr = qPtr->forw)
    {
	if (hostQMember(hp->host, qPtr)) 
	{
	    if (stateTransit & UNREACH_UNAVAIL)
		qPtr->numHUnAvail++;
	    else {
	       qPtr->numHUnAvail--;
	    }
	}
    }
} 

void
checkHWindow (void)
{
    struct hData *hp;
    struct dayhour dayhour;
    windows_t *wp;
    char windOpen;
    sTab hashSearchPtr;
    hEnt *hashEntryPtr;

    hashEntryPtr = h_firstEnt_(&hDataList, &hashSearchPtr);
    while (hashEntryPtr) {
        int oldStatus;

        hp = (struct hData *) hashEntryPtr->hData;
        oldStatus = hp->hStatus;

        hashEntryPtr = h_nextEnt_(&hashSearchPtr);
        if (hp->hStatus & HOST_STAT_REMOTE)
            goto NextLoop;
        
        if (hp->windEdge > now || hp->windEdge == 0)
            goto NextLoop;

        getDayHour (&dayhour, now);

        if (hp->week[dayhour.day] == NULL) {               
             hp->hStatus &= ~HOST_STAT_WIND;
             hp->windEdge = now + (24.0 - dayhour.hour) * 3600.0;
             goto NextLoop;
        }

        
        hp->hStatus |= HOST_STAT_WIND;              
        windOpen = FALSE;
        hp->windEdge = now + (24.0 - dayhour.hour) * 3600.0;
        for (wp = hp->week[dayhour.day]; wp; wp=wp->nextwind) {
            checkWindow(&dayhour, &windOpen, &hp->windEdge, wp, now);
            if (windOpen)                         
                hp->hStatus &= ~HOST_STAT_WIND;
        }
NextLoop:
	;
    }
} 

int
ctrlHost (struct controlReq *hcReq, struct hData *hData, struct lsfAuth *auth)
{
    sbdReplyType reply;

    switch (hcReq->opCode) {
    case HOST_REBOOT :
        reply = rebootSbd(hcReq->name);
        if (reply == ERR_NO_ERROR)
            return(LSBE_NO_ERROR);
        else
            return(LSBE_SBATCHD);

    case HOST_SHUTDOWN :
        reply = shutdownSbd(hcReq->name);
        if (reply == ERR_NO_ERROR)
            return(LSBE_NO_ERROR);
        else
            return(LSBE_SBATCHD);

    case HOST_OPEN :
        if (hData->hStatus & HOST_STAT_UNAVAIL)
            return(LSBE_SBATCHD);

        if (hData->hStatus & HOST_STAT_DISABLED) {
            hData->hStatus &= ~HOST_STAT_DISABLED;
            log_hoststatus(hData, hcReq->opCode, auth->uid, auth->lsfUserName);
            return(LSBE_NO_ERROR);
        }
        else
            return(LSBE_NO_ERROR);                        

    case HOST_CLOSE :
        if (hData->hStatus & HOST_STAT_DISABLED) {
            return(LSBE_NO_ERROR);                        
        }
        else {
            hData->hStatus |= HOST_STAT_DISABLED;
            log_hoststatus(hData, hcReq->opCode, auth->uid, auth->lsfUserName);
            return(LSBE_NO_ERROR);
        }
    default :
        return(LSBE_LSBLIB);
    }

} 

int
repeatHosts(char *host, char **hostTable, int numTable)
{
    int i;

    if (!numTable || host == NULL)
        return (FALSE);

    for (i = 0; i < numTable; i++) {
        if (hostTable[i] == NULL)
            continue;                 
            
        if (strcasecmp (hostTable[i], host) == 0)
            return (TRUE);
    }
    return (FALSE);

} 

int 
getLsbHostNames (char ***hostNames)
{
    static char fname[]="getLsbHostNames";
    static char **hosts = NULL;
    static int numHosts = 0 ;
    int i;
    struct sTab stab;
    struct hData *hData;
    hEnt *hent;

    numHosts = 0;
    FREEUP (hosts);
    if (numofhosts == 0) {
	ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 6002,
	    "%s: No host used by the batch system"), fname); /* catgets 6002 */
	*hostNames = NULL;
        return(-1);
    }

    hosts = (char **) my_malloc (numofhosts *sizeof (char *), fname);

    
    hent = h_firstEnt_ (&hDataList, &stab);
    hData = ((struct hData *) hent->hData);
    if ((strcmp (hData->host, LOST_AND_FOUND) != 0) &&
        !(hData->hStatus & HOST_STAT_REMOTE)) {
        hosts[numHosts] = hData->host;
        numHosts ++;
    }
    for (i=1; (hent = h_nextEnt_ (&stab)) != NULL; i++) {
        hData = ((struct hData *) hent->hData);
        if ((strcmp (hData->host, LOST_AND_FOUND) != 0) &&
            !(hData->hStatus & HOST_STAT_REMOTE)) {
            hosts[numHosts] = hData->host;
            numHosts ++;
        }
    }
    *hostNames = hosts;
    return (numHosts);

}  

void
getLsbHostInfo (void)
{
    static char fname[] = "getLsbHostInfo";
    struct qData *qp;
    int i;

    if (logclass & (LC_TRACE | LC_SCHED))
        ls_syslog(LOG_DEBUG3, "%s: Entering this routine...", fname);

    
    getLsfHostInfo(FALSE);
    
    numofprocs = 0;
    for (i = 0; i < numLsfHosts; i++) {
	struct hData *hD;
        bool_t isChanged = FALSE;
	if (lsfHostInfo[i].isServer != TRUE)
	    continue;

	if ((hD = getHostData (lsfHostInfo[i].hostName)) == NULL) {
	    
            if (logclass & LC_TRACE)
                ls_syslog (LOG_DEBUG2, "%s: Host <%s> not found", fname,
                                                   lsfHostInfo[i].hostName);
	    continue;
	}

        isChanged = (hD->cpuFactor != lsfHostInfo[i].cpuFactor ||
	    
	    (hD->numCPUs != lsfHostInfo[i].maxCpus && 
	     hD->numCPUs != 0 && lsfHostInfo[i].maxCpus != 0) ||
	    hD->maxMem != lsfHostInfo[i].maxMem ||
	    hD->maxSwap != lsfHostInfo[i].maxSwap ||
	    hD->maxTmp  != lsfHostInfo[i].maxTmp ||
	    strcmp(hD->hostType, lsfHostInfo[i].hostType) != 0 ||
	    strcmp(hD->hostModel, lsfHostInfo[i].hostModel) != 0);
	hD->cpuFactor = lsfHostInfo[i].cpuFactor;
        if (lsfHostInfo[i].maxCpus <= 0) {
            hD->numCPUs = 1;
        } else
            hD->numCPUs = lsfHostInfo[i].maxCpus;

       
       if (hD->flags & HOST_AUTOCONF_MXJ) {

           hD->maxJobs = hD->numCPUs;
       }

        

       if (daemonParams[LSB_VIRTUAL_SLOT].paramValue) {
           if (!strcasecmp("y", daemonParams[LSB_VIRTUAL_SLOT].paramValue)) {
              if (hD->maxJobs > 0
                  && hD->maxJobs < INFINIT_INT) {
                  hD->numCPUs = hD->maxJobs;
              }
           }
       }
	numofprocs += hD->numCPUs;
	FREEUP (hD->hostType);
	hD->hostType = safeSave (lsfHostInfo[i].hostType);
        FREEUP (hD->hostModel);
        hD->hostModel = safeSave (lsfHostInfo[i].hostModel);
        hD->maxMem    = lsfHostInfo[i].maxMem;
	if (hD->leftRusageMem == INFINIT_LOAD && hD->maxMem != 0) {
	    
	     hD->leftRusageMem = hD->maxMem;
	     if (logclass & LC_TRACE)
	         ls_syslog(LOG_DEBUG, "%s: update host %s lsftRusageMem <%f>",
	                   fname, hD->host, hD->leftRusageMem);
	}

        hD->maxSwap    = lsfHostInfo[i].maxSwap;
        hD->maxTmp    = lsfHostInfo[i].maxTmp;
        hD->nDisks    = lsfHostInfo[i].nDisks;
        FREEUP (hD->resBitMaps);
        hD->resBitMaps  = getResMaps(lsfHostInfo[i].nRes, lsfHostInfo[i].resources);

	if (isChanged && hD->maxMem > 0 && hD->maxMem < INFINIT_INT &&
	    hD->maxSwap > 0 && hD->maxSwap < INFINIT_INT &&
	    hD->maxTmp > 0 && hD->maxTmp < INFINIT_INT) {
	    if ( logclass & LC_TRACE) {
    	        ls_syslog(LOG_DEBUG2, "%s: host <%s> ncpus <%d> maxmem <%d> maxswp <%u> maxtmp <%u> ndisk <%d>",
		    fname, lsfHostInfo[i].hostName, lsfHostInfo[i].maxCpus,
                    lsfHostInfo[i].maxMem, lsfHostInfo[i].maxSwap,
                    lsfHostInfo[i].maxTmp, lsfHostInfo[i].nDisks);
	     }
	}
    } 

    
    i = TRUE;
    for (qp = qDataList->forw; (qp != qDataList); qp = qp->forw)
        queueHostsPF (qp, &i);
    
    if (logclass & LC_TRACE)
        ls_syslog(LOG_DEBUG2, "%s: Leaving this routine...", fname);
    
} 

int
getLsbHostLoad (void)
{
    static char fname[] = "getLsbHostLoad";
    static int errorcnt = 0;
    struct hostLoad *hosts;
    int i, numhosts = 0, num = 0, j;
    struct hData  *hD;
    struct jData *jpbw;
    char **hostNames;
   
    if (logclass & (LC_TRACE | LC_SCHED))
        ls_syslog(LOG_DEBUG1, "%s: Entering this routine...", fname);

    if ((numhosts = getLsbHostNames(&hostNames)) < 0) {
        ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "getLsbHostNames");
        mbdDie(MASTER_FATAL);
    }

    hosts = ls_loadofhosts ("-:server", &num, EFFECTIVE | LOCAL_ONLY,
			    NULL, hostNames, numhosts);
    
    if (hosts == NULL) {
        
        if (lserrno == LSE_LIM_DOWN) {
           ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 6006,
	       "%s: %s() failed, lim is down"), /* catgets 6006 */
	       fname,
	       "ls_loadofhosts");
           mbdDie(MASTER_FATAL);
        }
        
        errorcnt++;
        if (errorcnt > MAX_FAIL) {
            ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 6007,
		"%s: %s() failed for %d times: %M"), /* catgets 6007 */
		fname, 
		"ls_loadofhosts",
		errorcnt);
            mbdDie(MASTER_FATAL);
        }
        return (-1);
    }
    
    errorcnt = 0;

    if (num < numhosts)
        ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 6008,
	    "%s: LIM only returns %d hosts, but mbatchd has %d"), fname, num, numhosts); /* catgets 6008 */

    for (i = 0; i < num; i++) {
	if (logclass & LC_SCHED)
	    ls_syslog(LOG_DEBUG3, "%s: <%s> host status %x", fname,
		      hosts[i].hostName, hosts[i].status[0]);
	
        if ((hD = getHostData (hosts[i].hostName)) == NULL) {
            
            ls_syslog(LOG_ERR, I18N_FUNC_S_FAIL_M, 
		fname, "getHostData",
	        hosts[i].hostName);
            mbdDie(MASTER_FATAL);
        }
        
        if (!LS_ISUNAVAIL(hosts[i].status)) 
	     hD->hStatus &= ~HOST_STAT_NO_LIM;

        for (j = 0; j < allLsInfo->numIndx; j++) {
             hD->lsfLoad[j] = hosts[i].li[j];
             hD->lsbLoad[j] = hosts[i].li[j];
        }
	for (j = 0; j < 1 + GET_INTNUM (allLsInfo->numIndx); j++)
            hD->limStatus[j] = hosts[i].status[j];
        hD->flags |= HOST_UPDATE_LOAD;
    }

    for (jpbw = jDataList[SJL]->back;
	 (jpbw != jDataList[SJL]); jpbw = jpbw->back) {
         adjLsbLoad (jpbw, FALSE, TRUE);
    }

    setHostBLStatus();
    return (0);

} 

int
getHostsByResReq (struct resVal *resValPtr, int *num, 
    struct hData **hosts, struct hData ***thrown, struct hData *fromHost, int *overRideFromType)
{
    static char fname[]= "getHostsByResReq";
    struct hData **hData = NULL;     
    int i, numHosts, k = 0;
    struct tclHostData tclHostData;

    *overRideFromType = FALSE;

    INC_CNT(PROF_CNT_getHostsByResReq);

    if (hosts == NULL || *num == 0 || resValPtr == NULL) {
        *num = 0;
        return (*num);
    }
    if (hData == NULL)
        hData = (struct hData **) my_calloc(numofhosts, 
                                  sizeof(struct hData *), fname);
    numHosts = 0;
    for (i = 0, k = 0; i < *num; i++) {
	INC_CNT(PROF_CNT_loopgetHostsByResReq);
        if (hosts[i] == NULL)
            continue;
        if (strcmp (hosts[i]->host, LOST_AND_FOUND) == 0)
            continue;

        hData[k++] = hosts[i];     
        getTclHostData (&tclHostData, hosts[i], fromHost);
        if (evalResReq(resValPtr->selectStr, &tclHostData, DFT_FROMTYPE) != 1) {
	    freeTclHostData (&tclHostData);
            continue;
        }
        
        if (tclHostData.overRideFromType == TRUE)
            *overRideFromType = TRUE;

	freeTclHostData (&tclHostData);
        hosts[numHosts++] = hosts[i];
        k--;
    }

    *num = numHosts;
    if (thrown != NULL) {
        *thrown = hData;
    } else {
	FREEUP(hData);
    }
    return (*num);

} 

void
getTclHostData (struct tclHostData *tclHostData, struct hData *hPtr, struct hData *fromHost)
{
    static char fname[] = "getTclHostData";
    int i;

    tclHostData->hostName = hPtr->host;
    tclHostData->maxCpus = hPtr->numCPUs;
    tclHostData->maxMem = hPtr->maxMem;
    tclHostData->maxSwap = hPtr->maxSwap;
    tclHostData->maxTmp = hPtr->maxTmp;
    tclHostData->nDisks = hPtr->nDisks;

    tclHostData->hostInactivityCount = 0;

    tclHostData->status = hPtr->limStatus;
    tclHostData->loadIndex = 
            (float *) my_malloc (allLsInfo->numIndx * sizeof(float), fname);
    tclHostData->loadIndex[R15S] = (hPtr->cpuFactor != 0.0)?
	     ((hPtr->lsbLoad[R15S] +1.0)/hPtr->cpuFactor):hPtr->lsbLoad[R15S];
    tclHostData->loadIndex[R1M] = (hPtr->cpuFactor != 0.0)?
	     ((hPtr->lsbLoad[R1M] +1.0)/hPtr->cpuFactor):hPtr->lsbLoad[R1M];
    tclHostData->loadIndex[R15M] = (hPtr->cpuFactor != 0.0)?
	     ((hPtr->lsbLoad[R15M] +1.0)/hPtr->cpuFactor):hPtr->lsbLoad[R15M];
    for (i = 3; i < allLsInfo->numIndx; i++)
        tclHostData->loadIndex[i] = hPtr->lsbLoad[i];
    tclHostData->rexPriority = 0;
    tclHostData->hostType = hPtr->hostType;
    tclHostData->hostModel = hPtr->hostModel;
    if (fromHost != NULL) {
        tclHostData->fromHostType = fromHost->hostType;
        tclHostData->fromHostModel = fromHost->hostModel;
    } else {
        tclHostData->fromHostType = hPtr->hostType;
        tclHostData->fromHostModel = hPtr->hostModel;
    } 
        
    tclHostData->cpuFactor = hPtr->cpuFactor;
    tclHostData->ignDedicatedResource = FALSE;
    tclHostData->resBitMaps = hPtr->resBitMaps;
    tclHostData->DResBitMaps = NULL;
    tclHostData->numResPairs = hPtr->numInstances;
    tclHostData->resPairs = getResPairs(hPtr);
    tclHostData->flag = TCL_CHECK_EXPRESSION;
} 

static struct resPair *
getResPairs (struct hData *hPtr)
{
    int i;
    struct resPair *resPairs = NULL;

    if (hPtr->numInstances <= 0)
	return (NULL);
    resPairs = (struct resPair *) my_malloc 
            (hPtr->numInstances * sizeof (struct resPair), "getResPairs");

    for (i = 0; i <  hPtr->numInstances; i++) {
        resPairs[i].name = hPtr->instances[i]->resName;
        resPairs[i].value = hPtr->instances[i]->value;
    } 
    return (resPairs);

} 

time_t 
runTimeSinceResume(const struct jData *jp)
{
    if ( (jp->jStatus & JOB_STAT_RUN) && (jp->resumeTime >= 0) ) {
	return (now - jp->resumeTime);
    } else {
	return jp->runTime;
    }
} 

void
adjLsbLoad (struct jData *jpbw, int forResume, bool_t doAdj)
{
    static char fname[] = "adjLsbLoad";
    int i, ldx, resAssign = 0;
    float jackValue, orgnalLoad, duration, decay;
    struct  resVal *resValPtr;
    struct resourceInstance *instance;
    static int *rusgBitMaps = NULL;
    int adjForPreemptableResource = FALSE;

    if (logclass & LC_TRACE)
        ls_syslog(LOG_DEBUG, "%s: Entering this routine...", fname);

    if (rusgBitMaps == NULL) {
        rusgBitMaps = (int *) my_malloc 
                       (GET_INTNUM(allLsInfo->nRes) * sizeof (int), fname);
    }
	
    if (!jpbw->numHostPtr || jpbw->hPtr == NULL)
	return;

    if ((resValPtr 
	 = getReserveValues (jpbw->shared->resValPtr, jpbw->qPtr->resValPtr)) == NULL)
        return;
   

    for (i = 0; i < GET_INTNUM(allLsInfo->nRes); i++) {
         resAssign += resValPtr->rusgBitMaps[i];
         rusgBitMaps[i] = 0;
    }
    if (resAssign == 0)   
        return;      

    duration = (float) resValPtr->duration;
    decay = resValPtr->decay;
    if (resValPtr->duration != INFINIT_INT && (duration - jpbw->runTime <= 0)){ 
        
	if ((forResume != TRUE && (duration - runTimeSinceResume(jpbw) <= 0))
	    || !isReservePreemptResource(resValPtr)) {
            return;   
        }
        adjForPreemptableResource = TRUE;
    }
    for (i = 0; i < jpbw->numHostPtr; i++) {
	float load;
	char loadString[MAXLSFNAMELEN];

	if (jpbw->hPtr[i]->hStatus & HOST_STAT_UNAVAIL)
	    continue;   


        for (ldx = 0; ldx < allLsInfo->nRes; ldx++) {
            float factor;
	    int isSet;

	    if (NOT_NUMERIC(allLsInfo->resTable[ldx]))
		continue;

            TEST_BIT(ldx, resValPtr->rusgBitMaps, isSet);
	    if (isSet == 0)   
	        continue;

	    
	    if (adjForPreemptableResource && (!isItPreemptResourceIndex(ldx))) {
		continue;
            }

   	    
	    if (jpbw->jStatus & JOB_STAT_RUN) {

                goto adjustLoadValue;

	    } else if (IS_SUSP(jpbw->jStatus) 
		       && 
		       ! (allLsInfo->resTable[ldx].flags & RESF_RELEASE)
		       &&
		       forResume == FALSE) {

		goto adjustLoadValue;
		
		
	    } else if (IS_SUSP(jpbw->jStatus) 
		       && 
		       (jpbw->jStatus & JOB_STAT_RESERVE)) {

		goto adjustLoadValue;
		
		
	    } else if (IS_SUSP(jpbw->jStatus)
		       &&
		       forResume == TRUE
		       &&
		       (allLsInfo->resTable[ldx].flags & RESF_RELEASE)) {

		goto adjustLoadValue;
		
	    } else {

		

		continue;

	    }
		       
adjustLoadValue:
         
	    jackValue = resValPtr->val[ldx];
	    if (jackValue >= INFINIT_LOAD || jackValue <= -INFINIT_LOAD)
		continue;     

            if (ldx < allLsInfo->numIndx)
		load = jpbw->hPtr[i]->lsbLoad[ldx];
            else {
		load = getHRValue(allLsInfo->resTable[ldx].name,
				  jpbw->hPtr[i], &instance);
                if (load == -INFINIT_LOAD) {
                    if (logclass & LC_TRACE)
			ls_syslog (LOG_DEBUG3, "%s: Host <%s> doesn't share resource <%s>", fname, jpbw->hPtr[i]->host, allLsInfo->resTable[ldx].name);
                    continue;
                } else {
                    
                    TEST_BIT (ldx, rusgBitMaps, isSet)
                    if ((isSet == TRUE) && !slotResourceReserve) {
			
                        continue;
                    }
		    SET_BIT (ldx, rusgBitMaps);
                } 
            }


	    if (logclass & LC_SCHED)
		ls_syslog(LOG_DEBUG1,"\
%s: jobId=<%s>, hostName=<%s>, resource name=<%s>, the specified rusage of the load or instance <%f>, current lsbload or instance value <%f>, duration <%f>, decay <%f>",
			  fname,
			  lsb_jobid2str(jpbw->jobId),
			  jpbw->hPtr[i]->host,
			  allLsInfo->resTable[ldx].name,
			  jackValue,
			  load,
			  duration,
			  decay);

	    
            factor = 1.0;
	    if (resValPtr->duration != INFINIT_INT) {
	        if (resValPtr->decay != INFINIT_FLOAT) { 
		    float du;
		    
		    if ( isItPreemptResourceIndex(ldx) ) {
			if (forResume) {
			    
			    du = duration;
			} else {
			    du = duration - runTimeSinceResume(jpbw);
			}
		    } else {
			du = duration - jpbw->runTime;
		    }
		    if (du > 0) {
			factor = du/duration;
			factor = pow (factor, decay);
		    }
		}
		jackValue *= factor;
            }
            if (ldx == MEM && jpbw->runRusage.mem > 0) { 
		
		jackValue = jackValue - ((float) jpbw->runRusage.mem)* 0.001;
	    } else if (ldx == SWP && jpbw->runRusage.swap > 0) { 
		
		jackValue = jackValue - ((float) jpbw->runRusage.swap)* 0.001;
            }
	    if ((ldx == MEM || ldx == SWP) && jackValue < 0.0) {
		jackValue = 0.0;
	    }

	    if (!doAdj) {
		continue;
	    }

	    if ((ldx == MEM || ldx == SWP) && jackValue == 0.0) {
		
		continue;
	    }
	    if (allLsInfo->resTable[ldx].orderType == DECR)
	        jackValue = -jackValue;
             
            if (ldx < allLsInfo->numIndx) {
    	        orgnalLoad = jpbw->hPtr[i]->lsbLoad[ldx];
	        jpbw->hPtr[i]->lsbLoad[ldx] += jackValue;
	        if (jpbw->hPtr[i]->lsbLoad[ldx] <= 0.0
			 && forResume == FALSE)
	            jpbw->hPtr[i]->lsbLoad[ldx] = 0.0;
                if (ldx == UT && jpbw->hPtr[i]->lsbLoad[ldx] > 1.0
				       && forResume == FALSE)
                    jpbw->hPtr[i]->lsbLoad[ldx] = 1.0;
                load = jpbw->hPtr[i]->lsbLoad[ldx];
            } else {
	        orgnalLoad = atof (instance->value);
		load = orgnalLoad + jackValue;
		if (load < 0.0 && forResume == FALSE)
		    load = 0.0;
                FREEUP (instance->value);
		sprintf (loadString, "%-10.1f", load);
		instance->value = safeSave (loadString);
            }

	    if (logclass & LC_SCHED)
		ls_syslog(LOG_DEBUG1,"\
%s: JobId=<%s>, hostname=<%s>, resource name=<%s>, the amount by which the load or the instance has been adjusted <%f>, original load or instance value <%f>, runTime=<%d>, sinceresume=<%d>, value of the load or the instance after the adjustment <%f>, factor <%f>",
			  fname,
			  lsb_jobid2str(jpbw->jobId),
			  jpbw->hPtr[i]->host,
			  allLsInfo->resTable[ldx].name,
			  jackValue,
			  orgnalLoad,
			  jpbw->runTime, 
			  runTimeSinceResume(jpbw),
			  load,
			  factor);
        }
    }
} 

struct resVal * 
getReserveValues (struct resVal *jobResVal, struct resVal *qResVal)
{
    static char fname[]= "getReserveValues";
    static int first = TRUE;
    static struct resVal resVal; 
    int i, diffrent = FALSE;

    if (jobResVal == NULL && qResVal == NULL)
	return (NULL);

    if (jobResVal == NULL && qResVal != NULL) {
	
        if (hasResReserve (qResVal) == TRUE)
	    return (qResVal);
        else
	    return (NULL);
    }
    if (jobResVal != NULL && qResVal == NULL) {
        
        if (hasResReserve (jobResVal) == TRUE)
	    return (jobResVal);
        else            
            return (NULL);
    }
    
    for (i = 0; i < GET_INTNUM(allLsInfo->nRes); i++) {
	if (jobResVal->rusgBitMaps[i] == qResVal->rusgBitMaps[i])
	    continue;
        diffrent = TRUE;
    }
    if (diffrent == FALSE)
	return (jobResVal);

    if (first == TRUE) {
        resVal.val = (float *) my_malloc(allLsInfo->nRes * sizeof(float), fname);
	resVal.rusgBitMaps = (int *)
	      my_malloc (GET_INTNUM(allLsInfo->nRes) * sizeof (int), fname);
        first = FALSE;
    }

    
    for (i = 0; i < allLsInfo->nRes; i++)
	resVal.val[i] = INFINIT_FLOAT;
    for (i = 0; i < GET_INTNUM(allLsInfo->nRes); i++)
	resVal.rusgBitMaps[i] = 0;
    resVal.duration = INFINIT_INT;
    resVal.decay = INFINIT_FLOAT;

    for (i = 0; i < allLsInfo->nRes; i++) {
        int jobSet, queueSet;

        if (NOT_NUMERIC(allLsInfo->resTable[i]))
	    continue;

        TEST_BIT(i, jobResVal->rusgBitMaps, jobSet);
        TEST_BIT(i, qResVal->rusgBitMaps, queueSet);
	if (jobSet == 0 && queueSet == 0)   
	    
	    continue;
        else {
	    SET_BIT (i, resVal.rusgBitMaps);
	    if (jobSet == 0 && queueSet != 0) {
	        resVal.val[i] = qResVal->val[i];
	        continue;
            } else if (jobSet != 0 && queueSet == 0) {
		resVal.val[i] = jobResVal->val[i];
	        continue;
            } else if (jobSet != 0 && queueSet != 0) {
		resVal.val[i] = jobResVal->val[i];
	        continue;
	    }   
        }
    }
    
    if (jobResVal->duration < qResVal->duration)
	resVal.duration = jobResVal->duration;
    else
	resVal.duration = qResVal->duration;

    if (qResVal->decay != INFINIT_FLOAT && jobResVal->decay != INFINIT_FLOAT) {
	if (qResVal->decay < jobResVal->decay)
	    resVal.decay = jobResVal->decay;
	else
	    resVal.decay = qResVal->decay;
    } else if (qResVal->decay == INFINIT_FLOAT 
		    && jobResVal->decay != INFINIT_FLOAT)
            resVal.decay = jobResVal->decay;
    else if (qResVal->decay != INFINIT_FLOAT 
		    && jobResVal->decay == INFINIT_FLOAT)
            resVal.decay = qResVal->decay;
    return (&resVal);

} 

static void 
setHostBLStatus (void)
{
    int k, i;
    sTab hashSearchPtr;  
    hEnt *hashEntryPtr;
    struct hData *hData;

    hashEntryPtr = h_firstEnt_(&hDataList, &hashSearchPtr);
    while (hashEntryPtr) {
        int oldStatus;

        hData = (struct hData *) hashEntryPtr->hData;
        oldStatus = hData->hStatus;

        hashEntryPtr = h_nextEnt_(&hashSearchPtr);
        if (strcmp (hData->host, LOST_AND_FOUND) == 0 
	     || hData->limStatus == NULL || hData->lsbLoad == NULL)
            continue;

	hData->hStatus &= ~HOST_STAT_BUSY;
	hData->hStatus &= ~HOST_STAT_LOCKED;
	hData->hStatus &= ~HOST_STAT_LOCKED_MASTER;

	for (i = 0; i < GET_INTNUM (allLsInfo->numIndx); i++) {
	    hData->busyStop[i] = 0;
	    hData->busySched[i] = 0;
        }	
	if (LS_ISLOCKEDU(hData->limStatus))
            hData->hStatus |= HOST_STAT_LOCKED;

	if (LS_ISLOCKEDM(hData->limStatus)) {
            hData->hStatus |= HOST_STAT_LOCKED_MASTER;
	}
	for (k = 0; k < allLsInfo->numIndx; k++) {
            if (hData->lsbLoad[k] >= INFINIT_LOAD 
                || hData->lsbLoad[k] <= -INFINIT_LOAD)
                continue;                    
            if (allLsInfo->resTable[k].orderType == INCR) {
                if (hData->lsfLoad[k] >= hData->loadStop[k]) {
                    hData->hStatus |= HOST_STAT_BUSY;
                    
                    SET_BIT (k, hData->busyStop);
                }
                if (hData->lsbLoad[k] >= hData->loadSched[k]){
                    hData->hStatus |= HOST_STAT_BUSY;
		    SET_BIT (k, hData->busySched);
                }
            } else {
                if (hData->lsfLoad[k] <= hData->loadStop[k]) {
                    hData->hStatus |= HOST_STAT_BUSY;
		    SET_BIT (k, hData->busyStop);
                }
                if (hData->lsbLoad[k]<= hData->loadSched[k]){
                    hData->hStatus |= HOST_STAT_BUSY;
		    SET_BIT (k, hData->busySched);
                }
            }
	} 
    }                     

} 

void
getLsfHostInfo(int retry)
{
#define UPDATE_INTERVAL (10*60) 
    static char fname[] = "getLsfHostInfo";
    struct hostInfo *hostList;
    int i, numHosts;


    TIMEIT(0, hostList = ls_gethostinfo("-", &numHosts, NULL, 0, LOCAL_ONLY),
                                                             fname);

    for (i = 0; i < 3 && hostList == NULL 
                    && lserrno == LSE_TIME_OUT && retry == TRUE; i++) {
        millisleep_(6000);
        TIMEIT(0, hostList = ls_gethostinfo("-", &numHosts, NULL, 0, 
                                                    LOCAL_ONLY), fname);
    }
    if (hostList == NULL) {
	ls_syslog(LOG_ERR, I18N_FUNC_FAIL_MM, fname, "ls_gethostinfo");
	return;     
    }
    
    if (lsfHostInfo != NULL) {
	freeLsfHostInfo (lsfHostInfo, numLsfHosts);
       numLsfHosts = 0; 
       FREEUP(lsfHostInfo);
    } 

    
    lsfHostInfo = (struct hostInfo *) my_malloc 
		(numHosts * sizeof (struct hostInfo), fname);
    for (i = 0; i < numHosts; i++) {
	copyLsfHostInfo (&lsfHostInfo[i], &hostList[i]);
    }

    numLsfHosts = numHosts;

}  

struct hostInfo *
getLsfHostData (char *host)
{
    int i;

    if (lsfHostInfo == NULL || numLsfHosts <= 0 || host == NULL)
       return (NULL);

    for (i = 0; i < numLsfHosts; i++) {
	if (equalHost_(host, lsfHostInfo[i].hostName)) 
	    return (&lsfHostInfo[i]);
    }
    return (NULL);
} 

static int 
hasResReserve (struct resVal *resVal)
{
    int i;

    if (resVal == NULL)
        return (FALSE);

    for (i = 0; i < GET_INTNUM(allLsInfo->nRes); i++) {
        if (resVal->rusgBitMaps[i] != 0)
            return (TRUE);

    }
    return (FALSE);

} 

static void
getAHostInfo (struct hData *hp)
{
    static char fname[] = "getAHostInfo";
    struct hostInfo *hostInfo;
    struct qData *qp;
    int i, j, num = 1, oldMaxCpus;


    if (logclass & (LC_TRACE | LC_SCHED))
        ls_syslog(LOG_DEBUG3, "%s: Entering this routine...", fname);

    if (hp == NULL)   
        return;

    
    TIMEIT(0, hostInfo = ls_gethostinfo("-", &num, &hp->host, num, LOCAL_ONLY),
                                                             fname);

    for (i = 0; i < 3 && hostInfo == NULL
                    && lserrno == LSE_TIME_OUT; i++) {
        millisleep_(6000);
        TIMEIT(0, hostInfo = ls_gethostinfo("-", &num, &hp->host, num,
                                                    LOCAL_ONLY), fname);
    }
    if (hostInfo == NULL) {
        
        ls_syslog(LOG_ERR, I18N_FUNC_FAIL_MM, fname, "ls_gethostinfo");
        return;
    } 
    if (hostInfo[0].isServer != TRUE)
        return;	

    hp->cpuFactor = hostInfo[0].cpuFactor;
    oldMaxCpus = hp->numCPUs;
    if (hp->numCPUs != hostInfo[0].maxCpus) {
        if (hostInfo[0].maxCpus <= 0) {
            hp->numCPUs = 1;
        } else {
            hp->numCPUs = hostInfo[0].maxCpus;
        }
        ls_syslog(LOG_DEBUG3, "%s: host <%s> CPU=%d", fname, hp->host, hp->numCPUs);
    }


    
    if (hp->flags & HOST_AUTOCONF_MXJ) {
	hp->maxJobs = hp->numCPUs;
    }

    

    if (daemonParams[LSB_VIRTUAL_SLOT].paramValue) {
       if (!strcasecmp("y", daemonParams[LSB_VIRTUAL_SLOT].paramValue)) {
         if (hp->maxJobs > 0 && hp->maxJobs < INFINIT_INT) {
             hp->numCPUs = hp->maxJobs;
         }
       }
    }
    numofprocs += (hp->numCPUs - oldMaxCpus);
    FREEUP (hp->hostType);
    hp->hostType = safeSave (hostInfo[0].hostType);
    FREEUP (hp->hostModel);
    hp->hostModel = safeSave (hostInfo[0].hostModel);
    hp->maxMem    = hostInfo[0].maxMem;
    hp->maxSwap    = hostInfo[0].maxSwap;
    hp->maxTmp    = hostInfo[0].maxTmp;
    hp->nDisks    = hostInfo[0].nDisks;
    FREEUP (hp->resBitMaps);
    hp->resBitMaps  = getResMaps(hostInfo[0].nRes, hostInfo[0].resources);

    
    for (i = 0; i < numLsfHosts; i++) {
        if (strcmp (lsfHostInfo[i].hostName, hostInfo[0].hostName) != 0)
            continue;
        FREEUP (lsfHostInfo[i].hostType);
        FREEUP (lsfHostInfo[i].hostModel);
	if (lsfHostInfo[i].resources != NULL) {
	    for (j = 0; j < lsfHostInfo[i].nRes; j++)  {
	 	FREEUP (lsfHostInfo[i].resources[j]);
	    }
            FREEUP (lsfHostInfo[i].resources);
	}
        copyLsfHostInfo (&lsfHostInfo[i], hostInfo);
        break;
    }
        
    
    i = TRUE;
    for (qp = qDataList->forw; (qp != qDataList); qp = qp->forw)
        queueHostsPF (qp, &i);
    
    if (logclass & LC_TRACE)
        ls_syslog(LOG_DEBUG2, "%s: Leaving this routine...", fname);
    
} 

