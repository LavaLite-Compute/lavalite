/* $Id: lib.lim.c,v 1.9 2007/08/15 22:18:50 tmizan Exp $
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 *
 */
#include "lsf/lib/lib.h"
#include "lsf/lib/lib.channel.h"

#define MAXMSGLEN  32*MSGSIZE
#define CONNECT_TIMEOUT 5
#define RECV_TIMEOUT    20

struct sockaddr_in sockIds_[4];
int  limchans_[4];
static int conntimeout_ = CONNECT_TIMEOUT;
static int recvtimeout_ = RECV_TIMEOUT;

extern char *inet_ntoa(struct in_addr);
static u_int localAddr = 0;

static int callLimTcp_(char *, char **, int, struct packet_header *, int);
static int callLimUdp_(char *, char *, int, struct packet_header *, char *, int);
static int createLimSock_(struct sockaddr_in *);
static int rcvreply_(int, char *, char);

int lsf_lim_version = -1;

int
callLim_(enum limReqCode reqCode,
         void *dsend,
         bool_t (*xdr_sfunc)(),
         void *drecv,
         bool_t (*xdr_rfunc)(),
         char *host,
         int options,
         struct packet_header *hdr)
{
    // Bug for now static buffers
    static char   sbuf[LL_BUFSIZ_256];
    static char   rbuf[LL_BUFSIZ_4K];
    struct packet_header reqHdr;
    struct packet_header replyHdr;
    XDR    xdrs;
    char   *repBuf;
    enum limReplyCode limReplyCode;
    static char first = true;
    int reqLen;

    repBuf = NULL;
    masterLimDown = false;
    if (first) {

        if (initLimSock_() < 0)
            return -1;
        first = false;

        if (genParams_[LSF_API_CONNTIMEOUT].paramValue) {
            conntimeout_ = atoi(genParams_[LSF_API_CONNTIMEOUT].paramValue);
            if (conntimeout_ <= 0)
                conntimeout_ =  CONNECT_TIMEOUT;
        }

        if (genParams_[LSF_API_RECVTIMEOUT].paramValue) {
            recvtimeout_ = atoi(genParams_[LSF_API_RECVTIMEOUT].paramValue);
            if (recvtimeout_ <= 0)
                recvtimeout_ = RECV_TIMEOUT;
        }
    }

    initLSFHeader_(&reqHdr);
    reqHdr.operation = reqCode;

    reqHdr.sequence  = getRefNum_();
    reqHdr.version = CURRENT_PROTOCOL_VERSION;

    // Bug static bug size
    xdrmem_create(&xdrs, sbuf, LL_BUFSIZ_256, XDR_ENCODE);

    if (! xdr_encodeMsg(&xdrs, dsend, &reqHdr, xdr_sfunc, 0, NULL)) {
        xdr_destroy(&xdrs);
        lserrno = LSE_BAD_XDR;
        return -1;
    }

    reqLen = XDR_GETPOS(&xdrs);
    xdr_destroy(&xdrs);

    if (options & _USE_TCP_) {

        if (callLimTcp_(sbuf, &repBuf, reqLen, &replyHdr, options) < 0)
            return -1;

        // Use the reply buffer as need
        if (replyHdr.length == 0) {
            xdrmem_create(&xdrs, rbuf, sizeof(rbuf), XDR_DECODE);
        } else if (replyHdr.length <= sizeof(rbuf)) {
            xdrmem_create(&xdrs, repBuf, XDR_DECODE_SIZE_(replyHdr.length),
                          XDR_DECODE);
        } else {
            int dyn_rl = (size_t)replyHdr.length;
            dyn_r  = malloc(dyn_rl);
            if (!dyn_r) {
                lserrno = LSE_MALLOC;
                return -1;
            }
            xdrmem_create(&xdrs, (caddr_t)dyn_r, XDR_DECODE_SIZE_(dyn_rl),
                          XDR_DECODE);
        }

    }

    if (options & _USE_UDP_) {

        if (callLimUdp_(sbuf, rbuf, reqLen, &reqHdr, host, options) < 0)
            return -1;

        if (options & _NON_BLOCK_)
            return 0;

        xdrmem_create(&xdrs, rbuf, sizeof(rbuf), XDR_DECODE);
    }

