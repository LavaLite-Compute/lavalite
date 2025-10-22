/*
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

#ifndef _LIB_COMMON__
#define _LIB_COMMON_

/* This file is created by automake
 */
#include "config.h"

/* System headers
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <syslog.h>
#include <pwd.h>
#include <grp.h>
#include <ctype.h>
#include <limits.h>
#include <netdb.h>
#include <termios.h>
#include <signal.h>
#include <stdarg.h>
#include <dirent.h>
#include <stdbool.h>
#include <math.h>
#include <float.h>
#include <poll.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <rpc/types.h>
#include <rpc/xdr.h>
#include <rpcsvc/ypclnt.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <sys/statvfs.h>
#include <arpa/inet.h>

/* System XDR versionis the protocol version carried by the header
 */
#define _XDR_VERSION_0_1_0 1
/* Global message size is stdio BUFSIZ 8192 bytes
 */
#define MSGSIZE BUFSIZ

/* Public API headers
*/
#include "lsf.h"

/* Bug. We will remove this later
 */
#include "lsf/lib/lsi18n.h"
#include "lsf/lib/mls.h"

/* Some defines from the lava config.h which was manually
 * generated file, not the automake one.
 */
#define NICE_LEAST -40
#define NICE_MIDDLE 20
#define DEF_REXPRIORITY 0
#define LSTMPDIR lsTmpDir_
#define LSDEVNULL "/dev/null"
#define LSETCDIR "/etc"
#define LIM_PORT 36000
#define RES_PORT 36002
#define closesocket close
#define CLOSESOCKET(s) close((s))
#define SOCK_CALL_FAIL(c) ((c) < 0 )
#define SOCK_INVALID(c) ((c) < 0 )
#define CLOSEHANDLE close
/* Bug. Garbage defines....
 */
#define SOCK_READ_FIX  b_read_fix
#define SOCK_WRITE_FIX b_write_fix
#define NB_SOCK_READ_FIX   nb_read_fix
#define NB_SOCK_WRITE_FIX  nb_write_fix
#define LSF_NSIG NSIG

extern void Signal_(int, void (*f)(int));
extern char *get_username(void);

#endif /* _LIB_H_ */
