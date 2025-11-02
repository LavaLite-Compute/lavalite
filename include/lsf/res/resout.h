/* $Id: resout.h,v 1.6 2007/08/15 22:18:56 tmizan Exp $
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 *
 */

#ifndef _RESOUT_H_
#define _RESOUT_H_

#include "lsf/lib/lib.common.h"
#include "lsf/lib/lproto.h"

// Bug: this file is for compilation of the library only
// remote execution has been removed

/* BUG: legacy daemon path logic removed — stubbed for now
 */
#define getDaemonPath_(name, dir) ((char *)NULL)
#define saveDaemonDir_(dir)       ((void)0)

typedef enum {
    RESE_OK,
    RESE_SIGCHLD,
    RESE_NOMORECONN,
    RESE_BADUSER,
    RESE_ROOTSECURE,
    RESE_DENIED,
    RESE_REQUEST,
    RESE_CALLBACK,
    RESE_NOMEM,
    RESE_FATAL,
    RESE_PTYMASTER,
    RESE_PTYSLAVE,
    RESE_SOCKETPAIR,
    RESE_FORK,
    RESE_REUID,
    RESE_CWD,
    RESE_INVCHILD,
    RESE_KILLFAIL,
    RESE_VERSION,
    RESE_DIRW,
    RESE_NOLSF_HOST,
    RESE_NOCLIENT,
    RESE_RUSAGEFAIL,
    RESE_RES_PARENT,
    RESE_FILE,
    RESE_NOVCL,
    RESE_NOSYM,
    RESE_VCL_INIT,
    RESE_VCL_SPAWN,
    RESE_EXEC,
    RESE_ERROR_LAST,
    RESE_MLS_INVALID,
    RESE_MLS_CLEARANCE,
    RESE_MLS_DOMINATE,
    RESE_MLS_RHOST,
} resAck;

typedef enum {
    RES_CONNECT = 1,
    RES_SERVER  = 2,
    RES_EXEC    = 3,
    RES_SETENV  = 4,
    RES_INITTTY = 5,
    RES_RKILL   = 6,
    RES_CONTROL = 7,
    RES_CHDIR   = 8,
    RES_GETPID  = 9,
    RES_RUSAGE  =10,
    RES_NONRES  =11,
    RES_DEBUGREQ  =12,
    RES_ACCT  =13,
    RES_SETENV_ASYNC = 14,
    RES_INITTTY_ASYNC = 15,
    RES_RMI  =99
} resCmd;

struct resConnect {
    struct lenData eexec;
};

struct resCmdBill {
    u_short retport;
    int rpid;
    int filemask;
    int priority;
    int options;
    char cwd[MAXPATHLEN];
    char **argv;
    struct lsfLimit lsfLimits[LSF_RLIM_NLIMITS];
};

struct resSetenv {
    char **env;
};

#define RES_RID_ISTID          0x01
#define RES_RID_ISPID          0x02

struct resRKill {
    int rid;
    int whatid;
    int signal;
};

struct resChdir {
    char dir[MAXFILENAMELEN];
};

struct resControl {
    int opCode;
    int data;
};

struct resStty {
    struct termios termattr;
    struct winsize ws;
};

struct resPid {
    int rpid;
    int pid;
};


#define RES_RPID_KEEPPID 0x01

struct resRusage {
    int    rid;
    int    whatid;
    int    options;
};

typedef struct relaybuf {
    char       buf[LL_BUFSIZ_64];
    char       *bp;
    int        bcount;
} RelayBuf;

#define LINE_BUFSIZ 4096

typedef struct relaylinebuf {
    char       buf[LL_BUFSIZ_64];
    char       *bp;
    int        bcount;
} RelayLineBuf;

typedef struct channel {
        int        fd;
        RelayBuf   *rbuf;
        int        rcount;
        RelayBuf   *wbuf;
        int        wcount;
} Channel;

typedef struct nioschannel {
        int            fd;
        RelayBuf       *rbuf;
        int            rcount;
        RelayLineBuf   *wbuf;
        int            wcount;
        int            opCode;
} niosChannel;


typedef struct outputchannel {
    int             fd;
    int             endFlag;
    int             retry;
    int             bytes;
    RelayLineBuf    buffer;
} outputChannel;



struct niosConnect {
    int rpid;
    int exitStatus;
    int terWhiPendStatus;
};


struct niosStatus {
    resAck ack;
    struct sigStatusUsage {
        int ss;
        struct rusage ru;
    } s;
};

struct resSignal {
    int pid;
    int sigval;
};

#endif
