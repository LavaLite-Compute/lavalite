/* $Id: misc.c,v 1.8 2007/08/15 22:18:46 tmizan Exp $
 * Copyright (C) 2007 Platform Computing Inc
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

#include "lsbatch/daemons/daemons.h"

#define BATCH_SLAVE_PORT        40001
static char *reserved;

static int chuserId(uid_t);

extern struct listEntry * mkListHeader (void);
extern int shutdown (int, int);

void
die (int sig)
{
    char myhost[MAXHOSTNAMELEN];

    if (masterme) {
        releaseElogLock();
    }

    if (gethostname(myhost, MAXHOSTNAMELEN) <0) {
        ls_syslog(LOG_ERR, "%s", __func__, "gethostname", myhost);
        strcpy(myhost, "localhost");
    }

    if (sig > 0 && sig < 100) {
        ls_syslog(LOG_ERR, "Daemon on host <%s> received signal <%d>; exiting",
                  myhost, sig);
    } else {
        switch (sig) {
        case MASTER_RESIGN:
            ls_syslog(LOG_INFO, ("Master daemon on host <%s> resigned; exiting"), myhost);
            break;
        case MASTER_RECONFIG:
            ls_syslog(LOG_INFO, ("Master daemon on host <%s> exiting for reconfiguration"), myhost);
            break;

        case SLAVE_MEM:
            ls_syslog(LOG_ERR, "Slave daemon on host <%s> failed in memory allocation; fatal error - exiting", myhost);
            lsb_merr1("Slave daemon on host <%s> failed in memory allocation; fatal error - exiting", myhost);
            break;
        case MASTER_MEM:
            ls_syslog(LOG_ERR, "Master daemon on host <%s> failed in memory allocation; fatal error - exiting", myhost);
            break;
        case SLAVE_FATAL:
            ls_syslog(LOG_ERR, "Slave daemon on host <%s> dying; fatal error - see above messages for reason", myhost);
            break;
        case MASTER_FATAL:
            ls_syslog(LOG_ERR, "Master daemon on host <%s> dying; fatal error - see above messages for reason", myhost);
            break;
        case MASTER_CONF:
            ls_syslog(LOG_ERR, "Master daemon on host <%s> died of bad configuration file", myhost);
            break;
        case SLAVE_RESTART:
            ls_syslog(LOG_ERR, "Slave daemon on host <%s> restarting", myhost);
            break;
        case SLAVE_SHUTDOWN:
            ls_syslog(LOG_ERR, "Slave daemon on host <%s> shutdown", myhost);
            break;
        default:
            ls_syslog(LOG_ERR, "Daemon on host <%s> exiting; cause code <%d> unknown", myhost, sig);
            break;
        }
    }

    shutdown(chanSock_(batchSock), 2);

    exit(sig);
}

int
portok (struct sockaddr_in *from)
{
    if (from->sin_family != AF_INET) {
        syslog(LOG_ERR, "%s: sin_family(%d) != AF_INET(%d)",
               __func__, from->sin_family, AF_INET);
        return FALSE;
    }

    return TRUE;
}

int
get_ports(void)
{
    if (daemonParams[LSB_MBD_PORT].paramValue == NULL) {
        ls_syslog(LOG_ERR, "%s: LSB_MBD_PORT is not in lsf.conf", __func__);
        return -1;
    }

    mbd_port = atoi(daemonParams[LSB_MBD_PORT].paramValue);
    if (mbd_port <= 0) {
        ls_syslog(LOG_ERR, "%s: LSB_MBD_PORT <%s> must be a positive number",
                  __func__, daemonParams[LSB_MBD_PORT].paramValue);
        return -1;
    }
    mbd_port = htons(mbd_port);

    if (daemonParams[LSB_SBD_PORT].paramValue == NULL) {
        ls_syslog(LOG_ERR, "%s: LSB_SBD_PORT is not in lsf.conf", __func__);
        return -1;

    }

    sbd_port = atoi(daemonParams[LSB_SBD_PORT].paramValue);
    if (sbd_port <= 0) {
        ls_syslog(LOG_ERR, "%s: LSB_SBD_PORT <%s> must be a positive number",
                  __func__, daemonParams[LSB_SBD_PORT].paramValue);
        return -1;
    }
    sbd_port = htons(sbd_port);

    return 0;
}

uid_t
chuser(uid_t uid)
{
    uid_t myuid;
    int errnoSv = errno;

    // Bug handle mbd_debug and sbd_debug
    if (1)
        return(geteuid());

    if ((myuid = geteuid()) == uid)
        return myuid;

    if (myuid != 0 && uid != 0)
        chuserId(0);
    chuserId(uid);
    errno = errnoSv;
    return myuid;
}

static int
chuserId (uid_t uid)
{
    // Bug do nothing the mbatchd runs as administrator
    // the sbatch as root and should change get pid only the
    // the child while running pree/postxec or the jobs
#if 0
    static char fname[] = "chuserId";

    if (lsfSetEUid(uid) < 0) {
        ls_syslog(LOG_ERR, "%s", __func__, "setresuid/seteuid",
                  (int)uid);
        if (lsb_CheckMode) {
            lsb_CheckError = FATAL_ERR;
            return -1;
        } else
            die(MASTER_FATAL);
    }

    if (uid == 0) {
        if(lsfSetREUid(0, 0) < 0) {
            ls_syslog(LOG_ERR, "%s", __func__, "setresuid/setreuid",
                      (int)uid);
            if (lsb_CheckMode) {
                lsb_CheckError = FATAL_ERR;
                return -1;
            } else
                if (masterme)
                    die(MASTER_FATAL);
                else
                    die(SLAVE_FATAL);
        }
    }
#endif
    return 0;
}

char *
safeSave(char *str)
{
    char *sp;

    sp = putstr_(str);
    if (!sp) {
        if (masterme)
            die(MASTER_MEM);
        else
            die(SLAVE_MEM);
    }

    return sp;

}

char *
my_malloc(int len, char *fileName)
{
    static char fname[] = "my_malloc()";
    char *sp, *caller = fileName;

    if (caller == NULL)
        caller = "";

    if (!len) {
        len = 4;
        ls_syslog(LOG_ERR, "%s: %s- Zero bytes requested; allocating %d bytes",
                  fname, caller, len);
    }
    sp = malloc(len);
    if (!sp) {
        if (reserved) {
            free(reserved);
            reserved = NULL;
            return (my_malloc(len, caller));
        }

        ls_syslog(LOG_ERR, "%s", __func__, "malloc", caller, len);
        if (masterme)
            die(MASTER_FATAL);
        else
            relife();
    }

    if (! reserved)
        reserved = malloc(MSGSIZE);

    return sp;

}

char *
my_calloc(int nelem, int esize, char *fileName)
{
    static char fname[] = "my_calloc()";
    char *sp, *caller = fileName;

    if (caller == NULL)
        caller = "";

    if (!nelem || !esize) {
        nelem = 1;
        esize = 4;
        ls_syslog(LOG_ERR, "%s: %s- Zero bytes requested; allocating %d bytes",
                  fname, caller, esize);
    }

    sp = calloc(nelem, esize);
    if (!sp) {
        if (reserved) {
            FREEUP(reserved);
            return (my_calloc(nelem, esize, caller));
        }

        ls_syslog(LOG_ERR, "%s", __func__, "calloc",
                  caller, esize);
        if (masterme)
            die(MASTER_MEM);
        else
            relife();
    }

    if (! reserved)
        reserved = malloc(MSGSIZE);

    return sp;

}

void
daemon_doinit (void)
{

    if (! daemonParams[LSF_SERVERDIR].paramValue
        ||! daemonParams[LSB_SHAREDIR].paramValue) {
        syslog(LOG_ERR, "%s: One of the two following parameters "
               "are undefined: %s %s", __func__,
               daemonParams[LSF_SERVERDIR].paramName,
               daemonParams[LSB_SHAREDIR].paramName);
        if (masterme)
            die(MASTER_FATAL);
        else
            die(SLAVE_FATAL);
    }

    if (daemonParams[LSB_MAILTO].paramValue == NULL)
        daemonParams[LSB_MAILTO].paramValue = safeSave(DEFAULT_MAILTO);
    if (daemonParams[LSB_MAILPROG].paramValue == NULL)
        daemonParams[LSB_MAILPROG].paramValue = safeSave(DEFAULT_MAILPROG);
    if (daemonParams[LSB_CRDIR].paramValue == NULL)
        daemonParams[LSB_CRDIR].paramValue = safeSave(DEFAULT_CRDIR);
}

void
relife(void)
{
    int pid;
    char *margv[6];
    int i = 0;

    pid = fork();

    if (pid < 0)
        return;

    if (pid == 0) {
        sigset_t newmask;

        for (i=0; i< NOFILE; i++)
            close(i);
        millisleep_(3000);

        margv[0] = getDaemonPath_("/sbatchd", daemonParams[LSF_SERVERDIR].paramValue);

        i = 1;
        // Bug handle mbd_debug and sbd_debug
#if 0
        if (1) {
            margv[i] = my_malloc(MAXFILENAMELEN, "relife");
            sprintf(margv[i], "-%d", debug);
            i++;
        }
#endif
        if (env_dir != NULL) {
            margv[i] = "-d";
            i++;
            margv[i] = env_dir;
            i++;
        }
        margv[i] = NULL;
        sigemptyset(&newmask);
        sigprocmask(SIG_SETMASK, &newmask, NULL);
        /* clear signal mask */

        execve(margv[0], margv, environ);
        ls_syslog(LOG_ERR, "Cannot re-execute sbatchd: %m");
        lsb_mperr( "sbatchd died in an accident, failed in re-execute");
        exit(-1);
    }

    die(SLAVE_RESTART);
}

