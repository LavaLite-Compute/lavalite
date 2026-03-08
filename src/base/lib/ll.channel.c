/* Copyright (C) LavaLite Contributors
 * GPL v2
 */
#include "base/lib/ll.channel.h"

long chan_open_max;
struct chan_data *channels;
static int epoll_fd;
static void doread(struct chan_data *);
static void dowrite(struct chan_data *, int);
static struct Buffer *make_buf(void);
static void enqueueTail_(struct Buffer *, struct Buffer *);
static void dequeue_(struct Buffer *);
static int chan_find_free(void);
static inline bool chan_is_udp(enum chanType);
static inline bool chan_is_valid(int);

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

int chan_sock(int ch_id)
{
    if (ch_id < 0 || ch_id > chan_open_max) {
        return -1;
    }

    return channels[ch_id].sock;
}

int chan_udp_socket(u_short port)
{
    int ch_id = chan_find_free();
    if (ch_id < 0)
        return -1;

    int s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0)
        return -1;

    channels[ch_id].sock = s;

    int one = 1;
    if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) < 0) {
        close(s);
        channels[ch_id].sock = -1;
        return -1;
    }

    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);
    sin.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(s, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
        close(s);
        channels[ch_id].sock = -1;
        return -1;
    }

    channels[ch_id].type = CHAN_TYPE_UDP_LISTEN;

    return ch_id;
}

int chan_tcp_listen_socket(u_short port)
{
    int ch_id = chan_find_free();
    if (ch_id < 0)
        return -1;

    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0)
        return -1;

    channels[ch_id].sock = s;

    int one = 1;
    if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) < 0) {
        close(s);
        channels[ch_id].sock = -1;
        return -1;
    }

    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);
    sin.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(s, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
        close(s);
        channels[ch_id].sock = -1;
        return -1;
    }

    if (listen(s, SOMAXCONN) < 0) {
        close(s);
        channels[ch_id].sock = -1;
        return -1;
    }

    channels[ch_id].type = CHAN_TYPE_TCP_LISTEN;

    return ch_id;
}

int chan_udp_client_socket(void)
{
    int ch = chan_find_free();
    if (ch < 0)
        return -1;

    int s = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    if (s < 0)
        return -1;

    channels[ch].sock = s;
    channels[ch].chan_events = CHAN_EPOLLNONE;
    channels[ch].type = CHAN_TYPE_UDP_CLIENT;

    return ch;
}

int chan_tcp_client_socket(void)
{
    int ch = chan_find_free();
    if (ch < 0)
        return -1;

    int s = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (s < 0)
        return -1;

    channels[ch].sock = s;
    channels[ch].chan_events = CHAN_EPOLLNONE;
    channels[ch].type = CHAN_TYPE_TCP_CLIENT;

    return ch;
}

int chan_accept(int ch_id, struct sockaddr_in *from)
{
    int s;
    socklen_t len = sizeof(struct sockaddr);

    if (channels[ch_id].type != CHAN_TYPE_TCP_LISTEN) {
        return -1;
    }

    while (1) {
        s = accept(channels[ch_id].sock, (struct sockaddr *) from, &len);
        if (s >= 0)
            break;

        // The system call was interrupted by a signal that was caught  be‐
        // fore a valid connection arrived;
        if (errno == EINTR)
            continue;

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
        return -1;
    }
    // No peer for this channel?
    if (peer == NULL) {
        return -1;
    }

    cc = connect_timeout(channels[ch_id].sock, (struct sockaddr *) peer,
                         sizeof(struct sockaddr_in), timeout);
    if (cc < 0)
        return -1;

    return 0;
}

