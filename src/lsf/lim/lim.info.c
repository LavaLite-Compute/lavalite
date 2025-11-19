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

#include "lsf/lim/lim.h"

struct shortLsInfo oldShortInfo;

static struct shortLsInfo *getCShortInfo(struct packet_header *);
static int checkResources(struct resourceInfoReq *, struct resourceInfoReply *,
                          int *);
static int copyResource(struct resourceInfoReply *, struct sharedResource *,
                        int *, char *);
static void freeResourceInfoReply(struct resourceInfoReply *);

// LIM_GET_CLUSINFO
void clusInfoReq(XDR *xdrs, struct sockaddr_in *from,
                 struct packet_header *reqHdr, int s)
{
    XDR xdrs2;
    char buf[LL_BUFSIZ_4K];
    struct packet_header reply_hdr;
    struct clusterInfoReply clusterInfoReply;
    struct clusterInfoReq clusterInfoReq;

    if (!masterMe) {
        wrongMaster(from, buf, reqHdr, -1);
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

    int cc = chanWrite_(s, buf, XDR_GETPOS(&xdrs2));
    if (cc < 0) {
        ls_syslog(LOG_ERR, "%s: chanWrite_() to %s failed: %m", __func__,
                  sockAdd2Str_(from));
        xdr_destroy(&xdrs2);
        if (clusterInfoReply.clusterMatrix != NULL)
            free(clusterInfoReply.clusterMatrix);
        return;
    }

    if (clusterInfoReply.clusterMatrix != NULL)
        free(clusterInfoReply.clusterMatrix);

    xdr_destroy(&xdrs2);
}

// Bug this code is dead man walking
void hostInfoReq(XDR *xdrs, struct hostNode *fromHostP,
                 struct sockaddr_in *from, struct packet_header *reqHdr, int s)
{
    static char fname[] = "hostInfoReq";
    char *buf;
    XDR xdrs2;
    enum limReplyCode limReplyCode;
    struct hostInfoReply hostInfoReply;
    struct decisionReq hostInfoRequest;
    struct resVal resVal;
    int i, ncandidates, propt, bufSize;
    struct packet_header replyHdr;
    char *replyStruct;
    char fromEligible, clName;
    struct tclHostData tclHostData;

    if (logclass & (LC_TRACE | LC_COMM))
        ls_syslog(LOG_DEBUG1, "%s: Entering this routine...", fname);

    initResVal(&resVal);

    if (!xdr_decisionReq(xdrs, &hostInfoRequest, reqHdr)) {
        limReplyCode = LIME_BAD_DATA;
        goto Reply1;
    }

    if (!(hostInfoRequest.ofWhat == OF_HOSTS && hostInfoRequest.numPrefs == 2 &&
          equal_host(hostInfoRequest.preferredHosts[1], myHostPtr->hostName))) {
        if (!masterMe) {
            char tmpBuf[MSGSIZE];

            wrongMaster(from, tmpBuf, reqHdr, s);
            for (i = 0; i < hostInfoRequest.numPrefs; i++)
                free(hostInfoRequest.preferredHosts[i]);
            free(hostInfoRequest.preferredHosts);
            return;
        }
    }

    if (!validHosts(hostInfoRequest.preferredHosts, hostInfoRequest.numPrefs,
                    &clName, hostInfoRequest.options)) {
        limReplyCode = LIME_UNKWN_HOST;
        ls_syslog(LOG_INFO,
                  "%s: validHosts( failed for bad cluster/host name requested "
                  "for <%s>",
                  fname, sockAdd2Str_(from));
        goto Reply;
    }

    propt = PR_SELECT;
    if (hostInfoRequest.options & DFT_FROMTYPE)
        propt |= PR_DEFFROMTYPE;

    getTclHostData(&tclHostData, myHostPtr, myHostPtr, true);
    tclHostData.ignDedicatedResource = true;
    int cc = parseResReq(hostInfoRequest.resReq, &resVal, &allInfo, propt);
    if (cc != PARSE_OK ||
        evalResReq(resVal.selectStr, &tclHostData,
                   hostInfoRequest.options & DFT_FROMTYPE) < 0) {
        if (cc == PARSE_BAD_VAL)
            limReplyCode = LIME_UNKWN_RVAL;
        else if (cc == PARSE_BAD_NAME)
            limReplyCode = LIME_UNKWN_RNAME;
        else
            limReplyCode = LIME_BAD_RESREQ;
        goto Reply;
    }

