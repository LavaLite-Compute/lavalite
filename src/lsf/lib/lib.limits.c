/* $Id: lib.limits.c,v 1.4 2007/08/15 22:18:50 tmizan Exp $
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
#include "lsf/lib/lib.common.h"
#include "lsf/lib/lib.hdr.h"

#define NL_SETN   23
static int SHIFT_32 = 32;

void
rlimitEncode_(struct lsfLimit *lsflimit, struct rlimit *rlimit, int limit)
{
    int softok = false;
    int hardok = false;

    if (rlimit->rlim_cur == RLIM_INFINITY) {
        lsflimit->rlim_curl = 0xffffffff;
        lsflimit->rlim_curh = 0x7fffffff;
        softok = true;
    }

    if (rlimit->rlim_max == RLIM_INFINITY) {
        lsflimit->rlim_maxl = 0xffffffff;
        lsflimit->rlim_maxh = 0x7fffffff;
        hardok = true;
    }

    if (sizeof(rlimit->rlim_cur) == 8) {
        if (!softok) {
            lsflimit->rlim_curl = 0xffffffff & rlimit->rlim_cur;
            lsflimit->rlim_curh = rlimit->rlim_cur >> SHIFT_32;
        }
        if (!hardok) {
            lsflimit->rlim_maxl = 0xffffffff & rlimit->rlim_max;
            lsflimit->rlim_maxh = rlimit->rlim_max >> SHIFT_32;
        }
    } else {
        if (!softok) {
            lsflimit->rlim_curl = rlimit->rlim_cur;
            lsflimit->rlim_curh = 0;
        }
        if (!hardok) {
            lsflimit->rlim_maxl = rlimit->rlim_max;
            lsflimit->rlim_maxh = 0;
        }
    }

    if (logclass & LC_TRACE)
        ls_syslog(LOG_DEBUG3, "rlimitEncode_: limit %d rlim_cur %ld %x rlim_max %ld %x curl %d %x curh %d %x maxl %d %x maxh %d %x rlim_inf %d %x\r",
                  limit, rlimit->rlim_cur, (unsigned int)rlimit->rlim_cur,
                  rlimit->rlim_max, (unsigned int)rlimit->rlim_max,
                  lsflimit->rlim_curl, lsflimit->rlim_curl,
                  lsflimit->rlim_curh, lsflimit->rlim_curh,
                  lsflimit->rlim_maxl, lsflimit->rlim_maxl,
                  lsflimit->rlim_maxh, lsflimit->rlim_maxh,
                  RLIM_INFINITY, RLIM_INFINITY);

}

void
rlimitDecode_(struct lsfLimit *lsflimit, struct rlimit *rlimit, int limit)
{
    int softok = false;
    int hardok = false;

    if (lsflimit->rlim_curl == 0xffffffff && lsflimit->rlim_curh == 0x7fffffff)
    {
        rlimit->rlim_cur = RLIM_INFINITY;
        softok = true;
    }

    if (lsflimit->rlim_maxl == 0xffffffff && lsflimit->rlim_maxh == 0x7fffffff)
    {
        rlimit->rlim_max = RLIM_INFINITY;
        hardok = true;
    }
    if (sizeof(rlimit->rlim_cur) == 8) {
        if (!softok) {
            rlimit->rlim_cur = lsflimit->rlim_curh;
            rlimit->rlim_cur = rlimit->rlim_cur << SHIFT_32;
            rlimit->rlim_cur |= (lsflimit->rlim_curl & 0xffffffff);
        }

        if (!hardok) {
            rlimit->rlim_max = lsflimit->rlim_maxh;
            rlimit->rlim_max = rlimit->rlim_max << SHIFT_32;
            rlimit->rlim_max |= (lsflimit->rlim_maxl & 0xffffffff);
        }
    } else {
        if (!softok) {
            if ((lsflimit->rlim_curh > 0) || (lsflimit->rlim_curl & 0x80000000))
                rlimit->rlim_cur = RLIM_INFINITY;
            else
                rlimit->rlim_cur = lsflimit->rlim_curl;
        }

        if (!hardok) {
            if ((lsflimit->rlim_maxh > 0) || (lsflimit->rlim_maxl & 0x80000000))
                rlimit->rlim_max = RLIM_INFINITY;
            else
                rlimit->rlim_max = lsflimit->rlim_maxl;
        }
    }

    if (logclass & LC_TRACE)
        ls_syslog(LOG_DEBUG3, "rlimitDecode_: limit %d rlim_cur %ld %x rlim_max %ld %x curl %d %x curh %d %x maxl %d %x maxh %d %x rlim_inf %d %x\r",
                  limit, rlimit->rlim_cur, (unsigned int)rlimit->rlim_cur,
                  rlimit->rlim_max, (unsigned int)rlimit->rlim_max,
                  lsflimit->rlim_curl, lsflimit->rlim_curl,
                  lsflimit->rlim_curh, lsflimit->rlim_curh,
                  lsflimit->rlim_maxl, lsflimit->rlim_maxl,
                  lsflimit->rlim_maxh, lsflimit->rlim_maxh,
                  RLIM_INFINITY, RLIM_INFINITY);

    if (limit == LSF_RLIMIT_NOFILE || limit == LSF_RLIMIT_OPEN_MAX) {
        int sys_max = sysconf(_SC_OPEN_MAX);
        if (rlimit->rlim_cur > sys_max)
            rlimit->rlim_cur = sys_max;
        if (rlimit->rlim_max > sys_max)
            rlimit->rlim_max = sys_max;
    }
}
