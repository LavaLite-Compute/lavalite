/* $Id: lib.nios.c,v 1.3 2007/08/15 22:18:51 tmizan Exp $
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
#include "lsf/res/nios.h"

#define SIGEMT SIGBUS

int
ls_stdinmode(int onoff)
{
    fd_set rmask;
    struct timeval timeout;
    struct lslibNiosHdr reqHdr;;
    struct lslibNiosHdr replyHdr;
    sigset_t newMask, oldMask;
    int cc;

    if (!nios_ok_) {
        lserrno = LSE_NIOS_DOWN;
        return -1;
    }

    if (blockALL_SIGS_(&newMask, &oldMask) < 0)
        return -1;

    SET_LSLIB_NIOS_HDR(reqHdr,
                       (onoff ? LIB_NIOS_REM_ON : LIB_NIOS_REM_OFF), 0);

    FD_ZERO(&rmask);
    FD_SET(cli_nios_fd[0], &rmask);
    timeout.tv_sec = NIOS_TIMEOUT;
    timeout.tv_usec = 0;

    if (b_write_fix(cli_nios_fd[0], (char *) &reqHdr, sizeof(reqHdr)) !=
        sizeof(reqHdr)) {
        lserrno = LSE_MSG_SYS;
        sigprocmask(SIG_SETMASK, &oldMask, NULL);
        return -1;
    }


    if ((cc = select(cli_nios_fd[0] + 1, &rmask, 0, 0, &timeout)) <= 0) {
        if (cc == 0)
            lserrno = LSE_TIME_OUT;
        else
            lserrno = LSE_SELECT_SYS;
        sigprocmask(SIG_SETMASK, &oldMask, NULL);
        return -1;
    }

    if (b_read_fix(cli_nios_fd[0], (char *) &replyHdr, sizeof(replyHdr))
        == -1) {
        lserrno = LSE_MSG_SYS;
        sigprocmask(SIG_SETMASK, &oldMask, NULL);
        return -1;
    }

    if (replyHdr.opCode != REM_ONOFF) {
        lserrno = LSE_PROTOC_NIOS;
        sigprocmask(SIG_SETMASK, &oldMask, NULL);
        return -1;
    }

    sigprocmask(SIG_SETMASK, &oldMask, NULL);
    return 0;
}

int
ls_donerex(void)
{
    fd_set rmask;
    struct timeval timeout;
    struct lslibNiosHdr reqHdr, replyHdr;
    sigset_t newMask, oldMask;

    if (!nios_ok_) {
        lserrno = LSE_NIOS_DOWN;
        return -1;
    }

    if (blockALL_SIGS_(&newMask, &oldMask) < 0)
        return -1;

    SET_LSLIB_NIOS_HDR(reqHdr, LIB_NIOS_EXIT, 0);

    FD_ZERO(&rmask);
    FD_SET(cli_nios_fd[0], &rmask);
    timeout.tv_sec = NIOS_TIMEOUT;
    timeout.tv_usec = 0;

    if (b_write_fix(cli_nios_fd[0], (char *)&reqHdr, sizeof(reqHdr)) !=
        sizeof(reqHdr)) {
        lserrno = LSE_MSG_SYS;
        sigprocmask(SIG_SETMASK, &oldMask, NULL);
        return -1;
    }


    if (select(cli_nios_fd[0] + 1, &rmask, 0, 0, &timeout) <= 0) {
        lserrno = LSE_SELECT_SYS;
        sigprocmask(SIG_SETMASK, &oldMask, NULL);
        return -1;
    }

    if (b_read_fix(cli_nios_fd[0], (char *) &replyHdr, sizeof(replyHdr))
        == -1) {
        lserrno = LSE_MSG_SYS;
        sigprocmask(SIG_SETMASK, &oldMask, NULL);
        return -1;
    }

    if (replyHdr.opCode != NIOS_OK) {
        lserrno = LSE_PROTOC_NIOS;
        sigprocmask(SIG_SETMASK, &oldMask, NULL);
        return -1;
    }

    nios_ok_ = false;

    sigprocmask(SIG_SETMASK, &oldMask, NULL);
    return 0;
}

int
ls_stoprex(void)
{
    fd_set rmask;
    struct timeval timeout;
    struct lslibNiosHdr reqHdr, replyHdr;
    sigset_t newMask, oldMask;

    if (!nios_ok_) {
        lserrno = LSE_NIOS_DOWN;
        return -1;
    }

    if (blockALL_SIGS_(&newMask, &oldMask) < 0)
        return -1;

    FD_ZERO(&rmask);
    FD_SET(cli_nios_fd[0], &rmask);
    timeout.tv_sec = NIOS_TIMEOUT;
    timeout.tv_usec = 0;

    SET_LSLIB_NIOS_HDR(reqHdr, LIB_NIOS_SUSPEND, 0);

    if (b_write_fix(cli_nios_fd[0], (char *)&reqHdr, sizeof(reqHdr)) !=
        sizeof(reqHdr)) {
        lserrno = LSE_MSG_SYS;
        sigprocmask(SIG_SETMASK, &oldMask, NULL);
        return -1;
    }


    if (select(cli_nios_fd[0] + 1, &rmask, 0, 0, &timeout) <= 0) {
        lserrno = LSE_SELECT_SYS;
        sigprocmask(SIG_SETMASK, &oldMask, NULL);
        return -1;
    }

    if (b_read_fix(cli_nios_fd[0], (char *) &replyHdr, sizeof(replyHdr))
        == -1) {
        lserrno = LSE_MSG_SYS;
        sigprocmask(SIG_SETMASK, &oldMask, NULL);
        return -1;
    }

    if (replyHdr.opCode != NIOS_OK) {
        lserrno = LSE_PROTOC_NIOS;
        sigprocmask(SIG_SETMASK, &oldMask, NULL);
        return -1;
    }

    sigprocmask(SIG_SETMASK, &oldMask, NULL);
    return 0;
}

int
ls_niossync(int numTasks)
{
    fd_set rmask;
    struct timeval timeout;
    struct lslibNiosHdr replyHdr, reqHdr;
    sigset_t newMask, oldMask;

    if (!nios_ok_) {
        lserrno = LSE_NIOS_DOWN;
        return -1;
    }

    if (numTasks < 0) {
        lserrno = LSE_BAD_ARGS;
        return -1;
    }

    if (blockALL_SIGS_(&newMask, &oldMask) < 0)
        return -1;

    FD_ZERO(&rmask);
    FD_SET(cli_nios_fd[0], &rmask);
    timeout.tv_sec = NIOS_TIMEOUT;
    timeout.tv_usec = 0;

    SET_LSLIB_NIOS_HDR(reqHdr, LIB_NIOS_SYNC, numTasks);

    if (b_write_fix(cli_nios_fd[0], (char *)&reqHdr, sizeof(reqHdr))
        != sizeof(reqHdr)) {
        lserrno = LSE_MSG_SYS;
        sigprocmask(SIG_SETMASK, &oldMask, NULL);
        return -1;
    }


    if (select(cli_nios_fd[0] + 1, &rmask, 0, 0, &timeout) <= 0) {
        lserrno = LSE_SELECT_SYS;
        sigprocmask(SIG_SETMASK, &oldMask, NULL);
        return -1;
    }

    if (b_read_fix(cli_nios_fd[0], (char *) &replyHdr, sizeof(replyHdr))
        == -1) {
        lserrno = LSE_MSG_SYS;
        sigprocmask(SIG_SETMASK, &oldMask, NULL);
        return -1;
    }
    switch(replyHdr.opCode) {
    case SYNC_FAIL:
        lserrno = LSE_SETPARAM;
        sigprocmask(SIG_SETMASK, &oldMask, NULL);
        return -1;
    case SYNC_OK:
        break;
    default:
        lserrno = LSE_PROTOC_NIOS;
        sigprocmask(SIG_SETMASK, &oldMask, NULL);
        return -1;
    }

    sigprocmask(SIG_SETMASK, &oldMask, NULL);
    return 0;
}

int
ls_setstdout(int on, char *format)
{
    fd_set rmask;
    struct timeval timeout;
    struct lslibNiosStdout req;
    struct lslibNiosHdr replyHdr;
    sigset_t newMask, oldMask;

    if (!nios_ok_) {
        lserrno = LSE_NIOS_DOWN;
        return -1;
    }

    if (blockALL_SIGS_(&newMask, &oldMask) < 0)
        return -1;

    FD_ZERO(&rmask);
    FD_SET(cli_nios_fd[0], &rmask);
    timeout.tv_sec = NIOS_TIMEOUT;
    timeout.tv_usec = 0;

    req.r.set_on = on;
    req.r.len = (format == NULL) ? 0 : strlen(format);
    if (req.r.len > 0)
        req.r.len++;

    SET_LSLIB_NIOS_HDR(req.hdr, LIB_NIOS_SETSTDOUT, sizeof(req.r) +
                       req.r.len*sizeof(char));

    if (b_write_fix(cli_nios_fd[0], (char *)&req, sizeof(req.hdr) +
                    sizeof(req.r)) != sizeof(req.hdr) + sizeof(req.r)) {
        lserrno = LSE_MSG_SYS;
        sigprocmask(SIG_SETMASK, &oldMask, NULL);
        return -1;
    }

    if (req.r.len > 0) {


        if (b_write_fix(cli_nios_fd[0], (char *)format, req.r.len*sizeof(char))
            != req.r.len * sizeof(char)) {
            lserrno = LSE_MSG_SYS;
            sigprocmask(SIG_SETMASK, &oldMask, NULL);
            return -1;
        }
    }


    if (select(cli_nios_fd[0] + 1, &rmask, 0, 0, &timeout) <= 0) {
        lserrno = LSE_SELECT_SYS;
        sigprocmask(SIG_SETMASK, &oldMask, NULL);
        return -1;
    }

    if (b_read_fix(cli_nios_fd[0], (char *) &replyHdr, sizeof(replyHdr))
        == -1) {
        lserrno = LSE_MSG_SYS;
        sigprocmask(SIG_SETMASK, &oldMask, NULL);
        return -1;
    }
    switch(replyHdr.opCode) {
    case STDOUT_FAIL:
        lserrno = LSE_SETPARAM;
        sigprocmask(SIG_SETMASK, &oldMask, NULL);
        return -1;
    case STDOUT_OK:
        break;
    default:
        lserrno = LSE_PROTOC_NIOS;
        sigprocmask(SIG_SETMASK, &oldMask, NULL);
        return -1;
    }

    sigprocmask(SIG_SETMASK, &oldMask, NULL);
    return 0;
}

int
ls_setstdin(int on, int *rpidlist, int len)
{
    fd_set rmask;
    struct timeval timeout;
    struct lslibNiosStdin req;
    struct lslibNiosHdr replyHdr;
    sigset_t newMask, oldMask;

    if (!nios_ok_) {
        lserrno = LSE_NIOS_DOWN;
        return -1;
    }

    if (blockALL_SIGS_(&newMask, &oldMask) < 0)
        return -1;

    if ((rpidlist == NULL && len != 0)
        || (len < 0) || (len > NOFILE)) {
        lserrno = LSE_SETPARAM;
        sigprocmask(SIG_SETMASK, &oldMask, NULL);
        return -1;
    }

    FD_ZERO(&rmask);
    FD_SET(cli_nios_fd[0], &rmask);
    timeout.tv_sec = NIOS_TIMEOUT;
    timeout.tv_usec = 0;

    SET_LSLIB_NIOS_HDR(req.hdr, LIB_NIOS_SETSTDIN, sizeof(req.r) +
                       len * sizeof(int));
    req.r.set_on = on;
    req.r.len = len;

    if (b_write_fix(cli_nios_fd[0], (char *)&req, sizeof(req.hdr) +
                    sizeof(req.r)) != sizeof(req.hdr) + sizeof(req.r)) {
        lserrno = LSE_MSG_SYS;
        sigprocmask(SIG_SETMASK, &oldMask, NULL);
        return -1;
    }

    if (rpidlist != NULL && len != 0) {


        if (b_write_fix(cli_nios_fd[0], (char *)rpidlist, len*sizeof(int))
            != len * sizeof(int)) {
            lserrno = LSE_MSG_SYS;
            sigprocmask(SIG_SETMASK, &oldMask, NULL);
            return -1;
        }
    }


    if (select(cli_nios_fd[0] + 1, &rmask, 0, 0, &timeout) <= 0) {
        lserrno = LSE_SELECT_SYS;
        sigprocmask(SIG_SETMASK, &oldMask, NULL);
        return -1;
    }

    if (b_read_fix(cli_nios_fd[0], (char *) &replyHdr, sizeof(replyHdr))
        == -1) {
        lserrno = LSE_MSG_SYS;
        sigprocmask(SIG_SETMASK, &oldMask, NULL);
        return -1;
    }
    switch(replyHdr.opCode) {
    case STDIN_FAIL:
        lserrno = LSE_SETPARAM;
        sigprocmask(SIG_SETMASK, &oldMask, NULL);
        return -1;
    case STDIN_OK:
        break;
    default:
        lserrno = LSE_PROTOC_NIOS;
        sigprocmask(SIG_SETMASK, &oldMask, NULL);
        return -1;
    }

    sigprocmask(SIG_SETMASK, &oldMask, NULL);
    return 0;
}

int
ls_getstdin(int on, int *rpidlist, int maxlen)
{
    fd_set rmask;
    struct timeval timeout;
    struct lslibNiosStdin req;
    struct lslibNiosGetStdinReply reply;
    sigset_t newMask, oldMask;

    if (!nios_ok_) {
        lserrno = LSE_NIOS_DOWN;
        return -1;
    }

    if (blockALL_SIGS_(&newMask, &oldMask) < 0)
        return -1;

    FD_ZERO(&rmask);
    FD_SET(cli_nios_fd[0], &rmask);
    timeout.tv_sec = NIOS_TIMEOUT;
    timeout.tv_usec = 0;

    SET_LSLIB_NIOS_HDR(req.hdr, LIB_NIOS_GETSTDIN, sizeof(req.r.set_on));
    req.r.set_on = on;

    if (b_write_fix(cli_nios_fd[0], (char *)&req, sizeof(req.hdr) +
                    sizeof(req.r.set_on)) != sizeof(req.hdr) +
        sizeof(req.r.set_on)) {
        lserrno = LSE_MSG_SYS;
        sigprocmask(SIG_SETMASK, &oldMask, NULL);
        return -1;
    }


    if (select(cli_nios_fd[0] + 1, &rmask, 0, 0, &timeout) <= 0) {
        lserrno = LSE_SELECT_SYS;
        sigprocmask(SIG_SETMASK, &oldMask, NULL);
        return -1;
    }

    if (b_read_fix(cli_nios_fd[0], (char *) &reply.hdr, sizeof(reply.hdr))
        == -1) {
        lserrno = LSE_MSG_SYS;
        sigprocmask(SIG_SETMASK, &oldMask, NULL);
        return -1;
    }
    switch(reply.hdr.opCode) {
    case STDIN_OK:
        break;
    default:
        lserrno = LSE_PROTOC_NIOS;
        sigprocmask(SIG_SETMASK, &oldMask, NULL);
        return -1;
    }

    if (reply.hdr.len)
        if (b_read_fix(cli_nios_fd[0], (char *)reply.rpidlist, reply.hdr.len)
            != reply.hdr.len) {
            lserrno = LSE_MSG_SYS;
            sigprocmask(SIG_SETMASK, &oldMask, NULL);
            return -1;
        }

    if (reply.hdr.len <= maxlen*sizeof(int)) {
        memcpy((char *)rpidlist, (char *)reply.rpidlist, reply.hdr.len);
        sigprocmask(SIG_SETMASK, &oldMask, NULL);
        return(reply.hdr.len/sizeof(int));
    } else {
        lserrno = LSE_RPIDLISTLEN;
        sigprocmask(SIG_SETMASK, &oldMask, NULL);
        return -1;
    }
}
