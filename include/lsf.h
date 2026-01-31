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

// If it needs more than a Unix syscall, a struct, and libc, rethink it.

// Accept what the hardware gives you
// make it fast, make it clear, make it stop
// everything else is noise

// Stability isn’t the absence of problems — it’s the ability
// to recover gracefully.

// Simplicity and clarity over cleverness; minimal code, maximal intent
// Mathematicians chase proofs; computer scientists chase models;
// practitioners chase uptime.

// VP-level engineer or not, we all leave a little time capsule
// of “WTF” behind.

// Nice, time to let the robot chew through the fossils

// Fossil in → LavaLite out

// True man reads the man page, that’s why it’s called man

// Yes. Someone has suffered here before.

// Here’s a pointer. Here’s a number. Convert it. Don’t complain.”


#include <config.h>
// System headers
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <syslog.h>
#include <pwd.h>
#include <grp.h>
#include <ctype.h>
#include <limits.h>
#include <netdb.h>
#include <termios.h>
#include <signal.h>
#include <stdarg.h>
#include <dirent.h>
#include <math.h>
#include <float.h>
#include <poll.h>
#include <assert.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/resource.h>
#include <sys/timerfd.h>
#include <rpc/types.h>
#include <rpc/xdr.h>
#include <rpcsvc/ypclnt.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <sys/statvfs.h>
#include <arpa/inet.h>

// LavaLite
#include "ll_user_limits.h"
#include "ll_types.h"

// Define 64 bit types
typedef int64_t int64_t;
typedef uint64_t LS_UNS_LONG_INT;

/* A little macro that abstracts the access to the process status returned
 * by wait() or waitpid().
 */
#define LS_STATUS(s) (s)

/* Evaluate MAX or MIN ifdef it as they might be defined
 * in sys/params.h, which they are in POSIX environment.
 */
#ifndef MAX
#define MAX(a, b)                                                              \
    ({                                                                         \
        typeof(a) _a = (a);                                                    \
        typeof(b) _b = (b);                                                    \
        _a > _b ? _a : _b;                                                     \
    })
#endif

#ifndef MIN
#define MIN(a, b)                                                              \
    ({                                                                         \
        typeof(a) _a = (a);                                                    \
        typeof(b) _b = (b);                                                    \
        _a < _b ? _a : _b;                                                     \
    })
#endif

// 11 load indexes collected by lim on every cluster machine
enum lim_load_index {
    R15S = 0,
    R1M  = 1,
    R15M = 2,
    UT   = 3,
    PG   = 4,
    IO   = 5,
    LS   = 6,
    IT   = 7,
    TMP  = 8,
    SWP  = 9,
    MEM  = 10,
    LIM_NIDX = 11,   // number of built-in indices
    USR1 = 11,
    USR2 = 12
};

#define INFINIT_INT 0x7fffffff
#define INFINIT_LONG_INT 0x7fffffff

#define INFINIT_SHORT 0x7fff

#define DEFAULT_RLIMIT -1

#define LSF_RLIMIT_CPU 0
#define LSF_RLIMIT_FSIZE 1
#define LSF_RLIMIT_DATA 2
#define LSF_RLIMIT_STACK 3
#define LSF_RLIMIT_CORE 4
#define LSF_RLIMIT_RSS 5
#define LSF_RLIMIT_NOFILE 6
#define LSF_RLIMIT_OPEN_MAX 7
#define LSF_RLIMIT_VMEM 8
#define LSF_RLIMIT_SWAP LSF_RLIMIT_VMEM
#define LSF_RLIMIT_RUN 9
#define LSF_RLIMIT_PROCESS 10
#define LSF_RLIM_NLIMITS 11

#define EXACT 0x01
#define OK_ONLY 0x02
#define NORMALIZE 0x04
#define LOCALITY 0x08
#define IGNORE_RES 0x10
#define LOCAL_ONLY 0x20
#define DFT_FROMTYPE 0x40
#define ALL_CLUSTERS 0x80
#define EFFECTIVE 0x100
#define RECV_FROM_CLUSTERS 0x200
#define NEED_MY_CLUSTER_NAME 0x400

#define SEND_TO_CLUSTERS 0x400

// lim control options
#define LIM_CMD_REBOOT 1
#define LIM_CMD_SHUTDOWN 2

#define INTEGER_BITS 32
#define GET_INTNUM(i) ((i) / INTEGER_BITS + 1)

