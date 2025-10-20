/* $Id: lim.internal.c,v 1.10 2007/08/15 22:18:53 tmizan Exp $
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 *
 */

#include "lsf/lim/lim.h"

#define NL_SETN     24

#define SLIMCONF_GRP_SIZE    (64)
#define SLIMCONF_GRP_MAXWAIT (2048)
#define SLIMCONF_MAXMSG_SIZE (8192)

#define  MAXMSGLEN     (1<<24)
#define LIM_CONNECT_TIMEOUT 5
#define LIM_RECV_TIMEOUT    20
#define LIM_RETRYLIMIT      2
#define LIM_RETRYINTVL      500

static int addSLimConfInXdr(struct hostNode *hPtr,
                            int flags,
                            XDR *xdrPtr,
                            int xdrSize,
                            struct LSFHeader *hdr);
static int getMinSLimConfUDP(XDR *xdrs);
static int getMinSLimConfTCP(struct hostNode *masterPtr, int limRetryCnt,
                             int limRetryIntvl);
static int packMinSLimConfData(struct minSLimConfData *sLimConfDatap,
                               struct  hostNode *hPtr);
static int unpackMinSLimConfData(struct minSLimConfData *sLimConfDatap);

static int getMinSLimConfDataSize(struct minSLimConfData *sLimConfDatap);

extern short  hostInactivityLimit;
extern int daemonId;

extern int callLim_(enum limReqCode, void *, bool_t (*)(), void *, bool_t (*)(), char *, int, struct LSFHeader *);

extern int getHdrReserved(struct LSFHeader *);
extern void setHdrReserved(struct LSFHeader *, unsigned int);




void
masterRegister(XDR *xdrs, struct sockaddr_in *from, struct LSFHeader *reqHdr)
{
    static char fname[]="masterRegister";
    static int checkSumMismatch;
    struct hostNode *hPtr;
    struct masterReg masterReg;
    int isFromLocalCluster;
    int xdrSLIMConfDone = false;
    struct masterAnnSLIMConf sLimConf;

    if (!limPortOk(from))
        return;
    if (!xdr_masterReg(xdrs, &masterReg, reqHdr)) {
        ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "xdr_masterReg");
        return;
    }


    if (masterReg.flags & SEND_MASTER_QUERY) {

        if (isMasterCandidate && myClusterPtr->masterKnown) {
            hPtr = findHostbyList(myClusterPtr->hostList, masterReg.hostName);
            if (hPtr == NULL) {
                ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5502,
                                                 "%s: Got master announcement from unused host %s; \
Run lim -C on this host to find more information"), /* catgets 5502 */
                          fname, masterReg.hostName);
                goto cleanup;
            }

            announceMasterToHost(hPtr, SEND_NO_INFO);
            goto cleanup;
        } else {
            goto cleanup;
        }
    }


    if (isMasterCandidate == false && !limConfReady) {

        if (getMasterCandidateNoByName_(masterReg.hostName) >= 0) {

            if (myClusterPtr->clName) {
                free(myClusterPtr->clName);
            }
            myClusterPtr->clName = putstr_(masterReg.clName);
            strcpy(myClusterName, masterReg.clName);

            isFromLocalCluster = true;
        } else {
            isFromLocalCluster = false;
        }
    } else {
        if (strcmp(myClusterPtr->clName, masterReg.clName) == 0) {
            isFromLocalCluster = true;
        } else {
            isFromLocalCluster = false;
        }
    }

    if (isFromLocalCluster) {

        if (limConfReady
            && (masterReg.checkSum != myClusterPtr->checkSum)
            && (checkSumMismatch < 2)
            && (limParams[LSF_LIM_IGNORE_CHECKSUM].paramValue
                == NULL) ) {
            ls_syslog(LOG_WARNING, _i18n_msg_get(ls_catd , NL_SETN, 5501,
                                                 "%s: Sender (%s) may have different config."), /* catgets 5501 */
                      fname, masterReg.hostName);
            checkSumMismatch++;
        }

        if (equalHost_(myHostPtr->hostName, masterReg.hostName))
            goto cleanup;

        hPtr = findHostbyList(myClusterPtr->hostList, masterReg.hostName);
        if (hPtr == NULL) {
            ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5502,
                                             "%s: Got master announcement from unused host %s; \
Run lim -C on this host to find more information"), /* catgets 5502 */
                      fname, masterReg.hostName);
            goto cleanup;
        }

        if (isMasterCandidate == false) {


            if (!(masterReg.flags & SLIM_XDR_DATA)) {
                ls_syslog(LOG_ERR, I18N(5529,
                                        "%s: expected slave-only LIM configuration data, sender %s may have different lsf.conf, flags=0x%x"), /* catgets 5529 */
                          fname, masterReg.hostName, masterReg.flags);
                goto cleanup;
            }


            if (!xdr_masterAnnSLIMConf(xdrs, &sLimConf, reqHdr)) {
                ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname,
                          "xdr_masterAnnSLIMConf");
            } else {

                myHostPtr->hostNo = sLimConf.hostNo;
                xdrSLIMConfDone = true;
            }
        }

        if (myClusterPtr->masterKnown && hPtr == myClusterPtr->masterPtr){

            myClusterPtr->masterInactivityCount = 0;

            if (masterReg.flags & SEND_ELIM_REQ) {
                int i;
                myHostPtr->callElim = true ;
                myHostPtr->maxResIndex = masterReg.maxResIndex;
                if (myHostPtr->resBitArray) {
                    myHostPtr->resBitArray = (int *)
                        realloc((void *)myHostPtr->resBitArray,
                                myHostPtr->maxResIndex * sizeof(int));
                } else {
                    myHostPtr->resBitArray = (int *)
                        calloc(myHostPtr->maxResIndex, sizeof(int));
                }
                if ( myHostPtr->resBitArray == NULL ) {
                    ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "malloc");
                    myHostPtr->maxResIndex = 0;
                    myHostPtr->callElim = false;
                }

                for ( i=0; i < myHostPtr->maxResIndex; i++){
                    myHostPtr->resBitArray[i] = masterReg.resBitArray[i];
                }
                FREEUP(masterReg.resBitArray);
            } else {
                myHostPtr->callElim = false ;
                myHostPtr->maxResIndex = 0;
            }


            if ((masterReg.seqNo - hPtr->lastSeqNo > 2)
                && (masterReg.seqNo > hPtr->lastSeqNo)
                && (hPtr->lastSeqNo != 0))
                ls_syslog(LOG_WARNING, _i18n_msg_get(ls_catd , NL_SETN, 5503,
                                                     "%s: master %s lastSeqNo=%d seqNo=%d. Packets dropped?"), /* catgets 5503 */
                          fname,
                          hPtr->hostName,
                          hPtr->lastSeqNo,
                          masterReg.seqNo);
            hPtr->lastSeqNo = masterReg.seqNo;


            hPtr->statInfo.portno = masterReg.portno;


            if (masterReg.flags & SEND_CONF_INFO) {
                sndConfInfo(from, LIM_STARTUP);
            } else {

                if (masterReg.flags & SEND_LIM_LOCKEDM) {
                    myHostPtr->status[0] |= LIM_LOCKEDM;
                } else {
                    myHostPtr->status[0] &= ~LIM_LOCKEDM;
                }
            }

            if (masterReg.flags & SEND_LOAD_INFO) {

                mustSendLoad = true;
                ls_syslog(LOG_DEBUG,"%s: Master lim is probing me. Send my load in next interval",fname);
            }
        } else {



            if ((myClusterPtr->masterKnown) &&
                (hPtr->hostNo < myHostPtr->hostNo) &&
                (myClusterPtr->masterPtr->hostNo < hPtr->hostNo) &&
                (myClusterPtr->masterInactivityCount <= hostInactivityLimit)){
                ls_syslog(LOG_INFO, (_i18n_msg_get(ls_catd , NL_SETN, 5516, "%s: Host <%s> is trying to take over from <%s>, not accepted")), /* catgets 5516 */
                          fname, masterReg.hostName, myClusterPtr->masterPtr->hostName);

                announceMasterToHost(hPtr, SEND_NO_INFO);
                goto cleanup;
            }


            if (hPtr->hostNo < myHostPtr->hostNo) {



                hPtr->statInfo.portno = masterReg.portno;

                if (isMasterCandidate == false && !limConfReady) {



                    if (xdrSLIMConfDone) {

                        if (sLimConf.flags & ANN_SLIMCONF_ALL_MIN) {

                            getMinSLimConfUDP(xdrs);

                        } else {

                            int msIntvl, grp_max_wait, grp_sz;
                            char *sp;


                            sp = getenv("SLIMCONF_GRP_MAXWAIT");
                            if (sp) {
                                grp_max_wait = atoi(sp);
                                if (grp_max_wait <= 0) {
                                    grp_max_wait = SLIMCONF_GRP_MAXWAIT;
                                }
                            } else {
                                grp_max_wait = SLIMCONF_GRP_MAXWAIT;
                            }

                            sp = getenv("SLIMCONF_GRP_SIZE");
                            if (sp) {
                                grp_sz = atoi(sp);
                                if (grp_sz <= 0) {
                                    grp_sz = SLIMCONF_GRP_SIZE;
                                }
                            } else {
                                grp_sz = SLIMCONF_GRP_SIZE;
                            }


                            if (sLimConf.flags & ANN_SLIMCONF_TO_ALL) {

                                msIntvl = ((sLimConf.hostNo
                                            - numMasterCandidates)
                                           / grp_sz)
                                    * grp_max_wait
                                    + rand() % grp_max_wait;
                            } else {
                                msIntvl = rand() % grp_max_wait;
                            }

                            if (logclass & LC_COMM) {
                                ls_syslog(LOG_DEBUG, "%s: SLIM wait %d ms to TCP MLIM on host %s", fname, msIntvl, hPtr->hostName);
                            }

                            millisleep_(msIntvl);

                            getMinSLimConfTCP(hPtr, 0, 0);
                        }
                    }

                    if (limConfReady) {

                        masterReg.flags |= SEND_CONF_INFO;
                    } else {

                        goto cleanup;
                    }
                }

                hPtr->protoVersion = reqHdr->version;
                myClusterPtr->prevMasterPtr = myClusterPtr->masterPtr;
                myClusterPtr->masterPtr   = hPtr;

                if (masterMe) {
                    ls_syslog(LOG_INFO, (_i18n_msg_get(ls_catd , NL_SETN, 5517, "%s: Give in master to <%s>")), /* catgets 5517 */
                              fname, masterReg.hostName);

                }
                daemonId                  = DAEMON_ID_LIM;
                masterMe                  = false;
                myClusterPtr->masterKnown = true;
                myClusterPtr->masterInactivityCount = 0;
                mustSendLoad = true;


                if (masterReg.flags & SEND_CONF_INFO) {
                    sndConfInfo(from, LIM_STARTUP);
                }
                if (masterReg.flags & SEND_LOAD_INFO) {

                    mustSendLoad = true;
                    ls_syslog(LOG_DEBUG,"%s: Master lim is probing me. Send my load in next interval",fname);
                }
            } else {

                if (myClusterPtr->masterKnown &&
                    myClusterPtr->masterInactivityCount < hostInactivityLimit){

                    announceMasterToHost(hPtr, SEND_NO_INFO);
                    ls_syslog(LOG_INFO, (_i18n_msg_get(ls_catd , NL_SETN, 5518,"\
%s: Host <%s> is trying to take over master LIM from %s, not accepted")), /* catgets 5518 */
                              fname, masterReg.hostName,
                              myClusterPtr->masterPtr->hostName);
                } else
                    ls_syslog(LOG_INFO, (_i18n_msg_get(ls_catd , NL_SETN, 5519,"%s: Host <%s> is trying to take over master LIM, not accepted")), fname, /* catgets 5519 */
                              masterReg.hostName);
            }
        }
    }

