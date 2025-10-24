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

static void encodeHdr (unsigned int *word1,
                       unsigned int *word2,
                       unsigned int *word3,
                       struct LSFHeader *header);

bool_t
xdr_LSFHeader (XDR *xdrs, struct LSFHeader *header)
{
    unsigned int word1, word2, word3;

    if (xdrs->x_op == XDR_ENCODE) {
        header->version = _XDR_VERSION_0_1_0;
        encodeHdr(&word1, &word2, &word3, header);
    }

    if (!(xdr_u_int(xdrs, &word1) &&
          xdr_u_int(xdrs, &word2) &&
          xdr_u_int(xdrs, &word3)))
        return false;

    if (xdrs->x_op == XDR_DECODE) {
        header->refCode = word1 >> 16;
        header->opCode = word1 & 0xFFFF;
        header->length = word2;
        header->version = word3 >> 24;
        header->reserved0.High  = (word3 >> 16) & 0xFF;
        header->reserved0.Low = word3 & 0xFFFF;
    }

    return true;
}

bool_t
xdr_packLSFHeader (char *buf, struct LSFHeader *header)
{
    XDR xdrs;
    char hdrBuf[LSF_HEADER_LEN];

    xdrmem_create(&xdrs, hdrBuf, LSF_HEADER_LEN, XDR_ENCODE);

    if (!xdr_LSFHeader(&xdrs, header)) {
        lserrno = LSE_BAD_XDR;
        xdr_destroy(&xdrs);
        return false;
    }

    memcpy(buf, hdrBuf, XDR_GETPOS(&xdrs));
    xdr_destroy(&xdrs);

    return true;
}

static
void encodeHdr (unsigned int *word1,
                unsigned int *word2,
                unsigned int *word3,
                struct LSFHeader *header)
{
    *word1 = header->refCode;
    *word1 = *word1 << 16;
    *word1 = *word1 | (header->opCode & 0x0000FFFF);
    *word2 = header->length;
    *word3 = header->version;
    *word3 = *word3 << 8;
    *word3 = *word3 | (header->reserved0.High & 0x000000FF);
    *word3 = *word3 << 16;
    *word3 = *word3 | (header->reserved0.Low & 0x0000FFFF);

}

bool_t
xdr_encodeMsg (XDR *xdrs, char *data, struct LSFHeader *hdr,
               bool_t (*xdr_func)(), int options, struct lsfAuth *auth)
{
    int len;

    XDR_SETPOS(xdrs, LSF_HEADER_LEN);

    hdr->version = _XDR_VERSION_0_1_0;

    if (auth) {
        if (!xdr_lsfAuth(xdrs, auth, hdr))
            return false;
    }

    if (data) {
        if (!(*xdr_func)(xdrs, data, hdr))
            return false;
    }

    len = XDR_GETPOS(xdrs);
    if (!(options & ENMSG_USE_LENGTH))
        hdr->length = len - LSF_HEADER_LEN;
    XDR_SETPOS(xdrs, 0);
    if (!xdr_LSFHeader(xdrs, hdr))
        return false;
    XDR_SETPOS(xdrs, len);
    return true;
}

bool_t
xdr_arrayElement (XDR *xdrs, char *data, struct LSFHeader *hdr,
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
    return xdr_u_long(xdrs, (unsigned long *) t);
}

int
readDecodeHdr_(int s, char *buf, ssize_t (*readFunc)(), XDR *xdrs,
               struct LSFHeader *hdr)
{
    if ((*readFunc)(s, buf, LSF_HEADER_LEN) != LSF_HEADER_LEN) {
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
readDecodeMsg_(int s, char *buf, struct LSFHeader *hdr, ssize_t (*readFunc)(),
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
writeEncodeMsg_(int s, char *buf, int len, struct LSFHeader *hdr, char *data,
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
writeEncodeHdr_(int s, struct LSFHeader *hdr, ssize_t (*writeFunc)())
{
    XDR xdrs;
    struct LSFHeader buf;

    initLSFHeader_(&buf);
    hdr->length = 0;
    xdrmem_create(&xdrs, (char *) &buf, LSF_HEADER_LEN, XDR_ENCODE);

    if (!xdr_LSFHeader(&xdrs, hdr)) {
        lserrno = LSE_BAD_XDR;
        xdr_destroy(&xdrs);
        return -1;
    }

    xdr_destroy(&xdrs);

    if ((*writeFunc)(s, (char *) &buf, LSF_HEADER_LEN) != LSF_HEADER_LEN) {
        lserrno = LSE_MSG_SYS;
        return -2;
    }

    return 0;
}

bool_t
xdr_stringLen(XDR *xdrs, struct stringLen *str, struct LSFHeader *Hdr)
{
    if (xdrs->x_op == XDR_DECODE)
        str->name[0] = '\0';

    if (!xdr_string(xdrs, &str->name, str->len))
        return false;

    return true;
}

bool_t
xdr_lsfLimit (XDR *xdrs, struct lsfLimit *limits, struct LSFHeader *hdr)
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
              struct LSFHeader *hdr)
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
xdr_lsffree(bool_t (*xdr_func)(), char *objp, struct LSFHeader *hdr)
{

    XDR xdrs;

    xdrmem_create(&xdrs, NULL, 0, XDR_FREE);

    (*xdr_func)(&xdrs, objp, hdr);

    xdr_destroy(&xdrs);
}

int
getXdrStrlen(char *str)
{

    if (str == NULL)
        return 4;
    return((strlen(str)+7)/4*4);
}

int
getHdrReserved(struct LSFHeader *hdr)
{
    unsigned int word;

    word = hdr->reserved0.High;
    word = word << 16;
    word = word | (hdr->reserved0.Low & 0x0000FFFF);

    return word;
}