int chan_send_dgram(int ch_id, char *buf, size_t len, struct sockaddr_in *peer)
{
    if (!chan_is_udp(channels[ch_id].type)) {
        return -1;
    }

    ssize_t cc = sendto(chan_sock(ch_id), buf, len, 0, (struct sockaddr *) peer,
                        sizeof(struct sockaddr_in));
    if (cc < 0) {
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
    if (!chan_is_udp(channels[ch_id].type)) {
        return -1;
    }

    socklen_t peersize = sizeof(struct sockaddr_storage);
    int sock = chan_sock(ch_id);

    int cc = rd_poll(sock, timeout);
    if (cc < 0) {
        return -1;
    }
    if (cc == 0) {
        return -1;
    }

    cc = recvfrom(sock, buf, len, 0, (struct sockaddr *) peer, &peersize);
    if (cc < 0) {
        return -1;
    }

    return cc;
}
// Open channel after accept
int chan_open_sock(int s, int options)
{
    int i;

    if ((i = chan_find_free()) < 0) {
        return -1;
    }

    if ((options & CHAN_OP_NONBLOCK) && (io_non_block(s) < 0)) {
        return -1;
    }

    channels[i].type = CHAN_TYPE_TCP_CONNECT;
    channels[i].sock = s;

    channels[i].send = make_buf();
    channels[i].recv = make_buf();
    if (!channels[i].send || !channels[i].recv) {
        chan_close(i);
        return -1;
    }
    return i;
}

int chan_close(int ch_id)
{
    struct Buffer *buf;
    struct Buffer *nextbuf;

    if (ch_id < 0 || ch_id > chan_open_max) {
        return -1;
    }

    if (channels[ch_id].sock < 0) {
        return -1;
    }
    close(channels[ch_id].sock);

    if (channels[ch_id].send) {
        for (buf = channels[ch_id].send->forw; buf != channels[ch_id].send;
             buf = nextbuf) {
            nextbuf = buf->forw;
            free(buf->data);
            free(buf);
        }
        free(channels[ch_id].send);
    }
    if (channels[ch_id].recv) {
        for (buf = channels[ch_id].recv->forw; buf != channels[ch_id].recv;
             buf = nextbuf) {
            nextbuf = buf->forw;
            free(buf->data);
            free(buf);
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
    if (!chan_is_valid(ch_id))
        return -1;

    enqueueTail_(msg, channels[ch_id].send);
    return 0;
}

int chan_dequeue(int ch_id, struct Buffer **buf)
{
    if (!chan_is_valid(ch_id))
        return -1;

    if (channels[ch_id].recv->forw == channels[ch_id].recv) {
        return -1;
    }
    *buf = channels[ch_id].recv->forw;
    dequeue_(channels[ch_id].recv->forw);
    return 0;
}

ssize_t chan_read_nonblock(int ch_id, void *buf, size_t len, int timeout_sec)
{
    if (io_non_block(channels[ch_id].sock) < 0) {
        return -1;
    }

    int fd = channels[ch_id].sock;
    unsigned char *buffer = (unsigned char *) buf;
    size_t remaining = len;
    size_t total_read = 0;
    int timeout_ms = timeout_sec * 1000;

    while (remaining > 0) {
        struct pollfd pfd = {.fd = fd, .events = POLLIN};
        int poll_result = poll(&pfd, 1, timeout_ms);

        // Handle poll errors
        if (poll_result < 0) {
            if (errno == EINTR)
                continue; // Interrupted by signal, retry
            return -1;
        }

        // Handle timeout
        if (poll_result == 0) {
            return -1;
        }

        // Ready to read - attempt the read
        ssize_t bytes_read = recv(fd, buffer + total_read, remaining, 0);

        // Handle successful read
        if (bytes_read > 0) {
            remaining -= bytes_read;
            total_read += bytes_read;
            continue;
        }

        // Handle connection closed
        if (bytes_read == 0) {
            errno = ECONNRESET;
            return -1;
        }

        // Handle read errors (bytes_read < 0)
        if (errno == EINTR)
            continue; // Interrupted, retry
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            continue; // Would block, poll again

        // Real error occurred
        return -1;
    }

    return (ssize_t) total_read;
}

int chan_rpc(int ch_id, struct Buffer *in, struct Buffer *out,
             struct protocol_header *out_hdr, int timeout)
{
    if (in) {
        if (chan_write(ch_id, in->data, in->len) != in->len)
            return -1;

        if (in->forw != NULL) {
            struct Buffer *buf = in->forw;
            int nlen = htonl(buf->len);

            if (chan_write(ch_id, &nlen, sizeof(int)) != sizeof(int))
                return -1;

            if (chan_write(ch_id, buf->data, buf->len) != buf->len)
                return -1;
        }
    }

    if (!out) {
        return 0;
    }

    int cc = rd_poll(channels[ch_id].sock, timeout * 1000);
    if (cc < 0)
        return -1;

    if (cc == 0) {
        errno = ETIMEDOUT;
        return -1;
    }

    cc = recv_protocol_header(ch_id, out_hdr);
    if (cc < 0) {
        return -1;
    }

    out->data = NULL;
    out->len = out_hdr->length;
    if (out->len == 0)
        return 0;

    if ((out->data = calloc(out->len, sizeof(char))) == NULL) {
        return -1;
    }

    if ((cc = chan_read(ch_id, out->data, out->len)) != out->len) {
        free(out->data);
        return -1;
    }

    return 0;
}

// chan_epoll: drives I/O and sets chan->chan_events (enum state),
// does not expose epoll flags directly.
int chan_epoll(int ef, struct epoll_event *events, int max_events, int tm)
{
    // we may need access to the global ef in other parts of the code
    epoll_fd = ef;

    int cc = epoll_wait(ef, events, max_events, tm);
    if (cc == 0) // timeout
        return 0;
    if (cc < 0) {
        // dont do magic with EINTR or whatever else, caller knows the best
        return -1;
    }

    for (int i = 0; i < cc; i++) {
        struct epoll_event *e = &events[i];
        struct chan_data *chan = &channels[e->data.u32];
        int ch_id = e->data.u32;

        // clean channel specific events for the caller
        chan->chan_events = CHAN_EPOLLNONE;

        if ((e->events & EPOLLERR) || (e->events & EPOLLHUP) ||
            (e->events & EPOLLRDHUP)) {
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
        }

        if (e->events & EPOLLOUT) {
            dowrite(chan, ch_id);
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
        cc = read(chan->sock, rcvbuf->data + rcvbuf->pos,
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
            struct protocol_header hdr;

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

static void dowrite(struct chan_data *chan, int chan_id)
{
    struct Buffer *sendbuf;
    int cc;

    if (chan->send->forw == chan->send)
        return;

    sendbuf = chan->send->forw;

    cc = write(chan->sock, sendbuf->data + sendbuf->pos,
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
        // Remove from epoll EPOLLOUT only when the whole list
        // is all empty
        if (chan->send->forw == chan->send) {
            chan_set_write_interest(chan_id, false);
        }
    }
}

struct Buffer *chan_make_buf(void)
{
    return make_buf();
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
    if (!buf)
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

/*
 * chan_find_free()
 *
 * Find a free channel slot.
 *
 * A channel slot is considered free if channels[i].sock == -1.
 * This function clears send/recv pointers for the returned slot.
 *
 * Return values:
 *   channel index on success
 *   -1 if no free slots are available
 */
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

int io_non_block(int s)
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

int chan_sock_error(int ch_id)
{
    int err = 0;
    socklen_t len = sizeof(err);

    int fd = channels[ch_id].sock;
    if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &len) < 0)
        return -1;

    return err;
}

int chan_set_write_interest(int ch_id, bool on)
{
    // epoll_fd is process-global: each daemon has a single epoll instance,
    // initialized by chan_epoll() before any channel I/O.
    if (epoll_fd < 0) {
        errno = EINVAL;
        return -1;
    }

    struct epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.events = EPOLLIN;
    if (on) {
        ev.events |= EPOLLOUT;
    }
    ev.data.u32 = (uint32_t)ch_id;

    // The epoll_fd is set globally by the chan_epoll
    if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, chan_sock(ch_id), &ev) < 0) {
        return -1;
    }

    return 0;
}

ssize_t chan_read(int ch_id, void *buf, size_t len)
{
    if (len > (size_t) SSIZE_MAX) {
        errno = EOVERFLOW;
        return -1;
    }

    size_t total = 0;
    int s = channels[ch_id].sock;
    while (len > 0) {
        ssize_t cc = read(s, buf, len);

        if (cc > 0) {
            buf += (size_t) cc;
            len -= (size_t) cc;
            total += cc;
            continue;
        }

        if (cc == 0) {
            // Peer closed connection
            return -1;
        }

        if (cc == -1 && errno == EINTR)
            continue;

        // Unrecoverable error
        return -1;
    }

    return (ssize_t) total;

}

ssize_t chan_write(int ch_id, void *buf, size_t len)
{
    if (len > (size_t) SSIZE_MAX) {
        errno = EOVERFLOW;
        return -1;
    }

    size_t total = 0;
    int s = channels[ch_id].sock;
    while (len > 0) {
        // Do write
        ssize_t cc = write(s, buf, len);

        if (cc > 0) {
            buf += (size_t) cc;
            len -= (size_t) cc;
            total += cc;
            continue;
        }

        if (cc < 0 && errno == EINTR)
            continue;

        // Unrecoverable error
        return -1;
    }

    return (ssize_t) total;
}

int connect_timeout(int s, const struct sockaddr *name, socklen_t namelen,
                    int timeout_sec)
{
    int flags = fcntl(s, F_GETFL, 0);
    if (flags < 0)
        return -1;

    if (fcntl(s, F_SETFL, flags | O_NONBLOCK) < 0)
        return -1;

    int rc = connect(s, name, namelen);
    if (rc == 0) {
        // Connected immediately
        fcntl(s, F_SETFL, flags); // Restore flags
        return 0;
    }

    if (errno != EINPROGRESS) {
        // Immediate failure
        fcntl(s, F_SETFL, flags); // Restore flags
        return -1;
    }

    // Initialize poll data structure
    struct pollfd pfd = {.fd = s, .events = POLLOUT};

    rc = poll(&pfd, 1, timeout_sec * 1000);
    if (rc <= 0) {
        // Timeout or poll error
        errno = (rc == 0) ? ETIMEDOUT : errno;
        fcntl(s, F_SETFL, flags); // Restore original flags
        return -1;
    }

    int err = 0;
    socklen_t errlen = sizeof(err);
    if (getsockopt(s, SOL_SOCKET, SO_ERROR, &err, &errlen) < 0) {
        fcntl(s, F_SETFL, flags); // Restore flags
        return -1;
    }

    fcntl(s, F_SETFL, flags); // Restore original flags
    if (err != 0) {
        errno = err;
        return -1;
    }

    return 0;
}
static inline bool chan_is_udp(enum chanType t)
{
    if (t != CHAN_TYPE_UDP_CLIENT && t != CHAN_TYPE_UDP_LISTEN)
        return false;

    return true;
}

static inline bool chan_is_valid(int ch_id)
{
    if (ch_id < 0 || ch_id > chan_open_max) {
        return false;
    }

    if (channels[ch_id].sock == -1) {
        return false;
    }

    return true;
}

int send_protocol_header(int ch_id, struct protocol_header *hdr)
{
    XDR xdrs;
    char buf[PACKET_HEADER_SIZE];

    xdrmem_create(&xdrs, buf, PACKET_HEADER_SIZE, XDR_ENCODE);
    if (!xdr_pack_hdr(&xdrs, hdr)) {
        xdr_destroy(&xdrs);
        return -1;
    }
    xdr_destroy(&xdrs);

    if (chan_write(ch_id, buf, PACKET_HEADER_SIZE) != PACKET_HEADER_SIZE)
        return -1;

    return 0;
}

int recv_protocol_header(int ch_id, struct protocol_header *hdr)
{
    XDR xdrs;
    char buf[PACKET_HEADER_SIZE];

    if (chan_read(ch_id, buf, PACKET_HEADER_SIZE) != PACKET_HEADER_SIZE) {
        return -1;
    }

    xdrmem_create(&xdrs, (char *) buf, PACKET_HEADER_SIZE, XDR_DECODE);
    if (!xdr_pack_hdr(&xdrs, hdr)) {
        xdr_destroy(&xdrs);
        return -1;
    }
    xdr_destroy(&xdrs);

    return 0;
}
