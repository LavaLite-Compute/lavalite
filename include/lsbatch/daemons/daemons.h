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

#include "lsbatch/lib/lsb.h"
#include "lsbatch/daemons/daemonout.h"

#define MIN_CPU_TIME 0.0001

#define FATAL_ERR -1
#define WARNING_ERR -2

#define MASK_INT_JOB_STAT 0x000FFFFF
#define MASK_STATUS(s) ((s) & MASK_INT_JOB_STAT)

#define JOB_STAT_CHKPNTED_ONCE 0x10000000

#define JOB_STAT_RESERVE 0x20000000

#define JOB_STAT_MIG 0x40000000

#define JOB_STAT_MODIFY_ONCE 0x01000000

#define JOB_STAT_ZOMBIE 0x02000000

#define JOB_STAT_PRE_EXEC 0x04000000

#define JOB_STAT_SIGNAL 0x08000000

#define JOB_STAT_KILL 0x00800000
#define JOB_STAT_RSRC_PREEMPT_WAIT 0x00400000
#define JOB_STAT_VOID 0x00100000

#define SET_STATE(s, n) ((s) = ((s) & ~(MASK_INT_JOB_STAT)) | (n))

#define SBD_SET_STATE(jp, n)                                                   \
    {                                                                          \
        (jp->jobSpecs.jStatus) =                                               \
            ((jp->jobSpecs.jStatus) & ~(MASK_INT_JOB_STAT)) | (n);             \
        sbdlog_newstatus(jp);                                                  \
    }

#define IS_RUN_JOB_CMD(s) (((s) & JOB_STAT_RUN) && !((s) & JOB_STAT_PRE_EXEC))

#define MAX_FAIL 5
#define MAX_SEQ_NUM INFINIT_INT

#define DEF_MSLEEPTIME 60
#define DEF_SSLEEPTIME 30
#define DEF_RETRY_INTVL 2
#define DEF_PREEM_PERIOD INFINIT_INT
#define DEF_PG_SUSP_IT 180
#define WARN_TIME 600
#define DEF_RUSAGE_UPDATE_RATE 1
#define DEF_RUSAGE_UPDATE_PERCENT 10
#define DEF_JTERMINATE_INTERVAL 10
#define SLAVE_FATAL 101
#define SLAVE_MEM 102
#define SLAVE_RESTART 103
#define SLAVE_SHUTDOWN 104

#define NOT_LOG INFINIT_INT

#define JOB_SAVE_OUTPUT 0x10000000
#define JOB_FORCE_KILL 0x20000000

#define JOB_URGENT 0x40000000
#define JOB_URGENT_NOSTOP 0x80000000

extern char errbuf[MAXLINELEN];

#define lsb_merr1(fmt, a1) sprintf(errbuf, fmt, a1), lsb_merr(errbuf)
#define lsb_merr2(fmt, a1, a2) sprintf(errbuf, fmt, a1, a2), lsb_merr(errbuf)
#define lsb_merr3(fmt, a1, a2, a3)                                             \
    sprintf(errbuf, fmt, a1, a2, a3), lsb_merr(errbuf)
#define lsb_mperr1(fmt, a1) sprintf(errbuf, fmt, a1), lsb_mperr(errbuf)
#define lsb_mperr2(fmt, a1, a2) sprintf(errbuf, fmt, a1, a2), lsb_mperr(errbuf)
#define lsb_mperr3(fmt, a1, a2, a3)                                            \
    sprintf(errbuf, fmt, a1, a2, a3), lsb_mperr(errbuf)

// LavaLite
const char *job_state_str(int);
const char *mbd_op_str(mbdReqType);

typedef enum {
    ERR_NO_ERROR = 1,
    ERR_BAD_REQ,
    ERR_NO_JOB,
    ERR_NO_FILE,
    ERR_FORK_FAIL,
    ERR_NO_USER,
    ERR_LOCK_FAIL,
    ERR_NO_LIM,
    ERR_MEM,
    ERR_NULL,
    ERR_FAIL,
    ERR_BAD_REPLY,
    ERR_JOB_QUOTA,
    ERR_JOB_FINISH,
    ERR_CHKPNTING,
    ERR_ROOT_JOB,
    ERR_SYSACT_FAIL,
    ERR_SIG_RETRY,
    ERR_HOST_BOOT,
    ERR_PID_FAIL,
    ERR_SOCKETPAIR,
    ERR_UNREACH_SBD,
    ERR_JOB_TOO_LARGE,
    ERR_DUP_JOB
} sbdReplyType;