cleanup:
    FREEUP(masterReg.resBitArray);
    return;
}

static int
addSLimConfInXdr(struct hostNode *hPtr,
                 int flags,
                 XDR *xdrPtr,
                 int xdrSize,
                 struct LSFHeader *hdr)
{
    static char fname[]="addSLimConfInXdr";
    struct masterAnnSLIMConf sLimConf;
    struct minSLimConfData minSLimInfo;
    int    savePos, bufSize;
    int    sendConfData = true;
    int    maxMsgSize = 0;
    char   *sp;


    if (hPtr->hostNo < numMasterCandidates) {

        return 0;
    }

    if ((sp = (getenv("SLIMCONF_MAXMSG_SIZE"))) != NULL) {
        maxMsgSize = atoi(sp);
    }
    if (maxMsgSize <= 0) {
        maxMsgSize = SLIMCONF_MAXMSG_SIZE;
    }




    sLimConf.flags = 0;
    if (flags & ANN_SLIMCONF_TO_ALL) {
        sLimConf.flags |= ANN_SLIMCONF_TO_ALL;
    }
    sLimConf.hostNo = hPtr->hostNo;

    if (hPtr->infoValid == false
        && (flags & ANN_SLIMCONF_ALL_MIN)
        && masterMe) {

        if (packMinSLimConfData(&minSLimInfo, hPtr) < 0) {
            ls_syslog(LOG_DEBUG, "Error when pack kMinSLimConfData");
            sendConfData = false;
        } else {
            bufSize = getMinSLimConfDataSize(&minSLimInfo);
            bufSize = bufSize + XDR_GETPOS(xdrPtr);
            sendConfData = true;
        }

        if ((bufSize < maxMsgSize) && sendConfData) {

            savePos = XDR_GETPOS(xdrPtr);
            sLimConf.flags |= ANN_SLIMCONF_ALL_MIN;

            if (!(xdr_masterAnnSLIMConf(xdrPtr,  &sLimConf, hdr) &&
                  xdr_minSLimConfData(xdrPtr, &minSLimInfo, hdr))) {

                ls_syslog(LOG_DEBUG, I18N_FUNC_FAIL, fname,
                          "xdr_masterAnnSLIMConf/xdr_minSLimConfData");
            } else {
                bufSize = XDR_GETPOS(xdrPtr);
                if (bufSize < maxMsgSize) {
                    if (logclass & LC_COMM) {
                        ls_syslog(LOG_DEBUG, "%s: minSLimConfData is packed in LIM_MASTER_ANN, bufSize = %d",
                                  fname, bufSize);
                    }

                    return 0;
                }
            }

            sLimConf.flags &= ~ANN_SLIMCONF_ALL_MIN;
            XDR_SETPOS (xdrPtr, savePos);
        }
    }

    if (!xdr_masterAnnSLIMConf(xdrPtr, &sLimConf, hdr)) {
        ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname,
                  "xdr_masterAnnSLIMConf");
        return -1;
    }

    return 0;
}


static int
getMinSLimConfUDP(XDR *xdrs)
{
    static char fname[] = "getMinSLimConfUDP";
    struct minSLimConfData minSLimInfo;
    struct LSFHeader hdr;

    if (!xdr_minSLimConfData(xdrs, &minSLimInfo, &hdr)) {
        ls_syslog(LOG_ERR,  I18N_FUNC_FAIL, fname, "xdr_minSLimConfData");
        return -1;
    }

    if (unpackMinSLimConfData(&minSLimInfo) < 0) {
        ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "unpackMinSLimConfData");
        xdr_lsffree(xdr_minSLimConfData, (char *)&minSLimInfo, &hdr);
        return -1;
    }

    if (logclass & LC_COMM) {
        ls_syslog(LOG_DEBUG, "%s: minSLimConfData unpacked, and length = %d",
                  fname, XDR_GETPOS(xdrs));
    }

    xdr_lsffree(xdr_minSLimConfData, (char *)&minSLimInfo, &hdr);

    slaveOnlyPostConf(lim_CheckMode, &kernelPerm);

    return 0;
}

static int
getMinSLimConfDataSize(struct minSLimConfData *minSLimInfo)
{
    int    i, bufSize;
    struct sharedResourceInstance *tmp;

    bufSize = 128 + ALIGNWORD_(sizeof(minSLimInfo));
    bufSize += ALIGNWORD_(minSLimInfo->nClusAdmins * (sizeof(int)));

    for (i = 0; i < minSLimInfo->nClusAdmins; i++) {

        bufSize += ALIGNWORD_(strlen(minSLimInfo->clusAdminNames[i]) +
                              sizeof(int));
    }

    for (i = 0; i < (minSLimInfo->allInfo_nRes - NBUILTINDEX); i++) {

        bufSize += ALIGNWORD_(strlen(minSLimInfo->allInfo_resTable[i].name)
                              + sizeof(int));
        bufSize += ALIGNWORD_(sizeof(enum valueType));
        bufSize += ALIGNWORD_(sizeof(enum orderType));
        bufSize += ALIGNWORD_(sizeof(int));
        bufSize += ALIGNWORD_(sizeof(int));
    }

    bufSize += ALIGNWORD_(strlen(minSLimInfo->myCluster_eLimArgs) +
                          sizeof(int));
    bufSize += ALIGNWORD_(strlen(minSLimInfo->myHost_windows) + sizeof(int));

    for (i = 0; i < 8; i++) {
        bufSize += ALIGNWORD_(minSLimInfo->numMyhost_weekpair[i] *
                              sizeof(windows_t));
    }

    bufSize += ALIGNWORD_(minSLimInfo->allInfo_numIndx * sizeof(float));

    for (i = 0; i < minSLimInfo->myHost_numInstances; i++) {
        bufSize += ALIGNWORD_(strlen(minSLimInfo->myHost_instances[i]->resName)
                              + sizeof(int));
    }

    for (tmp = minSLimInfo->sharedResHead; tmp; tmp = tmp->nextPtr) {
        bufSize += ALIGNWORD_(strlen(tmp->resName) + sizeof(int));
    }

    return bufSize;

}

