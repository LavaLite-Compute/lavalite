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

void wrongMaster(struct client_node *node)
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

    if (!xdr_encodeMsg(&xdrs, (char *)&reply_buf, &hdr, xdr_masterInfo, 0,
                       NULL)) {
        LS_ERR("xdr_encodeMsg() failed");
        xdr_destroy(&xdrs);
        return;
    }

    if (logclass & LC_COMM)
        ls_syslog(LOG_DEBUG, "%s: Sending to %s", __func__, sockAdd2Str_(&addr));

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

void hostInfoReq(XDR *xdrs, struct client_node *client,
                 struct packet_header *req_hdr)
{
    struct host_info_reply reply;

    if (!masterMe) {
        wrongMaster(client);
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

        info->hostName = strdup(h->hostName);
        info->hostType = strdup(h->statInfo.hostType);
        info->hostModel = strdup(h->statInfo.hostArch);
         // Look up cpuFactor from global table
        info->cpuFactor = allInfo.cpuFactor[h->hModelNo];

        info->maxCpus = h->statInfo.maxCpus;
        info->maxMem = h->statInfo.maxMem;
        info->maxSwap = h->statInfo.maxSwap;
        info->maxTmp = h->statInfo.maxTmp;
        info->nDisks = h->statInfo.nDisks;
        info->isServer = 1; // or check some flag
    }

    XDR xdrs2;
    char buf[LL_BUFSIZ_4K];
    struct packet_header hdr;

    xdrmem_create(&xdrs2, buf, sizeof(buf), XDR_ENCODE);
    init_pack_hdr(&hdr);
    hdr.operation = LIME_NO_ERR;

