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

// Pad the n onject possible strlen(str)
#define XDR_PADLEN(n) (((4 - ((n) % 4)) % 4))
#define XDR_STRLEN(n) (4 + (n) + XDR_PADLEN(n))

// The XDR stream, the data structure, the context and the
// encode/decode function
bool xdr_array_element(XDR *, void *, void *, bool (*)());

// This is the type of the encode/decode function
typedef bool (*xdr_func_t)(XDR *, void *, void *);

// Generic XDR functions used throught the system libraries
void xdr_free_payload(bool (*)(), void *, struct protocol_header *);
bool xdr_time_t(XDR *, time_t *);
bool xdr_lsfRusage(XDR *, struct lsfRusage *, void *);
bool xdr_lvector(XDR *, float *, uint32_t);
bool xdr_array_string(XDR *, char **, int, int);
bool xdr_var_string(XDR *, char **);
bool xdr_stringLen(XDR *, struct stringLen *, struct protocol_header *);
bool xdr_lenData(XDR *, struct lenData *);
bool xdr_lsfLimit(XDR *, struct lsfLimit *, struct protocol_header *);
bool xdr_portno(XDR *, uint16_t *);
bool xdr_sockaddr_in(XDR *, struct sockaddr_in *);

// Protocol related xdr
extern bool xdr_pack_hdr(XDR *, struct protocol_header *);
extern bool xdr_encode_msg(XDR *, char *, struct protocol_header *, bool (*)(),
                            int, struct lsfAuth *);
extern bool xdr_arrayElement(XDR *, char *, struct protocol_header *,
                               bool (*)(), ...);
extern bool xdr_stringLen(XDR *, struct stringLen *, struct protocol_header *);
extern bool xdr_lsfAuth(XDR *, struct lsfAuth *, struct protocol_header *);
extern int xdr_lsfAuthSize(struct lsfAuth *);

// lim XDR functions
bool xdr_decisionReq(XDR *, struct decisionReq *, struct protocol_header *);
bool xdr_placeReply(XDR *, struct placeReply *, struct protocol_header *);
bool xdr_loadReply(XDR *, struct loadReply *, struct protocol_header *);
bool xdr_jobXfer(XDR *, struct jobXfer *, struct protocol_header *);
bool xdr_hostInfo(XDR *, struct shortHInfo *, struct protocol_header *);
bool xdr_limLock(XDR *, struct limLock *, struct protocol_header *);
bool xdr_lsInfo(XDR *, struct lsInfo *, struct protocol_header *);
bool xdr_hostInfoReply(XDR *, struct hostInfoReply *, struct protocol_header *);
bool xdr_masterInfo(XDR *, struct masterInfo *, struct protocol_header *);
bool xdr_clusterInfoReq(XDR *, struct clusterInfoReq *,
                          struct protocol_header *);
bool xdr_clusterInfoReply(XDR *, struct clusterInfoReply *,
                            struct protocol_header *);
bool xdr_shortHInfo(XDR *, struct shortHInfo *, void *);
bool xdr_shortCInfo(XDR *, struct shortCInfo *, void *);
bool xdr_cInfo(XDR *, struct cInfo *, struct protocol_header *);
bool xdr_resourceInfoReq(XDR *, struct resourceInfoReq *,
                           struct protocol_header *);
bool xdr_resourceInfoReply(XDR *, struct resourceInfoReply *, void *);
bool xdr_jRusage(XDR *, struct jRusage *, void *);

// For ls_gethostinfo()
struct wire_host {
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

struct wire_host_reply {
    int num_hosts;
    struct wire_host *hosts;
};

// wire it the ll naming for bytes on the wire in network format
// host info
bool xdr_wire_host(XDR *, struct wire_host *,
                     struct protocol_header *);
bool xdr_wire_host_reply(XDR *, struct wire_host_reply *,
                           struct protocol_header *);
// host load info
// For ls_loadinfo()
struct wire_load {
    char *host_name;
    float load_indices[NBUILTINDEX]; // r15s r1m r15m ut pg io ls it tmp swp mem
    int status[NBUILTINDEX];         // status per index
};

struct wire_load_reply {
    int num_hosts;
    struct wire_load *hosts;
};

bool xdr_wire_load(XDR *, struct wire_load *);
bool xdr_wire_load_reply(XDR *, struct wire_load_reply *);

// ls_info()
struct wire_lsinfo {
    int n_res;
    struct wire_res  *res_table; /* [n_res] */
    int n_types;
    struct wire_host_type *host_types; /* [n_types] */
    int n_models;
    struct wire_host_model *host_models; /* [n_models] */
    int num_indx;
    int num_usr_indx;
};

struct wire_res {
    char name[MAXLSFNAMELEN];
    char des[LL_RES_DESC_MAX];
    enum valueType value_type;
    enum orderType order_type;
    int flags;
    int interval;
};

struct wire_host_type {
    char name[MAXLSFNAMELEN];
};

struct wire_host_model {
    char model[MAXLSFNAMELEN];
    char arch[MAXLSFNAMELEN];
    int ref;
    float cpu_factor;
};

bool xdr_wire_lsinfo(XDR *, struct wire_lsinfo *);
bool xdr_wire_res(XDR *, struct wire_res *);
bool xdr_wire_host_type(XDR *, struct wire_host_type *);
bool xdr_wire_host_model(XDR *, struct wire_host_model *);


// ls_clusterinfo()
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
bool xdr_wire_cluster_info(XDR *, struct wire_cluster_info *);

// strings
bool xdr_string_raw(XDR *, char **, uint32_t);
