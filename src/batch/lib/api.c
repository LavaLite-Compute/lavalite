/* Copyright (C) LavaLite Contributors
 * GPL v2
 */

#include <stdlib.h>
#include <stdint.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/epoll.h>

#include "base/lib/ll.protocol.h"
#include "batch/lib/batch.h"

#include "llbatch.h"

struct job_info *llb_job_info(int64_t job_id, int32_t *num_job, int32_t flags)
{
    (void)job_id;
    (void)num_job;
    (void)flags;
    return NULL;
}

void llb_free_job_info(struct job_info *info, int32_t count)
{
    (void)info;
    (void)count;
}

struct host_info *llb_host_info(int32_t *count)
{
    (void)count;
    return NULL;
}

struct host_group *llb_group_info(int32_t *count)
{
    (void)count;
    return NULL;
}

struct queue_info *llb_queue_info(int32_t *count)
{
    (void)count;
    return NULL;
}

int32_t llb_signal_job(int64_t job_id, int32_t sig)
{
    (void)job_id;
    (void)sig;
    return -1;
}