struct listEntry *
tmpListHeader (struct listEntry *listHeader)
{
    static struct listEntry *tmp = NULL;

    if (tmp == NULL)
        tmp = mkListHeader();

    tmp->forw = listHeader->forw;
    tmp->back = listHeader->back;
    listHeader->forw->back = tmp;
    listHeader->back->forw = tmp;
    listHeader->forw = listHeader;
    listHeader->back = listHeader;
    return tmp;

}

int
fileExist (char *file, int uid, struct hostent *hp)
{
    int pid;
    int fds[2], i;
    int answer;

    if (socketpair (AF_UNIX, SOCK_STREAM, 0, fds) < 0) {
        ls_syslog(LOG_ERR, "%s", __func__, "socketpair");
        return TRUE;
    }

    pid = fork();
    if (pid < 0) {
        ls_syslog(LOG_ERR, "%s", __func__, "fork");
        return TRUE;
    }

    if (pid > 0) {
        close(fds[1]);
        if (b_read_fix(fds[0], (char *) &answer, sizeof (int)) < 0) {
            ls_syslog(LOG_ERR, "%s", __func__, "read");
            answer = TRUE;
        }
        close(fds[0]);
        return answer;
    } else {
        close(fds[0]);
        if (seteuid (uid) < 0) {
            ls_syslog(LOG_ERR, "%s", __func__, "setuid",
                      uid);
            answer = TRUE;
            write(fds[1], (char *) &answer, sizeof (int));
            close(fds[1]);
            exit(0);
        }
        if ((i = myopen_(file, O_RDONLY, 0, hp)) < 0) {
            ls_syslog(LOG_INFO, "fileExist", "myopen_", file);
            answer = FALSE;
        } else {
            close (i);
            answer = TRUE;
        }
        write(fds[1], &answer, sizeof (int));
        close(fds[1]);
        exit(0);
    }

}

