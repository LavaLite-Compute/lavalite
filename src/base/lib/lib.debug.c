/* $Id: lib.debug.c,v 1.3 2007/08/15 22:18:50 tmizan Exp $
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 USA
 *
 */

#include "lsf/lib/lib.common.h"
#include "lsf/lib/lproto.h"

#define LSF_DEBUG_CMD 0
#define LSF_TIME_CMD 1
#define LSF_CMD_LOGDIR 2
#define LSF_CMD_LOG_MASK 3

#ifdef LSF_LOG_MASK
#undef LSF_LOG_MASK
#endif
#define LSF_LOG_MASK 4

// LavaLite this is legacy we dont want to print/debug library just follow
// the signature contract
int ls_initdebug(const char *appName)
{
    const char *logMask;
    struct config_param *pPtr;
    struct config_param debParams[] = {
        {"LSF_DEBUG_CMD", NULL},  {"LSF_TIME_CMD", NULL},
        {"LSF_CMD_LOGDIR", NULL}, {"LSF_CMD_LOG_MASK", NULL},
        {"LSF_LOG_MASK", NULL},   {NULL, NULL}};

    if (initenv_(debParams, NULL) < 0)
        return -1;

    if (debParams[LSF_CMD_LOG_MASK].paramValue != NULL)
        logMask = debParams[LSF_CMD_LOG_MASK].paramValue;
    else
        logMask = debParams[LSF_LOG_MASK].paramValue;

    for (pPtr = debParams; pPtr->paramName != NULL; pPtr++)
        FREEUP(pPtr->paramValue);

    return 0;
}
