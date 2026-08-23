/*
 * Copyright (C) LavaLite Contributors
 * GPL v2
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

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
 * ack handlers below to resolve an async reply back to the instance
 * that asked for it. Same linear-scan-over-service_list shape that
 * svc_port_in_use() used to be -- realistic instance counts are tens,
 * not thousands.
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
 * Send BATCH_SVC_ADD to service_proxy. spd is an ordinary connected
 * client of mbd, same as sbd -- this is chan_enqueue/chan_dequeue and
 * XDR over the existing service_proxy_chan_id, the same enqueue_payload()
 * path every other opcode in this file uses. No separate socket, no
 * text grammar.
 */
static int svc_proxy_send_add(struct service_instance *inst)
{
    if (service_proxy_chan_id < 0) {
        LL_ERRX("svc_proxy_send_add: service_proxy not connected svc_id=%s",
                inst->svc_id);
        return -1;
    }

    struct wire_svc_add req;
    memset(&req, 0, sizeof(req));
    ll_strlcpy(req.svc_id, inst->svc_id, sizeof(req.svc_id));

    struct protocol_header hdr;
    init_protocol_header(&hdr);
    hdr.operation = BATCH_SVC_ADD;
    hdr.status = MBD_OK;

    if (auth_sign_header(&hdr) < 0) {
        LL_ERR("svc_proxy_send_add: auth_sign_header failed svc_id=%s",
               inst->svc_id);
        return -1;
    }

    size_t siz = sizeof(struct protocol_header) + sizeof(struct wire_svc_add)
                 + LL_BUFSIZ_64;

    if (enqueue_payload(service_proxy_chan_id, &hdr, &req, siz,
                        xdr_wire_svc_add) < 0) {
        LL_ERR("svc_proxy_send_add: enqueue_payload failed svc_id=%s",
               inst->svc_id);
        return -1;
    }

    LL_DEBUG("service: svc_id=%s: ADD sent to proxy chan=%d", inst->svc_id,
             service_proxy_chan_id);

    return 0;
}

/*
 * Send BATCH_SVC_REMOVE to service_proxy. Used both when mbd gives up
 * on an instance after the proxy already bound a port (job_prepare()
 * failure in svc_proxy_add_ack() below) and from service_stop_instance()
 * once that's implemented -- either way the port is proxy's to free,
 * not mbd's.
 */
static int svc_proxy_send_remove(const char *svc_id)
{
    if (service_proxy_chan_id < 0) {
        LL_ERRX("svc_proxy_send_remove: service_proxy not connected svc_id=%s",
                svc_id);
        return -1;
    }

    struct wire_svc_remove req;
    memset(&req, 0, sizeof(req));
    ll_strlcpy(req.svc_id, svc_id, sizeof(req.svc_id));

    struct protocol_header hdr;
    init_protocol_header(&hdr);
    hdr.operation = BATCH_SVC_REMOVE;
    hdr.status = MBD_OK;

    if (auth_sign_header(&hdr) < 0) {
        LL_ERR("svc_proxy_send_remove: auth_sign_header failed svc_id=%s",
               svc_id);
        return -1;
    }

    size_t siz = sizeof(struct protocol_header) +
                 sizeof(struct wire_svc_remove) + LL_BUFSIZ_64;

    if (enqueue_payload(service_proxy_chan_id, &hdr, &req, siz,
                        xdr_wire_svc_remove) < 0) {
        LL_ERR("svc_proxy_send_remove: enqueue_payload failed svc_id=%s",
               svc_id);
        return -1;
    }

    LL_DEBUG("service: svc_id=%s: REMOVE sent to proxy chan=%d", svc_id,
             service_proxy_chan_id);

    return 0;
}

/*
 * Phase 1 of service start: validate, build the instance and the job
 * submission it will need, ask service_proxy for a port, return
 * without touching job_prepare()/job_commit() at all. The job only
 * gets created once svc_proxy_add_ack() (below) fires with the real
 * port -- mbd never allocates or probes a port itself anymore.
 *
 * No reply to the client here either way: dispatch.c's service_start()
 * already replies immediately on a non-zero return (synchronous
 * validation failure), and stays silent on 0 -- the real
 * BATCH_SERVICE_START_ACK is deferred twice now: once for this ADD
 * round-trip (svc_proxy_add_ack), and again for mbd_new_job_reply()'s
 * RUNNING transition once the job exists.
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
    snprintf(inst->svc_id, sizeof(inst->svc_id), "%u@%s", hdr->uid, ws->name);

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
 * Phase 2 of service start: resolves the BATCH_SVC_ADD round-trip.
 * hdr->status carries success/failure exactly like every other ack in
 * this protocol (MBD_OK or an errno) -- no separate OK/FAIL opcode
 * pair needed, and no reason string on the wire, same as
 * BATCH_SERVICE_START_ACK already does.
 *
 * On success, only now do we build the actual job -- job_prepare()/
 * job_commit() were split out of service_start_instance() for exactly
 * this: they can't run until the real port comes back from proxy.
 */
