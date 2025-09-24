/* $Id: lib.nioback.c,v 1.3 2007/08/15 22:18:50 tmizan Exp $
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
#include "lsf/res/rescom.h"
#include "lsf/lib/lib.xdrres.h"

#define NL_SETN 23

int
niosCallback_(struct sockaddr_in *from,
              uint16_t port,
              int rpid,
              int exitStatus,
              int terWhiPendStatus)
{
    static char fname[] = "niosCallback_";
    int s;
    struct niosConnect conn;
    struct {
        struct LSFHeader hdr;
        struct niosConnect conn;
    } reqBuf;
    struct LSFHeader reqHdr;
    int resTimeout;

    struct linger linstr = {1, 1};

    from->sin_port = port;

    if ((s = TcpCreate_(false, 0)) < 0) {
        if (logclass & LC_EXEC)
            ls_syslog(LOG_ERR, I18N_FUNC_FAIL_M,fname,"tcpCreate");
        return -1;
    }

    if (genParams_[LSF_RES_TIMEOUT].paramValue)
        resTimeout = atoi(genParams_[LSF_RES_TIMEOUT].paramValue);
    else
        resTimeout = RES_TIMEOUT;

    if (b_connect_(s, (struct sockaddr *)from,
                   sizeof(struct sockaddr_in), resTimeout) < 0) {
        if (logclass & LC_EXEC)
            ls_syslog(LOG_DEBUG,"\
                    %s: connect(s=%d,%s,len=%d) failed: %m", fname,
                      s, sockAdd2Str_(from), sizeof(struct sockaddr_in));
        closesocket(s);
        return -1;
    }

    fcntl(s, F_SETFD, fcntl(s, F_GETFD) | FD_CLOEXEC);
    setsockopt(s, SOL_SOCKET, SO_LINGER, (char *)&linstr, sizeof(linstr));

    if (rpid == 0)
        return s;

    if (logclass & LC_TRACE)
        ls_syslog(LOG_DEBUG, "%s: exitStatus <%d> terWhiPendStatus <%d>",
                  fname, exitStatus, terWhiPendStatus);

    initLSFHeader_(&reqHdr);
    reqHdr.opCode = RES2NIOS_CONNECT;
    conn.rpid = rpid;
    if (terWhiPendStatus == 1)
        conn.exitStatus = 126;
    else
        conn.exitStatus = exitStatus;
    conn.terWhiPendStatus = terWhiPendStatus;

    memset((char*)&reqBuf, 0, sizeof(reqBuf));
    if (writeEncodeMsg_(s,
                        (char *)&reqBuf,
                        sizeof(reqBuf),
                        &reqHdr,
                        (char *)&conn,
                        NB_SOCK_WRITE_FIX,
                        xdr_niosConnect,
                        0) < 0) {
        if (logclass & LC_EXEC)
            ls_syslog(LOG_ERR,
                      I18N(6201,"%s: writeEncodeMsg_(%d,%d) RES2NIOS_connect failed: %M"),  /* catgets 6201*/
                      fname, s, rpid);
        closesocket(s);
        return -1;
    }

    return s;
}
