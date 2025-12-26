#pragma once
/* $Id: lib.h,v 1.8 2007/08/15 22:18:50 tmizan Exp $
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
 */

/* Bug. This function contains lots of crap.
 */

#include "lsf/lib/ll.bufsiz.h"
#include "lsf/lib/lib.common.h"
#include "lsf/lib/lproto.h"
#include "lsf/lim/limout.h"
#include "lsf/lib/lib.xdr.h"
#include "lsf/lib/ll.host.h"
#include "lsf/lib/ll.sysenv.h"
#include "lsf/lib/lib.channel.h"

// LavaLite parachute
extern __thread int lserrno;

extern char *indexfilter_;
extern int ls_readconfenv(struct config_param *, const char *);

extern int callLim_(enum limReqCode, void *, bool_t (*)(), void *, bool_t (*)(),
                    char *, int, struct packet_header *);

extern int initLimSock_(void);
extern void err_return_(enum limReplyCode);
extern struct hostLoad *loadinfo_(char *, struct decisionReq *, char *, int *,
                                  char ***);
extern struct hostent *Gethostbyname_(char *);
extern short getRefNum_(void);

extern char **placement_(char *, struct decisionReq *, char *, int *);

extern int sig_encode(int);
extern int sig_decode(int);
extern char *getSigSymbolList(void);
extern char *getSigSymbol(int);

extern void ls_errlog(FILE *fd, const char *fmt, ...);
extern void ls_verrlog(FILE *fd, const char *fmt, va_list ap);
