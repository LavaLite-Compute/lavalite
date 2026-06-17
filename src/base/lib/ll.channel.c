// Copyright (C) 2007 Platform Computing Inc
// Copyright (C) 2024-2025 LavaLite Contributors
// GPL v2

#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/timerfd.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <netinet/in.h>

#include "base/lib/ll.protocol.h"
#include "base/lib/ll.list.h"
#include "base/lib/ll.channel.h"
#include "base/lib/ll.bufsiz.h"

struct chan_data channels[CHAN_MAX];

static struct chan_buffer *make_buf(void)
{
    struct chan_buffer *buf = calloc(1, sizeof(*buf));
    if (!buf)
        return NULL;

    // link is zeroed by calloc; no sentinel init needed with ll_list
    return buf;
}

static void doread(struct chan_data *chan)
{
    struct chan_buffer *rcvbuf;
    int cc;

    // Get or create receive buffer
    if (ll_list_is_empty(&chan->recv)) {
        rcvbuf = make_buf();
        if (!rcvbuf) {
            chan->chan_events = CHAN_EPOLLERR;
            return;
        }
        ll_list_append(&chan->recv, &rcvbuf->link);
    } else {
        rcvbuf = (struct chan_buffer *) chan->recv.head;
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

            if (hdr.length > LL_MAX_PACKET_SIZE) {
                chan->chan_events = CHAN_EPOLLERR;
                return;
            }

            if (hdr.length > 0) {
                char *payload =
                    realloc(rcvbuf->data, PACKET_HEADER_SIZE + hdr.length);
                if (!payload) {
                    chan->chan_events = CHAN_EPOLLERR;
                    return;
                }
                rcvbuf->data = payload;
                rcvbuf->len = PACKET_HEADER_SIZE + hdr.length;
            }
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

        if (rcvbuf->pos == rcvbuf->len) {
            chan->chan_events = CHAN_EPOLLIN;
        }
    }
}

static void dowrite(struct chan_data *chan, int chan_id, int efd)
{
    struct chan_buffer *sendbuf;
    int cc;

    if (ll_list_is_empty(&chan->send))
        return;

    sendbuf = (struct chan_buffer *) chan->send.head;

    cc = write(chan->sock, sendbuf->data + sendbuf->pos,
               sendbuf->len - sendbuf->pos);
    if (cc < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
            return;
        chan->chan_events = CHAN_EPOLLERR;
        chan_set_write_interest(chan_id, efd, 0);
        return;
    }

    sendbuf->pos += cc;
    if (sendbuf->pos == sendbuf->len) {
        ll_list_remove(&chan->send, &sendbuf->link);
        free(sendbuf->data);
        free(sendbuf);
        if (ll_list_is_empty(&chan->send)) {
            chan_set_write_interest(chan_id, efd, 0);
        }
    }
}

static int chan_find_free(void)
{
    for (int i = 0; i < CHAN_MAX; i++) {
        if (channels[i].sock == -1)
            return i;
    }
    return -1;
}

static inline int chan_is_udp(enum chan_type t)
{
    if (t != UDP_CLIENT && t != UDP_SERVER)
        return 0;

    return 1;
}

static inline int chan_is_valid(int chan_id)
{
    if (chan_id < 0 || chan_id > CHAN_MAX)
        return 0;

    if (channels[chan_id].sock == -1)
        return 0;

    return 1;
}

void chan_init(void)
{
    static int initialised;

    if (initialised)
        return;

    for (int i = 0; i < CHAN_MAX; i++) {
        channels[i].sock = -1;
        channels[i].chan_events = CHAN_EPOLLNONE;
        ll_list_init(&channels[i].send);
        ll_list_init(&channels[i].recv);
    }
    initialised = 1;
}

int chan_sock(int chan_id)
{
    if (chan_id < 0 || chan_id > CHAN_MAX)
        return -1;

    return channels[chan_id].sock;
}

