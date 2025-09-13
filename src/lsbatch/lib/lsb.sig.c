/* $Id: lsb.sig.c,v 1.3 2007/08/15 22:18:48 tmizan Exp $
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

#include <unistd.h>
#include <netdb.h>
#include <string.h>
#include <pwd.h>
#include "lsb.h"

static int signalJob (int sigValue, LS_LONG_INT jobId, time_t period, int options);

int
lsb_signaljob(LS_LONG_INT jobId, int sigValue)
{
    if (sigValue < 0 || sigValue >= LSF_NSIG) {
	lsberrno = LSBE_BAD_SIGNAL;
	return (-1);
    }
    return (signalJob(sigValue, jobId, 0, 0));
} 

int
lsb_chkpntjob(LS_LONG_INT jobId, time_t period, int options)
{
    int lsbOptions = 0;
    
    if (period < LSB_CHKPERIOD_NOCHNG) {
	lsberrno = LSBE_BAD_ARG;
	return (-1);
    }

    

    if (options & LSB_CHKPNT_KILL)
	lsbOptions |= LSB_CHKPNT_KILL;
    if (options & LSB_CHKPNT_FORCE)
	lsbOptions |= LSB_CHKPNT_FORCE;
    if (options & LSB_CHKPNT_STOP)
        lsbOptions |= LSB_CHKPNT_STOP;
   
    return (signalJob(SIG_CHKPNT, jobId, period, lsbOptions));
} 

int
lsb_deletejob(LS_LONG_INT jobId, int times, int options)
{
    if (times < 0) {
	lsberrno = LSBE_BAD_ARG;
	return (-1);
    }
    if (options & LSB_KILL_REQUEUE) {
	return (signalJob(SIG_KILL_REQUEUE, jobId, 0, 0));
    }
    return (signalJob(SIG_DELETE_JOB, jobId, times, options));

} 


int
lsb_forcekilljob(LS_LONG_INT jobId)
{
    return (signalJob(SIG_TERM_FORCE, jobId, 0, 0));

} 


static int 
signalJob (int sigValue, LS_LONG_INT jobId, time_t period, int options)
{
    struct signalReq signalReq;
    char request_buf[MSGSIZE];
    char *reply_buf;
    XDR xdrs;
    mbdReqType mbdReqtype;
    int cc;
    struct LSFHeader hdr;
    struct lsfAuth auth;

    signalReq.jobId = jobId;

    if (authTicketTokens_(&auth, NULL) == -1)
	return (-1);
    
    signalReq.sigValue = sigValue;
    signalReq.chkPeriod = period;
    signalReq.actFlags = options;

    signalReq.sigValue = sig_encode(signalReq.sigValue);

    
    
    mbdReqtype = BATCH_JOB_SIG;
    xdrmem_create(&xdrs, request_buf, MSGSIZE, XDR_ENCODE);

    initLSFHeader_(&hdr);
    hdr.opCode = mbdReqtype;
    if (!xdr_encodeMsg(&xdrs, (char *)&signalReq, &hdr, xdr_signalReq, 0,
		       &auth)) {
        lsberrno = LSBE_XDR;
        xdr_destroy(&xdrs);
        return(-1);
    }

    
    if ((cc = callmbd(NULL, request_buf, XDR_GETPOS(&xdrs), &reply_buf, 
                      &hdr, NULL, NULL, NULL)) < 0)
    {
        xdr_destroy(&xdrs);
	return (-1);
    }

    xdr_destroy(&xdrs);

    if (cc)
	free(reply_buf);
    
    lsberrno = hdr.opCode;
    if (lsberrno == LSBE_NO_ERROR || lsberrno == LSBE_JOB_DEP)
        return(0);
    else
	return(-1);
} 
int
lsb_requeuejob(struct jobrequeue *reqPtr)
{
    int              cc;

    
    if (reqPtr == NULL
       || (*reqPtr).jobId <= 0) {
       lsberrno = LSBE_BAD_ARG;
       return(-1);
    }
   
    
    if ((*reqPtr).status != JOB_STAT_PEND
       && (*reqPtr).status != JOB_STAT_PSUSP) {
       (*reqPtr).status = JOB_STAT_PEND;
    }

    
    if ((*reqPtr).options != REQUEUE_DONE
       && (*reqPtr).options != REQUEUE_EXIT
       && (*reqPtr).options != REQUEUE_RUN) {
       (*reqPtr).options
           |= (REQUEUE_DONE | REQUEUE_EXIT | REQUEUE_RUN);
    }

    
    cc = signalJob(SIG_ARRAY_REQUEUE,
                  (*reqPtr).jobId,
                  (*reqPtr).status,
                  (*reqPtr).options);
    if (cc == LSBE_NO_ERROR) {
       return(0);
    } else {
       return(-1);
    }

} 