void svc_proxy_add_ack(XDR *xdrs, const struct protocol_header *hdr)
{
    struct wire_svc_add_ack ack;
    memset(&ack, 0, sizeof(ack));

    if (!xdr_wire_svc_add_ack(xdrs, &ack)) {
        LL_ERR("svc_proxy_add_ack: xdr decode failed");
        return;
    }

    struct service_instance *inst = svc_find_instance_by_id(ack.svc_id);
    if (inst == NULL) {
        LL_ERRX("svc_proxy_add_ack: unknown svc_id=%s (stale reply?)",
                ack.svc_id);
        return;
    }

    if (hdr->status != MBD_OK) {
        LL_ERRX("svc_proxy_add_ack: svc_id=%s failed status=%d", ack.svc_id,
                hdr->status);
        enqueue_header(inst->chan_id, BATCH_SERVICE_START_ACK, hdr->status);
        ll_list_remove(&inst->svc->instances, &inst->ent);
        free(inst);
        return;
    }

    inst->port = ack.port;

    struct wire_job_script script;
    memset(&script, 0, sizeof(script));
    script.data = inst->pend_cmd;
    script.len = (uint32_t) strlen(inst->pend_cmd);

    int err = 0;
    struct job_data *job = job_prepare(&inst->pend_ws, &script, &inst->pend_hdr,
                                       &err);
    if (job == NULL) {
        LL_ERRX("svc_proxy_add_ack: job_prepare failed svc_id=%s err=%d",
                inst->svc_id, err);
        /* Client is still blocked waiting on inst->chan_id -- reply now,
         * same op the eventual RUNNING-transition ack would have used.
         * proxy already bound the port on its side, so tell it to free
         * the port too -- mbd giving up here shouldn't leave it
         * listening forever. */
        enqueue_header(inst->chan_id, BATCH_SERVICE_START_ACK, err);
        svc_proxy_send_remove(inst->svc_id);
        ll_list_remove(&inst->svc->instances, &inst->ent);
        free(inst);
        return;
    }

    job->svc_inst = inst;
    inst->job_id = job->job_id;

    job_commit(job, &inst->pend_ws);

    LL_INFO("svc_proxy_add_ack: svc_id=%s job_id=%ld port=%d", inst->svc_id,
           job->job_id, inst->port);

    /* Still no reply to the client -- BATCH_SERVICE_START_ACK remains
     * deferred until mbd_new_job_reply() sees this job reach RUNNING. */
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
 * Send BATCH_SVC_UPDATE to service_proxy -- run_host changed (initial
 * dispatch, or redispatch after a restart landing elsewhere). Same
 * enqueue_payload() path as svc_proxy_send_add()/send_remove().
 */
static int svc_proxy_send_update(struct service_instance *inst)
{
    if (service_proxy_chan_id < 0) {
        LL_ERRX("svc_proxy_send_update: service_proxy not connected svc_id=%s",
                inst->svc_id);
        return -1;
    }

    struct wire_svc_update req;
    memset(&req, 0, sizeof(req));
    ll_strlcpy(req.svc_id, inst->svc_id, sizeof(req.svc_id));
    ll_strlcpy(req.run_host, inst->run_host, sizeof(req.run_host));

    struct protocol_header hdr;
    init_protocol_header(&hdr);
    hdr.operation = BATCH_SVC_UPDATE;
    hdr.status = MBD_OK;

    if (auth_sign_header(&hdr) < 0) {
        LL_ERR("svc_proxy_send_update: auth_sign_header failed svc_id=%s",
               inst->svc_id);
        return -1;
    }

    size_t siz = sizeof(struct protocol_header) +
                 sizeof(struct wire_svc_update) + LL_BUFSIZ_64;

    if (enqueue_payload(service_proxy_chan_id, &hdr, &req, siz,
                        xdr_wire_svc_update) < 0) {
        LL_ERR("svc_proxy_send_update: enqueue_payload failed svc_id=%s",
               inst->svc_id);
        return -1;
    }

    LL_DEBUG("service: svc_id=%s: UPDATE run_host=%s sent to proxy chan=%d",
             inst->svc_id, inst->run_host, service_proxy_chan_id);

    return 0;
}

/*
 * Correlation acks for BATCH_SVC_UPDATE/BATCH_SVC_REMOVE. Nothing on
 * mbd's side blocks waiting for these -- the client-facing state
 * (run_host, instance teardown) is already updated locally before the
 * request was even sent -- but we still decode and log them instead
 * of silently dropping, so a proxy-side failure (stale svc_id, whatever)
 * is visible instead of invisible.
 */
void svc_proxy_update_ack(XDR *xdrs, const struct protocol_header *hdr)
{
    struct wire_svc_update_ack ack;
    memset(&ack, 0, sizeof(ack));

    if (!xdr_wire_svc_update_ack(xdrs, &ack)) {
        LL_ERR("svc_proxy_update_ack: xdr decode failed");
        return;
    }

    if (hdr->status != MBD_OK)
        LL_ERRX("svc_proxy_update_ack: svc_id=%s failed status=%d",
                ack.svc_id, hdr->status);
    else
        LL_DEBUG("svc_proxy_update_ack: svc_id=%s ok", ack.svc_id);
}

void svc_proxy_remove_ack(XDR *xdrs, const struct protocol_header *hdr)
{
    struct wire_svc_remove_ack ack;
    memset(&ack, 0, sizeof(ack));

    if (!xdr_wire_svc_remove_ack(xdrs, &ack)) {
        LL_ERR("svc_proxy_remove_ack: xdr decode failed");
        return;
    }

    if (hdr->status != MBD_OK)
        LL_ERRX("svc_proxy_remove_ack: svc_id=%s failed status=%d",
                ack.svc_id, hdr->status);
    else
        LL_DEBUG("svc_proxy_remove_ack: svc_id=%s ok", ack.svc_id);
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
     * same request/ack shape as everything else on this channel. */
    svc_proxy_send_update(inst);

    struct wire_svc_info info;
    memset(&info, 0, sizeof(info));
    ll_strlcpy(info.svc_id, inst->svc_id, sizeof(info.svc_id));
    ll_strlcpy(info.name, inst->svc->name, sizeof(info.name));
    info.uid = inst->pend_hdr.uid;
    info.port = inst->port;
    ll_strlcpy(info.run_host, inst->run_host, sizeof(info.run_host));
    info.job_id = job->job_id;
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