void
freeWeek (windows_t *week[])
{
    windows_t *wp, *wpp;
    int j;

    for (j = 0; j < 8; j++) {
        for (wp = week[j]; wp; wp = wpp) {
            wpp =  wp->nextwind;
            if (wp)
                free (wp);
        }
        week[j] = NULL;
    }

}

void
errorBack(int chan, int replyCode, struct sockaddr_in *from)
{
    struct packet_header replyHdr;
    XDR  xdrs;
    char errBuf[MSGSIZE/8];

    xdrmem_create(&xdrs, errBuf, MSGSIZE/8, XDR_ENCODE);

    initLSFHeader_(&replyHdr);

    replyHdr.operation = replyCode;
    io_block_(chanSock_(chan));
    if (xdr_encodeMsg (&xdrs, NULL, &replyHdr, NULL, 0, NULL)) {
        if (chanWrite_(chan, errBuf, XDR_GETPOS(&xdrs)) < 0)
            ls_syslog(LOG_ERR, "%s", __func__, "chanWrite_",
                      sockAdd2Str_(from));
    } else
        ls_syslog(LOG_ERR, "%s", __func__, "xdr_encodeMsg",
                  sockAdd2Str_(from));

    xdr_destroy(&xdrs);
    return;

}

void
scaleByFactor(int *h32, int *l32, float cpuFactor)
{
    double limit, tmp;

    if (*h32 == 0x7fffffff && *l32 == 0xffffffff)

        return;

    limit = *h32;
    limit *= (1<<16);
    limit *= (1<<16);
    limit += *l32;

    limit = limit/cpuFactor + 0.5;
    if (limit < 1.0)
        limit = 1.0;

    tmp = limit/(double)(1<<16);
    tmp = tmp/(double)(1<<16);
    *h32 = tmp;
    tmp = (double) (*h32) * (double) (1<<16);
    tmp *= (double) (1<<16);
    *l32 = limit - tmp;

    return;
}

