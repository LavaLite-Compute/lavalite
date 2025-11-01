/* $Id: lib.rf.c,v 1.4 2007/08/15 22:18:51 tmizan Exp $
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
#include "lsf/lib/lib.rf.h"
#include "lsf/lib/lib.xdrres.h"

static int maxOpen = NOFILE;

static struct rHosts {
    char *hname;
    int sock;
    time_t atime;
    struct rHosts *next;
    int nopen;
} *rHosts = NULL;

static struct rfTab {
    struct rHosts *host;
    int fd;
} *ft = NULL;

static int nrh = 0;
static int maxnrh = RF_MAXHOSTS;

static struct rHosts *rhConnect(char *host);
static struct rHosts *rhFind(char *host);
static struct rHosts *allocRH(void);
static int rhTerminate(char *host);

static int rxFlags = 0;

static struct rHosts *
rhConnect(char *host)
{
    char *argv[2], *hname;
    int tid;
    const char *officialName;
    struct rHosts *rh;
    int sock, i;
    char officialHostNameBuf[MAXHOSTNAMELEN];

    if (ft == NULL) {
        if ((ft = (struct rfTab *) calloc(maxOpen, sizeof(struct rfTab)))
            == NULL) {
            lserrno = LSE_MALLOC;
            return NULL;
        }

        for (i = 0; i < maxOpen; i++)
            ft[i].host = NULL;
    }

    if ((officialName = getHostOfficialByName_(host)) == NULL) {
        lserrno = LSE_BAD_HOST;
        return NULL;
    }

    strcpy(officialHostNameBuf, officialName);
    if ((rh = (struct rHosts *)rhFind((char*)officialHostNameBuf))) {
        rh->atime = time(NULL);
        return rh;
    }

    argv[0] = RF_SERVERD;
    argv[1] = NULL;
    if ((tid = ls_rtask(host, argv, REXF_TASKPORT | rxFlags)) < 0) {
        return NULL;
    }

    if ((sock = ls_conntaskport(tid)) < 0)
        return NULL;

    if ((hname = putstr_(officialHostNameBuf)) == NULL) {
        closesocket(sock);
        lserrno = LSE_MALLOC;
        return NULL;
    }

    if (nrh >= maxnrh) {

        for (rh = rHosts->next; rh; rh = rh->next) {
            if (rh->nopen == 0)
                break;
        }

        if (rh) {

            struct rHosts *lrurh = rh;

            for (rh = rh->next; rh; rh = rh->next) {
                if (rh->atime < lrurh->atime && rh->nopen == 0)
                    lrurh = rh;
            }
            if (rhTerminate(lrurh->hname) < 0) {
                closesocket(sock);
                free(hname);
                return NULL;
            }
        }
    }

    if ((rh = allocRH()) == NULL) {
        free(hname);
        closesocket(sock);
        lserrno = LSE_MALLOC;
        return NULL;
    }

    rh->sock = sock;
    rh->atime = time(NULL);
    rh->hname = hname;
    rh->nopen = 0;

    return rh;

}

static struct rHosts *
allocRH(void)
{
    struct rHosts *rh, *tmp;

    if ((rh = (struct rHosts *) malloc(sizeof(struct rHosts))) == NULL) {
        return NULL;
    }

    tmp = rHosts;
    rHosts = rh;
    rh->next = tmp;
    nrh++;

    return rh;
}

static struct rHosts *
rhFind(char *host)
{
    struct rHosts *rh;

    for (rh = rHosts; rh; rh = rh->next) {
        if (equalHost_(rh->hname, host))
            return rh;
    }

    return NULL;
}

int
ls_ropen(char *host, char *fn, int flags, int mode)
{
    char buf[MSGSIZE];
    struct ropenReq req;
    struct packet_header hdr;
    struct rHosts *rh;
    int fd, i;

    if ((rh = rhConnect(host)) == NULL)
        return -1;

    for (fd = 0; fd < maxOpen && ft[fd].host; fd++);

    if (fd == maxOpen) {
        struct rfTab *tmpft;

        if ((tmpft = (struct rfTab *) realloc((char *) ft,
                                              (maxOpen + NOFILE) *
                                              sizeof(struct rfTab)))
            == NULL) {
            lserrno = LSE_MALLOC;
            return -1;
        }

        ft = tmpft;
        for (i = maxOpen; i < maxOpen + NOFILE; i++)
            ft[i].host = NULL;

        maxOpen += NOFILE;
    }

    req.fn = fn;

    req.flags = flags;
    req.mode = mode;

    if (lsSendMsg_(rh->sock, RF_OPEN, 0, (char *) &req, buf,
                   sizeof(struct packet_header) + MAXFILENAMELEN +
                   sizeof(req), xdr_ropenReq, SOCK_WRITE_FIX, NULL) < 0) {
        return -1;
    }

    if (lsRecvMsg_(rh->sock, buf, sizeof(hdr), &hdr, NULL, NULL, SOCK_READ_FIX)
        < 0) {
        return -1;
    }

    if (hdr.operation < 0) {
        // Bug. Legacy
        // errnoDecode_(abs(hdr.operation));
        lserrno = LSE_FILE_SYS;
        return -1;
    }

    ft[fd].host = rh;
    ft[fd].fd = hdr.operation;
    rh->nopen++;

    return fd;
}

int
ls_rclose(int fd)
{
    char buf[MSGSIZE];
    int reqfd;
    struct packet_header hdr;
    struct rHosts *rh;

    if (fd < 0 || fd >= maxOpen || ft[fd].host == NULL) {
        lserrno = LSE_BAD_ARGS;
        return -1;
    }

    rh = ft[fd].host;
    reqfd = ft[fd].fd;

    if (lsSendMsg_(rh->sock, RF_CLOSE, 0, (char *) &reqfd, buf,
                   sizeof(struct packet_header) + sizeof(reqfd), xdr_int,
                   SOCK_WRITE_FIX, NULL) < 0) {
        return -1;
    }

    if (lsRecvMsg_(rh->sock, buf, sizeof(hdr), &hdr, NULL, NULL, SOCK_READ_FIX)
        < 0) {
        return -1;
    }

    ft[fd].host = NULL;
    rh->nopen--;
    if (rh->nopen == 0 && nrh > maxnrh)
        rhTerminate(rh->hname);

    if (hdr.operation < 0) {
        // Bug. Legacy.
        // errno = errnoDecode_(abs(hdr.operation));
        lserrno = LSE_FILE_SYS;
        return -1;
    }

    return 0;
}

int
ls_rwrite(int fd, char *buf, int len)
{
    struct rrdwrReq req;
    struct packet_header hdr;
    struct {
        struct packet_header _;
        struct rrdwrReq __;
    } msgBuf;
    struct rHosts *rh;

    if (fd < 0 || fd >= maxOpen || ft[fd].host == NULL) {
        lserrno = LSE_BAD_ARGS;
        return -1;
    }

    rh = ft[fd].host;

    req.fd = ft[fd].fd;
    req.len = len;

    if (lsSendMsg_(rh->sock, RF_WRITE, 0, (char *) &req, (char *) &msgBuf,
                   sizeof(struct packet_header) + sizeof(req),
                   xdr_rrdwrReq, SOCK_WRITE_FIX, NULL) < 0) {
        return -1;
    }

    if (SOCK_WRITE_FIX(rh->sock, buf, len) != len) {
        lserrno = LSE_MSG_SYS;
        return -1;
    }

    if (lsRecvMsg_(rh->sock, (char *) &msgBuf, sizeof(hdr), &hdr, NULL, NULL,
                   SOCK_READ_FIX) < 0) {
        return -1;
    }

    if (hdr.operation < 0) {
        //  Bug. Legacy.
        // errno = errnoDecode_(abs(hdr.operation));
        lserrno = LSE_FILE_SYS;
        return -1;
    }

    return hdr.length;
}

int
ls_rread(int fd, char *buf, int len)
{
    struct rrdwrReq req;
    struct packet_header hdr;
    struct {
        struct packet_header _;
        struct rrdwrReq __;
    } msgBuf;
    struct rHosts *rh;

    if (fd < 0 || fd >= maxOpen || ft[fd].host == NULL) {
        lserrno = LSE_BAD_ARGS;
        return -1;
    }

    rh = ft[fd].host;

    req.fd = ft[fd].fd;
    req.len = len;

    if (lsSendMsg_(rh->sock, RF_READ, 0, (char *) &req, (char *) &msgBuf,
                   sizeof(struct packet_header) + sizeof(req),
                   xdr_rrdwrReq, SOCK_WRITE_FIX, NULL) < 0) {
        return -1;
    }

    if (lsRecvMsg_(rh->sock, (char *) &msgBuf, sizeof(hdr), &hdr, NULL, NULL,
                   SOCK_READ_FIX) < 0) {
        return -1;
    }

    if (hdr.operation < 0) {
        // Bug. Legacy
        // errno = errnoDecode_(abs(hdr.operation));
        lserrno = LSE_FILE_SYS;
        return -1;
    }

    if (SOCK_READ_FIX(rh->sock, buf, hdr.length) != hdr.length) {
        lserrno = LSE_MSG_SYS;
        return -1;
    }
    return hdr.length;
}

off_t
ls_rlseek(int fd, off_t offset, int whence)
{
    struct rlseekReq req;
    struct packet_header hdr;
    struct {
        struct packet_header _;
        struct rlseekReq __;
    } msgBuf;
    struct rHosts *rh;

    if (fd < 0 || fd >= maxOpen || ft[fd].host == NULL) {
        lserrno = LSE_BAD_ARGS;
        return -1;
    }

    rh = ft[fd].host;

    req.fd = ft[fd].fd;
    req.offset = offset;
    req.whence = whence;

    if (lsSendMsg_(rh->sock, RF_LSEEK, 0, (char *) &req, (char *) &msgBuf,
                   sizeof(msgBuf), xdr_rlseekReq, SOCK_WRITE_FIX, NULL) < 0) {
        return -1;
    }

    if (lsRecvMsg_(rh->sock, (char *) &msgBuf, sizeof(hdr), &hdr, NULL, NULL,
                   SOCK_READ_FIX) < 0) {
        return -1;
    }

    if (hdr.operation < 0) {
        // Bug. This is some sort of garbage legacy
        // errno = errnoDecode_(abs(hdr.operation));
        lserrno = LSE_FILE_SYS;
        return -1;
    }

    return (off_t) hdr.length;
}

int
ls_rfstat(int fd, struct stat *st)
{
    char buf[MSGSIZE];
    int reqfd;
    struct packet_header hdr;
    struct rHosts *rh;

    if (fd < 0 || fd >= maxOpen || ft[fd].host == NULL) {
        lserrno = LSE_BAD_ARGS;
        return -1;
    }

    rh = ft[fd].host;

    reqfd = ft[fd].fd;

    if (lsSendMsg_(rh->sock, RF_FSTAT, 0, (char *) &reqfd, buf,
                   sizeof(struct packet_header) + sizeof(reqfd), xdr_int,
                   SOCK_WRITE_FIX, NULL) < 0) {
        return -1;
    }

    if (lsRecvMsg_(rh->sock, buf, MSGSIZE, &hdr,
                   (char *) st, xdr_stat, SOCK_READ_FIX) < 0) {
        return -1;
    }

    if (hdr.operation < 0) {
        // errno = errnoDecode_(abs(hdr.operation));
        lserrno = LSE_FILE_SYS;
        return -1;
    }

    return 0;
}

int
ls_rfcontrol(int command, int arg)
{
    switch (command) {
        case RF_CMD_MAXHOSTS:
            if (arg < 1) {
                lserrno = LSE_BAD_ARGS;
                return -1;
            }
            maxnrh = arg;
            return 0;

        case RF_CMD_RXFLAGS:
            rxFlags = arg;
            return 0;

        default:
            lserrno = LSE_BAD_ARGS;
            return -1;
    }
}

int
ls_rfterminate(char *host)
{
    return (rhTerminate(host));
}

static
int rhTerminate(char *host)
{
    struct rHosts *rh, *prev;
    const char *officialName;
    struct packet_header buf;
    int i;

    if ((officialName = getHostOfficialByName_(host)) == NULL) {
        lserrno = LSE_BAD_HOST;
        return -1;
    }

    for (prev = NULL, rh = rHosts; rh; prev = rh, rh = rh->next) {
        if (equalHost_(officialName, rh->hname)) {
            if (prev == NULL)
                rHosts = rh->next;
            else
                prev->next = rh->next;

            lsSendMsg_(rh->sock, RF_TERMINATE, 0, NULL, (char *) &buf,
                       sizeof(buf), NULL, SOCK_WRITE_FIX, NULL);
            closesocket(rh->sock);

            for (i = 0; i < maxOpen; i++) {
                if (ft[i].host == rh)
                    ft[i].host = NULL;
            }

            free(rh->hname);
            free(rh);
            nrh--;
            return 0;
        }
    }

    lserrno = LSE_BAD_HOST;
    return -1;
}

int
ls_rstat(char *host, char *fn, struct stat *st)
{
    char buf[MSGSIZE];
    struct packet_header hdr;
    struct rHosts *rh;
    struct stringLen fnStr;

    if ((rh = rhConnect(host)) == NULL)
        return -1;

    fnStr.len = MAXFILENAMELEN;
    fnStr.name = fn;
    if (lsSendMsg_(rh->sock, RF_STAT, 0, (char *)&fnStr, buf,
                   sizeof(struct packet_header) + MAXFILENAMELEN,
                   xdr_stringLen, SOCK_WRITE_FIX, NULL) < 0) {
        return -1;
    }

    if (lsRecvMsg_(rh->sock, buf, MSGSIZE, &hdr,
                   (char *) st, xdr_stat, SOCK_READ_FIX) < 0) {
        return -1;
    }

    if (hdr.operation < 0) {
        // errno = errnoDecode_(abs(hdr.operation));
        lserrno = LSE_FILE_SYS;
        return -1;
    }

    return 0;
}

char *
ls_rgetmnthost(char *host, char *fn)
{
    char buf[MSGSIZE];
    struct packet_header hdr;
    struct rHosts *rh;
    static char hostname[MAXHOSTNAMELEN];
    struct stringLen hostStr;
    struct stringLen fnStr;

    if ((rh = rhConnect(host)) == NULL)
        return NULL;

    hostStr.len = MAXHOSTNAMELEN;
    hostStr.name = hostname;
    fnStr.len = MAXFILENAMELEN;
    fnStr.name = fn;

    if (lsSendMsg_(rh->sock, RF_GETMNTHOST, 0, (char *) &fnStr, buf,
                   sizeof(struct packet_header) + MAXFILENAMELEN,
                   xdr_stringLen, SOCK_WRITE_FIX, NULL) < 0) {
        return NULL;
    }

    if (lsRecvMsg_(rh->sock, buf, MSGSIZE, &hdr,
                   (char *) &hostStr, xdr_stringLen, SOCK_READ_FIX) < 0) {
        return NULL;
    }

    if (hdr.operation < 0) {
        // errno = errnoDecode_(abs(hdr.operation));
        lserrno = LSE_FILE_SYS;
        return NULL;
    }

    return hostname;
}

int ls_conntaskport(int rpid)
{
    struct tid *tid;
    int sock;
    struct sockaddr_in sin;
    int resTimeout;
    socklen_t slen = sizeof(sin);

    if (genParams_[LSF_RES_TIMEOUT].paramValue)
        resTimeout = atoi(genParams_[LSF_RES_TIMEOUT].paramValue);
    else
        resTimeout = RES_TIMEOUT;

    if ((tid = tid_find(rpid)) == NULL) {
        return -1;
    }

    if (getpeername(tid->sock, (struct sockaddr *) &sin, &slen) < 0) {
        lserrno = LSE_SOCK_SYS;
        return -1;
    }

    if ((sock = CreateSock_(SOCK_STREAM)) < 0) {
        return -1;
    }

    sin.sin_port = tid->taskPort;

    if (b_connect_(sock, (struct sockaddr *) &sin, sizeof(sin), resTimeout) < 0)  {
        closesocket(sock);
        lserrno = LSE_CONN_SYS;
        return -1;
    }

    return sock;
}

int
ls_runlink(char *host, char *fn)
{
    char buf[MSGSIZE];
    struct packet_header hdr;
    struct rHosts *rh;
    static char hostname[MAXHOSTNAMELEN];
    struct stringLen hostStr;
    struct stringLen fnStr;

    if ((rh = rhConnect(host)) == NULL)
        return -1;

    hostStr.len = MAXHOSTNAMELEN;
    hostStr.name = hostname;
    fnStr.len = MAXFILENAMELEN;
    fnStr.name = fn;

    if (lsSendMsg_(rh->sock, RF_UNLINK, 0, (char *) &fnStr, buf,
                   sizeof(struct packet_header) + MAXFILENAMELEN,
                   xdr_stringLen, SOCK_WRITE_FIX, NULL) < 0) {
        return -1;
    }

    if (lsRecvMsg_(rh->sock, buf, MSGSIZE, &hdr,
                   (char *) &hostStr, xdr_stringLen, SOCK_READ_FIX) < 0) {
        return -1;
    }

    if (hdr.operation < 0) {
        // errno = errnoDecode_(abs(hdr.operation));
        lserrno = LSE_FILE_SYS;
        return -1;
    }

    return 0;
}
