/*
 *  Copyright (C) LavaLite Contributors
 */

#ifndef _SYSCORE_
#define _SYSCORE_

/* Private module umbrella: fine to include config.h here because it is NOT
 * installed.
 */
#include "config.h"

/* System headers needed by multiple intlib .c files
 */
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <inttypes.h>
#include <limits.h>
#include <linux/limits.h>
#include <netdb.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/times.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <rpc/types.h>
#include <rpc/types.h>
#include <rpc/xdr.h>

/* Public header
 */
#include "lsf.h"

/* A maze... include list.h before bitset.h
#include "lsf/intlib/list.h"
#include "lsf/intlib/bitset.h"
#include "lsf/intlib/intlibout.h"
#include "lsf/intlib/jidx.h"
#include "lsf/intlib/listset.h"
#include "lsf/intlib/resreq.h"
#include "lsf/intlib/tokdefs.h"
#include "lsf/intlib/tcl_stub.h"
 */

#endif
