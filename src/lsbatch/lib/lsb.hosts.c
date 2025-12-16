/*
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
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 *
 */

#include "lsbatch/lib/lsb.h"

struct hostInfoEnt *lsb_hostinfo(char **hosts, int *numHosts,
                                 char *resReq, int options)
{
    mbdReqType mbdReqtype = BATCH_HOST_INFO;
    XDR xdrs;
    struct packet_header hdr;
    struct infoReq req;
    struct hostDataReply reply;
    char req_buf[LL_BUFSIZ_4K];
    char *reply_buf = NULL;
    char *name_array[LL_BUFSIZ_4K / MAXHOSTNAMELEN];
    int rc;
    int numReq;
    int i;
    int max_hosts;

    if (numHosts == NULL) {
        lsberrno = LSBE_BAD_ARG;
        return NULL;
    }

    numReq = *numHosts;
    *numHosts = 0;

    if (numReq < 0) {
        lsberrno = LSBE_BAD_ARG;
        return NULL;
    }

    // Either explicit host list OR resReq, not both
    if (hosts != NULL && numReq > 0 && resReq != NULL) {
        lsberrno = LSBE_BAD_ARG;
        return NULL;
    }

    memset(&req, 0, sizeof(req));

    // Default request fields
    req.options = options;
    req.numNames = 0;
    req.names = NULL;
    req.resReq = "";

    // Resource requirement if provided
    if (resReq != NULL) {
        size_t len = strlen(resReq);

        if (len >= LL_BUFSIZ_512) {
            lsberrno = LSBE_BAD_RESREQ;
            return NULL;
        }

        req.resReq = resReq;
        req.options |= SORT_HOST;
    }

    // Host list if provided
    if (hosts != NULL && numReq > 0) {
        max_hosts = (int)(LL_BUFSIZ_4K / MAXHOSTNAMELEN);
        if (numReq > max_hosts) {
            lsberrno = LSBE_BAD_ARG;
            return NULL;
        }
        // hostnames are separated by spaces
        for (int i = 0; i < numReq; i++) {
            if (hosts[i] == NULL
                || strlen(hosts[i]) + 1 >= MAXHOSTNAMELEN) {
                lsberrno = LSBE_BAD_HOST;
                *numHosts = i;
                return NULL;
            }
            name_array[i] = hosts[i];
        }

        req.names = name_array;
        req.numNames = numReq;
    }

    xdrmem_create(&xdrs, req_buf, sizeof(req_buf), XDR_ENCODE);

    hdr.operation = mbdReqtype;
    if (!xdr_encodeMsg(&xdrs, (char *)&req, &hdr, xdr_infoReq, 0, NULL)) {
        xdr_destroy(&xdrs);
        lsberrno = LSBE_XDR;
        return NULL;
    }

    rc = call_mbd(req_buf, XDR_GETPOS(&xdrs), &reply_buf, &hdr, NULL);

    xdr_destroy(&xdrs);

    if (rc < 0) {
        if (reply_buf != NULL)
            free(reply_buf);
        return NULL;
    }

    lsberrno = hdr.operation;
    if (lsberrno != LSBE_NO_ERROR && lsberrno != LSBE_BAD_HOST) {
        if (reply_buf != NULL)
            free(reply_buf);
        return NULL;
    }

    xdrmem_create(&xdrs, reply_buf, rc, XDR_DECODE);

    if (!xdr_hostDataReply(&xdrs, &reply, &hdr)) {
        lsberrno = LSBE_XDR;
        xdr_destroy(&xdrs);
        if (reply_buf != NULL)
            free(reply_buf);
        return NULL;
    }

    xdr_destroy(&xdrs);
    if (reply_buf != NULL)
        free(reply_buf);

    if (lsberrno == LSBE_BAD_HOST) {
        *numHosts = reply.badHost;
        return NULL;
    }

    *numHosts = reply.numHosts;
    return reply.hosts;
}
