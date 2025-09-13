/* $Id: lsb.hosts.c,v 1.4 2007/08/15 22:18:47 tmizan Exp $
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
struct hostInfoEnt * getHostStatus (struct hostDataReply reply);



struct hostInfoEnt * 
lsb_hostinfo (char **hosts, int *numHosts)
{
    return (lsb_hostinfo_ex (hosts, numHosts, NULL, 0));
} 


struct hostInfoEnt * 
lsb_hostinfo_ex (char **hosts, int *numHosts, char *resReq, int options)
{
    mbdReqType mbdReqtype;
    XDR xdrs;
    struct LSFHeader hdr;
    char *request_buf;
    char *reply_buf;
    int cc, i, numReq = -1;
    char *clusterName =NULL;
    static struct infoReq hostInfoReq;        
    struct hostDataReply reply;

    if (numHosts) {
        numReq = *numHosts;                     
        *numHosts = 0;                          
    }
    if (numReq < 0) {
	lsberrno = LSBE_BAD_ARG;
	return(NULL);
    }

    FREEUP (hostInfoReq.names);              
    if (numHosts == NULL || numReq == 0) {      
        hostInfoReq.numNames = 0;
        if ((hostInfoReq.names = (char **) malloc (sizeof(char *))) == NULL) {
            lsberrno = LSBE_NO_MEM;
            return(NULL);
        }

        hostInfoReq.numNames = 0;          

        
        if (resReq != NULL) {
            TIMEIT(0, (hostInfoReq.names[0] = ls_getmyhostname()), "ls_getmyhostname");
            options |= CHECK_HOST;
        }
        else {
            hostInfoReq.names[0] = "";
        }

        cc = 1;                                         
    }
    else if (numReq == 1 && hosts == NULL) {    
        hostInfoReq.numNames = 1;
        if ((hostInfoReq.names = (char **) malloc (sizeof(char *))) == NULL) {
            lsberrno = LSBE_NO_MEM;
            return(NULL);
        }
        TIMEIT(0, (hostInfoReq.names[0] = ls_getmyhostname()), "ls_getmyhostname");
        hostInfoReq.numNames = 1;          
        cc = 1;                                         
    }
    else {                                 
        if ((hostInfoReq.names = (char **) calloc
                                 (numReq, sizeof(char*))) == NULL) {
            lsberrno = LSBE_NO_MEM;
            return(NULL);
        }
        for (i = 0; i < numReq; i++) { 
            if (ls_isclustername(hosts[i]) <= 0)
                continue;
            hostInfoReq.names[0] = "";
            hostInfoReq.numNames = 0;
            clusterName = hosts[i];
            cc = 1;
            break;
        }
        if (clusterName == NULL) {
            for (i = 0; i < numReq; i++) {
                if (hosts[i] && strlen (hosts[i]) + 1 < MAXHOSTNAMELEN) 
          	        hostInfoReq.names[i] = hosts[i];
                else {
                    free (hostInfoReq.names);
                    lsberrno = LSBE_BAD_HOST;
                    *numHosts = i;
                    return (NULL);
                }
            }
            cc = numReq;                               
            hostInfoReq.numNames = numReq;          
        }
    }

    hostInfoReq.options = options;
    if (resReq != NULL) {
	if (strlen(resReq) > MAXLINELEN -1) {
	    lsberrno = LSBE_BAD_RESREQ;
	    return(NULL);
        } else {
	    hostInfoReq.resReq = resReq;
	    hostInfoReq.options |= SORT_HOST;
        }
    } else
	  hostInfoReq.resReq = "";


    mbdReqtype = BATCH_HOST_INFO;
    cc = sizeof(struct infoReq) + cc * MAXHOSTNAMELEN + cc + 100;
    if ((request_buf = malloc (cc)) == NULL) {
        lsberrno = LSBE_NO_MEM;
        return(NULL);
    }
    xdrmem_create(&xdrs, request_buf, cc, XDR_ENCODE);

    hdr.opCode = mbdReqtype;
    if (!xdr_encodeMsg(&xdrs, (char *)&hostInfoReq, &hdr, xdr_infoReq,
		       0, NULL)) {
        xdr_destroy(&xdrs);
        free (request_buf);
	lsberrno = LSBE_XDR;
	return(NULL);
    }

    
    TIMEIT(0, (cc = callmbd(clusterName, request_buf, XDR_GETPOS(&xdrs), &reply_buf, &hdr, NULL, NULL, NULL)), "callmbd");
    if (cc == -1) {
        xdr_destroy(&xdrs);
        free (request_buf);
	return (NULL);
    }
    xdr_destroy(&xdrs);
    free (request_buf);

     

    lsberrno = hdr.opCode;
    if (lsberrno == LSBE_NO_ERROR || lsberrno == LSBE_BAD_HOST) {
	xdrmem_create(&xdrs, reply_buf, XDR_DECODE_SIZE_(cc), XDR_DECODE);
	
        if(!xdr_hostDataReply(&xdrs, &reply, &hdr)) {
	    lsberrno = LSBE_XDR;
            xdr_destroy(&xdrs);
	    if (cc)
		free(reply_buf);
	    return(NULL);
        }
        xdr_destroy(&xdrs);
	if (cc)
	    free(reply_buf);
        if (lsberrno == LSBE_BAD_HOST) {
            *numHosts = reply.badHost;
            return (NULL);
        }
        *numHosts = reply.numHosts;
        return(reply.hosts);
    }

    if (cc)
	free(reply_buf);
    return(NULL);

} 

