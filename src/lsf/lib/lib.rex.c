/* $Id: lib.rex.c,v 1.3 2007/08/15 22:18:51 tmizan Exp $
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

extern int currentSN;

int
ls_rexecve(char *host, char **argv, int options, char **envp)
{
    register int d;
    int  retsock;
    struct timeval timeout;
    struct sockaddr_in sin;
    int max;
    char sock_buf[20];
    char *new_argv[5];
    char pathbuf[MAXPATHLEN];
    int s, descriptor[2];
    struct resCmdBill cmdmsg;
    int resTimeout;

    if (genParams_[LSF_RES_TIMEOUT].paramValue)
        resTimeout = atoi(genParams_[LSF_RES_TIMEOUT].paramValue);
    else
        resTimeout = RES_TIMEOUT;

    if (_isconnected_(host, descriptor))
        s = descriptor[0];
    else if ((s = ls_connect(host)) < 0)
        return -1;

    if (!FD_ISSET(s,&connection_ok_)){
        FD_SET(s,&connection_ok_);
        if (ackReturnCode_(s) < 0) {
            closesocket(s);
            _lostconnection_(host);
            return -1;
        }
    }

    cmdmsg.options = options & ~REXF_TASKPORT;
    if (cmdmsg.options & REXF_SHMODE)
        cmdmsg.options |= REXF_USEPTY;

    if (!isatty(0) && !isatty(1))
        cmdmsg.options &= ~REXF_USEPTY;
    else if ( cmdmsg.options & REXF_USEPTY ) {
        if (rstty_(host) < 0) {

            _lostconnection_(host);
            return -1;
        }
    }

    if ((genParams_[LSF_INTERACTIVE_STDERR].paramValue != NULL)
        && (strcasecmp(genParams_[LSF_INTERACTIVE_STDERR].paramValue,
                       "y") == 0) ) {
        cmdmsg.options |= REXF_STDERR;
    }

    if (mygetwd_(cmdmsg.cwd) == 0) {
        closesocket(s);
        _lostconnection_(host);
        lserrno = LSE_WDIR;
        return -1;
    }

    if (envp) {
        if (ls_rsetenv(host, envp) < 0) {
            _lostconnection_(host);
            return -1;
        }
    }

    if ((retsock = TcpCreate_(true, 0)) < 0) {
        closesocket(s);
        _lostconnection_(host);
        return -1;
    }

    socklen_t len = sizeof(sin);
    if (getsockname (retsock, (struct sockaddr *) &sin, &len) < 0) {
        (void)closesocket(retsock);
        closesocket(s);
        _lostconnection_(host);
        lserrno = LSE_SOCK_SYS;
        return -1;
    }

    cmdmsg.retport = sin.sin_port;

    cmdmsg.rpid = 0;
    cmdmsg.argv = argv;
    cmdmsg.priority = 0;

    timeout.tv_usec = 0;
    timeout.tv_sec  = resTimeout;
    if (sendCmdBill_(s, (resCmd) RES_EXEC, &cmdmsg, &retsock, &timeout)
        == -1) {
        closesocket(retsock);
        closesocket(s);
        _lostconnection_(host);
        return -1;
    }

    (void) sprintf (sock_buf, "%d", retsock);

    if (initenv_(NULL, NULL) <0)
        return -1;
    strcpy(pathbuf, genParams_[LSF_SERVERDIR].paramValue);
    strcat(pathbuf, "/nios");
    new_argv[0] = pathbuf;
    new_argv[1] = "-n";
    new_argv[2] = sock_buf;

    if (cmdmsg.options & REXF_USEPTY) {
        if (cmdmsg.options & REXF_SHMODE)
            new_argv[3] = "2";
        else
            new_argv[3] = "1";
    }
    else
        new_argv[3] = "0";
    new_argv[4] = 0;

    max = sysconf(_SC_OPEN_MAX);
    for (d = 3; d < max; ++d) {
        if (d != retsock)
            (void)close(d);
    }

    (void)lsfExecvp(new_argv[0], new_argv);
    lserrno = LSE_EXECV_SYS;
    close(retsock);
    close(s);
    return -1;

}


int
ls_rexecv(char *host, char **argv, int options)
{
    ls_rexecve(host, argv, options, environ);
    return -1;
}

int
ls_startserver(char *host, char **server, int options)
{
    int retsock;
    char official[MAXHOSTNAMELEN];
    struct timeval timeout;
    struct sockaddr_in sin;
    int s, descriptor[2];
    struct resCmdBill cmdmsg;
    int resTimeout;

    if (genParams_[LSF_RES_TIMEOUT].paramValue)
        resTimeout = atoi(genParams_[LSF_RES_TIMEOUT].paramValue);
    else
        resTimeout = RES_TIMEOUT;

    if (_isconnected_(host, descriptor))
        s = descriptor[0];
    else if ((s = ls_connect(host)) < 0)
        return -1;

    if (!FD_ISSET(s,&connection_ok_)){
        FD_SET(s,&connection_ok_);
        if (ackReturnCode_(s) < 0) {
            closesocket(s);
            _lostconnection_(host);
            return -1;
        }
    }

    if (!isatty(0) && !isatty(1))
        options &= ~REXF_USEPTY;
    else if ( options & REXF_USEPTY ) {
        if (rstty_(host) < 0) {
            _lostconnection_(host);
            return -1;
        }
    }

    if (mygetwd_(cmdmsg.cwd) == 0) {
        closesocket(s);
        _lostconnection_(host);
        lserrno = LSE_WDIR;
        return -1;
    }

    if ((retsock = TcpCreate_(true, 0)) < 0) {
        closesocket(s);
        _lostconnection_(host);
        return -1;
    }

    socklen_t len = sizeof(sin);
    if (getsockname (retsock, (struct sockaddr *) &sin, &len) < 0) {
        closesocket (retsock);
        closesocket(s);
        _lostconnection_(host);
        lserrno = LSE_SOCK_SYS;
        return -1;
    }

    cmdmsg.retport = sin.sin_port;
    cmdmsg.options = options & ~REXF_TASKPORT;
    cmdmsg.rpid = 0;
    cmdmsg.argv = server;

    timeout.tv_usec = 0;
    timeout.tv_sec  = resTimeout;

    if (sendCmdBill_(s, (resCmd) RES_SERVER, &cmdmsg, &retsock, &timeout)
        == -1) {
        closesocket(retsock);
        closesocket(s);
        _lostconnection_(host);
        return -1;
    }

    if (ackReturnCode_(s) < 0){
        closesocket(retsock);
        closesocket(s);
        _lostconnection_(host);
        return -1;
    }

    if (retsock <= 2 && (retsock = get_nonstd_desc_(retsock)) < 0) {
        closesocket(s);
        _lostconnection_(host);
        lserrno = LSE_SOCK_SYS;
        return -1;
    }
    gethostbysock_(s, official);

    connected_(official, -1, retsock, currentSN);

    return retsock;
}
