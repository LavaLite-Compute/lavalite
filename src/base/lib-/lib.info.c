/* $Id: lib.info.c,v 1.6 2007/08/15 22:18:50 tmizan Exp $
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
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 *
 */
#include "base/lib/lib.h"
#include "base/lib/lib.channel.h"

struct masterInfo masterInfo;
int masterknown = false;

static struct wire_host_reply *query_lim_hosts(void);
static void query_lim_host_free(struct wire_host_reply *);

static struct wire_load_reply *query_lim_load(void);
static void query_lim_load_free(struct wire_load_reply *);

static struct wire_cluster_info *query_lim_clusterinfo(void);

static struct wire_lsinfo *query_lim_lsinfo(void);
static void wire_lsinfo_free(struct wire_lsinfo *);

static int getname_(enum limReqCode limReqCode, char *name, int namesize)
{
    if (initenv_(NULL, NULL) < 0)
        return -1;

    if (limReqCode == LIM_GET_CLUSNAME) {
        struct stringLen str;
        str.name = name;
        str.len = namesize;
        if (callLim_(LIM_GET_CLUSNAME, NULL, NULL, &str, xdr_stringLen, NULL,
                     _USE_UDP_, NULL) < 0)
            return -1;
        return 0;
    }
    assert(limReqCode == LIM_GET_MASTINFO);
    if (callLim_(LIM_GET_MASTINFO, NULL, NULL, &masterInfo, xdr_masterInfo,
                 NULL, _USE_UDP_, NULL) < 0) {
        return -1;
    }

    // Set the master address
    sock_addr_in[UDP].sin_addr = masterInfo.addr.sin_addr;
    chan_close(lim_chans[UDP]);
    lim_chans[UDP] = -1;
    chan_close(lim_chans[TCP]);
    lim_chans[TCP] = -1;

    // Copy the tcp address
    sock_addr_in[TCP].sin_addr = masterInfo.addr.sin_addr;
    sock_addr_in[TCP].sin_port = masterInfo.portno;
    masterknown = true;
    strncpy(name, masterInfo.hostName, namesize);
    name[namesize - 1] = 0;

    return 0;
}

// Obsolete the non reentering version.
int ls_getclustername_r(char *buf, size_t buflen)
{
    if (buf == NULL || buflen == 0) {
        lserrno = LSE_BAD_ARGS;
        return -1;
    }

    if (getname_(LIM_GET_CLUSNAME, buf, (int)buflen) < 0)
        return -1;

    buf[buflen - 1] = 0;
    return 0;
}

char *ls_getclustername(void)
{
    static char clName[MAXLSFNAMELEN];

    if (clName[0] == '\0')
        if (getname_(LIM_GET_CLUSNAME, clName, MAXLSFNAMELEN) < 0)
            return NULL;

    return clName;
}

int ls_getmastername_r(char *buf, size_t buflen)
{
    if (buf == NULL || buflen == 0) {
        lserrno = LSE_BAD_ARGS;
        return -1;
    }

    if (getname_(LIM_GET_MASTINFO, buf, (int)buflen) < 0)
        return -1;

    buf[buflen - 1] = 0;
    return 0;
}

char *ls_getmastername(void)
{
    static char master[MAXHOSTNAMELEN];

    if (getname_(LIM_GET_MASTINFO, master, MAXHOSTNAMELEN) < 0)
        return NULL;

    return master;
}

struct clusterInfo *ls_clusterinfo(char *resReq, int *numclusters,
                                   char **clusterList, int listsize,
                                   int options)
{
    (void)resReq;
    (void)clusterList;
    (void)listsize;
    (void)options;

    if (numclusters == NULL) {
        lserrno = LSE_BAD_ARGS;
        return NULL;
    }

    *numclusters = 0;
    struct wire_cluster_info *wr = query_lim_clusterinfo();
    if (wr == NULL) {
        return NULL;
    }

    struct clusterInfo *ci = calloc(1, sizeof(*ci));
    if (!ci) {
        lserrno = LSE_MALLOC;
        free(wr);
        return NULL;
    }

    *numclusters = 1;
    snprintf(ci->clusterName, sizeof(ci->clusterName), "%s", wr->cluster_name);
    snprintf(ci->masterName, sizeof(ci->masterName), "%s", wr->master_name);
    snprintf(ci->managerName, sizeof(ci->managerName), "%s", wr->manager_name);

    ci->status = wr->status;
    ci->managerId = wr->manager_id;
    ci->numServers = wr->num_servers;
    ci->numClients = wr->num_clients;

    // Future-proofing: all extended fields empty
    ci->nRes = ci->nTypes = ci->nModels = ci->nAdmins = 0;
    ci->resources = ci->hostTypes = ci->hostModels = NULL;
    ci->adminIds = NULL;
    ci->admins = NULL;

    free(wr);

    return ci;
}

