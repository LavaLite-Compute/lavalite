/* $Id: lib.xdr.c,v 1.4 2007/08/15 22:18:51 tmizan Exp $
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
#include "lsf/lib/lib.h"

static bool marshal_packet_header(XDR *xdrs,
                                  struct packet_header *header);

bool_t
xdr_LSFHeader(XDR *xdrs, struct packet_header *header)
{
    if (xdrs->x_op == XDR_ENCODE) {
        header->version = CURRENT_PROTOCOL_VERSION;
        if (! marshal_packet_header(xdrs, header)) {
            lserrno = LSE_BAD_XDR;
            return false;
        }
        return true;
    }

    if (xdrs->x_op == XDR_DECODE) {
        if (! marshal_packet_header(xdrs, header)) {
            lserrno = LSE_BAD_XDR;
            return false;
        }
        // The protocol version comes in the packet
    }

    return true;
}

bool_t
xdr_packLSFHeader(char *buf, struct packet_header *header)
{
    XDR xdrs;
    char hdrBuf[PACKET_HEADER_SIZE];

    xdrmem_create(&xdrs, hdrBuf, PACKET_HEADER_SIZE, XDR_ENCODE);

    if (!xdr_LSFHeader(&xdrs, header)) {
        lserrno = LSE_BAD_XDR;
        xdr_destroy(&xdrs);
        return false;
    }

    memcpy(buf, hdrBuf, XDR_GETPOS(&xdrs));
    xdr_destroy(&xdrs);

    return true;
}

static bool
marshal_packet_header(XDR *xdrs,
                      struct packet_header *header)
{
    if (!xdr_int32_t(xdrs, &header->sequence))
        return false;
    if (!xdr_int32_t(xdrs, &header->operation))
        return false;
    if (!xdr_int32_t(xdrs, &header->version))
        return false;
    if (! xdr_uint32_t(xdrs, &header->length))
        return false;
    if (! xdr_int(xdrs, &header->reserved))
        return false;
    return true;
}

bool_t
xdr_encodeMsg (XDR *xdrs, char *data, struct packet_header *hdr,
               bool_t (*xdr_func)(), int options, struct lsfAuth *auth)
{
    int len;

    XDR_SETPOS(xdrs, PACKET_HEADER_SIZE);

    hdr->version = CURRENT_PROTOCOL_VERSION;

    if (auth) {
        if (!xdr_lsfAuth(xdrs, auth, hdr))
            return false;
    }

    if (data) {
        if (!(*xdr_func)(xdrs, data, hdr))
            return false;
    }

    len = XDR_GETPOS(xdrs);
    hdr->length = len - PACKET_HEADER_SIZE;
    XDR_SETPOS(xdrs, 0);
    if (!xdr_LSFHeader(xdrs, hdr))
        return false;
    XDR_SETPOS(xdrs, len);
    return true;
}

/* Replace with this

   bool_t
   xdr_arrayElement(XDR *xdrs, char *data, struct packet_header *hdr,
                 bool_t (*xdr_func)(XDR *, char *, struct packet_header *, char *), char *cp)
{
    int nextElementOffset, pos = XDR_GETPOS(xdrs);

    if (xdrs->x_op == XDR_ENCODE) {
        XDR_SETPOS(xdrs, pos + NET_INTSIZE_);
    } else {
        if (!xdr_int(xdrs, &nextElementOffset))
            return FALSE;
    }

    if (!(*xdr_func)(xdrs, data, hdr, cp))
        return FALSE;

    if (xdrs->x_op == XDR_ENCODE) {
        nextElementOffset = XDR_GETPOS(xdrs) - pos;
        XDR_SETPOS(xdrs, pos);
        if (!xdr_int(xdrs, &nextElementOffset))
            return FALSE;
    }

    XDR_SETPOS(xdrs, pos + nextElementOffset);
    return TRUE;
}
*/

bool_t
xdr_arrayElement (XDR *xdrs, char *data, struct packet_header *hdr,
                  bool_t (*xdr_func)(), ...)
{
    va_list ap;
    int nextElementOffset, pos;
    char *cp;

    va_start(ap, xdr_func);

    pos = XDR_GETPOS(xdrs);

    if (xdrs->x_op == XDR_ENCODE) {
        XDR_SETPOS(xdrs, pos + NET_INTSIZE_);
    } else {
        if (!xdr_int(xdrs, &nextElementOffset))
            return false;
    }

    cp = va_arg(ap, char *);
    if (cp) {
        if (!(*xdr_func)(xdrs, data, hdr, cp))
            return false;
    } else {
        if (!(*xdr_func)(xdrs, data, hdr))
            return false;
    }

    if (xdrs->x_op == XDR_ENCODE) {
        nextElementOffset = XDR_GETPOS(xdrs) - pos;
        XDR_SETPOS(xdrs, pos);
        if (!xdr_int(xdrs, &nextElementOffset))
            return false;
    }

    XDR_SETPOS(xdrs, pos + nextElementOffset);
    return true;
}

bool_t
xdr_array_string(XDR *xdrs, char **astring, int maxlen, int arraysize)
{
    int i, j;
    char line[MAXLINELEN];
    char *sp = line;

    for (i = 0; i < arraysize; i++) {
        if (xdrs->x_op == XDR_FREE) {
            FREEUP(astring[i]);
        } else if (xdrs->x_op == XDR_DECODE) {
            if (! xdr_string(xdrs, &sp, maxlen)
                || (astring[i] = putstr_(sp)) == NULL) {
                for (j = 0; j < i;j++)
                    FREEUP(astring[j]);
                return false;
            }
        } else {
            if (! xdr_string(xdrs, &astring[i], maxlen))
                return false;
        }
    }
    return true;
}