    strcpy(hostInfoRequest.hostType,
           (fromHostP->hTypeNo >= 0) ? shortInfo.hostTypes[fromHostP->hTypeNo]
                                     : "unknown");

    fromHostPtr = fromHostP;

    ncandidates = getEligibleSites(&resVal, &hostInfoRequest, 1, &fromEligible);

    if (ncandidates <= 0) {
        limReplyCode = ncandidates ? LIME_NO_MEM : LIME_NO_OKHOST;
        goto Reply;
    }
    hostInfoReply.shortLsInfo = getCShortInfo(reqHdr);
    hostInfoReply.nHost = ncandidates;
    hostInfoReply.nIndex = allInfo.numIndx;
    hostInfoReply.hostMatrix = calloc(ncandidates, sizeof(struct shortHInfo));
    if (hostInfoReply.hostMatrix == NULL) {
        ls_syslog(LOG_ERR, "%s: %s failed: %m", fname, "malloc");
        limReplyCode = LIME_NO_MEM;
        // Bug should this really happen abort the connection and die
        goto Reply;
    }

    for (i = 0; i < ncandidates; i++) {
        struct shortHInfo *infoPtr;
        infoPtr = &hostInfoReply.hostMatrix[i];
        if (candidates[i]->infoValid) {
            infoPtr->maxCpus = candidates[i]->statInfo.maxCpus;
            infoPtr->maxMem = candidates[i]->statInfo.maxMem;
            infoPtr->maxSwap = candidates[i]->statInfo.maxSwap;
            infoPtr->maxTmp = candidates[i]->statInfo.maxTmp;
            infoPtr->nDisks = candidates[i]->statInfo.nDisks;
            infoPtr->resBitMaps = candidates[i]->resBitMaps;
            infoPtr->nRInt = GET_INTNUM(allInfo.nRes);
        } else {
            infoPtr->maxCpus = 0;
            infoPtr->maxMem = 0;
            infoPtr->maxSwap = 0;
            infoPtr->maxTmp = 0;
            infoPtr->nDisks = 0;

            infoPtr->resBitMaps = candidates[i]->resBitMaps;
            infoPtr->nRInt = GET_INTNUM(allInfo.nRes);
        }
        infoPtr->hTypeIndx = candidates[i]->hTypeNo;
        infoPtr->hModelIndx = candidates[i]->hModelNo;
        infoPtr->resClass = candidates[i]->resClass;
        infoPtr->windows = candidates[i]->windows;
        strcpy(infoPtr->hostName, candidates[i]->hostName);
        infoPtr->busyThreshold = candidates[i]->busyThreshold;
        if (candidates[i]->hostInactivityCount == -1)
            infoPtr->flags = 0;
        else
            infoPtr->flags = HINFO_SERVER;
        if (definedSharedResource(candidates[i], &allInfo) == true) {
            infoPtr->flags |= HINFO_SHARED_RESOURCE;
        }
        infoPtr->rexPriority = candidates[i]->rexPriority;
    }
    limReplyCode = LIME_NO_ERR;

Reply:
    for (i = 0; i < hostInfoRequest.numPrefs; i++)
        free(hostInfoRequest.preferredHosts[i]);
    free(hostInfoRequest.preferredHosts);

Reply1:
    freeResVal(&resVal);
    init_pack_hdr(&replyHdr);
    replyHdr.operation = (short) limReplyCode;
    replyHdr.sequence = reqHdr->sequence;
    if (limReplyCode == LIME_NO_ERR) {
        replyStruct = (char *) &hostInfoReply;
        bufSize = ALIGNWORD_(MSGSIZE + hostInfoReply.nHost *
                                           (128 + hostInfoReply.nIndex * 4));
        bufSize = MAX(bufSize, 4 * MSGSIZE);
    } else {
        replyStruct = (char *) NULL;
        bufSize = 512;
    }

    buf = (char *) malloc(bufSize);
    if (!buf) {
        ls_syslog(LOG_ERR, "%s: %s failed: %m", fname, "malloc");
        if (limReplyCode == LIME_NO_ERR)
            free(hostInfoReply.hostMatrix);
        return;
    }
    xdrmem_create(&xdrs2, buf, bufSize, XDR_ENCODE);