int chan_udp_server(uint16_t port)
{
    int chan_id = chan_find_free();
    if (chan_id < 0)
        return -1;

    int s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0)
        return -1;

    channels[chan_id].sock = s;

    int one = 1;
    if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) < 0) {
        close(s);
        channels[chan_id].sock = -1;
        return -1;
    }

    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);
    sin.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(s, (struct sockaddr *) &sin, sizeof(sin)) < 0) {
        close(s);
        channels[chan_id].sock = -1;
        return -1;
    }

    channels[chan_id].type = UDP_SERVER;

    return chan_id;
}

int chan_tcp_server(uint16_t port)
{
    int chan_id = chan_find_free();
    if (chan_id < 0)
        return -1;

    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0)
        return -1;

    channels[chan_id].sock = s;

    int one = 1;
    if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) < 0) {
        close(s);
        channels[chan_id].sock = -1;
        return -1;
    }

    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);
    sin.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(s, (struct sockaddr *) &sin, sizeof(sin)) < 0) {
        close(s);
        channels[chan_id].sock = -1;
        return -1;
    }

    if (listen(s, SOMAXCONN) < 0) {
        close(s);
        channels[chan_id].sock = -1;
        return -1;
    }

    channels[chan_id].type = TCP_SERVER;

    return chan_id;
}

int chan_accept(int chan_id, struct sockaddr_in *from)
{
    if (channels[chan_id].type != TCP_SERVER)
        return -1;

    socklen_t len = sizeof(struct sockaddr);
    int s = accept(channels[chan_id].sock, (struct sockaddr *) from, &len);
    if (s < 0)
        return -1;

    return chan_open(s);
}

/*
 * Detect and reject TCP self-connect.
 *
 * Observed in simulation mode on loopback: connect() succeeded while mbd was
 * down, sbd received its own BATCH_SBD_REGISTER, and the mbd port became
 * unavailable. Treat as ECONNREFUSED.
 */
static int chan_self_connected(int fd)
{
    struct sockaddr_in local;
    struct sockaddr_in remote;
    socklen_t len;

    len = sizeof(local);
    if (getsockname(fd, (struct sockaddr *)&local, &len) < 0)
        return 0;

    len = sizeof(remote);
    if (getpeername(fd, (struct sockaddr *)&remote, &len) < 0)
        return 0;

    if (local.sin_addr.s_addr == remote.sin_addr.s_addr &&
        local.sin_port == remote.sin_port)
        return 1;

    return 0;
}

int chan_connect(int chan_id, struct sockaddr_in *peer, int timeout_sec)
{
    if (!chan_is_valid(chan_id))
        return -1;

    if (channels[chan_id].type != TCP_CLIENT)
        return -1;

    if (peer == NULL)
        return -1;

    int fd = channels[chan_id].sock;

    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0)
        return -1;

    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)
        return -1;

    int rc = connect(fd, (struct sockaddr *)peer, sizeof(*peer));
    if (rc == 0)
        goto done;

    int err;
    if (errno != EINPROGRESS)
        goto fail;

    struct pollfd pfd;
    pfd.fd = fd;
    pfd.events = POLLOUT;
    pfd.revents = 0;

    int cc = poll(&pfd, 1, timeout_sec * 1000);
    if (cc < 0)
        goto fail;

    if (cc == 0) {
        errno = ETIMEDOUT;
        goto fail;
    }

    err = 0;
    socklen_t len = sizeof(err);
    if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &len) < 0)
        goto fail;

    if (err != 0) {
        errno = err;
        goto fail;
    }

done:
    if (fcntl(fd, F_SETFL, flags) < 0)
        return -1;

    if (chan_self_connected(fd)) {
        errno = EPROTO;
        return -1;
    }
    return 0;

fail:
    {
        int save_errno = errno;
        fcntl(fd, F_SETFL, flags);
        errno = save_errno;
        return -1;
    }
}

int chan_send_dgram(int chan_id, char *buf, size_t len, struct sockaddr_in *peer)
{
    if (!chan_is_udp(channels[chan_id].type))
        return -1;

    ssize_t cc = sendto(chan_sock(chan_id), buf, len, 0, (struct sockaddr *) peer,
                        sizeof(struct sockaddr_in));
    if (cc < 0)
        return -1;

    return 0;
}