#define LOAD_REASONS                                                           \
    (SUSP_LOAD_REASON | SUSP_QUE_STOP_COND | SUSP_QUE_RESUME_COND |            \
     SUSP_PG_IT | SUSP_LOAD_UNAVAIL | SUSP_HOST_LOCK | SUSP_HOST_LOCK_MASTER)

struct thresholds {
    int nIdx;
    int nThresholds;
    float **loadStop;
    float **loadSched;
};
// LavaLite job specification with the job file
struct jobSpecs {
    int64_t jobId;                    // unique job identifier (cluster-wide)
    char    jobName[LL_BUFSIZ_512];      // user-visible job name

    int     jStatus;                  // scheduler job status flags (JOB_STAT_*)
    int     reasons;                  // why job is pending / suspended (legacy semantics)
    int     subreasons;               // finer-grained reasons (optional; may be deprecated)

    int     userId;                   // submitting user uid
    char    userName[LL_BUFSIZ_64] ;  // submitting username

    int     options;                  // submission options (SUB_*)
    int     jobPid;                   // process-id of running job (set by sbatchd)
    int     jobPGid;                  // process group-id of job (leader is child)

    char    queue[LL_BUFSIZ_64];     // queue name (policy entry)
    int     priority;                 // internal scheduling priority

    char    fromHost[MAXHOSTNAMELEN]; // submission host / routing source

    time_t  startTime;                // time job actually started
    int     runTime;                  // runtime used so far (seconds)

    int     numToHosts;   // number of candidate execution hosts
    char  **toHosts;      // list of exec hostnames build MPI hostfile

    int     jAttrib;                  // job attributes bitmask (ATTR_*)
    int     sigValue;                 // signal value used for control actions

    time_t  termTime;     // planned termination time or checkpoint deadline

    char    subHomeDir[PATH_MAX]; // submission-side $HOME
    char    command[LL_BUFSIZ_512];        // original command line

    char   job_file[PATH_MAX]; // script name/path
    struct wire_job_file job_file_data; // inline script content (blob received via XDR)

    char    inFile[PATH_MAX];     // stdin redirect path
    char    outFile[PATH_MAX];    // stdout redirect path
    char    errFile[PATH_MAX];    // stderr redirect path

    int     umask;                     // umask for job execution
    char    cwd[PATH_MAX];       // job working directory at submission

    time_t  submitTime;                // job submission timestamp

    char    preExecCmd[LL_BUFSIZ_512];    // command run before job exec

    int     numEnv;      // number of env vars passed
    char  **env;        // environment array (NULL-terminated pairs no required)

    struct lenData eexec;              // encrypted exec string
    char    projectName[LL_BUFSIZ_512];// project/accounting tag

    char    preCmd[LL_BUFSIZ_512];        // optional pre-run wrapper
    char    postCmd[LL_BUFSIZ_512];       // optional post-run wrapper
};


struct statusReq {
    int64_t jobId;
    int jobPid;
    int jobPGid;
    int newStatus;
    int reason;
    int subreasons;
    int seq;
    sbdReplyType sbdReply;
    struct lsfRusage lsfRusage;
    int actPid;
    int execUid;
    int numExecHosts;
    char **execHosts;
    int exitStatus;
    char *execHome;
    char *execCwd;
    char *execUsername;
    char *queuePreCmd;
    char *queuePostCmd;
    int msgId;
    struct jRusage runRusage;
    int sigValue;
    int actStatus;
};

struct chunkStatusReq {
    int numStatusReqs;
    struct statusReq **statusReqs;
};

struct sbdPackage {
    int managerId;
    char lsbManager[MAXLSFNAMELEN];
    int mbdPid;
    int sbdSleepTime;
    int retryIntvl;
    int preemPeriod;
    int pgSuspIdleT;
    int maxJobs;
    int numJobs;
    struct jobSpecs *jobs;
    int uJobLimit;
    int rusageUpdateRate;
    int rusageUpdatePercent;
    int jobTerminateInterval;
};

struct jobSig {
    int64_t jobId;
    int sigValue;
    time_t chkPeriod;
    int actFlags;
    char *actCmd;
    int reasons;
    int subReasons;
    int64_t newJobId;
};

struct jobReply {
    int64_t jobId;
    int jobPid;
    int jobPGid;
    int jStatus;
    int reasons;
    int actPid;
    int actValue;
    int actStatus;
};

