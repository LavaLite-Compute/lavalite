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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 USA
 *
 */
#include "lsf/lib/lib.h"

// Clarity over cleverness; minimal code, maximal intent

// Protocol function
static bool_t marshal_packet_header(XDR *xdrs, struct packet_header *header);

bool_t xdr_pack_hdr(XDR *xdrs, struct packet_header *header)
{
    if (xdrs->x_op == XDR_ENCODE) {
        header->version = CURRENT_PROTOCOL_VERSION;
        if (!marshal_packet_header(xdrs, header)) {
            lserrno = LSE_BAD_XDR;
            return false;
        }
        return true;
    }

    if (xdrs->x_op == XDR_DECODE) {
        if (!marshal_packet_header(xdrs, header)) {
            lserrno = LSE_BAD_XDR;
            return false;
        }
        // The protocol version comes in the packet
    }

    return true;
}

static bool_t marshal_packet_header(XDR *xdrs, struct packet_header *header)
{
    if (!xdr_int32_t(xdrs, &header->sequence))
        return false;
    if (!xdr_int32_t(xdrs, &header->operation))
        return false;
    if (!xdr_int32_t(xdrs, &header->version))
        return false;
    if (!xdr_int32_t(xdrs, &header->length))
        return false;
    if (!xdr_int(xdrs, &header->reserved))
        return false;
    return true;
}

bool_t xdr_encodeMsg(XDR *xdrs, char *data, struct packet_header *hdr,
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
    if (!xdr_pack_hdr(xdrs, hdr))
        return false;
    XDR_SETPOS(xdrs, len);
    return true;
}

// Clear intent, minimal indirection
bool_t xdr_array_string(XDR *xdrs, char **str, int maxlen, int arraysize)
{
    uint32_t cap = (maxlen > 0) ? (uint32_t) maxlen : UINT_MAX;

    for (int i = 0; i < arraysize; i++) {
        switch (xdrs->x_op) {
        case XDR_DECODE:
            str[i] = NULL;
            if (!xdr_string(xdrs, &str[i], cap)) {
                for (int j = 0; j < i; j++) {
                    FREEUP(str[j]);
                    str[j] = NULL;
                }
                return false;
            }
            break;

        case XDR_ENCODE: {
            char *tmp = str[i] ? str[i] : (char *) "";
            if (!xdr_string(xdrs, &tmp, cap))
                return false;
            break;
        }

        case XDR_FREE:
            if (str[i]) {
                free(str[i]);
                str[i] = NULL;
            }
            break;
        }
    }
    return true;
}

bool_t xdr_time_t(XDR *xdrs, time_t *t)
{
    return xdr_int64_t(xdrs, (int64_t *) t);
}

#if 0
int readDecodeHdr_(int s, char *buf, ssize_t (*readFunc)(), XDR *xdrs,
                   struct packet_header *hdr)
{
    if ((*readFunc)(s, buf, PACKET_HEADER_SIZE) != PACKET_HEADER_SIZE) {
        lserrno = LSE_MSG_SYS;
        return -2;
    }

    if (!xdr_pack_hdr(xdrs, hdr)) {
        lserrno = LSE_BAD_XDR;
        return -1;
    }

    return 0;
}

// Encode a packet header with XDR and write it out.
// Returns number of bytes written (like write()), or -1 on error.
ssize_t writeEncodeHdr_(int s, struct packet_header *hdr,
                        ssize_t (*writeFunc)())
{
    XDR xdrs;
    struct packet_header buf;

    init_pack_hdr(&buf);
    xdrmem_create(&xdrs, (char *)&buf, PACKET_HEADER_SIZE, XDR_ENCODE);

    if (!xdr_pack_hdr(&xdrs, hdr)) {
        lserrno = LSE_BAD_XDR;
        xdr_destroy(&xdrs);
        return -1;
    }

    xdr_destroy(&xdrs);

    /* In theory, we should use XDR_GETPOS(&xdrs) to know how many bytes
     * we actually encoded, but PACKET_HEADER_SIZE is fixed and aligned
     * for the header, so writing the full buffer is fine.
     */
    int cc = (*writeFunc)(s, (char *) &buf, PACKET_HEADER_SIZE);
    if (cc != PACKET_HEADER_SIZE) {
        lserrno = LSE_MSG_SYS;
        return -1;
    }

    return cc;
}
#endif

