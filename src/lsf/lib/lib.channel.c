/* $Id: lib.channel.c,v 1.6 2007/08/15 22:18:50 tmizan Exp $
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
#include "lsf/lib/lib.channel.h"

#define DEFAULT_MAX_CHANNELS 1024

#define CLOSEIT(i) {         \
    close(channels[i].handle);        \
    channels[i].state = CH_DISC;            \
    channels[i].handle = INVALID_HANDLE; }

static struct chanData *channels;
int cherrno = 0;
int chanIndex;
static int chanMaxSize;
static int chanGrowSize;
static int chanAllocSize;

static void doread(int , struct Masks *);
static void dowrite(int, struct Masks *);
static struct Buffer *newBuf(void);
static void enqueueTail_(struct Buffer *, struct Buffer *);
static void dequeue_(struct Buffer *);
static int findAFreeChannel(void);
static int growchanDataArray(void);

int
chanInit_(void)
{
    long divAmount;
    static char first = true;

    if (!first)
        return 0;
    first = false;

    chanMaxSize = sysconf(_SC_OPEN_MAX);

    if (chanMaxSize < 0)
        chanMaxSize = DEFAULT_MAX_CHANNELS;

    divAmount = ((chanMaxSize > 1000) ? 100 : 10);

    if (divAmount > 0)
        chanGrowSize = chanMaxSize / divAmount;
    else
        chanGrowSize = DEFAULT_MAX_CHANNELS;

    channels = calloc(sizeof(struct chanData), chanGrowSize);
    if (channels == NULL)
        return -1;

    chanAllocSize = chanGrowSize;
    chanIndex = 0;

    return 0;
}

int
chanServSocket_(int type, u_short port, int backlog, int options)
{
    int ch, s;
    struct sockaddr_in sin;

    if ((ch = findAFreeChannel()) < 0) {
        lserrno = LSE_NO_CHAN;
        return -1;
    }

    s = socket(AF_INET, type, 0);

    if (SOCK_INVALID(s)) {
        lserrno = LSE_SOCK_SYS;
        return -1;
    }

    memset((char*)&sin, 0, sizeof(sin));
    sin.sin_family      = AF_INET;
    sin.sin_port        = htons(port);
    sin.sin_addr.s_addr = htonl(INADDR_ANY);

    if (options & CHAN_OP_SOREUSE) {
        int one = 1;

        setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char *) &one,
                   sizeof (int));
    }

    if (bind(s, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
        (void)close(s);
        lserrno = LSE_SOCK_SYS;
        return -2;
    }

    if (backlog > 0) {
        if (listen(s, backlog) < 0) {
            (void)close(s);
            lserrno = LSE_SOCK_SYS;
            return -3;
        }
    }

    channels[ch].handle = s;
    channels[ch].state = CH_WAIT;
    if (type == SOCK_DGRAM)
        channels[ch].type  = CH_TYPE_UDP;
    else
        channels[ch].type  = CH_TYPE_PASSIVE;
    return ch;
}

int
chanClientSocket_(int domain, int type, int options)
{
    static char first=true;
    static ushort port;
    struct sockaddr_in cliaddr;

    if (domain != AF_INET) {
        lserrno = LSE_INTERNAL;
        return -1;
    }

    int ch = findAFreeChannel();
    if (ch < 0) {
        lserrno = LSE_NO_CHAN;
        return -1;
    }

    if (type == SOCK_STREAM)
        channels[ch].type = CH_TYPE_TCP;
    else
        channels[ch].type = CH_TYPE_UDP;

    int s = socket(domain, type, 0);
    if (s < 0) {
        lserrno = LSE_SOCK_SYS;
        return -1;
    }

    channels[ch].state = CH_DISC;
    channels[ch].handle = s;
    int s0 = ll_dup_stdio(s);
    if (s0 < 0) {
        close(s);
        chanClose_(ch);
        lserrno = LSE_SOCK_SYS;
        return -1;
    }
    channels[ch].handle = s0;

    if (options & CHAN_OP_PPORT) {
        if  (first) {
            first = false;
            port = IPPORT_RESERVED - 1;
        }
        if (port < IPPORT_RESERVED/2) {
            port = IPPORT_RESERVED - 1;
        }
    }

    s0 = channels[ch].handle;
    memset((char*)&cliaddr, 0, sizeof(cliaddr));
    cliaddr.sin_family      = AF_INET;
    cliaddr.sin_addr.s_addr = htonl(INADDR_ANY);

    int i;
    for (i=0; i < IPPORT_RESERVED/2; i++) {
        if (options & CHAN_OP_PPORT) {
            cliaddr.sin_port = htons(port);
            port--;
        } else
            cliaddr.sin_port = htons(0);

        if (bind(s0, (struct sockaddr *)&cliaddr, sizeof(cliaddr)) == 0)
            break;

        if (!(options & CHAN_OP_PPORT)) {

            if (errno == EADDRINUSE) {
                port = (ushort) (time(0) | getpid());
                port = ((port < 1024) ? (port + 1024) : port);
                cliaddr.sin_port = htons(port);
                if (bind(s0, (struct sockaddr *)&cliaddr, sizeof(cliaddr)) == 0)
                    break;
            }

            chanClose_(ch);
            lserrno = LSE_SOCK_SYS;
            return -1;
        }
        if (errno != EADDRINUSE && errno != EADDRNOTAVAIL) {
            chanClose_(ch);
            lserrno = LSE_SOCK_SYS;
            return -1;
        }

        if ((options & CHAN_OP_PPORT) && port < IPPORT_RESERVED/2)
            port = IPPORT_RESERVED - 1;
    }

    if ((options & CHAN_OP_PPORT) && i == IPPORT_RESERVED/2) {
        chanClose_(ch);
        lserrno = LSE_SOCK_SYS;
        return -1;
    }

    fcntl(s0, F_SETFD, (fcntl(s0, F_GETFD) | FD_CLOEXEC));

    return ch;
}

int
chanAccept_(int chfd, struct sockaddr_in *from)
{
    int s;
    socklen_t len = sizeof(struct sockaddr);

    if (channels[chfd].type != CH_TYPE_PASSIVE) {
        lserrno = LSE_INTERNAL;
        return -1;
    }

    s = accept(channels[chfd].handle, (struct sockaddr *) from, &len);
    if (SOCK_INVALID(s)) {
        lserrno = LSE_SOCK_SYS;
        return -1;
    }

    return chanOpenSock_(s, CHAN_OP_NONBLOCK);
}

void
chanInactivate_(int chfd)
{
    if (chfd < 0 || chfd > chanMaxSize)
        return;

    if (channels[chfd].state != CH_INACTIVE) {
        channels[chfd].prestate = channels[chfd].state;
        channels[chfd].state = CH_INACTIVE;
    }
}

void
chanActivate_(int chfd)
{
    if (chfd < 0 || chfd > chanMaxSize)
        return;

    if (channels[chfd].state == CH_INACTIVE) {
        channels[chfd].state = channels[chfd].prestate;
    }
}

int
chanConnect_(int chfd, struct sockaddr_in *peer, int timeout, int options)
{
    int cc;

    if (channels[chfd].state != CH_DISC) {
        lserrno = LSE_INTERNAL;
        return -1;
    }

    if (logclass & (LC_COMM | LC_TRACE))
        ls_syslog(LOG_DEBUG2,"chanConnect_: Connecting chan=%d to peer %s timeout %d",chfd, sockAdd2Str_(peer), timeout);

    if (channels[chfd].type == CH_TYPE_UDP) {
        cc = connect(channels[chfd].handle, (struct sockaddr *) peer,
                     sizeof(struct sockaddr_in));
        if (SOCK_CALL_FAIL(cc)) {
            lserrno = LSE_CONN_SYS;
            return -1;
        }
        channels[chfd].state = CH_CONN;
        return 0;
    }

    if (timeout >= 0) {
        if (b_connect_(channels[chfd].handle, (struct sockaddr *) peer,
                       sizeof(struct sockaddr_in), timeout/1000) < 0) {
            if (errno == ETIMEDOUT)
                lserrno = LSE_TIME_OUT;
            else
                lserrno = LSE_CONN_SYS;
            return -1;
        }
        channels[chfd].state = CH_CONN;
        return 0;
    }
    channels[chfd].state = CH_CONN;

    return 0;
}

int
chanSendDgram_(int chfd, char *buf, int len, struct sockaddr_in *peer)
{
    int s;
    int cc;

    s = channels[chfd].handle;

    if (logclass & (LC_COMM | LC_TRACE))
        ls_syslog(LOG_DEBUG3,"chanSendDgram_: Sending message size=%d peer=%s chan=%d",len,sockAdd2Str_(peer), chfd);

    if (channels[chfd].type != CH_TYPE_UDP) {
        lserrno = LSE_INTERNAL;
        return -1;
    }
    if (channels[chfd].state == CH_CONN)
        cc=send(s, buf, len, 0);
    else {
        cc=sendto(s, buf, len, 0, (struct sockaddr *)peer, sizeof(struct sockaddr_in));

    }

    if (SOCK_CALL_FAIL(cc)) {
        lserrno = LSE_MSG_SYS;
        return -1;
    }

    return 0;
}

int
chanRcvDgram_(int chfd, char *buf, int len, struct sockaddr_in *peer, int timeout)
{
    int sock;
    struct timeval timeval;
    struct timeval *time_ptr;
    int nReady, cc;
    socklen_t peersize = sizeof(struct sockaddr_in);

    sock = channels[chfd].handle;

    if (channels[chfd].type != CH_TYPE_UDP) {
        lserrno = LSE_INTERNAL;
        return -1;
    }

    if (logclass & (LC_COMM | LC_TRACE))
        ls_syslog(LOG_DEBUG2,"%s: Receive on chan %d timeout=%d",chfd, timeout);
    if (timeout < 0) {
        if (channels[chfd].state == CH_CONN)
            cc = recv(sock, buf, len, 0);
        else
            cc = recvfrom(sock, buf, len, 0, (struct sockaddr *)peer, &peersize);
        if (SOCK_CALL_FAIL(cc)) {
            lserrno = LSE_MSG_SYS;
            return -1;
        }
        return 0;
    }

    time_ptr = NULL;
    if (timeout > 0) {
        timeval.tv_sec  = timeout/1000;
        timeval.tv_usec = timeout - timeval.tv_sec*1000;
        time_ptr = &timeval;
    }

    for (;;) {
        nReady = rd_select_(sock, time_ptr);
        if (nReady < 0) {
            lserrno = LSE_SELECT_SYS;
            return -1;
        } else if (nReady == 0) {

            lserrno = LSE_TIME_OUT;
            return -1;
        } else {
            if (channels[chfd].state == CH_CONN)
                cc = recv(sock, buf, len, 0);
            else
                cc = recvfrom(sock, buf, len, 0, (struct sockaddr *)peer, &peersize);
            if (SOCK_CALL_FAIL(cc)) {
                if (channels[chfd].state == CH_CONN && (errno == ECONNREFUSED))
                    lserrno = LSE_LIM_DOWN;
                else
                    lserrno = LSE_MSG_SYS;
                return -1;
            }
            return 0;
        }
    }
    return 0;

}

// Open TCP channel
int
chanOpen_(u_int iaddr, u_short port, int options)
{
    int i;
    int cc;
    struct sockaddr_in addr;

    if ((i = findAFreeChannel()) < 0) {
        cherrno = CHANE_NOCHAN;
        return -1;
    }

    channels[i].type = CH_TYPE_TCP;

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    memcpy(&addr.sin_addr, &iaddr, sizeof(u_int));
    addr.sin_port = port;

    channels[i].handle = socket(AF_INET, SOCK_STREAM, SOCK_CLOEXEC);
    if (channels[i].handle < 0) {
        cherrno = CHANE_SYSCALL;
        return -1;
    }

    if (io_nonblock_(channels[i].handle) < 0) {
        CLOSEIT(i);
        cherrno = CHANE_SYSCALL;
        return -1;
    }

    if ((cc = connect(channels[i].handle,
                      (struct sockaddr *)&addr,
                      sizeof(addr))) < 0) {
        if (errno != EINPROGRESS)  {
            struct sockaddr laddr;
            socklen_t len = sizeof(laddr);

            ls_syslog(LOG_ERR,"chanOpen: connect() failed, cc=%d, errno=%d",
                      cc, errno);
            if (getsockname (channels[i].handle, &laddr, &len) == 0) {
                ls_syslog(LOG_ERR,"chanOpen: connect() failed, laddr=%s, addr=%s",
                          sockAdd2Str_((struct sockaddr_in *)&laddr),
                          sockAdd2Str_((struct sockaddr_in *)&addr));
            }

            CLOSEIT(i);
            cherrno = CHANE_SYSCALL;
            return -1;
        }
        channels[i].state = CH_PRECONN;
        return i;
    }
    channels[i].state = CH_CONN;
    channels[i].send  = newBuf();
    channels[i].recv  = newBuf();
    if (!channels[i].send || !channels[i].recv) {
        CLOSEIT(i);
        FREEUP(channels[i].send);
        FREEUP(channels[i].recv);
        lserrno = LSE_MALLOC;
        return -1;
    }
    return i;
}

int
chanOpenSock_(int s, int options)
{
    int i;

    if ((i = findAFreeChannel()) < 0) {
        lserrno = LSE_NO_CHAN;
        return -1;
    }

    if ((options & CHAN_OP_NONBLOCK) &&
        (io_nonblock_(s) < 0)) {
        lserrno = LSE_SOCK_SYS;
        return -1;
    }
    channels[i].type = CH_TYPE_TCP;
    channels[i].handle = s;
    channels[i].state = CH_CONN;

    if (options & CHAN_OP_RAW)
        return i;

    channels[i].send  = newBuf();
    channels[i].recv  = newBuf();
    if (!channels[i].send || !channels[i].recv) {
        CLOSEIT(i);
        FREEUP(channels[i].send);
        FREEUP(channels[i].recv);
        lserrno = LSE_MALLOC;
        return -1;
    }
    return i;
}

int
chanClose_(int chfd)
{
    struct Buffer *buf;
    struct Buffer *nextbuf;
    long maxfds;

    maxfds = sysconf(_SC_OPEN_MAX);

    if (chfd < 0 || chfd > maxfds) {
        lserrno = LSE_INTERNAL;
        return -1;
    }

    if (channels[chfd].handle < 0) {
        cherrno = CHANE_BADCHFD;
        return -1;
    }
    close(channels[chfd].handle);

    if (channels[chfd].send
        && channels[chfd].send != channels[chfd].send->forw) {
        for (buf = channels[chfd].send->forw;
             buf != channels[chfd].send; buf = nextbuf) {
            nextbuf = buf->forw;
            FREEUP(buf->data);
            FREEUP(buf);
        }
    }
    if (channels[chfd].recv
        && channels[chfd].recv != channels[chfd].recv->forw) {
        for (buf = channels[chfd].recv->forw;
             buf != channels[chfd].recv; buf = nextbuf) {
            nextbuf = buf->forw;
            FREEUP(buf->data);
            FREEUP(buf);
        }
    }
    FREEUP(channels[chfd].recv);
    FREEUP(channels[chfd].send);
    channels[chfd].state = CH_FREE;
    channels[chfd].handle = INVALID_HANDLE;
    channels[chfd].send  = NULL;
    channels[chfd].recv  = NULL;

    return 0;
}

void
chanCloseAll_(void)
{
    int i;

    for (i=0; i < chanIndex;  i++)
        if (channels[i].state != CH_FREE)
            chanClose_(i);

}

void
chanCloseAllBut_(int chfd)
{
    int i;

    for (i=0; i < chanIndex;  i++)
        if ((channels[i].state != CH_FREE) && (i != chfd))
            chanClose_(i);
}

int
chanSelect_(struct Masks *sockmask, struct Masks *chanmask, struct timeval *timeout)
{
    int i, nReady, maxfds;
    static char fname[] = "chanSelect_";

    FD_ZERO(&sockmask->wmask);
    FD_ZERO(&sockmask->emask);

    for(i=0; i < chanIndex; i++) {
        if (channels[i].state == CH_INACTIVE)
            continue;

        if (channels[i].handle == INVALID_HANDLE)
            continue;
        if (channels[i].state == CH_FREE) {
            ls_syslog(LOG_ERR,("%s: channel %d has socket %d but in %s state!"),
                      fname, i,channels[i].handle, "CH_FREE");
            continue;
        }

        if (channels[i].type == CH_TYPE_UDP &&
            channels[i].state != CH_WAIT)
            continue;

        if (logclass & LC_COMM)
            ls_syslog(LOG_DEBUG3,"chanSelect_: Considering channel %d handle %d state %d type %d", i, channels[i].handle, (int) channels[i].state, (int) channels[i].type);

        if (channels[i].type == CH_TYPE_TCP &&
            channels[i].state != CH_PRECONN &&
            !channels[i].recv &&
            !channels[i].send)
            continue;

        if (channels[i].state == CH_PRECONN) {
            FD_SET(channels[i].handle , &(sockmask->wmask));
            continue;
        }
        if (logclass & LC_COMM)
            ls_syslog(LOG_DEBUG3,"chanSelect_: Adding channel %d handle %d ", i,
                      channels[i].handle);
        FD_SET(channels[i].handle, &(sockmask->rmask));

        if (channels[i].type != CH_TYPE_UDP)
            FD_SET(channels[i].handle, &(sockmask->emask));

        if (channels[i].send &&
            channels[i].send->forw != channels[i].send)
            FD_SET(channels[i].handle, &(sockmask->wmask));
    }
    maxfds = FD_SETSIZE;

    nReady = select(maxfds,
                    &(sockmask->rmask),
                    &(sockmask->wmask),
                    &(sockmask->emask),
                    timeout);
    if (nReady <= 0) {
        return nReady;
    }

    FD_ZERO(&(chanmask->rmask));
    FD_ZERO(&(chanmask->wmask));
    FD_ZERO(&(chanmask->emask));
    for(i=0; i < chanIndex; i++) {
        if (channels[i].handle == INVALID_HANDLE)
            continue;

        if (FD_ISSET(channels[i].handle, &(sockmask->emask))) {
            ls_syslog(LOG_DEBUG, "chanSelect_: setting error mask for channel %d", channels[i].handle);
            FD_SET(i, &(chanmask->emask));
            continue;
        }
        if ((!channels[i].send || !channels[i].recv) &&
            (channels[i].state != CH_PRECONN) ) {
            if (FD_ISSET(channels[i].handle, &(sockmask->rmask)))
                FD_SET(i, &(chanmask->rmask));
            if (FD_ISSET(channels[i].handle, &(sockmask->wmask)))
                FD_SET(i, &(chanmask->wmask));
            continue;
        }

        if (channels[i].state == CH_PRECONN) {
            if (FD_ISSET(channels[i].handle, &(sockmask->wmask))) {
                channels[i].state = CH_CONN;
                channels[i].send  = newBuf();
                channels[i].recv  = newBuf();
                FD_SET(i, &(chanmask->wmask));
            }
        } else {
            if (FD_ISSET(channels[i].handle, &(sockmask->rmask))) {
                doread(i, chanmask);
                if (!FD_ISSET(i, &(chanmask->rmask)) &&
                    !FD_ISSET(i, &(chanmask->emask)) )
                    nReady--;
            }
            if ((channels[i].send->forw != channels[i].send) &&
                FD_ISSET(channels[i].handle, &(sockmask->wmask))) {
                dowrite(i, chanmask);
            }
            FD_SET(i, &(chanmask->wmask));
        }
        FD_CLR(channels[i].handle, &(sockmask->rmask));
        FD_CLR(channels[i].handle, &(sockmask->wmask));
        FD_CLR(channels[i].handle, &(sockmask->emask));
    }
    return nReady;
}

int
chanEnqueue_(int chfd, struct Buffer *msg)
{
    long maxfds;

    maxfds = sysconf(_SC_OPEN_MAX);
    maxfds = sysconf(_SC_OPEN_MAX);

    if (chfd < 0 || chfd > maxfds) {
        cherrno = CHANE_BADCHAN;
        return -1;
    }

    if (channels[chfd].handle == INVALID_HANDLE ||
        channels[chfd].state == CH_PRECONN) {
        cherrno = CHANE_NOTCONN;
        return -1;
    }

    enqueueTail_(msg, channels[chfd].send);
    return 0;
}

int
chanDequeue_(int chfd, struct Buffer **buf)
{
    long maxfds;

    maxfds = sysconf(_SC_OPEN_MAX);

    if (chfd < 0 || chfd > maxfds) {
        cherrno = CHANE_BADCHAN;
        return -1;
    }
    if (channels[chfd].handle == INVALID_HANDLE ||  channels[chfd].state == CH_PRECONN) {
        cherrno = CHANE_NOTCONN;
        return -1;
    }

    if (channels[chfd].recv->forw == channels[chfd].recv) {
        cherrno = CHANE_NOMSG;
        return -1;
    }
    *buf = channels[chfd].recv->forw;
    dequeue_(channels[chfd].recv->forw);
    return 0;
}

ssize_t
chanReadNonBlock_(int chfd, char *buf, int len, int timeout)
{
    static char fname[] = "chanReadNonBlock_";

    if (io_nonblock_(channels[chfd].handle) < 0) {
        lserrno = LSE_FILE_SYS;
        return -1;
    }

    return nb_read_timeout(channels[chfd].handle, buf, len, timeout);
}

ssize_t
chanRead_(int chfd, char *buf, int len)
{
    return b_read_fix(channels[chfd].handle, buf, len);

}

int
chanWrite_(int chfd, char *buf, int len)
{

    return (b_write_fix(channels[chfd].handle, buf, len));

}

int
chanRpc_(int chfd, struct Buffer *in, struct Buffer *out,
         struct packet_header *outhdr, int timeout)
{
    static char fname[]="chanRpc_";
    XDR xdrs;
    struct packet_header hdrBuf;
    struct timeval timeval, *timep=NULL;
    int cc;

    if (logclass & LC_COMM)
        ls_syslog(LOG_DEBUG1, "%s: Entering ... chfd=%d", fname, chfd);

    if (in) {
        if (logclass & LC_COMM)
            ls_syslog(LOG_DEBUG1,"%s: sending %d bytes", fname, in->len);
        if (chanWrite_(chfd, in->data, in->len ) != in->len)
            return -1;

        if (in->forw != NULL) {
            struct Buffer *buf=in->forw;
            int nlen = htonl(buf->len);

            if (logclass & LC_COMM)
                ls_syslog(LOG_DEBUG1,"%s: sending %d extra bytes", fname, nlen);
            if (chanWrite_(chfd, NET_INTADDR_(&nlen), NET_INTSIZE_)
                != NET_INTSIZE_)
                return -1;

            if (chanWrite_(chfd, buf->data, buf->len ) != buf->len)
                return -1;
        }
    }

    if (!out) {
        return 0;
    }

    if (logclass & LC_COMM)
        ls_syslog(LOG_DEBUG2,"%s: waiting for reply timeout=%d ms", fname,timeout);
    if (timeout > 0) {
        timeval.tv_sec = timeout/1000;
        timeval.tv_usec = timeout - timeval.tv_sec*1000;
        timep = &timeval;
    }
    if ((cc=rd_select_(channels[chfd].handle, timep)) <= 0) {
        if (cc == 0)
            lserrno = LSE_TIME_OUT;
        else
            lserrno = LSE_SELECT_SYS;
        return -1;
    }

    if (logclass & LC_COMM)
        ls_syslog(LOG_DEBUG2,"%s: reading reply header", fname);
    xdrmem_create(&xdrs, (char *)&hdrBuf, PACKET_HEADER_SIZE,  XDR_DECODE);
    cc = readDecodeHdr_(chfd,
                        (char *)&hdrBuf,
                        chanRead_,
                        &xdrs,
                        outhdr);
    if (cc < 0) {
        xdr_destroy(&xdrs);
        return -1;
    }
    xdr_destroy(&xdrs);

#define MAXMSGLEN     (1<<28)
    if (outhdr->length > MAXMSGLEN) {
        lserrno = LSE_PROTOCOL;
        return -1;
    }

    if (logclass & LC_COMM)
        ls_syslog(LOG_DEBUG2,"%s: reading reply size=%d", fname, outhdr->length);
    out->len = outhdr->length;
    if (out->len > 0) {
        if ((out->data = malloc(out->len)) == NULL) {
            lserrno = LSE_MALLOC;
            return -1;
        }

        if ((cc=chanRead_(chfd, out->data, out->len)) != out->len) {
            FREEUP(out->data);
            if (logclass & LC_COMM)
                ls_syslog(LOG_DEBUG2,"%s: read only %d bytes", fname,cc);
            lserrno = LSE_MSG_SYS;
            return -1;
        }
    } else
        out->data = NULL;

    if (logclass & LC_COMM)
        ls_syslog(LOG_DEBUG1, "%s: Leaving...repy_size=%d", fname, out->len);
    return 0;
}

int
chanSock_(int chfd)
{
    if (chfd < 0 || chfd > chanAllocSize) {
        lserrno = LSE_BAD_CHAN;
        return -1;
    }

    return(channels[chfd].handle);
}

int
chanSetMode_(int chfd, int mode)
{
    if (chfd < 0 || chfd > chanAllocSize) {
        lserrno = LSE_BAD_CHAN;
        return -1;
    }

    if (mode != CHAN_MODE_NONBLOCK
        &&  mode != CHAN_MODE_BLOCK) {
        lserrno = LSE_SOCK_SYS;
        return -1;
    }

    if (channels[chfd].state == CH_FREE
        || channels[chfd].handle == INVALID_HANDLE) {
        lserrno = LSE_BAD_CHAN;
        return -1;
    }

    if (mode == CHAN_MODE_NONBLOCK) {
        if (io_nonblock_(channels[chfd].handle) < 0) {
            lserrno = LSE_SOCK_SYS;
            return -1;
        }
        if (!channels[chfd].send)
            channels[chfd].send  = newBuf();
        if (!channels[chfd].recv)
            channels[chfd].recv  = newBuf();
        if (!channels[chfd].send || !channels[chfd].recv) {
            lserrno = LSE_MALLOC;
            return -1;
        }
        return 0;
    }

    if (io_block_(channels[chfd].handle) < 0) {
        lserrno = LSE_SOCK_SYS;
        return -1;
    }
    // we could abort() here
    return 0;
}

static void
doread(int chfd, struct Masks *chanmask)
{
    struct Buffer *rcvbuf;
    int cc;

    if (channels[chfd].recv->forw == channels[chfd].recv) {
        rcvbuf = newBuf();
        if(!rcvbuf) {
            FD_SET(chfd, &(chanmask->emask));
            channels[chfd].chanerr = LSE_MALLOC;
            return;
        }
        enqueueTail_(rcvbuf, channels[chfd].recv);
    } else
        rcvbuf = channels[chfd].recv->forw;

    if (!rcvbuf->len) {
        rcvbuf->data =  malloc(PACKET_HEADER_SIZE);
        if (!rcvbuf->data) {
            FD_SET(chfd, &(chanmask->emask));
            channels[chfd].chanerr = LSE_MALLOC;
            return;
        }
        rcvbuf->len = PACKET_HEADER_SIZE;
        rcvbuf->pos = 0;
    }

    if (rcvbuf->pos == rcvbuf->len) {
        FD_SET(chfd, &(chanmask->rmask));
        return;
    }

    errno = 0;

    cc = read(channels[chfd].handle, rcvbuf->data + rcvbuf->pos,
              rcvbuf->len - rcvbuf->pos);
   if (cc < 0) {
       // transient, try again later
       if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
           return;
       FD_SET(chfd, &chanmask->emask);
       channels[chfd].chanerr = CHANE_CONNRESET;
       return;
   }
   // EOF on the line
   if (cc == 0) {
       FD_SET(chfd, &chanmask->emask);
       channels[chfd].chanerr = CHANE_CONNRESET;
       return;
   }

    rcvbuf->pos += cc;

    if ((rcvbuf->len == PACKET_HEADER_SIZE) && (rcvbuf->pos == rcvbuf->len )) {
        XDR xdrs;
        struct packet_header hdr;
        char *newdata;

        xdrmem_create(&xdrs, rcvbuf->data, PACKET_HEADER_SIZE, XDR_DECODE);
        if (!xdr_LSFHeader(&xdrs, &hdr)) {
            FD_SET(chfd, &(chanmask->emask));
            channels[chfd].chanerr = CHANE_BADHDR;
            xdr_destroy(&xdrs);
            return;
        }

        if (hdr.length) {
            rcvbuf->len = hdr.length + PACKET_HEADER_SIZE;
            newdata = realloc(rcvbuf->data, rcvbuf->len);
            if (!newdata) {
                FD_SET(chfd, &(chanmask->emask));
                channels[chfd].chanerr = LSE_MALLOC;
                xdr_destroy(&xdrs);
                return;
            }
            rcvbuf->data = newdata;
        }

        xdr_destroy(&xdrs);
    }

    if (rcvbuf->pos == rcvbuf->len) {
        FD_SET(chfd, &(chanmask->rmask));
    }

    return;

}

static void
dowrite(int chfd, struct Masks *chanmask)
{
    struct Buffer *sendbuf;
    int cc;

    if (channels[chfd].send->forw == channels[chfd].send)
        return;
    else
        sendbuf = channels[chfd].send->forw;

    cc = write(channels[chfd].handle, sendbuf->data + sendbuf->pos,
               sendbuf->len - sendbuf->pos);

    if (cc < 0) {
        // transient, wait for writable again
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
            return;
        // real error
        FD_SET(chfd, &(chanmask->emask));
        channels[chfd].chanerr = LSE_MSG_SYS;
        return;
    }

    //* nothing written, but not an error
    if (cc == 0)
        return;

    sendbuf->pos += cc;
    if (sendbuf->pos == sendbuf->len) {
        dequeue_(sendbuf);
        free(sendbuf->data);
        free(sendbuf);
    }
    return;
}

static struct Buffer *
newBuf(void)
{
    struct Buffer *newbuf;

    newbuf = (struct Buffer *)malloc(sizeof(struct Buffer));
    if (!newbuf)
        return NULL;
    newbuf->forw = newbuf->back = newbuf;
    newbuf->len  = newbuf->pos = 0;
    newbuf->data = NULL;
    newbuf->stashed = false;

    return newbuf;
}

int
chanAllocBuf_(struct Buffer **buf, int size)
{
    *buf = newBuf();
    if (!*buf)
        return -1;
    (*buf)->data = malloc(size);
    if ((*buf)->data == NULL) {
        free(*buf);
        return -1;
    }

    memset((*buf)->data, 0, size);
    return 0;
}

int
chanFreeBuf_(struct Buffer *buf)
{
    if (buf) {
        if (buf->stashed) return 0;

        if (buf->data)
            free(buf->data);

        free(buf);
    }
    return 0;
}

int
chanFreeStashedBuf_(struct Buffer *buf)
{
    if (buf) {
        buf->stashed = false;
        return(chanFreeBuf_(buf));
    }
    return -1;
}

static void
dequeue_(struct Buffer *entry)
{
    entry->back->forw = entry->forw;
    entry->forw->back = entry->back;
}

static void
enqueueTail_(struct Buffer *entry, struct Buffer *pred)
{
    entry->back = pred->back;
    entry->forw = pred;
    pred->back->forw = entry;
    pred->back  = entry;
}

static int
findAFreeChannel(void)
{
    int i;

    if (growchanDataArray() < 0) {
        return -1;
    }

    i = 0;
    if (chanIndex != 0) {
        for(i = 0; i < chanIndex; i++) {
            if (channels[i].handle == INVALID_HANDLE)
                break;
        }
    }

    if (i == chanIndex)
        chanIndex++;

    if (i == chanMaxSize) {
        chanIndex = chanMaxSize;
        return -1;
    }

    channels[i].handle = INVALID_HANDLE;
    channels[i].state = CH_FREE;
    channels[i].send  = NULL;
    channels[i].recv = NULL;
    channels[i].chanerr = CHANE_NOERR;

    return i;
}

static int
growchanDataArray(void)
{
    if (chanAllocSize == chanIndex) {
        channels = realloc(channels, (sizeof(struct chanData)
                                      * (chanGrowSize + chanAllocSize)));
        if (channels == NULL)
            return -1;

        chanAllocSize += chanGrowSize;
    }
    return 0;
}

// If socket happens to be 0,1 or 2 dup it to a higher number
int
ll_dup_stdio(int fd)
{
    if (fd >= 3)
        return fd;

    int newfd = fcntl(fd, F_DUPFD, 3);
    if (newfd < 0)
        return -1;

    close(fd);
    return newfd;
}

int
io_nonblock_(int s)
{
    int flags = fcntl(s, F_GETFL, 0);
    if (flags < 0)
        return -1;
    return fcntl(s, F_SETFL, flags | O_NONBLOCK);
}

int
io_block_(int s)
{
    int flags = fcntl(s, F_GETFL, 0);
    return fcntl(s, F_SETFL, (flags & ~O_NONBLOCK));
}