int
sendMinSLimConfTCP(XDR *xdrs, struct LSFHeader *hdr, int chfd)
{
    static char fname[] = "sendMinSLimConfTCP";
    int    pid = 0;
    int    bFork = false;
    int    rc = 0;
    int     len, bufSize;
    XDR    xdrs2;
    char   *buf;
    struct LSFHeader replyHdr;
    struct minSLimConfData minSLimInfo;
    struct statInfo;
    struct hostNode *hPtr;

    if ((hPtr = clientMap[chfd]->fromHost) == NULL) {
        ls_syslog(LOG_ERR, I18N(5571, "the fromHost is NULL")); /* catgets 5571 */
        return -1;
    }



    if (getenv("SLIMCONF_MLIM_FORK")) {
        bFork = true;
    }


    if (bFork) {
        pid = fork();
    } else {
        pid = 0;
        io_block_(chanSock_(chfd));
    }

    if (pid != 0) {

        if (pid < 0) {
            ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "fork");
            rc = -1;
        }
        return 0;
    }

    if (packMinSLimConfData(&minSLimInfo, hPtr) < 0) {
        ls_syslog(LOG_ERR,  I18N(5570, "error when pack MinSLimConfData")); /* catgets 5570 */
        rc = -1;
    } else {

        bufSize = getMinSLimConfDataSize(&minSLimInfo);

        bufSize = MAX(bufSize, MSGSIZE);

        buf = (char *)malloc(bufSize);

        if (!buf) {
            ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "malloc");
            rc = -1;
        } else {
            xdrmem_create(&xdrs2, buf, bufSize, XDR_ENCODE);
            initLSFHeader_(&replyHdr);
            replyHdr.opCode = LIME_NO_ERR;

            if (!xdr_encodeMsg(&xdrs2, (char *) &minSLimInfo,  &replyHdr,
                               xdr_minSLimConfData, 0, NULL)) {
                ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "xdr_encodeMsg");
                FREEUP (buf);
                xdr_destroy(&xdrs2);
                rc = -1;
            } else {
                len = XDR_GETPOS(&xdrs2);
                if ((chanWrite_(chfd, buf, len)) < 0) {
                    ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "chanWrite_");
                    rc = -1;
                }
                FREEUP (buf);
                xdr_destroy(&xdrs2);
                if (logclass & LC_COMM) {
                    ls_syslog(LOG_DEBUG, "%s: minSLimConfData is packed and sent via Blocking TCP. bufSize = %d",
                              fname, len);
                }
            }
        }
    }

    if (bFork) {
        exit (0);
    } else {
        return rc;
    }
}


static int
getMinSLimConfTCP(struct hostNode *masterPtr, int limRetryCnt,
                  int limRetryIntvl)
{
    static char fname[] = "getMinSLimConfTCP";
    struct minSLimConfData minSLimInfo;
    struct LSFHeader hdr;
    int    rc = 0;
    int    retryCnt = 1;

    if (limRetryCnt <= 0) {
        limRetryCnt = LIM_RETRYLIMIT;
    }
    if (limRetryIntvl <= 0) {
        limRetryIntvl = LIM_RETRYINTVL;
    }

    do {
        rc = callMasterTcp(LIM_SLIMCONF_REQ, masterPtr, NULL, NULL,
                           &minSLimInfo, xdr_minSLimConfData, 0, 0, &hdr);
        if (rc < 0) {
            ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "callMasterTcp");
            return -1;
        } else if (rc > 0) {
            ls_syslog(LOG_DEBUG, (_i18n_msg_get(ls_catd , NL_SETN, 5527,
                                                "Network Error %d")),  /* catgets 5527 */
                      rc);
            if (retryCnt <= limRetryCnt) {
                millisleep_(limRetryIntvl);
                retryCnt++;
            } else {
                ls_syslog(LOG_ERR, (_i18n_msg_get(ls_catd , NL_SETN, 5528,
                                                  "Network Error after %d retry[s]")),  /* catgets 5528 */
                          retryCnt);
                return -1;
            }
        }
    } while (rc);

    if (unpackMinSLimConfData(&minSLimInfo) < 0) {
        ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "unpackMinSLimConfData");
        xdr_lsffree(xdr_minSLimConfData, (char *)&minSLimInfo, &hdr);
        return -1;
    }

    if (logclass & LC_COMM) {
        ls_syslog(LOG_DEBUG, "%s: minSLimConfData unpacked OK", fname);
    }
    xdr_lsffree(xdr_minSLimConfData, (char *)&minSLimInfo, &hdr);

    slaveOnlyPostConf(lim_CheckMode, &kernelPerm);

    return 0;
}

static int
setupMasterRegBitArray(struct masterReg *tmasterReg, struct hostNode *hPtr)
{
    int i, size;

    size = tmasterReg->maxResIndex = hPtr->maxResIndex;
    tmasterReg->resBitArray = (int *)malloc(size*sizeof(int));
    if (!(tmasterReg->resBitArray)){
        return -1;
    }
    for (i=0; i < size; i++ ){
        tmasterReg->resBitArray[i] = hPtr->resBitArray[i];
    }

    return 0;
}

static void
announceElimInstance(struct clusterNode *clPtr)
{
    struct hostNode *hostPtr;
    struct sharedResourceInstance *tmp;
    int i, newMax, bitPos;


    for (tmp=sharedResourceHead, bitPos=0; tmp ; tmp=tmp->nextPtr, bitPos++) {
        for (i=0;i<tmp->nHosts;i++){
            hostPtr = tmp->hosts[i] ;
            if (hostPtr){
                if (hostPtr->infoValid){
                    hostPtr->callElim = true ;
                    newMax = GET_INTNUM(bitPos);
                    if (newMax+1 > hostPtr->maxResIndex)
                        hostPtr->maxResIndex = newMax+1;
                    SET_BIT(bitPos,hostPtr->resBitArray);
                    if (logclass & LC_ELIM)
                        ls_syslog(LOG_DEBUG,"setting %s->callElim ...",hostPtr->hostName);
                    break ;
                }
            }
        }
    }

}


