/* $Id: mbd.proxy.c,v 1.5 2007/08/15 22:18:46 tmizan Exp $
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

#include "mbd.h"

#define NL_SETN		10	

LIST_T *     pxyRsvJL = NULL;



static int
proxyUSJLEnter(LIST_T *list, void *extra, LIST_EVENT_T *event)
{
    static char             fname[] = "proxyUSJLEnter";
    PROXY_LIST_ENTRY_T      *pxyPred;
    PROXY_LIST_ENTRY_T      *newPxy;
    struct jData            *theJob;
    struct uData            *uPtr;

    if (logclass & LC_TRACE)
	ls_syslog(LOG_DEBUG, "%s: Entering this routine...", fname);

    
    theJob = (struct jData *) event->entry;
    if (theJob == NULL) {
	ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 7400,
	    "%s: expected a non-NULL job pointer but got a NULL one"), /* catgets 7400 */
	    fname);
	return (-1);
    }

    uPtr = theJob->uPtr;
    if (uPtr->pxySJL == NULL) {
	char strBuf[512];
	sprintf(strBuf, "User %s's Started Job Proxy List", uPtr->user);
	uPtr->pxySJL = listCreate(strBuf);
    }

    
    pxyPred = (PROXY_LIST_ENTRY_T *)listSearchEntry(uPtr->pxySJL, 
						    (void *)theJob, 
						    (LIST_ENTRY_EQUALITY_OP_T)
						    startJobPrioEqual,
						    0);
    
    newPxy = proxyListEntryCreate((void *)theJob);
    
    if (pxyPred == NULL) 
	listInsertEntryAtBack(uPtr->pxySJL, (LIST_ENTRY_T *) newPxy);
    else
	listInsertEntryAfter(uPtr->pxySJL, 
			     (LIST_ENTRY_T *)pxyPred,
			     (LIST_ENTRY_T *)newPxy);
    return(0);
			      
} 

static int
proxyUSJLLeave(LIST_T *list, void *extra, LIST_EVENT_T *event)
{
    static char             fname[] = "proxyUSJLLeave";
    PROXY_LIST_ENTRY_T      *pxy;
    struct jData            *theJob;
    struct uData            *uPtr;

    if (logclass & LC_TRACE)
	ls_syslog(LOG_DEBUG, "%s: Entering this routine...", fname);

    
    theJob = (struct jData *) event->entry;
    if (theJob == NULL) {
	ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 7400,
	    "%s: expected a non-NULL job pointer but got a NULL one"),
	    fname);
	return (-1);
    }

    uPtr = theJob->uPtr;

    pxy = (PROXY_LIST_ENTRY_T *) listSearchEntry(uPtr->pxySJL, 
						 (void *)theJob,
						 (LIST_ENTRY_EQUALITY_OP_T)
						 proxyListEntryEqual, 
						 0);
    if (pxy == NULL) {
	ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 7402,
	    "%s: cannot find the proxy for the job <%s>"), /* catgets 7402 */
	    fname, lsb_jobid2str(theJob->jobId));
	return (-1);
    }

    listRemoveEntry(uPtr->pxySJL, (LIST_ENTRY_T *) pxy);
    proxyListEntryDestroy(pxy);

    return (0);

} 

void
proxyUSJLAttachObsvr()
{
    static char          fname[] = "proxyUSJLAttachObsvr";
    LIST_OBSERVER_T      *observer;
    int                  rc;

    if (logclass & LC_TRACE)
	ls_syslog(LOG_DEBUG, "%s: Entering this routine...", fname);

    observer = listObserverCreate("User's Started Job Proxy List Observer",
				  NULL,
				  (LIST_ENTRY_SELECT_OP_T) NULL,
				  LIST_EVENT_ENTER, proxyUSJLEnter,
				  LIST_EVENT_LEAVE, proxyUSJLLeave,
				  LIST_EVENT_NULL);

    if (observer == NULL) {
	ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 7403,
	    "%s: failed to create an observer for user's started job proxy list: %s"), /* catgets 7403 */
	    fname, listStrError(listerrno));
	mbdDie(MASTER_MEM);
    }

    rc = listObserverAttach(observer, (LIST_T *)jDataList[SJL]);
    if (rc < 0) {
	ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 7404,
	    "%s: failed to attach observer to the started job list"), /* catgets 7404 */
	    fname);
	mbdDie(MASTER_MEM);
    }
	    
} 

