/* $Id: lib.queue.c,v 1.3 2007/08/15 22:18:51 tmizan Exp $
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

#include "lsf/lib/lib.common.h"
#include "lsf/lib/lib.queue.h"

int
lsQueueInit_(struct lsQueue **head,
             int (*compareFunc)(char *data1, char *data2, int hint),
             void (*destroyFunc)(void *data))
{
    struct lsQueueEntry *qPtr;

    qPtr = calloc(1, sizeof(struct lsQueueEntry));
    if (qPtr == NULL) {
        *head = NULL;
        lserrno = LSE_MALLOC;
        return -1;
    }

    *head = calloc(1, sizeof(struct lsQueue));
    if (*head == NULL)  {
        lserrno = LSE_MALLOC;
        return -1;
    }

    (*head)->compare = compareFunc;
    if (destroyFunc == NULL) {
        (*head)->destroy = (lsQueueDestroyFuncType)free;
    } else {
        (*head)->destroy = (lsQueueDestroyFuncType)destroyFunc;
    }

    (*head)->start = qPtr;
    qPtr->forw = qPtr->back = qPtr;

    return 0;
}

int
lsQueueEntryAppend_(struct lsQueueEntry *entry, struct lsQueue *head)
{
    struct lsQueueEntry *qPtr;

    if (head->start == NULL) {
        lserrno = LSE_MSG_SYS;
        return -1;
    }

    qPtr = head->start;
    entry->back = qPtr->back;
    entry->forw = qPtr;
    qPtr->back->forw = entry;
    qPtr->back  = entry;

    return 0;
}

int
lsQueueDataAppend_(char *data, struct lsQueue *head)
{
    struct lsQueueEntry *entry;
    int rc;

    entry = calloc(1, sizeof(struct lsQueueEntry));
    if (entry == NULL) {
        lserrno = LSE_MALLOC;
        return -1;
    }
    rc =lsQueueEntryAppend_(entry, head);
    entry->data = data;

    return rc;
}

struct lsQueueEntry *
lsQueueDequeue_(struct lsQueue *head)
{
    struct lsQueueEntry *entry, *start;

    if (! head)
        return NULL;

    if (! LS_QUEUE_EMPTY(head)) {
        start = head->start;
        entry = start->forw;
        lsQueueEntryRemove_(entry);
        return entry;
    }

    return NULL;
}

int
lsQueueEntryAddFront_(struct lsQueueEntry *entry, struct lsQueue *head)
{
    struct lsQueueEntry *qPtr;

    if (head->start == NULL) {
        lserrno = LSE_MSG_SYS;
        return -1;
    }

    qPtr = head->start;

    entry->forw = qPtr->forw;
    entry->back = qPtr;
    qPtr->forw->back = entry;
    qPtr->forw = entry;

    return 0;
}

int
lsQueueDataAddFront_(char *data, struct lsQueue *head)
{
    struct lsQueueEntry *entry;
    int rc;

    entry = (struct lsQueueEntry *)malloc(sizeof(struct lsQueueEntry));
    if (entry == NULL) {
        lserrno = LSE_MALLOC;
        return -1;
    }
    rc =lsQueueEntryAddFront_(entry, head);
    entry->data = data;

    return rc;
}

void
lsQueueEntryRemove_(struct lsQueueEntry *entry)
{
    entry->back->forw = entry->forw;
    entry->forw->back = entry->back;
    entry->forw = entry->back = NULL;

}

void
lsQueueEntryDestroy_(struct lsQueueEntry *entry, struct lsQueue *head)
{
    if (entry->forw != NULL)
        lsQueueEntryRemove_(entry);

    if (entry->data) {
        if (head->destroy) {
            (*(head->destroy))(entry->data);
        }
    }

    free(entry);
}

void
lsQueueEntryDestroyAll_(struct lsQueue *head)
{
    struct lsQueueEntry *start, *qPtr, *nextQPtr;

    start = head->start;
    for (qPtr = start->forw; qPtr != start; qPtr = nextQPtr) {
        nextQPtr = qPtr->forw;

        lsQueueEntryDestroy_(qPtr, head);
    }
}

void
lsQueueDestroy_(struct lsQueue *head)
{
    struct lsQueueEntry *start, *qPtr, *nextQPtr;

    start = head->start;
    for (qPtr = start->forw; qPtr != start; qPtr = nextQPtr) {
        nextQPtr = qPtr->forw;

        lsQueueEntryDestroy_(qPtr, head);
    }
    free(head->start);
    free(head);
}

struct lsQueueEntry *
lsQueueSearch_(int hint, char *val, struct lsQueue *head)
{
    struct lsQueueEntry *start, *qPtr, *nextQPtr;
    int found;
    int rc;

    if (head->compare == NULL)
        return NULL;

    start = head->start;
    found = false;

    for (qPtr = start->forw; qPtr != start; qPtr = nextQPtr) {
        nextQPtr = qPtr->forw;

        rc = (*(head->compare))(val, qPtr->data, hint);
        if (rc == 0) {
            found = true;
            break;
        }
    }

    if (found == false)
        return (struct lsQueueEntry *)NULL;
    else
        return qPtr;

}

void
lsQueueSetAdd_(struct lsQueue *head1, struct lsQueue *head2,
               bool_t (*memberFunc)(struct lsQueueEntry *q, struct lsQueue *head))
{
    struct lsQueueEntry *start, *qEnt;

    start = head2->start;
    for (qEnt = start->forw;  qEnt != start; qEnt = qEnt->forw)
        if ((*memberFunc)(qEnt, head1) == false)  {
            lsQueueEntryRemove_(qEnt);
            lsQueueEntryAppend_(qEnt, head1);
        } else {
            lsQueueEntryRemove_(qEnt);
        }

}

void
lsQueueSort_(struct lsQueue *head, int hint)
{
    struct lsQueueEntry *start;
    struct lsQueueEntry *q1, *q2;
    struct lsQueueEntry *nq1;
    struct lsQueueEntry *selected;
    int rc;

    start = head->start;
    for (q1 = start->forw; q1 != start; q1 = nq1) {
        selected = q1;
        for (q2 = q1->forw; q2 != start; q2 = q2->forw) {
            rc = (*head->compare)((char *)selected, (char *)q2, hint);

            if (rc > 0)
                selected = q2;
        }

        lsQueueEntryRemove_(selected);
        lsQueueEntryAddFront_(selected, head);

        nq1 = selected->forw;
    }
}

int lsQueueDequeueData_(struct lsQueue *head, char **data)
{
    struct lsQueueEntry *ent = lsQueueDequeue_(head);
    if (ent == NULL) {
        return 0;
    }
    *data = ent->data;
    free(ent);
    return 1;
}

void lsQueueIter_(struct lsQueue *head, void (*func)(char *, void *), void *hdata)
{
    struct lsQueueEntry *start, *qPtr;

    start = head->start;

    for (qPtr = start->forw; qPtr != start; qPtr = qPtr->forw) {
        (*func)(qPtr->data, hdata);
    }
}
