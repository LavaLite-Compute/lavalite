/* $Id: lib.xdrnio.c,v 1.4 2007/08/15 22:18:51 tmizan Exp $
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
#include "lsf/res/resout.h"

bool_t
xdr_resConnect(XDR *xdrs, struct resConnect *connectPtr,
               struct LSFHeader *hdr)
{

    if (!xdr_lenData(xdrs, &connectPtr->eexec)) {
        return false;
    }

    return true;
}

bool_t
xdr_niosConnect (XDR *xdrs, struct niosConnect *conn, struct LSFHeader *hdr)
{


    if (!(xdr_int(xdrs, &conn->rpid)))
        return false;


    return (xdr_int(xdrs, &conn->exitStatus) &&
            xdr_int(xdrs, &conn->terWhiPendStatus));

    return true;
}

bool_t
xdr_niosStatus (XDR *xdrs, struct niosStatus *st, struct LSFHeader *hdr)
{
    struct lsfRusage lsfRu;

    memset((char*)&lsfRu, 0, sizeof(lsfRu));
    if (!xdr_int(xdrs, (int *) &st->ack))
        return false;

    if (st->ack != RESE_SIGCHLD)
        return true;

    if (!xdr_int(xdrs, &st->s.ss))
        return false;



    if (xdrs->x_op == XDR_ENCODE) {
        ls_ruunix2lsf(&(st->s.ru), &lsfRu);
    };

    if (!xdr_arrayElement(xdrs, (char *) &lsfRu, hdr, xdr_lsfRusage))
        return false;

    if (xdrs->x_op == XDR_ENCODE)
        return true;



    ls_rulsf2unix(&lsfRu, &(st->s.ru));

    return true;
}


bool_t
xdr_resSignal (XDR *xdrs, struct resSignal *sig, struct LSFHeader *hdr)
{
    return (xdr_int(xdrs, &sig->pid) &&
            xdr_int(xdrs, &sig->sigval));
}
