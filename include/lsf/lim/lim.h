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

#include "config.h"

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <math.h>
#include <float.h>
#include <signal.h>
#include <pwd.h>
#include <utmp.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/signal.h>
#include <sys/file.h>
#include <sys/utsname.h>
#include <rpc/types.h>
#include <rpc/xdr.h>
#include <sys/sysmacros.h>
#include <sys/vfs.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <dirent.h>

#include "lsf.h"
#include "lsf/lib/lib.h"
#include "lsf/lib/ll.sys.h"
#include "lsf/lib/intlibout.h"
#include "lsf/lib/tcl_stub.h"
#include "lsf/lib/lib.channel.h"
#include "lsf/lib/lib.conf.h"
#include "lsf/lim/limout.h"
#include "lsf/lib/lib.xdr.h"

#define EXCHINTVL 15
#define SAMPLINTVL 5

#define HOSTINACTIVITYLIMIT 5
#define MASTERINACTIVITYLIMIT 2
#define RESINACTIVITYLIMIT 9
#define RETRYLIMIT 2
#define INTERCLUSCACHEINTVL 60

#define SBD_ACTIVE_TIME 60 * 5
#define KEEPTIME 2

#define MAXCANDHOSTS 10

#define WARNING_ERR EXIT_WARNING_ERROR

/* BUG: legacy daemon path logic removed — stubbed for now */
#define getDaemonPath_(name, dir) ((char *) NULL)
#define saveDaemonDir_(dir) ((void) 0)

/* How we name things:
 * <subsystem>_<action>()   for exported subsystem functions
 * <action>_<object>() for local handlers/processing functions
 */