int chan_recv_dgram(int chan_id, void *buf, size_t len, struct sockaddr_in *peer,
                    int timeout)
{
    if (!chan_is_udp(channels[chan_id].type))
        return -1;

    socklen_t peersize = sizeof(struct sockaddr_in);
    int sock = chan_sock(chan_id);

    int cc = rd_poll(sock, timeout);
    if (cc <= 0)
        return -1;

    cc = recvfrom(sock, buf, len, 0, (struct sockaddr *) peer, &peersize);
    if (cc < 0)
        return -1;

    return cc;
}

// Open channel after accept
int chan_open(int s)
{
    int i;

    if ((i = chan_find_free()) < 0)
        return -1;

    if (io_non_block(s) < 0)
        return -1;

    channels[i].type = TCP_CONNECT;
    channels[i].sock = s;
    ll_list_init(&channels[i].send);
    ll_list_init(&channels[i].recv);

    return i;
}

int chan_close(int chan_id)
{
    if (chan_id < 0 || chan_id > CHAN_MAX)
        return -1;

    if (channels[chan_id].sock < 0)
        return -1;

    close(channels[chan_id].sock);

    ll_list_clear(&channels[chan_id].send, (void (*)(void *)) chan_free_buf);
    ll_list_clear(&channels[chan_id].recv, (void (*)(void *)) chan_free_buf);

    channels[chan_id].chan_events = CHAN_EPOLLNONE;
    channels[chan_id].sock = -1;

    return 0;
}

int chan_enqueue(int chan_id, struct chan_buffer *msg)
{
    if (!chan_is_valid(chan_id))
        return -1;

    ll_list_append(&channels[chan_id].send, &msg->link);
    return 0;
}

int chan_dequeue(int chan_id, struct chan_buffer **buf)
{
    struct ll_list_entry *e;

    if (!chan_is_valid(chan_id)) {
        errno = EINVAL;
        return -1;
    }

    e = ll_list_dequeue(&channels[chan_id].recv);
    if (!e) {
        errno = ENOENT;
        return -1;
    }

    *buf = (struct chan_buffer *) e;
    return 0;
}

ssize_t chan_read_nonblock(int chan_id, void *buf, size_t len, int timeout_sec)
{
    if (io_non_block(channels[chan_id].sock) < 0)
        return -1;

    int fd = channels[chan_id].sock;
    unsigned char *buffer = (unsigned char *) buf;
    size_t remaining = len;
    size_t total_read = 0;
    int timeout_ms = timeout_sec * 1000;

    while (remaining > 0) {
        struct pollfd pfd = {.fd = fd, .events = POLLIN};
        int poll_result = poll(&pfd, 1, timeout_ms);

        if (poll_result < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        if (poll_result == 0)
            return -1;

        ssize_t bytes_read = recv(fd, buffer + total_read, remaining, 0);

        if (bytes_read > 0) {
            remaining -= bytes_read;
            total_read += bytes_read;
            continue;
        }
        if (bytes_read == 0) {
            errno = ECONNRESET;
            return -1;
        }
        if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)
            continue;

        return -1;
    }

    return (ssize_t) total_read;
}

int chan_rpc(int chan_id, struct chan_buffer *snd, struct chan_buffer *rcv,
             struct protocol_header *hdr, int timeout)
{
    if (chan_write(chan_id, snd->data, snd->len) != snd->len)
        return -1;

    int cc = rd_poll(channels[chan_id].sock, timeout * 1000);
    if (cc < 0)
        return -1;

    if (cc == 0) {
        errno = ETIMEDOUT;
        return -1;
    }

    cc = recv_protocol_header(chan_id, hdr);
    if (cc < 0)
        return -1;

    rcv->data = NULL;
    rcv->len = hdr->length;
    if (rcv->len == 0)
        return 0;

    if (hdr->length > LL_MAX_PACKET_SIZE) {
        errno = EPERM;
        return -1;
    }

