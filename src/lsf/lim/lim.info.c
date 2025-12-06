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
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#include "lsf/lim/lim.h"
// lim_info.c - LIM request handlers
// Simplified: dump all data, let clients filter

void wrong_master(struct client_node *node)
{
    enum limReplyCode limReplyCode;
    XDR xdrs;
    struct packet_header hdr;
    struct masterInfo masterInfo;
    int cc;
    char *reply_buf;

    struct sockaddr_in addr;
    get_host_addrv4(node->from_host->v4_epoint, &addr);

    if (myClusterPtr->masterKnown) {
        limReplyCode = LIME_WRONG_MASTER;
        strcpy(masterInfo.hostName, myClusterPtr->masterPtr->hostName);
        get_host_addrv4(myClusterPtr->masterPtr->v4_epoint, &masterInfo.addr);
        masterInfo.portno = myClusterPtr->masterPtr->statInfo.portno;
        reply_buf = (char *) &masterInfo;
    } else {
        limReplyCode = LIME_MASTER_UNKNW;
        reply_buf = NULL;
    }

    char buf[LL_BUFSIZ_256];
    xdrmem_create(&xdrs, buf, sizeof(buf), XDR_ENCODE);

    init_pack_hdr(&hdr);
    hdr.operation = limReplyCode;

    if (!xdr_encodeMsg(&xdrs, (char *) &reply_buf, &hdr, xdr_masterInfo, 0,
                       NULL)) {
        LS_ERR("xdr_encodeMsg() failed");
        xdr_destroy(&xdrs);
        return;
    }

    if (logclass & LC_COMM)
        ls_syslog(LOG_DEBUG, "%s: Sending to %s", __func__,
                  sockAdd2Str_(&addr));

    cc = chan_write(node->ch_id, buf, XDR_GETPOS(&xdrs));
    if (cc < 0) {
        ls_syslog(LOG_ERR, "%s: send to %s failed: %m", __func__,
                  sockAdd2Str_(&addr));
        xdr_destroy(&xdrs);
        shutdown_client(node);
        return;
    }

    shutdown_client(node);
    xdr_destroy(&xdrs);
}

// Send error reply and close connection
void send_header(struct client_node *client, struct packet_header *reqHdr,
                 enum limReplyCode code)
{
    char buf[LL_BUFSIZ_512];
    XDR xdrs;
    struct packet_header replyHdr;

    xdrmem_create(&xdrs, buf, sizeof(buf), XDR_ENCODE);
    init_pack_hdr(&replyHdr);
    replyHdr.operation = code;
    replyHdr.sequence = reqHdr->sequence;

    xdr_encodeMsg(&xdrs, NULL, &replyHdr, NULL, 0, NULL);
    chan_write(client->ch_id, buf, XDR_GETPOS(&xdrs));

    xdr_destroy(&xdrs);
    shutdown_client(client);
}

void host_info_req(XDR *xdrs, struct client_node *client,
                   struct packet_header *req_hdr)
{
    struct wire_host_info_reply reply;

    if (!masterMe) {
        wrong_master(client);
        return;
    }

    // Count hosts
    int num_hosts = 0;
    for (struct hostNode *h = myClusterPtr->hostList; h; h = h->nextPtr) {
        num_hosts++;
    }

    // Allocate
    reply.num_hosts = num_hosts;
    reply.hosts = calloc(num_hosts, sizeof(struct wire_host_info));

    // Fill
    int i = 0;
    for (struct hostNode *h = myClusterPtr->hostList; h; h = h->nextPtr) {
        struct wire_host_info *info = &reply.hosts[i];
        ++i;

        info->host_name = strdup(h->hostName);
        info->host_type = strdup(h->statInfo.hostType);
        info->host_model = strdup(h->statInfo.hostArch);
        // Look up cpuFactor from global table
        info->cpu_factor = allInfo.cpuFactor[h->hModelNo];

        info->max_cpus = h->statInfo.maxCpus;
        info->max_mem = h->statInfo.maxMem;
        info->max_swap = h->statInfo.maxSwap;
        info->max_tmp = h->statInfo.maxTmp;
        info->num_disks = h->statInfo.nDisks;
        info->is_server = 1; // or check some flag
    }

