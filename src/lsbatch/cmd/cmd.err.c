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

#include "lsbatch/cmd/cmd.h"

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
	    strcpy (hostOrQueue, (" in queue <"));
            strcat (hostOrQueue, queue);
        }
        if (host) {
            if (ls_isclustername(host) <= 0) {
                if (hostOrQueue[0] == '\0')
	    	    strcpy(hostOrQueue, (" on host/group <"));
                else
		    strcat(hostOrQueue, ("> and on host/group <"));
            } else {
               if (hostOrQueue[0] == '\0')
                    strcpy(hostOrQueue, (" in cluster <"));
                else
                    strcat(hostOrQueue, ("> and in cluster <"));
            }
            strcat(hostOrQueue, host);
        }
        if (hostOrQueue[0] != '\0')
            strcat(hostOrQueue, ">");

        if (jobId) {
            if (options & JGRP_ARRAY_INFO) {
               if (LSB_ARRAY_IDX(jobId))
                   sprintf(errMsg, ("Job <%s> is not a job array"),
                        lsb_jobid2str(jobId));
               else
                   sprintf(errMsg, ("Job array <%s> is not found%s"),
                           lsb_jobid2str(jobId), hostOrQueue);
            }
            else
                sprintf(errMsg, ("Job <%s> is not found%s"),
	    	    lsb_jobid2str(jobId), hostOrQueue);
        }
        else if (jobName && !strcmp(jobName, "/") && (options & JGRP_ARRAY_INFO))
            sprintf(errMsg, ("Job array is not found%s"), hostOrQueue);
        else if (jobName)  {
            if (options & JGRP_ARRAY_INFO) {
                if (strchr(jobName, '['))
                   sprintf(errMsg, ("Job <%s> is not a job array"),
                                   jobName);
                else
                   sprintf(errMsg, ("Job array <%s> is not found%s"),
                           jobName, hostOrQueue);
            }
            else
                sprintf(errMsg, ("Job <%s> is not found%s"), jobName, hostOrQueue);
        }
        else if (options & ALL_JOB)
            sprintf(errMsg, ("No job found%s"), hostOrQueue);
        else if (options & (CUR_JOB | LAST_JOB))
            sprintf(errMsg, ("No unfinished job found%s"), hostOrQueue);
        else if (options & DONE_JOB)
            sprintf(errMsg, ("No DONE/EXIT job found%s"), hostOrQueue);
        else if (options & PEND_JOB)
            sprintf(errMsg, ("No pending job found%s"), hostOrQueue);
        else if (options & SUSP_JOB)
            sprintf(errMsg, ("No suspended job found%s"), hostOrQueue);
        else if (options & RUN_JOB)
            sprintf(errMsg, ("No running job found%s"), hostOrQueue);
        else
            sprintf(errMsg, ("No job found"));
        fprintf (stderr, "%s\n", errMsg);
        return;
    }

    lsb_perror (NULL);
    return;

}
