/* $Id: wait.c,v 1.3 2007/08/15 22:18:49 tmizan Exp $
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

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/times.h>
#include <unistd.h>
#include "lsf/intlib/libllcore.h"

#ifdef BROKEN_WAIT3

int
wait3TmpFix_(LS_WAIT_T *status, int options, struct rusage *rusage)
{
    int pid;
    struct tms oldtms, newtms;
    float s;

    if (rusage)
	(void) times(&oldtms);

    if ((pid = waitpid((pid_t) -1, status, options)) <= 0)
	return pid;

    if (rusage == NULL)
	return pid;

    (void) times(&newtms);
    s = (float) (newtms.tms_cutime - oldtms.tms_cutime) / (float) CLK_TCK;
    rusage->ru_utime.tv_sec = s;
    rusage->ru_utime.tv_usec = (s - (float) rusage->ru_utime.tv_sec) *
	1000000.0;

    s = (float) (newtms.tms_cstime - oldtms.tms_cstime) / (float) CLK_TCK;
    rusage->ru_stime.tv_sec = s;
    rusage->ru_stime.tv_usec = (s - (float) rusage->ru_stime.tv_sec) *
	1000000.0;

    return pid;
}

#endif