    XDR xdrs2;
    char buf[LL_BUFSIZ_4K];
    struct packet_header hdr;

    xdrmem_create(&xdrs2, buf, sizeof(buf), XDR_ENCODE);
    init_pack_hdr(&hdr);
    hdr.operation = LIME_NO_ERR;

    xdr_encodeMsg(&xdrs2, (char *) &reply, &hdr, xdr_wire_host_info_reply, 0,
                  NULL);

    int cc = chan_write(client->ch_id, buf, XDR_GETPOS(&xdrs2));
    if (cc < 0) {
        LS_ERR("chan_write() failed");
        shutdown_client(client);
        free(reply.hosts);
        xdr_destroy(&xdrs2);
    }

    // The tcp caller will destroy the client
    free(reply.hosts);
    xdr_destroy(&xdrs2);
}

static int lsinfo_to_wire(const struct lsInfo *src,
                          struct wire_lsinfo_reply *dst)
{
    int i;

    if (src == NULL || dst == NULL) {
        LS_ERR("invalid arguments");
        return -1;
    }

    memset(dst, 0, sizeof(*dst));

    dst->n_res = src->nRes;

    if (dst->n_res > 0) {
        dst->res_table =
            calloc((size_t) dst->n_res, sizeof(struct wire_res_item));

        if (dst->res_table == NULL) {
            LS_ERR("calloc(res_table) failed");
            return -1;
        }

        for (i = 0; i < dst->n_res; i++) {
            struct wire_res_item *wr;
            const struct resItem *r;

            wr = &dst->res_table[i];
            r = &src->resTable[i];

            memset(wr, 0, sizeof(*wr));

            snprintf(wr->name, sizeof(wr->name), "%s", r->name);

            snprintf(wr->des, sizeof(wr->des), "%s", r->des);

            wr->value_type = r->valueType;
            wr->order_type = r->orderType;
            wr->flags = r->flags;
            wr->interval = r->interval;
        }
    }

    dst->n_types = src->nTypes;

    if (dst->n_types > 0) {
        dst->host_types =
            calloc((size_t) dst->n_types, sizeof(struct wire_host_type));

        if (dst->host_types == NULL) {
            LS_ERR("calloc(host_types) failed");
            return -1;
        }

        for (i = 0; i < dst->n_types; i++) {
            struct wire_host_type *ht;

            ht = &dst->host_types[i];
            memset(ht, 0, sizeof(*ht));

            snprintf(ht->name, sizeof(ht->name), "%s", src->hostTypes[i]);
        }
    }

    dst->n_models = src->nModels;

    if (dst->n_models > 0) {
        dst->host_models =
            calloc((size_t) dst->n_models, sizeof(struct wire_host_model));

        if (dst->host_models == NULL) {
            LS_ERR("calloc(host_models) failed");
            return -1;
        }

        for (i = 0; i < dst->n_models; i++) {
            struct wire_host_model *hm;

            hm = &dst->host_models[i];
            memset(hm, 0, sizeof(*hm));

            snprintf(hm->model, sizeof(hm->model), "%s", src->hostModels[i]);

            snprintf(hm->arch, sizeof(hm->arch), "%s", src->hostArchs[i]);

            hm->ref = src->modelRefs[i];
            hm->cpu_factor = src->cpuFactor[i];
        }
    }

    dst->num_indx = src->numIndx;
    dst->num_usr_indx = src->numUsrIndx;

    return 0;
}

static void wire_lsinfo_free(struct wire_lsinfo_reply *wi)
{
    if (wi == NULL) {
        LS_WARNING("called with NULL pointer");
        return;
    }

    free(wi->res_table);
    wi->res_table = NULL;

    free(wi->host_types);
    wi->host_types = NULL;

    free(wi->host_models);
    wi->host_models = NULL;

    wi->n_res = 0;
    wi->n_types = 0;
    wi->n_models = 0;
    wi->num_indx = 0;
    wi->num_usr_indx = 0;
}

// infoReq - generic cluster info
void info_req(XDR *xdrs, struct client_node *client,
              struct packet_header *req_hdr)
{
    char buf[LL_BUFSIZ_16K];
    XDR xdrs2;
    enum limReplyCode limReplyCode;
    struct wire_lsinfo_reply reply;
    int cc;

