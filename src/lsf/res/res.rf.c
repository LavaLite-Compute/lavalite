/* $Id: res.rf.c,v 1.4 2007/08/15 22:18:56 tmizan Exp $
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
#include <stdio.h>
#include <sys/types.h>
#include <netinet/in.h>




#include <sys/signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <pwd.h>
#include <sys/time.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/wait.h>

#if 0


#include <stdlib.h>
#include "../lib/lib.rf.h"
#include "res.h"
#include "resout.h"
#include "../lib/lproto.h"

#define NL_SETN		29

#include <memory.h>
#include <malloc.h>

#define LSRCP_MSGSIZE 65536

static int clearSock(int sock, int len);
static int rread(int sock, struct LSFHeader *hdr);
static int rwrite(int sock, struct LSFHeader *hdr);
static int rclose(int sock, struct LSFHeader *hdr);
static int rlseek(int sock, struct LSFHeader *hdr);
static int ropen(int sock, struct LSFHeader *hdr);
static int rstat(int sock, struct LSFHeader *hdr);
static int rfstat(int sock, struct LSFHeader *hdr);
static int rgetmnthost(int sock, struct LSFHeader *hdr);
static int runlink(int sock, struct LSFHeader *hdr);

void
rfServ_(int acceptSock)
{
    static char fname[] = "rfServ_()";
    struct LSFHeader msgHdr;
    struct LSFHeader buf;
    struct sockaddr_in from;
    int fromLen = sizeof(from);
    int sock;
    XDR xdrs;

    if ((sock = accept(acceptSock, (struct sockaddr *)&from, &fromLen)) < 0) {
	ls_errlog(stderr, I18N_FUNC_S_FAIL_M, fname, "accept",
	    acceptSock);
	closesocket(acceptSock);
	return;
    }

    xdrmem_create(&xdrs, (char *) &buf, sizeof(buf), XDR_DECODE);

    for (;;) {
	XDR_SETPOS(&xdrs, 0);

	if (readDecodeHdr_(sock, (char *) &buf, SOCK_READ_FIX, &xdrs, &msgHdr)
	    < 0) {
	    ls_errlog(stderr, I18N_FUNC_FAIL_MM, fname, "readDecodeHdr_");
	    closesocket(sock);
	    xdr_destroy(&xdrs);
	    return;
	}

	switch (msgHdr.opCode) {
	  case RF_OPEN:
	    ropen(sock, &msgHdr);
	    break;

	  case RF_CLOSE:
	    rclose(sock, &msgHdr);
	    break;

	  case RF_WRITE:
	    rwrite(sock, &msgHdr);
	    break;

	  case RF_READ:
	    rread(sock, &msgHdr);
	    break;

	  case RF_STAT:
	    rstat(sock, &msgHdr);
	    break;

	  case RF_GETMNTHOST:
	    rgetmnthost(sock, &msgHdr);
	    break;

	  case RF_FSTAT:
	    rfstat(sock, &msgHdr);
	    break;

	  case RF_LSEEK:
	    rlseek(sock, &msgHdr);
	    break;

	  case RF_UNLINK:
	    runlink(sock, &msgHdr);
	    break;

	  case RF_TERMINATE:
	    closesocket(sock);
	    return;

	  default:
	    ls_errlog(stderr, _i18n_msg_get(ls_catd, NL_SETN, 602,
		"%s: Unknown opcode %d"),
		fname, msgHdr.opCode);
	    xdr_destroy(&xdrs);
	    break;
	}
    }

}

static int
ropen(int sock, struct LSFHeader *hdr)
{
    static char fname[] = "ropen()";
    struct ropenReq req;
    char buf[LSRCP_MSGSIZE];
    char fn[MAXFILENAMELEN];
    int fd;
    XDR xdrs;
    req.fn = fn;

    xdrmem_create(&xdrs, buf, LSRCP_MSGSIZE, XDR_DECODE);
    if (readDecodeMsg_(sock, buf, hdr, SOCK_READ_FIX, &xdrs, (char *) &req,
		       xdr_ropenReq, NULL)) {
	ls_errlog(stderr, I18N_FUNC_FAIL_MM, fname, "readDecodeMsg_");
	xdr_destroy(&xdrs);
	closesocket(sock);
	return -1;
    }
    xdr_destroy(&xdrs);

    if (req.flags & LSF_O_CREAT_DIR) {
        req.flags &= ~LSF_O_CREAT_DIR;


	if (createSpoolSubDir(fn) < 0) {
	    if (lsSendMsg_(sock, -errnoEncode_(errno), 0, NULL, buf,
		                   sizeof(struct LSFHeader),
			           NULL, SOCK_WRITE_FIX, NULL) < 0) {
	        ls_errlog(stderr, I18N_FUNC_FAIL_MM,
						 fname, "lsSendMsg_");
	        closesocket(sock);
	        return -1;
	    }
	    return 0;
	}
    }


    if ((fd = open(req.fn, req.flags, req.mode)) == -1) {
	if (lsSendMsg_(sock, -errnoEncode_(errno), 0, NULL, buf,
		       sizeof(struct LSFHeader), NULL, SOCK_WRITE_FIX, NULL)
	    < 0) {
	    ls_errlog(stderr, I18N_FUNC_FAIL_MM, fname, "lsSendMsg_");
	    closesocket(sock);
	    return -1;
	}
	return 0;
    }


    if (lsSendMsg_(sock, fd, 0, NULL, buf,
		   sizeof(struct LSFHeader), NULL, SOCK_WRITE_FIX, NULL) < 0) {
	ls_errlog(stderr,  I18N_FUNC_FAIL_MM, fname, "lsSendMsg_");
	closesocket(sock);
	close(fd);
	return -1;
    }

    return 0;
}


static int
rclose(int sock, struct LSFHeader *hdr)
{
    static char fname[] = "rclose()";
    int reqfd;
    char buf[LSRCP_MSGSIZE];
    XDR xdrs;

    xdrmem_create(&xdrs, buf, LSRCP_MSGSIZE, XDR_DECODE);
    if (readDecodeMsg_(sock, buf, hdr, SOCK_READ_FIX, &xdrs, (char *) &reqfd,
		       xdr_int, NULL)) {
	ls_errlog(stderr, I18N_FUNC_FAIL_MM, fname, "readDecodeMsg_");
	xdr_destroy(&xdrs);
	closesocket(sock);
	return -1;
    }
    xdr_destroy(&xdrs);

    if (close(reqfd) == -1) {
	if (lsSendMsg_(sock, -errnoEncode_(errno), 0, NULL, buf,
		       sizeof(struct LSFHeader), NULL, SOCK_WRITE_FIX, NULL)
	    < 0) {
	    ls_errlog(stderr, I18N_FUNC_FAIL_MM, fname, "lsSendMsg_");
	    close(sock);
	}
	return 0;
    }

    if (lsSendMsg_(sock, 0, 0, NULL, buf,
		   sizeof(struct LSFHeader), NULL, SOCK_WRITE_FIX, NULL) < 0) {
	ls_errlog(stderr, I18N_FUNC_FAIL_MM, fname, "lsSendMsg_");
	close(sock);
	return -1;
    }

    return 0;
}

static int
rwrite(int sock, struct LSFHeader *hdr)
{
    static char fname[] = "rwrite()";
    struct rrdwrReq req;
    char msgBuf[LSRCP_MSGSIZE];
    XDR xdrs;
    char *buf;

    xdrmem_create(&xdrs, msgBuf, LSRCP_MSGSIZE, XDR_DECODE);
    if (readDecodeMsg_(sock, msgBuf, hdr, SOCK_READ_FIX, &xdrs, (char *) &req,
		       xdr_rrdwrReq, NULL)) {
	ls_errlog(stderr, I18N_FUNC_FAIL_MM, fname, "readDeocdeMsg_");
	xdr_destroy(&xdrs);
	close(sock);
	return -1;
    }
    xdr_destroy(&xdrs);

    if ((buf = (char *) malloc(req.len)) == NULL) {
	if (lsSendMsg_(sock, -errnoEncode_(errno), 0, NULL, msgBuf,
		       sizeof(struct LSFHeader), NULL, SOCK_WRITE_FIX, NULL)
	    < 0) {
	    ls_errlog(stderr, I18N_FUNC_FAIL_MM, fname, "lsSendMsg_");
	    close(sock);
	}
	clearSock(sock, req.len);
	return 0;
    }

    if (SOCK_READ_FIX(sock, buf, req.len) != req.len) {
	ls_errlog(stderr, I18N_FUNC_D_FAIL_M, fname, "SOCK_READ_FIX", req.len);
	goto fail;
    }

    if ((req.len = write(req.fd, buf, req.len)) < 0) {
	goto fail;
    }

    free(buf);

    if (lsSendMsg_(sock, 0, req.len, NULL, msgBuf,
		   sizeof(struct LSFHeader), NULL, SOCK_WRITE_FIX, NULL) < 0) {
	ls_errlog(stderr, I18N_FUNC_FAIL_MM, fname, "lsSendMsg_");
	close(sock);
	return -1;
    }

    return 0;

 fail:
    free(buf);

    if (lsSendMsg_(sock, -errnoEncode_(errno), 0, NULL, msgBuf,
		   sizeof(struct LSFHeader), NULL, SOCK_WRITE_FIX, NULL) < 0) {
	ls_errlog(stderr, I18N_FUNC_FAIL_MM, fname, "lsSendMsg_");
	close(sock);
	return -1;
    }

    return 0;
}



static int
rread(int sock, struct LSFHeader *hdr)
{
    static char fname[] = "rread()";
    struct rrdwrReq req;
    char msgBuf[LSRCP_MSGSIZE];
    XDR xdrs;
    char *buf;

    xdrmem_create(&xdrs, msgBuf, LSRCP_MSGSIZE, XDR_DECODE);
    if (readDecodeMsg_(sock, msgBuf, hdr, SOCK_READ_FIX, &xdrs, (char *) &req,
		       xdr_rrdwrReq, NULL)) {
	ls_errlog(stderr, I18N_FUNC_FAIL_MM, fname, "readDecodeMsg_");
	xdr_destroy(&xdrs);
	close(sock);
	return -1;
    }
    xdr_destroy(&xdrs);

    if ((buf = (char *) malloc(req.len)) == NULL) {
	if (lsSendMsg_(sock, -errnoEncode_(errno), 0, NULL, msgBuf,
		       sizeof(struct LSFHeader), NULL, SOCK_WRITE_FIX, NULL)
	    < 0) {
	    ls_errlog(stderr, I18N_FUNC_FAIL_MM, fname, "lsSendMsg_");
	    close(sock);
	}
	return 0;
    }

    if ((req.len = read(req.fd, buf, req.len)) < 0) {
	goto fail;
    }

    if (lsSendMsg_(sock, 0, req.len, NULL, msgBuf,
		   sizeof(struct LSFHeader), NULL, SOCK_WRITE_FIX, NULL) < 0) {
	ls_errlog(stderr, I18N_FUNC_FAIL_MM, fname, "lsSendMsg_");
	close(sock);
	free(buf);
	return -1;
    }

    if (SOCK_WRITE_FIX(sock, buf, req.len) != req.len) {
	ls_errlog(stderr, I18N_FUNC_FAIL_M, fname, "SOCK_WRITE_FIX");
	close(sock);
	free(buf);
	return -1;
    }

    free(buf);
    return 0;

 fail:
    free(buf);

    if (lsSendMsg_(sock, -errnoEncode_(errno), 0, NULL, buf,
		   sizeof(struct LSFHeader), NULL, SOCK_WRITE_FIX, NULL) < 0) {
	ls_errlog(stderr, I18N_FUNC_FAIL_MM, fname, "lsSendMsg_");
	close(sock);
	return -1;
    }
    return 0;

}

static int
rlseek(int sock, struct LSFHeader *hdr)
{
    static char fname[] = "rlseek()";
    struct rlseekReq req;
    char msgBuf[LSRCP_MSGSIZE];
    XDR xdrs;
    off_t pos;

    xdrmem_create(&xdrs, msgBuf, LSRCP_MSGSIZE, XDR_DECODE);
    if (readDecodeMsg_(sock, msgBuf, hdr, SOCK_READ_FIX, &xdrs, (char *) &req,
		       xdr_rlseekReq, NULL)) {
	ls_errlog(stderr, I18N_FUNC_FAIL_MM, fname, "readDecodeMsg_");
	xdr_destroy(&xdrs);
	close(sock);
	return -1;
    }
    xdr_destroy(&xdrs);

    if ((pos = lseek(req.fd, (off_t) req.offset, req.whence)) < 0) {
	goto fail;
    }

    if (lsSendMsg_(sock, 0, (int) pos, NULL, msgBuf,
		   sizeof(struct LSFHeader), NULL, SOCK_WRITE_FIX, NULL) < 0) {
	ls_errlog(stderr, I18N_FUNC_FAIL_MM, fname, "lsSendMsg_");
	close(sock);
	return -1;
    }

    return 0;

 fail:
    if (lsSendMsg_(sock, -errnoEncode_(errno), 0, NULL, msgBuf,
		   sizeof(struct LSFHeader), NULL, SOCK_WRITE_FIX, NULL) < 0) {
	ls_errlog(stderr, I18N_FUNC_FAIL_MM, fname, "lsSendMsg_");
	close(sock);
	return -1;
    }

    return 0;
}



static
int clearSock(int sock, int len)
{
    static char fname[] = "clearSock()";
    int l;
    char buf[LSRCP_MSGSIZE];

    for (; len; len -= l) {
	l = len > LSRCP_MSGSIZE ? LSRCP_MSGSIZE : len;
	if (SOCK_READ_FIX(sock, buf, l) != l) {
	    ls_errlog(stderr, I18N_FUNC_D_FAIL_M, fname, "SOCK_READ_FIX",
		l);
	    close(sock);
	    return -1;
	}
    }

    return 0;
}

static int
rstat(int sock, struct LSFHeader *hdr)
{
    static char fname[] = "rstat()";
    struct stat st;
    char buf[LSRCP_MSGSIZE];
    char fn[MAXFILENAMELEN];
    XDR xdrs;
    struct stringLen fnStr;

    fnStr.len = MAXFILENAMELEN;
    fnStr.name = fn;

    xdrmem_create(&xdrs, buf, LSRCP_MSGSIZE, XDR_DECODE);
    if (readDecodeMsg_(sock, buf, hdr, SOCK_READ_FIX, &xdrs,
		(char *) &fnStr, xdr_stringLen, NULL)) {
	ls_errlog(stderr, I18N_FUNC_FAIL_MM, fname, "readDeocdeMsg_");
	xdr_destroy(&xdrs);
	close(sock);
	return -1;
    }
    xdr_destroy(&xdrs);

    if (stat(fn, &st) == -1) {
	if (lsSendMsg_(sock, -errnoEncode_(errno), 0, NULL, buf,
		       sizeof(struct LSFHeader), NULL, SOCK_WRITE_FIX, NULL)
	    < 0) {
	    ls_errlog(stderr, I18N_FUNC_FAIL_MM, fname, "lsSendMsg_");
	    close(sock);
	    return -1;
	}
	return 0;
    }

    if (lsSendMsg_(sock, 0, 0, (char *) &st, buf, LSRCP_MSGSIZE, xdr_stat,
		   SOCK_WRITE_FIX, NULL) < 0) {
	ls_errlog(stderr, I18N_FUNC_FAIL_MM, fname, "lsSendMsg_");
	close(sock);
	return -1;
    }

    return 0;
}


static int
rfstat(int sock, struct LSFHeader *hdr)
{
    static char fname[] = "rfstat()";
    int reqfd;
    char msgBuf[LSRCP_MSGSIZE];
    XDR xdrs;
    struct stat st;

    xdrmem_create(&xdrs, msgBuf, LSRCP_MSGSIZE, XDR_DECODE);
    if (readDecodeMsg_(sock, msgBuf, hdr, SOCK_READ_FIX, &xdrs, (char *) &reqfd,
		       xdr_int, NULL)) {
	ls_errlog(stderr, I18N_FUNC_FAIL_MM, fname, "readDecodeMsg_");
	xdr_destroy(&xdrs);
	close(sock);
	return -1;
    }
    xdr_destroy(&xdrs);

    if (fstat(reqfd, &st) == -1) {
	if (lsSendMsg_(sock, -errnoEncode_(errno), 0, NULL, msgBuf,
		       sizeof(struct LSFHeader), NULL, SOCK_WRITE_FIX, NULL)
	    < 0) {
	    ls_errlog(stderr, I18N_FUNC_FAIL_MM, fname, "lsSendMsg_");
	    close(sock);
	    return -1;
	}
	return 0;
    }

    if (lsSendMsg_(sock, 0, 0, (char *) &st, msgBuf, LSRCP_MSGSIZE, xdr_stat,
		   SOCK_WRITE_FIX, NULL) < 0) {
	ls_errlog(stderr, I18N_FUNC_FAIL_MM, fname, "lsSendMsg_");
	close(sock);
	return -1;
    }

    return 0;
}


static int
rgetmnthost(int sock, struct LSFHeader *hdr)
{
    static char fname[] = "rgetmnthost()";
    char buf[LSRCP_MSGSIZE];
    char fn[MAXFILENAMELEN], *host;
    XDR xdrs;
    struct stringLen fnStr;
    struct stringLen hostStr;

    fnStr.len = MAXFILENAMELEN;
    fnStr.name = fn;

    xdrmem_create(&xdrs, buf, LSRCP_MSGSIZE, XDR_DECODE);
    if (readDecodeMsg_(sock, buf, hdr, SOCK_READ_FIX, &xdrs,
		      (char *) &fnStr, xdr_stringLen, NULL)) {
	ls_errlog(stderr, I18N_FUNC_FAIL_MM, fname, "readDecodeMsg_");
	xdr_destroy(&xdrs);
	close(sock);
	return -1;
    }
    xdr_destroy(&xdrs);

    if ((host = ls_getmnthost(fn)) == NULL) {
	if (lsSendMsg_(sock, -errnoEncode_(errno), 0, NULL, buf,
		       sizeof(struct LSFHeader), NULL, SOCK_WRITE_FIX, NULL)
	    < 0) {
	    ls_errlog(stderr, I18N_FUNC_FAIL_MM, fname, "lsSendMsg_");
	    close(sock);
	    return -1;
	}
	return 0;
    }

    hostStr.len = MAXHOSTNAMELEN;
    hostStr.name = host;
    if (lsSendMsg_(sock, 0, 0, (char *) &hostStr, buf, LSRCP_MSGSIZE, xdr_stringLen,
		   SOCK_WRITE_FIX, NULL) < 0) {
	ls_errlog(stderr, I18N_FUNC_FAIL_MM, fname, "lsSendMsg_");
	close(sock);
	return -1;
    }

    return 0;
}



#include <sys/dir.h>

static int
runlink(int sock, struct LSFHeader *hdr)
{
    static char fname[] = "runlink()";
    char buf[LSRCP_MSGSIZE];
    char fn[MAXFILENAMELEN];
    XDR xdrs;
    struct stringLen fnStr;
    struct stat st;

    fnStr.len = MAXFILENAMELEN;
    fnStr.name = fn;

    xdrmem_create(&xdrs, buf, LSRCP_MSGSIZE, XDR_DECODE);
    if (readDecodeMsg_(sock, buf, hdr, SOCK_READ_FIX, &xdrs,
		      (char *) &fnStr, xdr_stringLen, NULL)) {
	ls_errlog(stderr, I18N_FUNC_FAIL_MM, fname, "readDecodeMsg_");
	xdr_destroy(&xdrs);
	close(sock);
	return -1;
    }
    xdr_destroy(&xdrs);

    if ((stat(fn, &st) == 0) && (st.st_mode & S_IFDIR)) {


        DIR *dirp;
        struct direct *dp;
        char path[MAXPATHLEN];

        if ((dirp = opendir(fn)) == NULL) {
	    goto errrtn;
	}

        for (dp = readdir(dirp); dp != NULL; dp = readdir(dirp)) {
	    if (strcmp(dp->d_name, ".") != 0 &&
		       strcmp(dp->d_name, "..") != 0) {
                sprintf (path, "%s/%s", fn, dp->d_name);
                rmdir (path);
                unlink (path);
	    }
        }

        closedir (dirp);
        if (rmdir(fn) != 0) {
	    goto errrtn;
	}
    }

    if (unlink(fn) < 0) {
	goto errrtn;
    }

    if (lsSendMsg_(sock, 0, 0, NULL, buf,
                   sizeof(struct LSFHeader), NULL, SOCK_WRITE_FIX, NULL) < 0) {
	ls_errlog(stderr, I18N_FUNC_FAIL_MM, fname, "lsSendMsg_");
        closesocket(sock);
        return -1;
    }

    return 0;

errrtn:

    if (lsSendMsg_(sock, -errnoEncode_(errno), 0, NULL, buf,
                       sizeof(struct LSFHeader), NULL, SOCK_WRITE_FIX, NULL)
        < 0) {
	ls_errlog(stderr, I18N_FUNC_FAIL_MM, fname, "lsSendMsg_");
        closesocket(sock);
        return -1;
    }
    return 0;
}


#endif
