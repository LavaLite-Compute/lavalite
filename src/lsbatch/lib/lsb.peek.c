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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 USA
 *
 */

#include "lsbatch/lib/lsb.h"

char *lsb_peekjob(int64_t jobid)
{
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
    char *pSpoolDirUnix = NULL;
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
    if (!xdr_encodeMsg(&xdrs, (char *) &jobPeekReq, &hdr, xdr_jobPeekReq, 0,
                       &auth)) {
        lsberrno = LSBE_XDR;
        xdr_destroy(&xdrs);
        return NULL;
    }

    cc = call_mbd(request_buf, XDR_GETPOS(&xdrs), &reply_buf, &hdr, NULL);
    if (cc < 0) {
        xdr_destroy(&xdrs);
        return NULL;
    }

    xdr_destroy(&xdrs);

    lsberrno = hdr.operation;
    if (lsberrno == LSBE_NO_ERROR) {
        char fnBuf[MAXPATHLEN];
        struct passwd *pw;
        struct stat st;
        xdrmem_create(&xdrs, reply_buf, cc, XDR_DECODE);

        if (!xdr_jobPeekReply(&xdrs, &jobPeekReply, &hdr)) {
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

        sprintf(fnBuf, "%s/.lsbatch/%s", pw->pw_dir, jobPeekReply.outFile);

        if (stat(fnBuf, &st) == -1) {
            pid_t pid;
            int status;

            if (errno == ENOENT ) {
                if (lsb_openjobinfo(jobid, NULL, NULL, NULL, NULL, 0)
                    || (jInfo = lsb_readjobinfo()) == NULL) {
                    lsberrno = LSBE_LSBLIB;
                    return NULL;
                }
                lsb_closejobinfo();

                if ((pid = fork()) == 0) {
                    // ssh read the job file
                    exit(false);
                }
                if (pid == -1) {
                    return NULL;
                }

                if (waitpid(pid, &status, 0) == -1) {
                    return NULL;
                }
                if (WEXITSTATUS(status) == true) {
                    sprintf(fnBuf, "%s/%s", pSpoolDirUnix,
                            jobPeekReply.outFile);
                } else {
                    sprintf(fnBuf, ".lsbatch/%s", jobPeekReply.outFile);
                }
            } else {
                sprintf(fnBuf, ".lsbatch/%s", jobPeekReply.outFile);
            }
        }
        strcpy(jobPeekReply.outFile, fnBuf);
    }

    return jobPeekReply.outFile;

    if (cc)
        free(reply_buf);
    return NULL;
}
