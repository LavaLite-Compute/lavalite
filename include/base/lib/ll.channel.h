/* Copyright (C) LavaLite Contributors
 * GLP v2
 */
#pragma once

#include "base/lib/ll.sys.h"
#include "base/lib/ll.wire.h"
#include "base/lib/ll.syslog.h"

#define CHAN_MAX 10204

// Channel type
enum chan_type {
    UDP_SERVER,
    TCP_SERVER,
    TCP_CONNECT,
    TCP_CLIENT,
    UDP_CLIENT,
    TIMER_FD,
};

// chan_events is a simple state state machine, not a bitmask.
// CHAN_EPOLLIN / CHAN_EPOLLOUT / CHAN_EPOLLERR are enum values,
// so always compare with ==, never with &.
enum chan_events {
    CHAN_EPOLLNONE,
    CHAN_EPOLLIN,
    CHAN_EPOLLOUT,
    CHAN_EPOLLERR,
};

struct chan_data {
    int sock;           // the socket
    enum chan_type type; // channel type UDP/TCP ...
    struct chan_buffer *send;
    struct chan_buffer *recv;
    enum chan_events chan_events; // output
};

struct chan_buffer {
    struct chan_buffer *forw; // move +1 on the x-axis
    struct chan_buffer *back; // move -1
    char *data;
    int pos;
    int len;
};

void chan_init(void);
int chan_close(int);
int chan_sock(int);
int chan_epoll(int, struct epoll_event *, int, int);
int chan_connect(int, struct sockaddr_in *, int, int);
int chan_enqueue(int, struct chan_buffer *);
int chan_dequeue(int, struct chan_buffer **);
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
int chan_open(int);
int chan_dup_stdio(int);
int io_non_block(int);
int io_block(int);
int chan_send_dgram(int, char *, size_t, struct sockaddr_in *);
int chan_recv_dgram(int, void *, size_t, struct sockaddr_in *, int);
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
int chan_udp_client(void);
int chan_tcp_client(void);
int chan_tcp_server(uint16_t);
int chan_udp_server(uint16_t);
int chan_is_readable(int);