    if (!(options & _USE_TCP_)) {
        if (!xdr_LSFHeader(&xdrs, &replyHdr)) {
            xdr_destroy(&xdrs);
            lserrno = LSE_BAD_XDR;
            return -1;
        }
    }

    limReplyCode = replyHdr.operation;

    lsf_lim_version = (int)replyHdr.version;

    switch (limReplyCode) {
    case LIME_NO_ERR:
        if (drecv != NULL) {
            if (! (*xdr_rfunc) (&xdrs, drecv, &replyHdr)) {
                xdr_destroy(&xdrs);
                if (options & _USE_TCP_)
                    FREEUP(repBuf);
                lserrno = LSE_BAD_XDR;
                return -1;
            }
        }
        xdr_destroy(&xdrs);
        if (options & _USE_TCP_)
            FREEUP(repBuf);
        if (hdr != NULL)
            *hdr = replyHdr;
        return 0;

    default:
        xdr_destroy(&xdrs);
        if (options & _USE_TCP_)
            FREEUP(repBuf);
        err_return_(limReplyCode);
        return -1;
    }
}

static int
callLimTcp_(char *reqbuf, char **rep_buf, int req_size,
            struct packet_header *replyHdr, int options)
{
    static char fname[]="callLimTcp_";
    char retried = false;
    int cc;
    XDR xdrs;
    struct Buffer sndbuf, rcvbuf;

    if (logclass & (LC_COMM | LC_TRACE))
        ls_syslog(LOG_DEBUG2,"%s: Entering...req_size=%d",fname, req_size);

    *rep_buf = NULL;
    if (!sockIds_[TCP].sin_addr.s_addr) {
        if (ls_getmastername() == NULL)
            return -1;
    }

contact:
    if (limchans_[TCP] < 0) {

        limchans_[TCP] = chanClientSocket_(AF_INET, SOCK_STREAM, 0);
        if (limchans_[TCP] < 0 )
            return -1;

        cc=chanConnect_(limchans_[TCP], &sockIds_[TCP], conntimeout_ * 1000, 0);
        if (cc < 0) {
            ls_syslog(LOG_DEBUG,"\
                    %s: failed in connecting to limChans_[TCP]=<%d> <%s>",
                      fname, limchans_[TCP], sockAdd2Str_(&sockIds_[TCP]));
            if (errno == ECONNREFUSED || errno == ENETUNREACH) {
                if (errno == ECONNREFUSED) {
                    lserrno = LSE_LIM_DOWN;

                    if (!getIsMasterCandidate_()) {
                        masterLimDown = true;
                    }
                }
                if (! retried) {
                    if (ls_getmastername() != NULL) {
                        retried = 1;
                        CLOSECD(limchans_[TCP]);
                        goto contact;
                    }
                }
            }
            sockIds_[TCP].sin_addr.s_addr = 0;
            sockIds_[TCP].sin_port        = 0;
            CLOSECD(limchans_[TCP]);
            return -1;
        }
    }

    CHAN_INIT_BUF(&sndbuf);
    sndbuf.data = reqbuf;
    sndbuf.len  = req_size;
    rcvbuf.data = NULL;
    rcvbuf.len  = 0;
    cc=chanRpc_(limchans_[TCP], &sndbuf, &rcvbuf, replyHdr, recvtimeout_*1000);
    if (cc < 0) {
        CLOSECD(limchans_[TCP]);
        return -1;
    }
    *rep_buf = rcvbuf.data;

