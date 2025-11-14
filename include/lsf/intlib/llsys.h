#pragma once
/*
 *  Copyright (C) LavaLite Contributors
 */

// Automake generated
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
#include <stdint.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>
#include <termios.h>
#include <dirent.h>
#include <netinet/in.h>
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

// Buf we are rewriting the job arrays but we need the
// data structure defintions for now
#include "lsf/intlib/jidx.h"
#include "lsf/intlib/tcl_stub.h"
// Sneak this one for now
void daemonize_(void);