bool_t
xdr_time_t(XDR *xdrs, time_t *t)
{
    return xdr_int64_t(xdrs, (int64_t *)t);
}

int
readDecodeHdr_(int s, char *buf, ssize_t (*readFunc)(), XDR *xdrs,
               struct packet_header *hdr)
{
    if ((*readFunc)(s, buf, PACKET_HEADER_SIZE) != PACKET_HEADER_SIZE) {
        lserrno = LSE_MSG_SYS;
        return -2;
    }

    if (!xdr_LSFHeader(xdrs, hdr)) {
        lserrno = LSE_BAD_XDR;
        return -1;
    }

    return 0;
}

int
readDecodeMsg_(int s, char *buf, struct packet_header *hdr, ssize_t (*readFunc)(),
               XDR *xdrs, char *data, bool_t (*xdrFunc)(),
               struct lsfAuth *auth)
{
    if ((*readFunc)(s, buf, hdr->length) != hdr->length) {
        lserrno = LSE_MSG_SYS;
        return -2;
    }

    if (auth) {
        if (!xdr_lsfAuth(xdrs, auth, hdr)) {
            lserrno = LSE_BAD_XDR;
            return -1;
        }
    }

    if (!(*xdrFunc)(xdrs, data, hdr)) {
        lserrno = LSE_BAD_XDR;
        return -1;
    }

    return 0;
}

int
writeEncodeMsg_(int s, char *buf, int len, struct packet_header *hdr, char *data,
                ssize_t (*writeFunc)(), bool_t (*xdrFunc)(), int options)
{
    XDR xdrs;

    xdrmem_create(&xdrs, buf, len, XDR_ENCODE);

    if (!xdr_encodeMsg(&xdrs, data, hdr, xdrFunc, options, NULL)) {
        lserrno = LSE_BAD_XDR;
        xdr_destroy(&xdrs);
        return -1;
    }

    if ((*writeFunc)(s, buf, XDR_GETPOS(&xdrs)) != XDR_GETPOS(&xdrs)) {
        lserrno = LSE_MSG_SYS;
        xdr_destroy(&xdrs);
        return -2;
    }

    xdr_destroy(&xdrs);
    return 0;
}

int
writeEncodeHdr_(int s, struct packet_header *hdr, ssize_t (*writeFunc)())
{
    XDR xdrs;
    struct packet_header buf;

    initLSFHeader_(&buf);
    hdr->length = 0;
    xdrmem_create(&xdrs, (char *) &buf, PACKET_HEADER_SIZE, XDR_ENCODE);

    if (!xdr_LSFHeader(&xdrs, hdr)) {
        lserrno = LSE_BAD_XDR;
        xdr_destroy(&xdrs);
        return -1;
    }

    xdr_destroy(&xdrs);

    if ((*writeFunc)(s, (char *) &buf, PACKET_HEADER_SIZE) != PACKET_HEADER_SIZE) {
        lserrno = LSE_MSG_SYS;
        return -2;
    }

    return 0;
}

bool_t
xdr_stringLen(XDR *xdrs, struct stringLen *str, struct packet_header *Hdr)
{
    if (xdrs->x_op == XDR_DECODE)
        str->name[0] = '\0';

    if (!xdr_string(xdrs, &str->name, str->len))
        return false;

    return true;
}

bool_t
xdr_lsfLimit (XDR *xdrs, struct lsfLimit *limits, struct packet_header *hdr)
{
    if (!(xdr_u_int(xdrs, (unsigned int*)&limits->rlim_curl) &&
          xdr_u_int(xdrs, (unsigned int*)&limits->rlim_curh) &&
          xdr_u_int(xdrs, (unsigned int*)&limits->rlim_maxl) &&
          xdr_u_int(xdrs, (unsigned int*)&limits->rlim_maxh)))
        return false;
    return true;
}

bool_t
xdr_portno (XDR *xdrs, u_short *portno)
{
    u_int len = 2;
    char *sp;

    if (xdrs->x_op == XDR_DECODE)
        *portno = 0;

    sp = (char *) portno;

    return (xdr_bytes(xdrs, &sp, &len, len));
}

bool_t
xdr_address (XDR *xdrs, u_int *addr)
{
    u_int len = NET_INTSIZE_;
    char *sp;
    if (xdrs->x_op == XDR_DECODE)
        *addr = 0;

    sp = (char *) addr;

    return (xdr_bytes(xdrs, &sp, &len, len));
}

bool_t
xdr_debugReq (XDR *xdrs, struct debugReq  *debugReq,
              struct packet_header *hdr)
{

    static char *sp = NULL;
    static char *phostname = NULL;

    sp = debugReq->logFileName;

    if (xdrs->x_op == XDR_DECODE) {
        debugReq->logFileName[0] = '\0';

        if (phostname == NULL) {
            phostname = (char *) malloc (MAXHOSTNAMELEN);
            if (phostname == NULL)
                return false;
        }
        debugReq->hostName = phostname;
        phostname[0] = '\0';

    }

    else
        phostname = debugReq->hostName;

    if (!(xdr_int (xdrs, &debugReq->opCode)
          && xdr_int(xdrs, &debugReq->level)
          && xdr_int(xdrs, &debugReq->logClass)
          && xdr_int(xdrs, &debugReq->options)
          && xdr_string(xdrs, &phostname, MAXHOSTNAMELEN)
          && xdr_string(xdrs, &sp, MAXPATHLEN)))
        return false;

    return true;

}

void
xdr_lsffree(bool_t (*xdr_func)(), char *objp, struct packet_header *hdr)
{

    XDR xdrs;

    xdrmem_create(&xdrs, NULL, 0, XDR_FREE);

    (*xdr_func)(&xdrs, objp, hdr);

    xdr_destroy(&xdrs);
}