    switch (replyHdr->operation) {
    case LIME_MASTER_UNKNW:
        lserrno = LSE_MASTR_UNKNW;
        FREEUP(*rep_buf);
        CLOSECD(limchans_[TCP]);
        return -1;
    case LIME_WRONG_MASTER:
        xdrmem_create(&xdrs, *rep_buf, MSGSIZE, XDR_DECODE);
        if (!xdr_masterInfo(&xdrs, &masterInfo_, replyHdr)) {
            lserrno = LSE_BAD_XDR;
            xdr_destroy(&xdrs);
            FREEUP(*rep_buf);
            CLOSECD(limchans_[TCP]);
            return -1;
        }

        xdr_destroy(&xdrs);

        if (masterInfo_.addr == 0 ||
            !memcmp((char *) &masterInfo_.addr,
                    (char *) &sockIds_[MASTER].sin_addr,
                    sizeof(u_int))) {
            if (!memcpy((char *) &masterInfo_.addr,
                        (char *) &sockIds_[MASTER].sin_addr,
                        sizeof(u_int)))
                lserrno = LSE_TIME_OUT;
            else
                lserrno = LSE_MASTR_UNKNW;
            FREEUP(*rep_buf);
            CLOSECD(limchans_[TCP]);
            return -1;
        }
        masterknown_ = true;
        memcpy((char *) &sockIds_[MASTER].sin_addr,
               (char *) &masterInfo_.addr, sizeof(u_int));
        memcpy((char *) &sockIds_[TCP].sin_addr,
               (char *) &masterInfo_.addr, sizeof(u_int));
        sockIds_[TCP].sin_port        = masterInfo_.portno;
        FREEUP(*rep_buf);

        CLOSECD(limchans_[TCP]);
        CLOSECD(limchans_[MASTER]);
        if (!retried) {
            retried = true;
            goto contact;
        } else {
            lserrno = LSE_LIM_DOWN;

            if (!getIsMasterCandidate_()) {
                masterLimDown = true;
            }
            return -1;
        }

    default:
        break;
    }

    if (!(options & _KEEP_CONNECT_)) {
        CLOSECD(limchans_[TCP]);
    }

    return 0;
}