void ls_freeclusterinfo(struct clusterInfo **cis, int numclusters)
{
    struct clusterInfo *ci;

    if (cis == NULL || *cis == NULL)
        return;

    ci = *cis;

    for (int i = 0; i < numclusters; i++) {
        int j;

        if (ci[i].resources != NULL) {
            for (j = 0; j < ci[i].nRes; j++) {
                free(ci[i].resources[j]);
            }
            free(ci[i].resources);
        }

        if (ci[i].hostTypes != NULL) {
            for (j = 0; j < ci[i].nTypes; j++) {
                free(ci[i].hostTypes[j]);
            }
            free(ci[i].hostTypes);
        }

        if (ci[i].hostModels != NULL) {
            for (j = 0; j < ci[i].nModels; j++) {
                free(ci[i].hostModels[j]);
            }
            free(ci[i].hostModels);
        }

        if (ci[i].admins != NULL) {
            for (j = 0; j < ci[i].nAdmins; j++) {
                free(ci[i].admins[j]);
            }
            free(ci[i].admins);
        }

        if (ci[i].adminIds != NULL) {
            free(ci[i].adminIds);
        }
    }

    free(ci);
    *cis = NULL;
}

static struct wire_cluster_info *query_lim_clusterinfo(void)
{
    struct wire_cluster_info *reply;

    reply = calloc(1, sizeof(struct wire_cluster_info));
    if (reply == NULL) {
        return NULL;
    }

    int cc = callLim_(LIM_GET_CLUSINFO, NULL, NULL, reply,
                      xdr_wire_cluster_info, NULL, _USE_TCP_, NULL);

    if (cc < 0) {
        lserrno = LSE_MSG_SYS;
        free(reply);
        return NULL;
    }

    return reply;
}

struct hostInfo *ls_gethostinfo(char *resReq, int *numhosts, char **hostlist,
                                int listsize, int options)
{
    // Query LIM for all hosts
    struct wire_host_reply *reply = query_lim_hosts();
    if (!reply) {
        *numhosts = 0;
        return NULL;
    }

    // Convert wire format to API format

    struct hostInfo *api_hosts;
    api_hosts = calloc(reply->num_hosts, sizeof(struct hostInfo));
    if (api_hosts == NULL) {
        query_lim_host_free(reply);
        NULL;
    }

    for (int i = 0; i < reply->num_hosts; i++) {
        strcpy(api_hosts[i].hostName, reply->hosts[i].host_name);
        api_hosts[i].hostType = strdup(reply->hosts[i].host_type);
        api_hosts[i].hostModel = strdup(reply->hosts[i].host_model);
        api_hosts[i].cpuFactor = reply->hosts[i].cpu_factor;
        api_hosts[i].maxCpus = reply->hosts[i].max_cpus;
        api_hosts[i].maxMem = reply->hosts[i].max_mem;
        api_hosts[i].maxSwap = reply->hosts[i].max_swap;
        api_hosts[i].maxTmp = reply->hosts[i].max_tmp;
        api_hosts[i].nDisks = reply->hosts[i].num_disks;
        api_hosts[i].isServer = reply->hosts[i].is_server;
        api_hosts[i].nRes = 0;
        api_hosts[i].resources = NULL;
        api_hosts[i].windows = NULL;
        api_hosts[i].numIndx = 11;
        api_hosts[i].busyThreshold = NULL;
        api_hosts[i].rexPriority = 0;
    }

    *numhosts = reply->num_hosts;
    query_lim_host_free(reply);

    return api_hosts;
}

void ls_freehostinfo(struct hostInfo **hosts, int numhosts)
{
    if (!hosts || !*hosts)
        return;

    for (int i = 0; i < numhosts; i++) {
        free((*hosts)[i].hostType);
        free((*hosts)[i].hostModel);
    }

    free(*hosts);
    *hosts = NULL;
}

static struct wire_host_reply *query_lim_hosts(void)
{
    struct wire_host_reply *reply;

    reply = calloc(1, sizeof(struct wire_host_reply));
    if (reply == NULL) {
        return NULL;
    }