void
proxyUSJLAddEntry(struct jData *job)
{
    static char       fname[] = "proxyHSJLAddEntry";
    LIST_EVENT_T      event;
    int               rc;

    if (logclass & LC_TRACE)
	ls_syslog(LOG_DEBUG, "%s: Entering this routine...", fname);

    memset((void *)&event, 0, sizeof(LIST_EVENT_T));
    event.type = LIST_EVENT_ENTER;
    event.entry = (LIST_ENTRY_T *) job;

    rc = proxyUSJLEnter((LIST_T *)jDataList[SJL], NULL, &event);
    if (rc < 0) 
	ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "proxyHSJLEnter");

} 



static int
proxyHSJLEnter(LIST_T *list, void *extra, LIST_EVENT_T *event)
{
    static char             fname[] = "proxyUSJLEnter";
    PROXY_LIST_ENTRY_T      *pxyPred;
    PROXY_LIST_ENTRY_T      *newPxy;
    struct jData            *theJob;

    if (logclass & LC_TRACE)
	ls_syslog(LOG_DEBUG, "%s: Entering this routine...", fname);

    
    theJob = (struct jData *) event->entry;
    if (theJob == NULL) {
	ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 7400,
	    "%s: expected a non-NULL job pointer but got a NULL one"),
	    fname);
	return (-1);
    }

    FOR_EACH_JOB_LOCAL_EXECHOST(hPtr, theJob) {
	if (hPtr->pxySJL == NULL) {
	    char strBuf[512];
	    sprintf(strBuf, "Host %s's Started Job Proxy List", hPtr->host);
	    hPtr->pxySJL = listCreate(strBuf);
	}

	pxyPred = (PROXY_LIST_ENTRY_T *)listSearchEntry(hPtr->pxySJL, 
						   (void *)theJob,
						   (LIST_ENTRY_EQUALITY_OP_T) 
						   startJobPrioEqual,
						   0);

	newPxy = proxyListEntryCreate((void *)theJob);
	
	if (pxyPred == NULL) 
	    listInsertEntryAtBack(hPtr->pxySJL, (LIST_ENTRY_T *) newPxy);
	else
	    listInsertEntryAfter(hPtr->pxySJL, 
				 (LIST_ENTRY_T *)pxyPred,
				 (LIST_ENTRY_T *)newPxy);

    } END_FOR_EACH_JOB_LOCAL_EXECHOST;

    return(0);
			      
} 

static int
proxyHSJLLeave(LIST_T *list, void *extra, LIST_EVENT_T *event)
{
    static char             fname[] = "proxyUSJLEnter";
    PROXY_LIST_ENTRY_T      *pxy;
    struct jData            *theJob;

    if (logclass & LC_TRACE)
	ls_syslog(LOG_DEBUG, "%s: Entering this routine...", fname);

    
    theJob = (struct jData *) event->entry;
    if (theJob == NULL) {
	ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 7400,
	    "%s: expected a non-NULL job pointer but got a NULL one"),
	    fname);
	return (-1);
    }

    FOR_EACH_JOB_LOCAL_EXECHOST(hPtr, theJob) {
	pxy = (PROXY_LIST_ENTRY_T *) listSearchEntry(hPtr->pxySJL, 
						     (void *)theJob,
						     (LIST_ENTRY_EQUALITY_OP_T)
						     proxyListEntryEqual, 
						     0);
	if (pxy == NULL) {
	    ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 7408,
		"%s: cannot find the proxy for the job <%s>"),/* catgets 7408 */
		fname, lsb_jobid2str(theJob->jobId));
	    return (-1);
	}

	listRemoveEntry(hPtr->pxySJL, (LIST_ENTRY_T *)pxy);
	proxyListEntryDestroy(pxy);

    } END_FOR_EACH_JOB_LOCAL_EXECHOST;

    return(0);

} 

