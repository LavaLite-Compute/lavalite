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

/* System headers needed by multiple intlib .c files (keep this minimal).
 */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <ctype.h>

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