enum _bufstat { MSG_STAT_QUEUED, MSG_STAT_SENT, MSG_STAT_RCVD };

typedef struct proto proto_t;
struct proto {
    int usrId;
    int64_t jobId;
    int msgId;
    int type;
    int instance;
    int (*sndfnc)(int, char *, int);
    int (*rcvfnc)(int, char *, int);
};

struct bucket {
    struct bucket *forw;
    struct bucket *back;
    struct Buffer *storage;
    enum _bufstat bufstat;
    proto_t proto;
    XDR xdrs;
};

#define NEW_BUCKET(BUCKET, chanBuf)                                            \
    {                                                                          \
        BUCKET = (struct bucket *) malloc(sizeof(struct bucket));              \
        if (BUCKET) {                                                          \
            BUCKET->proto.usrId = -1;                                          \
            BUCKET->proto.jobId = -1;                                          \
            BUCKET->proto.instance = -1;                                       \
            BUCKET->proto.sndfnc = b_write_fix;                                \
            BUCKET->proto.rcvfnc = b_read_fix;                                 \
            BUCKET->xdrs.x_ops = 0;                                            \
            BUCKET->storage = chanBuf;                                         \
        } else {                                                               \
            lsberrno = LSBE_NO_MEM;                                            \
        }                                                                      \
    }

#define FREE_BUCKET(BUCKET)                                                    \
    {                                                                          \
        if (BUCKET->xdrs.x_ops)                                                \
            xdr_destroy(&BUCKET->xdrs);                                        \
        free(BUCKET);                                                          \
    }

#define QUEUE_INIT(pred)                                                       \
    {                                                                          \
        struct bucket *_bucket_;                                               \
        NEW_BUCKET(_bucket_, NULL);                                            \
        pred = _bucket_;                                                       \
        pred->forw = pred->back = _bucket_;                                    \
    }

#define QUEUE_DESTROY(pred)                                                    \
    {                                                                          \
        struct bucket *bp, *bpnxt;                                             \
        for (bp = pred->forw; bp != pred; bp = bpnxt) {                        \
            bpnxt = bp->forw;                                                  \
            FREE_BUCKET(bp);                                                   \
        }                                                                      \
        FREE_BUCKET(pred);                                                     \
    }

#define QUEUE_APPEND(entry, pred)                                              \
    entry->back = pred->back;                                                  \
    entry->forw = pred;                                                        \
    pred->back->forw = entry;                                                  \
    pred->back = entry;

#define QUEUE_REMOVE(entry)                                                    \
    entry->back->forw = entry->forw;                                           \
    entry->forw->back = entry->back;

#define LSBMSG_DECL(hdr, jm)                                                   \
    char _src_[LSB_MAX_SD_LENGTH];                                             \
    char _dest_[LSB_MAX_SD_LENGTH];                                            \
    char _strBuf_[MSGSIZE];                                                    \
    struct lsbMsgHdr hdr;                                                      \
    struct lsbMsg jm;

#define LSBMSG_INIT(hdr, jm)                                                   \
    hdr.src = _src_;                                                           \
    hdr.dest = _dest_;                                                         \
    jm.header = &hdr;                                                          \
    jm.msg = _strBuf_;

#define LSBMSG_FINALIZE(xdrs, jm)                                              \
    if (xdrs->x_op == XDR_DECODE && jm.msg)                                    \
        free(jm.msg);

#define LSBMSG_CACHE_BUFFER(bucket, jm)                                        \
    bucket->proto.usrId = jm.header->usrId;                                    \
    bucket->proto.jobId = jm.header->jobId;                                    \
    bucket->proto.msgId = jm.header->msgId;                                    \
    bucket->proto.type = jm.header->type;

extern int errno;
extern char **environ;

extern struct config_param daemonParams[];

extern int nextJobId;
extern int numRemoveJobs;
extern int maxJobId;
extern int lsb_CheckMode;
extern int lsb_CheckError;
extern ushort mbd_port;
extern int batchSock;
extern char masterme;
extern char *masterHost;
extern char *clusterName;
extern time_t now;
extern int retryIntvl;
extern int sbdSleepTime;
extern int preemPeriod;
extern int pgSuspIdleT;
extern char *env_dir;
extern struct lsInfo *allLsInfo;
extern struct tclLsInfo *tclLsInfo;
extern int rusageUpdateRate;
extern int rusageUpdatePrecent;
extern int jobTerminateInterval;
extern int lsf_crossUnixNT;

#define DEFAULT_MAILTO "^U"

