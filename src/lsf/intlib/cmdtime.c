/* $Id: cmdtime.c,v 1.3 2007/08/15 22:18:49 tmizan Exp $
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

#include "lsf/intlib/llsys.h"
#include "lsf/lib/lproto.h"

static int getPtime(char *, char, time_t, char, time_t *);
static time_t mkTime(struct tm *, int, time_t);

static void checkThree(char *, char achar, char **cp1, char **cp2, char **cp3);
static int checkBEtime(time_t Btime, time_t Etime);
static int checkChar(char *string, char **cp);
static int checkYear(int *);
static int checkTime(int, int);
static time_t getToday(char endTime);

int getBEtime(char *toptarg, char flag, time_t tTime[])
{
    char *cp, *cp1, *cp3;
    time_t sbtime;
    int cc;
    char endTime;

    checkThree(toptarg, ',', &cp, &cp1, &cp3);
    if (cp1 != NULL) {
        lserrno = LSE_BAD_TIME;
        return -1;
    }

    if (cp == NULL) {
        checkThree(toptarg, '.', &cp, &cp1, &cp3);
        if (cp != NULL) {
            lserrno = LSE_BAD_TIME;
            return -1;
        }
        endTime = false;
        sbtime = 0;
        if ((cc = getPtime(toptarg, endTime, sbtime, flag, &tTime[0])) == -1)
            return -1;
        endTime = true;
        if (getPtime(toptarg, endTime, sbtime, flag, &tTime[1]) == -1)
            return -1;
        if (checkBEtime(tTime[0], tTime[1]) == -1)
            return -1;
        return 0;
    }

    while (true) {
        if (*toptarg == ',') {
            tTime[0] = 0;
            toptarg++;
            break;
        }
        if ((*toptarg == '.') && (*(toptarg + 1) == ',')) {
            tTime[0] = time(0);
            toptarg = cp;
        }
        if (*toptarg == ',') {
            tTime[0] = time(0);
            toptarg = cp + 1;
            break;
        }
        if ((*toptarg == '.') && (*(toptarg + 1) == '-')) {
            sbtime = time(0);
            toptarg += 2;
            *cp = '\000';
            endTime = false;
            if (getPtime(toptarg, endTime, sbtime, flag, &tTime[0]) == -1)
                return -1;
            toptarg = cp + 1;
            break;
        }

        if ((*toptarg == '@') && (*(toptarg + 1) == ',')) {
            endTime = false;
            tTime[0] = getToday(endTime);
            toptarg = cp + 1;
            break;
        }
        if ((*toptarg == '@') && (*(toptarg + 1) == '-')) {
            endTime = false;
            sbtime = getToday(endTime);
            toptarg += 2;
            *cp = '\000';
            if (getPtime(toptarg, endTime, sbtime, flag, &tTime[0]) == -1)
                return -1;
            toptarg = cp + 1;
            break;
        }

        endTime = false;
        *cp = '\000';
        sbtime = 0;
        if (getPtime(toptarg, endTime, sbtime, flag, &tTime[0]) == -1) {
            *cp = ',';
            return -1;
        }
        toptarg = cp + 1;
        break;
    }

    if ((*toptarg == '.' && *(toptarg + 1) == '\0') || (*toptarg == '\0')) {
        tTime[1] = time(0);
        if (checkBEtime(tTime[0], tTime[1]) == -1) {
            *cp = ',';
            return -1;
        }
        return 0;
    }
    if ((*toptarg == '.') && (*(toptarg + 1) == '-')) {
        sbtime = time(0);
        toptarg += 2;
        endTime = false;
        if (getPtime(toptarg, endTime, sbtime, flag, &tTime[1]) == -1) {
            *cp = ',';
            return -1;
        }
        if (checkBEtime(tTime[0], tTime[1]) == -1) {
            *cp = ',';
            return -1;
        }
        return 0;
    }

    if ((*toptarg == '@') && (*(toptarg + 1) == '\0')) {
        endTime = true;
        tTime[1] = getToday(endTime);
        if (checkBEtime(tTime[0], tTime[1]) == -1) {
            *cp = ',';
            return -1;
        }
        return 0;
    }
    if ((*toptarg == '@') && (*(toptarg + 1) == '-')) {
        endTime = true;
        sbtime = getToday(endTime);
        toptarg += 2;
        if (getPtime(toptarg, endTime, sbtime, flag, &tTime[1]) == -1) {
            *cp = ',';
            return -1;
        }
        if (checkBEtime(tTime[0], tTime[1]) == -1) {
            *cp = ',';
            return -1;
        }
        return 0;
    }

    endTime = true;
    sbtime = false;
    toptarg = cp + 1;
    if (getPtime(toptarg, endTime, sbtime, flag, &tTime[1]) == -1) {
        *cp = ',';
        return -1;
    }
    if (checkBEtime(tTime[0], tTime[1]) == -1) {
        *cp = ',';
        return -1;
    }
    return 0;
}

static int checkBEtime(time_t Btime, time_t Etime)
{
    if (Btime > Etime) {
        lserrno = LSE_BAD_TIME;
        return -1;
    } else
        return 0;
}

static int getPtime(char *toptarg, char endTime, time_t sbtime, char flag,
                    time_t *Ptime)
{
#define MINU 1
#define HOUR 2
#define DAY 4
#define MONTH 8
#define YEAR 16

    char *cp, *cp1, *cp2, *cp3;
    time_t ptimev;
    struct tm *tmPtr;
    int ptimef = 0, m, tempInt;

    if (!checkChar(toptarg, &cp)) {
        lserrno = LSE_BAD_TIME;
        return -1;
    }
    checkThree(toptarg, '/', &cp1, &cp2, &cp3);
    if (cp2 != NULL) {
        if ((cp1 + 1) == cp2) {
            lserrno = LSE_BAD_TIME;
            return -1;
        }
        *cp2 = '0';
        checkThree(toptarg, '/', &cp1, &cp, &cp3);
        *cp2 = '/';
        if (cp3 != NULL) {
            lserrno = LSE_BAD_TIME;
            return -1;
        }
    }
    checkThree(toptarg, ':', &cp1, &cp2, &cp3);
    if (cp1 == NULL) {
        checkThree(toptarg, '/', &cp1, &cp2, &cp3);
        if ((cp3 != NULL) && (*(cp3 + 1) != '0')) {
            lserrno = LSE_BAD_TIME;
            return -1;
        }
    }
    checkThree(toptarg, ':', &cp1, &cp2, &cp3);
    if (cp2 != NULL) {
        lserrno = LSE_BAD_TIME;
        return -1;
    }
    if ((cp1 != NULL) && ((cp1[-1] == '/') || (cp1[1] == '/'))) {
        lserrno = LSE_BAD_TIME;
        return -1;
    }

    if (flag == 'w') {
        tmPtr = malloc(sizeof(struct tm));
        tmPtr->tm_year = 70;
        tmPtr->tm_mon = 0;
        tmPtr->tm_mday = 1;
        tmPtr->tm_hour = 0;
        tmPtr->tm_min = 0;
    } else {
        ptimev = time(0);
        tmPtr = localtime(&ptimev);
    }
    tmPtr->tm_sec = 0;
    tmPtr->tm_isdst = -1;

    cp = cp1;

    checkThree(toptarg, '/', &cp1, &cp2, &cp3);

    if ((cp == NULL) && (cp1 != NULL) && (cp2 == NULL) && (cp1 > toptarg)) {
        *cp1 = '\000';
        tempInt = atoi(toptarg);
        if (tempInt > 12) {
            ptimef |= YEAR;
            tmPtr->tm_year = tempInt;
            if (checkYear(&tmPtr->tm_year) == -1) {
                *cp1 = '/';
                return -1;
            }
            if ((*(cp1 + 1) != '\000')) {
                ptimef |= MONTH;
                tmPtr->tm_mon = atoi(cp1 + 1);
                if (checkTime(MONTH, tmPtr->tm_mon) == -1) {
                    *cp1 = '/';
                    return -1;
                }
            } else
                tmPtr->tm_mon = (endTime ? 12 : 1);
            if (endTime) {
                m = tmPtr->tm_mon;
                if ((m == 1) || (m == 3) || (m == 5) || (m == 7) || (m == 8) ||
                    (m == 10) || (m == 12))
                    tmPtr->tm_mday = 31;
                else if (m != 2)
                    tmPtr->tm_mday = 30;
                else if (tmPtr->tm_year % 4 == 0)
                    tmPtr->tm_mday = 29;
                else
                    tmPtr->tm_mday = 28;
            } else
                tmPtr->tm_mday = 1;
        } else {
            ptimef |= MONTH;
            tmPtr->tm_mon = tempInt;
            if (checkTime(MONTH, tmPtr->tm_mon) == -1) {
                *cp1 = '/';
                return -1;
            }
            if (*(cp1 + 1) != '\000') {
                ptimef |= DAY;
                tmPtr->tm_mday = atoi(cp1 + 1);
                if (checkTime(DAY, tmPtr->tm_mday) == -1) {
                    *cp1 = '/';
                    return -1;
                }
            } else if (endTime) {
                m = tmPtr->tm_mon;
                if ((m == 1) || (m == 3) || (m == 5) || (m == 7) || (m == 8) ||
                    (m == 10) || (m == 12))
                    tmPtr->tm_mday = 31;
                else if (m != 2)
                    tmPtr->tm_mday = 30;
                else if (tmPtr->tm_year % 4 == 0)
                    tmPtr->tm_mday = 29;
                else
                    tmPtr->tm_mday = 28;
            } else
                tmPtr->tm_mday = 1;
        }

        *cp1 = '/';
        tmPtr->tm_mon--;
        tmPtr->tm_hour = (endTime ? 23 : 0);
        tmPtr->tm_min = (endTime ? 59 : 0);
        tmPtr->tm_sec = (endTime ? 59 : 0);
        if (flag == 'w') {
            if ((ptimef & YEAR) == YEAR) {
                lserrno = LSE_BAD_TIME;
                return -1;
            }
            if ((ptimef & MONTH) == MONTH)
                tmPtr->tm_year--;
            if ((ptimef & DAY) == DAY)
                tmPtr->tm_year -= 2;
        }
        *Ptime = mkTime(tmPtr, ptimef, sbtime);
        if (*Ptime < 0)
            return -1;
        return 0;
    }

    if ((cp == NULL) && (cp1 == toptarg) && (cp2 != NULL)) {
        *cp2 = '\000';
        cp2 = NULL;
    }
    if ((cp == NULL) && (cp1 == toptarg) && (cp2 == NULL)) {
        ptimef |= DAY;
        toptarg += 1;
        cp1 = NULL;
    }
    if ((cp == NULL) && (cp1 == NULL)) {
        ptimef |= DAY;
        tmPtr->tm_mday = atoi(toptarg);
        if (checkTime(DAY, tmPtr->tm_mday) == -1)
            return -1;
        tmPtr->tm_hour = (endTime ? 23 : 0);
        tmPtr->tm_min = (endTime ? 59 : 0);
        tmPtr->tm_sec = (endTime ? 59 : 0);
        if (flag == 'w') {
            if ((ptimef & YEAR) == YEAR) {
                lserrno = LSE_BAD_TIME;
                return -1;
            }
            if ((ptimef & MONTH) == MONTH)
                tmPtr->tm_year--;
            if ((ptimef & DAY) == DAY)
                tmPtr->tm_year -= 2;
        }
        *Ptime = mkTime(tmPtr, ptimef, sbtime);
        if (*Ptime < 0)
            return -1;
        return 0;
    }

    if ((cp == NULL) && (cp2 != NULL) && (cp1 != toptarg)) {
        if ((cp3 != NULL) || (*(cp2 + 1) != '\000')) {
            ptimef |= YEAR;
            ptimef |= MONTH;
            ptimef |= DAY;
            *cp1 = '\000';
            tmPtr->tm_year = atoi(toptarg);
            if (checkYear(&tmPtr->tm_year) == -1) {
                *cp1 = '/';
                return -1;
            }
            *cp1 = '/';
            *cp2 = '\000';
            tmPtr->tm_mon = atoi(cp1 + 1);
            *cp2 = '/';
            if (cp3 != NULL) {
                *cp3 = '\000';
                tmPtr->tm_mday = atoi(cp2 + 1);
                *cp3 = '/';
            } else
                tmPtr->tm_mday = atoi(cp2 + 1);
            if (checkTime(DAY, tmPtr->tm_mday) == -1)
                return -1;
        } else {
            *cp1 = '\000';
            tempInt = atoi(toptarg);
            if (tempInt > 12) {
                ptimef |= YEAR;
                tmPtr->tm_year = tempInt;
                if (checkYear(&tmPtr->tm_year) == -1) {
                    *cp1 = '/';
                    return -1;
                }
                ptimef |= MONTH;
                *cp2 = '\000';
                tmPtr->tm_mon = atoi(cp1 + 1);
                if (endTime) {
                    m = tmPtr->tm_mon;
                    if ((m == 1) || (m == 3) || (m == 5) || (m == 7) ||
                        (m == 8) || (m == 10) || (m == 12))
                        tmPtr->tm_mday = 31;
                    else if (m != 2)
                        tmPtr->tm_mday = 30;
                    else if (tmPtr->tm_year % 4 == 0)
                        tmPtr->tm_mday = 29;
                    else
                        tmPtr->tm_mday = 28;
                } else
                    tmPtr->tm_mday = 1;
            } else {
                ptimef |= MONTH;
                tmPtr->tm_mon = tempInt;
                ptimef |= DAY;
                *cp2 = '\000';
                tmPtr->tm_mday = atoi(cp1 + 1);
            }
            if (checkTime(DAY, tmPtr->tm_mday) == -1) {
                *cp1 = '/';
                *cp2 = '/';
                return -1;
            }
        }
        if (checkTime(MONTH, tmPtr->tm_mon) == -1)
            return -1;
        tmPtr->tm_mon--;
        *cp1 = '/';
        *cp2 = '/';
        tmPtr->tm_hour = (endTime ? 23 : 0);
        tmPtr->tm_min = (endTime ? 59 : 0);
        tmPtr->tm_sec = (endTime ? 59 : 0);
        if (flag == 'w') {
            if ((ptimef & YEAR) == YEAR) {
                lserrno = LSE_BAD_TIME;
                return -1;
            }
            if ((ptimef & MONTH) == MONTH)
                tmPtr->tm_year--;
            if ((ptimef & DAY) == DAY)
                tmPtr->tm_year -= 2;
        }
        *Ptime = mkTime(tmPtr, ptimef, sbtime);
        if (*Ptime < 0)
            return -1;
        return 0;
    }

    if ((cp != NULL) && (cp2 != NULL) && (cp3 != NULL)) {
        if (cp3 != NULL) {
            ptimef |= YEAR;
            ptimef |= MONTH;
            ptimef |= DAY;
            *cp1 = '\000';
            tmPtr->tm_year = atoi(toptarg);
            *cp1 = '/';
            if (checkYear(&tmPtr->tm_year) == -1)
                return -1;
            *cp2 = '\000';
            tmPtr->tm_mon = atoi(cp1 + 1);
            *cp2 = '/';
            if (checkTime(MONTH, tmPtr->tm_mon) == -1)
                return -1;
            tmPtr->tm_mon--;
            *cp3 = '\000';
            tmPtr->tm_mday = atoi(cp2 + 1);
            *cp3 = '/';
            if (checkTime(DAY, tmPtr->tm_mday) == -1)
                return -1;
            *cp = '\000';
            tmPtr->tm_hour = atoi(cp3 + 1);
            *cp = ':';
            if (checkTime(HOUR, tmPtr->tm_hour) == -1)
                return -1;
            if (*(cp + 1) != '\000') {
                tmPtr->tm_min = atoi(cp + 1);
                if (checkTime(MINU, tmPtr->tm_min) == -1)
                    return -1;
            } else {
                tmPtr->tm_min = (endTime ? 59 : 0);
                tmPtr->tm_sec = (endTime ? 59 : 0);
            }
            if (flag == 'w') {
                if ((ptimef & YEAR) == YEAR) {
                    lserrno = LSE_BAD_TIME;
                    return -1;
                }
                if ((ptimef & MONTH) == MONTH)
                    tmPtr->tm_year--;
                if ((ptimef & DAY) == DAY)
                    tmPtr->tm_year -= 2;
            }
            *Ptime = mkTime(tmPtr, ptimef, sbtime);
            if (*Ptime < 0)
                return -1;
            return 0;
        }
    }

    if ((cp != NULL) && (cp2 != NULL) && (cp3 == NULL)) {
        if (cp1 > toptarg) {
            ptimef |= MONTH;
            *cp1 = '\000';
            tmPtr->tm_mon = atoi(toptarg);
            *cp1 = '/';
            if (checkTime(MONTH, tmPtr->tm_mon) == -1)
                return -1;
            tmPtr->tm_mon--;
            toptarg = cp1;
        }
        if (toptarg[0] == '/') {
            toptarg += 1;
        }
        cp1 = cp2;
        cp2 = NULL;
    }
    if ((cp != NULL) && (cp1 != NULL) && (cp2 == NULL)) {
        if ((cp1 > toptarg) && (cp1 < cp)) {
            ptimef |= DAY;
            *cp1 = '\000';
            tmPtr->tm_mday = atoi(toptarg);
            *cp1 = '/';
            if (checkTime(DAY, tmPtr->tm_mday) == -1)
                return -1;
            toptarg = cp1 + 1;
        } else if (cp1 > cp) {
            lserrno = LSE_BAD_TIME;
            return -1;
        }
    }

    checkThree(toptarg, '/', &cp1, &cp2, &cp3);
    if ((cp != NULL) && (cp1 == toptarg) && (cp2 == NULL)) {
        toptarg += 1;
    }

    if (toptarg[0] == ':') {
        ptimef |= MINU;
        tmPtr->tm_min = atoi(cp + 1);
        if (checkTime(MINU, tmPtr->tm_min) == -1)
            return -1;

        if (flag == 'w') {
            if ((ptimef & YEAR) == YEAR) {
                lserrno = LSE_BAD_TIME;
                return -1;
            }
            if ((ptimef & MONTH) == MONTH)
                tmPtr->tm_year--;
            if ((ptimef & DAY) == DAY)
                tmPtr->tm_year -= 2;
        }
        *Ptime = mkTime(tmPtr, ptimef, sbtime);
        if (*Ptime < 0)
            return -1;
        return 0;
    }
    ptimef |= HOUR;
    if (*(cp + 1) != '\000') {
        ptimef |= MINU;
        tmPtr->tm_min = atoi(cp + 1);
        if (checkTime(MINU, tmPtr->tm_min) == -1)
            return -1;
    } else
        tmPtr->tm_min = (endTime ? 59 : 0);
    *cp = '\000';
    tmPtr->tm_hour = atoi(toptarg);
    if (checkTime(HOUR, tmPtr->tm_hour) == -1) {
        *cp = ':';
        return -1;
    }
    *cp = ':';
    if (flag == 'w') {
        if ((ptimef & YEAR) == YEAR) {
            lserrno = LSE_BAD_TIME;
            return -1;
        }
        if ((ptimef & MONTH) == MONTH)
            tmPtr->tm_year--;
        if ((ptimef & DAY) == DAY)
            tmPtr->tm_year -= 2;
    }
    *Ptime = mkTime(tmPtr, ptimef, sbtime);
    if (*Ptime < 0)
        return -1;
    return 0;
}

static void checkThree(char *strng, char achar, char **cp1, char **cp2,
                       char **cp3)
{
    char *cp0;

    *cp2 = NULL;
    *cp3 = NULL;
    *cp1 = (char *) strchr(strng, achar);
    cp0 = *cp1;
    if (cp0 != NULL) {
        cp0++;
        *cp2 = (char *) strchr(cp0, achar);
        cp0 = *cp2;
        if (cp0 != NULL) {
            cp0++;
            *cp3 = (char *) strchr(cp0, achar);
        }
    }
    return;
}

static int checkChar(char *string, char **cp)
{
    int i;
    *cp = NULL;
    for (i = 0; i < strlen(string); i++) {
        if ((string[i] < '0') || (string[i] > '9')) {
            if ((string[i] == '/') || (string[i] == ':'))
                *cp = &string[i];
            else {
                *cp = &string[i];
                return false;
            }
        }
    }
    return true;
}

static time_t mkTime(struct tm *tmPtr, int itimef, time_t sbtime)
{
    struct tm itm;
    time_t timeVal;
    int monthSaver;
    int mdaySaver;

    if (sbtime != 0) {
        itm.tm_mon = tmPtr->tm_mon;
        itm.tm_mday = tmPtr->tm_mday;
        itm.tm_hour = tmPtr->tm_hour;
        itm.tm_min = tmPtr->tm_min;
        itm.tm_sec = tmPtr->tm_sec;
        tmPtr = localtime(&sbtime);
        if ((itimef & MONTH) == MONTH)
            tmPtr->tm_mon -= itm.tm_mon + 1;
        if ((itimef & DAY) == DAY)
            tmPtr->tm_mday -= itm.tm_mday;
        if ((itimef & HOUR) == HOUR)
            tmPtr->tm_hour -= itm.tm_hour;
        if ((itimef & MINU) == MINU)
            tmPtr->tm_min -= itm.tm_min;
    }
    monthSaver = tmPtr->tm_mon;
    mdaySaver = tmPtr->tm_mday;
    timeVal = mktime(tmPtr);
    if (timeVal < 0 ||
        (tmPtr->tm_mon == monthSaver + 1 && tmPtr->tm_mday != mdaySaver)) {
        lserrno = LSE_BAD_TIME;
        return -1;
    }
    return timeVal;
}

static int checkYear(int *year)
{
    if (*year > 1900)
        *year -= 1900;
    if ((*year < 93) || (*year > 138)) {
        lserrno = LSE_BAD_TIME;
        return -1;
    }
    return 0;
}

static int checkTime(int type, int time)
{
    switch (type) {
    case MONTH:
        if ((time >= 1) && (time <= 12))
            return 0;
        break;
    case DAY:
        if ((time >= 1) && (time <= 31))
            return 0;
        break;
    case HOUR:
        if ((time >= 0) && (time <= 23))
            return 0;
        break;
    case MINU:
        if ((time >= 0) && (time <= 59))
            return 0;
        break;
    }
    lserrno = LSE_BAD_TIME;
    return -1;
}

static time_t getToday(char endTime)
{
    time_t now;
    struct tm *tmPtr;

    now = time(NULL);
    tmPtr = localtime(&now);

    if (endTime) {
        tmPtr->tm_hour = 23;
        tmPtr->tm_min = 59;
        tmPtr->tm_sec = 59;
    } else {
        tmPtr->tm_hour = 0;
        tmPtr->tm_min = 0;
        tmPtr->tm_sec = 0;
    }

    return timelocal(tmPtr);
}