    int cc = callLim_(LIM_GET_HOSTINFO,         // operation
                      NULL,                     // no data to send
                      NULL,                     // no data xdr
                      reply,                   // reply from lim
                      xdr_wire_host_reply, // xdr the reply
                      NULL,                     // hostname unused
                      _USE_TCP_,                // transport
                      NULL);                    // packet header in output
    if (cc < 0) {
        lserrno = LSE_MSG_SYS;
        free(reply);
        return NULL;
    }

    return reply;
}

struct hostLoad *ls_load(char *resreq, int *numhosts, int options,
                         char *fromhost)
{
    struct wire_load_reply *reply;
    struct hostLoad *api_loads;

    // Ignore resreq, fromhost for now - get all loads

    // Query LIM for load data
    reply = query_lim_load();
    if (!reply) {
        *numhosts = 0;
        return NULL;
    }

    // Convert wire format to API format
    api_loads = calloc(reply->num_hosts, sizeof(struct hostLoad));
    if (!api_loads) {
        free(reply->hosts);
        *numhosts = 0;
        return NULL;
    }

    for (int i = 0; i < reply->num_hosts; i++) {
        strcpy(api_loads[i].hostName, reply->hosts[i].host_name);

        //* Allocate and copy status array
        api_loads[i].status = malloc(NBUILTINDEX * sizeof(int));
        memcpy(api_loads[i].status, reply->hosts[i].status,
               NBUILTINDEX * sizeof(int));

        // Allocate and copy load indices
        api_loads[i].li = malloc(NBUILTINDEX * sizeof(float));
        memcpy(api_loads[i].li, reply->hosts[i].load_indices,
               NBUILTINDEX * sizeof(float));
    }

    *numhosts = reply->num_hosts;
    // fre library functions
    query_lim_load_free(reply);

    return api_loads;
}

void ls_freeload(struct hostLoad *loads, int numhosts)
{
    if (loads == NULL)
        return;

    for (int i = 0; i < numhosts; i++) {
        free(loads[i].status);
        free(loads[i].li);
    }

    free(loads);
}

static struct wire_load_reply *query_lim_load(void)
{
    struct wire_load_reply *reply;

    reply = calloc(1, sizeof(struct wire_load_reply));
    if (reply == NULL) {
        return NULL;
    }

    int cc = callLim_(LIM_LOAD_REQ,
                      NULL,
                      NULL,
                      reply,
                      xdr_wire_load_reply,
                      NULL,
                      _USE_TCP_,
                      NULL);
    if (cc < 0) {
        lserrno = LSE_MSG_SYS;
        return NULL;
    }

    return reply;
}

struct lsInfo *ls_info(void)
{
    struct wire_lsinfo *reply = query_lim_lsinfo();
    if (!reply) {
        return NULL;
    }

    struct lsInfo *info;
    int i;

    info = calloc(1, sizeof(*info));
    if (!info) {
        return NULL;
    }

    info->nRes = reply->n_res;
    if (info->nRes > 0) {
        info->resTable = calloc((size_t)info->nRes, sizeof(struct resItem));
        if (!info->resTable) {
            free(info);
            return NULL;
        }

        for (i = 0; i < info->nRes; i++) {
            struct wire_res *wr = &reply->res_table[i];
            struct resItem *r = &info->resTable[i];

            memset(r, 0, sizeof(*r));

            snprintf(r->name, sizeof(r->name), "%s", wr->name);
            snprintf(r->des, sizeof(r->des), "%s", wr->des);

            r->valueType = wr->value_type;
            r->orderType = wr->order_type;
            r->flags = wr->flags;
            r->interval = wr->interval;
        }
    }

    info->nTypes = reply->n_types;
    if (info->nTypes > LL_HOSTTYPE_MAX) {
        info->nTypes = LL_HOSTTYPE_MAX;
    }

    for (i = 0; i < info->nTypes; i++) {
        snprintf(info->hostTypes[i], sizeof(info->hostTypes[i]), "%s",
                 reply->host_types[i].name);
    }

    // Host models
    info->nModels = reply->n_models;
    if (info->nModels > LL_HOSTMODEL_MAX) {
        info->nModels = LL_HOSTMODEL_MAX;
    }

    for (i = 0; i < info->nModels; i++) {
        struct wire_host_model *hm = &reply->host_models[i];

        snprintf(info->hostModels[i], sizeof(info->hostModels[i]), "%s",
                 hm->model);

        snprintf(info->hostArchs[i], sizeof(info->hostArchs[i]), "%s",
                 hm->arch);

        info->modelRefs[i] = hm->ref;
        info->cpuFactor[i] = hm->cpu_factor;
    }

    info->numIndx = reply->num_indx;
    info->numUsrIndx = reply->num_usr_indx;

    wire_lsinfo_free(reply);

