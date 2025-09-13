/* $Id: lsb.debug.c,v 1.4 2007/08/15 22:18:47 tmizan Exp $
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
#include <sys/time.h>
#include <sys/resource.h>
#include <unistd.h>
#include <ctype.h>
#include <pwd.h>

#include "lsb.h"

int 
lsb_debugReq (struct debugReq  *pdebug , char *host)
{


    static struct debugReq  debug;

    mbdReqType mbdReqtype;
    XDR xdrs;
    char request_buf[MSGSIZE], hostName[MAXHOSTNAMELEN];
    char *reply_buf;
    int cc;
    struct LSFHeader hdr;
    struct lsfAuth auth; 
    char *toHost = NULL;


    debug.opCode = pdebug->opCode;
    debug.logClass  = pdebug->logClass;
    debug.level  = pdebug->level;
    debug.options = pdebug->options;
    debug.hostName = hostName;
    strcpy(debug.logFileName, pdebug->logFileName);

    if (host) {
        if (strlen (host) >= MAXHOSTNAMELEN - 1) {
            lsberrno = LSBE_BAD_ARG;
	    return (-1);
	}
  	strcpy(debug.hostName, host);
    } else {
        char *h;
        if ((h = ls_getmyhostname()) == NULL) {
	    lsberrno = LSBE_LSLIB;
	    return(-1);
	}
        strcpy(debug.hostName, h);
    }
     
    
    if ( debug.opCode == MBD_DEBUG || debug.opCode == MBD_TIMING){
	
        mbdReqtype = BATCH_DEBUG;
	toHost = NULL;
    } else {
	
        mbdReqtype = CMD_SBD_DEBUG;
	toHost = host;
    }
   
    
    if (authTicketTokens_(&auth, toHost) == -1){
	return (-1);
    }
    
    xdrmem_create(&xdrs, request_buf, MSGSIZE, XDR_ENCODE);
    initLSFHeader_(&hdr);

    hdr.opCode = mbdReqtype;

    if (!xdr_encodeMsg(&xdrs, (char *)&debug, &hdr, xdr_debugReq, 0, &auth)) {
        lsberrno = LSBE_XDR;
        return(-1);
    }

    

    if ( debug.opCode == MBD_DEBUG || debug.opCode == MBD_TIMING) {
        if ((cc = callmbd (NULL, request_buf, XDR_GETPOS(&xdrs), &reply_buf, 
                           &hdr, NULL, NULL, NULL)) == -1) {
            xdr_destroy(&xdrs); 
	    return (-1);
        }
    }
    else {
        if ((cc = cmdCallSBD_(debug.hostName, request_buf,
        	              XDR_GETPOS(&xdrs), &reply_buf, 
			      &hdr, NULL)) == -1) {
            xdr_destroy(&xdrs);
	    return (-1);
        }
    }
    xdr_destroy(&xdrs); 

    

    lsberrno = hdr.opCode;
    if (cc)
	free(reply_buf);    
    if (lsberrno == LSBE_NO_ERROR) 
        return(0);        

    return(-1);

} 

