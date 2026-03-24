
#include "batch/lib/batch.h"

struct job_info *llb_job_info(int64_t job_id, int32_t idx, int32_t flags)
{
    (void)job_id;
    (void)idx;
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
