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

enum chanState { CH_FREE, CH_DISC, CH_PRECONN, CH_CONN, CH_WAIT, CH_INACTIVE };

enum chanType {
    CH_TYPE_UDP,
    CH_TYPE_TCP,
    CH_TYPE_LOCAL,
    CH_TYPE_PASSIVE,
    CH_TYPE_NAMEDPIPE
};

// Obsolete CHAN_OP_PPORT privilege port
#define CHAN_OP_CONNECT 0x02
#define CHAN_OP_RAW 0x04
#define CHAN_OP_NONBLOCK 0x10
#define CHAN_OP_CLOEXEC 0x20
#define CHAN_OP_SOREUSE 0x40

enum chan_block_mode { CHAN_MODE_BLOCK, CHAN_MODE_NONBLOCK };

struct chan_data {
    int sock;       // the socket
    enum chanType type;  // channel type UDP/TCP ...
    enum chanState state;  // connectes, waiting, inactive...
    struct Buffer *send;
    struct Buffer *recv;
};

struct Buffer {
    struct Buffer *forw; // move +1 on the x-axis
    struct Buffer *back; // move -1
    char *data;
    int pos;
    int len;
};

struct Masks {
    fd_set rmask;
    fd_set wmask;
    fd_set emask;
};

//legacy
int chanSelect_(struct Masks *, struct Masks *, struct timeval *);

int chan_init(void);
int chan_close(int);
int chan_connect(int, struct sockaddr_in *, int, int);
int chan_enqueue(int, struct Buffer *);
int chan_dequeue(int, struct Buffer **);
int chan_listen_socket(int, u_short, int, int);
int chan_accept(int, struct sockaddr_in *);
int chan_client_socket(int, int, int);
int chan_rpc(int, struct Buffer *, struct Buffer *,
             struct packet_header *, int);
ssize_t chan_read(int, void *, size_t);
ssize_t chan_read_nonblock(int, char *, int, int);
ssize_t chan_write(int, void *, size_t);
int chan_alloc_buf(struct Buffer **, int);
int chan_free_buf(struct Buffer *);
int chan_open_sock(int, int);
int chan_set_mode(int, int);
int ll_dup_stdio(int);
int io_non_block(int);
int io_block(int);
int chan_send_dgram(int, char *, size_t, struct sockaddr_in *);
int chan_recv_dgram_(int, void *, size_t, struct sockaddr_storage *, int);
int chan_get_sock(int);