#define LIM_UNAVAIL 0x00010000
#define LIM_LOCKEDU 0x00020000
#define LIM_LOCKEDW 0x00040000
#define LIM_BUSY 0x00080000
#define LIM_RESDOWN 0x00100000
#define LIM_LOCKEDM 0x00200000

#define LIM_OK_MASK 0x00bf0000

#define LIM_SBDDOWN 0x00400000

#define LS_ISUNAVAIL(status) (((status[0]) & LIM_UNAVAIL) != 0)

#define LS_ISBUSYON(status, index)                                             \
    (((status[1 + (index) / INTEGER_BITS]) & (1 << (index) % INTEGER_BITS)) != \
     0)

#define LS_ISBUSY(status) (((status[0]) & LIM_BUSY) != 0)

#define LS_ISLOCKEDU(status) (((status[0]) & LIM_LOCKEDU) != 0)

#define LS_ISLOCKEDW(status) (((status[0]) & LIM_LOCKEDW) != 0)

#define LS_ISLOCKEDM(status) (((status[0]) & LIM_LOCKEDM) != 0)

#define LS_ISLOCKED(status)                                                    \
    (((status[0]) & (LIM_LOCKEDU | LIM_LOCKEDW | LIM_LOCKEDM)) != 0)

#define LS_ISRESDOWN(status) (((status[0]) & LIM_RESDOWN) != 0)

#define LS_ISSBDDOWN(status) (((status[0]) & LIM_SBDDOWN) != 0)

#define LS_ISOK(status) ((status[0] & LIM_OK_MASK) == 0)

#define LS_ISOKNRES(status) (((status[0]) & ~(LIM_RESDOWN | LIM_SBDDOWN)) == 0)

struct placeInfo {
    char hostName[LL_HOSTNAME_MAX];
    int numtask;
};

struct hostLoad {
    char hostName[MAXHOSTNAMELEN];
    int *status;
    float *li;
};

enum valueType { LS_BOOLEAN, LS_NUMERIC, LS_STRING, LS_EXTERNAL };

#define BOOLEAN LS_BOOLEAN
#define NUMERIC LS_NUMERIC
#define STRING LS_STRING
#define EXTERNAL LS_EXTERNAL

enum orderType { INCR, DECR, NA };

#define RESF_BUILTIN 0x01
#define RESF_DYNAMIC 0x02
#define RESF_GLOBAL 0x04
#define RESF_SHARED 0x08
#define RESF_EXTERNAL 0x10
#define RESF_RELEASE 0x20
#define RESF_DEFINED_IN_RESOURCEMAP 0x40

struct resItem {
    char name[MAXLSFNAMELEN];
    char des[LL_RES_DESC_MAX];
    enum valueType valueType;
    enum orderType orderType;
    int flags;
    int interval;
};

struct lsInfo {
    int nRes;
    struct resItem *resTable;
    int nTypes;
    char hostTypes[LL_HOSTTYPE_MAX][MAXLSFNAMELEN];
    int nModels;
    char hostModels[LL_HOSTMODEL_MAX][MAXLSFNAMELEN];
    char hostArchs[LL_HOSTMODEL_MAX][MAXLSFNAMELEN];
    int modelRefs[LL_HOSTMODEL_MAX];
    float cpuFactor[LL_HOSTMODEL_MAX];
    int numIndx;
    int numUsrIndx;
};

#define CLUST_STAT_OK 0x01
#define CLUST_STAT_UNAVAIL 0x02

struct clusterInfo {
    char clusterName[MAXLSFNAMELEN];
    int status;
    char masterName[MAXHOSTNAMELEN];
    char managerName[MAXLSFNAMELEN];
    int managerId;
    int numServers;
    int numClients;
    int nRes;
    char **resources;
    int nTypes;
    char **hostTypes;
    int nModels;
    char **hostModels;
    int nAdmins;
    int *adminIds;
    char **admins;
};

struct hostInfo {
    char hostName[MAXHOSTNAMELEN];
    char *hostType;
    char *hostModel;
    float cpuFactor;
    int maxCpus;
    int maxMem;
    int maxSwap;
    int maxTmp;
    int nDisks;
    int nRes;
    char **resources;
    char *windows;
    int numIndx;
    float *busyThreshold;
    char isServer;
    int rexPriority;
};

struct config_param {
    char *paramName;
    char *paramValue;
};

