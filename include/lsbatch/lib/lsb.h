/* $Id: lsb.h,v 1.6 2007/08/15 22:18:47 tmizan Exp $
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

#include "lsbatch.h"
#include "lsf/lib/list.h"
#include "lsf/lib/bitset.h"
#include "lsf/lib/intlibout.h"
#include "lsf/lib/tcl_stub.h"
#include "lsf/lib/jidx.h"
#include "lsf/lib/lib.h"
#include "lsbatch/daemons/daemonout.h"
#include "lsbatch/lib/lsb.xdr.h"

#ifndef MIN
#define MIN(a, b)                                                              \
    ({                                                                         \
        typeof(a) _a = (a);                                                    \
        typeof(b) _b = (b);                                                    \
        _a < _b ? _a : _b;                                                     \
    })
#endif

#ifndef MAX
#define MAX(a, b)                                                              \
    ({                                                                         \
        typeof(a) _a = (a);                                                    \
        typeof(b) _b = (b);                                                    \
        _a > _b ? _a : _b;                                                     \
    })
#endif

// Lavalite this has space for LAVALITE_VERSION_STR
#define MAX_VERSION_LEN LL_BUFSIZ_32
#define MAX_GROUPS 150
#define MAX_CHARLEN 20
#define MAX_CMD_DESC_LEN 256
#define MAX_USER_EQUIVALENT 128
#define DEFAULT_MSG_DESC "no description"

extern struct config_param lsbParams[];
extern int initenv_(struct config_param *, char *);
extern int sig_encode(int);

#define DEFAULT_API_CONNTIMEOUT 10
#define DEFAULT_API_RECVTIMEOUT 0

#define LSB_DEBUG 0
#define LSB_SHAREDIR 1
#define LSB_SBD_PORT 2
#define LSB_MBD_PORT 3
#define LSB_DEBUG_CMD 4
#define LSB_TIME_CMD 5
#define LSB_CMD_LOGDIR 6
#define LSB_CMD_LOG_MASK 7
#define LSB_API_CONNTIMEOUT 9
#define LSB_API_RECVTIMEOUT 10
#define LSB_SERVERDIR 11
#define LSB_MODE 12
#define LSB_SHORT_HOSTLIST 13
#define LSB_INTERACTIVE_STDERR 14
#define LSB_32_PAREN_ESC 15

#define LSB_API_QUOTE_CMD 14

typedef struct lsbSubSpoolFile {
    char inFileSpool[MAXFILENAMELEN];
    char commandSpool[MAXFILENAMELEN];
} LSB_SUB_SPOOL_FILE_T;

extern int serv_connect(char *, ushort, int);
extern int callmbd(char *, char *, int, char **, struct packet_header *, int *,
                   int (*)(), int *);
extern int cmdCallSBD_(char *, char *, int, char **, struct packet_header *,
                       int *);

extern int authTicketTokens_(struct lsfAuth *, char *);

extern char *getNextValue0(char **line, char, char);
extern int readNextPacket(char **, int, struct packet_header *, int);
extern void closeSession(int);
extern char *getMasterName(void);
extern ushort get_mbd_port(void);
extern ushort get_sbd_port(void);
extern int getAuth(struct lsfAuth *);
extern int getCommonParams(struct submit *, struct submitReq *,
                           struct submitReply *);
extern int getOtherParams(struct submit *, struct submitReq *,
                          struct submitReply *, struct lsfAuth *,
                          LSB_SUB_SPOOL_FILE_T *);
extern void prtBETime_(struct submit *);
extern int runBatchEsub(struct lenData *, struct submit *);
