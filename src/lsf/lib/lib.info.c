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
#include "lsf/lib/lib.h"
#include "lsf/lib/lib.channel.h"

struct masterInfo masterInfo;
int masterknown = false;

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

char *ls_getclustername(void)
{
    static char fname[] = "ls_getclustername";
    static char clName[MAXLSFNAMELEN];

    if (logclass & (LC_TRACE))
        ls_syslog(LOG_DEBUG, "%s: Entering this routine...", fname);

    if (clName[0] == '\0')
        if (getname_(LIM_GET_CLUSNAME, clName, MAXLSFNAMELEN) < 0)
            return NULL;

    return clName;
}

char *ls_getmastername(void)
{
    static __thread char master[MAXHOSTNAMELEN];

    if (getname_(LIM_GET_MASTINFO, master, MAXHOSTNAMELEN) < 0)
        return NULL;

    return master;
}

static struct wire_cluster_info_reply *query_lim_clusterinfo(void)
{
    static __thread struct wire_cluster_info_reply reply;
    int cc;

    memset(&reply, 0, sizeof(reply));

    cc = callLim_(LIM_GET_CLUSINFO, NULL, NULL, &reply,
                  xdr_wire_cluster_info_reply, NULL, _USE_TCP_, NULL);

    if (cc < 0) {
        lserrno = LSE_MSG_SYS;
        return NULL;
    }

    return &reply;
}

struct clusterInfo *
wire_clusterinfo_to_clusterInfo(const struct wire_cluster_info_reply *wr,
                                int *num_clusters)
{
    if (wr == NULL || num_clusters == NULL) {
        lserrno = LSE_BAD_ARGS;
        return NULL;
    }

    // We only have one cluster in the reply.
    *num_clusters = 1;

    struct clusterInfo *ci = calloc(1, sizeof(*ci));
    if (!ci) {
        lserrno = LSE_MALLOC;
        return NULL;
    }

    const struct wire_cluster_info *wc = &wr->cluster;

    snprintf(ci->clusterName, sizeof(ci->clusterName), "%s", wc->cluster_name);

    snprintf(ci->masterName, sizeof(ci->masterName), "%s", wc->master_name);

    snprintf(ci->managerName, sizeof(ci->managerName), "%s", wc->manager_name);

    ci->status = wc->status;
    ci->managerId = wc->manager_id;
    ci->numServers = wc->num_servers;
    ci->numClients = wc->num_clients;

    // Future-proofing: all extended fields empty
    ci->nRes = ci->nTypes = ci->nModels = ci->nAdmins = 0;
    ci->resources = ci->hostTypes = ci->hostModels = NULL;
    ci->adminIds = NULL;
    ci->admins = NULL;

    return ci;
}

struct clusterInfo *ls_clusterinfo(char *resReq, int *numclusters,
                                   char **clusterList, int listsize,
                                   int options)
{
    struct wire_cluster_info_reply *wr;
    struct clusterInfo *ci;

    (void) resReq;
    (void) clusterList;
    (void) listsize;
    (void) options;

    if (numclusters == NULL) {
        lserrno = LSE_BAD_ARGS;
        return NULL;
    }

    *numclusters = 0;

    wr = query_lim_clusterinfo();
    if (wr == NULL) {
        return NULL;
    }

    ci = wire_clusterinfo_to_clusterInfo(wr, numclusters);
    if (ci == NULL) {
        /* lserrno already set by translator */
        return NULL;
    }

    return ci;
}

static struct wire_host_info_reply *query_lim_hosts(void)
{
    static __thread struct wire_host_info_reply reply;

    int cc = callLim_(LIM_GET_HOSTINFO,         // operation
                      NULL,                     // no data to send
                      NULL,                     // no data xdr
                      &reply,                   // reply from lim
                      xdr_wire_host_info_reply, // xdr the reply
                      NULL,                     // hostname unused
                      _USE_TCP_,                // transport
                      NULL);                    // packet header in output
    if (cc < 0) {
        lserrno = LSE_MSG_SYS;
        return NULL;
    }

    return &reply;
}

struct hostInfo *ls_gethostinfo(char *resReq, int *numhosts, char **hostlist,
                                int listsize, int options)
{
    struct wire_host_info_reply *reply;
    struct hostInfo *api_hosts;

    /* Ignore resReq, hostlist, listsize for now - just get all hosts */

    /* Query LIM for all hosts */
    reply = query_lim_hosts();
    if (!reply) {
        *numhosts = 0;
        return NULL;
    }

    /* Convert wire format to API format
     */
    api_hosts = calloc(reply->num_hosts, sizeof(struct hostInfo));

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

    free(reply->hosts);
    *numhosts = reply->num_hosts;
    return api_hosts;
}

static struct wire_load_info_reply *query_lim_load(void)
{
    static __thread struct wire_load_info_reply reply;

    int cc = callLim_(LIM_LOAD_REQ, NULL, NULL, &reply,
                      xdr_wire_load_info_reply, NULL, _USE_TCP_, NULL);

