/* $Id: lib.utmp.c,v 1.4 2007/08/15 22:18:51 tmizan Exp $
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
#include "lsf/lib/liblavalite.h"

#define UTMP_FILENAME "/var/adm/utmpx"

int createUtmpEntry(char *uname, pid_t job_pid, char *current_tty)
{
    int err = 0;

    return err;
}

int removeUtmpEntry(pid_t job_pid)
{
    return 0;
}
