/*
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
/* Library parameters defining entry in genParams[] array
 * every system components has its own array of these variables
 * but in different order as some variables are specific to a
 * different component
 */


extern struct config_param genParams[];
extern struct masterInfo masterInfo;
extern int masterknown;

// Indexes in the limchans array based on transport
typedef enum {
    UDP,
    TCP,
    SOCK_COUNT // <-- sentinel
} lim_sock_t;

// Flags indicating what transport to use and its attributes
#define _USE_UDP_ 0x01
#define _USE_TCP_ 0x02
#define _NON_BLOCK_ 0x04
#define _KEEP_CONNECT_ 0x08

// historical alias; library now treats it purely as "use UDP"
#define _LOCAL_ _USE_UDP_

extern struct sockaddr_in sock_addr_in[];
extern int lim_chans[];

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>

int ll_atoi(const char *, int *);
int ll_atoll(const char *, int64_t *);
const char *ll_sig_to_str(int);
int ll_str_to_sig(const char *);
