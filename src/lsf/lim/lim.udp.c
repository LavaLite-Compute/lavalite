/*
 * Copyright (C) 2007 Platform Computing Inc
 * Copyright (C) LavaLite Contributors
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 USA
 *
 */

#include "lsf/lim/lim.h"

// This is UDP
void clusNameReq(XDR *xdrs, struct sockaddr_in *from,
                 struct packet_header *reqHdr)
{
    XDR xdrs2;
    char buf[MSGSIZE];
    enum limReplyCode limReplyCode;
    char *sp;
    struct packet_header replyHdr;

    memset(&buf, 0, sizeof(buf));
    init_pack_hdr(&replyHdr);
    limReplyCode = LIME_NO_ERR;
    replyHdr.operation = (short) limReplyCode;
    replyHdr.sequence = reqHdr->sequence;
    sp = myClusterPtr->clName;

    xdrmem_create(&xdrs2, buf, MSGSIZE, XDR_ENCODE);
    if (!xdr_pack_hdr(&xdrs2, &replyHdr) ||
        !xdr_string(&xdrs2, &sp, MAXLSFNAMELEN)) {
        ls_syslog(LOG_ERR, "%s: xdr_pack_hdr failed:%m", __func__);
        xdr_destroy(&xdrs2);
        return;
    }

    if (chanSendDgram_(lim_udp_sock, buf, xdr_getpos(&xdrs2), from) < 0) {
        ls_syslog(LOG_ERR, "%s: chanWrite() failed: %m", __func__,
                  sockAdd2Str_(from));
        xdr_destroy(&xdrs2);
        return;
    }

    xdr_destroy(&xdrs2);
}

// This is UDP
void masterInfoReq(XDR *xdrs, struct sockaddr_in *from,
                   struct packet_header *reqHdr)
{
    XDR xdrs2;
    char buf[LL_BUFSIZ_64];
    enum limReplyCode limReplyCode;
    struct hostNode *masterPtr;
    struct packet_header replyHdr;
    struct masterInfo masterInfo;

    memset((char *) &buf, 0, sizeof(buf));
    init_pack_hdr(&replyHdr);
    if (!myClusterPtr->masterKnown && myClusterPtr->prevMasterPtr == NULL)
        limReplyCode = LIME_MASTER_UNKNW;
    else
        limReplyCode = LIME_NO_ERR;

    xdrmem_create(&xdrs2, buf, MSGSIZE, XDR_ENCODE);
    replyHdr.operation = (short) limReplyCode;
    replyHdr.sequence = reqHdr->sequence;

    if (!xdr_pack_hdr(&xdrs2, &replyHdr)) {
        ls_syslog(LOG_ERR, "%s: xdr_pack_hdr() failed: %m", __func__);
        xdr_destroy(&xdrs2);
        return;
    }

    if (limReplyCode == LIME_NO_ERR) {
        masterPtr = myClusterPtr->masterKnown ? myClusterPtr->masterPtr
                                              : myClusterPtr->prevMasterPtr;
        // Fill the masterInfo structure
        strcpy(masterInfo.hostName, masterPtr->hostName);
        get_host_addrv4(masterPtr->v4_epoint, &masterInfo.addr);
        masterInfo.portno = masterPtr->statInfo.portno;

        if (!xdr_masterInfo(&xdrs2, &masterInfo, &replyHdr)) {
            ls_syslog(LOG_ERR, "%s: xdr_masterInfo() failed: %m", __func__);
            xdr_destroy(&xdrs2);
            return;
        }
    }

    int cc = chanSendDgram_(lim_udp_sock, buf, xdr_getpos(&xdrs2), from);
    if (cc < 0) {
        ls_syslog(LOG_ERR, "%s: chanWrite() failed: %m", __func__,
                  sockAdd2Str_(from));
        xdr_destroy(&xdrs2);
        return;
    }

    xdr_destroy(&xdrs2);
}

void pingReq(XDR *xdrs, struct sockaddr_in *from, struct packet_header *reqHdr)
{
    char buf[LL_BUFSIZ_32];
    XDR xdrs2;
    enum limReplyCode limReplyCode;
    struct packet_header replyHdr;

    limReplyCode = LIME_NO_ERR;
    replyHdr.operation = (short) limReplyCode;
    replyHdr.sequence = reqHdr->sequence;

    xdrmem_create(&xdrs2, buf, sizeof(buf), XDR_ENCODE);
    if (!xdr_pack_hdr(&xdrs2, &replyHdr) ||
        !xdr_string(&xdrs2, &myHostPtr->hostName, MAXHOSTNAMELEN)) {
        ls_syslog(LOG_ERR, "%s: xdr_pack_hdr failed: %m", __func__);
        xdr_destroy(&xdrs2);
        return;
    }

    int cc = chanSendDgram_(lim_udp_sock, buf, xdr_getpos(&xdrs2), from);
    if (cc < 0) {
        ls_syslog(LOG_ERR, "%s: chanWrite_() to %s failed: %m", __func__,
                  sockAdd2Str_(from));
        xdr_destroy(&xdrs2);
        return;
    }
    xdr_destroy(&xdrs2);
}
