/* $Id: listset.c,v 1.3 2007/08/15 22:18:49 tmizan Exp $
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 USA
 *
 */

#include "lsf/intlib/llsys.h"
#include "lsf/intlib/listset.h"

void listSetFree(struct listSet *);
struct listSet *listSetAlloc(long);

struct listSet *FreeSet = NULL;

void collectFreeSet(void)
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

    if (!(setHead = listSetAlloc(-1))) {
        listSetFree(set1);
        listSetFree(set2);
        return NULL;
    }
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
    struct listSet *setA, *setB, *tmp;

    setA = set1;
    setB = set2;

    while (setA) {
        if (setB == NULL) {
            setA->elem = -1;
            setA = setA->next;
        } else if (setA->elem == setB->elem) {
            setA = setA->next;
            setB = setB->next;
        } else if (setA->elem < setB->elem) {
            setA->elem = -1;
            setA = setA->next;
        } else {
            setB = setB->next;
        }
    }

    listSetFree(set2);

    setA = set1;
    while (setA && setA->next) {
        if (setA->next->elem < 0) {
            tmp = setA->next;
            setA->next = setA->next->next;
            tmp->next = NULL;
            listSetFree(tmp);
        } else {
            setA = setA->next;
        }
    }

    if (set1 && set1->elem < 0) {
        tmp = set1;
        set1 = set1->next;
        tmp->next = NULL;
        listSetFree(tmp);
    }
    return set1;
}

struct listSet *listSetDuplicate(struct listSet *set)
{
    struct listSet *setPtr, *setHead;

    if (!(setHead = listSetAlloc(-1))) {
        return NULL;
    }
    setPtr = setHead;
    while (set) {
        if (!(setPtr->next = listSetAlloc(set->elem))) {
            listSetFree(setHead);
            return NULL;
        }
        setPtr = setPtr->next;
        set = set->next;
    }
    setPtr = setHead;
    setHead = setHead->next;
    setPtr->next = NULL;
    listSetFree(setPtr);
    return setHead;
}

int listSetMember(long elem, struct listSet *set)
{
    while (set) {
        if (set->elem == elem)
            return true;
        set = set->next;
    }
    return false;
}

struct listSet *listSetDel(long elem, struct listSet *set)
{
    struct listSet *ptr, *ptmp;

    if (!set)
        return set;

    if (set->elem == elem) {
        ptr = set;
        set = set->next;
        ptr->next = NULL;
        listSetFree(ptr);
        return set;
    }
    ptr = set;
    while (ptr && ptr->next && ptr->next->elem != elem) {
        ptr = ptr->next;
    }

    if (ptr && ptr->next) {
        ptmp = ptr->next;
        ptr->next = ptmp->next;
        ptmp->next = NULL;
        listSetFree(ptmp);
    }
    return set;
}

struct listSet *listSetInsert(long elem, struct listSet *set)
{
    struct listSet *ptr, *ptmp;

    if (listSetMember(elem, set))
        return set;

    if (!set) {
        if (!(ptr = listSetAlloc(elem)))
            return NULL;
        return ptr;
    }

    if (set->elem > elem) {
        if (!(ptr = listSetAlloc(elem)))
            return NULL;
        ptr->next = set;
        return ptr;
    }

    ptr = set;
    while (ptr && ptr->next && ptr->next->elem < elem)
        ptr = ptr->next;

    ptmp = ptr->next;
    if (!(ptr->next = listSetAlloc(elem)))
        return NULL;
    ptr->next->next = ptmp;

    return set;
}

struct listSet *listSetSub(struct listSet *set1, struct listSet *set2)
{
    struct listSet *ptr1, *ptr2, *tmp;

    if (!set1) {
        listSetFree(set2);
        return NULL;
    }

    if (!set2)
        return set1;

    ptr1 = set1;
    ptr2 = set2;
    while (ptr1 && ptr2) {
        if (ptr1->elem == ptr2->elem) {
            ptr1->elem = -1;
            ptr1 = ptr1->next;
            ptr2 = ptr2->next;
        } else if (ptr1->elem > ptr2->elem)
            ptr2 = ptr2->next;
        else
            ptr1 = ptr1->next;
    }

    listSetFree(set2);
    ptr1 = set1;
    while (ptr1->next) {
        if (ptr1->next->elem < 0) {
            tmp = ptr1->next;
            ptr1->next = ptr1->next->next;
            tmp->next = NULL;
            listSetFree(tmp);
        } else
            ptr1 = ptr1->next;
    }

    if (set1->elem < 0) {
        tmp = set1;
        set1 = set1->next;
        tmp->next = NULL;
        listSetFree(tmp);
    }

    return set1;
}

struct listSet *listSetAlloc(long elem)
{
    struct listSet *ptr;

    if (FreeSet) {
        ptr = FreeSet;
        FreeSet = FreeSet->next;
    } else {
        if (!(ptr = calloc(1, sizeof(struct listSet))))
            return NULL;
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

int listSetGetEle(int k, struct listSet *set)
{
    int i = 0;

    if (k < 1)
        return 0;

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

int listSetNumEle(struct listSet *set)
{
    int len = 0;

    for (; set; set = set->next)
        len++;
    return len;
}

struct listSet *listSetSelect(long start, long end, struct listSet *set)
{
    struct listSet *head = NULL, *low = NULL, *up = NULL, *ptr;

    if (start > end) {
        listSetFree(set);
        return NULL;
    }
    ptr = set;
    while (ptr && ptr->elem <= end) {
        if (ptr->elem < start) {
            low = ptr;
        }
        if (ptr->elem <= end)
            up = ptr;
        ptr = ptr->next;
    }

    if ((low && (!low->next)) || (!up)) {
        listSetFree(set);
        return NULL;
    } else {
        listSetFree(up->next);
        up->next = NULL;
        if (low) {
            head = low->next;
            low->next = NULL;
            listSetFree(set);
        } else
            head = set;
    }
    return head;
}

struct listSetIterator *listSetIteratorCreate(void)
{
    struct listSetIterator *iter;

    iter = calloc(1, sizeof(struct listSetIterator));
    if (!iter) {
        return NULL;
    }

    return iter;
}
void listSetIteratorAttach(struct listSet *set, struct listSetIterator *iter)
{
    iter->pos = set;
}
long *listSetIteratorBegin(struct listSetIterator *iter)
{
    long *elem_addr;

    elem_addr = &(iter->pos->elem);
    iter->pos = iter->pos->next;

    return elem_addr;
}

long *listSetIteratorGetNext(struct listSetIterator *iter)
{
    long *elem_addr;

    if (iter->pos == NULL) {
        return NULL;
    }

    elem_addr = &(iter->pos->elem);
    iter->pos = iter->pos->next;

    return elem_addr;
}

long *listSetIteratorEnd(struct listSetIterator *iter)
{
    return NULL;
}

void listSetIteratorDestroy(struct listSetIterator *iter)
{
    listSetIteratorDetach(iter);
    free(iter);
}

void listSetIteratorDetach(struct listSetIterator *iter)
{
    iter->pos = NULL;
}
