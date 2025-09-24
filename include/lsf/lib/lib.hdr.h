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

#ifndef _LIB_HDR_
#define _LIB_HDR_

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

#define LSF_HEADER_LEN 12

struct stringLen {
    char *name;
    int   len;
};

struct lenData {
    int len;
    char *data;
};

#define AUTH_HOST_NT  0x01
#define AUTH_HOST_UX  0x02

struct lsfAuth {
    int uid;
    int gid;
    char lsfUserName[MAXLSFNAMELEN];
    enum {CLIENT_SETUID, CLIENT_IDENT, CLIENT_DCE, CLIENT_EAUTH} kind;
    union authBody {
	int filler;
        struct lenData authToken;
	struct eauth {
#define EAUTH_SIZE 4096
	    int len;
	    char data[EAUTH_SIZE];
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


extern bool_t xdr_LSFHeader(XDR *, struct LSFHeader *);
extern bool_t xdr_packLSFHeader(char *, struct LSFHeader *);

#define ENMSG_USE_LENGTH 1

extern bool_t xdr_encodeMsg(XDR *, char *, struct LSFHeader *,
			     bool_t (*)(), int, struct lsfAuth *);
extern bool_t xdr_arrayElement(XDR *, char *, struct LSFHeader *,
				bool_t (*)(), ...);
extern bool_t xdr_stringLen(XDR *, struct stringLen *, struct LSFHeader *);
extern bool_t xdr_stat(XDR *, struct stat *, struct LSFHeader *);
extern bool_t xdr_lsfAuth(XDR *, struct lsfAuth *, struct LSFHeader *);
extern int xdr_lsfAuthSize(struct lsfAuth *);
extern bool_t xdr_jRusage(XDR *, struct jRusage *, struct LSFHeader *);

#endif
