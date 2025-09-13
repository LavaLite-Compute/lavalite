/* $Id: mbd.window.c,v 1.4 2007/08/15 22:18:46 tmizan Exp $
 * Copyright (C) 2007 Platform Computing Inc
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
#include <stdlib.h> 
#include <time.h>
#include "daemons.h"
#include "mbd.h"

#define NL_SETN		10	

struct timeWindow *newTimeWindow (void); 
void freeTimeWindow(struct timeWindow *);
void updateTimeWindow(struct timeWindow *);



struct timeWindow * 
newTimeWindow(void)
{   
    struct timeWindow *timeW = NULL;
    int i;

    
    timeW = (struct timeWindow *)my_malloc(sizeof(struct timeWindow), 
                                            "newTimeWindow");
    timeW->status = WINDOW_OPEN;
    timeW->windows = NULL;
    for (i = 0; i < 8; i++) {
	timeW->week[i] = NULL;
    }
    timeW->windEdge = 0;

    return(timeW);
} 

void
freeTimeWindow(struct timeWindow *timeW)
{
    int i;

    if (!timeW) {
        return;
    }

    
    FREEUP(timeW->windows);
    for (i = 0; i < 8; i++) {
	delWindow(timeW->week[i]);
    }
    FREEUP(timeW);

} 

void
updateTimeWindow (struct timeWindow *timeW)
{

    struct dayhour dayhour;
    windows_t *wp;
    char windOpen;


    
    if (timeW->windEdge > now || timeW->windEdge == 0) {
	return;
    }

    getDayHour (&dayhour, now);

    if (timeW->week[dayhour.day] == NULL) {               
        timeW->windEdge = now + (24.0 - dayhour.hour) * 3600.0;
	return;
    }

    
    windOpen = FALSE;
    timeW->status = WINDOW_CLOSE;
    timeW->windEdge = now + (24.0 - dayhour.hour) * 3600.0;

    for (wp = timeW->week[dayhour.day]; wp; wp=wp->nextwind) {
        checkWindow(&dayhour, &windOpen, &timeW->windEdge, wp, now);
        if (windOpen) {        
            timeW->status = WINDOW_OPEN;
	    break;
	}
    }

    return;
}  