    return info;
}


static struct wire_lsinfo *query_lim_lsinfo(void)
{
    struct wire_lsinfo *reply;

    reply = calloc(1, sizeof(struct wire_lsinfo));

    if (callLim_(LIM_GET_INFO, NULL, NULL, reply, xdr_wire_lsinfo, NULL,
                 _USE_TCP_, NULL) < 0) {
        free(reply);
        return NULL;
    }

    return reply;
}

void ls_info_free(struct lsInfo **info)
{
    if (info == NULL || *info == NULL)
        return;

    if ((*info)->resTable != NULL)
        free((*info)->resTable);

    free(*info);
    *info = NULL;
}


static void wire_lsinfo_free(struct wire_lsinfo *wr)
{
    if (wr == NULL)
        return;

    if (wr->res_table != NULL)
        free(wr->res_table);

    if (wr->host_types != NULL)
        free(wr->host_types);

    if (wr->host_models != NULL)
        free(wr->host_models);

    free(wr);
}

// Garbage to be thrown away...
struct lsSharedResourceInfo *
ls_sharedresourceinfo(char **resources, int *numResources,
                      char *hostName, int options)
{
    struct lsSharedResourceInfo *info;

    /* LavaLite for now: no shared/floating resources are implemented.
     * Return an empty list, but treat it as a successful call.
     * Caller must handle numResources == 0 gracefully.
     */

    if (numResources != NULL) {
        *numResources = 0;
    }

    /* Allocate a dummy one-element array so callers can safely free() it.
     * numResources == 0 ensures nobody indexes info[0].
     */
    info = calloc(1, sizeof(struct lsSharedResourceInfo));

    info[0].resourceName = NULL;
    info[0].nInstances   = 0;
    info[0].instances    = NULL;

    // shared resources not implemented yet; returning empty list

    return info;
}

char *ls_gethosttype(const char *hostname)
{
    struct hostInfo *hostinfo;
    static char hostType[MAXLSFNAMELEN];

    if (hostname == NULL)
        if ((hostname = ls_getmyhostname()) == NULL)
            return NULL;

    hostinfo = ls_gethostinfo("-", NULL, (char **) &hostname, 1, 0);
    if (hostinfo == NULL)
        return NULL;

    strcpy(hostType, hostinfo[0].hostType);
    return hostType;
}

char *ls_gethostmodel(const char *hostname)
{
    struct hostInfo *hostinfo;
    static char hostModel[MAXLSFNAMELEN];

    if (hostname == NULL)
        if ((hostname = ls_getmyhostname()) == NULL)
            return NULL;

    hostinfo = ls_gethostinfo("-", NULL, (char **) &hostname, 1, 0);
    if (hostinfo == NULL)
        return NULL;

    strcpy(hostModel, hostinfo[0].hostModel);
    return hostModel;
}

float ls_gethostfactor(const char *hostname)
{
    struct hostInfo *hostinfo;

    if (hostname == NULL)
        if ((hostname = ls_getmyhostname()) == NULL)
            return 0.0f;

    hostinfo = ls_gethostinfo("-", NULL, (char **) &hostname, 1, 0);
    if (hostinfo == NULL)
        return 0.0f;

    return hostinfo->cpuFactor;
}

float ls_getmodelfactor(const char *modelname)
{
    if (!modelname)
        return ls_gethostfactor(NULL);

    if (initenv_(NULL, NULL) < 0)
        return 0.0f;

    float cpuf;
    struct stringLen str;
    str.name = (char *) modelname;
    str.len = MAXLSFNAMELEN;
    if (callLim_(LIM_GET_CPUF, &str, xdr_stringLen, &cpuf, xdr_float, NULL,
                 _USE_TCP_, NULL) < 0)
        return 0.0f;

    return cpuf;
}

static void query_lim_host_free(struct wire_host_reply *reply)
{
    if (reply == NULL)
        return;

    for (int i = 0; i < reply->num_hosts; i++) {
        free(reply->hosts[i].host_name);
        free(reply->hosts[i].host_type);
        free(reply->hosts[i].host_model);
    }
    free(reply->hosts);
    free(reply);
}

static void query_lim_load_free(struct wire_load_reply *reply)
{
    if (reply == NULL)
        return;

    if (reply->hosts == NULL)
        return;

    for (int i = 0; i < reply->num_hosts; i++) {
        free(reply->hosts[i].host_name); // allocated by xdr_string()
        reply->hosts[i].host_name = NULL;
    }
    free(reply->hosts);
    reply->hosts = NULL;

    free(reply);
}
