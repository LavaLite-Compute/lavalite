/* $Id: lib.xdrres.h,v 1.3 2007/08/15 22:18:51 tmizan Exp $
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

#ifndef LIB_XDRRES_H
#define LIB_XDRRES_H

/* lib.rf.h legacy garbage to remove..
*/
#include "lsf/lib/lib.rf.h"

extern int xdr_resConnect(XDR *, struct resConnect *, struct LSFHeader *);
extern int xdr_resCmdBill(XDR *, struct resCmdBill *, struct LSFHeader *);
extern int xdr_resSetenv(XDR *, struct resSetenv *, struct LSFHeader *);
extern int xdr_resRKill(XDR *, struct resRKill *, struct LSFHeader *);
extern int xdr_resGetpid(XDR *, struct resPid *, struct LSFHeader *);
extern bool_t xdr_resGetRusage(XDR *, struct resRusage *, struct LSFHeader *);
extern int xdr_resChdir(XDR *, struct resChdir *, struct LSFHeader *);
extern int xdr_resControl(XDR *, struct resControl *, struct LSFHeader *);

extern int xdr_resStty (XDR *, struct resStty *, struct LSFHeader *);
extern int xdr_niosConnect (XDR *, struct niosConnect *, struct LSFHeader *);
extern int xdr_niosStatus (XDR *, struct niosStatus *, struct LSFHeader *);
extern int xdr_resSignal (XDR *, struct resSignal *, struct LSFHeader *);

extern bool_t xdr_ropenReq (XDR *, struct ropenReq *, struct LSFHeader *);
extern bool_t xdr_rrdwrReq (XDR *, struct rrdwrReq *, struct LSFHeader *);
extern bool_t xdr_rlseekReq (XDR *, struct rlseekReq *, struct LSFHeader *);

#endif
