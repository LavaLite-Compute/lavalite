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

// Seconds
#define DEFAULT_API_CONNTIMEOUT 10
#define DEFAULT_API_RECVTIMEOUT 10

typedef enum lsb_params {
    // Shared daemon / library parameters */
    LSB_DEBUG,
    LSB_CONFDIR,
    LSB_SHAREDIR,
    LSB_MAILTO,
    LSB_MAILPROG,
    LSB_DEBUG_MBD,
    LSB_TIME_MBD,
    LSB_SIGSTOP,
    LSB_MBD_CONNTIMEOUT,
    LSB_MBD_MAILREPLAY,
    LSB_MBD_MIGTOPEND,
    LSB_MBD_BLOCK_SEND,
    LSB_MEMLIMIT_ENFORCE,
    LSB_BSUBI_OLD,
    LSB_STOP_IGNORE_IT,
    LSB_HJOB_PER_SESSION,
    LSB_REQUEUE_HOLD,
    LSB_SMTP_SERVER,
    LSB_MAILSERVER,
    LSB_MAILSIZE_LIMIT,
    LSB_REQUEUE_TO_BOTTOM,
    LSB_ARRAY_SCHED_ORDER,
    LSB_QPOST_EXEC_ENFORCE,
    LSB_MIG2PEND,
    LSB_UTMP,
    LSB_JOB_CPULIMIT,
    LSB_JOB_MEMLIMIT,
    LSB_MOD_ALL_JOBS,
    LSB_SET_TMPDIR,
    LSB_PTILE_PACK,
    LSB_STDOUT_DIRECT,
    LSB_NO_FORK,
    LSB_DEBUG_CMD,
    LSB_TIME_CMD,
    LSB_CMD_LOGDIR,
    LSB_CMD_LOG_MASK,
    LSB_API_CONNTIMEOUT,
    LSB_API_RECVTIMEOUT,
    LSB_SERVERDIR,
    LSB_SHORT_HOSTLIST,
    LSB_INTERACTIVE_STDERR,
    LSB_NULL_PARAM,
    LSB_PARAM_COUNT       // sentinel
} lsb_param_t;

extern struct config_param lsbParams[];

#define LSB_API_QUOTE_CMD 14

typedef struct lsbSubSpoolFile {
    char inFileSpool[MAXFILENAMELEN];
    char commandSpool[MAXFILENAMELEN];
} LSB_SUB_SPOOL_FILE_T;

extern int serv_connect(const char *, ushort, int);

extern int authTicketTokens_(struct lsfAuth *, char *);

extern int getAuth(struct lsfAuth *);
extern int getCommonParams(struct submit *, struct submitReq *,
                           struct submitReply *);
extern int getOtherParams(struct submit *, struct submitReq *,
                          struct submitReply *, struct lsfAuth *,
                          LSB_SUB_SPOOL_FILE_T *);
extern void prtBETime_(struct submit *);
extern int runBatchEsub(struct lenData *, struct submit *);
uint16_t get_mbd_port(void);
uint16_t get_sbd_port(void);
struct wire_job_file;
int call_mbd(void *, size_t, char **, struct packet_header *,
             struct wire_job_file *);
int open_mbd_stream(void *, size_t, char **, struct packet_header *);
void close_mbd_stream(int);
char *resolve_master_with_retry(void);
char *resolve_master_try(void);
// LavaLite
int ll_validate_jobid(const char *, int64_t *);
