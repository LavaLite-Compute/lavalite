/* $Id: lim.cluster.c,v 1.7 2007/08/15 22:18:53 tmizan Exp $
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

#define ABORT 1
#define RX_HINFO 2
#define RX_LINFO 3

#define HINFO_TIMEOUT 120
#define LINFO_TIMEOUT 60

static void clientReq(XDR *, struct packet_header *, int);
static void shutDownChan(int);
static void process_tcp_request(int);

int handle_tcp_client(struct Masks *chanmasks)
{
    for (int i = 0; (i < chanIndex) && (i < max_clients); i++) {
        if (i == lim_udp_sock || i == lim_tcp_sock)
            continue;

        if (FD_ISSET(i, &chanmasks->emask)) {
            // the client closed connected socket not an error
            if (clientMap[i])
                ls_syslog(LOG_DEBUG, "%s: Lost connection with client <%s>",
                          __func__, sockAdd2Str_(&clientMap[i]->from));
            shutDownChan(i);
            continue;
        }

        if (FD_ISSET(i, &chanmasks->rmask)) {
            process_tcp_request(i);
        }
    }
    return 0;
}

static void process_tcp_request(int chanfd)
{
    struct Buffer *buf;
    struct packet_header hdr;
    XDR xdrs;

    if (chanDequeue_(chanfd, &buf) < 0) {
        ls_syslog(LOG_ERR, "%s: chanDequeue_() failed: %d", __func__, cherrno);
        shutDownChan(chanfd);
        return;
    }

    ls_syslog(LOG_DEBUG, "%s: Received message with len %d on chan %d",
              __func__, buf->len, chanfd);

    xdrmem_create(&xdrs, buf->data, buf->len, XDR_DECODE);
    if (!xdr_pack_hdr(&xdrs, &hdr)) {
        ls_syslog(LOG_ERR, "%s: xdr_pack_hdr decode ", __func__);
        xdr_destroy(&xdrs);
        shutDownChan(chanfd);
        chanFreeBuf_(buf);
        return;
    }

    // How did a tcp client ended up here?
    if (!clientMap[chanfd]) {
        struct sockaddr_in from;
        socklen_t l = sizeof(struct sockaddr_in);

        if (getpeername(chanSock_(chanfd), (struct sockaddr *) &from, &l) < 0) {
            syslog(LOG_ERR, "%s: getpeername(%d) failed. %m", __func__,
                   chanSock_(chanfd));
            memset(&from, 0, sizeof(struct sockaddr_in));
        }
        ls_syslog(LOG_ERR, "%s: protocol error received operation: %d from %s",
                  __func__, hdr.operation, sockAdd2Str_(&from));
        xdr_destroy(&xdrs);
        shutDownChan(chanfd);
        chanFreeBuf_(buf);
        return;
    }

    if (logclass & LC_TRACE)
        syslog(LOG_DEBUG, "%s: received request: %d ", __func__, hdr.operation);

    switch (hdr.operation) {
    case LIM_LOAD_REQ:
    case LIM_GET_HOSTINFO:
    case LIM_PLACEMENT:
    case LIM_GET_RESOUINFO:
    case LIM_GET_INFO:
        clientMap[chanfd]->limReqCode = hdr.operation;
        clientMap[chanfd]->reqbuf = buf;
        clientReq(&xdrs, &hdr, chanfd);
        break;
    case LIM_LOAD_ADJ:
        loadadjReq(&xdrs, &clientMap[chanfd]->from, &hdr, chanfd);
        xdr_destroy(&xdrs);
        shutDownChan(chanfd);
        chanFreeBuf_(buf);
        break;
    case LIM_PING:
        // This is a connect from a master that is trying to
        // take over mastership, it does not expect any reply
        xdr_destroy(&xdrs);
        shutDownChan(chanfd);
        chanFreeBuf_(buf);
        break;
    case LIM_GET_CLUSINFO:
        clusInfoReq(&xdrs, &clientMap[chanfd]->from, &hdr, chanfd);
        xdr_destroy(&xdrs);
        shutDownChan(chanfd);
        chanFreeBuf_(buf);
        break;
    default:
        ls_syslog(LOG_ERR, "%s: invalid operation: %d", __func__,
                  hdr.operation);
        xdr_destroy(&xdrs);
        chanFreeBuf_(buf);
        break;
    }
}

static void clientReq(XDR *xdrs, struct packet_header *hdr, int chfd)
{
    struct decisionReq decisionRequest;
    int oldpos, i;

    clientMap[chfd]->clientMasks = 0;

    oldpos = XDR_GETPOS(xdrs);

    if (!xdr_decisionReq(xdrs, &decisionRequest, hdr)) {
        goto Reply1;
    }

    clientMap[chfd]->clientMasks = 0;
    for (i = 1; i < decisionRequest.numPrefs; i++) {
        if (!find_node_by_name(decisionRequest.preferredHosts[i])) {
            clientMap[chfd]->clientMasks = 0;
            break;
        }
    }

    for (i = 0; i < decisionRequest.numPrefs; i++)
        free(decisionRequest.preferredHosts[i]);
    free(decisionRequest.preferredHosts);

Reply1:

{
    int pid = 0;

    pid = fork();
    if (pid < 0) {
        ls_syslog(LOG_ERR, "%s: fork() failed: %d", __func__);
        xdr_destroy(xdrs);
        shutDownChan(chfd);
        return;
    }

    if (pid == 0) {
        int sock;

        chanClose_(lim_udp_sock);

        XDR_SETPOS(xdrs, oldpos);
        sock = chanSock_(chfd);
        if (io_block_(sock) < 0)
            ls_syslog(LOG_ERR, "%s: io_block_() failed: %m", __func__);

        switch (hdr->operation) {
        case LIM_GET_HOSTINFO:
            hostInfoReq(xdrs, clientMap[chfd]->fromHost, &clientMap[chfd]->from,
                        hdr, chfd);
            break;
        case LIM_GET_RESOUINFO:
            resourceInfoReq2(xdrs, &clientMap[chfd]->from, hdr, chfd);
            break;
        case LIM_LOAD_REQ:
            loadReq(xdrs, &clientMap[chfd]->from, hdr, chfd);
            break;
        case LIM_PLACEMENT:
            placeReq(xdrs, &clientMap[chfd]->from, hdr, chfd);
            break;
        case LIM_GET_INFO:
            infoReq(xdrs, &clientMap[chfd]->from, hdr, chfd);
            break;
        default:
            break;
        }
        exit(0);
    }
}
}

static void shutDownChan(int chanfd)
{
    chanClose_(chanfd);
    if (clientMap[chanfd]) {
        chanFreeBuf_(clientMap[chanfd]->reqbuf);
        FREEUP(clientMap[chanfd]);
    }
}
