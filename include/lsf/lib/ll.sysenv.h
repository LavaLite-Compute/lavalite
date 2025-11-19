#pragma once
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

/* Library parameters defining entry in genParams_[] array
 * every system components has its own array of these variables
 * but in different order as some variables are specific to a
 * different component
 */

typedef enum {
    // Common parameters
    LSF_CONFDIR,
    LSF_SERVERDIR,
    LSF_LOGDIR,
    LSF_LIM_DEBUG,
    LSF_LIM_PORT,
    LSF_RES_PORT,
    LSF_LOG_MASK,
    LSF_MASTER_LIST,

    // LIM-specific
    LSF_DEBUG_LIM,
    LSF_TIME_LIM,
    LSF_LIM_IGNORE_CHECKSUM,
    LSF_LIM_JACKUP_BUSY,

    // lib.common specific
    LSF_SERVER_HOSTS,
    LSF_AUTH,
    LSF_API_CONNTIMEOUT,
    LSF_API_RECVTIMEOUT,
    LSF_INTERACTIVE_STDERR,

    // Legacy sentinel
    LSF_NULL_PARAM, // back-compatibility placeholder
    LSF_PARAM_COUNT // sentinel for array sizing
} ll_params_t;

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
