/* $Id: lsb.mig.c,v 1.3 2007/08/15 22:18:47 tmizan Exp $
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
lsb_mig (struct submig *mig, int *badHostIdx)
{
    struct migReq migReq;
    char request_buf[MSGSIZE];
    char *reply_buf;
    XDR xdrs;
    mbdReqType mbdReqtype;
    int cc;
    struct LSFHeader hdr;
    struct lsfAuth auth;


    if (mig->jobId <= 0) {
	lsberrno = LSBE_BAD_ARG;
	return(-1);
    }

    if (authTicketTokens_(&auth, NULL) == -1)
	return (-1);
    
    migReq.jobId = mig->jobId;
    migReq.options = 0;
    if (mig->options & LSB_CHKPNT_FORCE)
	migReq.options |= LSB_CHKPNT_FORCE;
    
    if (mig->numAskedHosts > 0) {
	for (migReq.numAskedHosts = 0;
	     migReq.numAskedHosts < mig->numAskedHosts;
	     migReq.numAskedHosts++) {
            if (strlen (mig->askedHosts[migReq.numAskedHosts]) 
                                              >= MAXHOSTNAMELEN - 1) {
                lsberrno = LSBE_BAD_HOST;
                return(-1);
            }
        }
	migReq.askedHosts = mig->askedHosts;
    } else {
	migReq.numAskedHosts = 0;
    }

    
    
    mbdReqtype = BATCH_JOB_MIG;
    xdrmem_create(&xdrs, request_buf, MSGSIZE, XDR_ENCODE);
    hdr.opCode = mbdReqtype;
    if (!xdr_encodeMsg(&xdrs, (char *)&migReq, &hdr, xdr_migReq, 0, &auth)) {
        lsberrno = LSBE_XDR;
        return(-1);
    }

    
    if ((cc = callmbd (NULL, request_buf, XDR_GETPOS(&xdrs), &reply_buf, 
                       &hdr, NULL, NULL, NULL)) == -1) {
	xdr_destroy(&xdrs);
	return (-1);
    }

    xdr_destroy(&xdrs);

    lsberrno = hdr.opCode;
    if (lsberrno != LSBE_NO_ERROR) {
	struct submitMbdReply reply;

	if (cc == 0) {
	    *badHostIdx = 0;
	    return (-1);
	}
	
	xdrmem_create(&xdrs, reply_buf, XDR_DECODE_SIZE_(cc), XDR_DECODE);
	if (!xdr_submitMbdReply(&xdrs, &reply, &hdr)) {
	    lsberrno = LSBE_XDR;
	    xdr_destroy(&xdrs);
	    free(reply_buf);
	    return (-1);
	}

	xdr_destroy(&xdrs);
	free(reply_buf);
	
	*badHostIdx = reply.badReqIndx;
	return (-1);
    }


    if (cc)
	free(reply_buf);	
    return (0);

} 
