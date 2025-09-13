/* $Id: sbd.xdr.c,v 1.5 2007/08/15 22:18:46 tmizan Exp $
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

#include <string.h>
#include <stdlib.h>
#include "sbd.h"
#include "../../lsf/lib/lib.xdr.h"

#define NL_SETN		11	

bool_t 
xdr_jobSetup (XDR *xdrs, struct jobSetup *jsetup, struct LSFHeader *hdr)
{
    static char fname[]="xdr_jobSetup";
    char *sp1, *sp2, *sp3;
    int jobArrId, jobArrElemId;
    if (xdrs->x_op == XDR_ENCODE) {
	jobId64To32(jsetup->jobId, &jobArrId, &jobArrElemId);
    }    
    if (!(xdr_int(xdrs, &jobArrId) &&
	  xdr_int(xdrs, &jsetup->jStatus) &&
	  xdr_float(xdrs, &jsetup->cpuTime) &&
	  xdr_int(xdrs, &jsetup->w_status) &&
	  xdr_int(xdrs, &jsetup->reason) &&
	  xdr_int(xdrs, &jsetup->jobPid) &&
	  xdr_int(xdrs, &jsetup->jobPGid) &&
	  xdr_int(xdrs, &jsetup->execGid) &&	  
	  xdr_int(xdrs, &jsetup->execUid) &&
	  xdr_int(xdrs, &jsetup->execJobFlag))) {	
	ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "xdr_int");
	return (FALSE);
    }

    if (!xdr_arrayElement(xdrs, (char *) &jsetup->lsfRusage, hdr,
			   xdr_lsfRusage))
	return (FALSE);

    sp1 = jsetup->execUsername;
    sp2 = jsetup->execHome;
    sp3 = jsetup->execCwd;

    if (xdrs->x_op == XDR_DECODE) {
	sp1[0] = '\0';
	sp2[0] = '\0';
	sp3[0] = '\0';
    }

    if (!(xdr_string(xdrs, &sp1, MAX_LSB_NAME_LEN) &&
	  xdr_string(xdrs, &sp2, MAXFILENAMELEN) &&
	  xdr_string(xdrs, &sp3, MAXFILENAMELEN))) {
	ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "xdr_string");
	return (FALSE);
    }
    if (!xdr_int(xdrs, &jobArrElemId)) {
        return (FALSE);
    }

    if (xdrs->x_op == XDR_DECODE) {
	jobId32To64(&jsetup->jobId,jobArrId,jobArrElemId);
    }      

    return (TRUE);
} 


bool_t 
xdr_jobSyslog (XDR *xdrs, struct jobSyslog *slog, struct LSFHeader *hdr)
{
    static char fname[]="xdr_jobSyslog";
    char *sp1;
    
    if (!xdr_int(xdrs, &slog->logLevel)) {
	ls_syslog(LOG_ERR, I18N_FUNC_S_FAIL_M, fname, "xdr_int", "loglevel");
	return (FALSE);
    }

    sp1 = slog->msg;

    if (xdrs->x_op == XDR_DECODE) {
	sp1[0] = '\0';
    }

    if (!xdr_string(xdrs, &sp1, MAXLINELEN)) {
	ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "xdr_string");
	return (FALSE);
    }

    return (TRUE);
} 

bool_t
xdr_jobCard (XDR *xdrs, struct jobCard *jCard, struct LSFHeader *hdr)
{
    static char fname[]="xdr_jobCard";
    char *sp1;


    if (xdrs->x_op == XDR_FREE) {
        FREEUP(jCard->actCmd);
        FREEUP(jCard->exitFile);
        if (!xdr_jobSpecs(xdrs, &jCard->jobSpecs, hdr))
            return(FALSE);
        return(TRUE);
    }

    sp1 = jCard->execUsername;
    if (!xdr_int(xdrs, &jCard->execGid)  ||
        !xdr_int(xdrs, &jCard->notReported) ||
        !xdr_time_t(xdrs, &jCard->windEdge) ||
        !xdr_char(xdrs, &jCard->active) ||
        !xdr_char(xdrs, &jCard->timeExpire) ||
        !xdr_char(xdrs, &jCard->missing) ||
        !xdr_char(xdrs, &jCard->mbdRestarted) ||
        !xdr_time_t(xdrs, &jCard->windWarnTime) ||
        !xdr_int(xdrs, &jCard->runTime) ||
        !xdr_int(xdrs, &jCard->w_status) ||
        !xdr_float(xdrs, &jCard->cpuTime) ||
        !xdr_time_t(xdrs, &jCard->lastChkpntTime) ||
        !xdr_int(xdrs, &jCard->migCnt) ||
        !xdr_int(xdrs, &jCard->cleanupPid) ||
        !xdr_int(xdrs, &jCard->execJobFlag)){
        ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "xdr");
        return(FALSE);
    }

    if (! xdr_time_t(xdrs, &jCard->lastStatusMbdTime)) {
	return(FALSE);
    }
    
   if (xdrs->x_op == XDR_DECODE) {
      sp1[0] = '\0';
   }

   if (!xdr_string(xdrs, &sp1, MAX_LSB_NAME_LEN)) {
        ls_syslog(LOG_ERR, I18N_FUNC_S_FAIL_M, fname, 
	    "xdr_string", "execUserName");
        return(FALSE);
   }
   
   if (!xdr_int(xdrs, &jCard->actReasons) ||
       !xdr_int(xdrs, &jCard->actSubReasons)) {
       ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "xdr");
       return(FALSE);
   }

   if (!xdr_var_string(xdrs, &jCard->actCmd) ||
       !xdr_var_string(xdrs, &jCard->exitFile) ||
       !xdr_var_string(xdrs, &jCard->clusterName))
       return(FALSE);

   if (!xdr_arrayElement(xdrs, (char *) &jCard->jobSpecs, hdr,
			   xdr_jobSpecs)) {
       ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "xdr_jobSpecs");
       return(FALSE);
   }
  
   if (!xdr_int(xdrs, &jCard->servSocket))
       return(FALSE);

   if (!xdr_int(xdrs, &jCard->maxRusage.mem) ||
       !xdr_int(xdrs, &jCard->maxRusage.swap)||
       !xdr_int(xdrs, &jCard->maxRusage.utime)||   
       !xdr_int(xdrs, &jCard->maxRusage.stime) ||
       !xdr_int(xdrs, &jCard->maxRusage.npids)   )  
   {
       ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "xdr_int jRusage");
       return(FALSE);
   }
    
    if (!xdr_int(xdrs, (int *)&jCard->actFlags)) {
    	ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "xdr_actFlags");
        return(FALSE);
    }

   return(TRUE);
} 

int
sizeofJobCard(struct jobCard *jc) {

    int i, totalSize;

    totalSize = sizeof(struct jobSpecs) + sizeof(struct sbdPackage) + 100
                + jc->jobSpecs.numToHosts * MAXHOSTNAMELEN
                + jc->jobSpecs.thresholds.nThresholds
                  * jc->jobSpecs.thresholds.nIdx * 2 * sizeof (float)
                + jc->jobSpecs.nxf * sizeof (struct xFile)
                + jc->jobSpecs.eexec.len;
    for (i = 0; i < jc->jobSpecs.numEnv; i++) {
        totalSize += strlen(jc->jobSpecs.env[i]);
    }

    if (jc->actCmd != NULL) {
        totalSize += strlen(jc->actCmd);
    } else {
	jc->actCmd = "";
    }
    if (jc->exitFile != NULL) {
	totalSize += strlen(jc->exitFile);
    } else {
	jc->exitFile = "";
    }
    totalSize += sizeof(jc->maxRusage);
    totalSize = (totalSize * 4) / 4;

    return totalSize;
}

