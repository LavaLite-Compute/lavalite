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
#include "lsf/lib/protocol.h"

struct LSFHeader {
    unsigned short refCode;
    short opCode;
    unsigned int length;
    unsigned char version;
    struct reserved0 {
        unsigned char High;
        unsigned short  Low;
    } reserved0;
};

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
    char lsfUserName[BUFSIZ_64];
    auth_t kind;
    union authBody {
        int filler;
        struct lenData authToken;
        struct eauth {
            int len;
            char data[BUFSIZ_4K];
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


extern bool_t xdr_LSFHeader(XDR *, struct packet_header *);
extern bool_t xdr_packLSFHeader(char *, struct packet_header *);

extern bool_t xdr_encodeMsg(XDR *, char *, struct packet_header *,
                            bool_t (*)(), int, struct lsfAuth *);
extern bool_t xdr_arrayElement(XDR *, char *, struct packet_header *,
                               bool_t (*)(), ...);
extern bool_t xdr_stringLen(XDR *, struct stringLen *, struct packet_header *);
extern bool_t xdr_stat(XDR *, struct stat *, struct packet_header *);
extern bool_t xdr_lsfAuth(XDR *, struct lsfAuth *, struct packet_header *);
extern int xdr_lsfAuthSize(struct lsfAuth *);
extern bool_t xdr_jRusage(XDR *, struct jRusage *, struct packet_header *);
