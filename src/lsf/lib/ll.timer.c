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
#include "lsf/lib/lib.h"

/*
 * millisleep_() — sleep for `ms` milliseconds using clock_nanosleep()
 * Returns 0 on success, or -1 with errno set on error/interruption.
 */
int millisleep_(uint32_t ms)
{
    struct timespec ts;

    ts.tv_sec  = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000L;

    /* CLOCK_MONOTONIC is immune to system time changes */
    int ret;
    do {
        ret = clock_nanosleep(CLOCK_MONOTONIC, 0, &ts, &ts);
    } while (ret == EINTR);  /* restart if interrupted by a signal */

    return ret;
}
