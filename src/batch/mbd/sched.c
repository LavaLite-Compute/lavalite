/*
 * Copyright (C) LavaLite Contributors
 * GPL v2
 */

#include <stdlib.h>
#include <time.h>

#include "base/lib/ll.list.h"
#include "base/lib/ll.syslog.h"
#include "batch/mbd/mbd.h"

#define SCHED_SORT_BUF_MAX 131072   /* 1M of pointers — no alloc in hot path */

static struct ll_list_entry *sort_buf[SCHED_SORT_BUF_MAX];

static int pend_job_cmp(const void *a, const void *b)
{
    const struct job_data *ja = *(const struct job_data **)a;
    const struct job_data *jb = *(const struct job_data **)b;
    int r;

    /* 1. queue priority (higher = first) */
    r = jb->queue->priority - ja->queue->priority;
    if (r != 0)
        return r;

    /* 2. job priority */
    r = jb->priority - ja->priority;
    if (r != 0)
        return r;

    /* 3. job_id: lower = older = first */
    if (ja->job_id < jb->job_id)
        return -1;

    if (ja->job_id > jb->job_id)
        return 1;

    return 0;
}

void schedule(void)
{
    if (ll_list_is_empty(&pend_jobs_list))
        return;

    ll_list_sort_buf(&pend_jobs_list, sort_buf, pend_job_cmp);
}