#define DEFAULT_MAILPROG "/usr/lib/sendmail"
#define DEFAULT_CRDIR "/bin"

extern FILE *smail(char *to, char *tohost);
extern uid_t chuser(uid_t uid);
extern int get_ports(void);
extern void die(int sig);
extern char *my_malloc(int size, char *caller);
extern char *my_calloc(int num, int size, char *caller);
extern void lsb_merr(char *s);
extern void merr_user(char *user, char *host, char *msg, char *type);
extern int portok(struct sockaddr_in *from);
extern char *safeSave(char *);
extern void lsb_mperr(char *msg);
extern void mclose(FILE *file);
extern void relife(void);
extern int getElock(void);
extern int touchElock(void);
extern void getElogLock(void);
extern void touchElogLock(void);
extern void releaseElogLock(void);
extern struct listEntry *tmpListHeader(struct listEntry *listHeader);
extern struct tclLsInfo *getTclLsInfo(void);
extern struct resVal *checkThresholdCond(char *);
extern int *getResMaps(int, char **);
extern int checkResumeByLoad(int64_t, int, struct thresholds, struct hostLoad *,
                             int *, int *, int, struct resVal *,
                             struct tclHostData *);
extern void closeExceptFD(int);
extern void freeLsfHostInfo(struct hostInfo *, int);
extern void copyLsfHostInfo(struct hostInfo *, struct hostInfo *);
extern void freeTclHostData(struct tclHostData *);
extern void lsbFreeResVal(struct resVal **);

int initTcl(struct tclLsInfo *);

extern void freeWeek(windows_t **);
extern void errorBack(int, int, struct sockaddr_in *);

extern int init_ServSock(u_short port);
extern int server_reply(int, char *, int);
extern int rcvJobFile(int, struct lenData *);

#define FORK_REMOVE_SPOOL_FILE (0x1)
#define CALL_RES_IF_NEEDED (0x2)
extern void childRemoveSpoolFile(const char *, int, const struct passwd *);

extern int xdr_statusReq(XDR *, struct statusReq *, struct packet_header *);
extern int xdr_sbdPackage(XDR *, struct sbdPackage *, struct packet_header *);
extern int xdr_sbdPackage1(XDR *xdrs, struct sbdPackage *,
                           struct packet_header *);
extern int xdr_jobReply(XDR *xdrs, struct jobReply *jobReply,
                        struct packet_header *);
extern int xdr_jobSig(XDR *xdrs, struct jobSig *jobSig, struct packet_header *);
extern int xdr_chunkStatusReq(XDR *, struct chunkStatusReq *,
                              struct packet_header *);

extern float normalizeRq_(float rawql, float cpuFactor, int nprocs);

extern void daemon_doinit(void);

extern void scaleByFactor(int *, int *, float);


// LavaLite

// an ack that we have received the lastest status from sbd
// sbd enforces the porder job_reply -> job_execute -> job_finish
// with some rusage update between job_execute and job_finish
struct job_status_ack {
    int64_t job_id; // job identifier being acknowledged
    int32_t seq;    // correlation sequence
    int32_t acked_op; // opcode being acknowledged (e.g. BATCH_STATUS_JOB)
};

bool_t xdr_jobSpecs(XDR *xdrs, struct jobSpecs *jobSpecs, void *);
bool_t xdr_job_status_ack(XDR *,
                          struct job_status_ack *,
                          struct packet_header *);

int enqueue_header_reply(int, int);
// xdr_encodeMsg() uses old-style bool_t (*xdr_func)() so we keep the same type.
int enqueue_payload(int, int, void *, bool_t (*xdr_func)());
int enqueue_payload_bufsiz(int, int, void *,
                           bool_t (*xdr_func)(), size_t bufsz);

// Bug fix this extern the function is in mbd.h
void freeJobSpecs(struct jobSpecs *);
const char *batch_op2str(int);

// a simple view of the job we decided to signal
struct xdr_sig_sbd_jobs {
    int32_t sig;
    int32_t flags;
    uint32_t n;
    int64_t *job_ids;
};
bool_t xdr_sig_sbd_jobs(XDR *, struct xdr_sig_sbd_jobs *);

// sbd to mbd
struct wire_job_sig_reply {
    int64_t job_id;
    int sig;  // POSIX signal that was requested/delivered
    int32_t rc;            // LSBE_*
    int32_t detail_errno;  // errno from kill/killpg or 0
};

bool_t xdr_wire_job_sig_reply(XDR *, struct wire_job_sig_reply *);