static int
callLimUdp_(char *reqbuf, char *repbuf, int len, struct packet_header *reqHdr,
            char *host, int options)
{
    struct hostent *hp;
    int retried = 0;
    u_int  masterLimAddr;
    u_int  previousMasterLimAddr = 0;
    XDR   xdrs;
    enum limReplyCode limReplyCode;
    struct packet_header replyHdr;
    char *sp = genParams_[LSF_SERVER_HOSTS].paramValue;
    int cc;
    static char connected;
    int id=-1;
    char multicasting = false;

    if (options & _LOCAL_ && !sp) {
        id = PRIMARY;
    } else if (host != NULL) {

        if ((hp = (struct hostent *)getHostEntryByName_(host)) == (struct hostent *)NULL)
            return -1;

        if (options & _USE_PRIMARY_) {
            id = PRIMARY;
            if (memcmp((char *)&sockIds_[PRIMARY].sin_addr,
                       hp->h_addr, hp->h_length))
                CLOSECD(limchans_[PRIMARY]);

        } else {
            id = UNBOUND;
        }
        memcpy((char *)&sockIds_[id].sin_addr, hp->h_addr, hp->h_length);
        sockIds_[id].sin_family = AF_INET;
        sockIds_[id].sin_port   = sockIds_[PRIMARY].sin_port;
    } else {
        if (limchans_[MASTER] >= 0 || sp == NULL) {
            id = MASTER;
        } else {
            struct timeval timeout;

            timeout.tv_sec = 0;
            timeout.tv_usec= 20000;

            if (callLimUdp_(reqbuf, repbuf, len, reqHdr, ls_getmyhostname(),
                            options|_NON_BLOCK_) < 0)
                return -1;

            multicasting = true;
        checkMultiCast:
            do {
                timeout.tv_sec = 0;
                timeout.tv_usec= 20000;
                if (rd_select_(chanSock_(limchans_[UNBOUND]), &timeout) > 0)
                    break;
                host  = getNextWord_(&sp);
                if (host) {
                    if (callLimUdp_(reqbuf, repbuf, len, reqHdr, host,
                                    options|_NON_BLOCK_) < 0){
                        continue;
                    }
                }
            } while (host);
            retried = 1;
            id = UNBOUND;
            goto check;
        }
    }

contact:
    switch (id) {
    case PRIMARY:
    case MASTER:
        if (limchans_[id] < 0) {
            if ((limchans_[id] = createLimSock_(NULL)) < 0)
                return -1;

        }
        connected = false;
        break;
    case UNBOUND:
        if (limchans_[id] < 0) {
            if ((limchans_[id] = createLimSock_((struct sockaddr_in*)NULL)) < 0)
                return -1;
        }
        connected = false;
        break;
    default:
        break;
    }

    cc = chanSendDgram_(limchans_[id], reqbuf, len, &sockIds_[id]);
    if (cc < 0) {
        if (connected)
            CLOSECD(limchans_[id]);
        if (connected && errno == ECONNREFUSED) {
            connected = false;
            goto contact;
        }
        return -1;
    }
    if (options & _NON_BLOCK_) {
        return 0;
    }

check:
    if (rcvreply_(limchans_[id], repbuf, connected) < 0) {
        if (connected)
            CLOSECD(limchans_[id]);

        if (lserrno != LSE_TIME_OUT)
            return -1;

        if (host != NULL)
            return -1;

        if (id == PRIMARY) {
            if (retried) {
                lserrno = LSE_LIM_DOWN;
                return -1;
            } else {
                retried = 1;
            }
        }

        id = PRIMARY;
        goto contact;
    }

    xdrmem_create(&xdrs, repbuf, MSGSIZE, XDR_DECODE);
    if(!xdr_LSFHeader(&xdrs, &replyHdr)) {
        lserrno = LSE_BAD_XDR;
        xdr_destroy(&xdrs);
        return -1;
    }

    if (reqHdr->sequence != replyHdr.sequence) {
        xdr_destroy(&xdrs);
        if (multicasting)
            goto checkMultiCast;
        goto check;
    }

    limReplyCode = replyHdr.operation;
    switch (limReplyCode) {
    case LIME_MASTER_UNKNW:
        lserrno = LSE_MASTR_UNKNW;
        xdr_destroy(&xdrs);
        return -1;

    case LIME_WRONG_MASTER:
        if (!xdr_masterInfo(&xdrs, &masterInfo_, &replyHdr)) {
            lserrno = LSE_BAD_XDR;
            xdr_destroy(&xdrs);
            return -1;
        }
        masterLimAddr = masterInfo_.addr;

        if (masterLimAddr == 0 ||
            ((previousMasterLimAddr == masterLimAddr) && limchans_[MASTER] >= 0)
            ) {
            if (previousMasterLimAddr == masterLimAddr)
                lserrno = LSE_TIME_OUT;
            else
                lserrno = LSE_MASTR_UNKNW;
            xdr_destroy(&xdrs);
            return -1;
        }
        previousMasterLimAddr = masterLimAddr;

        memcpy((char *) &sockIds_[MASTER].sin_addr, (char *)&masterLimAddr,
               sizeof(u_int));
        memcpy((char *) &sockIds_[TCP].sin_addr, (char *)&masterLimAddr,
               sizeof(u_int));
        sockIds_[TCP].sin_port = masterInfo_.portno;

        id = MASTER;
        xdr_destroy(&xdrs);

        CLOSECD(limchans_[MASTER]);
        CLOSECD(limchans_[TCP]);
        goto contact;

    case LIME_NO_ERR:
    default:
        xdr_destroy(&xdrs);
        break;
    }

    return 0;

}

static int
createLimSock_(struct sockaddr_in *connaddr)
{
    int chfd;

    if (geteuid() == 0)
        chfd = chanClientSocket_(AF_INET, SOCK_DGRAM, CHAN_OP_PPORT);
    else
        chfd = chanClientSocket_(AF_INET, SOCK_DGRAM, 0);

    if (connaddr && chanConnect_(chfd, connaddr, -1, 0) < 0)
        return -1;

    return chfd;

}