    if ((rcv->data = calloc(rcv->len, sizeof(char))) == NULL)
        return -1;

    if ((cc = chan_read(chan_id, rcv->data, rcv->len)) != rcv->len) {
        free(rcv->data);
        return -1;
    }

    return 0;
}

int chan_epoll(int efd, struct epoll_event *events, int max_events, int tm)
{
    int cc = epoll_wait(efd, events, max_events, tm);
    if (cc == 0)
        return 0;
    if (cc < 0)
        return -1;

    for (int i = 0; i < cc; i++) {
        struct epoll_event *e = &events[i];
        struct chan_data *chan = &channels[e->data.u32];
        int chan_id = e->data.u32;

        chan->chan_events = CHAN_EPOLLNONE;

        if (chan->type == TCP_SERVER || chan->type == UDP_SERVER ||
            chan->type == TIMER_FD) {
            chan->chan_events = CHAN_EPOLLIN;
            continue;
        }

        if (e->events & (EPOLLIN | EPOLLHUP | EPOLLRDHUP | EPOLLERR))
            doread(chan);

        if (e->events & EPOLLOUT)
            dowrite(chan, chan_id, efd);
    }

    return cc;
}

struct chan_buffer *chan_make_buf(void)
{
    return make_buf();
}

int chan_alloc_buf(struct chan_buffer **buf, int size)
{
    *buf = make_buf();
    if (!*buf)
        return -1;

    (*buf)->data = calloc(size, sizeof(char));
    if (!(*buf)->data) {
        free(*buf);
        return -1;
    }

    return 0;
}

void chan_free_buf(struct chan_buffer *buf)
{
    if (!buf)
        return;
    free(buf->data);
    free(buf);
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
    int ch = chan_find_free();
    if (ch < 0)
        return -1;

    int tfd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC | TFD_NONBLOCK);
    if (tfd < 0)
        return -1;

    channels[ch].sock = tfd;
    channels[ch].type = TIMER_FD;

    struct itimerspec its;
    its.it_value.tv_sec = seconds;
    its.it_value.tv_nsec = 0;
    its.it_interval.tv_sec = seconds;
    its.it_interval.tv_nsec = 0;

    if (timerfd_settime(tfd, 0, &its, NULL) < 0) {
        chan_close(ch);
        return -1;
    }

    return ch;
}

int chan_sock_error(int chan_id)
{
    int err = 0;
    socklen_t len = sizeof(err);

    int fd = channels[chan_id].sock;
    if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &len) < 0)
        return -1;

    return err;
}

int chan_set_write_interest(int chan_id, int efd, int on)
{
    if (efd < 0) {
        errno = EINVAL;
        return -1;
    }

    struct epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.events = EPOLLIN | EPOLLRDHUP;
    if (on)
        ev.events |= EPOLLOUT;
    ev.data.u32 = (uint32_t) chan_id;

    if (epoll_ctl(efd, EPOLL_CTL_MOD, chan_sock(chan_id), &ev) < 0)
        return -1;

    return 0;
}

ssize_t chan_read(int chan_id, void *buf, size_t len)
{
    if (len > (size_t) SSIZE_MAX) {
        errno = EOVERFLOW;
        return -1;
    }

    size_t total = 0;
    int s = channels[chan_id].sock;

    while (len > 0) {
        ssize_t cc = read(s, buf, len);

        if (cc > 0) {
            buf += (size_t) cc;
            len -= (size_t) cc;
            total += cc;
            continue;
        }
        if (cc == 0) {
            errno = 0;
            return -1;
        }
        if (errno == EINTR)
            continue;

        return -1;
    }

    return (ssize_t) total;
}

ssize_t chan_write(int chan_id, void *buf, size_t len)
{
    if (len > (size_t) SSIZE_MAX) {
        errno = EOVERFLOW;
        return -1;
    }

    size_t total = 0;
    int s = channels[chan_id].sock;

    while (len > 0) {
        ssize_t cc = write(s, buf, len);

        if (cc > 0) {
            buf += (size_t) cc;
            len -= (size_t) cc;
            total += cc;
            continue;
        }
        if (cc < 0 && errno == EINTR)
            continue;

        return -1;
    }

    return (ssize_t) total;
}