    xdr_encodeMsg(&xdrs2, (char *)&reply, &hdr, xdr_host_info_reply, 0, NULL);

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

// resourceInfoReq - return all resource info
void resourceInfoReq(XDR *xdrs, struct client_node *client,
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
        wrongMaster(client);
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

// loadReq - return load info for all hosts
void loadReq(XDR *xdrs, struct client_node *client,
             struct packet_header *reqHdr)
{
#if 0
    struct loadReq req;
    struct loadReply reply;
    struct packet_header replyHdr;
    char *buf;
    XDR xdrs2;
    int bufSize;

    if (!xdr_loadReq(xdrs, &req, reqHdr)) {
        send_header(client, reqHdr, LIME_BAD_DATA);
        return;
    }

    // Free request
    for (int i = 0; i < req.numPrefs; i++)
        free(req.preferredHosts[i]);
    free(req.preferredHosts);

    if (!masterMe) {
        char tmpBuf[MSGSIZE];
        wrongMaster(client);
        return;
    }

    // Return load for all hosts
    reply.nEntry = allInfo.numHosts;
    reply.loadMatrix = calloc(reply.nEntry, sizeof(float *));

    if (!reply.loadMatrix) {
        syslog(LOG_ERR, "%s: calloc failed: %m", __func__);
        send_header(client, reqHdr, LIME_NO_MEM);
        return;
    }

    for (int i = 0; i < reply.nEntry; i++) {
        reply.loadMatrix[i] = calloc(allInfo.numIndx, sizeof(float));
        if (!reply.loadMatrix[i]) {
            syslog(LOG_ERR, "%s: calloc failed: %m", __func__);
            for (int j = 0; j < i; j++)
                free(reply.loadMatrix[j]);
            free(reply.loadMatrix);
            send_header(client, reqHdr, LIME_NO_MEM);
            return;
        }
        memcpy(reply.loadMatrix[i], hostNodes[i]->loadIndex,
               allInfo.numIndx * sizeof(float));
    }

    bufSize = MSGSIZE + reply.nEntry * allInfo.numIndx * sizeof(float);
    buf = malloc(bufSize);
    if (!buf) {
        syslog(LOG_ERR, "%s: malloc failed: %m", __func__);
        for (int i = 0; i < reply.nEntry; i++)
            free(reply.loadMatrix[i]);
        free(reply.loadMatrix);
        send_header(client, reqHdr, LIME_NO_MEM);
        return;
    }

    xdrmem_create(&xdrs2, buf, bufSize, XDR_ENCODE);
    init_pack_hdr(&replyHdr);
    replyHdr.operation = LIME_NO_ERR;
    replyHdr.sequence = reqHdr->sequence;

    if (!xdr_encodeMsg(&xdrs2, &reply, &replyHdr, xdr_loadReply, 0, NULL)) {
        syslog(LOG_ERR, "%s: xdr_encodeMsg failed", __func__);
        xdr_destroy(&xdrs2);
        free(buf);
        for (int i = 0; i < reply.nEntry; i++)
            free(reply.loadMatrix[i]);
        free(reply.loadMatrix);
        shutdown_client(client);
        return;
    }

    if (chan_write(client->ch_id, buf, XDR_GETPOS(&xdrs2)) < 0) {
        syslog(LOG_ERR, "%s: chan_write failed: %m", __func__);
    }

    xdr_destroy(&xdrs2);
    free(buf);
    for (int i = 0; i < reply.nEntry; i++)
        free(reply.loadMatrix[i]);
    free(reply.loadMatrix);
    shutdown_client(client);
#endif
}

// placeReq - placement request (dump all hosts)
void placeReq(XDR *xdrs, struct client_node *client,
              struct packet_header *reqHdr)
{
#if 0
    struct decisionReq req;
    struct placeReply reply;
    struct packet_header replyHdr;
    char *buf;
    XDR xdrs2;
    int bufSize;

    if (!xdr_decisionReq(xdrs, &req, reqHdr)) {
        send_header(client, reqHdr, LIME_BAD_DATA);
        return;
    }

    for (int i = 0; i < req.numPrefs; i++)
        free(req.preferredHosts[i]);
    free(req.preferredHosts);

    if (!masterMe) {
        char tmpBuf[MSGSIZE];
        wrongMaster(client);
        return;
    }

    // Return all hosts as candidates
    reply.numHosts = allInfo.numHosts;
    reply.hostNames = calloc(reply.numHosts, sizeof(char *));

    if (!reply.hostNames) {
        syslog(LOG_ERR, "%s: calloc failed: %m", __func__);
        send_header(client, reqHdr, LIME_NO_MEM);
        return;
    }

    for (int i = 0; i < reply.numHosts; i++) {
        reply.hostNames[i] = strdup(hostNodes[i]->hostName);
    }

    bufSize = MSGSIZE + reply.numHosts * MAXHOSTNAMELEN;
    buf = malloc(bufSize);
    if (!buf) {
        syslog(LOG_ERR, "%s: malloc failed: %m", __func__);
        for (int i = 0; i < reply.numHosts; i++)
            free(reply.hostNames[i]);
        free(reply.hostNames);
        send_header(client, reqHdr, LIME_NO_MEM);
        return;
    }

    xdrmem_create(&xdrs2, buf, bufSize, XDR_ENCODE);
    init_pack_hdr(&replyHdr);
    replyHdr.operation = LIME_NO_ERR;
    replyHdr.sequence = reqHdr->sequence;

    if (!xdr_encodeMsg(&xdrs2, &reply, &replyHdr, xdr_placeReply, 0, NULL)) {
        syslog(LOG_ERR, "%s: xdr_encodeMsg failed", __func__);
        xdr_destroy(&xdrs2);
        free(buf);
        for (int i = 0; i < reply.numHosts; i++)
            free(reply.hostNames[i]);
        free(reply.hostNames);
        shutdown_client(client);
        return;
    }

    if (chan_write(client->ch_id, buf, XDR_GETPOS(&xdrs2)) < 0) {
        syslog(LOG_ERR, "%s: chan_write failed: %m", __func__);
    }

    xdr_destroy(&xdrs2);
    free(buf);
    for (int i = 0; i < reply.numHosts; i++)
        free(reply.hostNames[i]);
    free(reply.hostNames);
    shutdown_client(client);
#endif
}

// infoReq - generic cluster info
void infoReq(XDR *xdrs, struct client_node *client,
             struct packet_header *reqHdr)
{
#if 0
    struct infoReq req;
    struct clusterInfo reply;
    struct packet_header replyHdr;
    char buf[MSGSIZE * 2];
    XDR xdrs2;

    if (!xdr_infoReq(xdrs, &req, reqHdr)) {
        send_header(client, reqHdr, LIME_BAD_DATA);
        return;
    }

    if (!masterMe) {
        char tmpBuf[MSGSIZE];
        wrongMaster(client);
        return;
    }

    // Fill in cluster info
    reply.masterName = myClusterPtr->masterPtr->hostName;
    reply.managerName = myClusterPtr->managerId;
    reply.numServers = allInfo.numHosts;
    reply.numClients = 0;  // Count if needed
    reply.nRes = allInfo.nRes;
    reply.nTypes = allInfo.nTypes;
    reply.nModels = allInfo.nModels;
    reply.numIndx = allInfo.numIndx;
    reply.numUsrIndx = allInfo.numUsrIndx;
    reply.resTable = allInfo.resTable;

    xdrmem_create(&xdrs2, buf, sizeof(buf), XDR_ENCODE);
    init_pack_hdr(&replyHdr);
    replyHdr.operation = LIME_NO_ERR;
    replyHdr.sequence = reqHdr->sequence;

    if (!xdr_encodeMsg(&xdrs2, &reply, &replyHdr, xdr_clusterInfo, 0, NULL)) {
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
void loadadjReq(XDR *xdrs, struct client_node *client, struct packet_header *hdr)
{
}
// LIM_GET_CLUSINFO
void clusInfoReq(XDR *xdrs, struct client_node *from, struct packet_header *rhd)
{
#if 0
    XDR xdrs2;
    char buf[LL_BUFSIZ_4K];
    struct packet_header reply_hdr;
    struct clusterInfoReply clusterInfoReply;
    struct clusterInfoReq clusterInfoReq;

    if (!masterMe) {
        wrongMaster(client);
        return;
    }

    memset(&clusterInfoReq, 0, sizeof(clusterInfoReq));
    if (!xdr_clusterInfoReq(xdrs, &clusterInfoReq, reqHdr)) {
        ls_syslog(LOG_WARNING, "%s: xdr_clusterInfoReq() failed: %m", __func__);
        // Bug we should set lserrno limReplyCode = LIME_BAD_DATA;
        // and return -1 just like system calls.
        return;
    }

    clusterInfoReply.shortLsInfo = getCShortInfo(reqHdr);

    clusterInfoReply.nClus = 1;
    clusterInfoReply.clusterMatrix = calloc(1, sizeof(struct shortCInfo));

    strcpy(clusterInfoReply.clusterMatrix[0].clName, myClusterPtr->clName);

    clusterInfoReply.clusterMatrix[0].status = CLUST_STAT_UNAVAIL;
    if ((myClusterPtr->status & CLUST_INFO_AVAIL) &&
        (masterMe || (!masterMe && myClusterPtr->masterPtr != NULL))) {
        clusterInfoReply.clusterMatrix[0].status = CLUST_STAT_OK;
        strcpy(clusterInfoReply.clusterMatrix[0].masterName,
               myClusterPtr->masterPtr->hostName);

        strcpy(clusterInfoReply.clusterMatrix[0].managerName,
               myClusterPtr->managerName);

        clusterInfoReply.clusterMatrix[0].managerId = myClusterPtr->managerId;
        clusterInfoReply.clusterMatrix[0].numServers = myClusterPtr->numHosts;
        clusterInfoReply.clusterMatrix[0].numClients = myClusterPtr->numClients;
        clusterInfoReply.clusterMatrix[0].resClass = myClusterPtr->resClass;
        clusterInfoReply.clusterMatrix[0].typeClass = myClusterPtr->typeClass;
        clusterInfoReply.clusterMatrix[0].modelClass = myClusterPtr->modelClass;
        clusterInfoReply.clusterMatrix[0].numIndx = myClusterPtr->numIndx;
        clusterInfoReply.clusterMatrix[0].numUsrIndx = myClusterPtr->numUsrIndx;
        clusterInfoReply.clusterMatrix[0].usrIndxClass =
            myClusterPtr->usrIndxClass;
        clusterInfoReply.clusterMatrix[0].nAdmins = myClusterPtr->nAdmins;
        clusterInfoReply.clusterMatrix[0].adminIds = myClusterPtr->adminIds;
        clusterInfoReply.clusterMatrix[0].admins = myClusterPtr->admins;
        clusterInfoReply.clusterMatrix[0].nRes = allInfo.nRes;
        clusterInfoReply.clusterMatrix[0].resBitMaps = myClusterPtr->resBitMaps;
        clusterInfoReply.clusterMatrix[0].nTypes = allInfo.nTypes;
        clusterInfoReply.clusterMatrix[0].hostTypeBitMaps =
            myClusterPtr->hostTypeBitMaps;
        clusterInfoReply.clusterMatrix[0].nModels = allInfo.nModels;
        clusterInfoReply.clusterMatrix[0].hostModelBitMaps =
            myClusterPtr->hostModelBitMaps;
    }

    init_pack_hdr(&reply_hdr);
    reply_hdr.operation = LIME_NO_ERR;
    reply_hdr.sequence = reqHdr->sequence;

    xdrmem_create(&xdrs2, buf, sizeof(buf), XDR_ENCODE);

    if (!xdr_encodeMsg(&xdrs2, (char *) &clusterInfoReply, &reply_hdr,
                       xdr_clusterInfoReply, 0, NULL)) {
        ls_syslog(LOG_ERR, "%s: xdr_pack_hdr() failed: %m", __func__);
        if (clusterInfoReply.clusterMatrix != NULL)
            free(clusterInfoReply.clusterMatrix);
        xdr_destroy(&xdrs2);
        return;
    }

    int cc = chan_write(s, buf, XDR_GETPOS(&xdrs2));
    if (cc < 0) {
        ls_syslog(LOG_ERR, "%s: chan_write() to %s failed: %m", __func__,
                  sockAdd2Str_(from));
        xdr_destroy(&xdrs2);
        if (clusterInfoReply.clusterMatrix != NULL)
            free(clusterInfoReply.clusterMatrix);
        return;
    }

    if (clusterInfoReply.clusterMatrix != NULL)
        free(clusterInfoReply.clusterMatrix);

    xdr_destroy(&xdrs2);
#endif
}
