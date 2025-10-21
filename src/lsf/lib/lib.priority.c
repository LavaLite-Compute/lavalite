/* $Id: lib.priority.c,v 1.5 2007/08/15 22:18:51 tmizan Exp $
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

#define SYSV_NICE_0      20
#define MAX_PRIORITY         20
#define MIN_PRIORITY        -20

#define LSF_TO_SYSV(x)      ((x) + SYSV_NICE_0)
#define SYSV_TO_LSF(x)      ((x) - SYSV_NICE_0)

int
ls_setpriority(int newPriority)
{
    int increment;

    if (newPriority > MAX_PRIORITY * 2) {
        newPriority = MAX_PRIORITY * 2;
    } else if (newPriority < MIN_PRIORITY * 2) {
        newPriority = MIN_PRIORITY * 2;
    }
    increment = newPriority;

    errno = 0;

    if (nice(increment) == -1 && (0 != errno)) {
        return false;
    }

    return true;
}
