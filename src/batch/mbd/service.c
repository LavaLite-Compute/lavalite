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

/* Port range and allocation are GONE from here. service_proxy owns the
 * range (SP_SVC_PORT_MIN/MAX in service_proxy.h) and does the real
 * bind() -- mbd never sees or picks a port number anymore. The old
 * bind()-probe-then-close() approach here was a real TOCTOU/false-
 * positive risk: the probe socket closed before service_proxy ever
 * bound the port for real, leaving a window for the OS to hand it to
 * something else. */

int service_proxy_chan_id = -1;

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
 * Send BATCH_SVC_ADD to service_proxy. spd is an ordinary connected
 * client of mbd, same as sbd -- this is chan_enqueue/chan_dequeue and
 * XDR over the existing service_proxy_chan_id, the same enqueue_payload()
 * path every other opcode in this file uses. No separate socket, no
 * text grammar.
 */
static int svc_proxy_send_add(struct service_instance *inst)
{
    if (service_proxy_chan_id < 0) {
        LL_ERRX("svc_proxy_send_add: service_proxy not connected uid=%ld proxy_port=%d",
                (long) inst->uid, inst->port);
        return -1;
    }

    struct wire_svc_add req;
    memset(&req, 0, sizeof(req));
    req.uid = inst->uid;
    req.app_port = inst->svc->port;
    req.job_id = inst->job_id;

    struct protocol_header hdr;
    init_protocol_header(&hdr);
    hdr.operation = BATCH_SVC_ADD;
    hdr.status = MBD_OK;

    if (auth_sign_header(&hdr) < 0) {
        LL_ERR("svc_proxy_send_add: auth_sign_header failed uid=%ld proxy_port=%d",
               (long) inst->uid, inst->port);
        return -1;
    }

    size_t siz = sizeof(struct protocol_header) + sizeof(struct wire_svc_add)
                 + LL_BUFSIZ_64;

    if (enqueue_payload(service_proxy_chan_id, &hdr, &req, siz,
                        xdr_wire_svc_add) < 0) {
        LL_ERR("svc_proxy_send_add: enqueue_payload failed uid=%ld proxy_port=%d",
               (long) inst->uid, inst->port);
        return -1;
    }

    LL_DEBUG("service: uid=%ld proxy_port=%d ADD sent to proxy chan=%d",
             (long) inst->uid, inst->port, service_proxy_chan_id);

    return 0;
}