void
proxyHSJLAttachObsvr()
{
    static char          fname[] = "proxyHSJLAttachObsvr";
    LIST_OBSERVER_T      *observer;
    int                  rc;

    if (logclass & LC_TRACE)
	ls_syslog(LOG_DEBUG, "%s: Entering this routine...", fname);

    observer = listObserverCreate("Host's Started Job Proxy List Observer",
				  NULL,
				  (LIST_ENTRY_SELECT_OP_T) NULL,
				  LIST_EVENT_ENTER, proxyHSJLEnter,
				  LIST_EVENT_LEAVE, proxyHSJLLeave,
				  LIST_EVENT_NULL);

    if (observer == NULL) {
	ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 7409,
	    "%s: failed to create an observer for user's started job proxy list: %s"), /* catgets 7409 */
	    fname, listStrError(listerrno));
	mbdDie(MASTER_MEM);
    }

    rc = listObserverAttach(observer, (LIST_T *)jDataList[SJL]);
    if (rc < 0) {
	ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 7410,
	    "%s: failed to attach observer to the started job list"), /* catgets 7410 */
	    fname);
	mbdDie(MASTER_FATAL);
    }
	    
} 

void
proxyHSJLAddEntry(struct jData *job)
{
    static char       fname[] = "proxyHSJLAddEntry";
    LIST_EVENT_T      event;
    int               rc;

    if (logclass & LC_TRACE)
	ls_syslog(LOG_DEBUG, "%s: Entering this routine...", fname);

    memset((void *)&event, 0, sizeof(LIST_EVENT_T));
    event.type = LIST_EVENT_ENTER;
    event.entry = (LIST_ENTRY_T *) job;

    rc = proxyHSJLEnter((LIST_T *)jDataList[SJL], NULL, &event);
    if (rc < 0) 
	ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "proxyHSJLEnter");

} 


void
proxyHRsvJLAddEntry(struct jData *job)
{
    static char            fname[] = "proxyHRsvJLAddEntry";
    PROXY_LIST_ENTRY_T     *pxy;

    if (logclass & LC_TRACE)
	ls_syslog(LOG_DEBUG, "%s: Entering this routine...", fname);

    if (JOB_RSVSLOT_NONPRMPT(job))
	return;

    FOR_EACH_JOB_LOCAL_EXECHOST(hPtr, job) {
	if (hPtr->pxyRsvJL == NULL) {
	    char strBuf[216];

	    sprintf(strBuf, "Host %s's Reserved Job Proxy List", hPtr->host);
	    hPtr->pxyRsvJL = listCreate(strBuf);
	    if (hPtr->pxyRsvJL == NULL) {
		ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 7412,
		    "%s: failed to create host %s's proxy list: %s"),  /* catgets 7412 */
		    fname, hPtr->host, listStrError(listerrno));
		mbdDie(MASTER_MEM);
	    }
	}

	
	pxy = (PROXY_LIST_ENTRY_T *)listSearchEntry(hPtr->pxyRsvJL, 
						    (LIST_ENTRY_T *)job,
						    (LIST_ENTRY_EQUALITY_OP_T)
						    proxyListEntryEqual,
						    0);
	if (pxy != NULL)
	    continue;

	pxy = proxyListEntryCreate((void *)job);
	listInsertEntryAtBack(hPtr->pxyRsvJL, (LIST_ENTRY_T *)pxy); 

    } END_FOR_EACH_JOB_LOCAL_EXECHOST;
    
} 

void
proxyHRsvJLRemoveEntry(struct jData *job)
{
    static char             fname[] = "proxyHRsvJLRemoveEntry";
    PROXY_LIST_ENTRY_T      *pxy;

    if (logclass & LC_TRACE)
	ls_syslog(LOG_DEBUG, "%s: Entering this routine...", fname);

    if (JOB_RSVSLOT_NONPRMPT(job))
	return;
    
    FOR_EACH_JOB_LOCAL_EXECHOST(hPtr, job) {
	
	if (hPtr->pxyRsvJL == NULL)
	    continue;

	pxy = (PROXY_LIST_ENTRY_T *)listSearchEntry(hPtr->pxyRsvJL, 
						    (LIST_ENTRY_T *)job,
						    (LIST_ENTRY_EQUALITY_OP_T)
						    proxyListEntryEqual,
						    0);

	if (pxy == NULL) {
            ls_syslog(LOG_DEBUG3, "\
%s: cannot find the proxy for the job <%s>",
		      fname, lsb_jobid2str(job->jobId));
	    
	    continue;
	}

	listRemoveEntry(hPtr->pxyRsvJL, (LIST_ENTRY_T *)pxy);
	proxyListEntryDestroy(pxy);

    } END_FOR_EACH_JOB_LOCAL_EXECHOST;

} 


