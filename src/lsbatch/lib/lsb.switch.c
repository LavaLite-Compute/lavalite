/* $Id: lsb.switch.c,v 1.3 2007/08/15 22:18:48 tmizan Exp $
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

int 
lsb_switchjob (LS_LONG_INT jobId, char *queue)
{
    struct jobSwitchReq jobSwitchReq;
    char request_buf[MSGSIZE];
    char *reply_buf;
    XDR xdrs;
    mbdReqType mbdReqtype;
    int cc;
    struct LSFHeader hdr;
    struct lsfAuth auth;


    if (jobId <= 0 || queue == 0) {
	lsberrno = LSBE_BAD_ARG;
	return(-1);
    }
    if (queue && (strlen (queue) >= MAX_LSB_NAME_LEN - 1)) {
        lsberrno = LSBE_BAD_QUEUE;
        return(-1);
    }


    if (authTicketTokens_(&auth, NULL) == -1)
	return (-1);
    
    jobSwitchReq.jobId = jobId;
    strcpy (jobSwitchReq.queue, queue);


    
    
    mbdReqtype = BATCH_JOB_SWITCH;
    xdrmem_create(&xdrs, request_buf, MSGSIZE, XDR_ENCODE);
    initLSFHeader_(&hdr);
    hdr.opCode = mbdReqtype;
    if (!xdr_encodeMsg(&xdrs, (char *)&jobSwitchReq, &hdr, xdr_jobSwitchReq,
		       0, &auth)) {
        lsberrno = LSBE_XDR;
        return(-1);
    }

    
    if ((cc = callmbd (NULL, request_buf, XDR_GETPOS(&xdrs), &reply_buf, 
                       &hdr, NULL, NULL, NULL)) == -1)
    {
	xdr_destroy(&xdrs);
	return (-1);
    }

    xdr_destroy(&xdrs);
    lsberrno = hdr.opCode;
    if (cc)
	free(reply_buf);

    if (lsberrno == LSBE_NO_ERROR)
        return(0);
    else
	return(-1);

} 
