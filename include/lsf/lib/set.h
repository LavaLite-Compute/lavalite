/* $Id: set.h,v 1.2 2007/08/15 22:18:49 tmizan Exp $
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

#ifndef _LLCORE_SET_
#define _LLCORE_SET_

struct listSet {
    int elem;
    struct listSet *next;
};

extern void listSetFree(struct listSet *);
extern struct listSet *listSetAlloc(int);
extern int listSetEqual(struct listSet *, struct listSet *);
extern struct listSet *listSetUnion(struct listSet *, struct listSet *);
extern struct listSet *listSetIntersect(struct listSet *, struct listSet *);
extern struct listSet *listSetDuplicate(struct listSet *);
extern int listSetIn(int, struct listSet *);
extern struct listSet *listSetInsert(int, struct listSet *);
extern struct listSet *listSetSub(struct listSet *, struct listSet *);

#endif
