/* $Id: lim.misc.c,v 1.8 2007/08/15 22:18:54 tmizan Exp $
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

#ifdef MEAS
#include "../lib/lib.table.h"
#endif

static struct hostNode *findHNbyAddr(u_int);

void
lim_Exit(const char *fname)
{
    ls_syslog(LOG_ERR, "%s: Above fatal errors found.", fname);
    exit (EXIT_FATAL_ERROR);
}

int
equivHostAddr(struct hostNode *hPtr, u_int from)
{
    int i;
    for (i=0; i < hPtr->naddr; i++) {
        if (hPtr->addr[i] == from)
            return true;
    }
    return false;
}

#ifdef MEAS
void
timingLog(char action)
{
    static char fname[] = "timingLog";
    static int tstat_count;
    static int tlogcnt;
    static struct rusage beg_time, end_time;
    struct timeval timeofday;
    struct timeval tdiff1, tdiff2;
    char *asciiTime;
    extern char *ctime2();

    if (action == 0) {
        gettimeofday(&timeofday, (struct timezone *)0);
        asciiTime = ctime2(&timeofday.tv_sec);
        fprintf(timfp, "%s", asciiTime);
        fprintf(timfp,"Timing log record for LIM begins ...\n\n");
        fflush(timfp);
        tlogcnt = timingLogIntvl/exchIntvl;
        tstat_count = 0;
        getrusage(RUSAGE_SELF, &beg_time);
        return;
    }

    tstat_count++;
    if(tstat_count < tlogcnt) return;
    gettimeofday(&timeofday, (struct timezone *)0);
    asciiTime = ctime2(&timeofday.tv_sec);
    fprintf(timfp, "%s", asciiTime);
    fprintf(timfp, "Overhead time since last Log record is: ");
    getrusage(RUSAGE_SELF, &end_time);
    tvsub(&tdiff1,&end_time.ru_utime,&beg_time.ru_utime);
    tvsub(&tdiff2,&end_time.ru_stime, &beg_time.ru_stime);
    tvadd(&tdiff1, &tdiff2);
    fprintf(timfp, "%d microseconds.\n\n",
            tdiff1.tv_sec*1000000+tdiff1.tv_usec);
    fflush(timfp);
    tstat_count = 0;
    getrusage(RUSAGE_SELF, &beg_time);
    return;

}

static void
tvsub(struct timeval *tdiff, struct timeval *t1, struct timeval *t0)
{

    tdiff->tv_sec = t1->tv_sec - t0->tv_sec;
    tdiff->tv_usec = t1->tv_usec - t0->tv_usec;
    if (tdiff->tv_usec < 0)
        tdiff->tv_sec--, tdiff->tv_usec += 1000000;
}

static void
tvadd(struct timeval *tsum, struct timeval *t0)
{
    tsum->tv_sec += t0->tv_sec;
    tsum->tv_usec += t0->tv_usec;
    if (tsum->tv_usec > 1000000)
        tsum->tv_sec++, tsum->tv_usec -= 1000000;
}



void
loadLog(char action)
{
    static int lstat_count = 0;
    char *asciiTime;
    static int ldlogcnt = 0;
    struct timeval timeofday;

    gettimeofday(&timeofday, (struct timezone *)0);
    asciiTime = ctime2(&timeofday.tv_sec);

    if (action == 0) {
        ldlogcnt = loadLogIntvl/exchIntvl;
        lstat_count =0;
        sd_cnt = 0;
        rcv_cnt = 0;
        fprintf(loadfp, "%s", asciiTime);
        fprintf(loadfp, "Load log begins ...\n\n");
        fflush(loadfp);
        return;
    }

    lstat_count++;
    if (lstat_count < ldlogcnt)
        return;

    fprintf(loadfp, "%s", asciiTime);
    fprintf(loadfp,"RealRQL  Pg/s    SMkb/s    Logins   CPU_usage\n");
    fprintf(loadfp,"%-5.2f    %-6.2f   %-5.0f    %d       %-5.1f %%\n"
            ,realcla, smpages, smkbps, loginses, cpu_usage*100);
    fprintf(loadfp, "ld_send count: %d  ld_recv count %d  ",
            sd_cnt, rcv_cnt);
    if(masterMe) {
        fprintf(loadfp, " I am the master LIM\n\n");
    } else {
        fprintf(loadfp, " I am a slave LIM\n\n");
    }
    fflush(loadfp);
    sd_cnt = 0;
    rcv_cnt = 0;
    lstat_count = 0;
    return;
}

static hTab xferVector;

void
xferLog(char action, char *host)
{
    static xlogcnt = 0;
    static xstat_count = 0;
    int new;
    struct timeval timeofday;
    char *asciiTime;
    extern char *ctime2();
    int i,j;
    char **newPtr;
    hEnt *hashEntryPtr;
    sTab hashSearchPtr;
    int xfercnt;
    if (action == 0) {
        h_initTab(&xferVector, NUMSLOTS * RESETFACTOR);
        xlogcnt = xferLogIntvl/exchIntvl;
        xstat_count = 0;
        gettimeofday(&timeofday, (struct timezone *)0);
        asciiTime = ctime2(&timeofday.tv_sec);
        fprintf(xferfp, "%s", asciiTime);
        fprintf(xferfp, "Job xfer log begins ...\n\n");
        fflush(xferfp);
        return;
    }

    if(action == 1) {
        hashEntryPtr = h_addEnt(&xferVector, host, &new);
        if(new) {
            hashEntryPtr->hData = (int *) 1;
        } else {
            xfercnt =(int) hashEntryPtr->hData;
            xfercnt++;
            hashEntryPtr->hData = (int *) xfercnt;
        }
    }

    if(action == 2) {
        xstat_count++;
        if(xstat_count < xlogcnt) return;

        hashEntryPtr = h_firstEnt(&xferVector, &hashSearchPtr);
        gettimeofday(&timeofday, (struct timezone *)0);
        asciiTime = ctime2(&timeofday.tv_sec);
        fprintf(xferfp, "%s", asciiTime);
        if(!hashEntryPtr) {
            fprintf(xferfp, "%s --> ANYHOST: no jobs ever xferred \n\n",
                    myHostPtr->hostName);
            fprintf(xferfp, "=-=-=-=-=-=-=-=-=-=-=-=-=-=-=\n");
            fflush(xferfp);
            xstat_count = 0;
            return;
        }
        while(hashEntryPtr) {
            xfercnt = (int) hashEntryPtr->hData;
            fprintf(xferfp, "%s --> %s  Total: %d\n",
                    myHostPtr->hostName, hashEntryPtr->keyname, xfercnt);
            fprintf(xferfp, "\n");
            hashEntryPtr->hData = (int *) 0;
            hashEntryPtr = h_nextEnt(&hashSearchPtr);
        }
        fprintf(xferfp, "=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=\n");
        fflush(xferfp);
        xstat_count = 0;
        return;
    }

    if(action == 3) {
        xstat_count = xlogcnt;
        xferLog(2, NULL);
        h_delTab(&xferVector);
        fclose(xferfp);
        return;
    }
}

#endif

struct hostNode *
findHost(char *hostName)
{
    register struct hostNode *hPtr;

    if ((hPtr = findHostbyList(myClusterPtr->hostList, hostName)) != NULL)
        return hPtr;
    if ((hPtr = findHostbyList(myClusterPtr->clientList, hostName)) != NULL)
        return hPtr;

    return NULL;

}

struct hostNode *
findHostbyList(struct hostNode *hList, char *hostName)
{
    struct hostNode *hPtr;

    for (hPtr = hList; hPtr; hPtr = hPtr->nextPtr)
        if (equalHost_(hPtr->hostName, hostName))
            return hPtr;

    return((struct hostNode *)NULL);
}

struct hostNode *
findHostbyNo(struct hostNode *hList, int hostNo)
{
    register struct hostNode *hPtr;

    for (hPtr = hList; hPtr; hPtr = hPtr->nextPtr)
        if (hPtr->hostNo == hostNo)
            return hPtr;
    return NULL;
}

struct hostNode *
findHostbyAddr(struct sockaddr_in *from, char *fname)
{
    struct hostNode *hPtr;
    struct hostent *hp;
    u_int *tPtr;

    if (from->sin_addr.s_addr == ntohl(LOOP_ADDR))
        return myHostPtr;
    if ((hPtr = findHNbyAddr(*((u_int *) &from->sin_addr))))
        return hPtr;

    if ((hp = getHostEntryByAddr_(&from->sin_addr)) == NULL) {
        syslog(LOG_ERR, "%s: Host %s is unknown by %s",
               fname, sockAdd2Str_(from), myHostPtr->hostName);
        return NULL;
    }
    if ((hPtr = findHNbyAddr(*((u_int *) hp->h_addr))) == NULL) {
        syslog(LOG_ERR, "%s: Host %s (hp=%s/%s is unknown by configuration; all hosts used by LSF must have unique official names",
                  fname, sockAdd2Str_(from), hp->h_name,
                  inet_ntoa(*((struct in_addr *) hp->h_addr)));
        return NULL;
    }

    if ((tPtr = realloc(hPtr->addr,  (hPtr->naddr + 1) * sizeof(u_int))) == NULL) {
        return hPtr;
    }

    hPtr->addr = tPtr;
    hPtr->addr[hPtr->naddr] = (u_int) from->sin_addr.s_addr;
    hPtr->naddr++;

    return hPtr;
}

static struct hostNode *
findHNbyAddr(u_int from)
{
    struct clusterNode *clPtr;
    struct hostNode *hPtr;

    clPtr = myClusterPtr;
    for (hPtr = clPtr->hostList; hPtr; hPtr = hPtr->nextPtr) {
        if (equivHostAddr(hPtr, from))
            return hPtr;
    }
    for (hPtr = clPtr->clientList; hPtr; hPtr = hPtr->nextPtr) {
        if (equivHostAddr(hPtr, from))
            return hPtr;
    }

    return NULL;
}

bool_t
findHostInCluster(char *hostname)
{

    if (strcmp(myClusterPtr->clName, hostname) == 0)
        return true;
    if (findHostbyList(myClusterPtr->hostList, hostname) != NULL ||
            findHostbyList(myClusterPtr->clientList, hostname) !=NULL)
        return true;
    return false;
}

int
definedSharedResource(struct hostNode *host, struct lsInfo *allInfo)
{
    int i, j;
    char *resName;
    for (i = 0; i < host->numInstances; i++) {
        resName = host->instances[i]->resName;
        for (j = 0; j < allInfo->nRes; j++) {
            if (strcmp(resName, allInfo->resTable[j].name) == 0) {
                if (allInfo->resTable[j].flags &  RESF_SHARED)
                    return true;
            }
        }
    }
    return false;
}

struct shortLsInfo *
shortLsInfoDup(struct shortLsInfo *src)
{
    char                   *memp;
    char                   *currp;
    int                    i;
    struct shortLsInfo     *shortLInfo;

    if (src->nRes == 0 && src->nTypes == 0 && src->nModels == 0)
        return NULL;

    shortLInfo = calloc(1, sizeof(struct shortLsInfo));
    if (shortLInfo == NULL)
        return NULL;

    shortLInfo->nRes = src->nRes;
    shortLInfo->nTypes = src->nTypes;
    shortLInfo->nModels = src->nModels;

    memp = malloc((src->nRes + src->nTypes + src->nModels) * MAXLSFNAMELEN +
            src->nRes * sizeof (char *));

    if (!memp) {
        FREEUP(shortLInfo);
        return NULL;
    }

    currp = memp;
    shortLInfo->resName = (char **)currp;
    currp += shortLInfo->nRes * sizeof (char *);
    for (i = 0; i < src->nRes; i++, currp += MAXLSFNAMELEN)
        shortLInfo->resName[i] = currp;
    for (i = 0; i < src->nTypes; i++, currp += MAXLSFNAMELEN)
        shortLInfo->hostTypes[i] = currp;
    for (i = 0; i < src->nModels; i++, currp += MAXLSFNAMELEN)
        shortLInfo->hostModels[i] = currp;

    for(i = 0; i < src->nRes; i++) {
        strcpy(shortLInfo->resName[i], src->resName[i]);
    }

    for(i = 0; i < src->nTypes; i++) {
        strcpy(shortLInfo->hostTypes[i], src->hostTypes[i]);
    }

    for(i = 0; i < src->nModels; i++) {
        strcpy(shortLInfo->hostModels[i], src->hostModels[i]);
    }

    for(i=0; i < src->nModels; i++)
        shortLInfo->cpuFactors[i] = src->cpuFactors[i];

    return shortLInfo;
}

void
shortLsInfoDestroy(struct shortLsInfo *shortLInfo)
{
    char *allocBase;

    allocBase = (char *)shortLInfo->resName;

    FREEUP(allocBase);
    FREEUP(shortLInfo);
}

int
getHostNodeIPAddr(const struct hostNode* hPtr,struct sockaddr_in* toAddr)
{
    memcpy((void *) &toAddr->sin_addr,
            (const void *) &hPtr->addr[0],
            sizeof(u_int));
    return true;

}