void
announceMaster(struct clusterNode *clPtr, char broadcast, char all)
{
    static char      fname[] = "announceMaster";
    struct hostNode *hPtr;
    struct sockaddr_in toAddr;
    struct masterReg tmasterReg ;
    XDR    xdrs1;
    XDR    xdrs2;
    XDR    xdrs3;
    XDR    xdrs4;
    XDR    xdrs5;
    static char *buf1 = NULL;
    static char *buf2 = NULL;
    static char *buf3 = NULL;
    static char *buf4 = NULL;
    static char *buf5 = NULL;
    static int  cnt = 0;
    char   *sp;
    enum limReqCode limReqCode;
    struct masterReg masterReg;
    struct LSFHeader reqHdr;
    int announceInIntvl,numAnnounce,i;
    int periods;
    int maxMsgSize = 0;
    int savePos1, savePos2, savePos5;
    int limConfAnnFlags = ANN_SLIMCONF_ALL_MIN;



    if ((sp = (getenv("SLIMCONF_MAXMSG_SIZE"))) != NULL) {
        maxMsgSize = atoi(sp);
    }
    if (maxMsgSize <= 0) {
        maxMsgSize = SLIMCONF_MAXMSG_SIZE;
    }

    if (buf1 == NULL) {
        if ((buf1 = (char *)malloc(maxMsgSize * sizeof(char))) == NULL) {
            ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "malloc");
            return;
        }
    }
    if (buf2 == NULL) {
        if ((buf2 = (char *)malloc(maxMsgSize * sizeof(char))) == NULL) {
            ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "malloc");
            return;
        }
    }
    if (buf3 == NULL) {
        if ((buf3 = (char *)malloc(maxMsgSize * sizeof(char))) == NULL) {
            ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "malloc");
            return;
        }
    }
    if (buf4 == NULL) {
        if ((buf4 = (char *)malloc(maxMsgSize * sizeof(char))) == NULL) {
            ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "malloc");
            return;
        }
    }
    if (buf5 == NULL) {
        if ((buf5 = (char *)malloc(maxMsgSize * sizeof(char))) == NULL) {
            ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "malloc");
            return;
        }
    }

    announceElimInstance(clPtr) ;

    periods = (hostInactivityLimit - 1)*exchIntvl/sampleIntvl;
    if (!all && (++cnt > (periods - 1))) {
        cnt = 0;
        masterAnnSeqNo++;
    }

    limReqCode = LIM_MASTER_ANN;
    strcpy(masterReg.clName, myClusterPtr->clName);
    strcpy(masterReg.hostName, myClusterPtr->masterPtr->hostName);
    masterReg.seqNo    = masterAnnSeqNo;
    masterReg.checkSum = myClusterPtr->checkSum;
    masterReg.portno   = myClusterPtr->masterPtr->statInfo.portno;
    masterReg.maxResIndex = 0;

    toAddr.sin_family = AF_INET;
    toAddr.sin_port = lim_port;

    initLSFHeader_(&reqHdr);
    reqHdr.opCode  = (short) limReqCode;
    reqHdr.refCode = 0;


    xdrmem_create(&xdrs1, buf1, maxMsgSize, XDR_ENCODE);
    masterReg.flags = SEND_NO_INFO ;
    if (!(xdr_LSFHeader(&xdrs1, &reqHdr) &&
          xdr_masterReg(&xdrs1, &masterReg, &reqHdr))) {
        ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "xdr_LSFHeader/xdr_masterReg");
        xdr_destroy(&xdrs1);
        return;
    }


    xdrmem_create(&xdrs2, buf2, maxMsgSize, XDR_ENCODE);
    masterReg.flags = SEND_CONF_INFO;
    if (!(xdr_LSFHeader(&xdrs2, &reqHdr) &&
          xdr_masterReg(&xdrs2, &masterReg, &reqHdr))) {
        ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "xdr_enum/xdr_masterReg");
        xdr_destroy(&xdrs1);
        xdr_destroy(&xdrs2);
        return;
    }


    xdrmem_create(&xdrs5, buf5, maxMsgSize, XDR_ENCODE);
    masterReg.flags = SEND_NO_INFO | SEND_LIM_LOCKEDM;
    if (!(xdr_LSFHeader(&xdrs5, &reqHdr) &&
          xdr_masterReg(&xdrs5, &masterReg, &reqHdr))) {
        ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "xdr_LSFHeader/xdr_masterReg");
        xdr_destroy(&xdrs1);
        xdr_destroy(&xdrs2);
        xdr_destroy(&xdrs5);
        return;
    }


    if (clPtr->masterKnown && ! broadcast) {

        if (!getHostNodeIPAddr(clPtr->masterPtr,&toAddr)) {
            ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "getHostNodeIPAddr");
            xdr_destroy(&xdrs1);
            xdr_destroy(&xdrs2);
            xdr_destroy(&xdrs5);
            return;
        }


        if (logclass & LC_COMM)
            ls_syslog(LOG_DEBUG, "announceMaster: Sending request to LIM on %s: %m", sockAdd2Str_(&toAddr));

        if (chanSendDgram_(limSock, buf1, XDR_GETPOS(&xdrs1),
                           (struct sockaddr_in *)&toAddr) < 0)
            ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5506,
                                             "%s: Failed to send request to LIM on %s: %m"), /* catgets 5506 */
                      fname, sockAdd2Str_(&toAddr));
        xdr_destroy(&xdrs1);
        xdr_destroy(&xdrs2);
        xdr_destroy(&xdrs5);
        return;
    }


    if (all) {
        hPtr = clPtr->hostList;
        announceInIntvl = clPtr->numHosts;
        limConfAnnFlags |= ANN_SLIMCONF_TO_ALL;
    } else {

        announceInIntvl = clPtr->numHosts/periods;
        if (announceInIntvl == 0)
            announceInIntvl = 1;


        hPtr = clPtr->hostList;
        for(i=0; i < cnt*announceInIntvl; i++) {
            if (!hPtr)
                break;
            hPtr = hPtr->nextPtr;
        }


        if (cnt == 11)
            announceInIntvl = clPtr->numHosts;
    }
    savePos1 = XDR_GETPOS(&xdrs1);
    savePos2 = XDR_GETPOS(&xdrs2);
    savePos5 = XDR_GETPOS(&xdrs5);

    for (numAnnounce=0;
         hPtr && (numAnnounce < announceInIntvl);
         hPtr=hPtr->nextPtr,numAnnounce++) {

        if (hPtr == myHostPtr)
            continue;


        XDR_SETPOS(&xdrs1, savePos1);
        XDR_SETPOS(&xdrs2, savePos2);
        XDR_SETPOS(&xdrs5, savePos5);


        if (hPtr->hostNo >= numMasterCandidates) {

            xdr_destroy(&xdrs1);
            xdr_destroy(&xdrs2);
            xdr_destroy(&xdrs5);


            xdrmem_create(&xdrs1, buf1, maxMsgSize, XDR_ENCODE);

            masterReg.flags = SEND_NO_INFO | SLIM_XDR_DATA;

            if (!(xdr_LSFHeader(&xdrs1, &reqHdr)
                  && xdr_masterReg(&xdrs1, &masterReg, &reqHdr))) {
                ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "1 xdr_masterReg");
                xdr_destroy(&xdrs1);
                return;
            }


            xdrmem_create(&xdrs2, buf2, maxMsgSize, XDR_ENCODE);

            masterReg.flags = SEND_CONF_INFO | SLIM_XDR_DATA;

            if (!(xdr_LSFHeader(&xdrs2, &reqHdr)
                  && xdr_masterReg(&xdrs2, &masterReg, &reqHdr))) {
                ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "2 xdr_masterReg");
                xdr_destroy(&xdrs1);
                xdr_destroy(&xdrs2);
                return;
            }


            xdrmem_create(&xdrs5, buf5, maxMsgSize, XDR_ENCODE);

            masterReg.flags = SEND_NO_INFO | SLIM_XDR_DATA | SEND_LIM_LOCKEDM;

            if (!(xdr_LSFHeader(&xdrs5, &reqHdr) &&
                  xdr_masterReg(&xdrs5, &masterReg, &reqHdr))) {
                ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "5 xdr_masterReg");
                xdr_destroy(&xdrs1);
                xdr_destroy(&xdrs2);
                xdr_destroy(&xdrs5);
                return;
            }

            if (addSLimConfInXdr(hPtr, limConfAnnFlags,
                                 &xdrs1, sizeof(buf1), &reqHdr) < 0) {
                ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname,
                          "addSLimConfInXdr(xdrs1)");
                xdr_destroy(&xdrs1);
                xdr_destroy(&xdrs2);
                xdr_destroy(&xdrs5);
                return;
            }
            if (addSLimConfInXdr(hPtr, limConfAnnFlags,
                                 &xdrs2, sizeof(buf2), &reqHdr) < 0) {
                ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname,
                          "addSLimConfInXdr(xdrs2)");
                xdr_destroy(&xdrs1);
                xdr_destroy(&xdrs2);
                xdr_destroy(&xdrs5);
                return;
            }
            if (addSLimConfInXdr(hPtr, limConfAnnFlags,
                                 &xdrs5, sizeof(buf5), &reqHdr) < 0) {
                ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname,
                          "addSLimConfInXdr(xdrs5)");
                xdr_destroy(&xdrs1);
                xdr_destroy(&xdrs2);
                xdr_destroy(&xdrs5);
                return;
            }
        }

        if (!getHostNodeIPAddr(hPtr,&toAddr)) {
            ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "getHostNodeIPAddr");
            xdr_destroy(&xdrs1);
            xdr_destroy(&xdrs2);
            xdr_destroy(&xdrs5);
            return;
        }



        if (hPtr->infoValid == true) {
            if (logclass & LC_COMM)
                ls_syslog(LOG_DEBUG, "\
Send announce (normal) to %s %s, inactivityCount=%d",
                          hPtr->hostName, sockAdd2Str_(&toAddr),
                          hPtr->hostInactivityCount);


            if (hPtr->callElim){

                xdrmem_create(&xdrs4, buf4, maxMsgSize, XDR_ENCODE);

                memcpy((char *)&tmasterReg,(char *)&masterReg,
                       sizeof(struct masterReg));
                tmasterReg.flags = SEND_NO_INFO | SEND_ELIM_REQ;


                if (hPtr->hostNo >= numMasterCandidates) {
                    tmasterReg.flags =
                        SEND_NO_INFO | SEND_ELIM_REQ | SLIM_XDR_DATA;
                }


                if (hPtr->status[0] & LIM_LOCKEDM) {
                    tmasterReg.flags |=  SEND_LIM_LOCKEDM;
                }

                setupMasterRegBitArray(&tmasterReg, hPtr);

                if (!(xdr_LSFHeader(&xdrs4, &reqHdr) &&
                      xdr_masterReg(&xdrs4, &tmasterReg, &reqHdr)) ) {
                    ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname,
                              "xdr_LSFHeader/xdr_masterReg");
                    xdr_destroy(&xdrs1);
                    xdr_destroy(&xdrs2);
                    xdr_destroy(&xdrs4);
                    xdr_destroy(&xdrs5);
                    if (tmasterReg.maxResIndex > 0) {
                        FREEUP(tmasterReg.resBitArray);
                    }
                    return;
                }


                if (addSLimConfInXdr(hPtr, limConfAnnFlags, &xdrs4,
                                     sizeof(buf4), &reqHdr) < 0) {
                    ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname,
                              "addSLimConfInXdr(xdrs4)");
                    xdr_destroy(&xdrs1);
                    xdr_destroy(&xdrs2);
                    xdr_destroy(&xdrs3);
                    xdr_destroy(&xdrs4);
                    xdr_destroy(&xdrs5);
                    if (tmasterReg.maxResIndex > 0) {
                        FREEUP(tmasterReg.resBitArray);
                    }
                    return;
                }

                if (chanSendDgram_(limSock, buf4, XDR_GETPOS(&xdrs4),
                                   (struct sockaddr_in *)&toAddr) < 0)
                    ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5508,
                                                     "%s: Failed to send request 1 to LIM on %s: %k"), fname, hPtr->hostName); /* catgets 5508 */

                if (tmasterReg.maxResIndex > 0)
                    FREEUP(tmasterReg.resBitArray);
                tmasterReg.maxResIndex = 0;
                xdr_destroy(&xdrs4);

                hPtr->maxResIndex = 0;
                hPtr->callElim = false;

            } else {

                if (hPtr->status[0] & LIM_LOCKEDM) {
                    if (chanSendDgram_(limSock, buf5, XDR_GETPOS(&xdrs5),
                                       (struct sockaddr_in *)&toAddr) < 0) {
                        ls_syslog(LOG_ERR, I18N(5534,
                                                "%s: Failed to send request 1 to LIM on %s: %m"), /* catgets 5534 */
                                  fname, hPtr->hostName);
                    }
                } else {
                    if (chanSendDgram_(limSock, buf1, XDR_GETPOS(&xdrs1),
                                       (struct sockaddr_in *)&toAddr) < 0) {
                        ls_syslog(LOG_ERR, I18N(5535,
                                                "%s: Failed to send request 1 to LIM on %s: %m"), /* catgets 5535 */
                                  fname, hPtr->hostName);
                    }
                }
            }
        } else {
            if (logclass & LC_COMM)
                ls_syslog(LOG_DEBUG,"\
Send announce (SEND_CONF) to %s %s %x, inactivityCount=%d",
                          hPtr->hostName, sockAdd2Str_(&toAddr), hPtr->addr[0],
                          hPtr->hostInactivityCount);

            if (chanSendDgram_(limSock, buf2, XDR_GETPOS(&xdrs2),
                               (struct sockaddr_in *)&toAddr) < 0)
                ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5521,
                                                 "%s: Failed to send request 2 to LIM on %s: %m"), fname, hPtr->hostName);
        }
    }

    xdr_destroy(&xdrs1);
    xdr_destroy(&xdrs2);
    xdr_destroy(&xdrs5);

    return;

}


