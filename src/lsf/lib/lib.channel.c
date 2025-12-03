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
#include "lsf/lib/lib.channel.h"

// LIM protocol max frame
#define MAXMSGLEN LL_BUFSIZ_4K

long chan_open_max;
struct chan_data *channels;

static void doread(struct chan_data *);
static void dowrite(struct chan_data *);
static struct Buffer *make_buf(void);
static void enqueueTail_(struct Buffer *, struct Buffer *);
static void dequeue_(struct Buffer *);
static int chan_find_free(void);
static inline bool_t chan_is_udp(enum chanType);
static inline bool_t chan_is_valid(int);

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

    for (int i = 0; i < chan_open_max; i++) {
        channels[i].sock = -1;
        channels[i].chan_events = CHAN_EPOLLNONE;
    }

    return 0;
}

int
chan_sock(int ch_id)
{
    if (ch_id < 0 || ch_id > chan_open_max) {
        lserrno = LSE_BAD_CHAN;
        return -1;
    }

    return channels[ch_id].sock;
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

    channels[ch].sock = s;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);
    sin.sin_addr.s_addr = htonl(INADDR_ANY);

    if (options & CHAN_OP_SOREUSE) {
        int one = 1;
        setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char *)&one, sizeof(int));
    }

    if (bind(s, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
        close(s);
        lserrno = LSE_SOCK_SYS;
        return -2;
    }

    if (type == SOCK_STREAM) {
        channels[ch].type = CHAN_TYPE_TCP_LISTEN;
        if (listen(s, backlog) < 0) {
            close(s);
            lserrno = LSE_SOCK_SYS;
            return -3;
        }
    } else {
        channels[ch].type = CHAN_TYPE_UDP_LISTEN;
    }

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

    int s = socket(domain, type|SOCK_CLOEXEC, 0);
    if (s < 0) {
        lserrno = LSE_SOCK_SYS;
        return -1;
    }

    channels[ch].sock = s;
    if (type == SOCK_STREAM)
        channels[ch].type = CHAN_TYPE_TCP_CLIENT;
    else
        channels[ch].type = CHAN_TYPE_UDP_CLIENT;

    return ch;
}

