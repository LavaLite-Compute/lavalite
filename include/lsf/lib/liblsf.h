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

#ifndef _LSFLIB_H_
#define _LSFLIB_H_

/* Private module umbrella: fine to include config.h here because it is NOT
 * installed.
 */
#include "config.h"

/* System headers
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
#include <sys/stat.h>
#include <sys/socket.h>
#include <rpc/types.h>
#include <rpcsvc/ypclnt.h>
#include <sys/wait.h>
#include <arpa/inet.h>

/* Public API headers
*/
#include "lsf.h"

/* Internal headers
*/
#include "lsf/lib/lib.channel.h"
#include "lsf/lib/lib.conf.h"
#include "lsf/lib/lib.h"
#include "lsf/lib/lib.hdr.h"
#include "lsf/lib/lib.pim.h"
#include "lsf/lib/lib.queue.h"
#include "lsf/lib/lib.rf.h"
#include "lsf/lib/lib.rmi.h"
#include "lsf/lib/lib.so.h"
#include "lsf/lib/lib.table.h"
#include "lsf/lib/lib.words.h"
#include "lsf/lib/lib.xdr.h"
#include "lsf/lib/lib.xdrlim.h"
#include "lsf/lib/lib.xdrres.h"
#include "lsf/lib/lproto.h"
#include "lsf/lib/lsi18n.h"
#include "lsf/lib/mls.h"

/* Some defines from the lava config.h which was manually
 * grenerated file, not the automake one.
 */
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
#define SOCK_READ_FIX  b_read_fix
#define SOCK_WRITE_FIX b_write_fix
#define NB_SOCK_READ_FIX   nb_read_fix
#define NB_SOCK_WRITE_FIX  nb_write_fix
#define LSF_NSIG NSIG

#endif /* _LSFLIB_H_ */
