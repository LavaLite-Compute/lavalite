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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 USA
 *
 */
#include "lsf/lib/lib.channel.h"

// LIM protocol max frame
#define MAXMSGLEN LL_BUFSIZ_4K


static long chan_open_max;
static __thread struct chan_data *channels;

static void doread(int, struct Masks *);
static void dowrite(int, struct Masks *);
static struct Buffer *make_buf(void);
static void enqueueTail_(struct Buffer *, struct Buffer *);
static void dequeue_(struct Buffer *);
static int chan_find_free(void);

int chan_init(void)
{
    if (channels)
        return 0;

    chan_open_max = sysconf(_SC_OPEN_MAX);
    if (chan_open_max < 0)
        return -1;

    channels = calloc(chan_open_max, sizeof(struct chan_data));
    if (channels == NULL)
        return -1;

    for (int i = 0; i < chan_open_max; i++)
        channels[i].sock = -1;

    return 0;
}

int chan_listen_socket(int type, u_short port, int backlog, int options)
{
    int ch, s;
    struct sockaddr_in sin;

    if ((ch = chan_find_free()) < 0) {
        lserrno = LSE_NO_CHAN;
        return -1;
    }

    s = socket(AF_INET, type, 0);
    if (s < 0) {
        lserrno = LSE_SOCK_SYS;
        return -1;
    }

    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);
    sin.sin_addr.s_addr = htonl(INADDR_ANY);

    if (options & CHAN_OP_SOREUSE) {
        int one = 1;
        setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char *)&one, sizeof(int));
    }

    if (bind(s, (struct sockaddr *) &sin, sizeof(sin)) < 0) {
        close(s);
        lserrno = LSE_SOCK_SYS;
        return -2;
    }

    if (backlog > 0) {
        if (listen(s, backlog) < 0) {
            close(s);
            lserrno = LSE_SOCK_SYS;
            return -3;
        }
    }

    channels[ch].sock = s;
    channels[ch].state = CH_WAIT;
    if (type == SOCK_DGRAM)
        channels[ch].type = CH_TYPE_UDP;
    else
        channels[ch].type = CH_TYPE_PASSIVE;
    return ch;
}

int chan_client_socket(int domain, int type, int options)
{
    if (domain != AF_INET) {
        lserrno = LSE_INTERNAL;
        return -1;
    }

    int ch = chan_find_free();
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
    channels[ch].sock = s;
    int s0 = ll_dup_stdio(s);
    if (s0 < 0) {
        close(s);
        close(channels[ch].sock);
        channels[ch].state = CH_DISC;
        channels[ch].sock = -1;
        lserrno = LSE_SOCK_SYS;
        return -1;
    }
    channels[ch].sock = s0;

    fcntl(s0, F_SETFD, (fcntl(s0, F_GETFD) | FD_CLOEXEC));

    return ch;
}

int chan_accept(int ch_id, struct sockaddr_in *from)
{
    int s;
    socklen_t len = sizeof(struct sockaddr);

    if (channels[ch_id].type != CH_TYPE_PASSIVE) {
        lserrno = LSE_INTERNAL;
        return -1;
    }

    s = accept4(channels[ch_id].sock, (struct sockaddr *) from, &len,
                SOCK_NONBLOCK | SOCK_CLOEXEC);
    if (s < 0) {
        lserrno = LSE_SOCK_SYS;
        return -1;
    }

    return chan_open_sock(s, CHAN_OP_NONBLOCK);
}

int chan_connect(int ch_id, struct sockaddr_in *peer, int timeout, int options)
{
    int cc;

    (void) options; // unused for now

    if (channels[ch_id].state != CH_DISC) {
        lserrno = LSE_INTERNAL;
        return -1;
    }

    if (peer == NULL) {
        lserrno = LSE_BAD_ARGS;
        return -1;
    }

    if (channels[ch_id].type == CH_TYPE_UDP) {
        // No more connected UDP; use chan_send_dgram/chan_recv_dgram_ instead.
        lserrno = LSE_BAD_ARGS;
        return -1;
    }

    if (timeout < 0) {
        // Old "just set CH_CONN" behavior is gone; this is now invalid.
        lserrno = LSE_BAD_ARGS;
        return -1;
    }

    cc = b_connect_(channels[ch_id].sock, (struct sockaddr *) peer,
                    sizeof(struct sockaddr_in), timeout);
    if (cc < 0) {
        if (errno == ETIMEDOUT)
            lserrno = LSE_TIME_OUT;
        else
            lserrno = LSE_CONN_SYS;
        return -1;
    }

    channels[ch_id].state = CH_CONN;

    return 0;
}

