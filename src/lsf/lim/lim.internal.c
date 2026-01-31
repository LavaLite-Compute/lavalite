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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 USA
 *
 */

#include "lsf/lim/lim.h"

#define LIM_CONNECT_TIMEOUT 5
#define LIM_RECV_TIMEOUT 20
#define LIM_RETRYLIMIT 2
#define LIM_RETRYINTVL 500

extern short hostInactivityLimit;

void masterRegister(XDR *xdrs, struct sockaddr_in *from,
                    struct packet_header *reqHdr)
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

    if (masterReg.checkSum != myClusterPtr->checkSum && checkSumMismatch < 2 &&
        (genParams[LSF_LIM_IGNORE_CHECKSUM].paramValue == NULL)) {
        syslog(LOG_WARNING, "%s: Sender %s may have different config.",
               __func__, masterReg.hostName);
        checkSumMismatch++;
    }

    if (equal_host(myHostPtr->hostName, masterReg.hostName))
        return;

    hPtr = find_node_by_cluster(myClusterPtr->hostList, masterReg.hostName);
    if (hPtr == NULL) {
        syslog(LOG_ERR, "\
%s: Got master announcement from unused host %s; \
Run lim -C on this host to find more information",
               __func__, masterReg.hostName);
        return;
    }
    /* Regular announce from the master.
     */
    if (myClusterPtr->masterKnown && hPtr == myClusterPtr->masterPtr) {
        myClusterPtr->masterInactivityCount = 0;

        if (masterReg.flags & SEND_ELIM_REQ)
            myHostPtr->callElim = TRUE;
        else
            myHostPtr->callElim = FALSE;

        if ((masterReg.seqNo - hPtr->lastSeqNo > 2) &&
            (masterReg.seqNo > hPtr->lastSeqNo) && (hPtr->lastSeqNo != 0))

            ls_syslog(LOG_WARNING, "\
%s: master %s lastSeqNo=%d seqNo=%d. Packets dropped?",
                      __func__, hPtr->hostName, hPtr->lastSeqNo,
                      masterReg.seqNo);

        hPtr->lastSeqNo = masterReg.seqNo;
        hPtr->statInfo.portno = masterReg.portno;

        if (masterReg.flags & SEND_CONF_INFO)
            sndConfInfo(from);

        if (masterReg.flags & SEND_LOAD_INFO) {
            mustSendLoad = TRUE;
            ls_syslog(LOG_DEBUG, "\
%s: Master lim is probing me. Send my load in next interval",
                      __func__);
        }

        return;
    }

    if (myClusterPtr->masterKnown && hPtr->hostNo < myHostPtr->hostNo &&
        myClusterPtr->masterPtr->hostNo < hPtr->hostNo &&
        myClusterPtr->masterInactivityCount <= hostInactivityLimit) {
        syslog(LOG_INFO,
               "%s: Host %s is trying to take over from %s, "
               "not accepted",
               __func__, masterReg.hostName, myClusterPtr->masterPtr->hostName);
        announceMasterToHost(hPtr, SEND_NO_INFO);
        return;
    }

    if (hPtr->hostNo < myHostPtr->hostNo) {
        // This is the regular master registration.
        hPtr->protoVersion = reqHdr->version;
        myClusterPtr->prevMasterPtr = myClusterPtr->masterPtr;
        myClusterPtr->masterPtr = hPtr;

        myClusterPtr->masterPtr->statInfo.portno = masterReg.portno;
        if (masterMe) {
            syslog(LOG_INFO, "%s: Give in master to %s", __func__,
                   masterReg.hostName);
        }
        masterMe = 0;
        myClusterPtr->masterKnown = 1;
        myClusterPtr->masterInactivityCount = 0;
        mustSendLoad = 1;

        if (masterReg.flags | SEND_CONF_INFO)
            sndConfInfo(from);

        if (masterReg.flags & SEND_LOAD_INFO) {
            mustSendLoad = 1;
            syslog(LOG_DEBUG, "\
%s: Master lim is probing me. Send my load in next interval",
                   __func__);
        }

        return;
    }

    if (myClusterPtr->masterKnown &&
        myClusterPtr->masterInactivityCount < hostInactivityLimit) {
        announceMasterToHost(hPtr, SEND_NO_INFO);
        syslog(LOG_INFO, "\
%s: Host %s is trying to take over master LIM from %s, not accepted",
               __func__, masterReg.hostName, myClusterPtr->masterPtr->hostName);
        return;
    }

    syslog(LOG_INFO, "\
%s: Host %s is trying to take over master LIM, not accepted",
           __func__, masterReg.hostName);
}

