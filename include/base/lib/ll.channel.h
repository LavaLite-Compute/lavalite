/* Copyright (C) LavaLite Contributors
 * GLP v2
 */
#pragma once

#include "base/lib/ll.sys.h"
#include "base/lib/ll.wire.h"

// Channel options for now we only use socketoption
#define CHAN_OP_SOREUSE 0x01
#define CHAN_OP_NONBLOCK 0x01

// Channel type
enum chanType {
    CHAN_TYPE_UDP_LISTEN,  // bound UDP socket waiting for datagrams
    CHAN_TYPE_TCP_LISTEN,  // listening TCP socket waiting for connections
    CHAN_TYPE_TCP_CONNECT, // connected/accepted tcp channel
    CHAN_TYPE_TCP_CLIENT,  // stateless TCP client channel
    CHAN_TYPE_UDP_CLIENT,  // stateless UDP client channel
    CHAN_TYPE_TIMER        // channel for timerfd
};

// epoll output type for a channel, if all data are read/written
// set the correct ch_events
// chan_events is a simple state, not a bitmask.
// CHAN_EPOLLIN / CHAN_EPOLLOUT / CHAN_EPOLLERR are enum values,
// so always compare with ==, never with &.
typedef enum {
    CHAN_EPOLLNONE,
    CHAN_EPOLLIN,
    CHAN_EPOLLOUT,
    CHAN_EPOLLERR,
} chan_epoll_t;

struct chan_data {
    int sock;           // the socket
    enum chanType type; // channel type UDP/TCP ...
    struct chan_buffer *send;
    struct chan_buffer *recv;
    chan_epoll_t chan_events; // output
};

struct chan_buffer {
    struct chan_buffer *forw; // move +1 on the x-axis
    struct chan_buffer *back; // move -1
    char *data;
    int pos;
    int len;
};

extern long chan_open_max;
extern struct chan_data *channels;

int chan_init(void);
int chan_close(int);
int chan_sock(int);
int chan_epoll(int, struct epoll_event *, int, int);
int chan_connect(int, struct sockaddr_in *, int, int);
int chan_enqueue(int, struct chan_buffer *);
int chan_dequeue(int, struct chan_buffer **);
int chan_udp_socket(u_short);
int chan_tcp_listen_socket(u_short);
int chan_udp_client_socket(void);
int chan_tcp_client_socket(void);
int chan_accept(int, struct sockaddr_in *);
int chan_rpc(int, struct chan_buffer *,
             struct chan_buffer *,
             struct protocol_header *,
             int);
ssize_t chan_read(int, void *, size_t);
ssize_t chan_read_nonblock(int, void *, size_t, int);
ssize_t chan_write(int, void *, size_t);
int chan_alloc_buf(struct chan_buffer **, int);
int chan_free_buf(struct chan_buffer *);
int chan_open_sock(int, int);
int chan_dup_stdio(int);
int io_non_block(int);
int io_block(int);
int chan_send_dgram(int, char *, size_t, struct sockaddr_in *);
int chan_recv_dgram(int, void *, size_t, struct sockaddr_storage *, int);
int chan_create_timer(int);
struct chan_buffer *chan_make_buf(void);
int chan_connect_begin(int, struct sockaddr_in *, int);
int chan_connect_finish(int);
int chan_sock_error(int);
int chan_set_write_interest(int, int, int);
int rd_poll(int, int);
int connect_timeout(int, const struct sockaddr *, socklen_t, int);
int connect_begin(int, const struct sockaddr *, socklen_t);
int connect_finish(int);
int rdwr_sock_error(int);
int send_protocol_header(int, struct protocol_header *);
int recv_protocol_header(int, struct protocol_header *);
int chan_has_error(int);
const char *chan_addr_str(int);
int chan_client_socket(int, int, int);