void
proxyRsvJLAddEntry(struct jData *job)
{
    static char            fname[] = "proxyRsvJLAddEntry";
    PROXY_LIST_ENTRY_T     *pxyPred;
    PROXY_LIST_ENTRY_T     *newPxy;

    if (logclass & LC_TRACE)
	ls_syslog(LOG_DEBUG, "%s: Entering this routine...", fname);

    
    if (pxyRsvJL == NULL) {
	pxyRsvJL = listCreate("Proxy List for Slot-reserving Jobs");
	if (pxyRsvJL == NULL) {
	    ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 7413,
		"%s: failed to create the system wide proxy list for slot-reserving jobs: %s"),  /* catgets 7413 */
		fname, listStrError(listerrno));
	    mbdDie(MASTER_MEM);
	}
    }

    
    pxyPred = (PROXY_LIST_ENTRY_T *)listSearchEntry(pxyRsvJL, 
						    (void *)job, 
						    (LIST_ENTRY_EQUALITY_OP_T)
						    pendJobPrioEqual,
						    0);
 
    newPxy = proxyListEntryCreate((void *)job);
    if (pxyPred == NULL) 
	listInsertEntryAtBack(pxyRsvJL, (LIST_ENTRY_T *) newPxy);
    else
	listInsertEntryAfter(pxyRsvJL, 
			     (LIST_ENTRY_T *)pxyPred,
			     (LIST_ENTRY_T *)newPxy);

    if (logclass & (LC_TRACE)) {
	char strBuf[1024];

	listCat(pxyRsvJL, LIST_TRAVERSE_FORWARD, strBuf, 1024,
		(LIST_ENTRY_CAT_FUNC_T)jobProxySprintf, 
		(void *) 0); 
	ls_syslog(LOG_DEBUG, "%s: the current pxyRsvJL contents: <%s>", 
		  fname, strBuf);
   }

} 

void
proxyRsvJLRemoveEntry(struct jData *job)
{
    static char             fname[] = "proxyRsvJLRemoveEntry";
    PROXY_LIST_ENTRY_T      *pxy;

    if (logclass & LC_TRACE)
	ls_syslog(LOG_DEBUG, "%s: Entering this routine...", fname);

    pxy = (PROXY_LIST_ENTRY_T *)listSearchEntry(pxyRsvJL, 
						(LIST_ENTRY_T *)job,
						(LIST_ENTRY_EQUALITY_OP_T)
						proxyListEntryEqual,
						0);

    if (pxy == NULL) {
	ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 7414,
	    "%s: cannot find the proxy for the job <%s>"), /* catgets 7414 */
	    fname, lsb_jobid2str(job->jobId));
	return;
    }
    
    listRemoveEntry(pxyRsvJL, (LIST_ENTRY_T *)pxy);
    proxyListEntryDestroy(pxy);
    
    if (logclass & (LC_TRACE)) {
	char strBuf[1024];

	listCat(pxyRsvJL, LIST_TRAVERSE_FORWARD, strBuf, 1024,
		(LIST_ENTRY_CAT_FUNC_T)jobProxySprintf, 
		(void *) 0); 
	ls_syslog(LOG_DEBUG, "%s: the current pxyRsvJL contents: <%s>", 
		  fname, strBuf);
   }

} 



static LIST_T *     proxyEntFreeList = NULL;

 
#define PROXY_FREE_LIST_MAX_ENTRIES     64


