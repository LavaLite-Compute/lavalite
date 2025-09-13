/* $Id: lsb.resource.c,v 1.2 2007/08/15 22:18:48 tmizan Exp $
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
#include <sys/time.h>
#include <sys/resource.h>
#include <string.h>

#include "../../lsf/lib/lib.h"
#include "lsb.h"


struct lsbSharedResourceInfo *
lsb_sharedresourceinfo(char **resources, int *numResources, char *hostName, int options)
{
    static char fname[] = "lsb_sharedresourceinfo";
    static struct lsbShareResourceInfoReply lsbResourceInfoReply;
    struct resourceInfoReq  resourceInfoReq;
    int cc = 0, i;
    char *clusterName = NULL;
    static struct LSFHeader hdr;
    char *request_buf;
    char *reply_buf;
    mbdReqType mbdReqtype;
    XDR xdrs, xdrs2;

#define FREE_MEMORY \
    { \
	free(request_buf); \
	free(resourceInfoReq.resourceNames); \
    }

#define FREE_REPLY_BUFFER \
    { \
	if (cc) \
	    free(reply_buf); \
    }


    if (logclass & (LC_TRACE))
        ls_syslog(LOG_DEBUG1, "%s: Entering this routine...", fname);

    if (lsbResourceInfoReply.numResources > 0)
	xdr_lsffree(xdr_lsbShareResourceInfoReply, (char *)&lsbResourceInfoReply, &hdr);
	
    if (numResources == NULL ||
	*numResources < 0 ||
	(resources == NULL && *numResources != 0) ||
	(resources != NULL && *numResources == 0)) {
	lsberrno = LSBE_BAD_ARG;
	return (NULL);
    }
    resourceInfoReq.options = 0;

    if (*numResources == 0) {
	
	if ((resourceInfoReq.resourceNames = 
		  (char **) malloc(sizeof (char *))) == NULL) {
             lsberrno = LSBE_NO_MEM;
             return (NULL);
	}
        resourceInfoReq.resourceNames[0] = ""; 
	resourceInfoReq.numResourceNames = 1;
        cc += 2;
    } else {
        if ((resourceInfoReq.resourceNames = 
	       (char **) malloc (*numResources * sizeof(char *))) == NULL) {
             lsberrno = LSBE_NO_MEM;
             return(NULL);
        }
        for (i = 0; i < *numResources; i++) {
            if (resources[i] && strlen (resources[i]) + 1 < MAXLSFNAMELEN) {
                resourceInfoReq.resourceNames[i] = resources[i]; 
                cc += MAXLSFNAMELEN;
            } else {
		free (resourceInfoReq.resourceNames);
		lserrno = LSBE_BAD_RESOURCE;
		return (NULL);
            }
        }
	resourceInfoReq.numResourceNames = *numResources;
    }
    if (hostName != NULL) {
        if (ls_isclustername(hostName) <= 0) {          
            if (strlen (hostName) + 1 < MAXHOSTNAMELEN) {
                resourceInfoReq.hostName = hostName;
            } else {
                lsberrno = LSBE_BAD_HOST;
                return (NULL);
            }
        } else {                      
            clusterName = hostName;
            cc += MAXHOSTNAMELEN;
        }
    } else
        resourceInfoReq.hostName = " ";

    

    mbdReqtype = BATCH_RESOURCE_INFO;
    cc = sizeof(struct resourceInfoReq) + cc + 100;
    if ((request_buf = malloc (cc)) == NULL) {
        lsberrno = LSBE_NO_MEM;
        return(NULL);
    }
    xdrmem_create(&xdrs, request_buf, MSGSIZE, XDR_ENCODE);
    initLSFHeader_(&hdr);
    hdr.opCode = mbdReqtype;
    if (!xdr_encodeMsg(&xdrs, (char*)&resourceInfoReq, &hdr, xdr_resourceInfoReq,
                       0, NULL)) {
        lsberrno = LSBE_XDR;
        xdr_destroy(&xdrs);
        FREE_MEMORY;
        return(NULL);
    }
 
    
    if ((cc = callmbd(clusterName, request_buf, XDR_GETPOS(&xdrs), &reply_buf,
                      &hdr, NULL, NULL, NULL)) == -1)
    {
        xdr_destroy(&xdrs);
        FREE_MEMORY;
        return (NULL);
    }
    FREE_MEMORY;

    
    lsberrno = hdr.opCode;
    if (lsberrno == LSBE_NO_ERROR) {
        xdrmem_create(&xdrs2, reply_buf, XDR_DECODE_SIZE_(cc), XDR_DECODE);
        if (!xdr_lsbShareResourceInfoReply(&xdrs2, &lsbResourceInfoReply, &hdr)) {
            lsberrno = LSBE_XDR;
            xdr_destroy(&xdrs2);
            FREE_REPLY_BUFFER;
            return(NULL);
        }
        xdr_destroy(&xdrs2);
        FREE_REPLY_BUFFER;
        *numResources = lsbResourceInfoReply.numResources;
        return (lsbResourceInfoReply.resources);
    }

    FREE_REPLY_BUFFER;
    return (NULL);
} 

