/* $Id: cmd.err.c,v 1.3 2007/08/15 22:18:44 tmizan Exp $
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

#include <pwd.h>
#include "cmd.h"
#include "../lib/lsb.h"

#include <netdb.h>
#include <ctype.h>

#define NL_SETN 8 	



void 
jobInfoErr (LS_LONG_INT jobId, char *jobName, char *user, char *queue, 
            char *host, int options)
{
    char errMsg[MAXLINELEN/2];
    char hostOrQueue[MAXLINELEN/2];

    if (user && lsberrno == LSBE_BAD_USER) {
	lsb_perror(user);
        return;
    }
    if (queue && lsberrno == LSBE_BAD_QUEUE) {
	lsb_perror(queue);
        return;
    }
    if (host && lsberrno == LSBE_BAD_HOST) {
	lsb_perror(host);
        return;
    }

    if (lsberrno == LSBE_NO_JOB) {
        hostOrQueue[0] = '\0';
        if (queue) {
	    strcpy (hostOrQueue, (_i18n_msg_get(ls_catd,NL_SETN,801, " in queue <"))); /* catgets  801  */
            strcat (hostOrQueue, queue);
        }
        if (host) {
            if (ls_isclustername(host) <= 0) { 
                if (hostOrQueue[0] == '\0')
	    	    strcpy(hostOrQueue, (_i18n_msg_get(ls_catd,NL_SETN,802, " on host/group <"))); /* catgets  802  */
                else
		    strcat(hostOrQueue, (_i18n_msg_get(ls_catd,NL_SETN,803, "> and on host/group <"))); /* catgets  803  */
            } else {                               
               if (hostOrQueue[0] == '\0')
                    strcpy(hostOrQueue, (_i18n_msg_get(ls_catd,NL_SETN,804, " in cluster <"))); /* catgets  804  */
                else
                    strcat(hostOrQueue, (_i18n_msg_get(ls_catd,NL_SETN,805, "> and in cluster <"))); /* catgets  805  */
            }
            strcat(hostOrQueue, host);
        }
        if (hostOrQueue[0] != '\0')
            strcat(hostOrQueue, ">");

        if (jobId) {
            if (options & JGRP_ARRAY_INFO) {
               if (LSB_ARRAY_IDX(jobId))
                   sprintf(errMsg, (_i18n_msg_get(ls_catd,NL_SETN,806, "Job <%s> is not a job array")),  /* catgets  806  */
                        lsb_jobid2str(jobId));
               else 
                   sprintf(errMsg, (_i18n_msg_get(ls_catd,NL_SETN,807, "Job array <%s> is not found%s")), /* catgets  807  */
                           lsb_jobid2str(jobId), hostOrQueue); 
            }
            else
                sprintf(errMsg, (_i18n_msg_get(ls_catd,NL_SETN,808, "Job <%s> is not found%s")),  /* catgets  808  */
	    	    lsb_jobid2str(jobId), hostOrQueue);
        }
        else if (jobName && !strcmp(jobName, "/") && (options & JGRP_ARRAY_INFO))
            sprintf(errMsg, (_i18n_msg_get(ls_catd,NL_SETN,810, "Job array is not found%s")), hostOrQueue); /* catgets  810  */
        else if (jobName)  {
            if (options & JGRP_ARRAY_INFO) {
                if (strchr(jobName, '['))
                   sprintf(errMsg, (_i18n_msg_get(ls_catd,NL_SETN,806, "Job <%s> is not a job array")),
                                   jobName);
                else
                   sprintf(errMsg, (_i18n_msg_get(ls_catd,NL_SETN,807, "Job array <%s> is not found%s")), 
                           jobName, hostOrQueue);
            }
            else
                sprintf(errMsg, (_i18n_msg_get(ls_catd,NL_SETN,808, "Job <%s> is not found%s")), jobName, hostOrQueue); 
        }
        else if (options & ALL_JOB)
            sprintf(errMsg, (_i18n_msg_get(ls_catd,NL_SETN,814, "No job found%s")), hostOrQueue); /* catgets  814  */
        else if (options & (CUR_JOB | LAST_JOB))
            sprintf(errMsg, (_i18n_msg_get(ls_catd,NL_SETN,815, "No unfinished job found%s")), hostOrQueue); /* catgets  815  */
        else if (options & DONE_JOB)
            sprintf(errMsg, (_i18n_msg_get(ls_catd,NL_SETN,816, "No DONE/EXIT job found%s")), hostOrQueue); /* catgets  816  */
        else if (options & PEND_JOB)
            sprintf(errMsg, (_i18n_msg_get(ls_catd,NL_SETN,817, "No pending job found%s")), hostOrQueue); /* catgets  817  */
        else if (options & SUSP_JOB)
            sprintf(errMsg, (_i18n_msg_get(ls_catd,NL_SETN,818, "No suspended job found%s")), hostOrQueue); /* catgets  818  */
        else if (options & RUN_JOB)
            sprintf(errMsg, (_i18n_msg_get(ls_catd,NL_SETN,819, "No running job found%s")), hostOrQueue); /* catgets  819  */
        else
            sprintf(errMsg, (_i18n_msg_get(ls_catd,NL_SETN,820, "No job found"))); /* catgets  820  */
        fprintf (stderr, "%s\n", errMsg);
        return;
    }

    lsb_perror (NULL);
    return;

} 

