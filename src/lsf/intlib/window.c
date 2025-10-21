/* $Id: window.c,v 1.3 2007/08/15 22:18:49 tmizan Exp $
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

#include "lsf/intlib/common.h"
#include "lsf/intlib/intlibout.h"

static int parse_time (char *word, float *hour, int *day);
static windows_t * new_wind (void);
static int mergeW (windows_t *wp, float ohour, float chour);

int
addWindow (
    char *wordpair,
    windows_t *week[],
    char *context
)
{
    int  oday, cday;
    float ohour, chour;
    float oHourTd, cHourTd;
    float oHourNd, cHourNd;
    char *sp;
    char *word;
    int i, j;

    sp = strchr(wordpair, '-');
    if (!sp) {
        ls_syslog(LOG_ERR,
		  "Bad time expression in %s",
		  context);
        return -1;
    }

    *sp = '\0';
    sp++;
    word = sp;

    if (parse_time(word, &chour, &cday) < 0) {
        ls_syslog(LOG_ERR,
		  "Bad time expression in %s",
		  context);
        return -1;
    }

    word = wordpair;

    if (parse_time(word, &ohour, &oday) < 0) {
        ls_syslog(LOG_ERR,
		  "Bad time expression in %s",
		  context);
        return -1;
    }

    if (((oday && cday) == 0) && (oday != cday)) {
        ls_syslog(LOG_ERR,
		  "Ambiguous time in %s",
		  context);
        return -1;
    }

    if (oday == 0 && cday == 0 && ohour == chour) {

        ohour = chour = 25.0;
    }

    if (oday == 0) {
        if (ohour > chour) {

            oHourTd = ohour;
            cHourTd = 25.0;
            oHourNd = -1.0;
            cHourNd = chour;

            for (i=1;i<8;i++)
            {

                insertW(&week[i], oHourTd, cHourTd);

                 insertW(&week[i], oHourNd, cHourNd);
            }
        }
        else {
            for (i=1;i<8;i++)
                insertW(&week[i], ohour, chour);
        }
    } else if (oday == cday) {

        if (ohour > chour) {
            ls_syslog(LOG_ERR,
                      "Ambiguous time in %s",
                      context);
            return -1;
        }
        else if (ohour==chour) {
            ohour = 25.0;
            chour = 25.0;
        }
        insertW(&week[oday], ohour, chour);
        for (j=1; j<8; j++)
            if ((week[j] == NULL) && (j != oday))
                insertW(&week[j], 25.0, 25.0);
    } else {
        for (i=oday;;i++) {
            if (i==8)
                i = 1;
            if (i == oday) {
                insertW(&week[i], ohour, 25.0);
                continue;
            }

            if (i == cday) {
                insertW(&week[i], -1.0, chour);
                for (j=1; j<8; j++)
                    if (week[j] == NULL)
                        insertW(&week[j], 25.0, 25.0);
                break;
            }

            insertW(&week[i], -1.0, 25.0);
        }
    }
    return 0;

}

void
insertW (windows_t **window, float ohour, float chour)
{
    windows_t *wp, *nextw;
    int merged;

    if (! *window) {
        if ((*window = new_wind()) == NULL)
	    return;
        wp = *window;
    } else {
	merged = false;

	for (wp = *window; ; wp = wp->nextwind) {
            if (mergeW(wp, ohour, chour) == true) {
                merged = true;
	    }
	    if (wp->nextwind == NULL) {
		break;
	    }
        }
	if (merged) {

	    wp = (*window)->nextwind;
	    (*window)->nextwind = NULL;
	    for (; wp != NULL; wp = nextw) {
		insertW(window, wp->opentime, wp->closetime);
		nextw = wp->nextwind;
		free(wp);
	    }
	    return;
	} else {

	    wp->nextwind = new_wind();
	    wp = wp->nextwind;
	}
    }

    if (ohour < chour) {
	    wp->opentime = ohour;
        wp->closetime = chour;
    }
    return;

}

static int
mergeW (windows_t *wp, float ohour, float chour)
{

    if ((wp->opentime == -1.0) && (wp->closetime == 25.0))
        return true;

    if ((ohour == -1.0) && (chour == 25.0)) {
        wp->opentime = -1.0;
        wp->closetime = 25.0;
        return true;
    }

    if ((ohour == 25.0) && (chour == 25.0)) {
        return true;
    }

    if (ohour >= chour) {

        return true;
    }

    if ((wp->opentime == 25.0) && (wp->closetime == 25.0)) {
        wp->opentime = ohour;
        wp->closetime = chour;
        return true;
    }

    ohour = (ohour <= 0.0) ? -1.0 : ohour;
    chour = (chour >= 24.0) ? 25.0 : chour;

    if (!(wp->opentime > chour) && !(ohour > wp->closetime)) {
        wp->opentime = MIN(ohour, wp->opentime);
        wp->closetime = MAX(chour, wp->closetime);
        return true;
    }

    return false;
}

void
checkWindow (struct dayhour *dayhour, char *active, time_t *wind_edge, windows_t *wp, time_t now)
{
    static char fname[] = "checkWindow";
    time_t  tmp_edge;
    float tmp_time;

    if (logclass & (LC_TRACE | LC_SCHED))
        ls_syslog(LOG_DEBUG, "%s: Entering this routine...", fname);

    if (dayhour->hour >= wp->opentime && dayhour->hour < wp->closetime) {
        *active = true;
        tmp_edge = now + (wp->closetime - dayhour->hour) * 3600;
        if (tmp_edge < *wind_edge)
            *wind_edge = tmp_edge;
    } else {

        tmp_time = MIN(wp->opentime, 24.0);
        tmp_edge = now + (tmp_time - dayhour->hour) * 3600;
        if (tmp_edge < *wind_edge)
            *wind_edge = tmp_edge;
    }
}

static windows_t *
new_wind (void)
{
   windows_t *wp;

   wp = (windows_t *) malloc(sizeof (struct windows));
   if (!wp) {
       ls_syslog(LOG_ERR, I18N_FUNC_FAIL_M, "new_wind",  "malloc");
       return NULL;
   }
   wp->nextwind = NULL;
   wp->opentime = 25.0;
   wp->closetime = 25.0;

   return wp;
}

void
delWindow (windows_t *wp)
{
   windows_t *nextWp;

   if (wp==NULL) {
       return;
   }

   while (wp) {
       nextWp = wp->nextwind;
       FREEUP(wp);
       wp = nextWp;
   }

   return;

}

static int
parse_time (char *word, float *hour, int *day)
{
    float min;
    char *sp;

    *day = 0;
    *hour = 0.0;
    min  = 0.0;

    sp = strrchr(word, ':');
    if (!sp) {
	if (!isint_(word) || atoi (word) < 0)
	    return -1;
        *hour = atof(word);
	if (*hour > 23)
	    return -1;

    }
    else {
        *sp = '\0';
        sp++;

	if (!isint_(sp) || atoi (sp) < 0)
	    return -1;

        min = atoi(sp);
	if (min > 59)
	    return -1;

        sp = strrchr(word, ':');
        if (!sp) {
	    if (!isint_(word) || atoi (word) < 0)
		return -1;
	    *hour = atof(word);
	    if (*hour > 23)
		return -1;
	}
        else {
           *sp = '\0';
           sp++;
	   if (!isint_(sp) || atoi (sp) < 0)
	       return -1;

           *hour = atof(sp);
	   if (*hour > 23)
	       return -1;

	   if (!isint_(word) || atoi (word) < 0)
	       return -1;

           *day  = atoi(word);
           if (*day == 0)
               *day = 7;
	   if (*day < 1 || *day > 7)
	       return -1;
        }
    }

    *hour = *hour + min/60.0;

    return 0;

}

void
getDayHour (struct dayhour *dayPtr, time_t nowtime)
{
    char *timep;

    timep = (char *) ctime(&nowtime);
    timep[3] = '\0';

    if (strcmp(timep, "Sun") == 0)
        dayPtr->day = 7;
    else if (strcmp(timep, "Mon") == 0)
        dayPtr->day = 1;
    else if (strcmp(timep, "Tue") == 0)
        dayPtr->day = 2;
    else if (strcmp(timep, "Wed") == 0)
        dayPtr->day = 3;
    else if (strcmp(timep, "Thu") == 0)
        dayPtr->day = 4;
    else if (strcmp(timep, "Fri") == 0)
        dayPtr->day = 5;
    else if (strcmp(timep, "Sat") == 0)
        dayPtr->day = 6;

    timep += 11;

    timep[2] = '\0';
    dayPtr->hour = atof(timep);

    timep += 3;
    timep[2] = '\0';
    dayPtr->hour += atof(timep) /60.0;

}