    struct sockaddr_in addr;
    get_host_addrv4(client->from_host->v4_epoint, &addr);

    (void) xdrs; // request body unused for now

    limReplyCode = LIME_NO_ERR;

    memset(&reply, 0, sizeof(reply));
    // Convert the canonical in-memory lsInfo (allInfo)
    // into the wire-level reply structure.
    if (lsinfo_to_wire(&allInfo, &reply) < 0) {
        LS_ERR("lsinfo_to_wire() to %s failed", sockAdd2Str_(&addr));
        // We do not send a partial reply.
        return;
    }

    xdrmem_create(&xdrs2, buf, sizeof(buf), XDR_ENCODE);

    struct packet_header reply_hdr;
    init_pack_hdr(&reply_hdr);
    reply_hdr.operation = limReplyCode;

    if (!xdr_encodeMsg(&xdrs2, (char *) &reply, &reply_hdr,
                       xdr_wire_lsinfo_reply, 0, NULL)) {
        LS_ERR("xdr_encodeMsg to %s failed", sockAdd2Str_(&addr));
        xdr_destroy(&xdrs2);
        wire_lsinfo_free(&reply);
        return;
    }

    cc = chan_write(client->ch_id, buf, XDR_GETPOS(&xdrs2));
    if (cc < 0) {
        LS_ERR("chan_write() to %s failed", sockAdd2Str_(&addr));
        xdr_destroy(&xdrs2);
        wire_lsinfo_free(&reply);
        return;
    }

    xdr_destroy(&xdrs2);
    wire_lsinfo_free(&reply);
}

// ls_load()
void load_req(XDR *xdrs, struct client_node *client,
              struct packet_header *req_hdr)
{
    struct wire_load_info_reply reply;
    XDR xdrs2;
    char buf[LL_BUFSIZ_4K];
    struct packet_header hdr;

    if (!masterMe) {
        wrong_master(client);
        return;
    }

    // Count hosts
    int num_hosts = 0;
    for (struct hostNode *h = myClusterPtr->hostList; h; h = h->nextPtr) {
        num_hosts++;
    }

    // Allocate
    reply.num_hosts = num_hosts;
    reply.hosts = calloc(num_hosts, sizeof(struct wire_load_info));
    if (!reply.hosts) {
        LS_ERR("calloc failed");
        send_header(client, req_hdr, LIME_NO_MEM);
        return;
    }

    // Fill the wire structure
    int i = 0;
    for (struct hostNode *h = myClusterPtr->hostList; h; h = h->nextPtr) {
        struct wire_load_info *host = &reply.hosts[i];
        ++i;
        host->host_name = strdup(h->hostName); // Fixed: was comma, need strdup

        memcpy(host->load_indices, h->loadIndex, NBUILTINDEX * sizeof(float));
        memcpy(host->status, h->status, NBUILTINDEX * sizeof(int));
    }

    // Encode and send
    xdrmem_create(&xdrs2, buf, sizeof(buf), XDR_ENCODE);
    init_pack_hdr(&hdr);
    hdr.operation = LIME_NO_ERR;

    if (!xdr_encodeMsg(&xdrs2, (char *) &reply, &hdr, xdr_wire_load_info_reply,
                       0, NULL)) {
        LS_ERR("xdr_encodeMsg failed");
        free(reply.hosts);
        xdr_destroy(&xdrs2);
        return;
    }

    int cc = chan_write(client->ch_id, buf, XDR_GETPOS(&xdrs2));
    if (cc < 0) {
        LS_ERR("chan_write() failed");
        // fall through
    }

    free(reply.hosts);
    xdr_destroy(&xdrs2);
}

void resource_info_req(XDR *xdrs, struct client_node *client,
                       struct packet_header *reqHdr)
{
#if 0
    struct resourceInfoReq req;
    struct resourceInfoReply reply;
    struct packet_header replyHdr;
    char buf[MSGSIZE * 4];
    XDR xdrs2;

    if (!xdr_resourceInfoReq(xdrs, &req, reqHdr)) {
        send_header(client, reqHdr, LIME_BAD_DATA);
        return;
    }

