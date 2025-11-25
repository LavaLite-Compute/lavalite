/* $Id: lsb.switch.c,v 1.3 2007/08/15 22:18:48 tmizan Exp $
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

int lsb_switchjob(int64_t jobId, char *queue)
{
    struct jobSwitchReq jobSwitchReq;
    char request_buf[MSGSIZE];
    char *reply_buf;
    XDR xdrs;
    mbdReqType mbdReqtype;
    int cc;
    struct packet_header hdr;
    struct lsfAuth auth;

    if (jobId <= 0 || queue == 0) {
        lsberrno = LSBE_BAD_ARG;
        return -1;
    }
    if (queue && (strlen(queue) >= LL_BUFSIZ_32 - 1)) {
        lsberrno = LSBE_BAD_QUEUE;
        return -1;
    }

    if (authTicketTokens_(&auth, NULL) == -1)
        return -1;

    jobSwitchReq.jobId = jobId;
    strcpy(jobSwitchReq.queue, queue);

    mbdReqtype = BATCH_JOB_SWITCH;
    xdrmem_create(&xdrs, request_buf, MSGSIZE, XDR_ENCODE);
    init_pack_hdr(&hdr);
    hdr.operation = mbdReqtype;
    if (!xdr_encodeMsg(&xdrs, (char *) &jobSwitchReq, &hdr, xdr_jobSwitchReq, 0,
                       &auth)) {
        lsberrno = LSBE_XDR;
        return -1;
    }

    if ((cc = call_mbd(request_buf, XDR_GETPOS(&xdrs), &reply_buf, &hdr,
                      NULL)) == -1) {
        xdr_destroy(&xdrs);
        return -1;
    }

    xdr_destroy(&xdrs);
    lsberrno = hdr.operation;
    if (cc)
        free(reply_buf);

    if (lsberrno == LSBE_NO_ERROR)
        return 0;

    return -1;
}