int send_packet_header(int chfd, struct packet_header *hdr)
{
    XDR xdrs;
    char buf[PACKET_HEADER_SIZE];

    xdrmem_create(&xdrs, buf, PACKET_HEADER_SIZE, XDR_ENCODE);
    if (!xdr_pack_hdr(&xdrs, hdr)) {
        xdr_destroy(&xdrs);
        lserrno = LSE_BAD_XDR;
        return -1;
    }
    xdr_destroy(&xdrs);

    if (chan_write(chfd, buf, PACKET_HEADER_SIZE) != PACKET_HEADER_SIZE) {
        lserrno = LSE_MSG_SYS;
        return -1;
    }

    return 0;
}

int recv_packet_header(int chfd, struct packet_header *hdr)
{
    XDR xdrs;
    char buf[PACKET_HEADER_SIZE];

    if (chan_read(chfd, buf, PACKET_HEADER_SIZE) != PACKET_HEADER_SIZE) {
        lserrno = LSE_MSG_SYS;
        return -1;
    }

    xdrmem_create(&xdrs, (char *) buf, PACKET_HEADER_SIZE, XDR_DECODE);
    if (!xdr_pack_hdr(&xdrs, hdr)) {
        xdr_destroy(&xdrs);
        lserrno = LSE_BAD_XDR;
        return -1;
    }
    xdr_destroy(&xdrs);

    return 0;
}

bool_t xdr_stringLen(XDR *xdrs, struct stringLen *str,
                     struct packet_header *Hdr)
{
    if (xdrs->x_op == XDR_DECODE)
        str->name[0] = '\0';

    if (!xdr_string(xdrs, &str->name, str->len))
        return false;

    return true;
}

bool_t xdr_lsfLimit(XDR *xdrs, struct lsfLimit *limits,
                    struct packet_header *hdr)
{
    if (!xdr_int(xdrs, &limits->rlim_curl))
        return false;
    if (!xdr_int(xdrs, &limits->rlim_curh))
        return false;
    if (!xdr_int(xdrs, &limits->rlim_maxl))
        return false;
    if (!xdr_int(xdrs, &limits->rlim_maxh))
        return false;

    return true;
}

bool_t xdr_portno(XDR *xdrs, uint16_t *port)
{
    uint32_t n;

    if (xdrs->x_op == XDR_ENCODE)
        n = (uint32_t) ntohs(*port);

    if (!xdr_uint32_t(xdrs, &n))
        return false;

    if (xdrs->x_op == XDR_DECODE) {
        // check if size is not greater than max port
        // and also reject privilege port
        // reject 0..1023 and >65535
        if (n < 1024 || n > 65535)
            return false;
        *port = htons((uint16_t) n);
    }
    return true;
}

bool_t xdr_sockaddr_in(XDR *xdrs, struct sockaddr_in *addr)
{
    uint32_t fam;
    uint32_t port;
    uint32_t ip;

    if (xdrs->x_op == XDR_ENCODE) {
        fam = (uint32_t) addr->sin_family;
        port = (uint32_t) ntohs(addr->sin_port);
        ip = (uint32_t) ntohl(addr->sin_addr.s_addr);
    }

    if (!xdr_uint32_t(xdrs, &fam))
        return false;
    if (!xdr_uint32_t(xdrs, &port))
        return false;
    if (!xdr_uint32_t(xdrs, &ip))
        return false;

    if (xdrs->x_op == XDR_DECODE) {
        addr->sin_family = (sa_family_t) fam;
        addr->sin_port = htons((uint16_t) port);
        addr->sin_addr.s_addr = htonl(ip);
        memset(addr->sin_zero, 0, sizeof(addr->sin_zero));
    }

    return true;
}

bool_t xdr_debugReq(XDR *xdrs, struct debugReq *debugReq,
                    struct packet_header *hdr)
{
    static char *sp = NULL;
    static char *phostname = NULL;

    sp = debugReq->logFileName;

    if (xdrs->x_op == XDR_DECODE) {
        debugReq->logFileName[0] = '\0';

        if (phostname == NULL) {
            phostname = (char *) malloc(MAXHOSTNAMELEN);
            if (phostname == NULL)
                return false;
        }
        debugReq->hostName = phostname;
        phostname[0] = '\0';

    }