static int service_build_script(const struct service_instance *inst,
                                char *buf, size_t bufsiz)
{
    int n = snprintf(buf, bufsiz,
        "#!/bin/sh\n"
        "# LavaLite: environment\n"
        "HOME='%s'; export HOME\n"
        "USER='%s'; export USER\n"
        "PATH='/usr/bin:/bin'; export PATH\n"
        "# LavaLite: end environment\n"
        "# LavaLite: user command\n"
        "%s\n"
        "ExitStat=$?\n"
        "echo \"$ExitStat $(date +%%s)\" > \"$LL_JOBDIR/exit\"\n"
        "exit $ExitStat\n",
        inst->pend_ws.home_dir, inst->pend_ws.username, inst->pend_cmd);

    if (n < 0 || (size_t) n >= bufsiz) {
        LL_ERRX("service_build_script: script truncated uid=%ld proxy_port=%d",
               (long) inst->uid, inst->port);
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

static struct service_instance *svc_find_instance(uid_t uid, int port)
{
    for (struct ll_list_entry *se = service_list.head; se != NULL;
         se = se->next) {
        struct service_data *svc = (struct service_data *) se;

        for (struct ll_list_entry *ie = svc->instances.head; ie != NULL;
             ie = ie->next) {
            struct service_instance *inst =
                (struct service_instance *) ie;

            if (inst->uid == uid && inst->port == port)
                return inst;
        }
    }

    return NULL;
}

int service_start_instance(const struct protocol_header *hdr, int chan_id,
                           const struct wire_svc_start *ws)
{
    struct service_data *svc = svc_find_by_name(ws->name);
    if (svc == NULL) {
        LL_ERRX("service_start_instance: service=%s asked by uid=%u not found",
                ws->name, hdr->uid);
        return ESRCH;
    }

    struct service_instance *inst = calloc(1, sizeof(*inst));
    if (inst == NULL) {
        LL_ERR("calloc failed");
        return ENOMEM;
    }

    inst->svc = svc;
    inst->chan_id = chan_id;
    inst->state = SVC_INST_STARTING;
    inst->uid = hdr->uid;

    int n = snprintf(inst->pend_cmd, sizeof(inst->pend_cmd),
                     "%s exec --bind %s:%s %s %s",
                     svc->runtime, ws->home_dir, ws->home_dir,
                     svc->image, svc->command);
    if (n < 0 || n >= (int) sizeof(inst->pend_cmd)) {
        LL_ERRX("service_start_instance: command too long service=%s",
               ws->name);
        free(inst);
        return EINVAL;
    }

    // Build the synthetic wire_job_submit structure
    memset(&inst->pend_ws, 0, sizeof(inst->pend_ws));
    inst->pend_ws.flags |= JOB_FLAG_SERVICE | JOB_FLAG_HOLD;

    ll_strlcpy(inst->pend_ws.name, svc->name, sizeof(inst->pend_ws.name));
    ll_strlcpy(inst->pend_ws.queue, svc->queue, sizeof(inst->pend_ws.queue));
    ll_strlcpy(inst->pend_ws.username, ws->username,
              sizeof(inst->pend_ws.username));
    ll_strlcpy(inst->pend_ws.home_dir, ws->home_dir,
              sizeof(inst->pend_ws.home_dir));
    ll_strlcpy(inst->pend_ws.cwd, ws->home_dir, sizeof(inst->pend_ws.cwd));
    ll_strlcpy(inst->pend_ws.command, inst->pend_cmd,
              sizeof(inst->pend_ws.command));
    inst->pend_ws.num_cpus = SVC_DEFAULT_NUM_CPUS;
    inst->pend_ws.num_hosts = SVC_DEFAULT_NUM_HOSTS;
    inst->pend_ws.mem_mb = SVC_DEFAULT_MEM_MB;
    inst->pend_ws.storage_mb = SVC_DEFAULT_STORAGE_MB;

    /* original requester's identity, same as it would have gotten
     * synchronously before this split -- and so a phase-2 failure can
     * still enqueue_header() the right client on the right chan_id.
     */
    inst->pend_hdr = *hdr;

    int err;
    struct job_data *job = service_job_create(inst, &err);
    if (job == NULL) {
        LL_ERRX("failed create service job for uid=%u err=%d", hdr->uid, err);
        free(inst);
        return err;
    }
    inst->job_id = job->job_id;

    LL_DEBUG("service: job_id=%ld uid=%ld proxy_port=%d cmd=[%s]",
             inst->job_id, (long) inst->uid, inst->port, inst->pend_cmd);

    ll_list_append(&svc->instances, &inst->ent);

    if (svc_proxy_send_add(inst) < 0) {
        LL_ERRX("proxy ADD failed service job_id=%ld uid=%u", inst->job_id,
                hdr->uid);
        /* Signal the job as bkill -s kill which will cleanup the job
         * correctly and update the counters
         */
        struct wire_job_sig sig;
        memset(&sig, 0, sizeof(sig));
        sig.job_id = job->job_id;
        sig.sig = SIGKILL;
        sig.uid = job->uid;
        signal_pending_job(job, &sig);
        job->svc_inst = NULL;
        ll_list_remove(&svc->instances, &inst->ent);
        free(inst);
        return ENOTCONN;
    }

    LL_INFO("service_start_instance: service=%s uid=%ld proxy_port=%d: ADD sent, "
           "awaiting port from proxy", ws->name, (long) inst->uid, inst->port);

    return 0;
}

void svc_proxy_add_ack(XDR *xdrs, const struct protocol_header *hdr)
{
    struct wire_svc_add_ack ack;
    memset(&ack, 0, sizeof(ack));

    if (!xdr_wire_svc_add_ack(xdrs, &ack)) {
        LL_ERR("xdr decode failed");
        return;
    }

    struct job_data *job = job_find(ack.job_id);
    if (job == NULL) {
        LL_ERRX("job_id=%ld not found?", ack.job_id);
        abort();
        return;
    }

    struct service_instance *inst = job->svc_inst;
    if (inst == NULL) {
        LL_ERRX("job_id=%ld has no service instance", ack.job_id);
        abort();
        return;
    }

    if (hdr->status != MBD_OK) {
        LL_ERRX("job_id=%ld uid=%ld proxy_port=%d failed status=%d",
                ack.job_id, (long) inst->uid, inst->port, hdr->status);
        enqueue_header(inst->chan_id, BATCH_SERVICE_START_ACK, hdr->status);

        struct wire_job_sig sig;
        memset(&sig, 0, sizeof(sig));
        sig.job_id = job->job_id;
        sig.sig = SIGKILL;
        sig.uid = job->uid;
        signal_pending_job(job, &sig);
        job->svc_inst = NULL;
        ll_list_remove(&inst->svc->instances, &inst->ent);
        free(inst);
        return;
    }

    inst->port = ack.port;
    /* Release the job from JOB_HELD state
     */
    struct wire_job_sig sig;
    memset(&sig, 0, sizeof(sig));
    sig.job_id = job->job_id;
    sig.sig = SIGCONT;
    sig.uid = job->uid;

    signal_pending_job(job, &sig);
    LL_INFO("job_id=%ld uid=%ld proxy_port=%d", ack.job_id,
            (long) inst->uid, inst->port);
    /* Still no reply to the client -- BATCH_SERVICE_START_ACK remains
     * deferred until mbd_new_job_reply() sees this job reach RUNNING.
     */
}

static int32_t svc_inst_state_to_wire(int state)
{
    switch (state) {
    case SVC_INST_STARTING:
        return SVC_PENDING;
    case SVC_INST_RUNNING:
        return SVC_RUNNING;
    default:
        /* unreachable in practice -- unknown internal state reported
         * as NONE
         */
        return SVC_NONE;
    }
}

/*
 * One instance -> one wire_svc_info row. Same shape as
 * svc_job_running()'s own info-fill block above, just generalized to
 * any instance/state instead of always RUNNING.
 */
static void svc_inst_to_wire(const struct service_instance *inst,
                             struct wire_svc_info *w)
{
    memset(w, 0, sizeof(*w));
    ll_strlcpy(w->name, inst->svc->name, sizeof(w->name));
    w->uid = inst->uid;
    w->port = inst->port;
    ll_strlcpy(w->run_host, inst->run_host, sizeof(w->run_host));
    w->job_id = inst->job_id;
    w->state = svc_inst_state_to_wire(inst->state);
}

/*
 * One configured-but-idle service_data -> one wire_svc_info row.
 * Same reasoning as bqueues showing an empty queue: a service defined
 * in llb.services is visible whether or not anyone has started it.
 * run_host/port/job_id all stay zeroed -- "-" for run_host is
 * bservice.c's own display convention for an empty field (see
 * print_services()'s `s[i].run_host ? ... : "-"`), not something mbd
 * encodes. state stays at the memset zero -- SVC_NONE is 0 in
 * llbatch.h precisely so this needs no explicit assignment.
 */
static void svc_def_to_wire(const struct service_data *svc,
                            struct wire_svc_info *w)
{
    memset(w, 0, sizeof(*w));
    ll_strlcpy(w->name, svc->name, sizeof(w->name));
}

/*
 * Two-layer walk of service_list, same shape bqueues uses for queues:
 * a service with no live instances gets one definition row
 * (svc_def_to_wire); a service with instances gets one row per
 * instance instead (svc_inst_to_wire), never both. uid/all filtering
 * only applies to instance rows -- a configured-but-idle service is
 * ownership-neutral, same as an empty queue in bqueues.
 *
 * Two-pass shape (count, then calloc, then fill) matches
 * jobs_info_list()'s collect_list() in dispatch.c.
 */
int service_collect_info(uid_t uid, int all, struct wire_svc_info **out)
{
    *out = NULL;
    int ntotal = 0;

    for (struct ll_list_entry *se = service_list.head; se != NULL;
         se = se->next) {
        struct service_data *svc = (struct service_data *) se;
        int ninst = ll_list_count(&svc->instances);

        if (ninst == 0)
            ntotal++;
        else
            ntotal += ninst;
    }

    // no services are configured
    if (ntotal == 0)
        return 0;

    struct wire_svc_info *dst = calloc(ntotal, sizeof(*dst));
    if (dst == NULL) {
        LL_ERR("calloc failed");
        errno = ENOMEM;
        return -1;
    }

    int n = 0;
    for (struct ll_list_entry *se = service_list.head; se != NULL;
         se = se->next) {
        struct service_data *svc = (struct service_data *) se;

        if (svc->instances.head == NULL) {
            svc_def_to_wire(svc, &dst[n]);
            n++;
            continue;
        }

        for (struct ll_list_entry *ie = svc->instances.head; ie != NULL;
             ie = ie->next) {
            struct service_instance *inst = (struct service_instance *) ie;

            if (!all && inst->uid != uid)
                continue;

            svc_inst_to_wire(inst, &dst[n]);
            n++;
        }
    }

    *out = dst;
    return n;
}

/*
 * Send BATCH_SVC_UPDATE to service_proxy -- run_host changed (initial
 * dispatch, or redispatch after a restart landing elsewhere). Same
 * enqueue_payload() path as svc_proxy_send_add()/send_remove().
 */
static int svc_proxy_send_update(struct service_instance *inst)
{
    if (service_proxy_chan_id < 0) {
        LL_ERRX("service_proxy not connected job_id=%ld uid=%ld proxy_port=%d",
                inst->job_id, (long) inst->uid, inst->port);
        return -1;
    }

    struct wire_svc_update req;
    memset(&req, 0, sizeof(req));
    req.job_id = inst->job_id;
    ll_strlcpy(req.run_host, inst->run_host, sizeof(req.run_host));

    struct protocol_header hdr;
    init_protocol_header(&hdr);
    hdr.operation = BATCH_SVC_UPDATE;
    hdr.status = MBD_OK;

    if (auth_sign_header(&hdr) < 0) {
        LL_ERR("auth_sign_header failed job_id=%ld uid=%ld proxy_port=%d",
               inst->job_id, (long) inst->uid, inst->port);
        return -1;
    }

    size_t siz = sizeof(struct protocol_header) +
                 sizeof(struct wire_svc_update) + LL_BUFSIZ_64;

    if (enqueue_payload(service_proxy_chan_id, &hdr, &req, siz,
                        xdr_wire_svc_update) < 0) {
        LL_ERR("enqueue_payload failed job_id=%ld uid=%ld proxy_port=%d",
               inst->job_id, (long) inst->uid, inst->port);
        return -1;
    }

    LL_DEBUG("job_id=%ld uid=%ld proxy_port=%d UPDATE run_host=%s sent to proxy chan=%d",
             inst->job_id, (long) inst->uid, inst->port, inst->run_host,
             service_proxy_chan_id);

    return 0;
}

/*
 * Correlation acks for BATCH_SVC_UPDATE/BATCH_SVC_REMOVE. Nothing on
 * mbd's side blocks waiting for these -- the client-facing state
 * (run_host, instance teardown) is already updated locally before the
 * request was even sent -- but we still decode and log them instead
 * of silently dropping, so a proxy-side failure is visible instead of
 * invisible.
 */
void svc_proxy_update_ack(XDR *xdrs, const struct protocol_header *hdr)
{
    struct wire_svc_update_ack ack;
    memset(&ack, 0, sizeof(ack));

    if (!xdr_wire_svc_update_ack(xdrs, &ack)) {
        LL_ERR("xdr decode failed");
        return;
    }

    if (hdr->status != MBD_OK) {
        LL_ERRX("job_id=%ld failed status=%d", ack.job_id, hdr->status);
        return;
    }

    LL_DEBUG("svc_proxy_update_ack: job_id=%ld ok", ack.job_id);
}

static int svc_proxy_send_remove(struct service_instance *inst)
{
    if (service_proxy_chan_id < 0) {
        LL_ERRX("service_proxy not connected job_id=%ld uid=%ld proxy_port=%d",
                inst->job_id, (long) inst->uid, inst->port);
        return -1;
    }

    struct wire_svc_remove req;
    memset(&req, 0, sizeof(req));
    req.job_id = inst->job_id;

    struct protocol_header hdr;
    init_protocol_header(&hdr);
    hdr.operation = BATCH_SVC_REMOVE;
    hdr.status = MBD_OK;

    if (auth_sign_header(&hdr) < 0) {
        LL_ERR("auth_sign_header failed job_id=%ld uid=%ld proxy_port=%d",
               inst->job_id, (long) inst->uid, inst->port);
        return -1;
    }

    size_t siz = sizeof(struct protocol_header) +
                 sizeof(struct wire_svc_remove) + LL_BUFSIZ_64;

    if (enqueue_payload(service_proxy_chan_id, &hdr, &req, siz,
                        xdr_wire_svc_remove) < 0) {
        LL_ERR("enqueue_payload failed job_id=%ld uid=%ld proxy_port=%d",
               inst->job_id, (long) inst->uid, inst->port);
        return -1;
    }

    LL_DEBUG("job_id=%ld uid=%u proxy_port=%d REMOVE sent to proxy chan=%d",
             inst->job_id, inst->uid, inst->port, service_proxy_chan_id);

    return 0;
}

void svc_proxy_remove_ack(XDR *xdrs, const struct protocol_header *hdr)
{
    struct wire_svc_remove_ack ack;
    memset(&ack, 0, sizeof(ack));

    if (!xdr_wire_svc_remove_ack(xdrs, &ack)) {
        LL_ERR("xdr decode failed");
        return;
    }

    if (hdr->status != MBD_OK) {
        LL_ERRX("job_id=%ld failed status=%d", ack.job_id, hdr->status);
        return;
    }

    LL_DEBUG("job_id=%ld ok", ack.job_id);
}

/*
 * RUNNING-transition callback, called from job.c's mbd_new_job_reply()
 * the moment a service job's fork is acked by sbd. This is the last
 * step of the whole bservice flow: tell proxy which host the backing
 * job landed on (redispatch across a restart can change this), then
 * finally reply to the client that's been blocked in
 * call_mbd_timeout() since service_start_instance() -- carrying the
 * real wire_svc_info llb_service_start() already expects to decode.
 */
void svc_job_running(struct job_data *job, struct mbd_host *host)
{
    struct service_instance *inst = job->svc_inst;

    ll_strlcpy(inst->run_host, host->net.name, sizeof(inst->run_host));
    inst->state = SVC_INST_RUNNING;

    /* mbd doesn't block this RUNNING-transition reply on the UPDATE
     * ack -- svc_proxy_update_ack() logs the result asynchronously,
     * same request/ack shape as everything else on this channel.
     */
    svc_proxy_send_update(inst);

    struct wire_svc_info info;
    memset(&info, 0, sizeof(info));
    ll_strlcpy(info.name, inst->svc->name, sizeof(info.name));
    info.uid = inst->uid;
    info.port = inst->port;
    ll_strlcpy(info.run_host, inst->run_host, sizeof(info.run_host));
    info.job_id = job->job_id;
    info.state = SVC_RUNNING;

    struct protocol_header rep_hdr;
    init_protocol_header(&rep_hdr);
    rep_hdr.operation = BATCH_SERVICE_START_ACK;
    rep_hdr.status = MBD_OK;

    if (auth_sign_header(&rep_hdr) < 0) {
        LL_ERR("svc_job_running: auth_sign_header failed uid=%ld proxy_port=%d",
               (long) inst->uid, inst->port);
        return;
    }

    size_t siz = sizeof(struct protocol_header) + sizeof(struct wire_svc_info)
                 + LL_BUFSIZ_64;

    if (enqueue_payload(inst->chan_id, &rep_hdr, &info, siz,
                        xdr_wire_svc_info) < 0) {
        LL_ERR("svc_job_running: enqueue_payload failed uid=%ld proxy_port=%d",
               (long) inst->uid, inst->port);
        return;
    }

    LL_INFO("svc_job_running: job_id=%ld uid=%ld proxy_port=%d run_host=%s "
           "RUNNING, client acked", job->job_id, (long) inst->uid,
           inst->port, inst->run_host);
}

int service_delete_instance(uid_t uid, const char *host, int port)
{
    LL_INFO("uid=%u host=%s proxy_port=%d", uid, host, port);

    struct service_instance *inst = svc_find_instance(uid, port);
    if (inst == NULL) {
        LL_ERRX("cannot find instance for uid=%u port=%d host=%s", uid, port, host);
        return -1;
    }
    assert(strcmp(inst->run_host, host) == 0);

    struct job_data *job = job_find(inst->job_id);
    if (job == NULL) {
        LL_ERRX("cannot find job_id=%ld for service=%s uid=%u port=%d",
                inst->job_id, inst->svc->name, uid, port);
        return -1;
    }

    assert(job->flags & JOB_FLAG_SERVICE);
    assert(job->svc_inst == inst);

    struct wire_job_sig sig;
    memset(&sig, 0, sizeof(sig));
    sig.job_id = job->job_id;
    sig.sig = SIGKILL;

    int rc = signal_running_job(job, &sig);
    if (rc != MBD_OK) {
        LL_ERRX("cannot kill job_id=%ld service=%s", job->job_id, inst->svc->name);
        return rc;
    }

    return MBD_OK;
}

int svc_service_instance_destroy(struct service_instance *inst)
{
    assert(inst != NULL);

    struct job_data *job = job_find(inst->job_id);
    assert(job != NULL);
    assert(job->svc_inst == inst);

    int rc = svc_proxy_send_remove(inst);
    if (rc < 0) {
        LL_ERRX("cannot remove proxy mapping job_id=%ld uid=%u "
                "proxy_port=%d", inst->job_id, inst->uid, inst->port);
    }

    job->svc_inst = NULL;

    ll_list_remove(&inst->svc->instances, &inst->ent);

    LL_INFO("destroyed service=%s uid=%ld proxy_port=%d job_id=%ld",
            inst->svc->name, (long) inst->uid, inst->port, inst->job_id);

    free(inst);

    return 0;
}

/*
 * spd connects in and registers, same accept path as sbd
 * (mbd_sbd_register() in sbd.c) but simpler: no per-job resync
 * payload, since service_proxy has no job state of its own -- the
 * real service registry resync (ADD per live instance) is a separate
 * exchange, not folded into this ack.
 */
int mbd_sp_register(XDR *xdrs, int chan_id, struct protocol_header *hdr)
{
    struct wire_sp_register reg;
    memset(&reg, 0, sizeof(reg));

    if (!xdr_wire_sp_register(xdrs, &reg)) {
        LL_ERR("SP_REGISTER decode failed");
        return -1;
    }

    char hostname[MAXHOSTNAMELEN];
    memcpy(hostname, reg.hostname, sizeof(hostname));
    hostname[sizeof(hostname) - 1] = 0;

    if (service_proxy_chan_id != -1) {
        LL_ERRX("duplicate service_proxy registration from host=%s "
                "uid=%u already on chan=%d, rejecting", hostname, hdr->uid,
                service_proxy_chan_id);
        return -1;
    }

    service_proxy_chan_id = chan_id;
    LL_INFO("service_proxy registered host=%s chan=%d", hostname, chan_id);

    struct protocol_header hdr2;
    init_protocol_header(&hdr2);
    hdr2.operation = BATCH_SP_REGISTER_ACK;
    hdr2.status = MBD_OK;

    if (auth_sign_header(&hdr2) < 0) {
        LL_ERR("auth_sign_header failed for service_proxy ack");
        service_proxy_chan_id = -1;
        return -1;
    }


    size_t siz = sizeof(struct protocol_header) + sizeof(struct wire_sp_register)
        + LL_BUFSIZ_64;

    if (enqueue_payload(chan_id, &hdr2, &reg, siz, xdr_wire_sp_register) < 0) {
        LL_ERR("enqueue_payload failed for service_proxy ack");
        service_proxy_chan_id = -1;
        return -1;
    }

    /* TODO once svc RESYNC exists: push ADD for every currently-live
     * service_instance here, so proxy rebuilds its forwarding table
     * on (re)connect instead of starting empty -- matters for mbd
     * restarts as much as proxy restarts. */

    return 0;
}
