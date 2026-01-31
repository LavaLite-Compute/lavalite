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

#include "lsf/lib/lib.hdr.h"

enum ofWhat { OF_ANY, OF_HOSTS, OF_TYPE };

typedef enum ofWhat ofWhat_t;

struct decisionReq {
    ofWhat_t ofWhat;
    int options;
    char hostType[MAXLSFNAMELEN];
    int numHosts;
    char resReq[MAXLINELEN];
    int numPrefs;
    char **preferredHosts;
};

struct placeReply {
    int numHosts;
    struct placeInfo *placeInfo;
};

struct jobXfer {
    int numHosts;
    char resReq[LL_BUFSIZ_512];
    struct placeInfo *placeInfo;
};

/* LIM TCP protocol operation codes (opcode)
 */
enum limReqCode {
    LIM_PLACEMENT = 1,
    LIM_LOAD_REQ,
    LIM_LOAD_ADJ,
    LIM_GET_CLUSNAME,
    LIM_GET_MASTINFO,
    LIM_GET_HOSTINFO,
    LIM_GET_CPUF,
    LIM_GET_INFO,
    LIM_GET_CLUSINFO,
    LIM_PING,
    LIM_CHK_RESREQ,
    LIM_DEBUGREQ,
    LIM_GET_RESOUINFO,
    LIM_REBOOT,
    LIM_LOCK_HOST,
    LIM_SERV_AVAIL,
    LIM_SHUTDOWN,
    LIM_LOAD_UPD,
    LIM_JOB_XFER,
    LIM_MASTER_ANN,
    LIM_CONF_INFO,
    LIM_CLUST_INFO,
    LIM_HINFO_REQ,
    LIM_HINFO_REPLY,
    LIM_LINFO_REQ,
    LIM_LINFO_REPLY,
    LIM_MASTER_REGISTER, // LavaCore
    LIM_LOAD_UPD2,
    LIM_PROTO_CNT // sentinel
};

// LIM reply codes
enum limReplyCode {
    LIME_NO_ERR = 1,
    LIME_WRONG_MASTER,
    LIME_BAD_RESREQ,
    LIME_NO_OKHOST,
    LIME_NO_ELHOST,
    LIME_BAD_DATA,
    LIME_BAD_REQ_CODE,
    LIME_MASTER_UNKNW,
    LIME_DENIED,
    LIME_IGNORED,
    LIME_UNKWN_HOST,
    LIME_UNKWN_MODEL,
    LIME_LOCKED_AL,
    LIME_NOT_LOCKED,
    LIME_BAD_SERVID,
    LIME_NAUTH_HOST,
    LIME_UNKWN_RNAME,
    LIME_UNKWN_RVAL,
    LIME_NO_MEM,
    LIME_BAD_FILTER,
    LIME_BAD_RESOURCE,
    LIME_NO_RESOURCE,
    LIME_ERR_CNT // sentinel
};

struct loadReply {
    int nEntry;
    int nIndex;
    char **indicies;
    struct hostLoad *loadMatrix;
#define LOAD_REPLY_SHARED_RESOURCE 0x1
    int flags;
};

struct shortHInfo {
    char hostName[MAXHOSTNAMELEN];
    int hTypeIndx;
    int hModelIndx;
    int maxCpus;
    int maxMem;
    int maxSwap;
    int maxTmp;
    int nDisks;
    int resClass;
    char *windows;
    float *busyThreshold;
    int flags;
#define HINFO_SERVER 0x01
#define HINFO_SHARED_RESOURCE 0x02
    int rexPriority;
    int nRInt;
    int *resBitMaps;
};

struct shortLsInfo {
    int nRes;
    char **resName;
    int nTypes;
    char *hostTypes[LL_HOSTTYPE_MAX];
    int nModels;
    char *hostModels[LL_HOSTMODEL_MAX];
    float cpuFactors[LL_HOSTMODEL_MAX];
    int *stringResBitMaps;
    int *numericResBitMaps;
};

struct hostInfoReply {
    int nHost;
    int nIndex;
    struct shortLsInfo *shortLsInfo;
    struct shortHInfo *hostMatrix;
};

struct clusterInfoReq {
    char *resReq;
    int listsize;
    char **clusters;
    int options;
};

struct shortCInfo {
    char clName[LL_NAME_MAX];
    char masterName[MAXHOSTNAMELEN];
    char managerName[LL_NAME_MAX];
    int managerId;
    int status;
    int resClass;
    int typeClass;
    int modelClass;
    int numIndx;
    int numUsrIndx;
    int usrIndxClass;
    int numServers;
    int numClients;
    int nAdmins;
    int *adminIds;
    char **admins;
    int nRes;
    int *resBitMaps;
    int nTypes;
    int *hostTypeBitMaps;
    int nModels;
    int *hostModelBitMaps;
};

