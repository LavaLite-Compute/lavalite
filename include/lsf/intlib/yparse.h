/* $Id: yparse.h,v 1.3 2007/08/15 22:18:49 tmizan Exp $
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

#ifndef MIN
#define MIN(x, y)  ((x) <= (y)) ? (x):(y)
#endif
#ifndef MAX
#define MAX(x, y)  ((x) >= (y)) ? (x):(y)
#endif

#ifndef TRUE
#define TRUE     1
#endif
#ifndef FALSE
#define FALSE    0
#endif


#define INFINIT_INT    0x7fffffff
#define MAXTOKENLEN 302

struct intRegion {
    int start, end;
};

struct listLink {
    struct intRegion iconList;
    struct listLink  *next;
};

struct mallocList {
    void               *space;  
    struct mallocList  *next;   
};

extern char *token;
extern char yyerr[];
extern FILE *yyout ;
extern struct mbd_func_type mbd_func;
extern struct mallocList  *idxAllocHead;

extern int     yylex(void);
extern void    calerror(register char *);
extern char   *safe_calloc(unsigned, unsigned);
extern void    timerror(register char *);
extern void   *yyalloc(struct mallocList **, int);
extern void    yyfree(struct mallocList **, void *);
extern void    yparseSucc(struct mallocList **);
extern void    yparseFail(struct mallocList **);
extern struct  calendarE * getCalExprNode();
extern void    idxerror(register char *);

 
extern int     checkNameSpec(char *, char **);
