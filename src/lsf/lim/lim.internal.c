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

#define  MAXMSGLEN     (1<<24)
#define LIM_CONNECT_TIMEOUT 5
#define LIM_RECV_TIMEOUT    20
#define LIM_RETRYLIMIT      2
#define LIM_RETRYINTVL      500

extern short  hostInactivityLimit;
extern int daemonId;

extern int callLim_(enum limReqCode, void *, bool_t (*)(), void *, bool_t (*)(), char *, int, struct packet_header *);

void
masterRegister(XDR *xdrs, struct sockaddr_in *from, struct packet_header *reqHdr)
{
    static int checkSumMismatch;
    struct hostNode *hPtr;
    struct masterReg masterReg;

    if (!limPortOk(from))
        return;

    if (!xdr_masterReg(xdrs, &masterReg, reqHdr)) {
        ls_syslog(LOG_ERR, "%s: Failed in xdr_masterReg", __func__);
        return;
    }

    if (strcmp(myClusterPtr->clName, masterReg.clName) != 0) {
        syslog(LOG_WARNING, "\
%s: Discard announce from a different cluster %s than mine %s (?)",
                  __func__, masterReg.clName, myClusterPtr->clName);
        return;
    }

    if (masterReg.checkSum != myClusterPtr->checkSum
        && checkSumMismatch < 2
        && (limParams[LSF_LIM_IGNORE_CHECKSUM].paramValue == NULL)) {

        syslog(LOG_WARNING, "%s: Sender %s may have different config.",
               __func__, masterReg.hostName);
        checkSumMismatch++;
    }

    if (equalHost_(myHostPtr->hostName, masterReg.hostName))
        return;

    hPtr = findHostbyList(myClusterPtr->hostList, masterReg.hostName);
    if (hPtr == NULL) {
        syslog(LOG_ERR, "\
%s: Got master announcement from unused host %s; \
Run lim -C on this host to find more information",
                  __func__, masterReg.hostName);
        return;
    }
    /* Regular announce from the master.
     */
    if (myClusterPtr->masterKnown
        && hPtr == myClusterPtr->masterPtr) {

        myClusterPtr->masterInactivityCount = 0;

        if (masterReg.flags & SEND_ELIM_REQ)
            myHostPtr->callElim = TRUE ;
        else
            myHostPtr->callElim = FALSE ;

        if ((masterReg.seqNo - hPtr->lastSeqNo > 2)
            && (masterReg.seqNo > hPtr->lastSeqNo)
            && (hPtr->lastSeqNo != 0))

            ls_syslog(LOG_WARNING, "\
%s: master %s lastSeqNo=%d seqNo=%d. Packets dropped?",
                      __func__, hPtr->hostName,
                      hPtr->lastSeqNo, masterReg.seqNo);

        hPtr->lastSeqNo = masterReg.seqNo;
        hPtr->statInfo.portno = masterReg.portno;

        if (masterReg.flags & SEND_CONF_INFO)
            sndConfInfo(from);

        if (masterReg.flags & SEND_LOAD_INFO) {
            mustSendLoad = TRUE;
            ls_syslog(LOG_DEBUG, "\
%s: Master lim is probing me. Send my load in next interval", __func__);
        }

        return;

    }

    if (myClusterPtr->masterKnown
        && hPtr->hostNo < myHostPtr->hostNo
        && myClusterPtr->masterPtr->hostNo < hPtr->hostNo
        && myClusterPtr->masterInactivityCount <= hostInactivityLimit) {
        syslog(LOG_INFO, "%s: Host %s is trying to take over from %s, "
               "not accepted", __func__, masterReg.hostName,
               myClusterPtr->masterPtr->hostName);
        announceMasterToHost(hPtr, SEND_NO_INFO);
        return;
    }

    if (hPtr->hostNo < myHostPtr->hostNo) {
        // This is the regular master registration.
        hPtr->protoVersion = reqHdr->version;
        myClusterPtr->prevMasterPtr = myClusterPtr->masterPtr;
        myClusterPtr->masterPtr   = hPtr;

        myClusterPtr->masterPtr->statInfo.portno = masterReg.portno;
        if (masterMe) {
            syslog(LOG_INFO, "%s: Give in master to %s", __func__, masterReg.hostName);
        }
        masterMe                  = 0;
        myClusterPtr->masterKnown = 1;
        myClusterPtr->masterInactivityCount = 0;
        mustSendLoad = 1;

        if (masterReg.flags | SEND_CONF_INFO)
            sndConfInfo(from);

        if (masterReg.flags & SEND_LOAD_INFO) {
            mustSendLoad = 1;
            syslog(LOG_DEBUG, "\
%s: Master lim is probing me. Send my load in next interval", __func__);
        }

        return;
    }

    if (myClusterPtr->masterKnown
        && myClusterPtr->masterInactivityCount < hostInactivityLimit) {

        announceMasterToHost(hPtr, SEND_NO_INFO);
        syslog(LOG_INFO, "\
%s: Host %s is trying to take over master LIM from %s, not accepted",
                  __func__, masterReg.hostName,
                  myClusterPtr->masterPtr->hostName);
        return;

    }

    syslog(LOG_INFO, "\
%s: Host %s is trying to take over master LIM, not accepted", __func__,
              masterReg.hostName);

}

