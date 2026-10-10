/*
 * Copyright (C) LavaLite Contributors
 * GPL v2
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <assert.h>

#include "base/lib/auth.h"
#include "base/lib/ll.syslog.h"
#include "base/lib/ll.sys.h"
#include "batch/lib/wire.h"
#include "batch/lib/rpc.h"
#include "batch/mbd/mbd.h"

/* prototype defaults -- promote to llb.services config later if a real
 * service ever needs to ask for more than this */
#define SVC_DEFAULT_NUM_CPUS    1
#define SVC_DEFAULT_NUM_HOSTS   1
#define SVC_DEFAULT_MEM_MB      0
#define SVC_DEFAULT_STORAGE_MB  0

struct service_data *svc_find_by_name(const char *name)
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

static int service_build_script(const struct service_instance *inst,
                                char *buf, size_t bufsiz)
{
    int n = snprintf(buf, bufsiz,
                     "#!/bin/sh\n"
                     "# LavaLite: environment\n"
                     "HOME='%s'; export HOME\n"
                     "USER='%s'; export USER\n"
                     "PATH='/usr/bin:/bin:/usr/local/bin'; export PATH\n"
                     "# LavaLite: end environment\n"
                     "# LavaLite: user command\n"
                     "%s\n"
                     "ExitStat=$?\n"
                     "echo \"$ExitStat $(date +%%s)\" > \"$LL_JOBDIR/exit\"\n"
                     "exit $ExitStat\n",
                     inst->pend_ws.home_dir,
                     inst->pend_ws.username,
                     inst->pend_ws.command);

    if (n < 0 || (size_t) n >= bufsiz) {
        LL_ERRX("script truncated uid=%u", inst->uid);
        return -1;
    }

    return n;
}

static struct job_data *service_job_create(struct service_instance *inst,
                                           int *err)
{
    char script_text[LL_BUFSIZ_8K];

    int script_len = service_build_script(inst, script_text, sizeof(script_text));
    if (script_len < 0) {
        *err = EINVAL;
        return NULL;
    }

    struct wire_job_script script;
    memset(&script, 0, sizeof(script));
    script.data = script_text;
    script.len = (uint32_t)script_len;

    struct job_data *job =
        job_prepare(&inst->pend_ws, &script, &inst->pend_hdr, err);
    if (job == NULL)
        return NULL;

    job->svc_inst = inst;
    inst->job_id = job->job_id;

    job_commit(job, &inst->pend_ws);
    job_id_seq_write(); /* sequence must never go backwards */

    return job;
}

static struct service_instance *svc_find_running_endpoint(uid_t uid,
                                                          const char *host,
                                                          int port)
{
    struct service_instance *found = NULL;

    for (struct ll_list_entry *se = service_list.head;
         se != NULL; se = se->next) {

        struct service_data *svc = (struct service_data *)se;

        for (struct ll_list_entry *ie = svc->instances.head;
             ie != NULL; ie = ie->next) {

            struct service_instance *inst = (struct service_instance *)ie;

            if (inst->uid != uid)
                continue;

            if (inst->status != SVC_RUNNING)
                continue;

            if (inst->port != port)
                continue;

            if (strcmp(inst->run_host, host) != 0)
                continue;

            if (found != NULL) {
                LL_ERRX("duplicate running service endpoint uid=%u host=%s "
                        "port=%d job1=%ld job2=%ld",  uid, host, port,
                        found->job_id, inst->job_id);
                assert(found == NULL);
            }
            found = inst;
        }
    }

    return found;
}

