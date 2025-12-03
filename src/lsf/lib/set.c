/* $Id: set.c,v 1.2 2007/08/15 22:18:49 tmizan Exp $
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

#include "lsf/lib/ll.sys.h"
#include "lsf/lib/listset.h"

void listSetFree(struct listSet *);
struct listSet *listSetAlloc(int);

struct listSet *FreeSet = NULL;

collectFreeSet()
{
    struct listSet *ptr, *tmp;

    ptr = FreeSet;
    while (ptr) {
        tmp = ptr;
        ptr = ptr->next;
        free(tmp);
    }
    FreeSet = NULL;
}

int listSetEqual(struct listSet *set1, struct listSet *set2)
{
    while (set1 && set2 && (set1->elem == set2->elem)) {
        set1 = set1->next;
        set2 = set2->next;
    }
    if (!set1 && !set2)
        return true;
    else
        return false;
}

struct listSet *listSetUnion(struct listSet *set1, struct listSet *set2)
{
    struct listSet *setPtr, *setHead;

    setHead = listSetAlloc(-1);
    setPtr = setHead;
    while (set1 && set2) {
        if (set1->elem <= set2->elem) {
            setPtr->next = set1;
            set1 = set1->next;
            setPtr = setPtr->next;
        } else {
            setPtr->next = set2;
            set2 = set2->next;
            setPtr = setPtr->next;
        }
    }
    setPtr->next = NULL;
    if (set1)
        setPtr->next = set1;
    else
        setPtr->next = set2;

    setPtr = setHead;
    setHead = setHead->next;
    setPtr->next = NULL;
    listSetFree(setPtr);
    return setHead;
}

struct listSet *listSetIntersect(struct listSet *set1, struct listSet *set2)
{
    struct listSet *setPtr, *setHead, *setTmp;

    setHead = listSetAlloc(-1);
    setPtr = setHead;
    while (set1 && set2) {
        if (set1->elem == set2->elem) {
            setPtr->next = set1;
            set1 = set1->next;
            setTmp = set2;
            set2 = set2->next;
            setTmp->next = NULL;
            listSetFree(setTmp);
            setPtr = setPtr->next;
        } else if (set1->elem < set2->elem) {
            setTmp = set1;
            set1 = set1->next;
            setTmp->next = NULL;
            listSetFree(setTmp);
        } else {
            setTmp = set2;
            set2 = set2->next;
            setTmp->next = NULL;
            listSetFree(setTmp);
        }
    }

    setPtr->next = NULL;
    if (!set1)
        listSetFree(set1);
    else
        listSetFree(set2);

    setPtr = setHead;
    setHead = setHead->next;
    setPtr->next = NULL;
    listSetFree(setPtr);
    return setHead;
}

struct listSet *listSetDuplicate(struct listSet *set)
{
    struct listSet *setPtr, *setHead;

    setHead = listSetAlloc(-1);
    setPtr = setHead;
    while (set) {
        setPtr->next = listSetAlloc(set->elem);
        setPtr = setPtr->next;
        set = set->next;
    }
    setPtr = setHead;
    setHead = setHead->next;
    setPtr->next = NULL;
    listSetFree(setPtr);
    return setHead;
}

int listSetIn(int elem, struct listSet *set)
{
    while (set) {
        if (set->elem == elem)
            return true;
        set = set->next;
    }
    return false;
}

struct listSet *listSetInsert(int elem, struct listSet *set)
{
    struct listSet *ptr, *ptmp;

    if (listSetIn(elem, set))
        return set;

    if (!set) {
        ptr = listSetAlloc(elem);
        return ptr;
    } else if (set->elem > elem) {
        ptr = listSetAlloc(elem);
        ptr->next = set;
        return ptr;
    }

    ptr = set;
    while (ptr && ptr->next && ptr->next->elem < elem)
        ptr = ptr->next;

    ptmp = ptr->next;
    ptr->next = listSetAlloc(elem);
    ptr->next->next = ptmp;

    return set;
}

struct listSet *listSetSub(struct listSet *set1, struct listSet *set2)
{
    struct listSet *ptr, *tmp;

    if (!set1) {
        listSetFree(set2);
        return NULL;
    }

    if (!set2)
        return set1;

    ptr = set1;
    while (ptr && set2) {
        if (ptr->elem == set2->elem) {
            ptr->elem = -1;
            ptr = ptr->next;
            set2 = set2->next;
        } else if (ptr->elem > set2->elem)
            set2 = set2->next;
        else
            ptr = ptr->next;
    }

    listSetFree(set2);
    ptr = set1;
    while (ptr->next) {
        if (ptr->next->elem < 0) {
            tmp = ptr->next;
            ptr->next = tmp->next;
            tmp->next = NULL;
            listSetFree(tmp);
        } else
            ptr = ptr->next;
    }

    if (set1->elem < 0) {
        tmp = set1;
        set1 = set1->next;
        tmp->next = NULL;
        listSetFree(tmp);
    }

    return set1;
}

struct listSet *listSetAlloc(int elem)
{
    struct listSet *ptr;

    if (FreeSet) {
        ptr = FreeSet;
        FreeSet = FreeSet->next;
    } else {
        ptr = (struct listSet *) malloc(sizeof(struct listSet));
    }
    ptr->elem = elem;
    ptr->next = NULL;
    return ptr;
}

void listSetFree(struct listSet *set)
{
    struct listSet *ptr;

    if (!set)
        return;
    for (ptr = set; ptr->next; ptr = ptr->next)
        ;
    ptr->next = FreeSet;
    FreeSet = set;
}

int listSetk(int k, struct listSet *set)
{
    int i = 0;

    for (i = 1; i < k; i++) {
        if (set)
            set = set->next;
        else
            return 0;
    }

    if (set)
        return (set->elem);
    else
        return 0;
}

int listSetLen(struct listSet *set)
{
    int len = 0;

    for (; set; set = set->next)
        len++;
    return len;
}
