/* $Id: lib.rf.h,v 1.2 2007/08/15 22:18:51 tmizan Exp $
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
#ifndef LIB_RF_H
#define LIB_RF_H

#define RF_SERVERD "_rf_Server_"

typedef enum {
    RF_OPEN,
    RF_READ,
    RF_WRITE,
    RF_CLOSE,
    RF_STAT,
    RF_GETMNTHOST,
    RF_LSEEK,
    RF_FSTAT,
    RF_UNLINK,
    RF_TERMINATE
} rfCmd;


struct ropenReq {
    char *fn;
    int flags;
    int mode;
};


struct rrdwrReq {
    int fd;
    int len;
};

struct rlseekReq {
    int fd;
    int whence;
    int offset;
};

#endif 
