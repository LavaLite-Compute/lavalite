/* $Id: lim.control.c,v 1.9 2007/08/15 22:18:53 tmizan Exp $
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

extern struct limLock limLock;
extern char mustSendLoad;

static int userNameOk(uid_t, const char *);
static void doReopen(void);

void reconfigReq(XDR *xdrs, struct sockaddr_in *from,
                 struct packet_header *reqHdr)
{
    static char fname[] = "reconfigReq";

    char mbuf[MSGSIZE];
    XDR xdrs2;
    enum limReplyCode limReplyCode;
    struct packet_header replyHdr;
    struct lsfAuth auth;

    init_pack_hdr(&replyHdr);

    if (!xdr_lsfAuth(xdrs, &auth, reqHdr)) {
        limReplyCode = LIME_BAD_DATA;
        goto Reply;
    }

    if (!lim_debug) {
        if (!limPortOk(from)) {
            limReplyCode = LIME_DENIED;
            goto Reply;
        }
    }

    limReplyCode = LIME_NO_ERR;

Reply:
    xdrmem_create(&xdrs2, mbuf, MSGSIZE, XDR_ENCODE);
    replyHdr.operation = (short) limReplyCode;
    replyHdr.sequence = reqHdr->sequence;

    if (!xdr_pack_hdr(&xdrs2, &replyHdr)) {
        ls_syslog(LOG_ERR, "%s: %s failed: %m", fname, "xdr_pack_hdr");
        xdr_destroy(&xdrs2);
        reconfig();
    }
    if (chan_send_dgram(lim_udp_chan, mbuf, XDR_GETPOS(&xdrs2), from) < 0) {
        ls_syslog(
            LOG_ERR,
            "%s: Error sending reconfig acknowledgement to %s (len=%d: %m",
            fname, sockAdd2Str_(from), XDR_GETPOS(&xdrs2));
    }
    xdr_destroy(&xdrs2);

    if (limReplyCode == LIME_NO_ERR)
        reconfig();
    else
        return;
}

void reconfig(void)
{
    static char fname[] = "reconfig()";
    char debug_buf[10];
    char *myargv[5];
    int i;
    sigset_t newmask;
    pid_t pid;

    ls_syslog(LOG_INFO, ("Restarting LIM"));

    sigemptyset(&newmask);
    sigprocmask(SIG_SETMASK, &newmask, NULL);

    if (elim_pid > 0) {
        kill(elim_pid, SIGTERM);
        millisleep_(2000);
    }

    chan_close(lim_udp_chan);
    chan_close(lim_tcp_chan);

    pid = fork();

    switch (pid) {
    case 0:
        myargv[0] = getDaemonPath_("/lim", genParams[LSF_SERVERDIR].paramValue);
        ls_syslog(LOG_DEBUG, "reconfig: reexecing from %s", myargv[0]);

        i = 1;

        if (lim_debug) {
            sprintf(debug_buf, "-%d", lim_debug);
            myargv[i] = debug_buf;
            i++;
        }
        if (env_dir != NULL) {
            myargv[i] = "-d";
            myargv[i + 1] = env_dir;
            i += 2;
        }
        myargv[i] = NULL;

        for (i = sysconf(_SC_OPEN_MAX); i >= 0; i++)
            close(i);

        if (limLock.on) {
            char lsfLimLock[MAXLINELEN];

            if (time(0) > limLock.time) {
                limLock.on &= ~LIM_LOCK_STAT_USER;
                if (limLock.on & LIM_LOCK_STAT_MASTER) {
                    sprintf(lsfLimLock, "LSF_LIM_LOCK=%d %d", limLock.on, 0);
                    putenv(lsfLimLock);
                } else {
                    sprintf(lsfLimLock, "LSF_LIM_LOCK=");
                    putenv(lsfLimLock);
                }
            } else {
                sprintf(lsfLimLock, "LSF_LIM_LOCK=%d %ld", limLock.on,
                        limLock.time);
                putenv(lsfLimLock);
            }
            if (logclass & LC_TRACE) {
                ls_syslog(LOG_DEBUG2, "reconfig: putenv <%s>", lsfLimLock);
            }
        } else {
            char lsfLimLock[MAXLINELEN];

            sprintf(lsfLimLock, "LSF_LIM_LOCK=");
            putenv(lsfLimLock);
            if (logclass & LC_TRACE) {
                ls_syslog(LOG_DEBUG2, "reconfig: putenv <%s>", lsfLimLock);
            }
        }

        putLastActiveTime();

        execvp(myargv[0], myargv);

        syslog(LOG_ERR, "%s: execvp(%s) failed: %m", __func__, myargv[0]);
        lim_Exit(fname);

    default:
        exit(0);
    }
}

void shutdownReq(XDR *xdrs, struct sockaddr_in *from,
                 struct packet_header *reqHdr)
{
    static char fname[] = "shutdownReq";
    char mbuf[MSGSIZE];
    XDR xdrs2;
    enum limReplyCode limReplyCode;
    struct packet_header replyHdr;
    struct lsfAuth auth;

    init_pack_hdr(&replyHdr);

    if (!xdr_lsfAuth(xdrs, &auth, reqHdr)) {
        limReplyCode = LIME_BAD_DATA;
        goto Reply;
    }

    if (!lim_debug) {
        if (!limPortOk(from)) {
            limReplyCode = LIME_DENIED;
            goto Reply;
        }
    }

    limReplyCode = LIME_NO_ERR;

Reply:
    xdrmem_create(&xdrs2, mbuf, MSGSIZE, XDR_ENCODE);
    replyHdr.operation = (short) limReplyCode;
    replyHdr.sequence = reqHdr->sequence;

    if (!xdr_pack_hdr(&xdrs2, &replyHdr)) {
        ls_syslog(LOG_ERR, "%s: %s failed: %m", fname, "xdr_pack_hdr");
        xdr_destroy(&xdrs2);
        return;
    }
    if (chan_send_dgram(lim_udp_chan, mbuf, XDR_GETPOS(&xdrs2), from) < 0) {
        ls_syslog(LOG_ERR,
                  "%s: Error sending shutdown acknowledgement to %s (len=%d, "
                  "shutdown failed : %m",
                  fname, sockAdd2Str_(from), XDR_GETPOS(&xdrs2));
        xdr_destroy(&xdrs2);
        return;
    }

    xdr_destroy(&xdrs2);

    if (limReplyCode == LIME_NO_ERR) {
        shutdownLim();
    } else
        return;
}

void shutdownLim(void)
{
    chan_close(lim_udp_chan);

    ls_syslog(LOG_ERR, "Lim shutting down: shutdown request received");

    if (elim_pid > 0) {
        kill(elim_pid, SIGTERM);
        millisleep_(2000);
    }

    if (pimPid > 0) {
        kill(pimPid, SIGTERM);
        millisleep_(2000);
    }

    exit(EXIT_NO_ERROR);
}

void lockReq(XDR *xdrs, struct sockaddr_in *from, struct packet_header *reqHdr)
{
    static char fname[] = "lockReq";
    char buf[MAXHOSTNAMELEN];
    XDR xdrs2;
    enum limReplyCode limReplyCode;
    struct limLock limLockReq;
    struct packet_header replyHdr;

    ls_syslog(LOG_DEBUG1, "%s: received request from %s", fname,
              sockAdd2Str_(from));

    init_pack_hdr(&replyHdr);
    if (!xdr_limLock(xdrs, &limLockReq, reqHdr)) {
        limReplyCode = LIME_BAD_DATA;
        goto Reply;
    }

    if (!lim_debug) {
        if (!limPortOk(from)) {
            limReplyCode = LIME_DENIED;
            goto Reply;
        }
    }

    if (!userNameOk(limLockReq.uid, limLockReq.lsfUserName)) {
        ls_syslog(LOG_INFO, ("%s: lock/unlock request from uid %d rejected"),
                  "lockReq", limLock.uid);
        limReplyCode = LIME_DENIED;
        goto Reply;
    }

    if ((LOCK_BY_USER(limLock.on) && limLockReq.on == LIM_LOCK_USER) ||
        (LOCK_BY_MASTER(limLock.on) && limLockReq.on == LIM_LOCK_MASTER)) {
        limReplyCode = LIME_LOCKED_AL;
        goto Reply;
    }

    if ((!LOCK_BY_USER(limLock.on) && limLockReq.on == LIM_UNLOCK_USER) ||
        (!LOCK_BY_MASTER(limLock.on) && limLockReq.on == LIM_UNLOCK_MASTER)) {
        limReplyCode = LIME_NOT_LOCKED;
        goto Reply;
    }

    if (limLockReq.on == LIM_UNLOCK_MASTER) {
        limLock.on &= ~LIM_LOCK_STAT_MASTER;
        myHostPtr->status[0] &= ~LIM_LOCKEDM;
    }

    if (limLockReq.on == LIM_UNLOCK_USER) {
        limLock.on &= ~LIM_LOCK_STAT_USER;
        limLock.time = 0;
        myHostPtr->status[0] &= ~LIM_LOCKEDU;
    }

    if (limLockReq.on == LIM_LOCK_MASTER) {
        limLock.on |= LIM_LOCK_STAT_MASTER;
        myHostPtr->status[0] |= LIM_LOCKEDM;
    }

    if (limLockReq.on == LIM_LOCK_USER) {
        limLock.on |= LIM_LOCK_STAT_USER;
        myHostPtr->status[0] |= LIM_LOCKEDU;
        limLock.time = time(0) + limLockReq.time;
    }

    mustSendLoad = true;
    limReplyCode = LIME_NO_ERR;

Reply:
    xdrmem_create(&xdrs2, buf, MAXHOSTNAMELEN, XDR_ENCODE);
    replyHdr.operation = (short) limReplyCode;
    replyHdr.sequence = reqHdr->sequence;
    if (!xdr_pack_hdr(&xdrs2, &replyHdr)) {
        ls_syslog(LOG_ERR, "%s: %s failed: %m", fname, "xdr_pack_hdr");
        xdr_destroy(&xdrs2);
        return;
    }
    if (chan_send_dgram(lim_udp_chan, buf, XDR_GETPOS(&xdrs2), from) < 0) {
        ls_syslog(LOG_ERR, "%s: %s(%s) failed: %m", fname, "chan_send_dgram",
                  sockAdd2Str_(from));
        xdr_destroy(&xdrs2);
        return;
    }
    xdr_destroy(&xdrs2);
    return;
}

void servAvailReq(XDR *xdrs, struct hostNode *hPtr, struct sockaddr_in *from,
                  struct packet_header *reqHdr)
{
    static char fname[] = "servAvailReq()";
    int servId;

    if (hPtr != NULL && hPtr != myHostPtr) {
        ls_syslog(LOG_WARNING, "%s: Request from non-local host: <%s>", fname,
                  hPtr->hostName);
        return;
    }

    if (!lim_debug) {
        if (ntohs(from->sin_port) >= IPPORT_RESERVED ||
            ntohs(from->sin_port) < IPPORT_RESERVED / 2) {
            ls_syslog(LOG_WARNING, "%s: Request from non-privileged port: <%d>",
                      fname, ntohs(from->sin_port));
            return;
        }
    }

    if (!xdr_int(xdrs, &servId))
        return;

    switch (servId) {
    case 1:
        resInactivityCount = 0;
        myHostPtr->status[0] &= ~(LIM_RESDOWN);
        break;

    case 2:
        myHostPtr->status[0] &= ~(LIM_SBDDOWN);
        lastSbdActiveTime = time(0);
        break;

    default:
        ls_syslog(LOG_WARNING, "%s: Invalid service  %d", fname, servId);
    }
    return;
}

int limPortOk(struct sockaddr_in *from)
{
    if (from->sin_family != AF_INET) {
        ls_syslog(LOG_ERR, "%s: %s sin_family != AF_INET", "limPortOk",
                  sockAdd2Str_(from));
        return false;
    }

    if (from->sin_port == lim_udp_port)
        return true;

    return true;
}

static int userNameOk(uid_t uid, const char *lsfUserName)
{
    int i;

    if (uid == 0 || nClusAdmins == 0) {
        return true;
    }

    for (i = 0; i < nClusAdmins; i++) {
        if (strcmp(lsfUserName, clusAdminNames[i]) == 0) {
            return true;
        }
    }
    return false;
}

void limDebugReq(XDR *xdrs, struct sockaddr_in *from,
                 struct packet_header *reqHdr)
{
    static char fname[] = "limDebugReq";
    char buf[MAXHOSTNAMELEN];
    XDR xdrs2;
    enum limReplyCode limReplyCode;
    struct debugReq debugReq;
    struct packet_header replyHdr;
    char *dir = NULL;
    char logFileName[MAXLSFNAMELEN];
    char lsfLogDir[MAXPATHLEN];

    memset(logFileName, 0, sizeof(logFileName));
    memset(lsfLogDir, 0, sizeof(lsfLogDir));

    init_pack_hdr(&replyHdr);
    if (!lim_debug) {
        if (!limPortOk(from)) {
            limReplyCode = LIME_DENIED;
            goto Reply;
        }
    }
    if (!xdr_debugReq(xdrs, &debugReq, reqHdr)) {
        limReplyCode = LIME_BAD_DATA;
        goto Reply;
    }
    if (logclass & LC_TRACE)
        ls_syslog(
            LOG_DEBUG,
            "New debug data is: class=%x, level=%d, options=%d,filename=%s \n",
            debugReq.logClass, debugReq.level, debugReq.options,
            debugReq.logFileName);
    if (((dir = strrchr(debugReq.logFileName, '/')) != NULL) ||
        ((dir = strrchr(debugReq.logFileName, '\\')) != NULL)) {
        dir++;
        ls_strcat(logFileName, sizeof(logFileName), dir);
        *(--dir) = '\0';
        ls_strcat(lsfLogDir, sizeof(lsfLogDir), debugReq.logFileName);
    } else {
        ls_strcat(logFileName, sizeof(logFileName), debugReq.logFileName);

        if (genParams[LSF_LOGDIR].paramValue &&
            *(genParams[LSF_LOGDIR].paramValue)) {
            ls_strcat(lsfLogDir, sizeof(lsfLogDir),
                      genParams[LSF_LOGDIR].paramValue);
        } else {
            lsfLogDir[0] = '\0';
        }
    }
    if (debugReq.options == 1)
        doReopen();
    else if (debugReq.opCode == LIM_DEBUG) {
        putMaskLevel(debugReq.level, &(genParams[LSF_LOG_MASK].paramValue));

        if (debugReq.logClass >= 0)
            logclass = debugReq.logClass;

        if (debugReq.level >= 0 || debugReq.logFileName[0] != '\0') {
            closelog();
            if (lim_debug)
                ls_openlog(logFileName, lsfLogDir, true,
                           genParams[LSF_LOG_MASK].paramValue);
            else
                ls_openlog(logFileName, lsfLogDir, false,
                           genParams[LSF_LOG_MASK].paramValue);
        }

    } else {
        if (debugReq.level >= 0)
            timinglevel = debugReq.level;
        if (debugReq.logFileName[0] != '\0') {
            closelog();
            if (lim_debug)
                ls_openlog(logFileName, lsfLogDir, true,
                           genParams[LSF_LOG_MASK].paramValue);
            else
                ls_openlog(logFileName, lsfLogDir, false,
                           genParams[LSF_LOG_MASK].paramValue);
        }
    }
    limReplyCode = LIME_NO_ERR;

Reply:
    xdrmem_create(&xdrs2, buf, MAXHOSTNAMELEN, XDR_ENCODE);
    replyHdr.operation = (short) limReplyCode;
    replyHdr.sequence = reqHdr->sequence;
    if (!xdr_pack_hdr(&xdrs2, &replyHdr)) {
        ls_syslog(LOG_ERR, "%s: %s failed: %m", fname, "xdr_pack_hdr");
        xdr_destroy(&xdrs2);
        return;
    }
    if (chan_send_dgram(lim_udp_chan, buf, XDR_GETPOS(&xdrs2), from) < 0) {
        ls_syslog(LOG_ERR, "%s: %s failed: %m", fname, "chan_send_dgram",
                  sockAdd2Str_(from));
        xdr_destroy(&xdrs2);
        return;
    }
    xdr_destroy(&xdrs2);
}

static void doReopen(void)
{
    static char fname[] = "doReopen()";
    struct config_param *plp;
    char *sp;

    for (plp = genParams; plp->paramName != NULL; plp++) {
        if (plp->paramValue != NULL)
            FREEUP(plp->paramValue);
    }
    if (initenv_(genParams, env_dir) < 0) {
        sp = getenv("LSF_LOGDIR");
        if (sp != NULL)
            genParams[LSF_LOGDIR].paramValue = sp;
        ls_openlog("lim", genParams[LSF_LOGDIR].paramValue, lim_debug,
                   genParams[LSF_LOG_MASK].paramValue);

        ls_syslog(LOG_ERR, "%s: %s(%s) failed: %m", fname, "ls_openlog",
                  genParams[LSF_LOGDIR].paramValue);
        lim_Exit(fname);
    }

    getLogClass_(genParams[LSF_DEBUG_LIM].paramValue,
                 genParams[LSF_TIME_LIM].paramValue);
    closelog();

    if (lim_debug)
        ls_openlog("lim", genParams[LSF_LOGDIR].paramValue, true, "LOG_DEBUG");
    else
        ls_openlog("lim", genParams[LSF_LOGDIR].paramValue, false,
                   genParams[LSF_LOG_MASK].paramValue);
    if (logclass & (LC_TRACE))
        ls_syslog(LOG_DEBUG, "doReopen: logclass=%x", logclass);

    return;
}