struct tclLsInfo *
getTclLsInfo(void)
{
    static char fname[] = "getTclLsInfo";
    int resNo, i;

    if (tclLsInfo) {
        freeTclLsInfo(tclLsInfo, 0);
    }

    tclLsInfo = (struct tclLsInfo *)my_malloc(sizeof(struct tclLsInfo ), fname);
    tclLsInfo->numIndx = allLsInfo->numIndx;
    tclLsInfo->indexNames = (char **)my_malloc (allLsInfo->numIndx *
                                                sizeof (char *), fname);
    for (resNo = 0; resNo < allLsInfo->numIndx; resNo++)
        tclLsInfo->indexNames[resNo] = allLsInfo->resTable[resNo].name;

    tclLsInfo->nRes = 0;
    tclLsInfo->resName = (char **)my_malloc(allLsInfo->nRes *sizeof(char*),
                                            fname);
    tclLsInfo->stringResBitMaps =
        (int *) my_malloc (GET_INTNUM(allLsInfo->nRes) * sizeof (int), fname);
    tclLsInfo->numericResBitMaps =
        (int *) my_malloc (GET_INTNUM(allLsInfo->nRes) * sizeof (int), fname);

    for (i =0; i< GET_INTNUM(allLsInfo->nRes); i++) {
        tclLsInfo->stringResBitMaps[i] = 0;
        tclLsInfo->numericResBitMaps[i] = 0;
    }
    for (resNo = 0; resNo < allLsInfo->nRes; resNo++) {

        if ((allLsInfo->resTable[resNo].flags & RESF_BUILTIN)
            || ((allLsInfo->resTable[resNo].flags & RESF_DYNAMIC)
                && (allLsInfo->resTable[resNo].flags & RESF_GLOBAL)))
            continue;

        if (allLsInfo->resTable[resNo].valueType == LS_STRING)
            SET_BIT (tclLsInfo->nRes, tclLsInfo->stringResBitMaps);
        if (allLsInfo->resTable[resNo].valueType == LS_NUMERIC)
            SET_BIT (tclLsInfo->nRes, tclLsInfo->numericResBitMaps);
        tclLsInfo->resName[tclLsInfo->nRes++] = allLsInfo->resTable[resNo].name;
    }

    return tclLsInfo;

}

struct resVal *
checkThresholdCond (char *resReq)
{
    static char fname[] = "checkThresholdCond";
    struct resVal *resValPtr;

    resValPtr = (struct resVal *)my_malloc (sizeof (struct resVal),
                                            "checkThresholdCond");
    initResVal (resValPtr);
    if (parseResReq (resReq, resValPtr, allLsInfo, PR_SELECT)
        != PARSE_OK) {
        lsbFreeResVal (&resValPtr);
        if (logclass & (LC_EXEC) && resReq)
            ls_syslog(LOG_DEBUG1, "%s: parseResReq(%s) failed",
                      fname, resReq);
        return NULL;
    }
    return resValPtr;

}

int *
getResMaps(int nRes, char **resource)
{
    int i, *temp, resNo;

    if (nRes < 0)
        return NULL;

    temp = (int *) my_malloc (GET_INTNUM(allLsInfo->nRes) * sizeof (int),
                              "getResMaps");

    for (i = 0; i < GET_INTNUM(allLsInfo->nRes); i++)
        temp[i] = 0;

    for (i = 0; i < nRes; i++) {
        for (resNo = 0; resNo < tclLsInfo->nRes; resNo++)
            if (!strcmp(resource[i], tclLsInfo->resName[resNo]))
                break;
        if (resNo < allLsInfo->nRes) {
            SET_BIT(resNo, temp);
        }
    }
    return temp;

}

