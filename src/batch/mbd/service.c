/*
 * Copyright (C) LavaLite Contributors
 * GPL v2
 */

#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include "base/lib/ll.syslog.h"
#include "batch/lib/wire.h"
#include "batch/mbd/mbd.h"

void service_init(void)
{
    ll_list_init(&service_list);
    LL_INFO("Services initialized");
}

/*
 * Stub only. Real implementation still to come: port allocation from
 * the configured range, the ADD to service_proxy, job_prepare()/
 * job_commit() to create the backing job, the job tag, and the
 * mbd_new_job_reply() callback that fires the deferred
 * BATCH_SERVICE_START_ACK once the job reaches RUNNING.
 */
int service_start_instance(const struct protocol_header *hdr, int chan_id,
                           const char *name)
{
    (void) hdr;
    (void) chan_id;

    LL_ERRX("service_start_instance: not implemented name=%s", name);
    return ENOSYS;
}

/*
 * Stub only. No instances tracked yet -- always reports an empty list.
 */
int service_collect_info(uid_t uid, int all, struct wire_svc_info **out)
{
    (void) uid;
    (void) all;

    *out = NULL;
    return 0;
}

/*
 * Stub only. Real implementation still to come: instance lookup by
 * svc_id, ownership check, signal the backing job, REMOVE to
 * service_proxy, free the port.
 */
int service_stop_instance(uid_t uid, const char *svc_id)
{
    (void) uid;

    LL_ERRX("service_stop_instance: not implemented svc_id=%s", svc_id);
    return ENOSYS;
}
