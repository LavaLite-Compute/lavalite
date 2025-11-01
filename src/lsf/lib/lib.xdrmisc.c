/* $Id: lib.xdrmisc.c,v 1.4 2007/08/15 22:18:51 tmizan Exp $
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

bool_t
xdr_stat(XDR *xdrs, struct stat *st, struct packet_header *hdr)
{
    int i;

    if (xdrs->x_op == XDR_ENCODE)
        i = st->st_dev;
    if (!xdr_int(xdrs, &i))
        return false;
    if (xdrs->x_op == XDR_DECODE)
        st->st_dev = i;

    if (xdrs->x_op == XDR_ENCODE)
        i = st->st_ino;
    if (!xdr_int(xdrs, &i))
        return false;
    if (xdrs->x_op == XDR_DECODE)
        st->st_ino = i;

    if (xdrs->x_op == XDR_ENCODE)
        i = st->st_mode;
    if (!xdr_int(xdrs, &i))
        return false;
    if (xdrs->x_op == XDR_DECODE)
        st->st_mode = i;

    if (xdrs->x_op == XDR_ENCODE)
        i = st->st_nlink;
    if (!xdr_int(xdrs, &i))
        return false;
    if (xdrs->x_op == XDR_DECODE)
        st->st_nlink = i;

    if (xdrs->x_op == XDR_ENCODE)
        i = st->st_uid;
    if (!xdr_int(xdrs, &i))
        return false;
    if (xdrs->x_op == XDR_DECODE)
        st->st_uid = i;

    if (xdrs->x_op == XDR_ENCODE)
        i = st->st_gid;
    if (!xdr_int(xdrs, &i))
        return false;
    if (xdrs->x_op == XDR_DECODE)
        st->st_gid = i;

    if (xdrs->x_op == XDR_ENCODE)
        i = st->st_rdev;

    if (!xdr_int(xdrs, &i))
        return false;
    if (xdrs->x_op == XDR_DECODE)
        st->st_rdev = i;

    if (xdrs->x_op == XDR_ENCODE)
        i = st->st_size;
    if (!xdr_int(xdrs, &i))
        return false;
    if (xdrs->x_op == XDR_DECODE)
        st->st_size = i;

    if (xdrs->x_op == XDR_ENCODE)
        i = st->st_atime;
    if (!xdr_int(xdrs, &i))
        return false;
    if (xdrs->x_op == XDR_DECODE)
        st->st_atime = i;

    if (xdrs->x_op == XDR_ENCODE)
        i = st->st_mtime;
    if (!xdr_int(xdrs, &i))
        return false;
    if (xdrs->x_op == XDR_DECODE)
        st->st_mtime = i;

    if (xdrs->x_op == XDR_ENCODE)
        i = st->st_ctime;
    if (!xdr_int(xdrs, &i))
        return false;
    if (xdrs->x_op == XDR_DECODE)
        st->st_ctime = i;

    return true;
}

bool_t
xdr_lsfRusage(XDR *xdrs, struct lsfRusage *lsfRu)
{

    if (!(xdr_double(xdrs, &lsfRu->ru_utime) &&
          xdr_double(xdrs, &lsfRu->ru_stime) &&
          xdr_double(xdrs, &lsfRu->ru_maxrss) &&
          xdr_double(xdrs, &lsfRu->ru_ixrss) &&
          xdr_double(xdrs, &lsfRu->ru_ismrss) &&
          xdr_double(xdrs, &lsfRu->ru_idrss) &&
          xdr_double(xdrs, &lsfRu->ru_isrss) &&
          xdr_double(xdrs, &lsfRu->ru_minflt) &&
          xdr_double(xdrs, &lsfRu->ru_majflt) &&
          xdr_double(xdrs, &lsfRu->ru_nswap) &&
          xdr_double(xdrs, &lsfRu->ru_inblock) &&
          xdr_double(xdrs, &lsfRu->ru_oublock) &&
          xdr_double(xdrs, &lsfRu->ru_ioch) &&
          xdr_double(xdrs, &lsfRu->ru_msgsnd) &&
          xdr_double(xdrs, &lsfRu->ru_msgrcv) &&
          xdr_double(xdrs, &lsfRu->ru_nsignals) &&
          xdr_double(xdrs, &lsfRu->ru_nvcsw) &&
          xdr_double(xdrs, &lsfRu->ru_nivcsw) &&
          xdr_double(xdrs, &lsfRu->ru_exutime)))
        return false;
    return true;
}

bool_t
xdr_var_string(XDR *xdrs, char **astring)
{
    int pos, len;

    if (xdrs->x_op == XDR_FREE) {
        FREEUP(*astring);
        return true;
    }

    if (xdrs->x_op == XDR_DECODE) {
        pos = XDR_GETPOS(xdrs);
        *astring = NULL;
        if (! xdr_int(xdrs, &len) ||
            ((*astring = malloc(len + 1)) == NULL) )
            return false;
        XDR_SETPOS(xdrs, pos);

    } else {
        len = strlen(*astring);
    }

    if (! xdr_string(xdrs, astring, len + 1)) {
        if (xdrs->x_op == XDR_DECODE) {
            FREEUP(*astring);
        }
        return false;
    }
    return true;
}

bool_t
xdr_lenData(XDR *xdrs, struct lenData *ld)
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

bool_t
xdr_lsfAuth(XDR *xdrs, struct lsfAuth *auth, struct packet_header *hdr)
{

    char  *sp;

    sp = auth->lsfUserName;
    if (xdrs->x_op == XDR_DECODE)
        sp[0] = '\0';

    if (!(xdr_int(xdrs, &auth->uid)
          && xdr_int(xdrs, &auth->gid)
          && xdr_string(xdrs, &sp, MAXLSFNAMELEN))) {
        return false;
    }

    if (!xdr_enum(xdrs, (int *) &auth->kind))
        return false;

    if (!xdr_int(xdrs, &auth->k.eauth.len))
        return false;

    sp = auth->k.eauth.data;
    if (!xdr_bytes(xdrs, &sp, (u_int *)&auth->k.eauth.len, auth->k.eauth.len))
        return false;

    if (xdrs->x_op == XDR_ENCODE) {
        auth->options = AUTH_HOST_UX;
    }

    if (!xdr_int(xdrs, &auth->options)) {
        return false;
    }

    return true;
}

int
xdr_lsfAuthSize(struct lsfAuth *auth)
{
    if (auth == NULL)
        return 0;

    int sz = ALIGNWORD_(sizeof(auth->uid))
        + ALIGNWORD_(sizeof(auth->gid))
        + ALIGNWORD_(strlen(auth->lsfUserName))
        + ALIGNWORD_(sizeof(auth->kind))
        + ALIGNWORD_(sizeof(auth->k.eauth.len))
        + ALIGNWORD_(auth->k.eauth.len)
        + ALIGNWORD_(sizeof(auth->options));

    return sz;
}

bool_t
xdr_pidInfo(XDR *xdrs, struct pidInfo *pidInfo, struct packet_header *hdr)
{

    if (! xdr_int(xdrs, &pidInfo->pid))
        return false;
    if (! xdr_int(xdrs, &pidInfo->ppid))
        return false;

    if (! xdr_int(xdrs, &pidInfo->pgid))
        return false;

    if (! xdr_int(xdrs, &pidInfo->jobid))
        return false;

    return true;
}

bool_t
xdr_jRusage(XDR *xdrs, struct jRusage *runRusage, struct packet_header *hdr)
{
    int i;

    if (xdrs->x_op == XDR_FREE) {
        FREEUP (runRusage->pidInfo);
        FREEUP (runRusage->pgid);
        return true;
    }

    if (xdrs->x_op == XDR_DECODE) {
        runRusage->pidInfo = NULL;
        runRusage->pgid = NULL;
    }

    if (!(xdr_int(xdrs, &runRusage->mem) &&
          xdr_int(xdrs, &runRusage->swap) &&
          xdr_int(xdrs, &runRusage->utime) &&
          xdr_int(xdrs, &runRusage->stime)))
        return false;

    if (!(xdr_int(xdrs, &runRusage->npids)))
        return false;

    if (xdrs->x_op == XDR_DECODE && runRusage->npids) {
        runRusage->pidInfo = (struct pidInfo *) calloc (runRusage->npids, sizeof(struct pidInfo));
        if (runRusage->pidInfo == NULL) {
            runRusage->npids = 0;
            return false;
        }
    }

    for (i = 0; i < runRusage->npids; i++) {
        if (!xdr_arrayElement(xdrs,
                              (char *)&(runRusage->pidInfo[i]),
                              hdr,
                              xdr_pidInfo)) {
            if (xdrs->x_op == XDR_DECODE)  {
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
        if (! xdr_arrayElement(xdrs, (char *) &(runRusage->pgid[i]),  hdr, xdr_int)) {
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