void announceMaster(struct clusterNode *clPtr, char broadcast, char all)
{
    struct hostNode *hPtr;
    struct masterReg tmasterReg;
    XDR xdrs1;
    char buf1[LL_BUFSIZ_256];
    XDR xdrs2;
    char buf2[LL_BUFSIZ_256];
    XDR xdrs4;
    char buf4[LL_BUFSIZ_256];
    enum limReqCode limReqCode;
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
    periods = (hostInactivityLimit - 1) * exchIntvl / sampleIntvl;
    if (!all && (++cnt > (periods - 1))) {
        cnt = 0;
        masterAnnSeqNo++;
    }

    // Master, me, parameters
    struct masterReg masterReg;
    limReqCode = LIM_MASTER_ANN;
    strcpy(masterReg.clName, myClusterPtr->clName);
    strcpy(masterReg.hostName, myClusterPtr->masterPtr->hostName);
    masterReg.seqNo = masterAnnSeqNo;
    masterReg.checkSum = myClusterPtr->checkSum;
    masterReg.portno = myClusterPtr->masterPtr->statInfo.portno;

    // send to
    struct sockaddr_in to_addr;
    memset(&to_addr, 0, sizeof(struct sockaddr_in));
    to_addr.sin_family = AF_INET;
    to_addr.sin_port = lim_udp_port;

    init_pack_hdr(&reqHdr);
    reqHdr.operation = limReqCode;
    reqHdr.sequence = 0;

    xdrmem_create(&xdrs1, buf1, sizeof(buf1), XDR_ENCODE);
    masterReg.flags = SEND_NO_INFO;

    if (!(xdr_pack_hdr(&xdrs1, &reqHdr) &&
          xdr_masterReg(&xdrs1, &masterReg, &reqHdr))) {
        ls_syslog(LOG_ERR, "\
%s: Error in xdr_pack_hdr/xdr_masterReg",
                  __func__);
        xdr_destroy(&xdrs1);
        return;
    }

    xdrmem_create(&xdrs2, buf2, sizeof(buf2), XDR_ENCODE);
    masterReg.flags = SEND_CONF_INFO;
    if (!(xdr_pack_hdr(&xdrs2, &reqHdr) &&
          xdr_masterReg(&xdrs2, &masterReg, &reqHdr))) {
        ls_syslog(LOG_ERR, "\
%s: Error in xdr_enum/xdr_masterReg",
                  __func__);
        xdr_destroy(&xdrs1);
        xdr_destroy(&xdrs2);
        return;
    }

    memcpy(&tmasterReg, &masterReg, sizeof(struct masterReg));
    tmasterReg.flags = SEND_NO_INFO | SEND_ELIM_REQ;

    xdrmem_create(&xdrs4, buf4, sizeof(buf4), XDR_ENCODE);
    if (!xdr_pack_hdr(&xdrs4, &reqHdr)) {
        ls_syslog(LOG_ERR, "\
%s: failed in xdr_pack_hdr",
                  __func__);
        xdr_destroy(&xdrs1);
        xdr_destroy(&xdrs2);
        xdr_destroy(&xdrs4);
        return;
    }

    if (!xdr_masterReg(&xdrs4, &tmasterReg, &reqHdr)) {
        ls_syslog(LOG_ERR, "\
%s: failed in xdr_masterRegister",
                  __func__);
        xdr_destroy(&xdrs1);
        xdr_destroy(&xdrs2);
        xdr_destroy(&xdrs4);
        return;
    }

    if (clPtr->masterKnown && !broadcast) {
        // Set the destination addr
        get_host_sinaddrv4(clPtr->masterPtr->v4_epoint, &to_addr);

        if (logclass & LC_COMM)
            ls_syslog(LOG_DEBUG, "\
%s: Sending request to LIM on %s: %m",
                      __func__, sockAdd2Str_(&to_addr));

        if (chan_send_dgram(lim_udp_chan, buf1, XDR_GETPOS(&xdrs1),
                            (struct sockaddr_in *) &to_addr) < 0)
            ls_syslog(LOG_ERR, "\
%s: Failed to send request to LIM on %s: %m",
                      __func__, sockAdd2Str_(&to_addr));
        xdr_destroy(&xdrs1);
        return;
    }

    if (all) {
        hPtr = clPtr->hostList;
        announceInIntvl = clPtr->numHosts;
    } else {
        announceInIntvl = clPtr->numHosts / periods;
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
    /*
      ls_syslog(LOG_DEBUG, "\
      %s: all %d cnt %d announceInIntvl %d",
      __func__, all, cnt, announceInIntvl);
    */
    for (numAnnounce = 0; hPtr && (numAnnounce < announceInIntvl);
         hPtr = hPtr->nextPtr, numAnnounce++) {
        if (hPtr == myHostPtr)
            continue;

        get_host_sinaddrv4(hPtr->v4_epoint, &to_addr);

        if (hPtr->infoValid == TRUE) {
            if (logclass & LC_COMM)
                ls_syslog(LOG_DEBUG, "\
%s: send announce (normal) to %s %s, inactivityCount=%d",
                          __func__, hPtr->hostName, sockAdd2Str_(&to_addr),
                          hPtr->hostInactivityCount);

            if (hPtr->callElim) {
                if (logclass & LC_COMM)
                    ls_syslog(LOG_DEBUG, "\
%s: announcing SEND_ELIM_REQ to host %s %s",
                              __func__, hPtr->hostName, sockAdd2Str_(&to_addr));

                if (chan_send_dgram(lim_udp_chan, buf4, XDR_GETPOS(&xdrs4),
                                    (struct sockaddr_in *) &to_addr) < 0) {
                    ls_syslog(LOG_ERR, "\
%s: Failed to send request 1 to LIM on %s: %m",
                              __func__, hPtr->hostName);
                }

                hPtr->callElim = FALSE;

            } else {
                if (chan_send_dgram(lim_udp_chan, buf1, XDR_GETPOS(&xdrs1),
                                    (struct sockaddr_in *) &to_addr) < 0)
                    ls_syslog(LOG_ERR, "\
announceMaster: Failed to send request 1 to LIM on %s: %m",
                              hPtr->hostName);
            }

        } else {
            if (logclass & LC_COMM)
                ls_syslog(LOG_DEBUG, "\
%s: send announce (SEND_CONF) to %s %s, inactivityCount=%d",
                          __func__, hPtr->hostName, sockAdd2Str_(&to_addr),
                          hPtr->hostInactivityCount);

            if (chan_send_dgram(lim_udp_chan, buf2, XDR_GETPOS(&xdrs2),
                                (struct sockaddr_in *) &to_addr) < 0)
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

void initNewMaster(void)
{
    static char fname[] = "initNewMaster";
    struct hostNode *hPtr;
    int j;

    for (hPtr = myClusterPtr->hostList; hPtr; hPtr = hPtr->nextPtr) {
        if (hPtr != myHostPtr) {
            hPtr->status[0] |= LIM_UNAVAIL;
            for (j = 0; j < GET_INTNUM(allInfo.numIndx); j++)
                hPtr->status[j + 1] = 0;
            hPtr->hostInactivityCount = 0;
            hPtr->infoValid = false;
            hPtr->lastSeqNo = 0;
        }
    }
    masterAnnSeqNo = 0;

    mustSendLoad = true;
    myClusterPtr->masterKnown = true;
    myClusterPtr->prevMasterPtr = myClusterPtr->masterPtr;
    myClusterPtr->masterPtr = myHostPtr;

    announceMaster(myClusterPtr, 1, true);
    myClusterPtr->masterInactivityCount = 0;

    masterMe = true;

    ls_syslog(LOG_WARNING, "%s: I am the master now.", fname);

    return;
}

void rcvConfInfo(XDR *xdrs, struct sockaddr_in *from, struct packet_header *hdr)
{
    static char fname[] = "rcvConfInfo()";
    struct statInfo sinfo;
    struct hostNode *hPtr;
    short sinfoTypeNo, sinfoModelNo;

    if (!limPortOk(from))
        return;

    sinfo.maxCpus = 0;

    if (!masterMe) {
        ls_syslog(LOG_DEBUG, "rcvConfInfo: I am not the master!");
        return;
    }

    if (!xdr_statInfo(xdrs, &sinfo, hdr)) {
        ls_syslog(LOG_ERR, "%s: %s failed: %m", fname, "xdr_statInfo");
        return;
    }

    hPtr = find_node_by_sockaddr_in(from);
    if (!hPtr)
        return;

    if (find_node_by_cluster(myClusterPtr->hostList, hPtr->hostName) == NULL) {
        ls_syslog(LOG_ERR, "%s: Got info from client-only host %s/%s", fname,
                  sockAdd2Str_(from), hPtr->hostName);
        return;
    }

    if ((sinfo.maxCpus <= 0) || (sinfo.maxMem < 0)) {
        ls_syslog(LOG_ERR, "%s: Invalid info received: maxCpus=%d, maxMem=%d",
                  fname, sinfo.maxCpus, sinfo.maxMem);
        return;
    }

    hPtr->statInfo.maxCpus = sinfo.maxCpus;
    hPtr->statInfo.maxMem = sinfo.maxMem;
    hPtr->statInfo.maxSwap = sinfo.maxSwap;
    hPtr->statInfo.maxTmp = sinfo.maxTmp;
    hPtr->statInfo.nDisks = sinfo.nDisks;
    hPtr->statInfo.portno = sinfo.portno;
    sinfoTypeNo = typeNameToNo(sinfo.hostType);
    if (sinfoTypeNo < 0) {
        ls_syslog(LOG_ERR, "%s: Unknown host type <%s>, using <DEFAULT>", fname,
                  sinfo.hostType);
        sinfoTypeNo = 1;
    }

    if (logclass & LC_TRACE) {
        ls_syslog(LOG_DEBUG2,
                  "%s: host <%s> ncpu <%d> maxmem <%u> maxswp <%u> maxtmp <%u> "
                  "ndisk <%d>",
                  fname, hPtr->hostName, hPtr->statInfo.maxCpus,
                  hPtr->statInfo.maxMem, hPtr->statInfo.maxSwap,
                  hPtr->statInfo.maxTmp, hPtr->statInfo.nDisks);
    }

    if (hPtr->hModelNo != DETECTMODELTYPE) {
        sinfoModelNo = hPtr->hModelNo;
    } else {
        sinfoModelNo = archNameToNo(sinfo.hostArch);
        if (sinfoModelNo < 0) {
            ls_syslog(LOG_ERR,
                      "%s: Unknown host architecture <%s>, using <DEFAULT>",
                      fname, sinfo.hostArch);
            sinfoModelNo = 1;
        } else {
            if (strcmp(allInfo.hostArchs[sinfoModelNo], sinfo.hostArch) != 0) {
                if (logclass & LC_EXEC) {
                    ls_syslog(LOG_WARNING,
                              "%s: Unknown host architecture <%s>, using best "
                              "match <%s>, model <%s>",
                              fname, sinfo.hostArch,
                              allInfo.hostArchs[sinfoModelNo],
                              allInfo.hostModels[sinfoModelNo]);
                }
            }
        }
    }

    if (hPtr->infoValid == false) {
        ++allInfo.modelRefs[sinfoModelNo];
    } else {
        if ((hPtr->hModelNo != sinfoModelNo) &&
            (hPtr->hModelNo != DETECTMODELTYPE)) {
            --allInfo.modelRefs[hPtr->hModelNo];
            ++allInfo.modelRefs[sinfoModelNo];
        }
    }

    if (hPtr->hTypeNo == DETECTMODELTYPE) {
        if ((hPtr->hTypeNo = sinfoTypeNo) < 0) {
            hPtr->hTypeNo = 0;
        } else {
            strcpy(hPtr->statInfo.hostType, sinfo.hostType);
            myClusterPtr->typeClass |= (1 << hPtr->hTypeNo);
            SET_BIT(hPtr->hTypeNo, myClusterPtr->hostTypeBitMaps);
        }
    }

    if (hPtr->hModelNo == DETECTMODELTYPE) {
        if ((hPtr->hModelNo = sinfoModelNo) < 0) {
            hPtr->hModelNo = 0;
        } else {
            strcpy(hPtr->statInfo.hostArch, sinfo.hostArch);
            myClusterPtr->modelClass |= (1 << hPtr->hModelNo);
            SET_BIT(hPtr->hModelNo, myClusterPtr->hostModelBitMaps);
        }
    }

    hPtr->protoVersion = hdr->version;
    hPtr->infoValid = true;
    hPtr->infoMask = 0;

    if (lim_debug)
        syslog(LOG_DEBUG,
               "%s: Host %s: maxCpus=%d maxMem=%d "
               "ndisks=%d typeNo=%d modelNo=%d",
               __func__, hPtr->hostName, hPtr->statInfo.maxCpus,
               hPtr->statInfo.maxMem, hPtr->statInfo.nDisks,
               (int) hPtr->hTypeNo, (int) hPtr->hModelNo);

    return;
}

void sndConfInfo(struct sockaddr_in *to)
{
    static char fname[] = "sndConfInfo()";
    char buf[MSGSIZE / 4];
    XDR xdrs;
    enum limReqCode limReqCode;
    struct packet_header reqHdr;

    memset((char *) &buf, 0, sizeof(buf));
    init_pack_hdr(&reqHdr);

    if (logclass & LC_COMM)
        ls_syslog(LOG_DEBUG, "%s: Sending info", fname);

    limReqCode = LIM_CONF_INFO;

    xdrmem_create(&xdrs, buf, MSGSIZE / 4, XDR_ENCODE);
    reqHdr.operation = (short) limReqCode;
    reqHdr.sequence = 0;

    if (logclass & LC_TRACE) {
        ls_syslog(LOG_DEBUG2,
                  "%s: host <%s> ncpu <%d> maxmem <%d> maxswp <%u> maxtmp <%u> "
                  "ndisk <%d>",
                  fname, myHostPtr->hostName, myHostPtr->statInfo.maxCpus,
                  myHostPtr->statInfo.maxMem, myHostPtr->statInfo.maxSwap,
                  myHostPtr->statInfo.maxTmp, myHostPtr->statInfo.nDisks);
    }

    if (!(xdr_pack_hdr(&xdrs, &reqHdr) &&
          xdr_statInfo(&xdrs, &myHostPtr->statInfo, &reqHdr))) {
        ls_syslog(LOG_ERR, "%s: %s failed: %m", fname,
                  "xdr_pack_hdr/xdr_statInfo");
        return;
    }

    if (logclass & LC_COMM)
        ls_syslog(LOG_DEBUG, "%s: chan_send_dgram info to %s", fname,
                  sockAdd2Str_(to));

    if (chan_send_dgram(lim_udp_chan, buf, XDR_GETPOS(&xdrs),
                        (struct sockaddr_in *) to) < 0) {
        ls_syslog(LOG_ERR, "%s: %s(%s) failed: %m", fname, "chan_send_dgram",
                  sockAdd2Str_(to));
        return;
    }

    xdr_destroy(&xdrs);

    return;
}

void checkHostWd(void)
{
    struct dayhour dayhour;
    windows_t *wp;
    char active;
    time_t now = time(0);

    if (myHostPtr->wind_edge > now || myHostPtr->wind_edge == 0)
        return;

    getDayHour(&dayhour, now);
    if (myHostPtr->week[dayhour.day] == NULL) {
        myHostPtr->status[0] |= LIM_LOCKEDW;
        myHostPtr->wind_edge = now + (24.0 - dayhour.hour) * 3600.0;
        return;
    }
    active = false;
    myHostPtr->wind_edge = now + (24.0 - dayhour.hour) * 3600.0;
    for (wp = myHostPtr->week[dayhour.day]; wp; wp = wp->nextwind)
        checkWindow(&dayhour, &active, &myHostPtr->wind_edge, wp, now);
    if (!active)
        myHostPtr->status[0] |= LIM_LOCKEDW;
    else
        myHostPtr->status[0] &= ~LIM_LOCKEDW;
}

void announceMasterToHost(struct hostNode *hPtr, int infoType)
{
    XDR xdrs;
    char buf[LL_BUFSIZ_1K];
    enum limReqCode limReqCode;
    struct masterReg masterReg;
    struct packet_header reqHdr;

    limReqCode = LIM_MASTER_ANN;

    strcpy(masterReg.clName, myClusterPtr->clName);
    strcpy(masterReg.hostName, myClusterPtr->masterPtr->hostName);
    masterReg.flags = infoType;
    masterReg.seqNo = masterAnnSeqNo;
    masterReg.checkSum = myClusterPtr->checkSum;
    masterReg.portno = myClusterPtr->masterPtr->statInfo.portno;

    struct sockaddr_in to_addr;
    to_addr.sin_family = AF_INET;
    to_addr.sin_port = lim_udp_port;

    xdrmem_create(&xdrs, buf, sizeof(buf), XDR_ENCODE);
    init_pack_hdr(&reqHdr);
    reqHdr.operation = (short) limReqCode;
    reqHdr.sequence = 0;

    if (!xdr_pack_hdr(&xdrs, &reqHdr) ||
        !xdr_masterReg(&xdrs, &masterReg, &reqHdr)) {
        ls_syslog(LOG_ERR, "\
%s: Error xdr_pack_hdr/xdr_masterReg to %s",
                  __func__, sockAdd2Str_(&to_addr));
        xdr_destroy(&xdrs);
        return;
    }

    // End point destination
    get_host_sinaddrv4(hPtr->v4_epoint, &to_addr);

    ls_syslog(LOG_DEBUG, "\
%s: Sending request %d to LIM on %s",
              __func__, infoType, sockAdd2Str_(&to_addr));

    if (chan_send_dgram(lim_udp_chan, buf, XDR_GETPOS(&xdrs),
                        (struct sockaddr_in *) &to_addr) < 0)
        ls_syslog(LOG_ERR, "\
%s: Failed to send request %d to LIM on %s: %m",
                  __func__, infoType, sockAdd2Str_(&to_addr));

    xdr_destroy(&xdrs);
}

int probeMasterTcp(struct clusterNode *clPtr)
{
    struct hostNode *hPtr;

    ls_syslog(LOG_DEBUG, "probeMasterTcp: enter.... ");

    hPtr = clPtr->masterPtr;
    if (!hPtr)
        hPtr = clPtr->prevMasterPtr;
    ls_syslog(LOG_ERR, "probeMasterTcp: Last master is  UNKNOWN");

    if (!hPtr)
        return -1;
    if (hPtr == myHostPtr)
        return -1;

    ls_syslog(LOG_ERR, "%s: probe last known master %s port %d timeout is %d",
              __func__, hPtr->hostName, ntohs(hPtr->statInfo.portno),
              probeTimeout);

    struct sockaddr_in mlim_addr;
    memset(&mlim_addr, 0, sizeof(mlim_addr));
    mlim_addr.sin_family = AF_INET;
    get_host_sinaddrv4(hPtr->v4_epoint, &mlim_addr);
    mlim_addr.sin_port = hPtr->statInfo.portno;

    int ch = chan_client_socket(AF_INET, SOCK_STREAM, 0);
    if (ch < 0) {
        ls_syslog(LOG_ERR, "%s: %s failed: %m", __func__, "chan_client_socket");
        return -2;
    }

    int rc = chan_connect(ch, &mlim_addr, probeTimeout * 1000, 0);
    if (rc < 0) {
        ls_syslog(LOG_ERR, "%s: %s(%s) failed: %m", __func__,
                  sockAdd2Str_(&mlim_addr));
        chan_close(ch);
        return -1;
    }

    struct packet_header hdr;
    init_pack_hdr(&hdr);
    hdr.operation = LIM_PING;
    rc = send_packet_header(ch, &hdr);
    if (rc < 0) {
        ls_syslog(LOG_ERR, "%s: failed send_packet_header() to %s %m", __func__,
                  sockAdd2Str_(&mlim_addr));
        chan_close(ch);
        return -1;
    }
    // Bug just because the connect() succeeded does not mean
    // the lim is ok...
    chan_close(ch);

    return rc;
}

int lockHost(char *hostName, int request)
{
    struct limLock lockReq;

    if (request != LIM_LOCK_MASTER && request != LIM_UNLOCK_MASTER) {
        return -1;
    }

    if (hostName == NULL || !masterMe) {
        return -2;
    }

    if (strcmp(hostName, myHostPtr->hostName) == 0) {
        if (request == LIM_LOCK_MASTER) {
            limLock.on |= LIM_LOCK_STAT_MASTER;
            myHostPtr->status[0] |= LIM_LOCKEDM;
        } else {
            limLock.on &= ~LIM_LOCK_STAT_MASTER;
            myHostPtr->status[0] &= ~LIM_LOCKEDM;
        }
        return 0;
    }

    lockReq.on = request;
    lockReq.uid = getuid();

    if (getpwnam2(lockReq.lsfUserName) == NULL) {
        return -1;
    }

    lockReq.time = 0;

    if (callLim_(LIM_LOCK_HOST, &lockReq, xdr_limLock, NULL, NULL, NULL,
                 _USE_TCP_, NULL) < 0) {
        ls_syslog(LOG_ERR, "%s: %s(%s) failed: %m", "lockHost", "callLim",
                  hostName);
        return -2;
    }

    return 0;
}

void announce_master_register(struct clusterNode *cl)
{
    struct hostNode *h;
    XDR xdrs;
    char buf[LL_BUFSIZ_256];
    struct sockaddr_in to;

    struct master_register reg;
    memset(&reg, 0, sizeof(reg));
    strncpy(reg.cluster,  myClusterPtr->clName, sizeof(reg.cluster) - 1);
    strncpy(reg.hostname, myHostPtr->hostName, sizeof(reg.hostname) - 1);
    reg.host_num = myHostPtr->hostNo;
    reg.seqno    = masterAnnSeqNo++;
    reg.tcp_port = myHostPtr->statInfo.portno;

    struct packet_header hdr;
    init_pack_hdr(&hdr);
    hdr.operation = LIM_MASTER_REGISTER;
    hdr.sequence  = reg.seqno;

    xdrmem_create(&xdrs, buf, sizeof(buf), XDR_ENCODE);

    if (! xdr_pack_hdr(&xdrs, &hdr)) {
        LS_ERR("hdr encode failed");
        xdr_destroy(&xdrs);
        return;
    }

    if (! xdr_master_register(&xdrs, &reg)) {
        LS_ERR("master resigster encode failed");
        xdr_destroy(&xdrs);
        return;
    }

    for (h = cl->hostList; h; h = h->nextPtr) {

        if (h == myHostPtr)
            continue;

        memset(&to, 0, sizeof(to));
        to.sin_family = AF_INET;
        to.sin_port   = lim_udp_port;

        get_host_sinaddrv4(h->v4_epoint, &to);

        chan_send_dgram(lim_udp_chan,
                        buf,
                        XDR_GETPOS(&xdrs),
                        &to);
    }

    xdr_destroy(&xdrs);
}
void
master_register_recv(XDR *xdrs,
                     struct sockaddr_in *from,
                     struct packet_header *hdr)
{
    if (!limPortOk(from)) {
        LS_ERR("master_register: invalid source port");
        return;
    }

    struct master_register reg;
    if (!xdr_master_register(xdrs, &reg)) {
        LS_ERR("master_register: decode failed");
        return;
    }

    LS_DEBUG("master_register: from=%s host=%s hostNo=%u seq=%u port=%u",
             sockAdd2Str_(from),
             reg.hostname,
             reg.host_num,
             reg.seqno,
             reg.tcp_port);

    if (strcmp(reg.cluster, myClusterPtr->clName) != 0) {
        LS_WARNING("master_register: cluster mismatch %s (mine=%s)",
                   reg.cluster,
                   myClusterPtr->clName);
        return;
    }

    struct hostNode *hPtr;
    hPtr = find_node_by_cluster(myClusterPtr->hostList,
                                reg.hostname);

    if (hPtr == NULL) {
        LS_ERR("master_register: unknown host %s", reg.hostname);
        return;
    }

    if (hPtr->hostNo != reg.host_num) {
        LS_WARNING("master_register: hostNo mismatch for %s "
                   "(local=%d remote=%u)",
                   hPtr->hostName,
                   hPtr->hostNo,
                   reg.host_num);
    }

    /* If sender has lower hostNo, it wins */
    if (hPtr->hostNo < myHostPtr->hostNo) {

        if (!myClusterPtr->masterKnown
            || myClusterPtr->masterPtr != hPtr) {

            LS_INFO("new master is %s (hostNo=%d)",
                    hPtr->hostName,
                    hPtr->hostNo);
        }

        myClusterPtr->masterPtr = hPtr;
        myClusterPtr->masterKnown = 1;
        myClusterPtr->masterInactivityCount = 0;

        masterMe = 0;

        hPtr->lastSeqNo = reg.seqno;
        hPtr->statInfo.portno = reg.tcp_port;

        return;
    }

    /* If I have lower hostNo, I win */
    if (myHostPtr->hostNo < hPtr->hostNo) {

        if (masterMe == 0) {
            LS_INFO("Reasserting master role (hostNo=%d)",
                    myHostPtr->hostNo);
        }

        masterMe = 1;
        myClusterPtr->masterPtr = myHostPtr;
        myClusterPtr->masterKnown = 1;
        myClusterPtr->masterInactivityCount = 0;

        return;
    }

    /* Same hostNo case (should never happen) */
    LS_ERR("master_register: identical hostNo conflict %d",
           myHostPtr->hostNo);
}