    else
        phostname = debugReq->hostName;

    if (!(xdr_int(xdrs, &debugReq->opCode) && xdr_int(xdrs, &debugReq->level) &&
          xdr_int(xdrs, &debugReq->logClass) &&
          xdr_int(xdrs, &debugReq->options) &&
          xdr_string(xdrs, &phostname, MAXHOSTNAMELEN) &&
          xdr_string(xdrs, &sp, MAXPATHLEN)))
        return false;

    return true;
}

void xdr_lsffree(bool_t (*xdr_func)(), void *ptr, struct packet_header *hdr)
{
    XDR xdrs;

    xdrmem_create(&xdrs, NULL, 0, XDR_FREE);

    (*xdr_func)(&xdrs, ptr, hdr);

    xdr_destroy(&xdrs);
}

bool_t xdr_array_element(XDR *xdrs,     // the stream
                         void *data,    // the struct on the wore
                         void *ctx,     // some extra if needed
                         bool_t (*f)()) // marshal function
{
    if (!f(xdrs, data, ctx))
        return false;

    return true;
}

bool_t xdr_lsfRusage(XDR *xdrs, struct lsfRusage *lsfRu, void *)
{
    if (!xdr_double(xdrs, &lsfRu->ru_utime))
        return false;

    if (!xdr_double(xdrs, &lsfRu->ru_stime))
        return false;

    if (!xdr_double(xdrs, &lsfRu->ru_maxrss))
        return false;

    if (!xdr_double(xdrs, &lsfRu->ru_ixrss))
        return false;

    if (!xdr_double(xdrs, &lsfRu->ru_ismrss))
        return false;

    if (!xdr_double(xdrs, &lsfRu->ru_idrss))
        return false;

    if (!xdr_double(xdrs, &lsfRu->ru_isrss))
        return false;

    if (!xdr_double(xdrs, &lsfRu->ru_minflt))
        return false;

    if (!xdr_double(xdrs, &lsfRu->ru_majflt))
        return false;

    if (!xdr_double(xdrs, &lsfRu->ru_nswap))
        return false;

    if (!xdr_double(xdrs, &lsfRu->ru_inblock))
        return false;

    if (!xdr_double(xdrs, &lsfRu->ru_oublock))
        return false;

    if (!xdr_double(xdrs, &lsfRu->ru_ioch))
        return false;

    if (!xdr_double(xdrs, &lsfRu->ru_msgsnd))
        return false;

    if (!xdr_double(xdrs, &lsfRu->ru_msgrcv))
        return false;

    if (!xdr_double(xdrs, &lsfRu->ru_nsignals))
        return false;

    if (!xdr_double(xdrs, &lsfRu->ru_nvcsw))
        return false;

    if (!xdr_double(xdrs, &lsfRu->ru_nivcsw))
        return false;

    if (!xdr_double(xdrs, &lsfRu->ru_exutime))
        return false;

    return true;
}

bool_t xdr_var_string(XDR *xdrs, char **str)
{
    if (xdrs->x_op == XDR_ENCODE) {
        char *tmp = (*str) ? *str : (char *) "";
        return xdr_wrapstring(xdrs, &tmp);
    }
    if (xdrs->x_op == XDR_DECODE) {
        // set str to NULL otherwise if the caller malloc it
        // and it points to random bytes the xdr_wrapstring segfaults
        *str = NULL;
        return xdr_wrapstring(xdrs, str);
    }
    if (xdrs->x_op == XDR_FREE) {
        return xdr_wrapstring(xdrs, str);
    }
    return true;
}

bool_t xdr_lenData(XDR *xdrs, struct lenData *ld)
{
    char *sp;

    if (!xdr_int(xdrs, &ld->len))
        return false;

    if (xdrs->x_op == XDR_FREE) {
        FREEUP(ld->data);
        return true;
    }

    if (ld->len == 0) {
        ld->data = NULL;
        return true;
    }

    if (xdrs->x_op == XDR_DECODE) {
        if ((ld->data = (char *) malloc(ld->len)) == NULL)
            return false;
    }

    sp = ld->data;
    if (!xdr_bytes(xdrs, &sp, (u_int *) &ld->len, ld->len)) {
        if (xdrs->x_op == XDR_DECODE)
            FREEUP(ld->data);
        return false;
    }

    return true;
}