    // Free request data
    // for now leaked it
    if (!masterMe) {
        wrong_master(client);
        return;
    }

    // Return all resources
    reply.nTypes = allInfo.nTypes;
    reply.nModels = allInfo.nModels;
    reply.hostTypes = allInfo.hostTypes;
    reply.hostModels = allInfo.hostModels;
    reply.hostArchs = allInfo.hostArchs;
    reply.nRes = allInfo.nRes;
    reply.resources = allInfo.resTable;

    xdrmem_create(&xdrs2, buf, sizeof(buf), XDR_ENCODE);
    init_pack_hdr(&replyHdr);
    replyHdr.operation = LIME_NO_ERR;
    replyHdr.sequence = reqHdr->sequence;

    if (!xdr_encodeMsg(&xdrs2, &reply, &replyHdr, xdr_resouInfoReply, 0, NULL)) {
        syslog(LOG_ERR, "%s: xdr_encodeMsg failed", __func__);
        xdr_destroy(&xdrs2);
        shutdown_client(client);
        return;
    }

    if (chan_write(client->ch_id, buf, XDR_GETPOS(&xdrs2)) < 0) {
        syslog(LOG_ERR, "%s: chan_write failed: %m", __func__);
    }

    xdr_destroy(&xdrs2);
    shutdown_client(client);
#endif
}

// LIM_GET_CLUSINFO
void clus_info_req(XDR *xdrs, struct client_node *client,
                   struct packet_header *req_hdr)
{
    XDR xdrs2;
    char buf[LL_BUFSIZ_16K];
    struct packet_header reply_hdr;
    struct wire_cluster_info_reply reply;
    enum limReplyCode limReplyCode;
    int cc;

    if (!masterMe) {
        wrong_master(client);
        return;
    }

    limReplyCode = LIME_NO_ERR;

    // Initialize everything, including the nested cluster struct.
    memset(&reply, 0, sizeof(reply));

    // Safe defaults so XDR never sees uninitialized data.
    reply.cluster.status = CLUST_STAT_UNAVAIL;

    snprintf(reply.cluster.cluster_name, sizeof(reply.cluster.cluster_name),
             "%s", myClusterPtr->clName);

    snprintf(reply.cluster.master_name, sizeof(reply.cluster.master_name), "%s",
             "master_unknown");

    snprintf(reply.cluster.manager_name, sizeof(reply.cluster.manager_name),
             "%s", "manager_unknown");

    // Availability comes only from cluster status bits now.
    if (myClusterPtr->status & CLUST_INFO_AVAIL) {
        reply.cluster.status = CLUST_STAT_OK;
    }

    // Master name: if we know it, overwrite the default.
    if (myClusterPtr->masterPtr != NULL) {
        snprintf(reply.cluster.master_name, sizeof(reply.cluster.master_name),
                 "%s", myClusterPtr->masterPtr->hostName);
    }

    if (myClusterPtr->managerName != NULL) {
        snprintf(reply.cluster.manager_name, sizeof(reply.cluster.manager_name),
                 "%s", myClusterPtr->managerName);
    }

    reply.cluster.manager_id = myClusterPtr->managerId;
    reply.cluster.num_servers = myClusterPtr->numHosts;
    reply.cluster.num_clients = myClusterPtr->numClients;

    struct sockaddr_in addr;
    get_host_addrv4(client->from_host->v4_epoint, &addr);

    xdrmem_create(&xdrs2, buf, sizeof(buf), XDR_ENCODE);

    init_pack_hdr(&reply_hdr);
    reply_hdr.operation = limReplyCode;

    if (!xdr_encodeMsg(&xdrs2, (char *) &reply, &reply_hdr,
                       xdr_wire_cluster_info_reply, 0, NULL)) {
        LS_ERR("xdr_encodeMsg() to %s failed", sockAdd2Str_(&addr));
        xdr_destroy(&xdrs2);
        return;
    }

    cc = chan_write(client->ch_id, buf, XDR_GETPOS(&xdrs2));
    if (cc < 0) {
        LS_ERR("chan_write() to %s failed", sockAdd2Str_(&addr));
        xdr_destroy(&xdrs2);
        return;
    }

    xdr_destroy(&xdrs2);
}
