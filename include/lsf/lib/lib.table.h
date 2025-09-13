/* $Id: lib.table.h,v 1.2 2007/08/15 22:18:51 tmizan Exp $
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

#ifndef _LIB_TABLE_H_
#define _LIB_TABLE_H_

#define RESETFACTOR        2
#define RESETLIMIT         1.5
#define DEFAULT_SLOTS      16

struct hLinks {
    struct hLinks *fwPtr;
    struct hLinks *bwPtr;
};

typedef struct hLinks hLinks;

typedef struct hEnt {
    hLinks links;            
    int     *hData;           
    char    *keyname;        
} hEnt;


typedef struct hTab {
    hLinks *slotPtr;          
    int     numEnts;           
    int     size;              
} hTab;


typedef struct sTab {
    hTab     *tabPtr;     
    int      nIndex;            
    hEnt     *hEntPtr;         
    hLinks   *hList;          
} sTab;

#define HTAB_ZERO_OUT(HashTab) \
{ \
    (HashTab)->numEnts = 0; \
    (HashTab)->size = 0; \
}

#define HTAB_NUM_ELEMENTS(HashTab) (HashTab)->numEnts

#define FOR_EACH_HTAB_ENTRY(Key, Entry, HashTab) \
{ \
    sTab __searchPtr__; \
    (Entry) = h_firstEnt_((HashTab), &__searchPtr__); \
    for ((Entry) = h_firstEnt_((HashTab), &__searchPtr__); \
         (Entry); (Entry) = h_nextEnt_(&__searchPtr__)) { \
	 (Key)   = (char *) (Entry)->keyname;

#define END_FOR_EACH_HTAB_ENTRY  }}

#define FOR_EACH_HTAB_DATA(Type, Key, Data, HashTab) \
{ \
    sTab __searchPtr__; \
    hEnt *__hashEnt__; \
    __hashEnt__ = h_firstEnt_((HashTab), &__searchPtr__); \
    for (__hashEnt__ = h_firstEnt_((HashTab), &__searchPtr__); \
         __hashEnt__; __hashEnt__ = h_nextEnt_(&__searchPtr__)) { \
        (Data) = (Type *) __hashEnt__->hData; \
	(Key)   = (char *) __hashEnt__->keyname;

#define END_FOR_EACH_HTAB_DATA  }}

typedef void       (*HTAB_DATA_DESTROY_FUNC_T)(void *);

extern void        insList_(hLinks *, hLinks *);
extern void        remList_(hLinks *);
extern void        initList_(hLinks *);
extern void        h_initTab_(hTab *, int);
extern void        h_freeTab_(hTab *, void (*destroy)(void *));
extern int         h_TabEmpty_(hTab *);
extern void        h_delTab_(hTab *);
extern hEnt *      h_getEnt_(hTab *, const char *);
extern hEnt *      h_addEnt_(hTab *, const char *, int *);
extern hEnt *      lh_addEnt_(hTab *, char *, int *);
extern void        h_delEnt_(hTab *, hEnt *);
extern void        h_rmEnt_(hTab *, hEnt *);
extern hEnt *      h_firstEnt_(hTab *, sTab *);
extern hEnt *      h_nextEnt_(sTab *);
extern void        h_freeRefTab_(hTab *);
extern void        h_delRef_(hTab *, hEnt *);
#endif




