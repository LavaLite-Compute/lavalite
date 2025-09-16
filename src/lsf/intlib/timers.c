/* $Id: timers.c,v 1.3 2007/08/15 22:18:49 tmizan Exp $
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

#include "lsf/intlib/libllcore.h"

#define MAX_TVSEC	INT_MAX

static struct timeval nextTime =
{
    MAX_TVSEC, 999999
};


void
setTimer(struct timeval *timerp, unsigned msec)
{
    gettimeofday(timerp, 0);

    timerp->tv_usec += 1000 * (msec % 1000);

    timerp->tv_sec += (msec / 1000) + (timerp->tv_usec / 1000000);

    timerp->tv_usec %= 1000000;


    if ( (nextTime.tv_sec == MAX_TVSEC)
        || (timerp->tv_sec < nextTime.tv_sec)
	|| ((timerp->tv_sec == nextTime.tv_sec)
	    && (timerp->tv_usec < nextTime.tv_usec)))
    {

	nextTime = *timerp;
    }
}

int
timerIsExpired(struct timeval *timerp)
{
    struct timeval tv;

    gettimeofday(&tv, 0);

    if ((timerp->tv_sec < tv.tv_sec)
	|| ((timerp->tv_sec == tv.tv_sec) && (timerp->tv_usec <= tv.tv_usec)))
    {

	return 1;
    }


    if ((timerp->tv_sec < nextTime.tv_sec)
	|| ((timerp->tv_sec == nextTime.tv_sec)
	    && (timerp->tv_usec >= nextTime.tv_usec)))
    {

	nextTime = *timerp;
    }

    return 0;
}

void
timeUntilEvent(struct timeval *timerp)
{
    gettimeofday(timerp, 0);

    timerp->tv_usec = nextTime.tv_usec - timerp->tv_usec;
    if (timerp->tv_usec < 0)
    {
	nextTime.tv_sec--;
	timerp->tv_usec += 1000000;
    }


    if (nextTime.tv_sec < timerp->tv_sec)
    {
	timerp->tv_sec = 0;
	timerp->tv_usec = 0;
    }
    else
	timerp->tv_sec = nextTime.tv_sec - timerp->tv_sec;


    nextTime.tv_sec = MAX_TVSEC;
}