int chan_accept(int ch_id, struct sockaddr_in *from)
{
    int s;
    socklen_t len = sizeof(struct sockaddr);

    if (channels[ch_id].type != CHAN_TYPE_TCP_LISTEN) {
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

// If connection fails the caller is expected to free the channel
int chan_connect(int ch_id, struct sockaddr_in *peer, int timeout, int options)
{
    int cc;

    (void) options; // unused for now

    // No more connected UDP; use chan_send_dgram/chan_recv_dgram instead.
    if (channels[ch_id].type != CHAN_TYPE_TCP_CLIENT) {
        lserrno = LSE_INTERNAL;
        return -1;
    }
    // No peer for this channel?
    if (peer == NULL) {
        lserrno = LSE_BAD_ARGS;
        return -1;
    }

    cc = connect_timeout(channels[ch_id].sock, (struct sockaddr *) peer,
                         sizeof(struct sockaddr_in), timeout);
    if (cc < 0) {
        if (errno == ETIMEDOUT)
            lserrno = LSE_TIME_OUT;
        else
            lserrno = LSE_CONN_SYS;
        return -1;
    }

    return 0;
}

int chan_send_dgram(int ch_id, char *buf, size_t len, struct sockaddr_in *peer)
{
    if (! chan_is_udp(channels[ch_id].type)) {
        lserrno = LSE_INTERNAL;
        return -1;
    }

    ssize_t cc = sendto(chan_sock(ch_id), buf, len, 0, (struct sockaddr *)peer,
                        sizeof(struct sockaddr_in));
    if (cc < 0) {
        lserrno = LSE_MSG_SYS;
        return -1;
    }

    return 0;
}

// Client call
int chan_recv_dgram(int ch_id, void *buf, size_t len,
                    struct sockaddr_storage *peer, int timeout)
{
    // We can receive dgram packets on both client udp but
    // also on listening if we are master lim
    if (! chan_is_udp(channels[ch_id].type)) {
        lserrno = LSE_INTERNAL;
        return -1;
    }

    socklen_t peersize = sizeof(struct sockaddr_storage);
    int sock = chan_sock(ch_id);

    int cc = rd_poll(sock, timeout);
    if (cc < 0) {
        lserrno = LSE_POLL_SYS;
        return -1;
    }
    if (cc == 0) {
        lserrno = LSE_TIME_OUT;
        return -1;
    }

    cc = recvfrom(sock, buf, len, 0, (struct sockaddr *)peer, &peersize);
    if (cc < 0) {
        lserrno = LSE_MSG_SYS;
        return -1;
    }

    return cc;
}
// Open channel after accept
int chan_open_sock(int s, int options)
{
    int i;

    if ((i = chan_find_free()) < 0) {
        lserrno = LSE_NO_CHAN;
        return -1;
    }

    // We already set the non block in accept but we can
    // have a caller that calls socket/connect before
    if ((options & CHAN_OP_NONBLOCK) && (io_nonblock_(s) < 0)) {
        lserrno = LSE_SOCK_SYS;
        return -1;
    }

    channels[i].type = CHAN_TYPE_TCP_CONNECT;
    channels[i].sock = s;

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
    channels[ch_id].chan_events = CHAN_EPOLLNONE;
    channels[ch_id].sock = -1;
    channels[ch_id].send = channels[ch_id].recv = NULL;

    return 0;
}

int chan_enqueue(int ch_id, struct Buffer *msg)
{
    if (! chan_is_valid(ch_id))
        return -1;

    enqueueTail_(msg, channels[ch_id].send);
    return 0;
}

int chan_dequeue(int ch_id, struct Buffer **buf)
{
    if (! chan_is_valid(ch_id))
        return -1;

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

// chan_epoll: drives I/O and sets chan->chan_events (enum state),
// does not expose epoll flags directly.
int chan_epoll(int ef, struct epoll_event *events, int max_events, int tm)
{
    int cc = epoll_wait(ef, events, max_events, tm);
    if (cc == 0) // timeout
        return 0;
    if (cc < 0)  {
        // dont do magic with EINTR or whatever else, caller knows the best
        return -1;
    }

    for (int i = 0; i < cc; i++) {
        struct epoll_event *e = &events[i];
        struct chan_data *chan = &channels[e->data.u32];

        // clean channel specific events for the caller
        chan->chan_events = CHAN_EPOLLNONE;

        if ((e->events & EPOLLERR)
            || (e->events & EPOLLHUP)
            || (e->events & EPOLLRDHUP)) {
            chan->chan_events = CHAN_EPOLLERR;
            continue;
        }

        if (chan->type == CHAN_TYPE_TCP_LISTEN) {
            chan->chan_events = CHAN_EPOLLIN;
            continue;
        }
        if (chan->type == CHAN_TYPE_UDP_LISTEN) {
            chan->chan_events = CHAN_EPOLLIN;
            continue;
        }
        if (chan->type == CHAN_TYPE_TIMER) {
            chan->chan_events = CHAN_EPOLLIN;
            continue;
        }

        if (e->events & EPOLLIN) {
            doread(chan);
            continue;
        }
        if (e->events & EPOLLOUT) {
            dowrite(chan);
        }
    }
    return cc;
}

static void doread(struct chan_data *chan)
{
    struct Buffer *rcvbuf;
    int cc;

    // Get or create receive buffer
    if (chan->recv->forw == chan->recv) {
        rcvbuf = make_buf();
        if (!rcvbuf) {
            chan->chan_events = CHAN_EPOLLERR;
            return;
        }
        enqueueTail_(rcvbuf, chan->recv);
    } else {
        rcvbuf = chan->recv->forw;
    }

    // Phase 1: Read header
    if (rcvbuf->len == 0) {
        rcvbuf->data = malloc(PACKET_HEADER_SIZE);
        if (!rcvbuf->data) {
            chan->chan_events = CHAN_EPOLLERR;
            return;
        }
        rcvbuf->len = PACKET_HEADER_SIZE;
        rcvbuf->pos = 0;
    }

    // Still reading header
    if (rcvbuf->len == PACKET_HEADER_SIZE && rcvbuf->pos < PACKET_HEADER_SIZE) {

        cc = read(chan->sock,
                  rcvbuf->data + rcvbuf->pos,
                  PACKET_HEADER_SIZE - rcvbuf->pos);
        if (cc < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
                return;
            chan->chan_events = CHAN_EPOLLERR;
            return;
        }
        if (cc == 0) {
            chan->chan_events = CHAN_EPOLLERR;
            return;
        }
        rcvbuf->pos += cc;

        // Header complete - parse and allocate for payload
        if (rcvbuf->pos == PACKET_HEADER_SIZE) {
            XDR xdrs;
            struct packet_header hdr;

            xdrmem_create(&xdrs, rcvbuf->data, PACKET_HEADER_SIZE, XDR_DECODE);
            if (!xdr_pack_hdr(&xdrs, &hdr)) {
                chan->chan_events = CHAN_EPOLLERR;
                xdr_destroy(&xdrs);
                return;
            }
            xdr_destroy(&xdrs);

            if (hdr.length > 0) {
                // Extend buffer for payload
                char *payload = realloc(rcvbuf->data,
                                        PACKET_HEADER_SIZE + hdr.length);
                if (!payload) {
                    chan->chan_events = CHAN_EPOLLERR;
                    return;
                }
                rcvbuf->data = payload;
                rcvbuf->len = PACKET_HEADER_SIZE + hdr.length;
            }
            // If no payload, packet is complete
            if (hdr.length == 0) {
                chan->chan_events = CHAN_EPOLLIN;
                return;
            }
        }
        return;
    }

    // Phase 2: Read payload
    if (rcvbuf->pos < rcvbuf->len) {
        cc = read(chan->sock, rcvbuf->data + rcvbuf->pos,
                  rcvbuf->len - rcvbuf->pos);
        if (cc < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
                return;
            chan->chan_events = CHAN_EPOLLERR;
            return;
        }
        if (cc == 0) {
            chan->chan_events = CHAN_EPOLLERR;
            return;
        }
        rcvbuf->pos += cc;

        // Packet complete
        if (rcvbuf->pos == rcvbuf->len) {
            chan->chan_events = CHAN_EPOLLIN;
        }
    }
}

static void dowrite(struct chan_data *chan)
{
    struct Buffer *sendbuf;
    int cc;

    if (chan->send->forw == chan->send)
        return;

    sendbuf = chan->send->forw;

    cc = write(chan->sock,
               sendbuf->data + sendbuf->pos,
               sendbuf->len - sendbuf->pos);

    if (cc < 0) {
        // transient, wait for writable again
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
            return;
        // real error
        chan->chan_events = CHAN_EPOLLERR;
        return;
    }

    // nothing written, but not an error
    if (cc == 0)
        return;

    // all sent
    sendbuf->pos += cc;
    if (sendbuf->pos == sendbuf->len) {
        dequeue_(sendbuf);
        free(sendbuf->data);
        free(sendbuf);
    }
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
            channels[i].send = NULL;
            channels[i].recv = NULL;
            return i;
        }
    }
    return -1;
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

int chan_create_timer(int seconds)
{
    int ch;

    if ((ch = chan_find_free()) < 0) {
        lserrno = LSE_NO_CHAN;
        return -1;
    }

    int tfd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC | TFD_NONBLOCK);
    if (tfd < 0) {
        syslog(LOG_ERR, "timerfd_create() failed: %m");
        return -1;
    }

    // set the time in the channel socket
    channels[ch].sock = tfd;
    channels[ch].type = CHAN_TYPE_TIMER;

    // Reload the time
    struct itimerspec its;
    its.it_value.tv_sec = seconds;
    its.it_value.tv_nsec = 0;
    its.it_interval.tv_sec = seconds;
    its.it_interval.tv_nsec = 0;

    if (timerfd_settime(tfd, 0, &its, NULL) < 0) {
        syslog(LOG_ERR, "timerfd_settime() failed: %m");
        chan_close(ch);
        return -1;
    }

    return ch;
}

static inline bool_t chan_is_udp(enum chanType t)
{
    if (t != CHAN_TYPE_UDP_CLIENT
        && t != CHAN_TYPE_UDP_LISTEN)
        return false;

    return true;
}

static inline bool_t chan_is_valid(int ch_id)
{
    if (ch_id < 0 || ch_id > chan_open_max) {
        lserrno = LSE_BAD_CHAN;
        return false;
    }

    if (channels[ch_id].sock == -1) {
        lserrno = LSE_NO_CHAN;
        return false;
    }

    return true;
}
