#pragma once
/* $Id: lib.h,v 1.8 2007/08/15 22:18:50 tmizan Exp $
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

/* Bug. This function contains lots of crap.
 */

#include "lsf/intlib/ll_bufsize.h"
#include "lsf/lib/lib.common.h"
#include "lsf/lib/lproto.h"
#include "lsf/lim/limout.h"
#include "lsf/lib/lib.xdr.h"

struct taskMsg {
    char *inBuf;
    char *outBuf;
    int len;
};

extern struct lsQueue * requestQ;
extern unsigned int requestSN;

enum lsTMsgType {
    LSTMSG_DATA,
    LSTMSG_IOERR,

    LSTMSG_EOF
};

struct lsTMsgHdr {
    enum lsTMsgType type;

    char *msgPtr;

    int len;
};

struct tid {
    int rtid;
    int sock;
    char *host;
    struct lsQueue *tMsgQ;
    bool_t isEOF;


    int refCount;


    int pid;
    u_short taskPort;
    struct tid *link;
};

#define getpgrp(n)  getpgrp()

#ifndef LOG_PRIMASK
#define LOG_PRIMASK     0xf
#define LOG_MASK(pri)   (1 << (pri))
#define LOG_UPTO(pri)   ((1 << ((pri)+1)) - 1)
#endif

#ifndef LOG_PRI
#define LOG_PRI(p)      ((p) & LOG_PRIMASK)
#endif


#define MIN_REF_NUM          1000
#define MAX_REF_NUM          32760

#define packshort_(buf, x)       memcpy(buf, (char *)&(x), sizeof(short))
#define packint_(buf, x)         memcpy(buf, (char *)&(x), sizeof(int))
#define pack_lsf_rlim_t_(buf, x) memcpy(buf, (char *)&(x), sizeof(lsf_rlim_t))

#define LSF_CONFDIR          0
#define LSF_SERVERDIR        1
#define LSF_LIM_DEBUG        2
#define LSF_RES_DEBUG        3
#define LSF_STRIP_DOMAIN     4
#define LSF_LIM_PORT         5
#define LSF_RES_PORT         6
#define LSF_LOG_MASK         7
#define LSF_SERVER_HOSTS     8
#define LSF_AUTH            9
#define LSF_USE_HOSTEQUIV   10
#define LSF_ID_PORT         11
#define LSF_RES_TIMEOUT     12
#define LSF_API_CONNTIMEOUT 13
#define LSF_API_RECVTIMEOUT 14
#define LSF_AM_OPTIONS      15
#define LSF_TMPDIR          16
#define LSF_LOGDIR          17
#define LSF_SYMBOLIC_LINK   18
#define LSF_MASTER_LIST     19
#define LSF_MLS_LOG         20
#define LSF_INTERACTIVE_STDERR 21

#define _NON_BLOCK_         0x01
#define _LOCAL_             0x02
#define _USE_TCP_           0x04
#define _KEEP_CONNECT_      0x08
#define _USE_PRIMARY_       0x10
#define _USE_PPORT_         0x20
#define _USE_UDP_           _LOCAL_

#define PRIMARY    0
#define MASTER     1
#define UNBOUND    2
#define TCP        3

#define CLOSEFD(s) if ((s) >= 0) {close((s)); (s) = -1;}

extern struct sockaddr_in sockIds_[];
extern int limchans_[];

extern struct config_param genParams_[];
extern struct sockaddr_in limSockId_;
extern struct sockaddr_in limTcpSockId_;
extern struct masterInfo masterInfo_;
extern int    masterknown_;
extern char   *indexfilter_;
extern char   *stripDomains_;

extern int readconfenv_(struct config_param *, struct config_param *, char *);
extern int ls_readconfenv(struct config_param *, char *);

extern int callLim_(enum limReqCode,
                    void *,
                    bool_t (*)(),
                    void *,
                    bool_t (*)(),
                    char *,
                    int,
                    struct packet_header *);

extern int initLimSock_(void);
extern void err_return_(enum limReplyCode);
extern struct hostLoad *loadinfo_(char *,
                                  struct decisionReq *,
                                  char *,
                                  int *,
                                  char ***);
extern struct hostent *Gethostbyname_(char *);
extern short getRefNum_(void);
extern void initLSFHeader_(struct packet_header *);

extern char **placement_(char *, struct decisionReq *, char *, int *);

extern int sig_encode(int);
extern int sig_decode(int);
extern char *getSigSymbolList(void);
extern char *getSigSymbol (int);

extern void ls_errlog(FILE *fd, const char *fmt, ...);
extern void ls_verrlog(FILE *fd, const char *fmt, va_list ap);