int connect_timeout(int s, const struct sockaddr *name, socklen_t namelen,
                    int timeout_sec)
{
    struct timeval tv = {.tv_sec = timeout_sec, .tv_usec = 0};

    if (setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) < 0)
        return -1;

    if (connect(s, name, namelen) < 0)
        return -1;

    struct sockaddr_in local;
    socklen_t len = sizeof(local);
    if (getsockname(s, (struct sockaddr *) &local, &len) < 0)
        return -1;

    struct sockaddr_in remote;
    if (getpeername(s, (struct sockaddr *) &remote, &len) < 0)
        return -1;

    if (local.sin_addr.s_addr == remote.sin_addr.s_addr
        && local.sin_port == remote.sin_port) {
        errno = ECONNREFUSED;
        return -1;
    }

    return 0;
}

int send_protocol_header(int chan_id, struct protocol_header *hdr)
{
    XDR xdrs;
    char buf[PACKET_HEADER_SIZE];

    xdrmem_create(&xdrs, buf, PACKET_HEADER_SIZE, XDR_ENCODE);
    if (!xdr_pack_hdr(&xdrs, hdr)) {
        xdr_destroy(&xdrs);
        return -1;
    }
    xdr_destroy(&xdrs);

    if (chan_write(chan_id, buf, PACKET_HEADER_SIZE) != PACKET_HEADER_SIZE)
        return -1;

    return 0;
}

int recv_protocol_header(int chan_id, struct protocol_header *hdr)
{
    XDR xdrs;
    char buf[PACKET_HEADER_SIZE];

    if (chan_read(chan_id, buf, PACKET_HEADER_SIZE) != PACKET_HEADER_SIZE)
        return -1;

    xdrmem_create(&xdrs, (char *) buf, PACKET_HEADER_SIZE, XDR_DECODE);
    if (!xdr_pack_hdr(&xdrs, hdr)) {
        xdr_destroy(&xdrs);
        return -1;
    }
    xdr_destroy(&xdrs);

    return 0;
}

int chan_has_error(int chan_id)
{
    if (channels[chan_id].chan_events == CHAN_EPOLLERR)
        return 1;
    return 0;
}

const char *chan_addr_str(int chan_id)
{
    static __thread char buf[INET_ADDRSTRLEN + 8];
    struct sockaddr_in peer;
    socklen_t plen = sizeof(peer);

    if (getpeername(chan_sock(chan_id), (struct sockaddr *) &peer, &plen) != 0) {
        strcpy(buf, "?.?:?");
        return buf;
    }

    char ip[INET_ADDRSTRLEN];
    if (!inet_ntop(AF_INET, &peer.sin_addr, ip, sizeof(ip))) {
        strcpy(buf, "?.?:?");
        return buf;
    }

    snprintf(buf, sizeof(buf), "%s:%u", ip, ntohs(peer.sin_port));
    return buf;
}

int chan_udp_client(void)
{
    int ch = chan_find_free();
    if (ch < 0)
        return -1;

    int s = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    if (s < 0)
        return -1;

    channels[ch].sock = s;
    channels[ch].type = UDP_CLIENT;
    channels[ch].chan_events = CHAN_EPOLLNONE;
    return ch;
}

int chan_tcp_client(void)
{
    int ch = chan_find_free();
    if (ch < 0)
        return -1;

    int s = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (s < 0)
        return -1;

    channels[ch].sock = s;
    channels[ch].type = TCP_CLIENT;
    channels[ch].chan_events = CHAN_EPOLLNONE;
    return ch;
}

int chan_is_readable(int chan_id)
{
    if (!chan_is_valid(chan_id))
        return 0;

    if (channels[chan_id].chan_events == CHAN_EPOLLIN ||
        channels[chan_id].chan_events == CHAN_EPOLLERR)
        return 1;

    return 0;
}
