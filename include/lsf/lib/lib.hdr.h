#pragma once
/* $Id: lib.hdr.h,v 1.5 2007/08/15 22:18:50 tmizan Exp $
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


// Include the lavalite protocol header and version
#include "lsf/lib/ll_proto.h"
#include "lsf/intlib/ll_bufsize.h"

struct stringLen {
    char *name;
    int   len;
};

struct lenData {
    int len;
    char *data;
};

#define AUTH_HOST_UX 0x1

// Lavalite authentication type
typedef enum {
	CLIENT_EAUTH = 1
} auth_t;

struct lsfAuth {
    int uid;
    int gid;
    char lsfUserName[LL_BUFSIZ_64];
    auth_t kind;
    union authBody {
        int filler;
        struct lenData authToken;
        struct eauth {
            int len;
            char data[LL_BUFSIZ_4K];
        } eauth;
    } k;
    int options;
};

struct lsfLimit {
    int rlim_curl;
    int rlim_curh;
    int rlim_maxl;
    int rlim_maxh;
};