/* Evaluate MAX or MIN ifdef it as they might be defined
 * in sys/params.h
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

#define DEFAULT_AFTER_HOUR "19:00-7:00 5:19:00-1:7:00"

struct timewindow {
    char *winName;
    windows_t *week[8];
    time_t wind_edge;
};

#define LIM_STARTUP 0
#define LIM_CHECK 1
#define LIM_RECONFIG 2

struct statInfo {
    short hostNo;
    int maxCpus;
    int maxMem;
    int maxSwap;
    int maxTmp;
    int nDisks;
    u_short portno;
    char hostType[MAXLSFNAMELEN];
    char hostArch[MAXLSFNAMELEN];
};

struct hostNode {
    char *hostName;
    short hModelNo;
    short hTypeNo;
    short hostNo;
    struct ll_host *v4_epoint; // IPv4 node endpoint (control-plane addr)
    struct statInfo statInfo;
    char infoValid;
    unsigned char protoVersion;
    short availHigh;
    short availLow;
    short use;
    int resClass;
    int DResClass;
    u_short nRes;
    char *windows;
    windows_t *week[8];
    time_t wind_edge;
    time_t lastJackTime;
    short hostInactivityCount;
    int *status;
    float *busyThreshold;
    float *loadIndex;
    float *uloadIndex;
    char conStatus;
    u_int lastSeqNo;
    int rexPriority;
    int infoMask;
    int loadMask;
    int *resBitMaps;
    int *DResBitMaps;
    int numInstances;
    struct resourceInstance **instances;
    int callElim;
    int maxResIndex;
    int *resBitArray;
    struct hostNode *nextPtr;
    time_t expireTime;
};

#define CLUST_ACTIVE 0x00010000
#define CLUST_MASTKNWN 0x00020000
#define CLUST_CONNECT 0x00040000
#define CLUST_INFO_AVAIL 0x00080000
#define CLUST_HINFO_AVAIL 0x00100000
#define CLUST_ELIGIBLE 0x00200000
#define CLUST_ALL_ELIGIBLE 0x00400000

struct clusterNode {
    short clusterNo;
    char *clName;
    int status;
    int currentAddr;
    char *masterName;
    struct sockaddr_in masterAddr;
    uint16_t masterPort;
    int resClass;
    int typeClass;
    int modelClass;
    char masterKnown;
    int masterInactivityCount;
    struct hostNode *masterPtr;
    struct hostNode *prevMasterPtr;
    u_short checkSum;
    int numHosts;
    int numClients;
    int managerId;
    char *managerName;
    struct hostNode *hostList;
    struct hostNode *clientList;
    struct clusterNode *nextPtr;
    char *eLimArgs;
    char **eLimArgv;
    int ch_id;
    int numIndx;
    int numUsrIndx;
    int usrIndxClass;
    char **loadIndxNames;
    int nAdmins;
    int *adminIds;
    char **admins;
    int nRes;
    int *resBitMaps;
    int *hostTypeBitMaps;
    int *hostModelBitMaps;
    int numSharedRes;
    char **sharedResource;
    struct shortLsInfo *shortInfo;
};

struct client_node {
    enum limReqCode limReqCode;
    int ch_id;
    struct hostNode *from_host;
};

struct liStruct {
    char *name;
    char increasing;
    float delta[2];
    float extraload[2];
    float valuesent;
    float exchthreshold;
    float sigdiff;
    float satvalue;
    float value;
};

#define SEND_NO_INFO 0x00
#define SEND_CONF_INFO 0x01
#define SEND_LOAD_INFO 0x02
#define SEND_MASTER_ANN 0x04
#define SEND_ELIM_REQ 0x08
#define SEND_MASTER_QUERY 0x10
#define SLIM_XDR_DATA 0x20
#define SEND_LIM_LOCKEDM 0x100

struct loadVectorStruct {
    int hostNo;
    int *status;
    u_int seqNo;
    int checkSum;
    int flags;
    int numIndx;
    int numUsrIndx;
    float *li;
    int numResPairs;
    struct resPair *resPairs;
};

#define DETECTMODELTYPE 0

struct masterReg {
    char clName[LL_BUFSIZ_32];
    char hostName[LL_HOSTNAME_MAX]; // Posix
    int flags;
    u_int seqNo;
    int checkSum;
    uint16_t portno;
    int licFlag;
    int maxResIndex;
    int *resBitArray;
};

struct masterAnnSLIMConf {
    int flags;
    short hostNo;
};

struct resourceInstance {
    char *resName;
    int nHosts;
    struct hostNode **hosts;
    char *orignalValue;
    char *value;
    time_t updateTime;
    struct hostNode *updHost;
};

typedef struct sharedResourceInstance {
    char *resName;
    int nHosts;
    struct hostNode **hosts;
    struct sharedResourceInstance *nextPtr;
} sharedResourceInstance;

extern struct sharedResourceInstance *sharedResourceHead;

#define THRLDOK(inc, a, thrld) (inc ? a <= thrld : a >= thrld)

extern bool_t lim_debug;
extern int lim_CheckMode;
extern int lim_CheckError;
extern int lim_udp_sock;
extern int lim_tcp_sock;
extern ushort lim_udp_port;
extern ushort lim_tcp_port;
extern struct clusterNode *myClusterPtr;
extern struct hostNode *myHostPtr;
extern char myClusterName[];
extern int masterMe;
extern float exchIntvl;
extern float sampleIntvl;
extern short hostInactivityLimit;
extern short masterInactivityLimit;
extern short resInactivityLimit;
extern short retryLimit;
extern short keepTime;
extern int probeTimeout;
extern short resInactivityCount;
extern short satper;
extern int nClusAdmins;
extern int *clusAdminIds;
extern int *clusAdminGids;
extern char **clusAdminNames;
extern struct liStruct *li;
extern int li_len;
extern int defaultRunElim;
extern time_t lastSbdActiveTime;

extern char mustSendLoad;
extern hTab hostModelTbl;

extern char *env_dir;
extern struct hostNode **candidates;
extern u_int loadVecSeqNo;
extern u_int masterAnnSeqNo;
extern struct hostNode *fromHostPtr;
extern struct lsInfo allInfo;
extern struct shortLsInfo shortInfo;
extern int clientHosts[];
extern struct floatClientInfo floatClientPool;
extern int ncpus;
extern pid_t elim_pid;
extern pid_t pimPid;

extern struct limLock limLock;
extern int numHostResources;
extern struct sharedResource **hostResources;

extern u_short lsfSharedCkSum;

extern int numMasterCandidates;
extern int isMasterCandidate;
extern int limConfReady;
extern long max_clients;

extern int readShared(void);
extern int readCluster(int);
extern void reCheckRes(void);
extern int reCheckClass(void);
extern void setMyClusterName(void);
extern int resNameDefined(char *);
extern struct sharedResource *inHostResourcs(char *);
extern struct resourceInstance *isInHostList(struct sharedResource *, char *);
extern void freeHostNodes(struct hostNode *, int);
extern int validResource(const char *);
extern int validLoadIndex(const char *);
extern int validHostType(const char *);
extern int validHostModel(const char *);
extern char *stripIllegalChars(char *);
extern int initTypeModel(struct hostNode *);
extern const char *getHostType();
extern int typeNameToNo(const char *);
extern int archNameToNo(const char *);

extern void reconfigReq(XDR *, struct sockaddr_in *, struct packet_header *);
extern void shutdownReq(XDR *, struct sockaddr_in *, struct packet_header *);
extern void limDebugReq(XDR *, struct sockaddr_in *, struct packet_header *);
extern void lockReq(XDR *, struct sockaddr_in *, struct packet_header *);
extern int limPortOk(const struct sockaddr_in *);
extern void servAvailReq(XDR *, struct hostNode *, struct sockaddr_in *,
                         struct packet_header *);
extern void masterRegister(XDR *, struct sockaddr_in *, struct packet_header *);
extern void rcvConfInfo(XDR *, struct sockaddr_in *, struct packet_header *);
extern void tellMasterWho(XDR *, struct sockaddr_in *, struct packet_header *);
extern void whoIsMaster(struct clusterNode *);
extern void announceMaster(struct clusterNode *, char, char);
extern void sndConfInfo(struct sockaddr_in *);
extern void checkHostWd(void);
extern void announceMasterToHost(struct hostNode *, int);
extern int probeMasterTcp(struct clusterNode *clPtr);
extern void initNewMaster(void);
extern int validateHost(char *, int);
extern int validateHostbyAddr(struct sockaddr_in *, int);
extern void checkAfterHourWindow();

extern void sendLoad(void);
extern void rcvLoad(XDR *, struct sockaddr_in *, struct packet_header *);
extern void copyIndices(float *, int, int, struct hostNode *);
extern float normalizeRq(float rawql, float cpuFactor, int nprocs);
extern struct resPair *getResPairs(struct hostNode *);
extern void satIndex(void);
extern void loadIndex(void);
extern void initReadLoad(int);
extern void initConfInfo(void);
extern void readLoad(void);
extern const char *getHostModel(void);

extern void getLastActiveTime(void);
extern void putLastActiveTime(void);

extern void lim_Exit(const char *);

extern int definedSharedResource(struct hostNode *, struct lsInfo *);

extern void errorBack(struct sockaddr_in *, struct packet_header *,
                      enum limReplyCode, int);
extern void initLiStruct(void);

extern void updExtraLoad(struct hostNode **, char *, int);

extern int getEligibleSites(register struct resVal *, struct decisionReq *,
                            char, char *);
extern int validHosts(char **, int, char *, int);
extern int checkValues(struct resVal *, int);
extern void chkResReq(XDR *, struct sockaddr_in *, struct packet_header *);
extern void getTclHostData(struct tclHostData *, struct hostNode *,
                           struct hostNode *, int);

extern void reconfig(void);
extern void shutdownLim(void);

extern int xdr_loadvector(XDR *, struct loadVectorStruct *,
                          struct packet_header *);
extern int xdr_loadmatrix(XDR *, int, struct loadVectorStruct *,
                          struct packet_header *);
extern int xdr_masterReg(XDR *, struct masterReg *, struct packet_header *);
extern int xdr_statInfo(XDR *, struct statInfo *, struct packet_header *);
extern struct liStruct *li;
extern int li_len;

extern char *getElimRes(void);
extern int saveSBValue(char *, char *);
extern void getusr(void);
extern void smooth(float *, float, float);
extern float idletime(int *);
extern float tmpspace(void);
extern float getswap(void);
extern float getpaging(float);
extern float getIoRate(float);
extern void cpuTime(double *, double *);
extern float queueLength(void);
extern int realMem(float);
extern int numCpus(void);
extern int queueLengthEx(float *, float *, float *);

// Lavalite
extern struct client_node **client_map;
extern int lim_udp_chan;
extern int lim_tcp_chan;
extern int lim_timer_chan;
extern uint16_t lim_udp_port;
extern uint16_t lim_tcp_port;

// Make the hostNode fundamental node representation
struct hostNode *make_host_node(void);

// Lavalite hostNode family functions. The ev_epoint control plane
// end point if set during configuration.
// These functions operate on already constructed ll_host in
// the hostNode structure
struct hostNode *find_node_by_sockaddr_in(const struct sockaddr_in *);
struct hostNode *find_node_by_name(const char *);
struct hostNode *find_node_by_cluster(struct hostNode *, const char *);
struct hostNode *find_node_by_hostNo(int);
int handle_tcp_client(int);

// These 2 are UDP request to the local LIM
void cluster_name_req(XDR *, struct sockaddr_in *, struct packet_header *);
void master_info_req(XDR *, struct sockaddr_in *, struct packet_header *);
// This does not do anything, remove it
void ping_req(XDR *, struct sockaddr_in *, struct packet_header *);
// The rest of API processing is TCP
void clus_info_req(XDR *, struct client_node *, struct packet_header *);
void load_req(XDR *, struct client_node *, struct packet_header *);
void host_info_req(XDR *, struct client_node *, struct packet_header *);
// Avoid the same function name as the data structure
void resource_info_req(XDR *, struct client_node *, struct packet_header *);
void info_req(XDR *, struct client_node *, struct packet_header *);
void shutdown_client(struct client_node *);
void wrong_master(struct client_node *);
void send_header(struct client_node *, struct packet_header *,
                 enum limReplyCode);
void lim_proc_init_read_load(int);

#define SWP_INTVL_CNT 45 / exchIntvl
#define TMP_INTVL_CNT 120 / exchIntvl
#define PAGE_INTVL_CNT 120 / exchIntvl
#define EXP3 0.716531311
#define EXP4 0.77880078
#define EXP6 0.846481725
#define EXP12 0.920044415
#define EXP180 0.994459848

// LavaCore

struct master_beacon {
    char     cluster[LL_BUFSIZ_32];
    char     hostname[MAXHOSTNAMELEN];
    uint32_t hostNo;
    uint32_t seqno;
    uint16_t tcp_port;
};

struct wire_load_update {
    uint32_t hostNo;
    uint32_t seqNo;
    uint32_t status0;
    uint32_t nidx;
    float li[LIM_NIDX];
};

#define MASTER_INVALID_TICKS 3
#define SLAVE_MISSING_LOAD_TICKS 3

void master_beacon_send(struct clusterNode *);
void master_beacon_recv(XDR *, struct sockaddr_in *, struct packet_header *);
bool_t xdr_master_beacon(XDR *, struct master_beacon *);
void read_load(void);
int send_load_update(void);
void rcv_load_update(XDR *, struct sockaddr_in *, struct packet_header *);
bool_t xdr_wire_load_update(XDR *, struct wire_load_update *);
void lim_proc_read_load(void);