void
jobxferReq(XDR *xdrs, struct sockaddr_in *from, struct LSFHeader *reqHdr)
{
    static char fname[] = "jobxferReq()";
    struct hostNode *hPtr;
    struct jobXfer jobXfer;
    int i;

    if (!limPortOk(from))
        return;


    if (myClusterPtr->masterKnown && myClusterPtr->masterPtr &&
        equivHostAddr(myClusterPtr->masterPtr, *(u_int *)&from->sin_addr))
        myClusterPtr->masterInactivityCount = 0;

    if (!xdr_jobXfer(xdrs, &jobXfer, reqHdr)) {
        ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "xdr_jobXfer");
        return;
    }


    for (i = 0; i < jobXfer.numHosts; i++) {
        if ((hPtr = findHost(jobXfer.placeInfo[i].hostName)) != NULL) {
            hPtr->use = jobXfer.placeInfo[i].numtask;
            updExtraLoad(&hPtr, jobXfer.resReq, 1);
        } else {
            ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5510,
                                             "%s: %s not found in jobxferReq"), /* catgets 5510 */
                      fname,
                      jobXfer.placeInfo[i].hostName);
        }
    }

    return;

}

void
wrongMaster(struct sockaddr_in *from, char *buf, struct LSFHeader *reqHdr, int
            s)
{
    static char fname[] = "wrongMaster()";
    enum limReplyCode limReplyCode;


    XDR xdrs;
    struct LSFHeader replyHdr;
    struct masterInfo masterInfo;
    int cc;
    char *replyStruct;


    if (myClusterPtr->masterKnown) {
        limReplyCode = LIME_WRONG_MASTER;
        strcpy(masterInfo.hostName, myClusterPtr->masterPtr->hostName);
        masterInfo.addr = myClusterPtr->masterPtr->addr[0];
        masterInfo.portno = myClusterPtr->masterPtr->statInfo.portno;
        replyStruct = (char *)&masterInfo;
    } else  {
        limReplyCode = LIME_MASTER_UNKNW;
        replyStruct = (char *)NULL;
    }

    xdrmem_create(&xdrs, buf, MSGSIZE, XDR_ENCODE);
    initLSFHeader_(&replyHdr);
    replyHdr.opCode  = (short) limReplyCode;
    replyHdr.refCode = reqHdr->refCode;

    if (!xdr_encodeMsg(&xdrs, replyStruct, &replyHdr, xdr_masterInfo, 0, NULL)) {
        ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "xdr_encodeMsg");
        xdr_destroy(&xdrs);
        return;
    }

    if (logclass & LC_COMM)
        ls_syslog(LOG_DEBUG, "%s: Sending s=%d to %s",
                  fname,s, sockAdd2Str_(from));

    if (s < 0)
        cc = chanSendDgram_(limSock, buf, XDR_GETPOS(&xdrs), (struct sockaddr_in *)from);
    else
        cc = chanWrite_(s, buf, XDR_GETPOS(&xdrs));

    if (cc < 0) {
        ls_syslog(LOG_ERR, I18N_FUNC_S_FAIL_M, fname,
                  "chanSendDgram_/chanWrite_",
                  sockAdd2Str_(from));
        xdr_destroy(&xdrs);
        return;
    }

    xdr_destroy(&xdrs);
    return;

}


void
initNewMaster(void)
{
    static char fname[]="initNewMaster";
    struct hostNode *hPtr;
    int j;


    for (hPtr = myClusterPtr->hostList; hPtr; hPtr = hPtr->nextPtr) {
        if (hPtr != myHostPtr)  {
            hPtr->status[0] |= LIM_UNAVAIL;
            for (j = 0; j < GET_INTNUM(allInfo.numIndx); j++)
                hPtr->status[j+1] = 0;
            hPtr->hostInactivityCount = 0;
            hPtr->infoValid = false;
            hPtr->lastSeqNo = 0;
        }
    }
    masterAnnSeqNo = 0;

    mustSendLoad = true;
    myClusterPtr->masterKnown  = true;
    myClusterPtr->prevMasterPtr = myClusterPtr->masterPtr;
    myClusterPtr->masterPtr = myHostPtr;

    announceMaster(myClusterPtr, 1, true);
    myClusterPtr->masterInactivityCount = 0;

    daemonId = DAEMON_ID_MLIM;
    masterMe = true;

    ls_syslog(LOG_WARNING, _i18n_msg_get(ls_catd , NL_SETN, 5511,
                                         "%s: I am the master now."), fname); /* catgets 5511 */



    return;

}

void
rcvConfInfo(XDR *xdrs, struct sockaddr_in *from, struct LSFHeader *hdr)
{
    static char     fname[] = "rcvConfInfo()";
    struct statInfo sinfo;
    struct hostNode *hPtr;
    short  sinfoTypeNo, sinfoModelNo;
    int    limMode = -1;
    int    maxCpusInc = 0;

    if (!limPortOk(from))
        return;

    sinfo.maxCpus = 0;

    if (!masterMe) {
        ls_syslog(LOG_DEBUG, "rcvConfInfo: I am not the master!");
        return;
    }




    limMode = getHdrReserved(hdr);

    if (!xdr_statInfo(xdrs, &sinfo, hdr)) {
        ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "xdr_statInfo");
        return;
    }



    hPtr = findHostbyAddr(from, "rcvConfInfo");
    if (!hPtr)
        return;


    if (findHostbyList(myClusterPtr->hostList, hPtr->hostName) == NULL) {
        ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5513,
                                         "%s: Got info from client-only host %s/%s"), /* catgets 5513 */
                  fname, sockAdd2Str_(from), hPtr->hostName);
        return;
    }



    if (hPtr->hostNo < numMasterCandidates
        && hPtr->infoValid == true
        && limMode != LIM_RECONFIG ) {
        return;
    }

    if ((sinfo.maxCpus <= 0) || (sinfo.maxMem < 0)) {
        ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5514,
                                         "%s: Invalid info received: maxCpus=%d, maxMem=%d"), /* catgets 5514 */
                  fname, sinfo.maxCpus, sinfo.maxMem);
        return;
    }

    if (hPtr->infoValid == true && limMode == LIM_RECONFIG) {
        if (hPtr->statInfo.maxCpus > 0) {
            maxCpusInc = sinfo.maxCpus - hPtr->statInfo.maxCpus;
        } else {
            maxCpusInc = sinfo.maxCpus;
        }
    }

    hPtr->statInfo.maxCpus   = sinfo.maxCpus;
    hPtr->statInfo.maxMem    = sinfo.maxMem;
    hPtr->statInfo.maxSwap   = sinfo.maxSwap;
    hPtr->statInfo.maxTmp    = sinfo.maxTmp;
    hPtr->statInfo.nDisks    = sinfo.nDisks;
    hPtr->statInfo.portno    = sinfo.portno;
    sinfoTypeNo  = typeNameToNo(sinfo.hostType);
    if (sinfoTypeNo < 0) {
        ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd, NL_SETN, 5456,
                                         "%s: Unknown host type <%s>, using <DEFAULT>"), /* catgets 5456 */
                  fname, sinfo.hostType);
        sinfoTypeNo = 1;
    }

    if ( logclass & LC_TRACE) {
        ls_syslog(LOG_DEBUG2, "%s: host <%s> ncpu <%d> maxmem <%u> maxswp <%u> maxtmp <%u> ndisk <%d>",
                  fname, hPtr->hostName,
                  hPtr->statInfo.maxCpus,
                  hPtr->statInfo.maxMem,
                  hPtr->statInfo.maxSwap,
                  hPtr->statInfo.maxTmp,
                  hPtr->statInfo.nDisks);
    }

    if ( hPtr->hModelNo != DETECTMODELTYPE ) {

        sinfoModelNo = hPtr->hModelNo;
    } else {
        sinfoModelNo = archNameToNo(sinfo.hostArch);
        if (sinfoModelNo < 0) {
            ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd, NL_SETN, 5458,
                                             "%s: Unknown host architecture <%s>, using <DEFAULT>"), /* catgets 5458 */ fname, sinfo.hostArch);
            sinfoModelNo = 1;
        } else {
            if (strcmp(allInfo.hostArchs[sinfoModelNo], sinfo.hostArch) != 0) {
                if ( logclass & LC_EXEC )  {
                    ls_syslog(LOG_WARNING, _i18n_msg_get(ls_catd, NL_SETN, 5457,
                                                         "%s: Unknown host architecture <%s>, using best match <%s>, model <%s>"), /* catgets 5457 */
                              fname, sinfo.hostArch,
                              allInfo.hostArchs[sinfoModelNo],
                              allInfo.hostModels[sinfoModelNo]);
                }
            }

        }
    }


    if (hPtr->infoValid == false) {
        ++allInfo.modelRefs[sinfoModelNo];
    } else  {

        if ((hPtr->hModelNo != sinfoModelNo) && (hPtr->hModelNo != DETECTMODELTYPE)) {
            --allInfo.modelRefs[hPtr->hModelNo];
            ++allInfo.modelRefs[sinfoModelNo];
        }
    }


    if (hPtr->hTypeNo == DETECTMODELTYPE) {
        if ((hPtr->hTypeNo = sinfoTypeNo) < 0) {
            hPtr->hTypeNo = 0;
        } else {
            strcpy(hPtr->statInfo.hostType, sinfo.hostType);
            myClusterPtr->typeClass |= ( 1 << hPtr->hTypeNo);
            SET_BIT(hPtr->hTypeNo, myClusterPtr->hostTypeBitMaps);
        }
    }

    if (hPtr->hModelNo == DETECTMODELTYPE) {
        if ((hPtr->hModelNo = sinfoModelNo) < 0) {
            hPtr->hModelNo = 0;
        } else {
            strcpy(hPtr->statInfo.hostArch, sinfo.hostArch);
            myClusterPtr->modelClass |= ( 1 << hPtr->hModelNo);
            SET_BIT(hPtr->hModelNo, myClusterPtr->hostModelBitMaps);
        }
    }

    hPtr->protoVersion = hdr->version;
    hPtr->infoValid      = true;
    hPtr->infoMask       = 0;

    if (lim_debug >= 2)
        ls_syslog(LOG_DEBUG, "rcvConfInfo: Host %s: maxCpus=%d maxMem=%d "
                  "ndisks=%d typeNo=%d modelNo=%d",
                  hPtr->hostName, hPtr->statInfo.maxCpus, hPtr->statInfo.maxMem,
                  hPtr->statInfo.nDisks, (int)hPtr->hTypeNo, (int)hPtr->hModelNo);

    return;
}