int service_start_instance(const struct protocol_header *hdr, int chan_id,
                           const struct wire_svc_start *ws)
{
    struct service_data *svc = svc_find_by_name(ws->name);
    if (svc == NULL) {
        LL_ERRX("service=%s asked by uid=%u not found", ws->name, hdr->uid);
        return ESRCH;
    }

    struct service_instance *inst = calloc(1, sizeof(*inst));
    if (inst == NULL) {
        LL_ERR("calloc failed");
        return ENOMEM;
    }

    inst->svc = svc;
    inst->chan_id = chan_id;
    inst->uid = hdr->uid;

    // Build the synthetic wire_job_submit structure
    memset(&inst->pend_ws, 0, sizeof(inst->pend_ws));
    int n = snprintf(inst->pend_ws.command, sizeof(inst->pend_ws.command),
                     "%s exec --bind %s:%s %s %s",
                     svc->runtime,
                     ws->home_dir,
                     ws->home_dir,
                     svc->image,
                     svc->command);
    if (n < 0 || n >= (int) sizeof(inst->pend_ws.command)) {
        LL_ERRX("command too long service=%s", ws->name);
        free(inst);
        return EINVAL;
    }

    inst->pend_ws.flags |= JOB_FLAG_SERVICE;

    ll_strlcpy(inst->pend_ws.name, svc->name, sizeof(inst->pend_ws.name));
    ll_strlcpy(inst->pend_ws.queue, svc->queue, sizeof(inst->pend_ws.queue));
    ll_strlcpy(inst->pend_ws.username, ws->username,
               sizeof(inst->pend_ws.username));
    ll_strlcpy(inst->pend_ws.home_dir, ws->home_dir,
               sizeof(inst->pend_ws.home_dir));
    ll_strlcpy(inst->pend_ws.cwd, ws->home_dir, sizeof(inst->pend_ws.cwd));
    inst->pend_ws.num_cpus = SVC_DEFAULT_NUM_CPUS;
    inst->pend_ws.num_hosts = SVC_DEFAULT_NUM_HOSTS;
    inst->pend_ws.mem_mb = SVC_DEFAULT_MEM_MB;
    inst->pend_ws.storage_mb = SVC_DEFAULT_STORAGE_MB;

    /* requester's identity, used by job_prepare() */
    inst->pend_hdr = *hdr;
    inst->status = SVC_PENDING;

    int err;
    struct job_data *job = service_job_create(inst, &err);
    if (job == NULL) {
        LL_ERRX("failed create service job for uid=%u err=%d", hdr->uid, err);
        free(inst);
        return err;
    }

    ll_list_append(&svc->instances, &inst->ent);

    /* No reply yet: BATCH_SERVICE_START_ACK is deferred until
     * mbd_new_job_reply() sees this job reach RUNNING, then the
     * client gets run_host:port.
     */
    LL_INFO("SVC_PENDING service=%s job=%ld uid=%u cmd=[%s]", svc->name,
            inst->job_id, inst->uid, inst->pend_ws.command);

    return 0;
}

static void svc_inst_to_wire(const struct service_instance *inst,
                             struct wire_svc_instance_info *w)
{
    memset(w, 0, sizeof(*w));

    ll_strlcpy(w->service, inst->svc->name, sizeof(w->service));
    w->uid = inst->uid;
    w->port = inst->port;
    w->job_id = inst->job_id;
    w->status = inst->status;

    if (inst->run_host[0] != 0)
        ll_strlcpy(w->run_host, inst->run_host, sizeof(w->run_host));
}

int service_collect_info(uid_t uid, int all, struct wire_svc_info **out)
{
    *out = NULL;

    int nsvc = ll_list_count(&service_list);
    if (nsvc == 0)
        return 0;

    struct wire_svc_info *dst = calloc(nsvc, sizeof(*dst));
    if (dst == NULL) {
        LL_ERR("calloc failed");
        errno = ENOMEM;
        return -1;
    }

    int n = 0;

    for (struct ll_list_entry *se = service_list.head; se != NULL;
         se = se->next) {
        struct service_data *svc = (struct service_data *) se;
        struct wire_svc_info *wsvc = &dst[n];

        ll_strlcpy(wsvc->name, svc->name, sizeof(wsvc->name));
        ll_strlcpy(wsvc->queue, svc->queue, sizeof(wsvc->queue));

        uint32_t ninstances = 0;

        for (struct ll_list_entry *ie = svc->instances.head; ie != NULL;
             ie = ie->next) {
            struct service_instance *inst = (struct service_instance *) ie;

            if (!all && inst->uid != uid)
                continue;

            ninstances++;
        }

        wsvc->ninstances = ninstances;
        if (wsvc->ninstances == 0) {
            ++n;
            continue;
        }

        wsvc->instances = calloc(ninstances, sizeof(*wsvc->instances));
        if (wsvc->instances == NULL) {
            LL_ERR("calloc failed");
            errno = ENOMEM;
            goto fail;
        }

        uint32_t j = 0;
        for (struct ll_list_entry *ie = svc->instances.head;
             ie != NULL;
             ie = ie->next) {
            struct service_instance *inst = (struct service_instance *) ie;

            if (!all && inst->uid != uid)
                continue;

            svc_inst_to_wire(inst, &wsvc->instances[j]);
            j++;
        }
        n++;
    }

    *out = dst;
    return nsvc;

fail:
    for (int i = 0; i < nsvc; i++)
        free(dst[i].instances);

    free(dst);
    return -1;
}

