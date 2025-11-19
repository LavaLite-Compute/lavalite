/* $Id: lim.load.c,v 1.6 2007/08/15 22:18:54 tmizan Exp $
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

enum loadstruct { e_vec, e_mat };

float exchIntvl = EXCHINTVL;
float sampleIntvl = SAMPLINTVL;
short hostInactivityLimit = HOSTINACTIVITYLIMIT;
short masterInactivityLimit = MASTERINACTIVITYLIMIT;
short resInactivityLimit = RESINACTIVITYLIMIT;
short retryLimit = RETRYLIMIT;
short keepTime = KEEPTIME;

time_t lastSbdActiveTime = 0;

char mustSendLoad = true;

extern int maxnLbHost;

static void rcvLoadVector(XDR *, struct sockaddr_in *, struct packet_header *);
static void logcnt(int);
static void copyResValues(struct loadVectorStruct, struct hostNode *);

void sendLoad(void)
{
    static char fname[] = "sendLoad";
    static int noSendCount = 0;
    struct loadVectorStruct myLoadVector;
    enum loadstruct loadType;
    struct hostNode *hPtr;
    int i, bufSize;
    enum limReqCode limReqCode;
    XDR xdrs;
    char *repBuf;
    int sendInfo = SEND_NO_INFO;
    struct packet_header reqHdr;

    limReqCode = LIM_LOAD_UPD;
    resInactivityCount++;

    if (resInactivityCount > resInactivityLimit) {
        myHostPtr->status[0] |= LIM_RESDOWN;
    }

    if (time(0) - lastSbdActiveTime > SBD_ACTIVE_TIME)
        myHostPtr->status[0] |= LIM_SBDDOWN;

    if (logclass & LC_TRACE)
        ls_syslog(LOG_DEBUG, "%s: Entering ..", fname);

    if (masterMe) {
        for (hPtr = myClusterPtr->hostList; hPtr; hPtr = hPtr->nextPtr) {
            if (hPtr != myHostPtr) {
                hPtr->hostInactivityCount++;
                if (hPtr->hostInactivityCount > 10000)
                    hPtr->hostInactivityCount = 100;

                if (!LS_ISUNAVAIL(hPtr->status)) {
                    if (hPtr->hostInactivityCount >
                        (hostInactivityLimit + retryLimit)) {
                        ls_syslog(
                            LOG_DEBUG,
                            "%s: Declaring %s unavailable inactivity Count=%d",
                            fname, hPtr->hostName, hPtr->hostInactivityCount);

                        hPtr->status[0] |= LIM_UNAVAIL;
                        hPtr->infoValid = false;
                        if (hPtr->numInstances > 0) {
                            int resNo;
                            for (i = 0; i < hPtr->numInstances; i++) {
                                if (hPtr->instances[i]->updHost == NULL ||
                                    hPtr->instances[i]->updHost != hPtr)
                                    continue;
                                resNo =
                                    resNameDefined(hPtr->instances[i]->resName);
                                if (allInfo.resTable[resNo].flags &
                                    RESF_DYNAMIC) {
                                    strcpy(hPtr->instances[i]->value, "-");
                                    hPtr->instances[i]->updHost = NULL;
                                }
                            }
                        }
                        hPtr->loadMask = 0;
                        hPtr->infoMask = 0;
                    }
                    if ((hPtr->hostInactivityCount > hostInactivityLimit) &&
                        (hPtr->hostInactivityCount <=
                         (hostInactivityLimit + retryLimit))) {
                        if (logclass & LC_COMM) {
                            ls_syslog(LOG_DEBUG3,
                                      "%s: Asking %s to send load info %d %d",
                                      fname, hPtr->hostName,
                                      hPtr->hostInactivityCount,
                                      hostInactivityLimit + retryLimit);
                        }
                        announceMasterToHost(hPtr, SEND_LOAD_INFO);
                    }
                }
            }
        }
    } else {
        myClusterPtr->masterInactivityCount++;

        if (logclass & LC_COMM) {
            ls_syslog(LOG_DEBUG3,
                      "%s: masterInactivityCount=%d, hostInactivityLimit=%d, "
                      "masterKnown=%d, retryLimit=%d",
                      fname, myClusterPtr->masterInactivityCount,
                      hostInactivityLimit, myClusterPtr->masterKnown,
                      retryLimit);
        }

        if (myClusterPtr->masterInactivityCount > hostInactivityLimit) {
            if (myClusterPtr->masterKnown &&
                (myClusterPtr->masterInactivityCount > hostInactivityLimit) &&
                (myClusterPtr->masterInactivityCount <=
                 hostInactivityLimit + retryLimit)) {
                if (logclass & LC_COMM) {
                    ls_syslog(LOG_DEBUG3, "%s: Attempting to probe master %s",
                              fname, myClusterPtr->masterPtr->hostName);
                }
                mustSendLoad = true;
                sendInfo = SEND_MASTER_ANN;
            }

            if (myClusterPtr->masterKnown &&
                (myClusterPtr->masterInactivityCount >
                 hostInactivityLimit + retryLimit)) {
                myClusterPtr->masterPtr->status[0] = LIM_UNAVAIL;
                myClusterPtr->masterKnown = false;
                myClusterPtr->prevMasterPtr = myClusterPtr->masterPtr;
                myClusterPtr->masterPtr = (struct hostNode *) NULL;
                ls_syslog(LOG_INFO, ("%s: Master LIM unknown now"), fname);
            }
            if (myClusterPtr->masterInactivityCount >
                hostInactivityLimit +
                    myHostPtr->hostNo * masterInactivityLimit) {
                if (probeMasterTcp(myClusterPtr) < 0) {
                    if (isMasterCandidate == true) {
                        initNewMaster();
                    } else {
                        ls_syslog(
                            LOG_WARNING,
                            "%s: All the master candidates seem not available.",
                            fname);
                    }

                    return;
                } else {
                    myClusterPtr->masterInactivityCount = 0;
                    myClusterPtr->masterKnown = true;
                    myClusterPtr->masterPtr = myClusterPtr->prevMasterPtr;
                }
            }
        }
        if (!myClusterPtr->masterKnown) {
            return;
        }
    }

    if (!mustSendLoad) {
        for (i = 0; i < allInfo.numIndx; i++) {
            if (fabs(myHostPtr->loadIndex[i] - li[i].valuesent) >
                li[i].exchthreshold) {
                mustSendLoad = true;
                break;
            }
        }
    }

    if (!mustSendLoad && noSendCount < hostInactivityLimit - 2) {
        noSendCount++;
        return;
    }

    for (i = 0; i < allInfo.numIndx; i++)
        li[i].valuesent = myHostPtr->loadIndex[i];

    if (!masterMe) {
        loadType = e_vec;
        myLoadVector.hostNo = myHostPtr->hostNo;
        myLoadVector.status = myHostPtr->status;
        myLoadVector.seqNo = loadVecSeqNo++;
        myLoadVector.checkSum = myClusterPtr->checkSum;
        myLoadVector.flags = sendInfo;
        myLoadVector.numIndx = allInfo.numIndx;
        myLoadVector.numUsrIndx = allInfo.numUsrIndx;
        myLoadVector.numResPairs = myHostPtr->numInstances;
        if (myLoadVector.numResPairs > 0) {
            if ((myLoadVector.resPairs = getResPairs(myHostPtr)) == NULL) {
                ls_syslog(LOG_ERR, "%s: %s failed: %m", fname, "getResPairs");
                return;
            }
        } else
            myLoadVector.resPairs = NULL;
        myLoadVector.li = myHostPtr->loadIndex;
        bufSize = sizeof(struct loadVectorStruct) +
                  allInfo.numIndx * sizeof(float) +
                  GET_INTNUM(allInfo.numIndx) * sizeof(int) +
                  myLoadVector.numResPairs * sizeof(struct resPair) + 100;
        for (i = 0; i < myLoadVector.numResPairs; i++) {
            bufSize +=
                ALIGNWORD_(
                    strlen(myLoadVector.resPairs[i].name) * sizeof(char) + 1) +
                4;
            bufSize +=
                ALIGNWORD_(
                    strlen(myLoadVector.resPairs[i].value) * sizeof(char) + 1) +
                4;
        }

        if (bufSize > MSGSIZE) {
            ls_syslog(LOG_ERR, "%s: message bigger then receive buf(%d", fname,
                      bufSize);

            return;
        }
        if ((repBuf = (char *) malloc(bufSize)) == NULL) {
            ls_syslog(LOG_ERR, "%s: %s failed: %m", fname, "malloc");
            return;
        }

        xdrmem_create(&xdrs, repBuf, bufSize, XDR_ENCODE);
        init_pack_hdr(&reqHdr);
        reqHdr.operation = (short) limReqCode;
        reqHdr.sequence = 0;

        if (!(xdr_pack_hdr(&xdrs, &reqHdr) &&
              xdr_enum(&xdrs, (int *) &loadType) &&
              xdr_loadvector(&xdrs, &myLoadVector, &reqHdr))) {
            ls_syslog(LOG_ERR, "%s: %s failed: %m", fname,
                      "xdr_enum/xdr_loadvector");
            xdr_destroy(&xdrs);
            FREEUP(repBuf);
            return;
        }

        struct sockaddr_in to_addr;
        to_addr.sin_family = AF_INET;
        to_addr.sin_port = lim_udp_port;

        get_host_addrv4(myClusterPtr->masterPtr->v4_epoint, &to_addr);

        logcnt(1);

        if (logclass & LC_COMM)
            ls_syslog(LOG_DEBUG, "sendLoad: sending to %s (len=%d,port=%d)",
                      sockAdd2Str_(&to_addr), XDR_GETPOS(&xdrs),
                      ntohs(lim_udp_port));

        if (chan_send_dgram(lim_udp_sock, repBuf, XDR_GETPOS(&xdrs), &to_addr) <
            0) {
            ls_syslog(LOG_ERR, "%s: chan_send_dgram() to %s failed: %m",
                      __func__, sockAdd2Str_(&to_addr));
            xdr_destroy(&xdrs);
            FREEUP(repBuf);
            return;
        }
        xdr_destroy(&xdrs);
        FREEUP(repBuf);
    }

    mustSendLoad = false;
    noSendCount = 0;
    return;
}

struct resPair *getResPairs(struct hostNode *hPtr)
{
    static char fname[] = "getResPairs()";
    int i;
    static struct resPair *resPairs = NULL;

    FREEUP(resPairs);

    if (hPtr->numInstances > 0) {
        if ((resPairs = (struct resPair *) malloc(
                 hPtr->numInstances * sizeof(struct resPair))) == NULL) {
            ls_syslog(LOG_ERR, "%s: %s failed: %m", fname, "malloc");
            return NULL;
        }
    }

    for (i = 0; i < hPtr->numInstances; i++) {
        resPairs[i].name = hPtr->instances[i]->resName;
        resPairs[i].value = hPtr->instances[i]->value;
    }

    return resPairs;
}

void rcvLoad(XDR *xdrs, struct sockaddr_in *from, struct packet_header *hdr)
{
    static char fname[] = "rcvLoad";
    enum loadstruct loadType;

    logcnt(0);

    if (from->sin_port != lim_udp_port) {
        ls_syslog(LOG_ERR, "%s: Update not from LIM: <%s>, expected %d", fname,
                  sockAdd2Str_(from), ntohs(lim_udp_port));
        return;
    }

    if (!xdr_enum(xdrs, (int *) &loadType)) {
        ls_syslog(LOG_ERR, "%s: %s failed: %m", fname, "xdr_enum");
        return;
    }

    if (loadType == e_vec)
        rcvLoadVector(xdrs, from, hdr);
    else {
        ls_syslog(LOG_ERR, "%s: Invalide load type %d from host <%s>", fname,
                  loadType, sockAdd2Str_(from));
        return;
    }
}

static void rcvLoadVector(XDR *xdrs, struct sockaddr_in *from,
                          struct packet_header *hdr)
{
    static char fname[] = "rcvLoadVector";
    static int checkSumMismatch;
    static struct loadVectorStruct *loadVector = NULL;
    struct hostNode *hPtr;
    int i;

    if (loadVector == NULL) {
        if ((loadVector = (struct loadVectorStruct *) malloc(
                 sizeof(struct loadVectorStruct))) == NULL) {
            ls_syslog(LOG_ERR, "%s: %s failed: %m", fname, "malloc");
            return;
        }
        if ((loadVector->li =
                 (float *) malloc(allInfo.numIndx * sizeof(float))) == NULL) {
            ls_syslog(LOG_ERR, "%s: %s failed: %m", fname, "malloc");
            return;
        }

        if ((loadVector->status = (int *) malloc(
                 (1 + GET_INTNUM(allInfo.numIndx)) * sizeof(int))) == NULL) {
            ls_syslog(LOG_ERR, "%s: %s failed: %m", fname, "malloc");
            return;
        }
    }

    if (!xdr_loadvector(xdrs, loadVector, hdr)) {
        ls_syslog(LOG_DEBUG, "%s: Error on xdr_loadvector from %s", fname,
                  sockAdd2Str_(from));
        return;
    }

    if (masterMe) {
        if ((myClusterPtr->checkSum != loadVector->checkSum) &&
            (checkSumMismatch < 5) &&
            (genParams[LSF_LIM_IGNORE_CHECKSUM].paramValue == NULL)) {
            ls_syslog(LOG_WARNING, "%s: Sender %s may have different config?.",
                      fname, sockAdd2Str_(from));
            checkSumMismatch++;
        }

        hPtr = find_node_by_sockaddr_in(from);
        if (hPtr == NULL) {
            return;
        }

        if (hPtr->infoValid != true) {
            return;
        }

        if (logclass & LC_COMM)
            ls_syslog(LOG_DEBUG, "%s: Received load update from host %s",
                      __func__, hPtr->hostName);

        hPtr->hostInactivityCount = 0;
        int masterLock = false;

        if (hPtr->status[0] & LIM_LOCKEDM) {
            masterLock = true;
        }
        hPtr->status[0] = loadVector->status[0];
        if (masterLock) {
            hPtr->status[0] |= LIM_LOCKEDM;
        } else {
            hPtr->status[0] &= ~LIM_LOCKEDM;
        }
        for (i = 0; i < GET_INTNUM(allInfo.numIndx); i++)
            hPtr->status[i + 1] = loadVector->status[i + 1];

        hPtr->loadMask = 0;

        if ((loadVector->seqNo - hPtr->lastSeqNo > 2) &&
            (loadVector->seqNo > hPtr->lastSeqNo) && (hPtr->lastSeqNo != 0))
            ls_syslog(
                LOG_ERR,
                "%s: host %s lastSeqNo=%d seqNo=%d. Packets being dropped?",
                fname, hPtr->hostName, hPtr->lastSeqNo, loadVector->seqNo);
        hPtr->lastSeqNo = loadVector->seqNo;

        copyResValues(*loadVector, hPtr);

        copyIndices(loadVector->li, loadVector->numIndx, loadVector->numUsrIndx,
                    hPtr);

        if (loadVector->flags & SEND_MASTER_ANN) {
            ls_syslog(LOG_INFO, ("%s: Sending master announce to %s"), fname,
                      hPtr->hostName);
            announceMasterToHost(hPtr, SEND_NO_INFO);
        }
    } else {
        ls_syslog(LOG_DEBUG, "%s: %s thinks I am the master, but I'm not",
                  fname, sockAdd2Str_(from));
        return;
    }
}

static void copyResValues(struct loadVectorStruct loadVector,
                          struct hostNode *hPtr)
{
    static char fname[] = "copyResValues";
    int i, j, updHostNo, curHostNo;
    struct sharedResource *resource;
    struct resourceInstance *instance;
    struct hostNode *hostPtr;
    char *temp;

    if (loadVector.numResPairs <= 0)
        return;

    for (i = 0; i < loadVector.numResPairs; i++) {
        if ((resource = inHostResourcs(loadVector.resPairs[i].name)) == NULL) {
            ls_syslog(LOG_DEBUG2,
                      "%s: Resource name <%s> reported by host <%s> is not in "
                      "shared resource list",
                      fname, loadVector.resPairs[i].name, hPtr->hostName);
            continue;
        }
        if ((instance = isInHostList(resource, hPtr->hostName)) == NULL) {
            ls_syslog(LOG_DEBUG2,
                      "%s: Host <%s> does not have the resource <%s> defined",
                      fname, hPtr->hostName, loadVector.resPairs[i].name);
            continue;
        }
        if (!strcmp(loadVector.resPairs[i].value, "-") &&
            !strcmp(instance->value, "-"))
            continue;
        if (instance->updHost == NULL)
            hostPtr = hPtr;
        else {
            updHostNo = -1;
            curHostNo = -1;
            for (j = 0; j < instance->nHosts; j++) {
                if (instance->updHost == instance->hosts[j])
                    updHostNo = j;
                if (instance->hosts[j] == hPtr)
                    curHostNo = j;
                if (curHostNo >= 0 && updHostNo >= 0)
                    break;
            }
            if (curHostNo < 0 || updHostNo < 0)
                continue;
            hostPtr = find_node_by_cluster(
                myClusterPtr->hostList, instance->hosts[curHostNo]->hostName);
            if (hostPtr == NULL)
                continue;
            if (updHostNo < curHostNo &&
                (!strcmp(loadVector.resPairs[i].value, "-") ||
                 strcmp(instance->value, "-")))

                continue;

            if (updHostNo > curHostNo &&
                !strcmp(loadVector.resPairs[i].value, "-"))

                continue;
            if (updHostNo == curHostNo && instance->value &&
                loadVector.resPairs[i].value &&
                !strcmp(loadVector.resPairs[i].value, instance->value))
                continue;
        }
        if ((temp = putstr_(loadVector.resPairs[i].value)) == NULL) {
            ls_syslog(LOG_ERR, "%s: %s failed: %m", fname, "malloc");
            return;
        }
        FREEUP(instance->value);
        instance->value = temp;
        instance->updHost = hostPtr;
    }
}

static void logcnt(int sendlog)
{
    static int sendcnt;
    static int recvcnt;
    static int first = true;
    static int sloglimit, rloglimit;
    char *sp;

    if (first) {
        first = false;
        if ((sp = getenv("SLIM_LOG")) == NULL)
            sloglimit = 0;
        else
            sloglimit = atoi(sp);
        if ((sp = getenv("RLIM_LOG")) == NULL)
            rloglimit = 0;
        else
            rloglimit = atoi(sp);
    }

    if (sendlog)
        sendcnt++;
    else
        recvcnt++;

    if (sloglimit == 0 && rloglimit == 0)
        return;

    if (sendcnt > sloglimit || recvcnt > rloglimit) {
        ls_syslog(LOG_INFO,
                  ("%s: sendcnt=%d recvcnt=%d sloglimit=%d rloglimit=%d"),
                  "logcnt", sendcnt, recvcnt, sloglimit, rloglimit);
        sendcnt = 0;
        recvcnt = 0;
        return;
    }
}

void copyIndices(float *lindx, int numIndx, int numUsrIndx,
                 struct hostNode *hPtr)
{
    int myBuiltIn, slaveBuiltIn, i;

    myBuiltIn = allInfo.numIndx - allInfo.numUsrIndx;
    slaveBuiltIn = numIndx - numUsrIndx;

    for (i = 0; (i < myBuiltIn) && (i < slaveBuiltIn); i++) {
        int nprocs = hPtr->statInfo.maxCpus;
        float cpuf =
            (hPtr->hModelNo >= 0) ? shortInfo.cpuFactors[hPtr->hModelNo] : 1.0;
        float rawql;

        if (i == R15S || i == R1M || i == R15M) {
            rawql = lindx[i];
            hPtr->loadIndex[i] = normalizeRq(rawql, cpuf, nprocs);
            hPtr->uloadIndex[i] = rawql;
        } else {
            hPtr->loadIndex[i] = lindx[i];
            hPtr->uloadIndex[i] = lindx[i];
        }
    }

    for (; i < myBuiltIn; i++) {
        hPtr->loadIndex[i] = INFINITY;
        hPtr->uloadIndex[i] = INFINITY;
    }

    for (i = 0; (i < numUsrIndx) && (i < allInfo.numUsrIndx); i++) {
        hPtr->loadIndex[myBuiltIn + i] = lindx[slaveBuiltIn + i];
        hPtr->uloadIndex[myBuiltIn + i] = lindx[slaveBuiltIn + i];
    }
}

float normalizeRq(float rawql, float cpuFactor, int nprocs)
{
    float nrq;
    float f, k1, k2;
    float slope;
    int ifloor;

    if (rawql < 0)
        return 0.0;

    if (rawql >= INFINITY)
        return rawql;

    if (nprocs >= 1) {
        ifloor = rawql / nprocs;
        k1 = (rawql - nprocs * ifloor) / nprocs;
        k2 = MIN(k1, 0.9);
        slope = 1.0 - 1.0 / nprocs;
        k2 = MIN(k2, slope);
        slope = MIN(slope, 0.9);
        f = slope * (-k1 + 1.0) / (1.0 - k2);
    } else {
        return ((rawql + 1) / cpuFactor);
    }

    nrq = (f * ifloor + (1.0 - f) * rawql / nprocs + 1) / cpuFactor;
    if (nrq < 0)
        return 0.0;

    return nrq;
}