void
sndConfInfo(struct sockaddr_in *to, int mode)
{
    static char fname[] = "sndConfInfo()";
    char   buf[MSGSIZE/4];
    XDR    xdrs;
    enum limReqCode limReqCode;
    struct LSFHeader reqHdr;

    memset((char*)&buf, 0, sizeof(buf));
    initLSFHeader_(&reqHdr);

    if (logclass & LC_COMM)
        ls_syslog(LOG_DEBUG, "%s: Sending info",fname);

    limReqCode = LIM_CONF_INFO;

    xdrmem_create(&xdrs, buf, MSGSIZE/4, XDR_ENCODE);
    reqHdr.opCode  = (short) limReqCode;
    reqHdr.refCode =  0;
    setHdrReserved(&reqHdr, mode);

    if (logclass & LC_TRACE) {
        ls_syslog(LOG_DEBUG2, "%s: host <%s> ncpu <%d> maxmem <%d> maxswp <%u> maxtmp <%u> ndisk <%d>",
                  fname, myHostPtr->hostName,
                  myHostPtr->statInfo.maxCpus,
                  myHostPtr->statInfo.maxMem,
                  myHostPtr->statInfo.maxSwap,
                  myHostPtr->statInfo.maxTmp,
                  myHostPtr->statInfo.nDisks);
    }

    if ( !(xdr_LSFHeader(&xdrs, &reqHdr) &&
           xdr_statInfo(&xdrs, &myHostPtr->statInfo, &reqHdr)) ) {
        ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "xdr_LSFHeader/xdr_statInfo");
        return;
    }

    if (logclass & LC_COMM)
        ls_syslog(LOG_DEBUG, "%s: chanSendDgram_ info to %s",
                  fname,sockAdd2Str_(to));

    if ( chanSendDgram_(limSock, buf, XDR_GETPOS(&xdrs),
                        (struct sockaddr_in *)to) < 0) {
        ls_syslog(LOG_ERR, I18N_FUNC_S_FAIL_M, fname, "chanSendDgram_",
                  sockAdd2Str_(to));
        return;
    }

    xdr_destroy(&xdrs);

    return;
}

void
checkHostWd (void)
{
    struct dayhour dayhour;
    windows_t *wp;
    char   active;
    time_t now = time(0);

    if (myHostPtr->wind_edge > now || myHostPtr->wind_edge == 0)
        return;
    getDayHour (&dayhour, now);
    if (myHostPtr->week[dayhour.day] == NULL) {
        myHostPtr->status[0] |= LIM_LOCKEDW;
        myHostPtr->wind_edge = now + (24.0 - dayhour.hour) * 3600.0;
        return;
    }
    active = false;
    myHostPtr->wind_edge = now + (24.0 - dayhour.hour) * 3600.0;
    for (wp = myHostPtr->week[dayhour.day]; wp; wp=wp->nextwind)
        checkWindow(&dayhour, &active, &myHostPtr->wind_edge, wp, now);
    if (!active)
        myHostPtr->status[0] |= LIM_LOCKEDW;
    else
        myHostPtr->status[0] &= ~LIM_LOCKEDW;

}

void announceMasterToHost(struct hostNode *hPtr, int infoType )
{
    static char fname[] = "announceMasterToHost()";
    struct sockaddr_in toAddr;
    XDR    xdrs;
    static char *buf = NULL;
    char   *sp;
    int    maxMsgSize = 0;
    enum limReqCode limReqCode;
    struct masterReg masterReg;
    struct LSFHeader reqHdr;
    char sockAddStr[MAXHOSTNAMELEN];

    if ((sp = (getenv("SLIMCONF_MAXMSG_SIZE"))) != NULL) {
        maxMsgSize = atoi(sp);
    }
    if (maxMsgSize <= 0) {
        maxMsgSize = SLIMCONF_MAXMSG_SIZE;
    }

    if (buf == NULL) {
        if ((buf = (char *)malloc(maxMsgSize * sizeof(char))) == NULL) {
            ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "malloc");
            return;
        }
    }

    limReqCode = LIM_MASTER_ANN;
    masterReg.flags = infoType;
    if (!(infoType & SEND_MASTER_QUERY)) {
        strcpy(masterReg.clName, myClusterPtr->clName);
        strcpy(masterReg.hostName, myClusterPtr->masterPtr->hostName);
        masterReg.seqNo    = masterAnnSeqNo;
        masterReg.checkSum = myClusterPtr->checkSum;
        masterReg.portno   = myClusterPtr->masterPtr->statInfo.portno;
        masterReg.maxResIndex = 0;
    } else {

        masterReg.clName[0] = 0;
        strcpy(masterReg.hostName, myHostPtr->hostName);
        masterReg.maxResIndex = 0;
    }

    toAddr.sin_family = AF_INET;
    toAddr.sin_port = lim_port;

    xdrmem_create(&xdrs, buf, maxMsgSize, XDR_ENCODE);
    initLSFHeader_(&reqHdr);
    reqHdr.opCode  = (short) limReqCode;
    reqHdr.refCode =  0;

    if (!(xdr_LSFHeader(&xdrs,  &reqHdr) &&
          xdr_masterReg(&xdrs, &masterReg, &reqHdr))) {
        ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "xdr_LSFHeader/xdr_masterReg");
        xdr_destroy(&xdrs);
        return;
    }

    if (!(infoType & SEND_MASTER_QUERY)) {
        int limConfAnnFlags = ANN_SLIMCONF_ALL_MIN;


        if (hPtr->hostNo >= numMasterCandidates) {


            xdr_destroy(&xdrs);


            xdrmem_create(&xdrs, buf, maxMsgSize, XDR_ENCODE);


            masterReg.flags |=  SLIM_XDR_DATA;

            if (!(xdr_LSFHeader(&xdrs,  &reqHdr)
                  && xdr_masterReg(&xdrs, &masterReg, &reqHdr))) {
                ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "xdr_LSFHeader");
                xdr_destroy(&xdrs);
                return;
            }
        }


        if (addSLimConfInXdr(hPtr, limConfAnnFlags,
                             &xdrs, sizeof(buf), &reqHdr) < 0) {
            ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname,
                      "addSLimConfInXdr(xdrs)");
            xdr_destroy(&xdrs);
            return;
        }
    }

    if (!getHostNodeIPAddr(hPtr,&toAddr)) {
        ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "getHostNodeIPAddr");
        xdr_destroy(&xdrs);
        return;
    }

    if (logclass & LC_COMM)
        ls_syslog(LOG_DEBUG,
                  "announceMasterToHost: Sending request to LIM on %s",
                  sockAdd2Str_(&toAddr));

    strcpy(sockAddStr, sockAdd2Str_(&toAddr));
    if (chanSendDgram_(limSock, buf, XDR_GETPOS(&xdrs),
                       (struct sockaddr_in *)&toAddr) < 0) {
        ls_syslog(LOG_ERR, I18N_FUNC_S_FAIL_MN, fname, "chanSendDgram_",
                  sockAddStr);
    }
    xdr_destroy(&xdrs);
}