int
initLimSock_()
{
    struct servent *sv;
    ushort service_port;

    if (initenv_(NULL, NULL) <0)
        return -1;

    if (genParams_[LSF_LIM_PORT].paramValue) {
        if ((service_port = atoi(genParams_[LSF_LIM_PORT].paramValue)) != 0)
            service_port = htons(service_port);
        else
        {
            lserrno = LSE_LIM_NREG;
            return -1;
        }
    } else if (genParams_[LSF_LIM_DEBUG].paramValue) {
        service_port = htons(LIM_PORT);
    } else {
# if defined(_COMPANY_X_)
        if ((service_port = get_port_number(LIM_SERVICE, (char *)NULL)) < 0) {
            lserrno = LSE_LIM_NREG;
            return -1;
        }
# else
        if ((sv = getservbyname("lim", "udp")) == NULL) {
            lserrno = LSE_LIM_NREG;
            return -1;
        }
        service_port = sv->s_port;
# endif
    }

    sockIds_[TCP].sin_family = AF_INET;
    sockIds_[TCP].sin_addr.s_addr = 0;
    sockIds_[TCP].sin_port = 0;
    limchans_[TCP] = -1;

    localAddr = htonl(LOOP_ADDR);
    sockIds_[PRIMARY].sin_family = AF_INET;
    sockIds_[PRIMARY].sin_addr.s_addr = localAddr;
    sockIds_[PRIMARY].sin_port = (u_short) service_port;
    limchans_[PRIMARY] = -1;

    sockIds_[MASTER].sin_family = AF_INET;
    sockIds_[MASTER].sin_addr.s_addr = localAddr;
    sockIds_[MASTER].sin_port = (u_short) service_port;
    limchans_[MASTER] = -1;

    sockIds_[UNBOUND].sin_family = AF_INET;
    sockIds_[UNBOUND].sin_addr.s_addr = localAddr;
    sockIds_[UNBOUND].sin_port = (u_short) service_port;
    limchans_[UNBOUND] = -1;

    return 0;
}

static int
rcvreply_(int sock, char *rep, char connected)
{
    struct sockaddr_in from;

    return(chanRcvDgram_(sock, rep, MSGSIZE, &from, conntimeout_ *1000));
}

void
err_return_(enum limReplyCode limReplyCode)
{
    switch (limReplyCode) {
    case LIME_BAD_RESREQ:
        lserrno = LSE_BAD_EXP;
        return;
    case LIME_NO_OKHOST:
        lserrno = LSE_NO_HOST;
        return;
    case LIME_NO_ELHOST:
        lserrno = LSE_NO_ELHOST;
        return;
    case LIME_BAD_DATA:
        lserrno = LSE_BAD_ARGS;
        return;
    case LIME_WRONG_MASTER:
        lserrno = LSE_MASTR_UNKNW;
        return;
    case LIME_MASTER_UNKNW:
        lserrno = LSE_MASTR_UNKNW;
        return;
    case LIME_IGNORED:
        lserrno = LSE_LIM_IGNORE;
        return;
    case LIME_DENIED:
        lserrno = LSE_LIM_DENIED;
        return;
    case LIME_UNKWN_HOST:
        lserrno = LSE_LIM_BADHOST;
        return;
    case LIME_LOCKED_AL:
        lserrno = LSE_LIM_ALOCKED;
        return;
    case LIME_NOT_LOCKED:
        lserrno = LSE_LIM_NLOCKED;
        return;
    case LIME_UNKWN_MODEL:
        lserrno = LSE_LIM_BADMOD;
        return;
    case LIME_BAD_SERVID:
        lserrno = LSE_BAD_SERVID;
        return;
    case LIME_NAUTH_HOST:
        lserrno = LSE_NLSF_HOST;
        return;
    case LIME_UNKWN_RNAME:
        lserrno = LSE_UNKWN_RESNAME;
        return;
    case LIME_UNKWN_RVAL:
        lserrno = LSE_UNKWN_RESVALUE;
        return;
    case LIME_BAD_FILTER:
        lserrno = LSE_BAD_NAMELIST;
        return;
    case LIME_NO_MEM:
        lserrno = LSE_LIM_NOMEM;
        return;
    case LIME_BAD_REQ_CODE:
        lserrno = LSE_PROTOC_LIM;
        return;
    case LIME_BAD_RESOURCE:
        lserrno = LSE_BAD_RESOURCE;
        return;
    case LIME_NO_RESOURCE:
        lserrno = LSE_NO_RESOURCE;
        return;
    case LIME_CONF_NOTREADY:
        lserrno = LSE_LIMCONF_NOTREADY;
        return;
    default:
        lserrno = limReplyCode + NOCODE;
        return;
    }
}