int
checkResumeByLoad (LS_LONG_INT jobId, int num, struct thresholds thresholds,
                   struct hostLoad *loads, int *reason, int *subreasons, int jAttrib,
                   struct resVal *resumeCondVal, struct tclHostData *tclHostData)
{
    static char fname[] = "checkResumeByLoad";
    int i, j;
    int resume = TRUE;
    int lastReason = *reason;

    if (logclass & (LC_SCHED | LC_EXEC))
        ls_syslog(LOG_DEBUG3, "%s: reason=%x, subreasons=%d, numHosts=%d", fname, *reason, *subreasons, thresholds.nThresholds);

    if (num <= 0)
        return FALSE;

    for (j = 0; j < num; j++) {
        if (loads[j].li == NULL)
            continue;

        if (((*reason & SUSP_PG_IT)
             || ((*reason & SUSP_LOAD_REASON) && (*subreasons) == PG))
            && loads[j].li[IT] < pgSuspIdleT / 60
            && thresholds.loadSched[j][PG] != INFINIT_LOAD) {
            resume = FALSE;
            *reason = SUSP_PG_IT;
            *subreasons = 0;
        }
        else if (LS_ISUNAVAIL (loads[j].status)) {
            resume = FALSE;
            *reason = SUSP_LOAD_UNAVAIL;
        }
        else if (LS_ISLOCKEDU (loads[j].status)
                 && !(jAttrib & Q_ATTRIB_EXCLUSIVE)) {
            resume = FALSE;
            *reason = SUSP_HOST_LOCK;
        } else if (LS_ISLOCKEDM (loads[j].status)) {
            resume = FALSE;
            *reason = SUSP_HOST_LOCK_MASTER;
        }

        if (!resume) {
            if (logclass & (LC_SCHED | LC_EXEC))
                ls_syslog(LOG_DEBUG2, "%s: Can't resume job %s; *reason=%x",
                          fname, lsb_jobid2str(jobId), *reason);
            if (lastReason & SUSP_MBD_LOCK)
                *reason |= SUSP_MBD_LOCK;
            return FALSE;
        }

        if (resumeCondVal != NULL) {
            if (evalResReq (resumeCondVal->selectStr,
                            &tclHostData[j], DFT_FROMTYPE) == 1) {
                resume = TRUE;
                break;
            } else {
                resume = FALSE;
                *reason = SUSP_QUE_RESUME_COND;
                if ((logclass & (LC_SCHED | LC_EXEC)) && !resume)
                    ls_syslog(LOG_DEBUG2, "%s: Can't resume job %s; reason=%x",
                              fname, lsb_jobid2str(jobId), *reason);
                if (lastReason & SUSP_MBD_LOCK)
                    *reason |= SUSP_MBD_LOCK;
                return FALSE;
            }
        }

        if (loads[j].li[R15M] > thresholds.loadSched[j][R15M]) {
            resume = FALSE;
            *reason = SUSP_LOAD_REASON;
            *subreasons = R15M;
        }
        else if (loads[j].li[R1M] > thresholds.loadSched[j][R1M]) {
            resume = FALSE;
            *reason = SUSP_LOAD_REASON;
            *subreasons = R1M;
        }
        else if (loads[j].li[R15S] > thresholds.loadSched[j][R15S]) {
            resume = FALSE;
            *reason = SUSP_LOAD_REASON;
            *subreasons = R15S;
        }
        else if (loads[j].li[UT] > thresholds.loadSched[j][UT]) {
            resume = FALSE;
            *reason = SUSP_LOAD_REASON;
            *subreasons = UT;
        }
        else if (loads[j].li[PG] > thresholds.loadSched[j][PG]) {
            resume = FALSE;
            *reason = SUSP_LOAD_REASON;
            *subreasons = PG;
        }
        else if (loads[j].li[IO] > thresholds.loadSched[j][IO]) {
            resume = FALSE;
            *reason = SUSP_LOAD_REASON;
            *subreasons = IO;
        }
        else if (loads[j].li[LS] > thresholds.loadSched[j][LS]) {
            resume = FALSE;
            *reason = SUSP_LOAD_REASON;
            *subreasons = LS;
        }
        else if (loads[j].li[IT] < thresholds.loadSched[j][IT]) {
            resume = FALSE;
            *reason = SUSP_LOAD_REASON;
            *subreasons = IT;
        }
        else if (loads[j].li[MEM] < thresholds.loadSched[j][MEM]) {
            resume = FALSE;
            *reason = SUSP_LOAD_REASON;
            *subreasons = MEM;
        }

        else if (loads[j].li[TMP] < thresholds.loadSched[j][TMP]) {
            resume = FALSE;
            *reason = SUSP_LOAD_REASON;
            *subreasons = TMP;
        }
        else if (loads[j].li[SWP] < thresholds.loadSched[j][SWP]) {
            resume = FALSE;
            *reason = SUSP_LOAD_REASON;
            *subreasons = SWP;
        }
        for (i = MEM + 1; resume &&
                 i < MIN(thresholds.nIdx, allLsInfo->numIndx);
             i++) {
            if (loads[j].li[i] >= INFINIT_LOAD
                || loads[j].li[i] <= -INFINIT_LOAD
                || thresholds.loadSched[j][i] >= INFINIT_LOAD
                || thresholds.loadSched[j][i] <= -INFINIT_LOAD)
                continue;

            if (allLsInfo->resTable[i].orderType == INCR)  {
                if (loads[j].li[i] > thresholds.loadSched[j][i]) {
                    resume = FALSE;
                    *reason = SUSP_LOAD_REASON;
                    *subreasons = i;
                }
            } else {
                if (loads[j].li[i] < thresholds.loadSched[j][i]) {
                    resume = FALSE;
                    *reason = SUSP_LOAD_REASON;
                    *subreasons = i;
                }
            }
        }
    }
    if (lastReason & SUSP_MBD_LOCK)
        *reason |= SUSP_MBD_LOCK;

