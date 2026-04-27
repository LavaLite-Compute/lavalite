/* Copyright (C) LavaLite Contributors
 * GPL v2
 */
#pragma once

#include <netinet/in.h>
#include <sys/epoll.h>

#include "base/lib/ll.protocol.h"
#include "base/lib/ll.list.h"

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

// chan_events is a simple state machine, not a bitmask.
// Always compare with ==, never with &.
enum chan_events {
    CHAN_EPOLLNONE,
    CHAN_EPOLLIN,
    CHAN_EPOLLOUT,
    CHAN_EPOLLERR,
};

struct chan_buffer {
    struct ll_list_entry link;  // intrusive list node
    char *data;
    int pos;
    int len;
};

struct chan_data {
    int sock;
    enum chan_type type;
    struct ll_list send;
    struct ll_list recv;
    enum chan_events chan_events;
};

extern struct chan_data channels[];

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
void chan_free_buf(struct chan_buffer *);
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
