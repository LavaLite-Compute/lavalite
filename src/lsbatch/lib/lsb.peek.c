/* $Id: lsb.peek.c,v 1.5 2007/08/15 22:18:47 tmizan Exp $
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

#include "lsbatch/lib/lsb.h"

char *
lsb_peekjob (LS_LONG_INT jobid)
{
    static char fname[] = "lsb_peekjob";
    struct jobPeekReq jobPeekReq;
    mbdReqType mbdReqtype;
    XDR xdrs;
    char request_buf[MSGSIZE];
    char *reply_buf;
    int cc;
    struct packet_header hdr;
    static struct jobPeekReply jobPeekReply;
    struct lsfAuth auth;
    struct jobInfoEnt *jInfo;
    char* pSpoolDirUnix = NULL;
    char lsfUserName[MAXLINELEN];

    if (jobid <= 0) {
        lsberrno = LSBE_BAD_ARG;
        return NULL;
    }

    jobPeekReq.jobId = jobid;

    if (authTicketTokens_(&auth, NULL) == -1) {
        return NULL;
    }

    mbdReqtype = BATCH_JOB_PEEK;
    xdrmem_create(&xdrs, request_buf, MSGSIZE, XDR_ENCODE);

    hdr.operation = mbdReqtype;
    if (!xdr_encodeMsg(&xdrs, (char *)&jobPeekReq, &hdr, xdr_jobPeekReq, 0,
                &auth)) {
        lsberrno = LSBE_XDR;
        xdr_destroy(&xdrs);
        return NULL;
    }

    if ((cc = callmbd (NULL, request_buf, XDR_GETPOS(&xdrs), &reply_buf,
                    &hdr, NULL, NULL, NULL)) == -1) {
        xdr_destroy(&xdrs);
        return NULL;
    }

    xdr_destroy(&xdrs);

    lsberrno = hdr.operation;
    if (lsberrno == LSBE_NO_ERROR) {
        char fnBuf[MAXPATHLEN];
        struct passwd *pw;
        struct stat st;
        xdrmem_create(&xdrs, reply_buf, XDR_DECODE_SIZE_(cc), XDR_DECODE);

        if(!xdr_jobPeekReply(&xdrs, &jobPeekReply, &hdr)) {
            lsberrno = LSBE_XDR;
            xdr_destroy(&xdrs);
            if (cc) {
                free(reply_buf);
            }
            return NULL;
        }
        xdr_destroy(&xdrs);
        if (cc) {
            free(reply_buf);
        }

        if ((pw = getpwnam2(lsfUserName)) == NULL) {
            lsberrno = LSBE_SYS_CALL;
            return NULL;
        }

        if (logclass & LC_EXEC) {
            ls_syslog(LOG_DEBUG, "%s: the jobReply.outfile is <%s>",
                    fname, jobPeekReply.outFile);
        }
        pSpoolDirUnix = getUnixSpoolDir(jobPeekReply.pSpoolDir);

        if ((pSpoolDirUnix != NULL)
                && access(pSpoolDirUnix, W_OK) == 0) {

            sprintf(fnBuf, "%s/%s", pSpoolDirUnix,
                    jobPeekReply.outFile);
        } else {
            sprintf(fnBuf, "%s/.lsbatch/%s", pw->pw_dir,
                    jobPeekReply.outFile);

            if (stat(fnBuf, &st) == -1){
                pid_t pid;
                int status;

                if ( errno == ENOENT && pSpoolDirUnix !=NULL ) {
                    if ( lsb_openjobinfo (jobid, NULL, NULL, NULL, NULL, 0) < 0
                            || (jInfo=lsb_readjobinfo (NULL)) == NULL){
                        lsberrno = LSBE_LSBLIB;
                        return NULL;
                    }
                    lsb_closejobinfo();

                    if ((pid = fork()) == 0) {
#if 0
                        if (ls_initrex(1,0) < 0) {
                            exit(false);
                        }
                        // Bug we need to rewrite bpeek and we dont support
                        // remote execution anymore

                        if (ls_rstat(jInfo->exHosts[0], pSpoolDirUnix, &st) == 0) {
                            ls_donerex();
                            exit(true);
                        }
#endif
                        exit(false);

                    }
                    if (pid == -1) {
                        return NULL;
                    }

                    if (waitpid(pid, &status, 0) == -1) {
                        return NULL;
                    }
                    if (WEXITSTATUS(status) == true) {
                        sprintf(fnBuf, "%s/%s", pSpoolDirUnix,jobPeekReply.outFile);
                    }else {
                        sprintf(fnBuf, ".lsbatch/%s", jobPeekReply.outFile);
                    }
                }else {
                    sprintf(fnBuf, ".lsbatch/%s", jobPeekReply.outFile);
                }
            }
        }
        strcpy(jobPeekReply.outFile, fnBuf);
        return(jobPeekReply.outFile);
    }

    if (cc)
        free(reply_buf);
    return NULL;
}