    if (cc < 0) {
        lserrno = LSE_MSG_SYS;
        return NULL;
    }

    return &reply;
}

struct hostLoad *ls_load(char *resreq, int *numhosts, int options,
                         char *fromhost)
{
    struct wire_load_info_reply *reply;
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
    free(reply->hosts);

    return api_loads;
}

// LavaLite ls_info()
static struct lsInfo *
wire_lsinfo_to_lsinfo(const struct wire_lsinfo_reply *reply)
{
    struct lsInfo *info;
    int i;

    info = calloc(1, sizeof(*info));
    if (!info) {
        return NULL;
    }

    /* Resources */
    info->nRes = reply->n_res;
    if (info->nRes > 0) {
        info->resTable = calloc((size_t) info->nRes, sizeof(struct resItem));
        if (!info->resTable) {
            free(info);
            return NULL;
        }

        for (i = 0; i < info->nRes; i++) {
            const struct wire_res_item *wr = &reply->res_table[i];
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

    // Host types
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
        const struct wire_host_model *hm = &reply->host_models[i];

        snprintf(info->hostModels[i], sizeof(info->hostModels[i]), "%s",
                 hm->model);

        snprintf(info->hostArchs[i], sizeof(info->hostArchs[i]), "%s",
                 hm->arch);

        info->modelRefs[i] = hm->ref;
        info->cpuFactor[i] = hm->cpu_factor;
    }

    info->numIndx = reply->num_indx;
    info->numUsrIndx = reply->num_usr_indx;

    return info;
}

static struct wire_lsinfo_reply *query_lim_lsinfo(void)
{
    static __thread struct wire_lsinfo_reply reply;

    if (callLim_(LIM_GET_INFO, NULL, NULL, &reply, xdr_wire_lsinfo_reply, NULL,
                 _USE_TCP_, NULL) < 0)
        return NULL;

    return &reply;
}

struct lsInfo *ls_info(void)
{
    struct wire_lsinfo_reply *reply;
    struct lsInfo *info;

    reply = query_lim_lsinfo();
    if (!reply) {
        return NULL;
    }

    info = wire_lsinfo_to_lsinfo(reply);
    if (!info) {
        lserrno = LSE_NO_MEM;
        return NULL;
    }

    return info;
}

// Garbage to be thrown away...
struct lsSharedResourceInfo *ls_sharedresourceinfo(char **resources,
                                                   int *numResources,
                                                   char *hostName, int options)
{
    static char fname[] = "ls_sharedresourceinfo";
    static struct resourceInfoReq resourceInfoReq;
    int cc, i;
    static struct resourceInfoReply resourceInfoReply;
    static struct packet_header replyHdr;
    static int first = true;

    if (logclass & (LC_TRACE))
        ls_syslog(LOG_DEBUG1, "%s: Entering this routine...", fname);

    if (first == true) {
        resourceInfoReply.numResources = 0;
        resourceInfoReq.resourceNames = NULL;
        resourceInfoReq.numResourceNames = 0;
        resourceInfoReq.hostName = NULL;
        first = false;
    }

    if (resourceInfoReply.numResources > 0)
        xdr_lsffree(xdr_resourceInfoReply, (char *) &resourceInfoReply,
                    &replyHdr);
    FREEUP(resourceInfoReq.resourceNames);
    FREEUP(resourceInfoReq.hostName);

    if (numResources == NULL || *numResources < 0 ||
        (resources == NULL && *numResources > 0)) {
        lserrno = LSE_BAD_ARGS;
        return NULL;
    }
    if (*numResources == 0 && resources == NULL) {
        if ((resourceInfoReq.resourceNames = malloc(sizeof(char *))) == NULL) {
            lserrno = LSE_MALLOC;
            return NULL;
        }
        resourceInfoReq.resourceNames[0] = "";
        resourceInfoReq.numResourceNames = 1;
    } else {
        if ((resourceInfoReq.resourceNames =
                 malloc(*numResources * sizeof(char *))) == NULL) {
            lserrno = LSE_MALLOC;
            return NULL;
        }
        for (i = 0; i < *numResources; i++) {
            if (resources[i] && strlen(resources[i]) + 1 < MAXLSFNAMELEN)
                resourceInfoReq.resourceNames[i] = resources[i];
            else {
                FREEUP(resourceInfoReq.resourceNames);
                lserrno = LSE_BAD_RESOURCE;
                *numResources = i;
                return NULL;
            }
            resourceInfoReq.numResourceNames = *numResources;
        }
    }
    if (hostName != NULL) {
        resourceInfoReq.hostName = putstr_(hostName);
    } else
        resourceInfoReq.hostName = putstr_(" ");

    cc = callLim_(LIM_GET_RESOUINFO, &resourceInfoReq, xdr_resourceInfoReq,
                  &resourceInfoReply, xdr_resourceInfoReply, NULL, _USE_TCP_,
                  &replyHdr);
    if (cc < 0) {
        return NULL;
    }

    *numResources = resourceInfoReply.numResources;
    return resourceInfoReply.resources;
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
