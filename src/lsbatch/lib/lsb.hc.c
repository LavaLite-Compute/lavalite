/* $Id: lsb.hc.c,v 1.3 2007/08/15 22:18:47 tmizan Exp $
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

int
lsb_hostcontrol(char *host, int operation)
{
    XDR xdrs;
    char request_buf[MSGSIZE];
    char *reply_buf, *contactHost = NULL;
    static struct controlReq hostControlReq;
    int cc;
    struct packet_header hdr;
    struct lsfAuth auth;

    if (hostControlReq.name == NULL) {
        hostControlReq.name = (char *) malloc (MAXHOSTNAMELEN);
        if (hostControlReq.name == NULL) {
            lsberrno = LSBE_NO_MEM;
            return -1;
        }
    }
    if (operation != HOST_OPEN && operation != HOST_CLOSE &&
        operation != HOST_REBOOT && operation != HOST_SHUTDOWN) {
        lsberrno = LSBE_BAD_ARG;
        return -1;
    }
    if (host)
        if (strlen (host) >= MAXHOSTNAMELEN - 1) {
            lsberrno = LSBE_BAD_ARG;
            return -1;
        }

    hostControlReq.opCode = operation;
    if (host)
        strcpy(hostControlReq.name, host);
    else {
        char *h;
        if ((h = ls_getmyhostname()) == NULL) {
            lsberrno = LSBE_LSLIB;
            return -1;
        }
        strcpy(hostControlReq.name, h);
    }

    switch (operation) {
    case HOST_REBOOT:
        hdr.operation = CMD_SBD_REBOOT;
        contactHost = host;
        break;
    case HOST_SHUTDOWN:
        hdr.operation = CMD_SBD_SHUTDOWN;
        contactHost = host;
        break;
    default:
        hdr.operation = BATCH_HOST_CTRL;
        break;
    }

    if (authTicketTokens_(&auth, contactHost) == -1)
        return -1;

    xdrmem_create(&xdrs, request_buf, MSGSIZE, XDR_ENCODE);

    if (!xdr_encodeMsg(&xdrs, (char*) &hostControlReq, &hdr,
                       xdr_controlReq, 0, &auth)) {
        lsberrno = LSBE_XDR;
        return -1;
    }

    if (operation == HOST_REBOOT || operation == HOST_SHUTDOWN) {

        if ((cc = cmdCallSBD_(hostControlReq.name, request_buf,
                              XDR_GETPOS(&xdrs), &reply_buf,
                              &hdr, NULL)) == -1)
            return -1;
    } else {

        if ((cc = callmbd (NULL, request_buf, XDR_GETPOS(&xdrs), &reply_buf,
                           &hdr, NULL, NULL, NULL)) == -1)
            return -1;
    }

    lsberrno = hdr.operation;
    if (cc)
        free(reply_buf);
    if (lsberrno == LSBE_NO_ERROR)
        return 0;
    else
        return -1;

}