struct lsfRusage {
    double ru_utime;
    double ru_stime;
    double ru_maxrss;
    double ru_ixrss;
    double ru_ismrss;
    double ru_idrss;
    double ru_isrss;
    double ru_minflt;
    double ru_majflt;
    double ru_nswap;
    double ru_inblock;
    double ru_oublock;
    double ru_ioch;
    double ru_msgsnd;
    double ru_msgrcv;
    double ru_nsignals;
    double ru_nvcsw;
    double ru_nivcsw;
    double ru_exutime;
};

struct lsfAcctRec {
    int pid;
    char *username;
    int exitStatus;
    time_t dispTime;
    time_t termTime;
    char *fromHost;
    char *execHost;
    char *cwd;
    char *cmdln;
    struct lsfRusage lsfRu;
};

struct confNode {
    struct confNode *leftPtr;
    struct confNode *rightPtr;
    struct confNode *fwPtr;
    char *cond;
    int beginLineNum;
    int numLines;
    char **lines;
    char tag;
};

struct pStack {
    int top;
    int size;
    struct confNode **nodes;
};

struct confHandle {
    struct confNode *rootNode;
    char *fname;
    struct confNode *curNode;
    int lineCount;
    struct pStack *ptrStack;
};

struct lsConf {
    struct confHandle *confhandle;
    int numConds;
    char **conds;
    int *values;
};

struct sharedConf {
    struct lsInfo *lsinfo;
    char *clusterName;
    char *servers;
};

typedef struct lsSharedResourceInstance {
    char *value;
    int nHosts;
    char **hostList;

} LS_SHARED_RESOURCE_INST_T;

typedef struct lsSharedResourceInfo {
    char *resourceName;
    int nInstances;
    LS_SHARED_RESOURCE_INST_T *instances;
} LS_SHARED_RESOURCE_INFO_T;

struct clusterConf {
    struct clusterInfo *clinfo;
    int numHosts;
    struct hostInfo *hosts;
    int numShareRes;
    LS_SHARED_RESOURCE_INFO_T *shareRes;
};

struct pidInfo {
    int pid;
    int ppid;
    int pgid;
    int jobid;
};

struct jRusage {
    int mem;
    int swap;
    int utime;
    int stime;
    int npids;
    struct pidInfo *pidInfo;

    int npgids;
    int *pgid;
};

enum lse_errno {
    LSE_NO_ERR = 0,
    LSE_BAD_XDR,
    LSE_MSG_SYS,
    LSE_BAD_ARGS,
    LSE_MASTR_UNKNW,
    LSE_LIM_DOWN,
    LSE_PROTOC_LIM,
    LSE_SOCK_SYS,
    LSE_ACCEPT_SYS,
    LSE_NO_HOST,
    LSE_NO_ELHOST,
    LSE_TIME_OUT,
    LSE_NIOS_DOWN,
    LSE_LIM_DENIED,
    LSE_LIM_IGNORE,
    LSE_LIM_BADHOST,
    LSE_LIM_ALOCKED,
    LSE_LIM_NLOCKED,
    LSE_LIM_BADMOD,
    LSE_SIG_SYS,
    LSE_BAD_EXP,
    LSE_NORCHILD,
    LSE_MALLOC,
    LSE_LSFCONF,
    LSE_BAD_ENV,
    LSE_LIM_NREG,
    LSE_RES_NREG,
    LSE_RES_NOMORECONN,
    LSE_BADUSER,
    LSE_BAD_OPCODE,
    LSE_PROTOC_RES,
    LSE_NOMORE_SOCK,
    LSE_LOSTCON,
    LSE_BAD_HOST,
    LSE_WAIT_SYS,
    LSE_SETPARAM,
    LSE_BAD_CLUSTER,
    LSE_EXECV_SYS,
    LSE_BAD_SERVID,
    LSE_NLSF_HOST,
    LSE_UNKWN_RESNAME,
    LSE_UNKWN_RESVALUE,
    LSE_TASKEXIST,
    LSE_LIMIT_SYS,
    LSE_BAD_NAMELIST,
    LSE_LIM_NOMEM,
    LSE_CONF_SYNTAX,
    LSE_FILE_SYS,
    LSE_CONN_SYS,
    LSE_SELECT_SYS,
    LSE_EOF,
    LSE_ACCT_FORMAT,
    LSE_BAD_TIME,
    LSE_FORK,
    LSE_PIPE,
    LSE_ESUB,
    LSE_EAUTH,
    LSE_NO_FILE,
    LSE_NO_CHAN,
    LSE_BAD_CHAN,
    LSE_INTERNAL,
    LSE_PROTOCOL,
    LSE_RES_RUSAGE,
    LSE_NO_RESOURCE,
    LSE_BAD_RESOURCE,
    LSE_RES_PARENT,
    LSE_NO_MEM,
    LSE_FILE_CLOSE,
    LSE_LIMCONF_NOTREADY,
    LSE_MASTER_LIM_DOWN,
    LSE_POLL_SYS,
    LSE_NERR /* Sentinel - must be last */
};
extern const char *ls_errmsg[];

