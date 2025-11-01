/* $Id: daemons.c,v 1.11 2007/08/15 22:18:44 tmizan Exp $
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

// Bug LSB_CONFDIR is not used anymore in ll. Change the defines
// to use enum like ll errors.
struct config_param daemonParams[] = {
    {"LSB_DEBUG", NULL},
    {"LSB_CONFDIR", NULL},
    {"LSF_SERVERDIR", NULL},
    {"LSF_LOGDIR", NULL},
    {"LSB_SHAREDIR", NULL},
    {"LSB_MAILTO", NULL},
    {"LSB_MAILPROG", NULL},
    {"LSB_SBD_PORT", NULL},
    {"LSB_MBD_PORT", NULL},
    {"LSF_ID_PORT", NULL},
    {"LSF_AUTH", NULL},
    {"LSB_CRDIR", NULL},
    {"LSF_USE_HOSTEQUIV", NULL},
    {"LSF_ROOT_REX", NULL},
    {"LSB_DEBUG_MBD", NULL},
    {"LSB_DEBUG_SBD", NULL},
    {"LSB_TIME_MBD", NULL},
    {"LSB_TIME_SBD", NULL},
    {"LSB_SIGSTOP", NULL},
    {"LSF_LOG_MASK", NULL},
    {"LSF_BINDIR", NULL},
    {"LSB_MBD_CONNTIMEOUT", NULL},
    {"LSB_SBD_CONNTIMEOUT", NULL},
    {"LSF_CONFDIR", NULL},
    {"LSB_MBD_MAILREPLAY", NULL},
    {"LSB_MBD_MIGTOPEND", NULL},
    {"LSB_SBD_READTIMEOUT", NULL},
    {"LSB_MBD_BLOCK_SEND", NULL},
    {"LSF_GETPWNAM_RETRY", NULL},
    {"LSB_MEMLIMIT_ENFORCE", NULL},
    {"LSB_BSUBI_OLD", NULL},
    {"LSB_STOP_IGNORE_IT", NULL},
    {"LSB_HJOB_PER_SESSION", NULL},
    {"LSF_AUTH_DAEMONS", NULL},
    {"LSB_REQUEUE_HOLD", NULL},
    {"LSB_SMTP_SERVER", NULL},
    {"LSB_MAILSERVER", NULL},
    {"LSB_MAILSIZE_LIMIT", NULL},
    {"LSB_REQUEUE_TO_BOTTOM", NULL},
    {"LSB_ARRAY_SCHED_ORDER", NULL},
    {"LSF_LIBDIR", NULL},
    {"LSB_QPOST_EXEC_ENFORCE", NULL},
    {"LSB_MIG2PEND", NULL},
    {"LSB_UTMP", NULL},
    {"LSB_JOB_CPULIMIT", NULL},
    {"LSB_RENICE_NEVER_AT_RESTART", NULL},
    {"LSF_MLS_LOG", NULL},
    {"LSB_JOB_MEMLIMIT", NULL},
    {"LSB_MOD_ALL_JOBS", NULL},
    {"LSB_SET_TMPDIR", NULL},
    {"LSB_PTILE_PACK", NULL},
    {"LSB_SBD_FINISH_SLEEP", NULL},
    {"LSB_VIRTUAL_SLOT",NULL},
    {"LSB_STDOUT_DIRECT",NULL},
    {NULL, NULL}
};


int
init_ServSock (u_short port)
{
    int ch;

    ch = chanServSocket_(SOCK_STREAM, ntohs(port), 1024, CHAN_OP_SOREUSE);
    if (ch < 0) {
        ls_syslog(LOG_ERR, "init_ServSock", "chanServSocket_");
        return -1;
    }

    return ch;
}

int
rcvJobFile(int chfd, struct lenData *jf)
{
    int cc;

    jf->data = NULL;
    jf->len = 0;

    if ((cc = chanRead_(chfd, NET_INTADDR_(&jf->len), NET_INTSIZE_)) !=
	NET_INTSIZE_) {
	ls_syslog(LOG_ERR, "%s", __func__, "chanRead_");
	return -1;
    }

    jf->len = ntohl(jf->len);
    jf->data = my_malloc(jf->len, "rcvJobFile");

    if ((cc = chanRead_(chfd, jf->data, jf->len)) != jf->len) {
	ls_syslog(LOG_ERR, "%s", __func__, "chanRead_");
        free(jf->data);
	return -1;
    }

    return 0;
}

int
do_readyOp(XDR *xdrs, int chanfd, struct sockaddr_in *from,
           struct packet_header *reqHdr )
{
    XDR xdrs2;
    struct Buffer *buf;
    struct packet_header replyHdr;

    if (chanAllocBuf_(&buf, sizeof(struct packet_header)) < 0) {
        ls_syslog(LOG_ERR, "%s", __func__, "malloc");
        return -1;
    }
    initLSFHeader_(&replyHdr);
    replyHdr.operation = READY_FOR_OP;
    replyHdr.length = 0;

    xdrmem_create (&xdrs2, buf->data, PACKET_HEADER_SIZE, XDR_ENCODE);
    if (!xdr_LSFHeader(&xdrs2, &replyHdr)) {
        ls_syslog(LOG_ERR, "%s", __func__, "xdr_LSFHeader");
        xdr_destroy(&xdrs2);
        return -1;

    }

    buf->len = XDR_GETPOS(&xdrs2);

    if (chanEnqueue_(chanfd, buf) < 0) {
        ls_syslog(LOG_ERR, "%s", __func__, "chanEnqueue_");
        xdr_destroy(&xdrs2);
        return -1;
    }

    xdr_destroy(&xdrs2);
    return 0;
}

void childRemoveSpoolFile(const char* spoolFile, int options,
                          const struct passwd* pwUser)
{
    // Bug good bye spool
#if 0
    char fname[] = "childRemoveSpoolFile";
    char apiName[] = "ls_initrex";
    pid_t pid;
    char hostName[MAXHOSTNAMELEN];
    char errMsg[MAXLINELEN];
    int status;
    char * fromHost = NULL;
    const char *sp;
    char dirName[MAXFILENAMELEN];

    status = -1;

    if ( ( fromHost =  (char *) getSpoolHostBySpoolFile(spoolFile)) != NULL) {
        strcpy( hostName, fromHost );
    } else {
        ls_syslog(LOG_ERR, "Unable to get spool host from the string \'%s\'"
            , (spoolFile ? spoolFile : "NULL"));
        goto Done;
    }

    if ( pwUser == NULL ) {
        ls_syslog(LOG_ERR, "%s: Parameter const struct passwd * pwUser is NULL!"
            ,fname);
       goto Done;
    }

    sp = getLowestDir_(spoolFile);
    if (sp) {
        strcpy(dirName, sp);
    } else {
        strcpy(dirName, spoolFile);
    }

    sprintf( errMsg, "%s: Unable to remove spool file:\n,\'%s\'\n on host %s\n"
            ,fname, dirName ,fromHost );

    if ( ! (options &  FORK_REMOVE_SPOOL_FILE) ) {

        if ( (options &  CALL_RES_IF_NEEDED) ) {
            if (ls_initrex(1, 0) < 0) {
                status = -1;
                sprintf( errMsg, "%s: %s failed when trying to delete %s from %s\n"
                  ,fname, apiName, dirName, fromHost );
                goto Error;
            }
        }

        chuser( pwUser->pw_uid );

        status = removeSpoolFile( hostName, dirName );

        chuser(batchId);

        if ( status != 0 ) {
            goto Error;
        }
        goto Done;
    }

    switch(pid = fork()) {
        case 0:

            if (debug < 2) {
                 closeExceptFD(-1);
            }

            if ( (options &  CALL_RES_IF_NEEDED) ) {
                if (ls_initrex(1, 0) < 0) {
                    status = -1;
                    sprintf( errMsg, "%s: %s failed when trying to delete %s from %s\n"
                      ,fname, apiName, dirName, fromHost );
                    goto Error;
                }
            }

            chuser( pwUser->pw_uid );
            status = 0;
            if ( removeSpoolFile( hostName, dirName ) == 0 ) {
                exit( 0 );
            } else {
                exit( -1);
            }
            goto Done;
            break;

        case -1:

            if (logclass & (LC_FILE)) {
                ls_syslog(LOG_ERR, "%s", __func__,"fork" );
            }
            status = -1;
            sprintf( errMsg, "%s: Unable to fork to remove spool file:\n,\'%s\'\n on host %s\n"
              ,fname, dirName ,fromHost );

            goto Error;

        default:

            status = 0;
            goto Done;
    }

Error:

    if ( status == -1 )
    {

        lsb_merr(errMsg);
    }
Done:
#endif
}
