/*
 * Copyright (C) LavaLite Contributors
 * GPL v2
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <pwd.h>

#include "base/lib/ll.syslog.h"
#include "base/lib/ll.sys.h"
#include "batch/lib/wire.h"
#include "batch/mbd/mbd.h"

/* prototype defaults -- promote to llb.services config later if a real
 * service ever needs to ask for more than this */
#define SVC_DEFAULT_NUM_CPUS    1
#define SVC_DEFAULT_NUM_HOSTS   1
#define SVC_DEFAULT_MEM_MB      0
#define SVC_DEFAULT_STORAGE_MB  0

#define LL_SVC_PORT_MIN 30000
#define LL_SVC_PORT_MAX 35000

void service_init(void)
{
    ll_list_init(&service_list);
    LL_INFO("Services initialized");
}

/*
 * True if port is held by any live instance across every service.
 * Realistic instance counts are tens, not thousands -- a linear scan
 * over what's already there beats maintaining a second bookkeeping
 * structure (bitmap/hash) just for this.
 */
static int svc_port_in_use(int port)
{
    for (struct ll_list_entry *se = service_list.head; se != NULL;
         se = se->next) {
        struct service_data *svc = (struct service_data *) se;

        for (struct ll_list_entry *ie = svc->instances.head; ie != NULL;
             ie = ie->next) {
            struct service_instance *inst = (struct service_instance *) ie;

            if (inst->port == port)
                return 1;
        }
    }

    return 0;
}

/*
 * Rotating-cursor allocation over [LL_SVC_PORT_MIN, LL_SVC_PORT_MAX].
 * No free/mark-inuse bookkeeping to keep in sync -- svc_port_in_use()
 * IS the allocation state.
 */
static int svc_port_alloc(void)
{
    static int next = LL_SVC_PORT_MIN;
    int start = next;

    do {
        int port = next++;
        if (next > LL_SVC_PORT_MAX)
            next = LL_SVC_PORT_MIN;
        if (!svc_port_in_use(port))
            return port;
    } while (next != start);

    return -1; /* range exhausted */
}

static struct service_data *svc_find_by_name(const char *name)
{
    struct ll_list_entry *e;
    struct service_data *svc;

    for (e = service_list.head; e != NULL; e = e->next) {
        svc = (struct service_data *) e;
        if (strcmp(svc->name, name) == 0)
            return svc;
    }

    return NULL;
}

/*
 * Real implementation. Still missing: the ADD to service_proxy (the
 * socket/reconnect layer isn't built yet) and the mbd_new_job_reply()
 * RUNNING-transition callback that fires the proxy UPDATE and the
 * deferred BATCH_SERVICE_START_ACK -- both come next.
 */
int service_start_instance(const struct protocol_header *hdr, int chan_id,
                           const char *name)
{
    struct service_data *svc = svc_find_by_name(name);
    if (svc == NULL) {
        LL_ERRX("service_start_instance: service=%s not found", name);
        return ESRCH;
    }

    struct passwd *pw = getpwuid2(hdr->uid);
    if (pw == NULL || pw->pw_name == NULL) {
        LL_ERR("service_start_instance: getpwuid2(%u) failed", hdr->uid);
        return EINVAL;
    }

    int port = svc_port_alloc();
    if (port < 0) {
        LL_ERRX("service_start_instance: port range exhausted service=%s",
                name);
        return ENOSPC;
    }

    struct service_instance *inst = calloc(1, sizeof(*inst));
    if (inst == NULL) {
        LL_ERR("calloc failed");
        return ENOMEM;
    }

    inst->svc = svc;
    inst->port = port;
    inst->chan_id = chan_id;
    inst->state = SVC_INST_STARTING;
    snprintf(inst->svc_id, sizeof(inst->svc_id), "%s@%s", pw->pw_name, name);

    char cmd[LL_BUFSIZ_512];
    int n = snprintf(cmd, sizeof(cmd), "apptainer exec %s %s",
                     svc->image, svc->command);
    if (n < 0 || n >= (int) sizeof(cmd)) {
        LL_ERRX("service_start_instance: command too long service=%s", name);
        free(inst);
        return EINVAL;
    }

    struct wire_job_submit ws;
    memset(&ws, 0, sizeof(ws));
    ll_strlcpy(ws.name, inst->svc_id, sizeof(ws.name));
    ll_strlcpy(ws.queue, svc->queue, sizeof(ws.queue));
    ll_strlcpy(ws.username, pw->pw_name, sizeof(ws.username));
    ll_strlcpy(ws.home_dir, pw->pw_dir, sizeof(ws.home_dir));
    ll_strlcpy(ws.cwd, pw->pw_dir, sizeof(ws.cwd));
    ll_strlcpy(ws.command, cmd, sizeof(ws.command));
    ws.num_cpus = SVC_DEFAULT_NUM_CPUS;
    ws.num_hosts = SVC_DEFAULT_NUM_HOSTS;
    ws.mem_mb = SVC_DEFAULT_MEM_MB;
    ws.storage_mb = SVC_DEFAULT_STORAGE_MB;
    ws.flags = JOB_FLAG_SERVICE;

    /* the synthesized command IS the whole job -- no user shell script
     * for a service, unlike an ordinary bsub */
    struct wire_job_script script;
    memset(&script, 0, sizeof(script));
    script.data = cmd;
    script.len = (uint32_t) strlen(cmd);

    int err = 0;
    struct job_data *job = job_prepare(&ws, &script, hdr, &err);
    if (job == NULL) {
        LL_ERRX("service_start_instance: job_prepare failed service=%s err=%d",
                name, err);
        free(inst);
        return err;
    }

    job->svc_inst = inst;
    inst->job_id = job->job_id;

    ll_list_append(&svc->instances, &inst->ent);
    job_commit(job, &ws);

    /* TODO: ADD svc_id:<svc_id> port:<port> image:<image>
     * app_port:<svc->port> to service_proxy over the SEQPACKET socket
     * once that connection layer exists. */

    LL_INFO("service_start_instance: service=%s svc_id=%s job_id=%ld port=%d",
            name, inst->svc_id, job->job_id, inst->port);

    /* No reply here -- BATCH_SERVICE_START_ACK is deferred until
     * mbd_new_job_reply() sees this job reach RUNNING. */
    return 0;
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
