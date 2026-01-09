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
#pragma once

#include "lsf/lib/lib.h"
#include "lsf/lib/ll.sys.h"

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
    struct Buffer *send;
    struct Buffer *recv;
    chan_epoll_t chan_events; // output
};

struct Buffer {
    struct Buffer *forw; // move +1 on the x-axis
    struct Buffer *back; // move -1
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
int chan_enqueue(int, struct Buffer *);
int chan_dequeue(int, struct Buffer **);
int chan_listen_socket(int, u_short, int, int);
int chan_accept(int, struct sockaddr_in *);
int chan_client_socket(int, int, int);
int chan_rpc(int, struct Buffer *, struct Buffer *, struct packet_header *,
             int);
ssize_t chan_read(int, void *, size_t);
ssize_t chan_read_nonblock(int, char *, int, int);
ssize_t chan_write(int, void *, size_t);
int chan_alloc_buf(struct Buffer **, int);
int chan_free_buf(struct Buffer *);
int chan_open_sock(int, int);
int chan_dup_stdio(int);
int io_non_block(int);
int io_block(int);
int chan_send_dgram(int, char *, size_t, struct sockaddr_in *);
int chan_recv_dgram(int, void *, size_t, struct sockaddr_storage *, int);
int chan_create_timer(int); // seconds for the timerfd
struct Buffer *chan_make_buf(void);
int chan_connect_begin(int, struct sockaddr_in *, int);
int chan_connect_finish(int);
