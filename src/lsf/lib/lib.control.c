/* $Id: lib.control.c,v 1.3 2007/08/15 22:18:50 tmizan Exp $
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

#include "lsf/lib/lib.h"
#include "lsf/lib/ll.sysenv.h"

static int
setLockOnOff_(int on, time_t duration, char *hname)
{
    struct limLock lockReq;
    char *host = hname;

    if (initenv_(NULL, NULL) <0)
        return -1;

    lockReq.on = on;

    lockReq.uid = getuid();

    if (getpwnam2(lockReq.lsfUserName) == NULL) {
        return -1;
    }

    if (duration == 0)
        lockReq.time = 77760000;
    else
        lockReq.time = duration;

    if (host == NULL)
        host = ls_getmyhostname();

    if (callLim_(LIM_LOCK_HOST, &lockReq, xdr_limLock, NULL, NULL,
                host, 0, NULL) < 0)
        return -1;

    return 0;
}

int
ls_limcontrol(char *hname, int operation)
{
    enum limReqCode limReqCode;
    struct lsfAuth auth;

    memset(&auth, 0, sizeof(struct lsfAuth));

    switch (operation) {
        case LIM_CMD_SHUTDOWN:
            limReqCode = LIM_SHUTDOWN;
            break;
        case LIM_CMD_REBOOT:
            limReqCode = LIM_REBOOT;
            break;
        default:
            lserrno = LSE_BAD_OPCODE;
            return -1;
    }

    putEauthClientEnvVar("user");
    putEauthServerEnvVar("lim");
    getAuth_(&auth, hname);

    if (callLim_(limReqCode, &auth, xdr_lsfAuth, NULL, NULL, hname, 0, NULL) < 0)
        return -1;

    return 0;

}

int
ls_lockhost(time_t duration)
{
    return setLockOnOff_(LIM_LOCK_USER, duration, NULL);
}

int
ls_unlockhost(void)
{
    return setLockOnOff_(LIM_UNLOCK_USER, 0, NULL);
}

int
lockHost_(time_t duration, char *hname)
{
    return setLockOnOff_(LIM_LOCK_USER, duration, hname);

}

int
unlockHost_(char *hname)
{
    return setLockOnOff_(LIM_UNLOCK_USER, 0, hname);

}

int
oneLimDebug(struct debugReq *pdebug, char *hostname)
{
    struct debugReq debugData;
    char *host = hostname;
    char space[ ]=" ";
    enum limReqCode limReqCode;

    limReqCode = LIM_DEBUGREQ;
    debugData.opCode = pdebug->opCode;
    debugData.logClass = pdebug->logClass;
    debugData.level    = pdebug->level;
    debugData.hostName = space;
    debugData.options  = pdebug->options;
    strcpy (debugData.logFileName, pdebug->logFileName);

    if (callLim_(limReqCode, &debugData, xdr_debugReq, NULL,
                NULL, host, 0, NULL) < 0)
        return -1;

    return 0;
}

int
ls_servavail(int servId, int nonblock)
{
    int options = 0;

    if (nonblock)
        options |= _NON_BLOCK_;

    if (initenv_(NULL, NULL) < 0)
        return -1;

    if (genParams[LSF_LIM_DEBUG].paramValue == NULL) {
        if (getuid() != 0) {
            lserrno = LSE_LIM_DENIED;
            return -1;
        }
    }

    if (callLim_(LIM_SERV_AVAIL,
                 &servId,
                 xdr_int,
                 NULL,
                 NULL,
                 ls_getmyhostname(),
                 options,
                 NULL) < 0)
        return -1;

    return 0;
}