bool_t xdr_lsfAuth(XDR *xdrs, struct lsfAuth *auth, struct packet_header *hdr)
{
    char *sp;

    sp = auth->lsfUserName;
    if (xdrs->x_op == XDR_DECODE)
        sp[0] = '\0';

    if (!(xdr_int(xdrs, &auth->uid) && xdr_int(xdrs, &auth->gid) &&
          xdr_string(xdrs, &sp, MAXLSFNAMELEN))) {
        return false;
    }

    if (!xdr_enum(xdrs, (int *) &auth->kind))
        return false;

    if (!xdr_int(xdrs, &auth->k.eauth.len))
        return false;

    sp = auth->k.eauth.data;
    if (!xdr_bytes(xdrs, &sp, (u_int *) &auth->k.eauth.len, auth->k.eauth.len))
        return false;

    if (xdrs->x_op == XDR_ENCODE) {
        auth->options = AUTH_HOST_UX;
    }

    if (!xdr_int(xdrs, &auth->options)) {
        return false;
    }

    return true;
}

int xdr_lsfAuthSize(struct lsfAuth *auth)
{
    if (auth == NULL)
        return 0;

    int sz = ALIGNWORD_(sizeof(auth->uid)) + ALIGNWORD_(sizeof(auth->gid)) +
             ALIGNWORD_(strlen(auth->lsfUserName)) +
             ALIGNWORD_(sizeof(auth->kind)) +
             ALIGNWORD_(sizeof(auth->k.eauth.len)) +
             ALIGNWORD_(auth->k.eauth.len) + ALIGNWORD_(sizeof(auth->options));

    return sz;
}

static bool_t xdr_pidInfo(XDR *xdrs, struct pidInfo *pidInfo, void *ctx)
{
    if (!xdr_int(xdrs, &pidInfo->pid))
        return false;
    if (!xdr_int(xdrs, &pidInfo->ppid))
        return false;

    if (!xdr_int(xdrs, &pidInfo->pgid))
        return false;

    if (!xdr_int(xdrs, &pidInfo->jobid))
        return false;

    return true;
}

bool_t xdr_jRusage(XDR *xdrs, struct jRusage *runRusage, void *)
{
    int i;

    if (xdrs->x_op == XDR_FREE) {
        FREEUP(runRusage->pidInfo);
        FREEUP(runRusage->pgid);
        return true;
    }

    if (xdrs->x_op == XDR_DECODE) {
        runRusage->pidInfo = NULL;
        runRusage->pgid = NULL;
    }

    if (!(xdr_int(xdrs, &runRusage->mem) && xdr_int(xdrs, &runRusage->swap) &&
          xdr_int(xdrs, &runRusage->utime) && xdr_int(xdrs, &runRusage->stime)))
        return false;

    if (!(xdr_int(xdrs, &runRusage->npids)))
        return false;

    if (xdrs->x_op == XDR_DECODE && runRusage->npids) {
        runRusage->pidInfo = calloc(runRusage->npids, sizeof(struct pidInfo));
        if (runRusage->pidInfo == NULL) {
            runRusage->npids = 0;
            return false;
        }
    }

    for (i = 0; i < runRusage->npids; i++) {
        if (!xdr_array_element(xdrs, (void *) &(runRusage->pidInfo[i]), NULL,
                               xdr_pidInfo)) {
            if (xdrs->x_op == XDR_DECODE) {
                FREEUP(runRusage->pidInfo);
                runRusage->npids = 0;
                runRusage->pidInfo = NULL;
            }
            return false;
        }
    }

    if (!(xdr_int(xdrs, &runRusage->npgids)))
        return false;

    if (xdrs->x_op == XDR_DECODE && runRusage->npgids) {
        runRusage->pgid = calloc(runRusage->npgids, sizeof(int));
        if (runRusage->pgid == NULL) {
            runRusage->npgids = 0;
            return false;
        }
    }

    for (i = 0; i < runRusage->npgids; i++) {
        if (!xdr_array_element(xdrs, (void *) &(runRusage->pgid[i]), NULL,
                               xdr_int)) {
            if (xdrs->x_op == XDR_DECODE) {
                FREEUP(runRusage->pgid);
                runRusage->npgids = 0;
                runRusage->pgid = NULL;
            }
            return false;
        }
    }
    return true;
}
