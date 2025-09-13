/* $Id: lsb.groups.c,v 1.3 2007/08/15 22:18:47 tmizan Exp $
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

#include "lsb.h"

static struct groupInfoEnt *getGrpInfo (char **, int *, int);

static int sendGrpReq (char * ,int, struct infoReq *, struct groupInfoReply *);

struct groupInfoEnt *
lsb_usergrpinfo (char **groups, int *numGroups, int options) 
{
    options |= USER_GRP;
    return (getGrpInfo(groups, numGroups, options));

} 

struct groupInfoEnt *
lsb_hostgrpinfo(char **groups, int *numGroups, int options) 
{
    options |= HOST_GRP;
    return (getGrpInfo(groups, numGroups, options));
      
} 

static struct groupInfoEnt *
getGrpInfo(char **groups, int *numGroups, int options) 
{
    int                            i;
    char *                         clusterName = NULL;
    static  struct groupInfoReply  reply;
    struct  infoReq                groupInfo;

    memset((struct infoReq *)&groupInfo, '\0', sizeof(struct infoReq));

    if (numGroups != NULL && *numGroups < 0) {
        lsberrno = LSBE_BAD_ARG;
        return (NULL);
    }

    if (*numGroups < 0 || *numGroups > MAX_GROUPS) {
        lsberrno = LSBE_BAD_ARG;
        return (NULL);
    }

    
    if (numGroups == NULL || *numGroups == 0 || groups == NULL) {

	options |= GRP_ALL;
	
	
	groupInfo.options = options;
	groupInfo.resReq = "";
	
    }
    else {
	
    
	for (i = 0; i < *numGroups; i++) { 
	    if (ls_isclustername(groups[i]) <= 0 || (options & USER_GRP)) 
		continue;
	    
	    options |= GRP_ALL;
	    clusterName = groups[i];
	}
	
	
	if (clusterName == NULL) {
	    groupInfo.options  = options;
	    groupInfo.numNames = *numGroups;
	    groupInfo.names    = groups;
	    groupInfo.resReq   = "";
	} else {
	    
	    groupInfo.options  = options;
	    groupInfo.numNames = 0;
	    groupInfo.names    = NULL;
	    groupInfo.resReq   = "";
	}
	    
    }
    
    
    if (sendGrpReq(clusterName, 
		   options, 
		   &groupInfo, 
		   &reply) < 0) {
	
        *numGroups = reply.numGroups; 
	return(NULL);
    }

    *numGroups = reply.numGroups;
    
    return(reply.groups);

} 
				       
static int 
sendGrpReq (char *clusterName, int options, struct infoReq *groupInfo, 
	    struct groupInfoReply *reply)
{
    XDR                    xdrs;
    char                   request_buf[MSGSIZE];
    char *                 reply_buf;
    struct LSFHeader       hdr;
    mbdReqType             mbdReqtype;
    int                    cc;


    
    xdr_lsffree(xdr_groupInfoReply, (char *)reply, &hdr);

    
    mbdReqtype = BATCH_GRP_INFO;
    xdrmem_create(&xdrs, request_buf, MSGSIZE, XDR_ENCODE);

    hdr.opCode = mbdReqtype;
    if (!xdr_encodeMsg(&xdrs, 
		       (char *)groupInfo, 
		       &hdr, 
		       xdr_infoReq,
		       0, 
		       NULL)){
	lsberrno = LSBE_XDR;
        xdr_destroy(&xdrs);
	return(-1);
    }

    
    if ((cc = callmbd (clusterName, 
		       request_buf, 
		       XDR_GETPOS(&xdrs), 
		       &reply_buf, 
                       &hdr, 
		       NULL, 
		       NULL, 
		       NULL)) == -1) {
        xdr_destroy(&xdrs);
	return (-1);
    }
    
    
    xdr_destroy(&xdrs);

    
    lsberrno =  hdr.opCode;
    if (lsberrno == LSBE_NO_ERROR || lsberrno == LSBE_BAD_GROUP ) {
	xdrmem_create(&xdrs, 
		      reply_buf, 
		      XDR_DECODE_SIZE_(cc), 
		      XDR_DECODE);
	
        if (!xdr_groupInfoReply(&xdrs, reply, &hdr)) {
	    lsberrno = LSBE_XDR;
            xdr_destroy(&xdrs);
	    if (cc)
		free(reply_buf);
	    return(-1);
        }
        xdr_destroy(&xdrs);
	if (cc)
	    free(reply_buf);
        return (0);
    }

    if (cc)
	free(reply_buf);
    return(-1);

} 

void
freeGroupInfoReply (struct groupInfoReply *reply)
{
    int i;

    if (reply == NULL)
        return;

    for (i = 0; i < reply->numGroups; i++) {
        FREEUP(reply->groups[i].memberList);
	
    }

    FREEUP (reply->groups);

} 