#define LSE_SYSCALL(s)                                                         \
    (((s) == LSE_SELECT_SYS) || ((s) == LSE_CONN_SYS) ||                       \
     ((s) == LSE_FILE_SYS) || ((s) == LSE_MSG_SYS) || ((s) == LSE_SOCK_SYS) || \
     ((s) == LSE_ACCEPT_SYS) || ((s) == LSE_SIG_SYS) ||                        \
     ((s) == LSE_WAIT_SYS) || ((s) == LSE_EXECV_SYS) ||                        \
     ((s) == LSE_LIMIT_SYS) || ((s) == LSE_PIPE) || ((s) == LSE_ESUB))

#define TIMEIT(level, func, name)                                              \
    {                                                                          \
        if (timinglevel > level) {                                             \
            struct timeval before, after;                                      \
            struct timezone tz;                                                \
            gettimeofday(&before, &tz);                                        \
            func;                                                              \
            gettimeofday(&after, &tz);                                         \
            ls_syslog(LOG_INFO, "L%d %s %d ms", level, name,                   \
                      (int) ((after.tv_sec - before.tv_sec) * 1000 +           \
                             (after.tv_usec - before.tv_usec) / 1000));        \
        } else                                                                 \
            func;                                                              \
    }

#define TIMEVAL(level, func, val)                                              \
    {                                                                          \
        if (timinglevel > level) {                                             \
            struct timeval before, after;                                      \
            struct timezone tz;                                                \
            gettimeofday(&before, &tz);                                        \
            func;                                                              \
            gettimeofday(&after, &tz);                                         \
            val = (int) ((after.tv_sec - before.tv_sec) * 1000 +               \
                         (after.tv_usec - before.tv_usec) / 1000);             \
        } else {                                                               \
            func;                                                              \
            val = 0;                                                           \
        }                                                                      \
    }

#define LC_SCHED 0x00000001
#define LC_EXEC 0x00000002
#define LC_TRACE 0x00000004
#define LC_COMM 0x00000008
#define LC_XDR 0x00000010
#define LC_CHKPNT 0x00000020
#define LC_FILE 0x00000080
#define LC_AUTH 0x00000200
/* Deprecated: LC_HANG was useless (can’t log when hung).
 * Keep slot to avoid ABI churn.
 */
#define LC_HANG 0x00000400
#define LC_SIGNAL 0x00001000
#define LC_PIM 0x00004000
#define LC_SYS 0x00008000
#define LC_JLIMIT 0x00010000
#define LC_PEND 0x00080000
#define LC_LOADINDX 0x00200000
#define LC_JGRP 0x00400000
#define LC_JARRAY 0x00800000
#define LC_MPI 0x01000000
#define LC_ELIM 0x02000000
#define LC_M_LOG 0x04000000
#define LC_PERFM 0x08000000

#define LOG_DEBUG1 LOG_DEBUG + 1
#define LOG_DEBUG2 LOG_DEBUG + 2
#define LOG_DEBUG3 LOG_DEBUG + 3

#define LSDEVNULL "/dev/null"

// lserrno is per thread
extern __thread int lserrno;

extern int ls_nerr;
extern int logclass;
extern int timinglevel;

extern int lsf_lim_version;
extern int lsf_res_version;

// Lavalite simple LIM API to get then name and status of the cluster
// and the host, load and resource info
extern char **ls_placereq(char *, int *, int, char *);
extern char **ls_placeofhosts(char *, int *, int, char *, char **, int);
extern struct hostLoad *ls_load(char *, int *, int, char *);
extern struct hostLoad *ls_loadofhosts(char *, int *, int, char *, char **,
                                       int);
extern struct hostLoad *ls_loadinfo(char *, int *, int, char *, char **, int,
                                    char ***);
