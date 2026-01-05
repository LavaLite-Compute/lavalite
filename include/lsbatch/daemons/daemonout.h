/* $Id: daemonout.h,v 1.18 2007/08/15 22:18:44 tmizan Exp $
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

#include "lsbatch/lib/lsb.h"
#include "lsbatch/lib/lsb.sig.h"

#define ALL_HOSTS "all"

#define PUT_LOW(word, s) (word = (s | (word & ~0x0000ffff)))
#define PUT_HIGH(word, s) (word = ((s << 16) | (word & 0x0000ffff)))
#define GET_LOW(s, word) (s = word & 0x0000ffff)
#define GET_HIGH(s, word) (s = (word >> 16) & 0x0000ffff)

#define RSCHED_LISTSEARCH_BY_EXECJID 0
#define RSCHED_LISTSEARCH_BY_EXECLUSNAME 1

/* Library to daemon protocol operations
 */
typedef enum {
    BATCH_STATUS_ACK = 1, // ack from daemon to daemon
    BATCH_JOB_SUB,
    BATCH_JOB_INFO,
    BATCH_JOB_PEEK,
    BATCH_JOB_SIG,
    BATCH_HOST_INFO,
    BATCH_QUE_INFO,
    BATCH_GRP_INFO,
    BATCH_QUE_CTRL,
    BATCH_RECONFIG,
    BATCH_HOST_CTRL,
    BATCH_JOB_SWITCH,
    BATCH_JOB_MOVE,
    BATCH_JOB_MIG,
    BATCH_STATUS_JOB,
    BATCH_SLAVE_RESTART,
    BATCH_USER_INFO,
    BATCH_PARAM_INFO,
    BATCH_JOB_MODIFY,
    BATCH_JOB_EXECED,
    BATCH_JOB_MSG,
    BATCH_STATUS_MSG_ACK,
    BATCH_RESOURCE_INFO,
    BATCH_RUSAGE_JOB,
    BATCH_JOB_FORCE,
    BATCH_STATUS_CHUNK,
    BATCH_SET_JOB_ATTR,
    SBD_REGISTER,         // send registration request to mbd
    SBD_REGISTER_REPLY,   // reply to sbd the reistration was received
    BATCH_NEW_JOB_ACK,    // ack mbd got the pid for the new job
    BATCH_LAST_OP
} mbdReqType;

#define SUB_RLIMIT_UNIT_IS_KB 0x80000000

struct submitReq {
    int options;
    int options2;
    char *jobName;
    char *queue;
    int numAskedHosts;
    char **askedHosts;
    char *resReq;
    int rLimits[LSF_RLIM_NLIMITS];
    char *hostSpec;
    int numProcessors;
    char *dependCond;
    time_t beginTime;
    time_t termTime;
    int sigValue;
    char *subHomeDir;
    char *inFile;
    char *outFile;
    char *errFile;
    char *command;
    char *inFileSpool;
    char *commandSpool;
    time_t chkpntPeriod;
    char *chkpntDir;
    int restartPid;
    int nxf;
    struct xFile *xf;
    char *jobFile;
    char *fromHost;
    time_t submitTime;
    int umask;
    char *cwd;
    char *preExecCmd;
    char *mailUser;
    char *projectName;
    int niosPort;
    int maxNumProcessors;
    char *loginShell;
    char *schedHostType;
    char *userGroup;
    int userPriority;
};

#define SHELLLINE "#! /bin/sh\n\n"
#define CMDSTART "# LSBATCH: User input\n"
#define CMDEND "# LSBATCH: End user input\n"
#define ENVSSTART "# LSBATCH: Environments\n"
#define LSBNUMENV "#LSB_NUM_ENV="
#define EDATASTART "# LSBATCH: edata\n"
#define EXITCMD "exit `expr $? \"|\" $ExitStat`\n"
#define WAITCLEANCMD "\nExitStat=$?\nwait\n# LSBATCH: End user input\ntrue\n"
#define TAILCMD "'; export "
#define TRAPSIGCMD "$LSB_TRAPSIGS\n$LSB_RCP1\n$LSB_RCP2\n$LSB_RCP3\n"
#define JOB_STARTER_KEYWORD "%USRCMD"
#define SCRIPT_WORD "_USER_\\SCRIPT_"
#define SCRIPT_WORD_END "_USER_SCRIPT_"

struct submitMbdReply {
    int64_t jobId;
    char *queue;
    int badReqIndx;
    char *badJobName;
};