PROXY_LIST_ENTRY_T *
proxyListEntryCreate(void *subject)
{
    static char            fname[] = "proxyListEntryCreate";
    PROXY_LIST_ENTRY_T     *pxy; 
    
    if (proxyEntFreeList == NULL) {
	proxyEntFreeList = listCreate("Proxy Entry Free List");

	if (! proxyEntFreeList) {
	    ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 7415,
		"%s: failed to create the proxy entry list: %s"), /* catgets 7415 */
		fname, listStrError(listerrno));
	    mbdDie(MASTER_MEM);
	}
    }

    pxy = (PROXY_LIST_ENTRY_T *) listGetFrontEntry(proxyEntFreeList);
    if (pxy == NULL) 
	pxy = (PROXY_LIST_ENTRY_T *)my_calloc(1, 
	  			         sizeof(PROXY_LIST_ENTRY_T), 
					 fname);
    else 
	listRemoveEntry(proxyEntFreeList, (LIST_ENTRY_T *)pxy);
			
    pxy->subject = subject;

    return (pxy);

} 


void
proxyListEntryDestroy(PROXY_LIST_ENTRY_T  *proxy)
{
    if (proxyEntFreeList->numEnts >= PROXY_FREE_LIST_MAX_ENTRIES)
	free(proxy);
    else {
	proxy->subject = NULL;
	listInsertEntryAtBack(proxyEntFreeList, (LIST_ENTRY_T *)proxy);
    }
	
} 


bool_t
proxyListEntryEqual(PROXY_LIST_ENTRY_T *pxy, void *subject, int hint)
{
    return (pxy == NULL || pxy->subject != subject) ? FALSE : TRUE;

} 


bool_t
pendJobPrioEqual(PROXY_LIST_ENTRY_T *pxy, struct jData *subjectJob, int hint)
{
    struct jData *jPtr;

    if (pxy == NULL)
	return (FALSE);
    
    jPtr = (struct jData *)pxy->subject;
    if (jPtr == NULL || subjectJob == NULL) 
	return (FALSE);
    
    if (subjectJob->qPtr->priority < jPtr->qPtr->priority)
	
	return (TRUE);
    else if (subjectJob->qPtr->priority == jPtr->qPtr->priority) {
	if (JOB_SUBMIT_TIME(subjectJob) > JOB_SUBMIT_TIME(jPtr))
	    
	    return(TRUE);
	else if (   (JOB_SUBMIT_TIME(subjectJob) == JOB_SUBMIT_TIME(jPtr)) 
		 && (subjectJob->jobId > jPtr->jobId))
	    
	    return (TRUE);
    }
    
    return (FALSE);

} 


bool_t
startJobPrioEqual(PROXY_LIST_ENTRY_T *pxy, struct jData *subjectJob, int hint)
{
    struct jData *jPtr;

    if (pxy == NULL)
	return (FALSE);
    
    jPtr = (struct jData *)pxy->subject;
    if (jPtr == NULL || subjectJob == NULL) 
	return (FALSE);
    
    if (subjectJob->qPtr->priority < jPtr->qPtr->priority)
	
	return (TRUE);
    else if (subjectJob->qPtr->priority == jPtr->qPtr->priority) {
	if (subjectJob->startTime > jPtr->startTime)
	    
	    return(TRUE);
	else if (   (   subjectJob->startTime == jPtr->startTime 
		     || subjectJob->startTime == 0)
		 && (subjectJob->jobId > jPtr->jobId))
	    
	    return (TRUE);
    }
    
    return (FALSE);

} 


void
jobProxyPrintf(PROXY_LIST_ENTRY_T *proxy, void *hint)
{
    struct jData *job = proxy->subject;
    printf("%s\n", lsb_jobidinstr(job->jobId));
} 

char *
jobProxySprintf(PROXY_LIST_ENTRY_T *proxy, void *hint)
{
    static char       strBuf[32];
    struct jData      *job = proxy->subject;

    sprintf(strBuf, "<%s>", lsb_jobidinstr(job->jobId));
    return (strBuf);
    
} 


struct jData *
jobProxyGetPendJob(PROXY_LIST_ENTRY_T *proxy)
{
    struct jData *job = (struct jData *)proxy->subject;

    if (IS_PEND(job->jStatus))
	return job;
    else
	return (struct jData *) NULL;

} 