    if (!xdr_encodeMsg(&xdrs2, replyStruct, &replyHdr, xdr_hostInfoReply, 0,
                       NULL)) {
        ls_syslog(LOG_ERR, "%s: %s failed: %m", fname, "xdr_encodeMsg");
        xdr_destroy(&xdrs2);
        if (limReplyCode == LIME_NO_ERR)
            free(hostInfoReply.hostMatrix);
        return;
    }
    if (limReplyCode == LIME_NO_ERR)
        free(hostInfoReply.hostMatrix);

    cc = chanWrite_(s, buf, XDR_GETPOS(&xdrs2));
    if (cc < 0) {
        ls_syslog(LOG_ERR, "%s: chanWrite() failed: %m", __func__,
                  sockAdd2Str_(from));
        xdr_destroy(&xdrs2);
        return;
    }

    free(buf);
    xdr_destroy(&xdrs2);
}

void infoReq(XDR *xdrs, struct sockaddr_in *from, struct packet_header *reqHdr,
             int s)
{
    char buf[LL_BUFSIZ_4K];
    XDR xdrs2;
    enum limReplyCode limReplyCode;
    struct packet_header replyHdr;
    int cc;

    if (logclass & (LC_TRACE | LC_COMM))
        ls_syslog(LOG_DEBUG1, "%s: Entering this routine...", __func__);

    limReplyCode = LIME_NO_ERR;

    xdrmem_create(&xdrs2, buf, sizeof(buf), XDR_ENCODE);
    init_pack_hdr(&replyHdr);
    replyHdr.operation = limReplyCode;
    replyHdr.sequence = reqHdr->sequence;

    if (!xdr_encodeMsg(&xdrs2, (char *) &allInfo, &replyHdr, xdr_lsInfo, 0,
                       NULL)) {
        ls_syslog(LOG_ERR, "%s: xdr_encodeMs failed: %m", __func__);
        xdr_destroy(&xdrs2);
        return;
    }

    cc = chanWrite_(s, buf, XDR_GETPOS(&xdrs2));
    if (cc < 0) {
        ls_syslog(LOG_ERR, "%s: chanWrite() failed: %m", __func__,
                  sockAdd2Str_(from));
        xdr_destroy(&xdrs2);
        return;
    }

    xdr_destroy(&xdrs2);
    return;
}

int validHosts(char **hostList, int num, char *clName, int options)
{
    static char fname[] = "validHosts";
    int i, cc;
    struct clusterNode *clPtr;

    *clName = false;
    clPtr = myClusterPtr;
    clPtr->status &= ~CLUST_ELIGIBLE;
    clPtr->status &= ~CLUST_ALL_ELIGIBLE;

    if (num == 1)
        myClusterPtr->status |= CLUST_ELIGIBLE;

    for (i = 0; i < num; i++) {
        if (!(clPtr->status & CLUST_ACTIVE))
            continue;
        if (((cc = strcmp(hostList[i], clPtr->clName)) == 0) ||
            find_node_by_cluster(clPtr->hostList, hostList[i]) != NULL ||
            find_node_by_cluster(clPtr->clientList, hostList[i]) != NULL) {
            if (i > 0) {
                if (!cc) {
                    clPtr->status |= CLUST_ALL_ELIGIBLE;
                    *clName = true;
                }
                clPtr->status |= CLUST_ELIGIBLE;
            }
        } else {
            if (logclass & (LC_TRACE | LC_SCHED))
                ls_syslog(LOG_DEBUG2, "%s: Cannot find host <%s>", fname,
                          hostList[i]);
            return false;
        }
    }

    return true;
}

static struct shortLsInfo *getCShortInfo(struct packet_header *reqHdr)
{
    int i;

    if (reqHdr->version >= 6) {
        return (&shortInfo);
    }

    oldShortInfo.nRes = shortInfo.nRes;
    oldShortInfo.resName = shortInfo.resName;
    if (shortInfo.nTypes > LL_HOSTTYPE_MAX) {
        oldShortInfo.nTypes = LL_HOSTTYPE_MAX;
    } else {
        oldShortInfo.nTypes = shortInfo.nTypes;
    }
    for (i = 0; i < oldShortInfo.nTypes; i++) {
        oldShortInfo.hostTypes[i] = shortInfo.hostTypes[i];
    }
    if (shortInfo.nModels > LL_HOSTMODEL_MAX) {
        oldShortInfo.nModels = LL_HOSTMODEL_MAX;
    } else {
        oldShortInfo.nModels = shortInfo.nModels;
    }
    for (i = 0; i < oldShortInfo.nModels; i++) {
        oldShortInfo.hostModels[i] = shortInfo.hostModels[i];
        oldShortInfo.cpuFactors[i] = shortInfo.cpuFactors[i];
    }

