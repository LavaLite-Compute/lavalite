/* $Id: lsb.reconfig.c,v 1.4 2007/08/15 22:18:48 tmizan Exp $
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
#include <pwd.h>

#include "lsb.h"

extern void setHdrReserved(struct LSFHeader *, unsigned int );

int 
lsb_reconfig (int configFlag)
{
    mbdReqType mbdReqtype;
    XDR xdrs;
    char request_buf[MSGSIZE];
    char *reply_buf;
    int cc;
    struct LSFHeader hdr;
    struct lsfAuth auth;
    int tmp;

    

    mbdReqtype = BATCH_RECONFIG;

    if (authTicketTokens_(&auth, NULL) == -1)
	return (-1);
    
    xdrmem_create(&xdrs, request_buf, MSGSIZE, XDR_ENCODE);

    initLSFHeader_(&hdr);
    hdr.opCode = mbdReqtype;
    tmp = (short) configFlag;
    setHdrReserved(&hdr, tmp);

    if (!xdr_encodeMsg(&xdrs, (char *)NULL, &hdr, NULL, 0, &auth)) {
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
    if (cc)
	free(reply_buf);
        
    lsberrno = hdr.opCode;
    if (lsberrno == LSBE_NO_ERROR)
	return(0);
    else
	return(-1);
} 
