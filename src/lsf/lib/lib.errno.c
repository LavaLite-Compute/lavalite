/* $Id: lib.errno.c,v 1.3 2007/08/15 22:18:50 tmizan Exp $
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
#include "lsf/lib/liblsf.h"




static int errno_map[] = {    0,
                              EPERM,
                              ENOENT,
                              ESRCH,
                              EINTR,
                              EIO,
                              ENXIO,
                              E2BIG,
                              ENOEXEC,
                              EBADF,
                              ECHILD,
                              EAGAIN,
                              ENOMEM,
                              EACCES,
                              EFAULT,
                              ENOTBLK,
                              EBUSY,
                              EEXIST,
                              EXDEV,
                              ENODEV,
                              ENOTDIR,
                              EISDIR,
                              EINVAL,
                              ENFILE,
                              EMFILE,
                              ENOTTY,
                              ETXTBSY,
                              EFBIG,
                              ENOSPC,
                              ESPIPE,
                              EROFS,
                              EMLINK,
                              EPIPE,
                              EDOM,
                              ERANGE,
                              EWOULDBLOCK,
                              EINPROGRESS,
                              EALREADY,
                              ENOTSOCK,
                              EDESTADDRREQ,
                              EMSGSIZE,
                              EPROTOTYPE,
                              ENOPROTOOPT,
                              EPROTONOSUPPORT,
                              ESOCKTNOSUPPORT,
                              EOPNOTSUPP,
                              EPFNOSUPPORT,
                              EAFNOSUPPORT,
                              EADDRINUSE,
                              EADDRNOTAVAIL,
                              ENETDOWN,
                              ENETUNREACH,
                              ENETRESET,
                              ECONNABORTED,
                              ECONNRESET,
                              ENOBUFS,
                              EISCONN,
                              ENOTCONN,
                              ESHUTDOWN,
                              ETOOMANYREFS,
                              ETIMEDOUT,
                              ECONNREFUSED,
                              ELOOP,
                              ENAMETOOLONG,
                              EHOSTDOWN,
                              EHOSTUNREACH,
                              ENOTEMPTY,
                              ESTALE,
                              EREMOTE,
                              EDEADLK,
                              ENOLCK,
                              ENOSYS
};

#define  NERRNO_MAP  (sizeof(errno_map)/sizeof(int))

int
errnoEncode_(int eno)
{
    int i;

    if (eno < 0)
        return eno;

    for (i=0; i<NERRNO_MAP; i++) {
        if (errno_map[i] == eno) {
            return (i);
        }
    }

    if (eno >= NERRNO_MAP)
        return(eno);
    else
        return(0);

}

int
errnoDecode_(int eno)
{
    if (eno < 0)
        return (eno);

    if (eno >= NERRNO_MAP) {
        if (strerror(eno) != NULL)
            return(eno);
        else
            return(0);
    }

    return(errno_map[eno]);
}