    if (reqHdr->version < 4) {
        if (shortInfo.nRes > MAXSRES) {
            oldShortInfo.nRes = MAXSRES;
        } else {
            oldShortInfo.nRes = shortInfo.nRes;
        }
        oldShortInfo.resName = shortInfo.resName;
    }

    return &oldShortInfo;
}

void resourceInfoReq2(XDR *xdrs, struct sockaddr_in *from,
                      struct packet_header *reqHdr, int s)
{
    static char fname[] = "resourceInfoReq";
    char buf1[MSGSIZE];
    XDR xdrs2;
    char *replyStruct;
    enum limReplyCode limReplyCode;
    struct packet_header replyHdr;
    struct resourceInfoReq resourceInfoReq;
    struct resourceInfoReply resourceInfoReply;
    int cc = 0;

    if (logclass & (LC_TRACE | LC_COMM))
        ls_syslog(LOG_DEBUG1, "%s: Entering this routine...", fname);

    limReplyCode = LIME_NO_ERR;

    if (!masterMe) {
        wrongMaster(from, buf1, reqHdr, s);
        return;
    }

    if (!xdr_resourceInfoReq(xdrs, &resourceInfoReq, reqHdr)) {
        ls_syslog(LOG_ERR, "%s: %s failed: %m", fname, "xdr_resourceInfoReq");
        limReplyCode = LIME_BAD_DATA;
        cc = MSGSIZE;
    } else {
        limReplyCode =
            checkResources(&resourceInfoReq, &resourceInfoReply, &cc);
        if (limReplyCode != LIME_NO_ERR)
            cc = MSGSIZE;
        else
            cc += 4 * MSGSIZE;
    }

    xdr_lsffree(xdr_resourceInfoReq, &resourceInfoReq, reqHdr);

    char *buf = calloc(cc, sizeof(char));

    xdrmem_create(&xdrs2, buf, cc, XDR_ENCODE);
    init_pack_hdr(&replyHdr);
    replyHdr.operation = (short) limReplyCode;
    replyHdr.sequence = reqHdr->sequence;
    if (limReplyCode == LIME_NO_ERR)
        replyStruct = (char *) &resourceInfoReply;
    else
        replyStruct = NULL;

    if (!xdr_encodeMsg(&xdrs2, replyStruct, &replyHdr, xdr_resourceInfoReply, 0,
                       NULL)) {
        ls_syslog(LOG_ERR, "%s: %s failed: %m", fname, "xdr_encodeMsg");
        FREEUP(buf);
        xdr_destroy(&xdrs2);
        freeResourceInfoReply(&resourceInfoReply);
        return;
    }

    cc = chanWrite_(s, buf, XDR_GETPOS(&xdrs2));
    if (cc < 0) {
        ls_syslog(LOG_ERR,
                  "%s: failed sending lsresources reply to %s len=%d: %m",
                  fname, sockAdd2Str_(from), XDR_GETPOS(&xdrs2));
        FREEUP(buf);
        xdr_destroy(&xdrs2);
        freeResourceInfoReply(&resourceInfoReply);
        return;
    }

    FREEUP(buf);
    xdr_destroy(&xdrs2);
    freeResourceInfoReply(&resourceInfoReply);
    return;
}

static int checkResources(struct resourceInfoReq *resourceInfoReq,
                          struct resourceInfoReply *reply, int *len)
{
    static char fname[] = "checkResources";
    int i, j, allResources = false, found = false;
    enum limReplyCode limReplyCode;
    char *host;

    if (resourceInfoReq->numResourceNames == 1 &&
        !strcmp(resourceInfoReq->resourceNames[0], "")) {
        allResources = true;
    }
    *len = 0;
    reply->numResources = 0;

    if (numHostResources == 0)
        return LIME_NO_RESOURCE;

