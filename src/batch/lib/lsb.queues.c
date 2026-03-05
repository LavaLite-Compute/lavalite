/* $Id: lsb.queues.c,v 1.4 2007/08/15 22:18:47 tmizan Exp $
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

struct queueInfoEnt *lsb_queueinfo(char **queues,
                                   int *numQueues,
                                   char *users,
                                   char *hosts,
                                   int options)
{
    struct infoReq req;
    struct queueInfoReply reply;
    struct queueInfoEnt **qinfo;
    struct packet_header hdr;
    XDR xdrs_req, xdrs_rep;
    char request_buf[MSGSIZE];
    char *reply_buf = NULL;
    int i, rc;

    (void)users;
    (void)hosts;
    (void)options;

    if (numQueues == NULL) {
        lsberrno = LSBE_BAD_ARG;
        return NULL;
    }

    if ((queues == NULL && *numQueues > 0) || *numQueues < 0) {
        lsberrno = LSBE_BAD_ARG;
        return NULL;
    }

    memset(&req, 0, sizeof(req));
    memset(&reply, 0, sizeof(reply));
    memset(&hdr, 0, sizeof(hdr));

    req.options = 0;

    if (*numQueues == 0 || queues == NULL) {
        /* All queues */
        req.options |= ALL_QUEUE;

        req.names = calloc(1, sizeof(char *));
        if (req.names == NULL) {
            lsberrno = LSBE_NO_MEM;
            return NULL;
        }
        req.names[0] = (char *)"";
        req.numNames = 1;
    } else {
        /* Explicit list of queues */
        req.names = calloc((size_t)*numQueues, sizeof(char *));
        if (req.names == NULL) {
            lsberrno = LSBE_NO_MEM;
            return NULL;
        }

        req.numNames = *numQueues;

        for (i = 0; i < *numQueues; i++) {
            if (queues[i] && strlen(queues[i]) + 1 < MAXHOSTNAMELEN) {
                req.names[i] = queues[i];
            } else {
                free(req.names);
                req.names = NULL;
                lsberrno = LSBE_BAD_QUEUE;
                *numQueues = i;
                return NULL;
            }
        }
    }

    /* We don’t use resource requirements */
    req.resReq = (char *)"";

    init_pack_hdr(&hdr);
    hdr.operation = BATCH_QUE_INFO;

    xdrmem_create(&xdrs_req, request_buf, sizeof(request_buf), XDR_ENCODE);

    if (!xdr_encodeMsg(&xdrs_req,
                       (char *)&req,
                       &hdr,
                       xdr_infoReq,
                       0,
                       NULL)) {
        lsberrno = LSBE_XDR;
        xdr_destroy(&xdrs_req);
        free(req.names);
        return NULL;
    }

    rc = call_mbd(request_buf,
                  (int)XDR_GETPOS(&xdrs_req),
                  &reply_buf,
                  &hdr,
                  NULL);

    xdr_destroy(&xdrs_req);
    free(req.names);

    if (rc < 0) {
        if (reply_buf)
            free(reply_buf);
        return NULL;
    }

    lsberrno = hdr.operation;

    if (lsberrno != LSBE_NO_ERROR && lsberrno != LSBE_BAD_QUEUE) {
        if (reply_buf)
            free(reply_buf);
        *numQueues = 0;
        return NULL;
    }

    xdrmem_create(&xdrs_rep, reply_buf, (u_int)rc, XDR_DECODE);

    if (!xdr_queueInfoReply(&xdrs_rep, &reply, &hdr)) {
        lsberrno = LSBE_XDR;
        xdr_destroy(&xdrs_rep);
        free(reply_buf);
        *numQueues = 0;
        return NULL;
    }

    xdr_destroy(&xdrs_rep);
    free(reply_buf);

    if (lsberrno == LSBE_BAD_QUEUE) {
        *numQueues = reply.badQueue;
        return NULL;
    }

    if (reply.numQueues <= 0) {
        *numQueues = 0;
        return NULL;
    }

    qinfo = malloc((size_t)reply.numQueues *
                   sizeof(struct queueInfoEnt *));
    if (qinfo == NULL) {
        lsberrno = LSBE_NO_MEM;
        *numQueues = 0;
        return NULL;
    }

    for (i = 0; i < reply.numQueues; i++)
        qinfo[i] = &reply.queues[i];

    *numQueues = reply.numQueues;
    return qinfo[0];
}
