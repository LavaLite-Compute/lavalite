/* $Id: lsb.params.c,v 1.2 2007/08/15 22:18:47 tmizan Exp $
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
#include <sys/types.h>
#include <netdb.h>
#include <string.h>
#include <pwd.h>

#include "lsb.h"


struct parameterInfo * 
lsb_parameterinfo (char **names, int *numUsers, int options)
{
    mbdReqType mbdReqtype;
    XDR xdrs;
    struct LSFHeader hdr;
    char *request_buf;
    char *reply_buf;
    static struct parameterInfo paramInfo;        
    struct parameterInfo *reply;
    static struct infoReq infoReq;
    static int alloc = FALSE;
    int cc = 0;

    
    infoReq.options = options;

    
    if (alloc == TRUE) {
	alloc = FALSE;
	FREEUP(infoReq.names);
    }

    if (numUsers)
	infoReq.numNames = *numUsers;
    else
        infoReq.numNames = 0;
    if (names)
	infoReq.names = names;
    else {
        if ((infoReq.names = (char **)malloc (sizeof(char *))) == NULL) {
            lsberrno = LSBE_NO_MEM;
            return(NULL);
        }
	alloc = TRUE;
        infoReq.names[0] = "";
        cc = 1;                                     
    }
    infoReq.resReq = "";

    
    mbdReqtype = BATCH_PARAM_INFO;
    cc = sizeof(struct infoReq) + cc * MAXHOSTNAMELEN + cc + 100;
    if ((request_buf = malloc (cc)) == NULL) {
        lsberrno = LSBE_NO_MEM;
        return(NULL);
    }
    xdrmem_create(&xdrs, request_buf, cc, XDR_ENCODE);

    hdr.opCode = mbdReqtype;
    if (!xdr_encodeMsg(&xdrs, (char *)&infoReq, &hdr, xdr_infoReq, 0, NULL)) {
        xdr_destroy(&xdrs);
        free (request_buf);
        lsberrno = LSBE_XDR;
        return(NULL);
    }

    
    if ((cc = callmbd (NULL,request_buf, XDR_GETPOS(&xdrs), &reply_buf, &hdr,
		       NULL, NULL, NULL)) == -1) {
        xdr_destroy(&xdrs);
        free (request_buf);
        return (NULL);
    }
    xdr_destroy(&xdrs);
    free (request_buf);

    lsberrno = hdr.opCode;
    if (lsberrno == LSBE_NO_ERROR || lsberrno == LSBE_BAD_USER) {
	xdrmem_create(&xdrs, reply_buf, XDR_DECODE_SIZE_(cc), XDR_DECODE);	
        reply = &paramInfo;
        if(!xdr_parameterInfo (&xdrs, reply, &hdr)) {
	    lsberrno = LSBE_XDR;
            xdr_destroy(&xdrs);
	    if (cc)
		free(reply_buf);
	    return(NULL);
        }
        xdr_destroy(&xdrs);
	if (cc)
	    free(reply_buf);
	return(reply);
    }

    if (cc)
	free(reply_buf);
    return(NULL);

} 

