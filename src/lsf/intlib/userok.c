/* $Id: userok.c,v 1.7 2007/08/15 22:18:49 tmizan Exp $
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

#include "lsf/intlib/libllcore.h"
#include "lsf/lib/lib.hdr.h"
#include "lsf/lib/lproto.h"
#include "lsf/lib/ll.host.h"

bool_t
userok(struct sockaddr_in *from, struct lsfAuth *auth)
{
    if (verifyEAuth_(auth, from) == -1) {
        ls_syslog(LOG_ERR, "%s: eath authentication failed for %s/%s",
                  __func__, auth->lsfUserName, sockAdd2Str_(from));
        return false;
    }
    return true;
}
