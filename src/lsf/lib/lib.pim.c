/* $Id: lib.pim.c,v 1.4 2007/08/15 22:18:51 tmizan Exp $
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

/* Dont include lib.h as it redefines the LSF_* macros used in
 * config_params.
 */

#include "lsf/lib/lib.common.h"

enum pim_param {
    LSF_PIM_INFODIR,
    LSF_PIM_SLEEPTIME,
    LSF_LIM_DEBUG,
    LSF_LOGDIR,
    LSF_PIM_SLEEPTIME_UPDATE,
    LSF_PARAM_COUNT
};

// Use an array of char * to represent pim parameters
static const char *pim_params[] = {
    [LSF_PIM_INFODIR]  = "",
    [LSF_PIM_SLEEPTIME] = "",
    [LSF_LIM_DEBUG] = "",
    [LSF_LOGDIR] = "",
    [LSF_PIM_SLEEPTIME_UPDATE] = ""
};

struct jRusage *
getJInfo_(int npgid, int *pgid, int options, int cpgid)
{
    (void)pim_params;
    return NULL;
}