int
probeMasterTcp(struct clusterNode *clPtr)
{
    static char fname[] = "probeMasterTcp";
    struct hostNode *hPtr;
    struct sockaddr_in mlim_addr;
    int ch, rc;
    struct LSFHeader reqHdr;


    ls_syslog (LOG_DEBUG, "probeMasterTcp: enter.... ");
    hPtr = clPtr->masterPtr;
    if (!hPtr)
        hPtr = clPtr->prevMasterPtr;
    ls_syslog (LOG_ERR, I18N(5532,"probeMasterTcp: Last master is  UNKNOWN"));/*catgets 5532*/

    if (!hPtr)
        return -1;
    if (hPtr == myHostPtr)
        return -1;

    ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5515,
                                     "%s: Attempting to probe last known master %s port %d timeout is %d"), /* catgets 5515 */
              fname, hPtr->hostName,ntohs(hPtr->statInfo.portno),
              probeTimeout);


    memset((char*)&mlim_addr, 0, sizeof(mlim_addr));
    mlim_addr.sin_family      = AF_INET;

    if (!getHostNodeIPAddr(hPtr,&mlim_addr)) {
        ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "getHostNodeIPAddr");
        return -1;
    }
    mlim_addr.sin_port        = hPtr->statInfo.portno;

    ch= chanClientSocket_(AF_INET, SOCK_STREAM, 0);
    if (ch < 0 ) {
        ls_syslog(LOG_ERR, I18N_FUNC_FAIL_MM, fname, "chanClientSocket_");
        return -2;
    }

    rc = chanConnect_(ch, &mlim_addr, probeTimeout * 1000, 0);
    if (rc < 0) {
        ls_syslog(LOG_ERR, I18N_FUNC_S_FAIL_MM, fname, "chanConnect_",
                  sockAdd2Str_(&mlim_addr));
        chanClose_(ch);
    } else
        ls_syslog(LOG_ERR, "%s: %s(%s) %s ",
                  fname,
                  "chanConnect_",
                  sockAdd2Str_(&mlim_addr),
                  I18N_OK);



    if ( rc >= 0) {
        initLSFHeader_(&reqHdr);
        reqHdr.opCode = LIM_PING;
        writeEncodeHdr_(ch, &reqHdr, chanWrite_ );
        chanClose_(ch);
    }

    return rc;
}

int
callMasterTcp(enum limReqCode ReqCode, struct hostNode *masterPtr,
              void *reqBuf, bool_t (*xdr_sfunc)(),
              void *replyBuf, bool_t (*xdr_rfunc)(), int rcvTimeout,
              int connTimeout, struct LSFHeader *hdr)
{
    static char fname[] = "callMasterTcp";
    XDR    xdrs;
    char   sbuf[4*MSGSIZE];
    char   replyHdrBuf[LSF_HEADER_LEN];
    char   *tmpBuf;
    enum limReplyCode ReplyCode;
    struct hostNode *hPtr;
    struct LSFHeader reqHdr;
    struct LSFHeader replyHdr;
    struct sockaddr_in mlim_addr;
    struct in_addr *tmp_addr = NULL;
    struct timeval timeval;
    struct timeval *timep = NULL;
    int    reqLen;
    int    ch, rc;

    if (logclass & (LC_TRACE | LC_COMM)) {
        ls_syslog (LOG_DEBUG, "callMasterTcp: enter.... ");
    }

    if (myClusterPtr->masterPtr != NULL) {
        if ((masterPtr != NULL) && (myClusterPtr->masterPtr != masterPtr)) {
            ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5572,
                                             "%s: Bad Master Information passed."),  /* catgets 5572 */
                      fname);
            return -1;
        }
        hPtr = myClusterPtr->masterPtr;
    } else {
        if (masterPtr == NULL) {
            ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5573,
                                             "%s: No Master Information."),  /* catgets 5573 */
                      fname);
            return -1;
        }
        hPtr = masterPtr;
    }

    if (hPtr == myHostPtr) {
        return -1;
    }

    memset((char*)&mlim_addr, 0, sizeof(mlim_addr));
    mlim_addr.sin_family = AF_INET;

    if (!(tmp_addr = (struct in_addr *)getHostFirstAddr_(hPtr->hostName))) {
        ls_syslog (LOG_ERR, I18N_FUNC_FAIL, fname, "getHostFirstAddr_");
        return -1;
    }

    memcpy((void *) &mlim_addr.sin_addr, (const void*)tmp_addr, sizeof(u_int));
    mlim_addr.sin_port = hPtr->statInfo.portno;

    ch = chanClientSocket_(AF_INET, SOCK_STREAM, 0);
    if (ch < 0 ) {
        ls_syslog(LOG_ERR, I18N_FUNC_FAIL_MM, fname, "chanClientSocket_");
        return 1;
    }

    if (connTimeout <= 0) {
        connTimeout = LIM_CONNECT_TIMEOUT;
    }

    rc = chanConnect_(ch, &mlim_addr, connTimeout * 1000, 0);
    if (rc < 0) {
        ls_syslog(LOG_ERR, I18N_FUNC_S_FAIL_MM, fname, "chanConnect_",
                  sockAdd2Str_(&mlim_addr));
        chanClose_(ch);
        return 1;
    }

    initLSFHeader_(&reqHdr);
    reqHdr.opCode = ReqCode;
    xdrmem_create(&xdrs, sbuf, 4*MSGSIZE, XDR_ENCODE);

    if (!xdr_encodeMsg(&xdrs, reqBuf, &reqHdr, xdr_sfunc, 0, NULL)) {
        xdr_destroy(&xdrs);
        lserrno = LIME_BAD_DATA;
        chanClose_(ch);
        return 2;
    }

    reqLen = XDR_GETPOS(&xdrs);
    if (chanWrite_ (ch, sbuf, reqLen) != reqLen) {
        xdr_destroy(&xdrs);
        chanClose_(ch);
        return 2;
    }

    xdr_destroy(&xdrs);
    if (!replyBuf && !hdr) {
        chanClose_(ch);
        return 0;
    }

    if (rcvTimeout <= 0) {
        timeval.tv_sec = LIM_RECV_TIMEOUT;
    } else {
        timeval.tv_sec = rcvTimeout;
    }

    timeval.tv_usec = 0;
    timep = &timeval;

    if ((rc=rd_select_(chanSock_(ch), timep)) <= 0) {
        ls_syslog(LOG_ERR, I18N_FUNC_FAIL_M, fname, "rd_select_");
        chanClose_(ch);
        return 3;
    }

    xdrmem_create(&xdrs, replyHdrBuf, LSF_HEADER_LEN , XDR_DECODE);
    rc = readDecodeHdr_(ch, replyHdrBuf, chanRead_, &xdrs, &replyHdr);
    if (rc < 0) {
        xdr_destroy(&xdrs);
        chanClose_(ch);
        return 3;
    }

    xdr_destroy(&xdrs);

    if (replyHdr.length > MAXMSGLEN) {
        chanClose_(ch);
        return 3;
    }

    if (replyHdr.length > 0) {
        if ((tmpBuf = (char *) malloc(replyHdr.length)) == NULL) {
            chanClose_(ch);
            return 3;
        }

        if ((rc = chanRead_(ch, tmpBuf, replyHdr.length)) !=
            replyHdr.length) {
            FREEUP(tmpBuf);
            chanClose_(ch);
            ls_syslog(LOG_DEBUG2,"%s: read only %d bytes", fname,ch);
            return 3;
        }

        xdrmem_create(&xdrs, tmpBuf, replyHdr.length, XDR_DECODE);
        ReplyCode = replyHdr.opCode;

        switch(ReplyCode) {
        case LIME_NO_ERR:
            if (!(*xdr_rfunc)(&xdrs, replyBuf, &replyHdr)) {
                ls_syslog(LOG_ERR,I18N(5533,"xdr fail"));  /* catgets 5533 */
                xdr_destroy(&xdrs);
                FREEUP(tmpBuf);
                chanClose_(ch);
                return 3;
            }
            break;
        default:
            xdr_destroy(&xdrs);
            FREEUP(tmpBuf);
            chanClose_(ch);
            return 3;
        }
        xdr_destroy(&xdrs);
        FREEUP(tmpBuf);
    } else {
        replyBuf = NULL;
    }

    if (logclass & LC_COMM) {
        ls_syslog(LOG_DEBUG, "%s: Reply length : %d", fname, replyHdr.length);
    }

    chanClose_(ch);

    if (hdr != NULL) {
        memcpy (hdr, &replyHdr, sizeof(replyHdr));
    }

    return 0;
}