static void
announceElimInstance(struct clusterNode *clPtr)
{
    // Bug there must be better way to do this using the
    // external hooks
}

void
announceMaster(struct clusterNode *clPtr, char broadcast, char all)
{
    struct hostNode *hPtr;
    struct sockaddr_in toAddr;
    struct masterReg tmasterReg ;
    XDR    xdrs1;
    char   buf1[MSGSIZE/4];
    XDR    xdrs2;
    char   buf2[MSGSIZE/4];
    XDR    xdrs4;
    char   buf4[MSGSIZE/4];
    enum limReqCode limReqCode;
    struct masterReg masterReg;
    static int cnt = 0;
    struct packet_header reqHdr;
    int announceInIntvl;
    int numAnnounce;
    int i;
    int periods;

    // Bug implement ad moving window on the list of hosts

    /* hostInactivityLimit = 5
     * exchIntvl = 15
     * sampleIntvl = 5
     * periods = (5 - 1) * 15/5 = 60/5 = 12
     *         = 4 * 30/5 = 24
     *         = 4 * 60/5 = 48
     */
    periods = (hostInactivityLimit - 1) * exchIntvl/sampleIntvl;
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

    toAddr.sin_family = AF_INET;
    toAddr.sin_port = lim_port;

    initLSFHeader_(&reqHdr);
    reqHdr.operation  = (short) limReqCode;
    reqHdr.sequence = 0;

    xdrmem_create(&xdrs1, buf1, MSGSIZE/4, XDR_ENCODE);
    masterReg.flags = SEND_NO_INFO ;

    if (! (xdr_LSFHeader(&xdrs1, &reqHdr)
           && xdr_masterReg(&xdrs1, &masterReg, &reqHdr))) {
        ls_syslog(LOG_ERR, "\
%s: Error in xdr_LSFHeader/xdr_masterReg", __func__);
        xdr_destroy(&xdrs1);
        return;
    }

    xdrmem_create(&xdrs2, buf2, MSGSIZE/4, XDR_ENCODE);
    masterReg.flags = SEND_CONF_INFO;
    if (! (xdr_LSFHeader(&xdrs2, &reqHdr)
           && xdr_masterReg(&xdrs2, &masterReg, &reqHdr))) {
        ls_syslog(LOG_ERR, "\
%s: Error in xdr_enum/xdr_masterReg", __func__);
        xdr_destroy(&xdrs1);
        xdr_destroy(&xdrs2);
        return;
    }

    memcpy(&tmasterReg, &masterReg, sizeof(struct masterReg));
    tmasterReg.flags = SEND_NO_INFO | SEND_ELIM_REQ;

    xdrmem_create(&xdrs4, buf4, MSGSIZE/4, XDR_ENCODE);
    if (! xdr_LSFHeader(&xdrs4, &reqHdr)) {
        ls_syslog(LOG_ERR, "\
%s: failed in xdr_LSFHeader", __func__);
        xdr_destroy(&xdrs1);
        xdr_destroy(&xdrs2);
        xdr_destroy(&xdrs4);
        return;
    }

    if (! xdr_masterReg(&xdrs4, &tmasterReg, &reqHdr)) {
        ls_syslog(LOG_ERR,"\
%s: failed in xdr_masterRegister", __func__);
        xdr_destroy(&xdrs1);
        xdr_destroy(&xdrs2);
        xdr_destroy(&xdrs4);
        return;
    }

    if (clPtr->masterKnown && ! broadcast) {

        memcpy(&toAddr.sin_addr, &clPtr->masterPtr->addr[0], sizeof(u_int));
        if (logclass & LC_COMM)
            ls_syslog(LOG_DEBUG, "\
%s: Sending request to LIM on %s: %m", __func__, sockAdd2Str_(&toAddr));

        if (chanSendDgram_(limSock,
                           buf1,
                           XDR_GETPOS(&xdrs1),
                           (struct sockaddr_in *)&toAddr) < 0)
            ls_syslog(LOG_ERR, "\
%s: Failed to send request to LIM on %s: %m", __func__,
                      sockAdd2Str_(&toAddr));
        xdr_destroy(&xdrs1);
        return;
    }

    if (all) {
        hPtr = clPtr->hostList;
        announceInIntvl = clPtr->numHosts;
    } else {
        announceInIntvl = clPtr->numHosts/periods;
        if (announceInIntvl == 0)
            announceInIntvl = 1;

        hPtr = clPtr->hostList;
        for (i = 0; i < cnt * announceInIntvl; i++) {
            if (!hPtr)
                break;
            hPtr = hPtr->nextPtr;
        }

        /* Let's announce the rest of the hosts,
         * this takes care of the reminder
         * numHosts/periods.
         */
        if (cnt == (periods - 1))
            announceInIntvl = clPtr->numHosts;
    }

    ls_syslog(LOG_DEBUG, "\
%s: all %d cnt %d announceInIntvl %d",
              __func__, all, cnt, announceInIntvl);

    for (numAnnounce = 0;
         hPtr && (numAnnounce < announceInIntvl);
         hPtr = hPtr->nextPtr, numAnnounce++) {

        if (hPtr == myHostPtr)
            continue;

        memcpy(&toAddr.sin_addr, &hPtr->addr[0], sizeof(u_int));

        if (hPtr->infoValid == TRUE) {

            if (logclass & LC_COMM)
                ls_syslog(LOG_DEBUG, "\
%s: send announce (normal) to %s %s, inactivityCount=%d",
                          __func__,
                          hPtr->hostName, sockAdd2Str_(&toAddr),
                          hPtr->hostInactivityCount);

            if (hPtr->callElim){

                if (logclass & LC_COMM)
                    ls_syslog(LOG_DEBUG,"\
%s: announcing SEND_ELIM_REQ to host %s %s",
                              __func__, hPtr->hostName,
                              sockAdd2Str_(&toAddr));

                if (chanSendDgram_(limSock,
                                   buf4,
                                   XDR_GETPOS(&xdrs4),
                                   (struct sockaddr_in *)&toAddr) < 0) {
                    ls_syslog(LOG_ERR,"\
%s: Failed to send request 1 to LIM on %s: %m", __func__,
                              hPtr->hostName);
                }

                hPtr->callElim = FALSE;

            } else {
                if (chanSendDgram_(limSock,
                                   buf1,
                                   XDR_GETPOS(&xdrs1),
                                   (struct sockaddr_in *)&toAddr) < 0)
                    ls_syslog(LOG_ERR,"\
announceMaster: Failed to send request 1 to LIM on %s: %m", hPtr->hostName);
            }

        } else {

            if (logclass & LC_COMM)
                ls_syslog(LOG_DEBUG,"\
%s: send announce (SEND_CONF) to %s %s %x, inactivityCount=%d",
                          __func__,
                          hPtr->hostName, sockAdd2Str_(&toAddr),
                          hPtr->addr[0],
                          hPtr->hostInactivityCount);

            if (chanSendDgram_(limSock,
                               buf2,
                               XDR_GETPOS(&xdrs2),
                               (struct sockaddr_in *)&toAddr) < 0)
                ls_syslog(LOG_ERR, "\
%s: Failed to send request 2 to LIM on %s: %m",
                          __func__, hPtr->hostName);
        }

    }

    xdr_destroy(&xdrs1);
    xdr_destroy(&xdrs2);
    xdr_destroy(&xdrs4);

    return;
}


