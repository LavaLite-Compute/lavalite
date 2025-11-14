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

struct clientNode *clientMap[2 * MAXCLIENTS];

extern int chanIndex;

static void processMsg(int);
static void clientReq(XDR *, struct packet_header *, int);

static void shutDownChan(int);

void clientIO(struct Masks *chanmasks)
{
    static char fname[] = "clientIO";
    int i;

    for (i = 0; (i < chanIndex) && (i < 2 * MAXCLIENTS); i++) {
        if (i == lim_udp_sock || i == lim_tcp_sock)
            continue;

        if (FD_ISSET(i, &chanmasks->emask)) {
            if (clientMap[i])
                ls_syslog(LOG_ERR, "%s: Lost connection with client <%s>",
                          fname, sockAdd2Str_(&clientMap[i]->from));
            shutDownChan(i);
            continue;
        }

        if (FD_ISSET(i, &chanmasks->rmask)) {
            processMsg(i);
        }
    }
}

static void processMsg(int chanfd)
{
    static char fname[] = "processMsg";
    struct Buffer *buf;
    struct packet_header hdr;
    XDR xdrs;

    if (clientMap[chanfd] && clientMap[chanfd]->inprogress)
        return;

    if (chanDequeue_(chanfd, &buf) < 0) {
        ls_syslog(LOG_ERR, "%s: chanDequeue_() failed: %d", cherrno);
        shutDownChan(chanfd);
        return;
    }

    ls_syslog(LOG_DEBUG, "%s: Received message with len %d on chan %d", fname,
              buf->len, chanfd);

    xdrmem_create(&xdrs, buf->data, buf->len, XDR_DECODE);
    if (!xdr_pack_hdr(&xdrs, &hdr)) {
        ls_syslog(LOG_ERR, "%s: Bad header received", fname);
        xdr_destroy(&xdrs);
        shutDownChan(chanfd);
        chanFreeBuf_(buf);
        return;
    }

    if ((clientMap[chanfd] && hdr.operation >= FIRST_LIM_PRIV) ||
        (!clientMap[chanfd] && hdr.operation < FIRST_INTER_CLUS)) {
        ls_syslog(LOG_ERR, "%s: Invalid operation <%d> from client", fname,
                  hdr.operation);
        xdr_destroy(&xdrs);
        shutDownChan(chanfd);
        chanFreeBuf_(buf);
        return;
    }

    if (hdr.operation >= FIRST_INTER_CLUS && !masterMe) {
        ls_syslog(LOG_ERR,
                  "%s: Intercluster request received, but I'm not master",
                  fname);
        xdr_destroy(&xdrs);
        shutDownChan(chanfd);
        chanFreeBuf_(buf);
        return;
    }

    if (!clientMap[chanfd]) {
        if (hdr.operation != LIM_CLUST_INFO) {
            struct sockaddr_in fromAddr;
            socklen_t fromLen = sizeof(struct sockaddr_in);

            if (getpeername(chanSock_(chanfd), (struct sockaddr *) &fromAddr,
                            &fromLen) < 0) {
                syslog(LOG_ERR, "%s: getpeername(%d) failed. %m", __func__,
                       chanSock_(chanfd));
            }

            ls_syslog(LOG_ERR,
                      "%s: Protocol error received operation <%d> from %s",
                      fname, hdr.operation, sockAdd2Str_(&fromAddr));
            xdr_destroy(&xdrs);
            shutDownChan(chanfd);
            chanFreeBuf_(buf);
            return;
        }
    }

    if (logclass & LC_TRACE)
        syslog(LOG_DEBUG, "%s: Received request <%d> ", __func__,
               hdr.operation);

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
        xdr_destroy(&xdrs);
        shutDownChan(chanfd);
        chanFreeBuf_(buf);
        break;

    default:
        ls_syslog(LOG_ERR, "%s: Invalid operation <%d>", fname, hdr.operation);
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

#if 0
static void handle_load_req(const struct packet_header *hdr,
                            struct Buffer *buf, int chfd, u_int header_end);
static void handle_resinfo_req(const struct packet_header *hdr,
                               struct Buffer *buf, int chfd, u_int header_end);
/* ... */

static void
processMsg(int chfd)
{
    struct Buffer *buf = NULL;
    struct packet_header hdr;
    XDR xdr;
    u_int header_end;

    if (clientMap[chfd] && clientMap[chfd]->inprogress)
        return;

    if (chanDequeue_(chfd, &buf) < 0) {
        ls_syslog(LOG_ERR, "%s: chanDequeue_() failed: %d", __func__, cherrno);
        shutDownChan(chfd);
        return;
    }

    xdrmem_create(&xdr, buf->data, buf->len, XDR_DECODE);
    if (!xdr_pack_hdr(&xdr, &hdr)) {
        ls_syslog(LOG_ERR, "%s: bad header", __func__);
        xdr_destroy(&xdr);
        chanFreeBuf_(buf);
        shutDownChan(chfd);
        return;
    }
    header_end = XDR_GETPOS(&xdr);
    xdr_destroy(&xdr);

    switch (hdr.operation) {
    case LIM_LOAD_REQ:       handle_load_req(&hdr, buf, chfd, header_end);    break;
    case LIM_GET_HOSTINFO:   /* ... */                                         break;
    case LIM_PLACEMENT:      /* ... */                                         break;
    case LIM_GET_RESOUINFO:  handle_resinfo_req(&hdr, buf, chfd, header_end); break;
    case LIM_GET_INFO:       /* ... */                                         break;
    default:
        ls_syslog(LOG_WARNING, "%s: unknown op %d", __func__, hdr.operation);
        chanFreeBuf_(buf);
        break;
    }
}
#endif

#if 0
static void
handle_resinfo_req(const struct packet_header *hdr,
                   struct Buffer *buf, int chfd, u_int header_end)
{
    XDR x;
    xdrmem_create(&x, buf->data, buf->len, XDR_DECODE);
    XDR_SETPOS(&x, header_end);

    struct resourceInfoReq req = {0};
    if (!xdr_resourceInfoReq(&x, &req, (struct packet_header *)hdr)) {
        ls_syslog(LOG_ERR, "%s: decode resourceInfoReq failed", __func__);
        xdr_destroy(&x);
        chanFreeBuf_(buf);
        shutDownChan(chfd);
        return;
    }
    xdr_destroy(&x);

    pid_t pid = fork();
    if (pid < 0) {
        ls_syslog(LOG_ERR, "%s: fork() failed: %m", __func__);
        chanFreeBuf_(buf);
        shutDownChan(chfd);
        return;
    }
    if (pid == 0) { /* child */
        int sock = chanSock_(chfd);
        (void)io_block_(sock);               /* if you need blocking I/O */
        /* close epoll/event fds etc. if inherited; child must not use them */
        /* do the work; write reply */
        resourceInfoReq_do(&req, chfd, sock, hdr);
        _exit(0);
    }

    /* parent: we own the inbound buffer, child doesn’t need it */
    chanFreeBuf_(buf);
    /* optionally mark inprogress, track pid to reap via SIGCHLD */
}
#endif
