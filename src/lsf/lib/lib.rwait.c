/* $Id: lib.rwait.c,v 0.3 2007/08/15 22:18:51 tmizan Exp $
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
#include "lsf/lib/liblavalite.h"
#include "lsf/res/nios.h"

#define SIGEMT SIGBUS


static int rwait_(int tid, LS_WAIT_T *status, int block, struct rusage *ru);
static int readWaitReply(LS_WAIT_T *status, struct rusage *ru);
static void restartRWait(sigset_t);
static void usr1Handler(int);
int isPamBlockWait = 0;

    int
ls_rwait(LS_WAIT_T *status, int options, struct rusage *ru)
{
    return (rwait_(0, status, options, ru));
}


    int
ls_rwaittid(int tid, LS_WAIT_T *status, int options, struct rusage *ru)
{
    return (rwait_(tid, status, options, ru));
}

    static int
rwait_(int tid, LS_WAIT_T *status, int options, struct rusage *ru)
{
    int rpid;
    struct lslibNiosWaitReq req;
    struct lslibNiosHdr hdr;
    fd_set rmask;
    struct timeval timeout;
    sigset_t newMask, oldMask;
    int cc;

    if (tid < 0) {
        lserrno = LSE_BAD_ARGS;
        return -1;
    }

    if (!nios_ok_)  {
        lserrno = LSE_NORCHILD;
        return -1;
    }


    blockALL_SIGS_(&newMask, &oldMask);

Start:

    FD_ZERO(&rmask);
    FD_SET(cli_nios_fd[0], &rmask);
    timeout.tv_sec = NIOS_TIMEOUT;
    timeout.tv_usec = 0;

    SET_LSLIB_NIOS_HDR(req.hdr, LIB_NIOS_RWAIT, sizeof(req.r));
    req.r.options = options;
    req.r.tid = tid;

    if (b_write_fix(cli_nios_fd[0], (char *) &req, sizeof(req))
            != sizeof(req)) {
        lserrno = LSE_MSG_SYS;
        sigprocmask(SIG_SETMASK, &oldMask, NULL);
        return -1;
    }


    cc = select(cli_nios_fd[0] + 1, &rmask, 0, 0, &timeout);
    if (cc <= 0) {
        if (cc < 0)
            lserrno = LSE_SELECT_SYS;
        else
            lserrno = LSE_TIME_OUT;
        sigprocmask(SIG_SETMASK, &oldMask, NULL);
        return -1;
    }

    if (b_read_fix(cli_nios_fd[0], (char *) &hdr, sizeof(hdr)) == -1) {
        lserrno = LSE_MSG_SYS;
        sigprocmask(SIG_SETMASK, &oldMask, NULL);
        return -1;
    }

    if (WAIT_BLOCK(options) && hdr.opCode == NONB_RETRY) {

        restartRWait(oldMask);


        if(!isPamBlockWait){

            goto Start;
        }
    }

    switch (hdr.opCode) {
        case CHILD_FAIL:
            lserrno = LSE_NORCHILD;
            sigprocmask(SIG_SETMASK, &oldMask, NULL);
            return -1;

        case NONB_RETRY:
            sigprocmask(SIG_SETMASK, &oldMask, NULL);
            return 0;

        case CHILD_OK:
            rpid = readWaitReply(status, ru);
            sigprocmask(SIG_SETMASK, &oldMask, NULL);
            return rpid;

        default:

            lserrno = LSE_PROTOC_NIOS;
            sigprocmask(SIG_SETMASK, &oldMask, NULL);
            return -1;
    }

}

    static int
readWaitReply(LS_WAIT_T *status, struct rusage *ru)
{
    struct lslibNiosWaitReply reply;

    if (b_read_fix(cli_nios_fd[0], (char *) &reply.r, sizeof(reply.r))
            != sizeof(reply.r)) {
        lserrno = LSE_MSG_SYS;
        return -1;
    }
    (void) tid_remove(reply.r.pid);
    if (status)
        LS_STATUS(*status) = reply.r.status;
    if (ru)
        *ru = reply.r.ru;

    return (reply.r.pid);
}


    static void
restartRWait(sigset_t oldMask)
{
    int usr1handler = false;
    struct sigaction act, oact, usr1sigact;
    sigset_t pauseMask;



    sigaction(SIGUSR1, NULL, &oact);

    if (oact.sa_handler == SIG_ERR ||
#ifdef SIG_HOLD
            oact.sa_handler == SIG_HOLD ||
#endif
#ifdef SIG_CATCH
            oact.sa_handler == SIG_CATCH ||
#endif
            oact.sa_handler == SIG_IGN ||
            oact.sa_handler == SIG_DFL) {

        usr1handler = true;
        usr1sigact = oact;
        act.sa_handler = usr1Handler;
        sigfillset(&act.sa_mask);
        act.sa_flags = 0;
        sigaction(SIGUSR1, &act, NULL);
    }


    pauseMask = oldMask;
    sigdelset(&pauseMask, SIGUSR1);
    sigsuspend(&pauseMask);


    lserrno = LSE_SIG_SYS;


    if (usr1handler)
        sigaction(SIGUSR1, &usr1sigact, NULL);

}

    static void
usr1Handler (int sig)
{
}