struct modifyReq {
    int64_t jobId;
    char *jobIdStr;
    int delOptions;
    int delOptions2;
    struct submitReq submitReq;
};

struct jobInfoReq {
    int options;
    char *userName;
    int64_t jobId;
    char *jobName;
    char *queue;
    char *host;
};

struct jobInfoReply {
    int64_t jobId;
    int status;
    int *reasonTb;
    int numReasons;
    int reasons;
    int subreasons;
    time_t startTime;
    time_t predictedStartTime;
    time_t endTime;
    float cpuTime;
    int numToHosts;
    char **toHosts;
    int nIdx;
    float *loadSched;
    float *loadStop;
    int userId;
    char *userName;
    int execUid;
    int exitStatus;
    char *execHome;
    char *execCwd;
    char *execUsername;
    struct submitReq *jobBill;
    time_t reserveTime;
    int jobPid;
    time_t jRusageUpdateTime;
    struct jRusage runRusage;
    int jType;
    char *parentGroup;
    char *jName;
    int counter[NUM_JGRP_COUNTERS];
    u_short port;
    int jobPriority;
};

struct infoReq {
    int options;
    int numNames;
    char **names;
    char *resReq;
};

struct userInfoReply {
    int badUser;
    int numUsers;
    struct userInfoEnt *users;
};

struct queueInfoReply {
    int badQueue;
    int numQueues;
    int nIdx;
    struct queueInfoEnt *queues;
};

struct hostDataReply {
    int badHost;
    int numHosts;
    int nIdx;
    int flag;
#define LOAD_REPLY_SHARED_RESOURCE 0x1
    struct hostInfoEnt *hosts;
};

struct groupInfoReply {
    int numGroups;
    struct groupInfoEnt *groups;
};

struct jobPeekReq {
    int64_t jobId;
};

struct jobPeekReply {
    char *outFile;
    char *pSpoolDir;
};

struct signalReq {
    int sigValue;
    int64_t jobId;
    time_t chkPeriod;
    int actFlags;
};

struct jobMoveReq {
    int opCode;
    int64_t jobId;
    int position;
};

struct jobSwitchReq {
    int64_t jobId;
    char queue[MAXLSFNAMELEN];
};

struct controlReq {
    int opCode;
    char *name;
};

struct migReq {
    int64_t jobId;
    int options;
    int numAskedHosts;
    char **askedHosts;
};

typedef enum {
    MBD_NEW_JOB_KEEP_CHAN = 1,
    MBD_NEW_JOB,
    MBD_SIG_JOB,
    MBD_SWIT_JOB,
    MBD_PROBE,
    CMD_SBD_DEBUG,
    MBD_MODIFY_JOB,
    SBD_JOB_SETUP,
    SBD_SYSLOG,
    SBD_DONE_MSG_JOB,
    RM_JOB_MSG,
    RM_CONNECT,
    CMD_SBD_REBOOT,
    CMD_SBD_SHUTDOWN,
    SBD_LAST_OP
} sbdReqType;

struct lenDataList {
    int numJf;
    struct lenData *jf;
};

extern void initTab(struct hTab *tabPtr);
extern hEnt *addMemb(struct hTab *tabPtr, int64_t member);
extern char remvMemb(struct hTab *tabPtr, int64_t member);
extern hEnt *chekMemb(struct hTab *tabPtr, int64_t member);
extern hEnt *addMembStr(struct hTab *tabPtr, char *member);
extern char remvMembStr(struct hTab *tabPtr, char *member);
extern hEnt *chekMembStr(struct hTab *tabPtr, char *member);
extern void convertRLimit(int *pRLimits, int toKb);
extern int limitIsOk_(int *rLimits);

extern int handShake_(int, char, int);

#define CALL_SERVER_NO_WAIT_REPLY 0x1
#define CALL_SERVER_USE_SOCKET 0x2
#define CALL_SERVER_NO_HANDSHAKE 0x4
#define CALL_SERVER_ENQUEUE_ONLY 0x8
extern int call_server(char *, ushort, char *, int, char **,
                       struct packet_header *, int, int, int *, int (*)(),
                       int *, int);

extern int sndJobFile_(int, struct lenData *);

extern struct group *mygetgrnam(const char *);
extern void freeUnixGrp(struct group *);
extern struct group *copyUnixGrp(struct group *);

extern void freeGroupInfoReply(struct groupInfoReply *reply);
extern void appendEData(struct lenData *jf, struct lenData *ed);
