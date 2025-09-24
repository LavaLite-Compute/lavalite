/* $Id: lib.queue.h,v 1.2 2007/08/15 22:18:51 tmizan Exp $
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

#ifndef INCLUDE_LIB_QUEUE_H
#define INCLUDE_LIB_QUEUE_H

struct lsQueueEntry {
    struct lsQueueEntry *forw;
    struct lsQueueEntry *back;
    char *data;
};

typedef void (*lsQueueDestroyFuncType)(void *);
struct lsQueue {
    struct lsQueueEntry *start;

    int (*compare)(char *data1, char *data2, int hint);


    lsQueueDestroyFuncType destroy;
};

#define LS_QUEUE_EMPTY(Head) ((Head)->start->forw == (Head)->start)

extern void tMsgDestroy_(void *);
extern int lsReqCmp_(char *, char *, int);
extern int  lsQueueInit_(struct lsQueue **head,
			 int (*compare)(char *, char *, int),

			 lsQueueDestroyFuncType destroy);

extern int  lsQueueEntryAddFront_(struct lsQueueEntry *entry,
			       struct lsQueue *head);
extern int  lsQueueDataAddFront_(char *data, struct lsQueue *head);

extern int  lsQueueEntryAppend_(struct lsQueueEntry *entry,
			       struct lsQueue *head);
extern int  lsQueueDataAppend_(char *data, struct lsQueue *head);
extern void lsQueueEntryRemove_(struct lsQueueEntry *entry);
extern void lsQueueEntryDestroy_(struct lsQueueEntry *entry, struct lsQueue *head);
extern void lsQueueEntryDestroyAll_(struct lsQueue *head);
extern void lsQueueDestroy_(struct lsQueue *head);
extern struct lsQueueEntry * lsQueueDequeue_(struct lsQueue *head);
extern struct lsQueueEntry * lsQueueSearch_(int hint, char *data, struct lsQueue *head);
extern void lsQueueSetAdd_(struct lsQueue *q1, struct lsQueue *q2,
			   bool_t (*memberFunc)(struct lsQueueEntry *,
					     struct lsQueue *));
extern void lsQueueSort_(struct lsQueue *q, int hint);
extern int lsQueueDequeueData_(struct lsQueue *head, char **data);
extern void lsQueueIter_(struct lsQueue *head,
			 void (*func)(char *data, void *hdata),
			 void *hdata);

#endif
