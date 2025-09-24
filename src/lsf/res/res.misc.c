/* $Id: res.misc.c,v 1.4 2007/08/15 22:18:56 tmizan Exp $
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
#include "lsf/res/res.h"
#include "lsf/lim/limout.h"

#define NL_SETN     29

bool_t
xdr_resChildInfo(XDR  *xdrs, struct resChildInfo *childInfo, struct LSFHeader *hdr)
{
    if (!xdr_lsfAuth(xdrs, childInfo->lsfAuth, hdr) ||
            !xdr_resConnect(xdrs, childInfo->resConnect, hdr))
        return false;

    if (!xdr_var_string(xdrs, &childInfo->pw->pw_name) ||
            !xdr_var_string(xdrs, &childInfo->pw->pw_dir)  ||
            !xdr_var_string(xdrs, &childInfo->pw->pw_shell) ||
            !xdr_int(xdrs, &childInfo->pw->pw_uid) ||
            !xdr_int(xdrs, &childInfo->pw->pw_gid))
        return false;

    if (!xdr_var_string(xdrs, &childInfo->host->h_name))
        return false;


    if (xdrs->x_op == XDR_DECODE)
        childInfo->host->h_aliases = NULL;

    if (!xdr_portno(xdrs, &childInfo->parentPort))
        return false;

    if (!xdr_int(xdrs, &childInfo->currentRESSN))
        return false;

    return true;

}
