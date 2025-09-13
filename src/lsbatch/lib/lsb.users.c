/* $Id: lsb.users.c,v 1.2 2007/08/15 22:18:48 tmizan Exp $
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

#include "lsb.h"


struct userInfoEnt * 
lsb_userinfo (char **users, int *numUsers)
{
    mbdReqType mbdReqtype;
    XDR xdrs;
    struct LSFHeader hdr;
    char *reply_buf;
    char *request_buf;
    struct userInfoReply userInfoReply, *reply;
    static struct infoReq userInfoReq;         
    int i, cc = 0, numReq = -1;
    char lsfUserName[MAXLINELEN];

    if (numUsers) {
	numReq = *numUsers;                     
        *numUsers = 0;                          
    }
    if (numReq < 0) {
        lsberrno = LSBE_BAD_ARG;
        return (NULL);
    }

    if (userInfoReq.names)
        free (userInfoReq.names);              

    if (numUsers == NULL || numReq == 0) {     
	userInfoReq.numNames = 0;
        if ((userInfoReq.names = (char **)malloc (sizeof(char *))) == NULL) {
            lsberrno = LSBE_NO_MEM;
            return(NULL);
        }
        userInfoReq.names[0] = "";
        cc = 1;                                     
    } 
    else if (numReq == 1 && users == NULL) {     
        if (getLSFUser_(lsfUserName, MAXLINELEN) != 0) {
            return (NULL);
        }
        userInfoReq.numNames = 1;
        if ((userInfoReq.names = (char **)malloc (sizeof(char *))) == NULL) {
            lsberrno = LSBE_NO_MEM;
            return(NULL);
        }
        userInfoReq.names[0] = lsfUserName;
        cc = 1;                                              
    }
    else {                                  
        if ((userInfoReq.names = (char **) calloc 
                                 (numReq, sizeof(char*))) == NULL) {
            lsberrno = LSBE_NO_MEM;
            return(NULL);
        }
        userInfoReq.numNames = numReq;
        for (i = 0; i < numReq; i++) {
            if (users[i] && strlen (users[i]) + 1 < MAXHOSTNAMELEN)
                userInfoReq.names[i] = users[i];
            else {
                free (userInfoReq.names);
                lsberrno = LSBE_BAD_USER;                        *numUsers = i;
                return (NULL);
            }
        }
        cc = numReq;                               
    }
    userInfoReq.resReq = "";

    
    mbdReqtype = BATCH_USER_INFO;
    cc = sizeof(struct infoReq) + cc * MAXHOSTNAMELEN + cc + 100;
    if ((request_buf = malloc (cc)) == NULL) {
        lsberrno = LSBE_NO_MEM;
	return(NULL);
    }
    xdrmem_create(&xdrs, request_buf, cc, XDR_ENCODE);

    initLSFHeader_(&hdr);
    hdr.opCode = mbdReqtype;
    if (!xdr_encodeMsg(&xdrs, (char *)&userInfoReq, &hdr, xdr_infoReq,
		       0, NULL)) {
        xdr_destroy(&xdrs);
        free (request_buf);
        lsberrno = LSBE_XDR;
        return(NULL);
    }

    
    if ((cc = callmbd (NULL, request_buf, XDR_GETPOS(&xdrs), 
                            &reply_buf, &hdr, NULL, NULL, NULL)) == -1) {
        xdr_destroy(&xdrs);
        free (request_buf);
        return (NULL);
    }
    xdr_destroy(&xdrs);
    free (request_buf);

    lsberrno = hdr.opCode;
    if (lsberrno == LSBE_NO_ERROR || lsberrno == LSBE_BAD_USER) {
	xdrmem_create(&xdrs, reply_buf, XDR_DECODE_SIZE_(cc), XDR_DECODE);	
        reply = &userInfoReply;
        if(!xdr_userInfoReply(&xdrs, reply, &hdr)) {
	    lsberrno = LSBE_XDR;
            xdr_destroy(&xdrs);
	    if (cc)
		free(reply_buf);
	    return(NULL);
        }
        xdr_destroy(&xdrs);
	if (cc)
	    free(reply_buf);
        if (lsberrno == LSBE_BAD_USER) {
            *numUsers = reply->badUser;
            return (NULL);
        }
        *numUsers = reply->numUsers;
	return(reply->users);
    }

    if (cc)
	free(reply_buf);
    return(NULL);

} 