struct cInfo {
    char clName[LL_NAME_MAX];
    char masterName[MAXHOSTNAMELEN];
    char managerName[LL_NAME_MAX];
    int managerId;
    int status;
    int resClass;
    int typeClass;
    int modelClass;
    int numIndx;
    int numUsrIndx;
    int usrIndxClass;
    int numServers;
    int numClients;
    int nAdmins;
    int *adminIds;
    char **admins;
    int nRes;
    int *resBitMaps;
    char **loadIndxNames;
    struct shortLsInfo shortInfo;
    int nTypes;
    int *hostTypeBitMaps;
    int nModels;
    int *hostModelBitMaps;
};

struct clusterInfoReply {
    int nClus;
    struct shortLsInfo *shortLsInfo;
    struct shortCInfo *clusterMatrix;
};

struct masterInfo {
    char hostName[MAXHOSTNAMELEN]; // short hostname, NOFQDN per policy
    struct sockaddr_in addr;       // control-plane IPv4 endpoint *
    uint16_t portno;               // host byte order
};

struct clusterList {
    int nClusters;
    char **clNames;
};

#define LIM_UNLOCK_USER 0
#define LIM_LOCK_USER 1
#define LIM_UNLOCK_MASTER 2
#define LIM_LOCK_MASTER 3

#define LIM_LOCK_STAT_USER 0x1
#define LIM_LOCK_STAT_MASTER 0x2

#define LOCK_BY_USER(stat) (((stat) & LIM_LOCK_STAT_USER) != 0)
#define LOCK_BY_MASTER(stat) (((stat) & LIM_LOCK_STAT_MASTER) != 0)

// Bug in lavalite we should call the master and tell it what host to
// lock
struct limLock {
    int uid;
    int on;
    time_t time;
    char lsfUserName[LL_NAME_MAX];
};

// Lavalite

// In limout.h - clean wire protocol structs

// For ls_gethostinfo()
struct wire_host_info {
    char *host_name;
    char *host_type;
    char *host_model;
    float cpu_factor;
    int max_cpus;
    int max_mem;
    int max_swap;
    int max_tmp;
    int num_disks;
    int is_server;
    int status;
};

struct wire_host_info_reply {
    int num_hosts;
    struct wire_host_info *hosts;
};

// For ls_loadinfo()
struct wire_load_info {
    char *host_name;
    float load_indices[NBUILTINDEX]; // r15s r1m r15m ut pg io ls it tmp swp mem
    int status[NBUILTINDEX];         // status per index
};

struct wire_load_info_reply {
    int num_hosts;
    struct wire_load_info *hosts;
};

struct wire_res_item {
    char name[MAXLSFNAMELEN];
    char des[LL_RES_DESC_MAX];
    enum valueType value_type;
    enum orderType order_type;
    int flags;
    int interval;
};

/* host type	meaning
 *-----------------------
 * LINUX64	64-bit Linux
 * WINDOWS	Windows nodes
 * AARCH64	ARM servers
 */
struct wire_host_type {
    char name[MAXLSFNAMELEN];
};

/*
 *  host model	arch	cpuFactor	meaning
 * ------------------------------------------------
 * intel-xeon-e5	x86_64	1.0	baseline
 * amd-epyc-rome	x86_64	1.4	faster
 * graviton2	arm64	0.9	slower per-core
 * nvidia-grace	armv9	1.8	fast ARM
 */
struct wire_host_model {
    char model[MAXLSFNAMELEN];
    char arch[MAXLSFNAMELEN];
    int ref;
    float cpu_factor;
};

struct wire_lsinfo_reply {
    int n_res;
    struct wire_res_item *res_table; /* [n_res] */
    int n_types;
    struct wire_host_type *host_types; /* [n_types] */
    int n_models;
    struct wire_host_model *host_models; /* [n_models] */
    int num_indx;
    int num_usr_indx;
};

// ls_clusterinfo()
struct wire_cluster_info {
    char cluster_name[MAXLSFNAMELEN];
    int status;
    char master_name[MAXHOSTNAMELEN];
    char manager_name[MAXLSFNAMELEN];
    int manager_id;
    int num_servers;
    int num_clients;
};

struct wire_cluster_info_reply {
    struct wire_cluster_info cluster;
};
