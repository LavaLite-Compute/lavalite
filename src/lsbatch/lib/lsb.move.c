/* $Id: lsb.move.c,v 1.3 2007/08/15 22:18:47 tmizan Exp $
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
#include <pwd.h>

#include "lsb.h"

int 
lsb_movejob (LS_LONG_INT jobId, int *position, int opCode)
{
    struct jobMoveReq jobMoveReq;
    char request_buf[MSGSIZE];
    char *reply_buf;
    XDR xdrs;
    mbdReqType mbdReqtype;
    int cc;
    struct LSFHeader hdr;
    struct lsfAuth auth;

    if (opCode != TO_TOP && opCode != TO_BOTTOM) {
	lsberrno = LSBE_BAD_ARG;
	return(-1);
    }

    if (position == NULL ) {
	lsberrno = LSBE_BAD_ARG;
	return(-1);
    }

    if (jobId <= 0 || *position <= 0) {
	lsberrno = LSBE_BAD_ARG;
	return(-1);
    }

    if (authTicketTokens_(&auth, NULL) == -1)
	return (-1);
	 
    jobMoveReq.jobId = jobId;
    jobMoveReq.position = *position;
    jobMoveReq.opCode = opCode;

    
    mbdReqtype = BATCH_JOB_MOVE;
    xdrmem_create(&xdrs, request_buf, MSGSIZE, XDR_ENCODE);

    hdr.opCode = mbdReqtype;
    if (!xdr_encodeMsg(&xdrs, (char *) &jobMoveReq, &hdr, xdr_jobMoveReq, 0, &auth)) {
	xdr_destroy(&xdrs);
	lsberrno = LSBE_XDR;
	return(-1);
    } 
    
    
    if ((cc = callmbd (NULL, request_buf, XDR_GETPOS(&xdrs), &reply_buf, 
                       &hdr, NULL, NULL, NULL)) == -1)    {
	xdr_destroy(&xdrs);
	return (-1);
    }
    xdr_destroy(&xdrs);

    
    lsberrno = hdr.opCode;
    if (lsberrno == LSBE_NO_ERROR) {
        xdrmem_create(&xdrs, reply_buf, XDR_DECODE_SIZE_(cc), XDR_DECODE);
	if (!xdr_jobMoveReq(&xdrs, &jobMoveReq, &hdr)) {
	    lsberrno = LSBE_XDR;
            xdr_destroy(&xdrs);
	    if (cc)
		free(reply_buf);
	    return(-1);
        }
        *position = jobMoveReq.position ;
        xdr_destroy(&xdrs);
	if (cc)
	    free(reply_buf);
        return(0);
    }

    if (cc)
	free(reply_buf);
    return(-1);

} 