    if ((logclass & (LC_SCHED | LC_EXEC)) && !resume)
        ls_syslog(LOG_DEBUG2, "%s: Can't resume job %s; reason=%x, subreasons=%d", fname, lsb_jobid2str(jobId), *reason, *subreasons);

    return resume;

}

void
closeExceptFD(int except_)
{
    int i;

    for (i = sysconf(_SC_OPEN_MAX) - 1; i >= 3 ; i--) {
        if (i != except_)
            close(i);
    }
}

void
freeLsfHostInfo (struct hostInfo  *hostInfo, int num)
{
    int i, j;

    if (hostInfo == NULL || num < 0)
        return;

    for (i = 0; i < num; i++) {
        if (hostInfo[i].resources != NULL) {
            for (j = 0; j < hostInfo[i].nRes; j++)
                FREEUP (hostInfo[i].resources[j]);
            FREEUP (hostInfo[i].resources);
        }
        FREEUP (hostInfo[i].hostType);
        FREEUP (hostInfo[i].hostModel);
    }

}

void
copyLsfHostInfo (struct hostInfo *to, struct hostInfo *from)
{
    int i;

    strcpy (to->hostName, from->hostName);
    to->hostType = safeSave (from->hostType);
    to->hostModel = safeSave (from->hostModel);
    to->cpuFactor = from->cpuFactor;
    to->maxCpus = from->maxCpus;
    to->maxMem = from->maxMem;
    to->maxSwap = from->maxSwap;
    to->maxTmp = from->maxTmp;
    to->nDisks = from->nDisks;
    to->nRes = from->nRes;
    if (from->nRes > 0) {
        to->resources = (char **) my_malloc (from->nRes * sizeof (char *),
                                             "copyLsfHostInfo");
        for (i = 0; i < from->nRes; i++)
            to->resources[i] = safeSave (from->resources[i]);
    } else
        to->resources = NULL;

    to->isServer = from->isServer;
    to->rexPriority = from->rexPriority;

}

void
freeTclHostData (struct tclHostData *tclHostData)
{

    if (tclHostData == NULL)
        return;
    FREEUP (tclHostData->resPairs);
    FREEUP (tclHostData->loadIndex);

}

void
lsbFreeResVal (struct resVal **resVal)
{

    if (resVal == NULL || *resVal == NULL)
        return;
    freeResVal (*resVal);
    FREEUP (*resVal);
}
