/*
 * Copyright (C) LavaLite Contributors
 * GPL v2
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <pwd.h>

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

 * Find an instance across every service by svc_id. Used by the proxy
 * ADD_OK/ADD_FAIL handlers below to resolve an async reply back to the
 * instance that asked for it. Same linear-scan-over-service_list shape
 * that svc_port_in_use() used to be -- realistic instance counts are
 * tens, not thousands.
 */
static struct service_instance *svc_find_instance_by_id(const char *svc_id)
{
    for (struct ll_list_entry *se = service_list.head; se != NULL;
         se = se->next) {
        struct service_data *svc = (struct service_data *) se;

        for (struct ll_list_entry *ie = svc->instances.head; ie != NULL;
             ie = ie->next) {
            struct service_instance *inst = (struct service_instance *) ie;

            if (strcmp(inst->svc_id, svc_id) == 0)
                return inst;
        }
    }
    return NULL;
}
/*
 * Send ADD svc_id:<svc_id> to service_proxy over the plain-text
 * svc_proto.h grammar. This is deliberately NOT xdr -- enqueue_payload()
 * (net.c) needs an xdr_func and svc_proto.h's grammar was decided
 * plain-text specifically to avoid that, so this can't reuse
 * enqueue_payload() as written. Needs a raw-send counterpart in net.c
 * (something like enqueue_raw(chan_id, buf, len)) that doesn't exist
 * yet -- everything below the LL_DEBUG is a placeholder for it.
 */
static int svc_proxy_send_add(struct service_instance *inst)
{
    if (service_proxy_chan_id < 0) {
        LL_ERRX("svc_proxy_send_add: service_proxy not connected svc_id=%s",
                inst->svc_id);
        return -1;
    }

    LL_DEBUG("service: svc_id=%s: ADD sent to proxy chan=%d", inst->svc_id,
             service_proxy_chan_id);


    /* TODO: format "ADD svc_id:%s\n" (or whatever svc_proto.h settles
     * on) and send it via net.c's raw-send path once that exists. */

    return 0;
}

/*
 * Phase 1 of service start: validate, build the instance and the job
 * submission it will need, ask service_proxy for a port, return
 * without touching job_prepare()/job_commit() at all. The job only
 * gets created once svc_proxy_add_ok() (below) fires with the real
 * port -- mbd never allocates or probes a port itself anymore.
 *
 * No reply to the client here either way: dispatch.c's service_start()
 * already replies immediately on a non-zero return (synchronous
 * validation failure), and stays silent on 0 -- the real
 * BATCH_SERVICE_START_ACK is deferred twice now: once for this ADD
 * round-trip (svc_proxy_add_ok/svc_proxy_add_fail), and again for
 * mbd_new_job_reply()'s RUNNING transition once the job exists.
 *
 * username/home_dir come straight off the wire (ws->username/home_dir,
 * filled client-side by llb_service_start(), same convention as
 * wire_job_submit) -- no getpwuid*() call here. A service has no
 * meaningful launch directory of its own, so cwd is just rooted at
 * home_dir rather than asking the client for a third field.
 */