    if (resourceInfoReq->hostName == NULL ||
        (resourceInfoReq->hostName && !strcmp(resourceInfoReq->hostName, " ")))
        host = NULL;
    else {
        if (find_node_by_cluster(myClusterPtr->hostList,
                                 resourceInfoReq->hostName) == NULL) {
            ls_syslog(LOG_ERR, "%s: Host <%s>  is not used by cluster <%s>",
                      fname, resourceInfoReq->hostName, myClusterName);
            return LIME_UNKWN_HOST;
        }
        host = resourceInfoReq->hostName;
    }

    reply->numResources = 0;
    if ((reply->resources = (struct lsSharedResourceInfo *) malloc(
             numHostResources * sizeof(struct lsSharedResourceInfo))) == NULL) {
        ls_syslog(LOG_ERR, "%s: %s failed: %m", fname, "malloc");
        return LIME_NO_MEM;
    }
    *len = numHostResources * sizeof(struct lsSharedResourceInfo) +
           sizeof(struct resourceInfoReply);

    for (i = 0; i < resourceInfoReq->numResourceNames; i++) {
        found = false;
        for (j = 0; j < numHostResources; j++) {
            if (allResources == false &&
                (strcmp(resourceInfoReq->resourceNames[i],
                        hostResources[j]->resourceName)))
                continue;
            found = true;
            if ((limReplyCode = copyResource(reply, hostResources[j], len,
                                             host)) != LIME_NO_ERR) {
                return limReplyCode;
            }
            reply->numResources++;
            if (allResources == false)
                break;
        }
        if (allResources == false && found == false) {
            return LIME_UNKWN_RNAME;
        }
        found = false;
        if (allResources == true)
            break;
    }
    return LIME_NO_ERR;
}

static int copyResource(struct resourceInfoReply *reply,
                        struct sharedResource *resource, int *len,
                        char *hostName)
{
    static char fname[] = "copyResource";
    int i, j, num, cc = 0, found = false, numInstances;

    num = reply->numResources;
    reply->resources[num].resourceName = resource->resourceName;
    cc += strlen(resource->resourceName) + 1;

    if ((reply->resources[num].instances = (struct lsSharedResourceInstance *)
             malloc(resource->numInstances *
                    sizeof(struct lsSharedResourceInstance))) == NULL) {
        ls_syslog(LOG_ERR, "%s: %s failed: %m", fname, "malloc");
        return LIME_NO_MEM;
    }
    cc += resource->numInstances * sizeof(struct lsSharedResourceInstance);
    reply->resources[num].nInstances = 0;
    numInstances = 0;
    reply->resources[num].instances[numInstances].nHosts = 0;

    for (i = 0; i < resource->numInstances; i++) {
        if (hostName) {
            for (j = 0; j < resource->instances[i]->nHosts; j++) {
                if (equal_host(hostName,
                               resource->instances[i]->hosts[j]->hostName)) {
                    found = true;
                    break;
                } else
                    continue;
            }
        }
        if (hostName && found == false)
            continue;

        found = false;
        reply->resources[num].instances[numInstances].value =
            resource->instances[i]->value;
        reply->resources[num].instances[numInstances].nHosts = 0;
        if ((reply->resources[num].instances[numInstances].hostList =
                 (char **) malloc(resource->instances[i]->nHosts *
                                  sizeof(char *))) == NULL) {
            ls_syslog(LOG_ERR, "%s: %s failed: %m", fname, "malloc");
            return LIME_NO_MEM;
        }
        cc += strlen(resource->instances[i]->value) + 1;
        for (j = 0; j < resource->instances[i]->nHosts; j++) {
            reply->resources[num].instances[numInstances].hostList[j] =
                resource->instances[i]->hosts[j]->hostName;
            cc += MAXHOSTNAMELEN;
        }
        reply->resources[num].instances[numInstances].nHosts =
            resource->instances[i]->nHosts;
        numInstances++;
    }
    reply->resources[num].nInstances = numInstances;
    *len += cc;
    return LIME_NO_ERR;
}

static void freeResourceInfoReply(struct resourceInfoReply *reply)
{
    int i, j;

    if (reply == NULL || reply->numResources <= 0 || reply->resources == NULL)
        return;
    for (i = 0; i < reply->numResources; i++) {
        for (j = 0; j < reply->resources[i].nInstances; j++)
            FREEUP(reply->resources[i].instances[j].hostList);
        FREEUP(reply->resources[i].instances);
    }
    FREEUP(reply->resources);
}