int chan_send_dgram(int ch_id, char *buf, size_t len, struct sockaddr_in *peer)
{
    int s = channels[ch_id].sock;
    if (channels[ch_id].type != CH_TYPE_UDP) {
        lserrno = LSE_INTERNAL;
        return -1;
    }

    ssize_t cc = sendto(s, buf, len, 0, (struct sockaddr *) peer,
                        sizeof(struct sockaddr_in));
    if (cc < 0) {
        lserrno = LSE_MSG_SYS;
        return -1;
    }

    return 0;
}

int chan_recv_dgram_(int ch_id, void *buf, size_t len,
                     struct sockaddr_storage *peer, int timeout)
{
    int sock;
    struct timeval timeval;
    struct timeval *time_ptr;
    int nReady, cc;
    socklen_t peersize = sizeof(struct sockaddr_storage);

    sock = channels[ch_id].sock;

    if (channels[ch_id].type != CH_TYPE_UDP) {
        lserrno = LSE_INTERNAL;
        return -1;
    }

    if (timeout < 0) {
        if (channels[ch_id].state == CH_CONN)
            cc = recv(sock, buf, len, 0);
        else
            cc = recvfrom(sock, buf, len, 0, (struct sockaddr *) peer,
                          &peersize);
        if (cc < 0) {
            lserrno = LSE_MSG_SYS;
            return -1;
        }
        return 0;
    }

