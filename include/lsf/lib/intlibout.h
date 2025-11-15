/* $Id: intlibout.h,v 1.8 2007/08/15 22:18:49 tmizan Exp $
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
#pragma once

#include "lsf/lib/ll_bufsize.h"
#include "lsf/lib/resreq.h"
#include "lsf/lib/list.h"
#include "lsf/lib/bitset.h"
#include "lsf/lib/listset.h"

#define MINPASSWDLEN_LS (3)

#define EXIT_NO_ERROR (0)
#define EXIT_FATAL_ERROR (-1)
#define EXIT_WARNING_ERROR (-2)
#define EXIT_RUN_ERROR (-8)

struct windows {
    struct windows *nextwind;
    float opentime;
    float closetime;
};

typedef struct windows windows_t;

struct dayhour {
    short day;
    float hour;
};

struct listEntry {
    struct listEntry *forw;
    struct listEntry *back;
    int entryData;
};

extern int addWindow(char *wordpair, windows_t *week[], char *context);
extern void insertW(windows_t **window, float ohour, float chour);
extern void checkWindow(struct dayhour *dayhour, char *active,
                        time_t *wind_edge, windows_t *wp, time_t now);
extern void getDayHour(struct dayhour *dayPtr, time_t nowtime);
extern void delWindow(windows_t *wp);

extern int userok(int, struct sockaddr_in *, char *, struct sockaddr_in *,
                  struct lsfAuth *, int);
extern int parseResReq(char *, struct resVal *, struct lsInfo *, int);
extern void initParse(struct lsInfo *);
extern int getResEntry(char *);
extern void freeResVal(struct resVal *resVal);
extern void initResVal(struct resVal *resVal);

extern int matchName(char *, char *);
extern char **parseCommandArgs(char *, char *);
extern int FCLOSEUP(FILE **fp);

extern struct listEntry *mkListHeader(void);
extern void offList(struct listEntry *);
extern void inList(struct listEntry *, struct listEntry *);
extern int getResourceNames(int, char **, int, char **);
extern void displayShareResource(int, char **, int, int, int);
extern int makeShareField(char *, int, char ***, char ***, char ***);