static int
packMinSLimConfData(struct minSLimConfData *sLimConfDatap, struct  hostNode *hPtr)
{
    int i, j;
    windows_t * windowsPtr;

    if ( sLimConfDatap == NULL || hPtr == NULL )  {
        return -1;
    }
    sLimConfDatap->defaultRunElim = defaultRunElim;
    sLimConfDatap->nClusAdmins = nClusAdmins;
    sLimConfDatap->clusAdminIds = clusAdminIds;
    sLimConfDatap->clusAdminNames = clusAdminNames;
    sLimConfDatap->exchIntvl = exchIntvl;
    sLimConfDatap->sampleIntvl = sampleIntvl;
    sLimConfDatap->hostInactivityLimit = hostInactivityLimit;
    sLimConfDatap->masterInactivityLimit = masterInactivityLimit;
    sLimConfDatap->retryLimit = retryLimit;
    sLimConfDatap->keepTime = keepTime;
    sLimConfDatap->allInfo_resTable = allInfo.resTable + NBUILTINDEX;
    sLimConfDatap->allInfo_nRes = allInfo.nRes;
    sLimConfDatap->allInfo_numIndx = allInfo.numIndx;
    sLimConfDatap->allInfo_numUsrIndx = allInfo.numUsrIndx;
    sLimConfDatap->myCluster_checkSum = myClusterPtr->checkSum;

    if (myClusterPtr->eLimArgs == NULL ) {
        sLimConfDatap->myCluster_eLimArgs = "";
    } else {
        sLimConfDatap->myCluster_eLimArgs = myClusterPtr->eLimArgs;
    }

    if (hPtr->windows == NULL ) {
        sLimConfDatap->myHost_windows = "";
    } else {
        sLimConfDatap->myHost_windows = hPtr->windows;
    }

    for (i = 0;i < 8; i++) {
        sLimConfDatap->myHost_week[i] = hPtr->week[i];
        for (j = 0, windowsPtr = hPtr->week[i]; windowsPtr != NULL;
             windowsPtr = windowsPtr->nextwind, j++);
        sLimConfDatap->numMyhost_weekpair[i] = j;
    }
    sLimConfDatap->myHost_wind_edge = hPtr->wind_edge;
    sLimConfDatap->myHost_busyThreshold = hPtr->busyThreshold;
    sLimConfDatap->myHost_rexPriority = hPtr->rexPriority;


    sLimConfDatap->myHost_numInstances = hPtr->numInstances;
    sLimConfDatap->myHost_instances = hPtr->instances;
    sLimConfDatap->sharedResHead = sharedResourceHead;

    return 0;
}


static int
unpackMinSLimConfData(struct minSLimConfData *sLimConfDatap)
{
    static char fname[] = "unpackMinSLimConfData";
    int i, j;
    windows_t * windowsPtr;
    struct resItem *sourceResPtr, *destResPtr;
    struct sharedResourceInstance *tmp, *tmp1, *prevPtr;

    defaultRunElim = sLimConfDatap->defaultRunElim;
    nClusAdmins = sLimConfDatap->nClusAdmins;
    if ( nClusAdmins < 1 ) {
        return -1;
    }
    clusAdminIds = (int *)realloc(clusAdminIds, nClusAdmins*sizeof(int));
    if (clusAdminNames) {
        FREEUP(clusAdminNames[0]);
    }
    clusAdminNames = (char **)realloc(clusAdminNames,
                                      nClusAdmins*sizeof(char *));

    if (!clusAdminIds || !clusAdminNames) {
        sLimConfDatap->nClusAdmins = 0;
        return -1;
    }

    for (i = 0; i < nClusAdmins; i++) {
        clusAdminIds[i] = sLimConfDatap->clusAdminIds[i];
        clusAdminNames[i] = putstr_(sLimConfDatap->clusAdminNames[i]);
    }

    exchIntvl = sLimConfDatap->exchIntvl;
    sampleIntvl = sLimConfDatap->sampleIntvl;
    hostInactivityLimit = sLimConfDatap->hostInactivityLimit;
    masterInactivityLimit = sLimConfDatap->masterInactivityLimit;
    retryLimit = sLimConfDatap->retryLimit;
    keepTime = sLimConfDatap->keepTime;
    allInfo.nRes = sLimConfDatap->allInfo_nRes;

    allInfo.resTable = (struct resItem *)realloc(allInfo.resTable,
                                                 allInfo.nRes*sizeof(struct resItem));
    if (!allInfo.resTable)
    {
        ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "realloc");
        return -1;
    }
    destResPtr = allInfo.resTable + NBUILTINDEX;
    sourceResPtr = sLimConfDatap->allInfo_resTable;
    for (i = 0; i < allInfo.nRes-NBUILTINDEX; i++) {
        memcpy(destResPtr->name, sourceResPtr->name, MAXLSFNAMELEN);
        destResPtr->valueType = sourceResPtr->valueType;
        destResPtr->orderType = sourceResPtr->orderType;
        destResPtr->flags = sourceResPtr->flags;
        destResPtr->interval = sourceResPtr->interval;
        destResPtr++;
        sourceResPtr++;
    }

    allInfo.numIndx = sLimConfDatap->allInfo_numIndx;
    allInfo.numUsrIndx = sLimConfDatap->allInfo_numUsrIndx;

    myClusterPtr->checkSum = sLimConfDatap->myCluster_checkSum;

    if (sLimConfDatap->myCluster_eLimArgs == NULL) {
        return -1;
    }
    if (strcmp(sLimConfDatap->myCluster_eLimArgs, "") == 0) {
        myClusterPtr->eLimArgs = NULL;
    } else {
        myClusterPtr->eLimArgs = putstr_(sLimConfDatap->myCluster_eLimArgs);
    }

    if (sLimConfDatap->myHost_windows == NULL ) {
        return -1;
    }
    if (strcmp(sLimConfDatap->myHost_windows, "")  == 0) {
        myHostPtr->windows = NULL;
    } else {
        myHostPtr->windows = putstr_(sLimConfDatap->myHost_windows);
    }

    myHostPtr->wind_edge = sLimConfDatap->myHost_wind_edge;

    for (i = 0; i < 8; i++) {
        for (j = 0, windowsPtr = sLimConfDatap->myHost_week[i]; j < sLimConfDatap->numMyhost_weekpair[i]; j++) {
            insertW(&(myHostPtr->week[i]), windowsPtr->opentime,
                    windowsPtr->closetime);
            windowsPtr = windowsPtr->nextwind;
        }
    }

    if ( allInfo.numIndx < 1 ) {
        return -1;
    }

    if (!(myHostPtr->busyThreshold = (float *)realloc(myHostPtr->busyThreshold,
                                                      allInfo.numIndx*sizeof(float)))) {
        ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "malloc");
        return -1;
    }
    memcpy(myHostPtr->busyThreshold, sLimConfDatap->myHost_busyThreshold,
           allInfo.numIndx*sizeof(float));

    myHostPtr->rexPriority = sLimConfDatap->myHost_rexPriority;

    myHostPtr->numInstances = sLimConfDatap->myHost_numInstances;

    if ( myHostPtr->numInstances) {
        myHostPtr->instances = (struct resourceInstance **)
            calloc(myHostPtr->numInstances,
                   (sizeof(struct resourceInstance *)));

        if (! myHostPtr->instances && myHostPtr->numInstances) {
            ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "malloc");
            return -1;
        }
    } else {
        myHostPtr->instances = NULL;
    }

    for (i = 0; i < myHostPtr->numInstances; i++) {
        myHostPtr->instances[i] = (struct resourceInstance *)calloc(1, sizeof(struct resourceInstance));
        if (myHostPtr->instances[i] == NULL) {
            ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "malloc");
            return -1;
        }
        myHostPtr->instances[i]->resName =
            putstr_(sLimConfDatap->myHost_instances[i]->resName);
        myHostPtr->instances[i]->value =
            putstr_(sLimConfDatap->myHost_instances[i]->value);
        myHostPtr->instances[i]->orignalValue =
            putstr_(sLimConfDatap->myHost_instances[i]->value);
    }

    sharedResourceHead = NULL;
    for (tmp = sLimConfDatap->sharedResHead, i = 0; tmp; tmp = tmp->nextPtr, i++) {
        tmp1 = (struct sharedResourceInstance *)
            calloc (1, sizeof(sharedResourceInstance));

        if (!tmp1) {
            ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "malloc");
            return -1;
        }
        tmp1->resName = putstr_(tmp->resName);
        tmp1->nextPtr = NULL;
        if (i == 0) {
            sharedResourceHead = tmp1;
            prevPtr = tmp1;
        } else {
            prevPtr->nextPtr = tmp1;
            prevPtr = tmp1;
        }
    }

    return 0;

}

int
lockHost(char *hostName, int request)
{
    struct limLock lockReq;

    if ( request != LIM_LOCK_MASTER && request != LIM_UNLOCK_MASTER) {
        return -1;
    }

    if ( hostName == NULL || !masterMe) {

        return -2;
    }

    if ( strcmp(hostName, myHostPtr->hostName) == 0 ) {

        if ( request == LIM_LOCK_MASTER) {
            limLock.on   |= LIM_LOCK_STAT_MASTER;
            myHostPtr->status[0] |= LIM_LOCKEDM;
        } else {
            limLock.on   &= ~LIM_LOCK_STAT_MASTER;
            myHostPtr->status[0] &= ~LIM_LOCKEDM;
        }
        return 0;
    }

    lockReq.on   = request;
    lockReq.uid = getuid();

    if (getpwnam(lockReq.lsfUserName) == NULL) {
        return -1;
    }

    lockReq.time = 0;

    if (callLim_(LIM_LOCK_HOST, &lockReq, xdr_limLock, NULL, NULL,
                 hostName, 0, NULL) < 0) {
        ls_syslog(LOG_ERR, I18N_FUNC_S_FAIL_M, "lockHost", "callLim",
                  hostName);
        return -2;
    }


    return 0;
}
