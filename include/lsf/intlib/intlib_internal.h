/*
 *  Copyright (C) LavaLite Contributors
 */

/* src/lsf/intlib/intlib_internal.h
 */
#ifndef LAVALITE_INTLIB_INTERNAL_H
#define LAVALITE_INTLIB_INTERNAL_H

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
#include <malloc.h>
#include <netdb.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <syslog.h>
#include <tcl.h>
#include <time.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/times.h>
#include <sys/types.h>
#include <sys/wait.h>

/* Public header
 */
#include "lsf.h"

/* Private tree headers under include/… that intlib uses a lot.
 * Keep these focused; don’t pull half the world unless you must.
 */
#include "lsf/intlib/bitset.h"
#include "lsf/intlib/intlib.h"
#include "lsf/intlib/intlibout.h"
#include "lsf/intlib/jidx.h"
#include "lsf/intlib/list.h"
#include "lsf/intlib/listset.h"
#include "lsf/intlib/lsftcl.h"
#include "lsf/intlib/resreq.h"
#include "lsf/intlib/tokdefs.h"
#include "lsf/intlib/vector.h"
#include "lsf/intlib/yparse.h"

/* Optional: common macros, inline helpers confined to intlib.
 * static inline helpers here if they truly belong to intlib-wide usage
*/

#endif /* LAVALITE_INTLIB_INTERNAL_H */

