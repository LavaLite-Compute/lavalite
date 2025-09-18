/* $Id: rescom.h,v 1.2 2007/08/15 22:18:56 tmizan Exp $
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

#ifndef _RESCOM_H_
#define _RESCOM_H_

typedef enum {
    RES2NIOS_CONNECT,
    RES2NIOS_STATUS,
    RES2NIOS_STDOUT,
    RES2NIOS_EOF,
    RES2NIOS_REQUEUE,
    RES2NIOS_NEWTASK,
    RES2NIOS_STDERR
} resNiosCmd;

typedef enum {
    NIOS2RES_SIGNAL,
    NIOS2RES_STDIN,
    NIOS2RES_EOF,
    NIOS2RES_TIMEOUT,
    NIOS2RES_SETTTY,
    NIOS2RES_HEARTBEAT
} niosResCmd;

#endif