void service_job_running(struct job_data *job, struct mbd_host *host,
                         int32_t port)
{
    if (job->svc_inst == NULL)
        return;

    struct service_instance *inst = job->svc_inst;

    ll_strlcpy(inst->run_host, host->net.name, sizeof(inst->run_host));

    inst->status = SVC_RUNNING;
    inst->port = port;

    /* Only the first incarnation has a client waiting on the deferred
     * ack. After a restart chan_id is -1: the old channel is closed
     * or, worse, reused by another client.
     */
    if (inst->chan_id < 0) {
        LL_INFO("channel already invalid job=%ld endpoint=%s:%d restart=%u",
                job->job_id, inst->run_host, inst->port,
                inst->restart_count);
        return;
    }

    struct wire_svc_instance_info info;
    svc_inst_to_wire(inst, &info);

    struct protocol_header rep_hdr;
    init_protocol_header(&rep_hdr);
    rep_hdr.operation = BATCH_SERVICE_START_ACK;
    rep_hdr.status = MBD_OK;

    int chan_id = inst->chan_id;
    inst->chan_id = -1;

    if (auth_sign_header(&rep_hdr) < 0) {
        LL_ERR("auth_sign_header failed job=%ld uid=%u", job->job_id,
               inst->uid);
        return;
    }

    size_t siz = PACKET_HEADER_SIZE
        + xdr_sizeof((xdrproc_t)xdr_wire_svc_instance_info, &info);

    if (enqueue_payload(chan_id, &rep_hdr, &info, siz,
                        xdr_wire_svc_instance_info) < 0) {
        LL_ERR("enqueue_payload failed job=%ld uid=%u", job->job_id,
               inst->uid);
        return;
    }

    LL_INFO("job=%ld endpoint=%s:%d client acked",
            job->job_id, inst->run_host, inst->port);
}

int service_delete_instance(uid_t uid, const char *host, int port)
{
    LL_INFO("uid=%u host=%s port=%d", uid, host, port);

    struct service_instance *inst = svc_find_running_endpoint(uid, host, port);
    if (inst == NULL) {
        LL_ERRX("cannot find instance for uid=%u port=%d host=%s", uid, port,
                host);
        return ESRCH;
    }
    assert(strcmp(inst->run_host, host) == 0);

    struct job_data *job = job_find(inst->job_id);
    if (job == NULL) {
        LL_ERRX("cannot find job=%ld for service=%s uid=%u port=%d",
                inst->job_id, inst->svc->name, uid, port);
        return ESRCH;
    }

    assert(job->flags & JOB_FLAG_SERVICE);
    assert(job->svc_inst == inst);

    struct wire_job_sig sig;
    memset(&sig, 0, sizeof(sig));
    sig.job_id = job->job_id;
    sig.sig = SIGKILL;

    uint32_t old_flags = inst->flags;
    inst->flags |= SVC_FLAG_TERMINATE;

    int rc = signal_running_job(job, &sig);
    if (rc != MBD_OK) {
        inst->flags = old_flags;
        LL_ERRX("cannot kill job=%ld service=%s", job->job_id, inst->svc->name);
        return rc;
    }

    return MBD_OK;
}

int service_instance_finish(struct service_instance *inst)
{
    assert(inst != NULL);

    struct job_data *job = job_find(inst->job_id);
    assert(job != NULL);
    assert(job->svc_inst == inst);
    assert(job->list_id == JOB_LIST_FINISH);
    assert(job->state == JOB_DONE || job->state == JOB_EXITED);

    inst->status = SVC_FINISH;

    if (inst->flags & SVC_FLAG_TERMINATE) {
        LL_INFO("SVC_FINISH terminated by bservice service=%s uid=%u "
                "job=%ld", inst->svc->name, inst->uid, inst->job_id);
        return 0;
    }

    inst->flags |= SVC_FLAG_RESTART_PENDING;

    /* mbd_job_finish() already released resources and decremented
     * the queue counters. Restore only the pending job counters.
     */
    job->queue->num_jobs++;
    job->queue->num_pend++;

    job->pid = 0;
    job->fork_time = 0;
    job->dispatch_time = 0;
    job->end_time = 0;
    job->exit_status = 0;
    job->pend_reason = PEND_NONE;

    memset(job->run_hosts, 0,
           job->res.num_hosts * sizeof(job->run_hosts[0]));
    job->run_nhosts = 0;

    job->state = JOB_PENDING;
    job_move_list(job, &finish_jobs_list, &pend_jobs_list, JOB_LIST_PEND);

    /* endpoint belongs to the old incarnation, the next dispatch
     * picks a new host and port */
    inst->status = SVC_PENDING;
    inst->port = 0;
    inst->run_host[0] = 0;

    event_job_pend(job);
    mbd_assert_counters();

    LL_INFO("service=%s job=%ld requeued for restart", inst->svc->name,
            job->job_id);

    return 0;
}

int job_is_service(const struct job_data *job)
{
    if ((job->flags & JOB_FLAG_SERVICE)) {
        assert(job->svc_inst);
        return 1;
    }

    return 0;
}

/* Invalidate the channel if the user reset the connection
 */
void service_invalidate_chan(int chan_id)
{
    for (struct ll_list_entry *se = service_list.head; se != NULL;
         se = se->next) {

        struct service_data *svc = (struct service_data *)se;

        for (struct ll_list_entry *ie = svc->instances.head; ie != NULL;
             ie = ie->next) {
            struct service_instance *inst = (struct service_instance *)ie;
            if (inst->chan_id == chan_id) {
                LL_DEBUG("job=%ld service=%s channel=%d", inst->job_id,
                         inst->svc->name, chan_id);
                inst->chan_id = -1;
                return;
            }
        }

    }
}
