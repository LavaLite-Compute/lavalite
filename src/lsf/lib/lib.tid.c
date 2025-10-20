/* $Id: lib.tid.c,v 1.3 2007/08/15 22:18:51 tmizan Exp $
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
#include "lsf/lib/lib.h"
#include "lsf/lib/lib.queue.h"

#define TID_BNUM   23
#define tid_index(x)   (x%TID_BNUM)

static struct tid *tid_buckets[TID_BNUM];

int
tid_register(int taskid, int socknum, u_short taskPort, char *host, bool_t doTaskInfo)
{
    struct tid *tidp;
    int i;

    if ((tidp = calloc(1, sizeof(struct tid))) == NULL) {
        lserrno = LSE_MALLOC;
        return -1;
    }

    tidp->rtid = taskid;
    tidp->sock = socknum;
    tidp->taskPort = taskPort;

    tidp->host = putstr_(host);

    i = tid_index(taskid);
    tidp->link = tid_buckets[i];
    tid_buckets[i] = tidp;


    if (doTaskInfo) {
        lsQueueInit_(&tidp->tMsgQ, NULL, tMsgDestroy_);
        if (tidp->tMsgQ == NULL) {
            return -1;
        }
    } else
        tidp->tMsgQ = NULL;


    tidp->refCount = (doTaskInfo) ? 2 : 1;
    tidp->isEOF = (doTaskInfo)? false : true;

    return 0;
}

int
tid_remove(int taskid)
{
    int i = tid_index(taskid);
    struct tid *p1, *p2=NULL;

    p1 = tid_buckets[i];

    while (p1 != (struct tid *)NULL) {
        if (p1->rtid == taskid)
            break;
        p2 = p1;
        p1 = p2->link;
    }

    if (p1 == (struct tid *)NULL)
        return -1;

    p1->refCount--;
    if (p1->refCount > 0)
        return 0;

    if (p1 == tid_buckets[i])
        tid_buckets[i] = p1->link;
    else
        p2->link = p1->link;

    if (p1->tMsgQ)
        lsQueueDestroy_(p1->tMsgQ);

    free(p1);

    return 0;

}

struct tid *
tid_find(int taskid)
{
    int i = tid_index(taskid);
    struct tid *p1;

    p1 = tid_buckets[i];
    while (p1 != (struct tid *)NULL) {
        if (p1->rtid == taskid) {
            if (p1->sock == -1) {
                lserrno = LSE_LOSTCON;
                return NULL;
            }
            return p1;
        }
        p1 = p1->link;
    }

    lserrno = LSE_RES_INVCHILD;
    return NULL;
}

struct tid *
tidFindIgnoreConn_(int taskid)
{
    int i = tid_index(taskid);
    struct tid *p1;

    p1 = tid_buckets[i];
    while (p1 != (struct tid *)NULL) {
        if (p1->rtid == taskid) {
            return p1;
        }
        p1 = p1->link;
    }

    lserrno = LSE_RES_INVCHILD;
    return NULL;
}


void
tid_lostconnection(int socknum)
{
    int i;
    struct tid *p1;

    for (i = 0; i < TID_BNUM; i++) {
        p1 = tid_buckets[i];
        while (p1 != (struct tid *)NULL) {
            if (p1->sock == socknum)
                p1->sock = -1;
            p1 = p1->link;
        }
    }
}

int
tidSameConnection_(int socknum, int *ntids, int **tidArray)
{
    int tidCnt = 0;
    int i;
    struct tid *p1;
    int *intp;

    *tidArray = calloc(TID_BNUM, sizeof(int));

    if (! *tidArray) {
        lserrno = LSE_MALLOC;
        return -1;
    }

    intp = *tidArray;
    for (i = 0; i < TID_BNUM; i++) {
        p1 = tid_buckets[i];
        while (p1 != (struct tid *)NULL) {
            if (p1->sock == socknum) {
                *intp = p1->rtid;
                intp++;
                tidCnt++;
            }
            p1 = p1->link;
        }
    }

    if (ntids)
        *ntids = tidCnt;

    return 0;

}