    time_ptr = NULL;
    if (timeout > 0) {
        timeval.tv_sec = timeout / 1000;
        timeval.tv_usec = timeout - timeval.tv_sec * 1000;
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
            if (channels[ch_id].state == CH_CONN)
                cc = recv(sock, buf, len, 0);
            else
                cc = recvfrom(sock, buf, len, 0, (struct sockaddr *) peer,
                              &peersize);
            if (cc < 0) {
                if (channels[ch_id].state == CH_CONN && (errno == ECONNREFUSED))
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

int chan_open_sock(int s, int options)
{
    int i;

    if ((i = chan_find_free()) < 0) {
        lserrno = LSE_NO_CHAN;
        return -1;
    }

    if ((options & CHAN_OP_NONBLOCK) && (io_nonblock_(s) < 0)) {
        lserrno = LSE_SOCK_SYS;
        return -1;
    }

    channels[i].type = CH_TYPE_TCP;
    channels[i].sock = s;
    channels[i].state = CH_CONN;

    if (options & CHAN_OP_RAW)
        return i;

    channels[i].send = make_buf();
    channels[i].recv = make_buf();
    if (!channels[i].send || !channels[i].recv) {
        chan_close(i);
        lserrno = LSE_MALLOC;
        return -1;
    }
    return i;
}

int chan_close(int ch_id)
{
    struct Buffer *buf;
    struct Buffer *nextbuf;

    if (ch_id < 0 || ch_id > chan_open_max) {
        lserrno = LSE_BAD_CHAN;
        return -1;
    }

    if (channels[ch_id].sock < 0) {
        lserrno = LSE_BAD_CHAN;
        return -1;
    }
    close(channels[ch_id].sock);

    if (channels[ch_id].send) {
        for (buf = channels[ch_id].send->forw; buf != channels[ch_id].send;
             buf = nextbuf) {
            nextbuf = buf->forw;
            FREEUP(buf->data);
            FREEUP(buf);
        }
        free(channels[ch_id].send);
    }
    if (channels[ch_id].recv
        && channels[ch_id].recv != channels[ch_id].recv->forw) {
        for (buf = channels[ch_id].recv->forw; buf != channels[ch_id].recv;
             buf = nextbuf) {
            nextbuf = buf->forw;
            FREEUP(buf->data);
            FREEUP(buf);
        }
        free(channels[ch_id].recv);
    }
    channels[ch_id].state = CH_FREE;
    channels[ch_id].sock = -1;
    channels[ch_id].send = channels[ch_id].recv = NULL;

    return 0;
}

int chanSelect_(struct Masks *sockmask, struct Masks *chanmask,
                struct timeval *timeout)
{
    int i, nReady, maxfds;
    static char fname[] = "chanSelect_";

    FD_ZERO(&sockmask->wmask);
    FD_ZERO(&sockmask->emask);

    // Bug this is to make the code compile during dev
    int chanIndex = chan_open_max;
    for (i = 0; i < chanIndex; i++) {
        if (channels[i].state == CH_INACTIVE)
            continue;

        if (channels[i].sock == -1)
            continue;
        if (channels[i].state == CH_FREE) {
            ls_syslog(LOG_ERR,
                      ("%s: channel %d has socket %d but in %s state!"), fname,
                      i, channels[i].sock, "CH_FREE");
            continue;
        }

        if (channels[i].type == CH_TYPE_UDP && channels[i].state != CH_WAIT)
            continue;

        if (logclass & LC_COMM)
            ls_syslog(LOG_DEBUG3,
                      "chanSelect_: Considering channel %d sock %d state %d "
                      "type %d",
                      i, channels[i].sock, (int) channels[i].state,
                      (int) channels[i].type);

        if (channels[i].type == CH_TYPE_TCP &&
            channels[i].state != CH_PRECONN && !channels[i].recv &&
            !channels[i].send)
            continue;

        if (channels[i].state == CH_PRECONN) {
            FD_SET(channels[i].sock, &(sockmask->wmask));
            continue;
        }
        if (logclass & LC_COMM)
            ls_syslog(LOG_DEBUG3, "chanSelect_: Adding channel %d sock %d ",
                      i, channels[i].sock);
        FD_SET(channels[i].sock, &(sockmask->rmask));

        if (channels[i].type != CH_TYPE_UDP)
            FD_SET(channels[i].sock, &(sockmask->emask));

        if (channels[i].send && channels[i].send->forw != channels[i].send)
            FD_SET(channels[i].sock, &(sockmask->wmask));
    }
    maxfds = FD_SETSIZE;

    nReady = select(maxfds, &(sockmask->rmask), &(sockmask->wmask),
                    &(sockmask->emask), timeout);
    if (nReady <= 0) {
        return nReady;
    }

    FD_ZERO(&(chanmask->rmask));
    FD_ZERO(&(chanmask->wmask));
    FD_ZERO(&(chanmask->emask));
    for (i = 0; i < chanIndex; i++) {
        if (channels[i].sock == -1)
            continue;

        if (FD_ISSET(channels[i].sock, &(sockmask->emask))) {
            ls_syslog(LOG_DEBUG,
                      "chanSelect_: setting error mask for channel %d",
                      channels[i].sock);
            FD_SET(i, &(chanmask->emask));
            continue;
        }
        if ((!channels[i].send || !channels[i].recv) &&
            (channels[i].state != CH_PRECONN)) {
            if (FD_ISSET(channels[i].sock, &(sockmask->rmask)))
                FD_SET(i, &(chanmask->rmask));
            if (FD_ISSET(channels[i].sock, &(sockmask->wmask)))
                FD_SET(i, &(chanmask->wmask));
            continue;
        }

        if (channels[i].state == CH_PRECONN) {
            if (FD_ISSET(channels[i].sock, &(sockmask->wmask))) {
                channels[i].state = CH_CONN;
                channels[i].send = make_buf();
                channels[i].recv = make_buf();
                FD_SET(i, &(chanmask->wmask));
            }
        } else {
            if (FD_ISSET(channels[i].sock, &(sockmask->rmask))) {
                doread(i, chanmask);
                if (!FD_ISSET(i, &(chanmask->rmask)) &&
                    !FD_ISSET(i, &(chanmask->emask)))
                    nReady--;
            }
            if ((channels[i].send->forw != channels[i].send) &&
                FD_ISSET(channels[i].sock, &(sockmask->wmask))) {
                dowrite(i, chanmask);
            }
            FD_SET(i, &(chanmask->wmask));
        }
        FD_CLR(channels[i].sock, &(sockmask->rmask));
        FD_CLR(channels[i].sock, &(sockmask->wmask));
        FD_CLR(channels[i].sock, &(sockmask->emask));
    }
    return nReady;
}

int chan_enqueue(int ch_id, struct Buffer *msg)
{
    if (ch_id < 0 || ch_id > chan_open_max) {
        lserrno = LSE_BAD_CHAN;
        return -1;
    }

    if (channels[ch_id].sock == -1
        || channels[ch_id].state == CH_PRECONN) {
        lserrno = LSE_NO_CHAN;
        return -1;
    }

    enqueueTail_(msg, channels[ch_id].send);
    return 0;
}

int chan_dequeue(int ch_id, struct Buffer **buf)
{

    if (ch_id < 0 || ch_id > chan_open_max) {
        lserrno = LSE_BAD_CHAN;
        return -1;
    }
    if (channels[ch_id].sock == -1 ||
        channels[ch_id].state == CH_PRECONN) {
        lserrno = LSE_NO_CHAN;
        return -1;
    }

    if (channels[ch_id].recv->forw == channels[ch_id].recv) {
        lserrno = LSE_BAD_CHAN;
        return -1;
    }
    *buf = channels[ch_id].recv->forw;
    dequeue_(channels[ch_id].recv->forw);
    return 0;
}

ssize_t chan_read_nonblock(int ch_id, char *buf, int len, int timeout)
{
    if (io_nonblock_(channels[ch_id].sock) < 0) {
        lserrno = LSE_FILE_SYS;
        return -1;
    }

    return nb_read_timeout(channels[ch_id].sock, buf, len, timeout);
}

ssize_t chan_read(int ch_id, void *buf, size_t len)
{
    return b_read_fix(channels[ch_id].sock, buf, len);
}

ssize_t chan_write(int ch_id, void *buf, size_t len)
{
    return (b_write_fix(channels[ch_id].sock, buf, len));
}

int chan_rpc(int ch_id, struct Buffer *in, struct Buffer *out,
             struct packet_header *out_hdr, int timeout)
{
    if (in) {
        if (chan_write(ch_id, in->data, in->len) != in->len)
            return -1;

        if (in->forw != NULL) {
            struct Buffer *buf = in->forw;
            int nlen = htonl(buf->len);

            if (chan_write(ch_id, (void *) &nlen, NET_INTSIZE_) != NET_INTSIZE_)
                return -1;

            if (chan_write(ch_id, buf->data, buf->len) != buf->len)
                return -1;
        }
    }

    if (!out) {
        return 0;
    }
    // poll wants milliseconds
    int cc = rd_poll(channels[ch_id].sock, timeout * 1000);
    if (cc <= 0) {
        if (cc == 0)
            lserrno = LSE_TIME_OUT;
        else
            lserrno = LSE_SELECT_SYS;
        return -1;
    }

    cc = recv_packet_header(ch_id, out_hdr);
    if (cc < 0) {
        return -1;
    }

    // Bug what is this for... auth the socket?
    if (out_hdr->length > MAXMSGLEN) {
        lserrno = LSE_PROTOCOL;
        return -1;
    }

    out->data = NULL;
    out->len = out_hdr->length;
    if (out->len > 0) {
        if ((out->data = malloc(out->len)) == NULL) {
            lserrno = LSE_MALLOC;
            return -1;
        }

        if ((cc = chan_read(ch_id, out->data, out->len)) != out->len) {
            FREEUP(out->data);
            lserrno = LSE_MSG_SYS;
            return -1;
        }
    }

    return 0;
}

int chan_get_sock(int ch_id)
{
    if (ch_id < 0 || ch_id > chan_open_max) {
        lserrno = LSE_BAD_CHAN;
        return -1;
    }

    return (channels[ch_id].sock);
}

int chan_set_mode(int ch_id, int mode)
{
    if (ch_id < 0 || ch_id > chan_open_max) {
        lserrno = LSE_BAD_CHAN;
        return -1;
    }

    if (mode != CHAN_MODE_NONBLOCK && mode != CHAN_MODE_BLOCK) {
        lserrno = LSE_SOCK_SYS;
        return -1;
    }

    if (channels[ch_id].state == CH_FREE
        || channels[ch_id].sock == -1) {
        lserrno = LSE_BAD_CHAN;
        return -1;
    }

    if (mode == CHAN_MODE_NONBLOCK) {
        if (io_nonblock_(channels[ch_id].sock) < 0) {
            lserrno = LSE_SOCK_SYS;
            return -1;
        }
        if (!channels[ch_id].send)
            channels[ch_id].send = make_buf();
        if (!channels[ch_id].recv)
            channels[ch_id].recv = make_buf();
        if (!channels[ch_id].send || !channels[ch_id].recv) {
            lserrno = LSE_MALLOC;
            return -1;
        }
        return 0;
    }

    if (io_block(channels[ch_id].sock) < 0) {
        lserrno = LSE_SOCK_SYS;
        return -1;
    }
    // we could abort() here
    return 0;
}

static void doread(int ch_id, struct Masks *chanmask)
{
    struct Buffer *rcvbuf;
    int cc;

    if (channels[ch_id].recv->forw == channels[ch_id].recv) {
        rcvbuf = make_buf();
        if (!rcvbuf) {
            FD_SET(ch_id, &(chanmask->emask));
            return;
        }
        enqueueTail_(rcvbuf, channels[ch_id].recv);
    } else
        rcvbuf = channels[ch_id].recv->forw;

    if (!rcvbuf->len) {
        rcvbuf->data = malloc(PACKET_HEADER_SIZE);
        if (! rcvbuf->data) {
            FD_SET(ch_id, &(chanmask->emask));
            return;
        }
        rcvbuf->len = PACKET_HEADER_SIZE;
        rcvbuf->pos = 0;
    }

    if (rcvbuf->pos == rcvbuf->len) {
        FD_SET(ch_id, &(chanmask->rmask));
        return;
    }

    errno = 0;

    cc = read(channels[ch_id].sock, rcvbuf->data + rcvbuf->pos,
              rcvbuf->len - rcvbuf->pos);
    if (cc < 0) {
        // transient, try again later
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
            return;
        FD_SET(ch_id, &chanmask->emask);
        return;
    }
    // EOF on the line
    if (cc == 0) {
        FD_SET(ch_id, &chanmask->emask);
        return;
    }

    rcvbuf->pos += cc;

    if ((rcvbuf->len == PACKET_HEADER_SIZE) && (rcvbuf->pos == rcvbuf->len)) {
        XDR xdrs;
        struct packet_header hdr;
        char *newdata;

        xdrmem_create(&xdrs, rcvbuf->data, PACKET_HEADER_SIZE, XDR_DECODE);
        if (!xdr_pack_hdr(&xdrs, &hdr)) {
            FD_SET(ch_id, &(chanmask->emask));
            xdr_destroy(&xdrs);
            return;
        }

        if (hdr.length) {
            rcvbuf->len = hdr.length + PACKET_HEADER_SIZE;
            newdata = realloc(rcvbuf->data, rcvbuf->len);
            if (!newdata) {
                FD_SET(ch_id, &(chanmask->emask));
                xdr_destroy(&xdrs);
                return;
            }
            rcvbuf->data = newdata;
        }

        xdr_destroy(&xdrs);
    }

    if (rcvbuf->pos == rcvbuf->len) {
        FD_SET(ch_id, &(chanmask->rmask));
    }

    return;
}

static void dowrite(int ch_id, struct Masks *chanmask)
{
    struct Buffer *sendbuf;
    int cc;

    if (channels[ch_id].send->forw == channels[ch_id].send)
        return;
    else
        sendbuf = channels[ch_id].send->forw;

    cc = write(channels[ch_id].sock, sendbuf->data + sendbuf->pos,
               sendbuf->len - sendbuf->pos);

    if (cc < 0) {
        // transient, wait for writable again
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
            return;
        // real error
        FD_SET(ch_id, &(chanmask->emask));
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

static struct Buffer *make_buf(void)
{
    struct Buffer *newbuf;

    newbuf = calloc(1, sizeof(struct Buffer));
    if (!newbuf)
        return NULL;

    newbuf->forw = newbuf->back = newbuf;

    return newbuf;
}

int chan_alloc_buf(struct Buffer **buf, int size)
{
    // make new buffer and initialize its linked list
    *buf = make_buf();
    if (!*buf)
        return -1;

    (*buf)->data = calloc(size, sizeof(char));
    if ((*buf)->data == NULL) {
        free(*buf);
        return -1;
    }

    return 0;
}

int chan_free_buf(struct Buffer *buf)
{
    if (! buf)
        return 0;
    if (buf->data)
        free(buf->data);

    free(buf);

    return 0;
}

static void dequeue_(struct Buffer *entry)
{
    entry->back->forw = entry->forw;
    entry->forw->back = entry->back;
}

static void enqueueTail_(struct Buffer *entry, struct Buffer *pred)
{
    entry->back = pred->back;
    entry->forw = pred;
    pred->back->forw = entry;
    pred->back = entry;
}

static int chan_find_free(void)
{
    for (int i = 0; i < chan_open_max; i++) {
        if (channels[i].sock == -1) {
            channels[i].state = CH_FREE;
            channels[i].send = NULL;
            channels[i].recv = NULL;
            return i;
        }
    }
    return -1;
}


// If socket happens to be 0,1 or 2 dup it to a higher number
int ll_dup_stdio(int fd)
{
    if (fd >= 3)
        return fd;

    int newfd = fcntl(fd, F_DUPFD, 3);
    if (newfd < 0)
        return -1;

    close(fd);
    return newfd;
}

int io_nonblock_(int s)
{
    int flags = fcntl(s, F_GETFL, 0);
    if (flags < 0)
        return -1;
    return fcntl(s, F_SETFL, flags | O_NONBLOCK);
}

int io_block(int s)
{
    int flags = fcntl(s, F_GETFL, 0);
    return fcntl(s, F_SETFL, (flags & ~O_NONBLOCK));
}
