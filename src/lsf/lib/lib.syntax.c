/* $Id: lib.syntax.c,v 1.3 2007/08/15 22:18:51 tmizan Exp $
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

int
expSyntax_(char *resReq)
{
    struct stringLen str;

    if (initenv_(NULL, NULL) < 0)
        return -1;

    if (!resReq)
        resReq = " ";

    str.name = resReq;
    str.len  = MAXLINELEN;

    if (callLim_(LIM_CHK_RESREQ, &str, xdr_stringLen,
                 NULL, NULL, NULL, 0, NULL) < 0)
        return -1;

   return 0;

}