int service_start_instance(const struct protocol_header *hdr, int chan_id,
                           const struct wire_svc_start *ws)
{
    struct service_data *svc = svc_find_by_name(ws->name);
    if (svc == NULL) {
        LL_ERRX("service_start_instance: service=%s not found", ws->name);
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
    snprintf(inst->svc_id, sizeof(inst->svc_id), "%d@%s", hdr->uid, ws->name);

    /* the synthesized command IS the whole job -- no user shell script
     * for a service, unlike an ordinary bsub. Stashed on the instance
     * (pend_cmd/pend_ws) since job_prepare() doesn't get called until
     * phase 2, once the port comes back from proxy. */
    int n = snprintf(inst->pend_cmd, sizeof(inst->pend_cmd),
                     "apptainer exec %s %s", svc->image, svc->command);
    if (n < 0 || n >= (int) sizeof(inst->pend_cmd)) {
        LL_ERRX("service_start_instance: command too long service=%s",
               ws->name);
        free(inst);
        return EINVAL;
    }

    memset(&inst->pend_ws, 0, sizeof(inst->pend_ws));
    ll_strlcpy(inst->pend_ws.name, inst->svc_id, sizeof(inst->pend_ws.name));
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
    inst->pend_ws.flags = JOB_FLAG_SERVICE;

    /* protocol_header saved so phase 2 can call job_prepare() with the
     * original requester's identity, same as it would have gotten
     * synchronously before this split -- and so a phase-2 failure can
     * still enqueue_header() the right client on the right chan_id. */
    inst->pend_hdr = *hdr;

    LL_DEBUG("service: uid=%u %s: cmd=[%s]", hdr->uid, inst->svc_id,
             inst->pend_cmd);

    ll_list_append(&svc->instances, &inst->ent);

    if (svc_proxy_send_add(inst) < 0) {
        LL_ERRX("service_start_instance: proxy ADD failed svc_id=%s",
                inst->svc_id);
        ll_list_remove(&svc->instances, &inst->ent);
        free(inst);
        return ENOTCONN;
    }

    LL_INFO("service_start_instance: service=%s svc_id=%s: ADD sent, "
           "awaiting port from proxy", ws->name, inst->svc_id);

    return 0;
}

/*
 * Phase 2, success path: service_proxy bound a real port and told us
 * so. Only now do we build the actual job -- job_prepare()/job_commit()
 * moved here wholesale from the old single-phase service_start_instance().
 *
 * Called from wherever mbd's net.c dispatches proxy-channel replies
 * (not yet written) once ADD_OK svc_id:<> port:<> is parsed off
 * service_proxy_chan_id.
 */
void svc_proxy_add_ok(const char *svc_id, int port)
{
    struct service_instance *inst = svc_find_instance_by_id(svc_id);
    if (inst == NULL) {
        LL_ERRX("svc_proxy_add_ok: unknown svc_id=%s (stale reply?)", svc_id);
        return;
    }

    inst->port = port;

    struct wire_job_script script;
    memset(&script, 0, sizeof(script));
    script.data = inst->pend_cmd;
    script.len = (uint32_t) strlen(inst->pend_cmd);

    int err = 0;
    struct job_data *job = job_prepare(&inst->pend_ws, &script, &inst->pend_hdr,
                                       &err);
    if (job == NULL) {
        LL_ERRX("svc_proxy_add_ok: job_prepare failed svc_id=%s err=%d",
                svc_id, err);
        /* Client is still blocked waiting on inst->chan_id -- reply now,
         * same op the eventual RUNNING-transition ack would have used. */
        enqueue_header(inst->chan_id, BATCH_SERVICE_START_ACK, err);
        /* TODO: tell proxy REMOVE svc_id -- it already bound the port,
         * mbd giving up here shouldn't leave it listening forever. */
        ll_list_remove(&inst->svc->instances, &inst->ent);
        free(inst);
        return;
    }

    job->svc_inst = inst;
    inst->job_id = job->job_id;

    job_commit(job, &inst->pend_ws);

    LL_INFO("svc_proxy_add_ok: svc_id=%s job_id=%ld port=%d", inst->svc_id,
           job->job_id, inst->port);

    /* Still no reply to the client -- BATCH_SERVICE_START_ACK remains
     * deferred until mbd_new_job_reply() sees this job reach RUNNING. */
}

/*
 * Phase 2, failure path: proxy couldn't bind a port (range exhausted,
 * or whatever reason it reports). Tear down the instance and tell the
 * waiting client now -- this is a genuinely new failure mode the old
 * single-phase code never had, since port allocation used to be
 * synchronous and any failure returned straight from
 * service_start_instance() through dispatch.c's existing error-reply
 * path. This one has to reply explicitly since dispatch.c already
 * returned 0 and went silent.
 */
void svc_proxy_add_fail(const char *svc_id, const char *reason)
{
    struct service_instance *inst = svc_find_instance_by_id(svc_id);
    if (inst == NULL) {
        LL_ERRX("svc_proxy_add_fail: unknown svc_id=%s (stale reply?)",
                svc_id);
        return;
    }

    LL_ERRX("svc_proxy_add_fail: svc_id=%s reason=%s", svc_id, reason);

    enqueue_header(inst->chan_id, BATCH_SERVICE_START_ACK, ENOSPC);

    ll_list_remove(&inst->svc->instances, &inst->ent);
    free(inst);
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
 * Send UPDATE svc_id:<> run_host:<> to service_proxy. Same status as
 * svc_proxy_send_add(): needs a real BATCH_SVC_UPDATE opcode + wire
 * struct in rpc.h/wire.h (same pattern as BATCH_SP_REGISTER, now that
 * svc_proto.h's plain-text approach is gone) once that grammar is
 * designed. Placeholder call site only.
 */
static int svc_proxy_send_update(struct service_instance *inst)
{
    if (service_proxy_chan_id < 0) {
        LL_ERRX("svc_proxy_send_update: service_proxy not connected svc_id=%s",
                inst->svc_id);
        return -1;
    }

    LL_DEBUG("service: svc_id=%s: UPDATE run_host=%s sent to proxy chan=%d",
             inst->svc_id, inst->run_host, service_proxy_chan_id);

    /* TODO: real BATCH_SVC_UPDATE send once designed. */

    return 0;
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

    /* best-effort: a proxy UPDATE failure shouldn't block telling the
     * client the service is up -- the URL is still correct, proxy just
     * needs to catch up on reconnect/RESYNC same as any other drop. */
    svc_proxy_send_update(inst);

    struct wire_svc_info info;
    memset(&info, 0, sizeof(info));
    ll_strlcpy(info.svc_id, inst->svc_id, sizeof(info.svc_id));
    ll_strlcpy(info.name, inst->svc->name, sizeof(info.name));
    info.uid = inst->pend_hdr.uid;
    info.port = inst->port;
    ll_strlcpy(info.run_host, inst->run_host, sizeof(info.run_host));
    info.job_id = job->job_id;
    /* ASSUMPTION: SVC_RUNNING is llbatch.h's name for this state, per
     * wire_svc_info's "state -- SVC_* from llbatch.h" comment. Haven't
     * seen llbatch.h -- verify the real name before this compiles. */
    info.state = SVC_RUNNING;

    struct protocol_header rep_hdr;
    init_protocol_header(&rep_hdr);
    rep_hdr.operation = BATCH_SERVICE_START_ACK;
    rep_hdr.status = MBD_OK;

    if (auth_sign_header(&rep_hdr) < 0) {
        LL_ERR("svc_job_running: auth_sign_header failed svc_id=%s",
               inst->svc_id);
        return;
    }

    size_t siz = sizeof(struct protocol_header) + sizeof(struct wire_svc_info)
                 + LL_BUFSIZ_64;

    if (enqueue_payload(inst->chan_id, &rep_hdr, &info, siz,
                        xdr_wire_svc_info) < 0) {
        LL_ERR("svc_job_running: enqueue_payload failed svc_id=%s",
               inst->svc_id);
        return;
    }

    LL_INFO("svc_job_running: svc_id=%s job_id=%ld run_host=%s port=%d "
           "RUNNING, client acked", inst->svc_id, job->job_id,
           inst->run_host, inst->port);
}

/*
 * Stub only. Real implementation still to come: instance lookup by
 * svc_id, ownership check, signal the backing job, REMOVE to
 * service_proxy (port is proxy's to free now, not mbd's).
 */
int service_stop_instance(uid_t uid, const char *svc_id)
{
    (void) uid;

    LL_ERRX("service_stop_instance: not implemented svc_id=%s", svc_id);
    return ENOSYS;
}

/*
 * spd connects in and registers, same accept path as sbd
 * (mbd_sbd_register() in sbd.c) but simpler: no per-job resync
 * payload, since service_proxy has no job state of its own -- the
 * real service registry resync (ADD per live instance) is a separate
 * exchange, not folded into this ack.
 */
int mbd_sp_register(XDR *xdrs, int chan_id)
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
               "(already on chan=%d), rejecting", hostname,
               service_proxy_chan_id);
        return -1;
    }

    service_proxy_chan_id = chan_id;
    LL_INFO("service_proxy registered host=%s chan=%d", hostname, chan_id);

    struct protocol_header hdr;
    init_protocol_header(&hdr);
    hdr.operation = BATCH_SP_REGISTER_ACK;
    hdr.status = MBD_OK;

    if (auth_sign_header(&hdr) < 0) {
        LL_ERR("auth_sign_header failed for service_proxy ack");
        service_proxy_chan_id = -1;
        return -1;
    }

    size_t siz = sizeof(struct protocol_header) + sizeof(struct wire_sp_register)
        + LL_BUFSIZ_64;

    if (enqueue_payload(chan_id, &hdr, &reg, siz, xdr_wire_sp_register) < 0) {
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