extern int ls_loadadj(char *, struct placeInfo *, int);
extern char *ls_getclustername(void);
extern struct clusterInfo *ls_clusterinfo(char *, int *, char **, int, int);
extern struct lsSharedResourceInfo *ls_sharedresourceinfo(char **, int *,
                                                          char *, int);
extern struct hostInfo *ls_gethostinfo(char *, int *, char **, int, int);
extern struct lsInfo *ls_info(void);
extern char *ls_getmastername(void);
extern char *ls_getmyhostname(void);

extern char *ls_gethosttype(const char *);
extern float ls_getmodelfactor(const char *);
extern float ls_gethostfactor(const char *);
extern char *ls_gethostmodel(const char *);
extern int ls_lockhost(time_t);
extern int ls_unlockhost(void);
extern int ls_limcontrol(const char *, int);

extern const char *ls_sysmsg(void);
extern void ls_perror(const char *);

extern struct lsConf *ls_getconf(char *);
extern void ls_freeconf(struct lsConf *);
extern struct sharedConf *ls_readshared(char *);
extern struct clusterConf *ls_readcluster(char *, struct lsInfo *);
extern struct clusterConf *ls_readcluster_ex(char *, struct lsInfo *, int);

extern int ls_initdebug(const char *);

// LavaLite logging
int ls_openlog(const char *,   // identity
               const char *,   // logdir
               int,            // to_stderr
               int,            // to_syslog
               const char *);  // log mask

void ls_syslog(int, const char *, ...)
#if defined(__GNUC__) && defined(CHECK_ARGS)
    __attribute__((format(printf, 2, 3)))
#endif
    ;
void ls_set_time_level(const char *);
void ls_setlogtag(const char *);
void ls_closelog(void);
void ls_set_log_to_stderr(int);

extern int ls_servavail(int, int);
extern int ls_setpriority(int);

extern void cleanLsfRusage(struct lsfRusage *);
extern void cleanRusage(struct rusage *);

extern struct lsfAcctRec *ls_getacctrec(FILE *, int *);
extern int ls_putacctrec(FILE *, struct lsfAcctRec *);
extern int getBEtime(char *, char, time_t *);

// LavaLite
const char *ctime2(time_t *);
const char *ctime3(const time_t *);
struct passwd *getpwuid2(uid_t);
struct passwd *getpwnam2(const char *);
void open_log(const char *, const char *, bool_t);

// Bug rethink this
struct extResInfo {
    char *name;
    char *type;
    char *interval;
    char *increasing;
    char *des;
};

// LavaLite

/* Host information */
struct host_info {
    char hostname[MAXHOSTNAMELEN];
    char arch[32];   /* x86_64, arm64, ppc64le, etc */
    char vendor[32]; /* intel, amd, apple, ibm, etc */
    int ncores;
    int mem_mb;
    int swap_mb;
    int is_server; /* can run jobs? */
    int nres;
    struct res_item *resources;
};

enum value_type { RES_TYPE_NUMERIC, RES_TYPE_STRING, RES_TYPE_BOOLEAN };

enum order_type {
    ORDER_INCR, /* higher is better (free memory, disk space) */
    ORDER_DECR, /* lower is better (load, temperature) */
    ORDER_NA    /* no ordering (hostname, arch) */
};

struct res_item {
    char name[MAXLSFNAMELEN];
    char value[256];
    enum value_type type;
    enum order_type order;
};

/* Shared resource instance */
struct shared_res_inst {
    char hostname[MAXHOSTNAMELEN]; /* where this instance lives */
    int total;
    int available;
    int reserved;
};

/* Shared resource definition */
struct shared_res {
    char name[MAXLSFNAMELEN];    /* e.g. "verilog_lic" */
    char server[MAXHOSTNAMELEN]; /* license server (optional) */
    int total_instances;
    int available_instances;
    int nlocations;
    struct shared_res_inst *locations;
};

/* Cluster information */
struct cluster_info {
    char name[MAXLSFNAMELEN];
    char master[MAXHOSTNAMELEN];
    int nhosts;
    struct host_info *hosts;
    int nres_defined;
    struct res_item *res_defs;
    int nshared_res;
    struct shared_res *shared_res;
};

/* Job resource usage */
struct job_rusage {
    int mem;
    int swap;
    int utime;
    int stime;
    int npids;
    struct pid_info *pidinfo;
    int npgids;
    int *pgid;
};

struct pid_info {
    int pid;
    int ppid;
    int pgid;
    int jobid;
};