void
jobxferReq(XDR *xdrs, struct sockaddr_in *from, struct packet_header *reqHdr)
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
            ls_syslog(LOG_ERR, "%s: %s not found in jobxferReq",
                      fname,
                      jobXfer.placeInfo[i].hostName);
        }
    }

    return;

}

void
wrongMaster(struct sockaddr_in *from, char *buf, struct packet_header *reqHdr, int
            s)
{
    static char fname[] = "wrongMaster()";
    enum limReplyCode limReplyCode;

    XDR xdrs;
    struct packet_header replyHdr;
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
    replyHdr.operation  = (short) limReplyCode;
    replyHdr.sequence = reqHdr->sequence;

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

    ls_syslog(LOG_WARNING, "%s: I am the master now.", fname);

    return;

}

void
rcvConfInfo(XDR *xdrs, struct sockaddr_in *from, struct packet_header *hdr)
{
    static char     fname[] = "rcvConfInfo()";
    struct statInfo sinfo;
    struct hostNode *hPtr;
    short  sinfoTypeNo, sinfoModelNo;

    if (!limPortOk(from))
        return;

    sinfo.maxCpus = 0;

    if (!masterMe) {
        ls_syslog(LOG_DEBUG, "rcvConfInfo: I am not the master!");
        return;
    }

    if (!xdr_statInfo(xdrs, &sinfo, hdr)) {
        ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "xdr_statInfo");
        return;
    }

    hPtr = findHostbyAddr(from, "rcvConfInfo");
    if (!hPtr)
        return;

    if (findHostbyList(myClusterPtr->hostList, hPtr->hostName) == NULL) {
        ls_syslog(LOG_ERR, "%s: Got info from client-only host %s/%s",
                  fname, sockAdd2Str_(from), hPtr->hostName);
        return;
    }

    if ((sinfo.maxCpus <= 0) || (sinfo.maxMem < 0)) {
        ls_syslog(LOG_ERR, "%s: Invalid info received: maxCpus=%d, maxMem=%d",
                  fname, sinfo.maxCpus, sinfo.maxMem);
        return;
    }

    hPtr->statInfo.maxCpus   = sinfo.maxCpus;
    hPtr->statInfo.maxMem    = sinfo.maxMem;
    hPtr->statInfo.maxSwap   = sinfo.maxSwap;
    hPtr->statInfo.maxTmp    = sinfo.maxTmp;
    hPtr->statInfo.nDisks    = sinfo.nDisks;
    hPtr->statInfo.portno    = sinfo.portno;
    sinfoTypeNo  = typeNameToNo(sinfo.hostType);
    if (sinfoTypeNo < 0) {
        ls_syslog(LOG_ERR, "%s: Unknown host type <%s>, using <DEFAULT>",
                  fname, sinfo.hostType);
        sinfoTypeNo = 1;
    }

    if (logclass & LC_TRACE) {
        ls_syslog(LOG_DEBUG2, "%s: host <%s> ncpu <%d> maxmem <%u> maxswp <%u> maxtmp <%u> ndisk <%d>",
                  fname, hPtr->hostName,
                  hPtr->statInfo.maxCpus,
                  hPtr->statInfo.maxMem,
                  hPtr->statInfo.maxSwap,
                  hPtr->statInfo.maxTmp,
                  hPtr->statInfo.nDisks);
    }

    if (hPtr->hModelNo != DETECTMODELTYPE) {

        sinfoModelNo = hPtr->hModelNo;
    } else {
        sinfoModelNo = archNameToNo(sinfo.hostArch);
        if (sinfoModelNo < 0) {
            ls_syslog(LOG_ERR, "%s: Unknown host architecture <%s>, using <DEFAULT>",  fname, sinfo.hostArch);
            sinfoModelNo = 1;
        } else {
            if (strcmp(allInfo.hostArchs[sinfoModelNo], sinfo.hostArch) != 0) {
                if ( logclass & LC_EXEC )  {
                    ls_syslog(LOG_WARNING, "%s: Unknown host architecture <%s>, using best match <%s>, model <%s>",
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

    if (lim_debug)
        syslog(LOG_DEBUG, "%s: Host %s: maxCpus=%d maxMem=%d "
               "ndisks=%d typeNo=%d modelNo=%d", __func__,
               hPtr->hostName, hPtr->statInfo.maxCpus, hPtr->statInfo.maxMem,
               hPtr->statInfo.nDisks, (int)hPtr->hTypeNo, (int)hPtr->hModelNo);

    return;
}

void
sndConfInfo(struct sockaddr_in *to)
{
    static char fname[] = "sndConfInfo()";
    char   buf[MSGSIZE/4];
    XDR    xdrs;
    enum limReqCode limReqCode;
    struct packet_header reqHdr;

    memset((char*)&buf, 0, sizeof(buf));
    initLSFHeader_(&reqHdr);

    if (logclass & LC_COMM)
        ls_syslog(LOG_DEBUG, "%s: Sending info",fname);

    limReqCode = LIM_CONF_INFO;

    xdrmem_create(&xdrs, buf, MSGSIZE/4, XDR_ENCODE);
    reqHdr.operation  = (short) limReqCode;
    reqHdr.sequence =  0;

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
checkHostWd(void)
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
      struct sockaddr_in toAddr;
    XDR    xdrs;
    char   buf[MSGSIZE/4];
    enum limReqCode limReqCode;
    struct masterReg masterReg;
    struct packet_header reqHdr;

    limReqCode = LIM_MASTER_ANN;
    strcpy(masterReg.clName, myClusterPtr->clName);
    strcpy(masterReg.hostName, myClusterPtr->masterPtr->hostName);
    masterReg.flags = infoType;
    masterReg.seqNo    = masterAnnSeqNo;
    masterReg.checkSum = myClusterPtr->checkSum;
    masterReg.portno   = myClusterPtr->masterPtr->statInfo.portno;

    toAddr.sin_family = AF_INET;
    toAddr.sin_port = lim_port;

    xdrmem_create(&xdrs, buf, MSGSIZE/4, XDR_ENCODE);
    initLSFHeader_(&reqHdr);
    reqHdr.operation  = (short) limReqCode;
    reqHdr.sequence =  0;

    if (! xdr_LSFHeader(&xdrs,  &reqHdr)
        || ! xdr_masterReg(&xdrs, &masterReg, &reqHdr)) {
        ls_syslog(LOG_ERR, "\
%s: Error xdr_LSFHeader/xdr_masterReg to %s",
                  __func__, sockAdd2Str_(&toAddr));
        xdr_destroy(&xdrs);
        return;
    }

    memcpy(&toAddr.sin_addr, &hPtr->addr[0], sizeof(in_addr_t));

    ls_syslog(LOG_DEBUG, "\
%s: Sending request %d to LIM on %s",
              __func__, infoType, sockAdd2Str_(&toAddr));

    if (chanSendDgram_(limSock,
                       buf,
                       XDR_GETPOS(&xdrs),
                       (struct sockaddr_in *)&toAddr) < 0)
        ls_syslog(LOG_ERR, "\
%s: Failed to send request %d to LIM on %s: %m", __func__,
                  infoType, sockAdd2Str_(&toAddr));

    xdr_destroy(&xdrs);

}

int
probeMasterTcp(struct clusterNode *clPtr)
{
    static char fname[] = "probeMasterTcp";
    struct hostNode *hPtr;
    struct sockaddr_in mlim_addr;
    int ch, rc;
    struct packet_header reqHdr;

    ls_syslog (LOG_DEBUG, "probeMasterTcp: enter.... ");
    hPtr = clPtr->masterPtr;
    if (!hPtr)
        hPtr = clPtr->prevMasterPtr;
    ls_syslog (LOG_ERR, "probeMasterTcp: Last master is  UNKNOWN");

    if (!hPtr)
        return -1;
    if (hPtr == myHostPtr)
        return -1;

    ls_syslog(LOG_ERR, "%s: Attempting to probe last known master %s port %d timeout is %d",
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
                  "OK");

    if ( rc >= 0) {
        initLSFHeader_(&reqHdr);
        reqHdr.operation = LIM_PING;
        writeEncodeHdr_(ch, &reqHdr, chanWrite_ );
        chanClose_(ch);
    }

    return rc;
}

int
callMasterTcp(enum limReqCode ReqCode, struct hostNode *masterPtr,
              void *reqBuf, bool_t (*xdr_sfunc)(),
              void *replyBuf, bool_t (*xdr_rfunc)(), int rcvTimeout,
              int connTimeout, struct packet_header *hdr)
{
    static char fname[] = "callMasterTcp";
    XDR    xdrs;
    char   sbuf[4*MSGSIZE];
    char   replyHdrBuf[PACKET_HEADER_SIZE];
    char   *tmpBuf;
    enum limReplyCode ReplyCode;
    struct hostNode *hPtr;
    struct packet_header reqHdr;
    struct packet_header replyHdr;
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
            ls_syslog(LOG_ERR, "%s: Bad Master Information passed.",
                      fname);
            return -1;
        }
        hPtr = myClusterPtr->masterPtr;
    } else {
        if (masterPtr == NULL) {
            ls_syslog(LOG_ERR, "%s: No Master Information.",
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
    reqHdr.operation = ReqCode;
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

    xdrmem_create(&xdrs, replyHdrBuf, PACKET_HEADER_SIZE , XDR_DECODE);
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
        ReplyCode = replyHdr.operation;

        switch(ReplyCode) {
        case LIME_NO_ERR:
            if (!(*xdr_rfunc)(&xdrs, replyBuf, &replyHdr)) {
                ls_syslog(LOG_ERR,"xdr fail");
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

    if (getpwnam2(lockReq.lsfUserName) == NULL) {
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
