/* $Id: list.c,v 1.3 2007/08/15 22:18:49 tmizan Exp $
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
#include "lsf/lib/list.h"
#include "lsf/lib/intlibout.h"

int listerrno = 0;

static char *errMsg = "%s: expected a non-null %s but got a null one";

LIST_T *listCreate(char *name)
{
    LIST_T *list;

    list = calloc(1, sizeof(LIST_T));
    if (list == NULL) {
        return NULL;
    }

    list->name = strdup(name);
    list->forw = list->back = (LIST_ENTRY_T *) list;

    return list;
}

void listDestroy(LIST_T *list, void (*destroy)(LIST_ENTRY_T *))
{
    static char fname[] = "listDestroy";
    LIST_ENTRY_T *entry;

    if (!list) {
        ls_syslog(LOG_ERR, (errMsg), fname, "list");
        return;
    }

    while (!LIST_IS_EMPTY(list)) {
        entry = list->forw;

        listRemoveEntry(list, entry);
        if (destroy)
            (*destroy)(entry);
        else
            free(entry);
    }

    if (list->allowObservers) {
        listDestroy(list->observers,
                    (LIST_ENTRY_DESTROY_FUNC_T) &listObserverDestroy);
    }

    free(list->name);
    free(list);
}

int listAllowObservers(LIST_T *list)
{
    static char fname[] = "listAllowObservers";

    if (!list) {
        ls_syslog(LOG_ERR, (errMsg), fname, "list");
        return -1;
    }

    if (list->allowObservers)

        return 0;

    list->allowObservers = true;
    list->observers = listCreate("Observer list");

    if (!list->observers)

        return -1;

    return 0;
}

LIST_ENTRY_T *listGetFrontEntry(LIST_T *list)
{
    static char fname[] = "listGetFrontEntry";

    if (!list) {
        ls_syslog(LOG_ERR, (errMsg), fname, "list");
        return (LIST_ENTRY_T *) NULL;
    }

    if (LIST_IS_EMPTY(list))
        return (LIST_ENTRY_T *) NULL;
    else
        return (list->forw);
}

LIST_ENTRY_T *listGetBackEntry(LIST_T *list)
{
    static char fname[] = "listGetBackEntry";

    if (!list) {
        ls_syslog(LOG_ERR, (errMsg), fname, "list");
        return (LIST_ENTRY_T *) NULL;
    }

    if (LIST_IS_EMPTY(list))
        return (LIST_ENTRY_T *) NULL;
    else
        return (list->back);
}

int listInsertEntryBefore(LIST_T *list, LIST_ENTRY_T *succ, LIST_ENTRY_T *entry)
{
    static char fname[] = "listInsertEntryBefore";

    if (!list || !entry || !succ) {
        ls_syslog(LOG_ERR, (errMsg), fname,
                  (!list) ? "list" : ((!entry) ? "entry" : "successor"));

        return -1;
    }

    entry->forw = succ;
    entry->back = succ->back;
    succ->back->forw = entry;
    succ->back = entry;

    if (list->allowObservers && !LIST_IS_EMPTY(list->observers)) {
        LIST_EVENT_T event;

        event.type = LIST_EVENT_ENTER;
        event.entry = entry;

        (void) listNotifyObservers(list, &event);
    }

    list->numEnts++;

    return 0;
}

int listInsertEntryAfter(LIST_T *list, LIST_ENTRY_T *pred, LIST_ENTRY_T *entry)
{
    return listInsertEntryBefore(list, pred->forw, entry);
}

int listInsertEntryAtFront(LIST_T *list, LIST_ENTRY_T *entry)
{
    return listInsertEntryBefore(list, list->forw, entry);
}

int listInsertEntryAtBack(LIST_T *list, LIST_ENTRY_T *entry)
{
    return listInsertEntryBefore(list, (LIST_ENTRY_T *) list, entry);
}

LIST_ENTRY_T *listSearchEntry(LIST_T *list, void *subject,
                              bool_t (*equal)(void *, void *, int), int hint)
{
    static char fname[] = "listSearchEntry";
    LIST_ITERATOR_T iter = {0};
    LIST_ENTRY_T *ent;

    if (!list || !subject || !equal) {
        ls_syslog(LOG_ERR, (errMsg), fname,
                  (!list) ? "list" : ((!subject) ? "subject" : "equal-op"));

        return NULL;
    }

    listIteratorAttach(&iter, list);

    for (ent = listIteratorGetCurEntry(&iter); ent != NULL;
         listIteratorNext(&iter, &ent)) {
        if ((*equal)((void *) ent, subject, hint) == true)
            return ent;
    }

    return (LIST_ENTRY_T *) NULL;
}

void listRemoveEntry(LIST_T *list, LIST_ENTRY_T *entry)
{
    static char fname[] = "listRemoveEntry";

    if (!list || !entry) {
        ls_syslog(LOG_ERR, (errMsg), fname, (!list) ? "list" : "entry");
        return;
    }

    if (entry->back == NULL || entry->forw == NULL) {
        return;
    }

    entry->back->forw = entry->forw;
    entry->forw->back = entry->back;

    if (list->allowObservers && !LIST_IS_EMPTY(list->observers)) {
        LIST_EVENT_T event;

        event.type = LIST_EVENT_LEAVE;
        event.entry = entry;

        (void) listNotifyObservers(list, &event);
    }

    list->numEnts--;
}

int listNotifyObservers(LIST_T *list, LIST_EVENT_T *event)
{
    static char fname[] = "listNotifyObservers";
    LIST_OBSERVER_T *observer;
    LIST_ITERATOR_T iter;

    if (!list || !event) {
        ls_syslog(LOG_ERR, (errMsg), fname, (!list) ? "list" : "event");
        return -1;
    }

    listIteratorAttach(&iter, list->observers);

    for (observer = (LIST_OBSERVER_T *) listIteratorGetCurEntry(&iter);
         !listIteratorIsEndOfList(&iter);
         listIteratorNext(&iter, (LIST_ENTRY_T **) &observer)) {
        ls_syslog(LOG_DEBUG3, "\
%s: Notifying observer \"%s\" of the event <%d>",
                  fname, observer->name, (int) event->type);

        if (observer->select != NULL) {
            if (!(*observer->select)(observer->extra, event))
                continue;
        }

        switch (event->type) {
        case (int) LIST_EVENT_ENTER:
            if (observer->enter)
                (*observer->enter)(list, observer->extra, event);
            break;
        case (int) LIST_EVENT_LEAVE:
            if (observer->leave_)
                (*observer->leave_)(list, observer->extra, event);
            break;
        default:
            ls_syslog(LOG_ERR, "%s: invalide event type %d", fname,
                      event->type);
            return -1;
        }
    }

    return 0;
}

void list2Vector(LIST_T *list, int direction, void *vector,
                 void (*putVecEnt)(void *vector, int index,
                                   LIST_ENTRY_T *entry))
{
    static char fname[] = "list2Vector";
    LIST_ENTRY_T *entry;
    int entIdx;

    if (!list) {
        ls_syslog(LOG_ERR, errMsg, fname, "list");
        return;
    }

    if (direction == 0)
        direction = LIST_TRAVERSE_FORWARD;

    LIST_ITERATOR_T iter = {0};
    listIteratorAttach(&iter, list);

    if (direction & LIST_TRAVERSE_BACKWARD)
        listIteratorSetCurEntry(&iter, listGetBackEntry(list), false);

    entIdx = 0;
    for (entry = listIteratorGetCurEntry(&iter); entry != NULL;
         (direction & LIST_TRAVERSE_FORWARD)
             ? listIteratorNext(&iter, &entry)
             : listIteratorPrev(&iter, &entry)) {
        if (putVecEnt != NULL)
            (*putVecEnt)(vector, entIdx, entry);
        else
            *(void **) ((long) vector + entIdx * sizeof(void *)) =
                (void *) entry;

        entIdx++;
    }
}

void listDisplay(LIST_T *list, int direction,
                 void (*displayFunc)(LIST_ENTRY_T *, void *), void *hint)
{
    static char fname[] = "listDisplay";
    LIST_ENTRY_T *entry;

    if (!list || !displayFunc) {
        ls_syslog(LOG_ERR, errMsg, fname, (!list) ? "list" : "displayFunc");
        return;
    }

    if (direction == 0)
        direction = LIST_TRAVERSE_FORWARD;

    LIST_ITERATOR_T iter = {0};
    listIteratorAttach(&iter, list);

    if (direction & LIST_TRAVERSE_BACKWARD)
        listIteratorSetCurEntry(&iter, listGetBackEntry(list), false);

    for (entry = listIteratorGetCurEntry(&iter); entry != NULL;
         (direction & LIST_TRAVERSE_FORWARD)
             ? listIteratorNext(&iter, &entry)
             : listIteratorPrev(&iter, &entry)) {
        (*displayFunc)(entry, hint);
    }
}

void listCat(LIST_T *list, int direction, char *buffer, int bufferSize,
             char *(*catFunc)(LIST_ENTRY_T *, void *), void *hint)
{
    static char fname[] = "listCat";
    LIST_ENTRY_T *entry;
    int curSize;

    if (!list || !catFunc) {
        ls_syslog(LOG_ERR, errMsg, fname, (!list) ? "list" : "catFunc");
        return;
    }

    buffer[0] = '\000';
    if (direction == 0)
        direction = LIST_TRAVERSE_FORWARD;

    LIST_ITERATOR_T iter = {0};
    listIteratorAttach(&iter, list);

    if (direction & LIST_TRAVERSE_BACKWARD)
        listIteratorSetCurEntry(&iter, listGetBackEntry(list), false);

    curSize = 0;
    for (entry = listIteratorGetCurEntry(&iter); entry != NULL;
         (direction & LIST_TRAVERSE_FORWARD)
             ? listIteratorNext(&iter, &entry)
             : listIteratorPrev(&iter, &entry)) {
        char *str;

        str = (*catFunc)(entry, hint);
        if (!str) {
            ls_syslog(LOG_ERR, "%s: catFunc returned NULL string", fname);
            continue;
        }

        if (curSize + strlen(str) > bufferSize - 1) {
            ls_syslog(LOG_ERR, "%s: the provided buffer is not big enough",
                      fname);
            break;
        }

        strcat(buffer, str);
        curSize += strlen(str);
    }
}

LIST_T *listDup(LIST_T *list, int sizeOfEntry)
{
    static char fname[] = "listDup";
    LIST_T *newList;
    LIST_ENTRY_T *listEntry;
    LIST_ENTRY_T *newListEntry;

    if (!list) {
        ls_syslog(LOG_ERR, "\
%s: expect a non-NULL list but get a NULL one",
                  fname);
        return NULL;
    }

    newList = listCreate(list->name);
    if (!newList) {
        ls_syslog(LOG_ERR, "\
%s: %s",
                  fname, listStrError(listerrno));
        return NULL;
    }

    LIST_ITERATOR_T iter = {0};
    listIteratorAttach(&iter, list);

    for (listEntry = listIteratorGetCurEntry(&iter); listEntry != NULL;
         listIteratorNext(&iter, &listEntry)) {
        newListEntry = (LIST_ENTRY_T *) calloc(1, sizeOfEntry);

        memcpy(newListEntry, listEntry, sizeOfEntry);

        listInsertEntryAtBack(newList, newListEntry);
    }

    listIteratorDetach(&iter);

    return newList;
}

void listDump(LIST_T *list)
{
    static char fname[] = "listDump";
    LIST_ENTRY_T *listEntry;

    ls_syslog(LOG_DEBUG, "\
%s: Attaching iterator to the list <%s> numEnts=%d",
              fname, list->name, list->numEnts);

    LIST_ITERATOR_T iter = {0};
    listIteratorAttach(&iter, list);

    for (listEntry = listIteratorGetCurEntry(&iter);
         !listIteratorIsEndOfList(&iter); listIteratorNext(&iter, &listEntry)) {
        ls_syslog(LOG_DEBUG, "\
%s: Entry=<%x> is in list=<%s>",
                  fname, listEntry, list->name);
    }

    listIteratorDetach(&iter);
}

LIST_OBSERVER_T *listObserverCreate(char *name, void *extra,
                                    LIST_ENTRY_SELECT_OP_T select, ...)
{
    static char fname[] = "listObserverCreate";
    LIST_OBSERVER_T *observer;
    LIST_EVENT_TYPE_T etype;
    LIST_EVENT_CALLBACK_FUNC_T callback;
    va_list ap;

    observer = (LIST_OBSERVER_T *) calloc(1, sizeof(LIST_OBSERVER_T));
    if (observer == NULL) {
        goto Fail;
    }

    observer->name = putstr_(name);
    observer->select = select;
    observer->extra = extra;

    va_start(ap, select);

    for (;;) {
        etype = va_arg(ap, LIST_EVENT_TYPE_T);

        if (etype == LIST_EVENT_NULL)
            break;

        callback = va_arg(ap, LIST_EVENT_CALLBACK_FUNC_T);

        switch (etype) {
        case (int) LIST_EVENT_ENTER:
            observer->enter = callback;
            break;

        case (int) LIST_EVENT_LEAVE:
            observer->leave_ = callback;
            break;

        default:
            ls_syslog(LOG_ERR, "%s: invalid event ID %d from the invoker",
                      fname, (int) etype);
            goto Fail;
        }
    }

    return observer;

Fail:
    FREEUP(observer);
    return (LIST_OBSERVER_T *) NULL;
}

void listObserverDestroy(LIST_OBSERVER_T *observer)
{
    static char fname[] = "listObserverDestroy";

    if (!observer) {
        ls_syslog(LOG_ERR, errMsg, fname, "observer");
        return;
    }

    free(observer->name);
    free(observer);
}

int listObserverAttach(LIST_OBSERVER_T *observer, LIST_T *list)
{
    static char fname[] = "listObserverAttach";
    int rc;

    if (!observer || !list) {
        ls_syslog(LOG_ERR, errMsg, fname, (!list) ? "list" : "observer");
        return -1;
    }

    if (!list->allowObservers) {
        ls_syslog(LOG_ERR, "%s: list does not accept observers: \"%s\"", fname,
                  list->name);
        return -1;
    }

    rc =
        listInsertEntryBefore(list->observers, (LIST_ENTRY_T *) list->observers,
                              (LIST_ENTRY_T *) observer);
    if (rc < 0)
        return rc;

    observer->list = list;
    return 0;
}

void listObserverDetach(LIST_OBSERVER_T *observer, LIST_T *list)
{
    static char fname[] = "listObserverDetach";

    if (!observer || !list) {
        ls_syslog(LOG_ERR, errMsg, fname, (!observer) ? "observer" : "list");
        return;
    }

    if (observer->list)
        listRemoveEntry(observer->list, (LIST_ENTRY_T *) observer);

    observer->list = NULL;
}

LIST_ITERATOR_T *listIteratorCreate(char *name)
{
    LIST_ITERATOR_T *iter;

    iter = (LIST_ITERATOR_T *) calloc(1, sizeof(LIST_ITERATOR_T));
    if (!iter) {
        return NULL;
    }

    iter->name = putstr_(name);
    return iter;
}

void listIteratorDestroy(LIST_ITERATOR_T *iter)
{
    static char fname[] = "listIteratorDestroy";

    if (!iter) {
        ls_syslog(LOG_ERR, errMsg, fname, "iterator");
        return;
    }

    free(iter->name);
    free(iter);
}

int listIteratorAttach(LIST_ITERATOR_T *iter, LIST_T *list)
{
    static char fname[] = "listIteratorAttach";

    if (!iter || !list) {
        ls_syslog(LOG_ERR, errMsg, fname, (!iter) ? "iterator" : "list");
        return -1;
    }

    iter->list = list;
    iter->curEnt = list->forw;

    return 0;
}

void listIteratorDetach(LIST_ITERATOR_T *iter)
{
    static char fname[] = "listIteratorDetach";

    if (!iter) {
        ls_syslog(LOG_ERR, errMsg, fname, "iterator");
        return;
    }

    iter->list = NULL;
    iter->curEnt = NULL;
}

LIST_T *listIteratorGetSubjectList(LIST_ITERATOR_T *iter)
{
    static char fname[] = "listIteratorGetSubjectList";

    if (!iter) {
        ls_syslog(LOG_ERR, errMsg, fname, "iterator");
        return (LIST_T *) NULL;
    }

    return (iter->list);
}

LIST_ENTRY_T *listIteratorGetCurEntry(LIST_ITERATOR_T *iter)
{
    static char fname[] = "listIteratorGetCurEntry";

    if (!iter) {
        ls_syslog(LOG_ERR, errMsg, fname, "iterator");
        return (LIST_ENTRY_T *) NULL;
    }

    if (iter->curEnt == (LIST_ENTRY_T *) iter->list)
        return (LIST_ENTRY_T *) NULL;
    else
        return (iter->curEnt);
}

int listIteratorSetCurEntry(LIST_ITERATOR_T *iter, LIST_ENTRY_T *entry,
                            bool_t validateEnt)
{
    static char fname[] = "listIteratorSetCurEntry";
    LIST_ENTRY_T *savedCurEnt;
    LIST_ENTRY_T *ent;

    if (!iter || !entry) {
        ls_syslog(LOG_ERR, errMsg, fname, (!iter) ? "iterator" : "entry");
        return -1;
    }

    if (validateEnt) {
        bool_t found = false;

        savedCurEnt = iter->curEnt;

        iter->curEnt = listGetFrontEntry(iter->list);
        for (ent = listIteratorGetCurEntry(iter);
             !listIteratorIsEndOfList(iter);
             listIteratorNext(iter, (LIST_ENTRY_T **) &ent)) {
            if (ent == entry) {
                found = true;
                break;
            }
        }

        if (!found) {
            ls_syslog(LOG_ERR, "%s: failed to find the entry in list: %s",
                      fname, iter->list->name);

            iter->curEnt = savedCurEnt;
            return -1;
        }
    }

    iter->curEnt = entry;
    return 0;
}

void listIteratorNext(LIST_ITERATOR_T *iter, LIST_ENTRY_T **next)
{
    static char fname[] = "listIteratorNext";

    if (!iter) {
        ls_syslog(LOG_ERR, errMsg, fname, "iterator");
        *next = NULL;
        return;
    }

    iter->curEnt = iter->curEnt->forw;
    *next = listIteratorGetCurEntry(iter);
}

void listIteratorPrev(LIST_ITERATOR_T *iter, LIST_ENTRY_T **prev)
{
    static char fname[] = "listIteratorPrev";

    if (!iter) {
        ls_syslog(LOG_ERR, errMsg, fname, "iterator");
        *prev = NULL;
        return;
    }

    iter->curEnt = iter->curEnt->back;
    *prev = listIteratorGetCurEntry(iter);
}

bool_t listIteratorIsEndOfList(LIST_ITERATOR_T *iter)
{
    static char fname[] = "listIteratorIsEndOfList";

    if (!iter) {
        ls_syslog(LOG_ERR, errMsg, fname, "iterator");
        return true;
    }

    return (iter->curEnt == (LIST_ENTRY_T *) iter->list);
}

int listerrno;

static char *listErrList[] = {
    "No Error", "Bad arguments", "Memory allocation failed",
    "Permission denied for attaching observers", "Last Error (no error)"};

char *listStrError(int errnum)
{
    static char buf[216];

    if (errnum < 0 || errnum > 4) {
        sprintf(buf, "Unknown error number %d", errnum);
        return buf;
    }

    return (listErrList[errnum]);
}

/* Bug. This listEntry is not the _listEntry just creates confusion, remove.
 */

void inList(struct listEntry *pred, struct listEntry *entry)
{
    entry->forw = pred;
    entry->back = pred->back;
    pred->back->forw = entry;
    pred->back = entry;
}

void offList(struct listEntry *entry)
{
    entry->back->forw = entry->forw;
    entry->forw->back = entry->back;
}

struct listEntry *mkListHeader(void)
{
    struct listEntry *q;

    if ((q = calloc(1, sizeof(struct listEntry))) == NULL)
        return NULL;
    q->forw = q->back = q;
    q->entryData = 0;
    return q;
}
