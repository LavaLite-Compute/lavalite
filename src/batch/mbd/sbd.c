/*
 * Copyright (C) LavaLite Contributors
 * GPL v2
 */

#include <string.h>
#include <syslog.h>
#include <errno.h>
#include <assert.h>
#include <sys/stat.h>

#include "base/lib/auth.h"
#include "base/lib/ll.host.h"
#include "base/lib/ll.bufsiz.h"
#include "base/lib/ll.syslog.h"
#include "base/lib/ll.hash.h"
#include "base/lib/ll.list.h"
#include "batch/lib/rpc.h"
#include "batch/mbd/mbd.h"

static int build_sbd_run_list(struct mbd_host *n,
                              struct wire_sbd_register *reg_ack)
{
    int count = 0;
    struct ll_list_entry *e;

    for (e = run_jobs_list.head; e != NULL; e = e->next) {
        struct job_data *job = (struct job_data *) e;
        if (job->run_hosts[0] != n)
            continue;
        count++;
    }

    LL_DEBUG("host=%s has count=%d number of jobs", n->net.name, count);
    if (count == 0)
        return 0;

    reg_ack->jobs = calloc(count, sizeof(struct wire_sbd_job));
    if (reg_ack->jobs == NULL) {
        LL_ERR("calloc failed count=%d", count);
        return -1;
    }

    int i = 0;
    for (e = run_jobs_list.head; e != NULL; e = e->next) {
        struct job_data *job = (struct job_data *) e;
        if (job->run_hosts[0] != n)
            continue;
        reg_ack->jobs[i].job_id = job->job_id;
        reg_ack->jobs[i].pid = (int32_t) job->pid;
        i++;
    }

    reg_ack->num_jobs = i;

    return 0;
}

int mbd_sbd_register(XDR *xdrs, int32_t chan_id)
{
    struct wire_sbd_register reg;
    memset(&reg, 0, sizeof(reg));

    if (!xdr_wire_sbd_register(xdrs, &reg)) {
        LL_ERR("SBD_REGISTER decode failed");
        xdr_free((xdrproc_t)xdr_wire_sbd_register, &reg);
        chan_shutdown(chan_id);
        return -1;
    }

    char hostname[MAXHOSTNAMELEN];
    memcpy(hostname, reg.hostname, sizeof(hostname));
    hostname[sizeof(hostname) - 1] = 0;

    /* done with wire data */
    xdr_free((xdrproc_t)xdr_wire_sbd_register, &reg);

    struct mbd_host *n = ll_hash_search(&host_name_hash, hostname);
    if (n == NULL) {
        LL_ERRX("register from unknown host %s", hostname);
        chan_shutdown(chan_id);
        return -1;
    }

    if (n->sbd_chan != -1) {
        LL_ERRX("duplicate sbd registration from host=%s "
                "(already on chan=%d), rejecting",
                hostname, n->sbd_chan);
        chan_shutdown(chan_id);
        return -1;
    }
    n->sbd_chan = chan_id;
    char key[LL_BUFSIZ_32];
    snprintf(key, sizeof(key), "%d", chan_id);
    ll_hash_insert(&sbd_chan_hash, key, n, 0);

    struct wire_sbd_register reg_ack;
    memset(&reg_ack, 0, sizeof(reg_ack));
    if (build_sbd_run_list(n, &reg_ack) < 0) {
        LL_ERRX("host=%s build_sbd_run_list failed", n->net.name);
        mbd_sbd_disconnect(n);
        return -1;
    }
    n->state = HOST_OK | (n->state & HOST_CLOSED);
    LL_INFO("hostname=%s canon=%s addr=%s chan_fd=%d state=%d",
            hostname, n->net.name, n->net.addr, chan_id, n->state);

    struct protocol_header hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.operation = BATCH_SBD_REGISTER_ACK;
    hdr.status = MBD_OK;

    if (auth_sign_header(&hdr) < 0) {
        LL_ERR("auth_sign_header failed");
        xdr_free((xdrproc_t)xdr_wire_sbd_register, &reg_ack);
        mbd_sbd_disconnect(n);
        return -1;
    }

    size_t siz = sizeof(struct protocol_header) + MAXHOSTNAMELEN +
                 sizeof(int32_t) +
                 reg_ack.num_jobs * sizeof(struct wire_sbd_job) + LL_BUFSIZ_64;

    enqueue_payload(chan_id, &hdr, &reg_ack, siz, xdr_wire_sbd_register);

    xdr_free((xdrproc_t)xdr_wire_sbd_register, &reg_ack);
    return 0;
}

int32_t mbd_sbd_route(struct mbd_host *n)
{
    int chan_id = n->sbd_chan;

    if (chan_has_error(chan_id)) {
        LL_DEBUG("channel=%d sbd from=%s closed connection", chan_id,
                 chan_addr_str(chan_id));
        mbd_sbd_disconnect(n);
        return -1;
    }

    struct chan_buffer *buf;
    if (chan_dequeue(chan_id, &buf) < 0) {
        LL_ERR("chan_dequeue failed chan_id=%d", chan_id);
        chan_shutdown(chan_id);
        return -1;
    }

    struct protocol_header hdr;
    XDR xdrs;
    xdrmem_create(&xdrs, buf->data, buf->len, XDR_DECODE);
    if (!xdr_pack_hdr(&xdrs, &hdr)) {
        LL_ERR("xdr_pack_hdr failed chan_id=%d", chan_id);
        xdr_destroy(&xdrs);
        chan_free_buf(buf);
        chan_shutdown(chan_id);
        return -1;
    }

    /* validate opcode and version early */
    if (!valid_batch_op(hdr.operation)) {
        LL_ERR("invalid opcode=%d from=%s", hdr.operation,
               chan_addr_str(chan_id));
        xdr_destroy(&xdrs);
        chan_free_buf(buf);
        chan_shutdown(chan_id);
        return -1;
    }

    if (auth_verify_header(&hdr) < 0) {
        LL_ERR("failed validate header opcode=%s from=%s",
               batch_op_str(hdr.operation), chan_addr_str(chan_id));
        xdr_destroy(&xdrs);
        chan_free_buf(buf);
        chan_shutdown(chan_id);
        return -1;
    }

    if (hdr.version != CURRENT_PROTOCOL_VERSION) {
        LL_ERR("unsupported version=0x%x from=%s", hdr.version,
               chan_addr_str(chan_id));
        xdr_destroy(&xdrs);
        chan_free_buf(buf);
        chan_shutdown(chan_id);
        return -1;
    }

    switch (hdr.operation) {
    case BATCH_NEW_JOB_REPLY:
        mbd_new_job_reply(n, &xdrs, &hdr);
        break;
    case BATCH_JOB_FINISH:
        mbd_job_finish(n, &xdrs);
        break;
    case BATCH_SBD_JOB_SIGNAL_REPLY:
        mbd_job_signal_reply(n, &xdrs, &hdr);
        break;
    case BATCH_JOB_ORPHAN:
        mbd_job_orphan(n, &xdrs);
        break;
    }

    xdr_destroy(&xdrs);
    chan_free_buf(buf);

    return 0;
}

int mbd_sbd_disconnect(struct mbd_host *n)
{
    int chan_id = n->sbd_chan;

    LL_INFO("closing connection with on chan=%d host=%s addr=%s", chan_id,
            n->net.name, n->net.addr);

    n->sbd_chan = -1;
    n->state = HOST_UNAVAIL | (n->state & HOST_CLOSED);

    char key[LL_BUFSIZ_32];
    snprintf(key, sizeof(key), "%d", chan_id);
    ll_hash_remove(&sbd_chan_hash, key);
    chan_shutdown(chan_id);

    return 0;
}

static int read_sidecar(const struct job_data *job, struct wire_job_start *ws)
{
    char path[PATH_MAX];
    int n;

    n = snprintf(path, sizeof(path), "%s/%ld/%ld/submit", jobs_dir,
                 (job->job_id % JOB_BUCKETS), job->job_id);
    if (n < 0 || n >= (int) sizeof(path))
        return -1;

    FILE *fp = fopen(path, "r");
    if (fp == NULL) {
        LL_ERR("fopen=%s failed", path);
        return -1;
    }

    char line[PATH_MAX + 16];
    while (fgets(line, sizeof(line), fp) != NULL) {
        char *nl = strchr(line, '\n');
        if (nl)
            *nl = 0;

        if (strncmp(line, "IN_FILE=", 8) == 0) {
            ll_strlcpy(ws->in_file, line + 8, sizeof(ws->in_file));
            continue;
        }
        if (strncmp(line, "OUT_FILE=", 9) == 0) {
            ll_strlcpy(ws->out_file, line + 9, sizeof(ws->out_file));
            continue;
        }
        if (strncmp(line, "ERR_FILE=", 9) == 0) {
            ll_strlcpy(ws->err_file, line + 9, sizeof(ws->err_file));
            continue;
        }
        if (strncmp(line, "HOME_DIR=", 9) == 0) {
            ll_strlcpy(ws->home_dir, line + 9, sizeof(ws->home_dir));
            continue;
        }
        if (strncmp(line, "UMASK=", 6) == 0) {
            int u;
            ll_atoi(line + 6, &u);
            ws->umask = (uint32_t) u;
            continue;
        }
        if (strncmp(line, "CWD=", 4) == 0) {
            ll_strlcpy(ws->cwd, line + 4, sizeof(ws->cwd));
            continue;
        }
        if (strncmp(line, "COMMAND=", 8) == 0) {
            ll_strlcpy(ws->command, line + 8, sizeof(ws->command));
            continue;
        }
    }

    fclose(fp);
    return 0;
}

/*
 * Read the job script into ws->script.
 * Caller must free ws->script.data on success.
 * Returns 0 on success, -1 on error.
 */
static int read_script(const struct job_data *job,
                       struct wire_job_script *script)
{
    char path[PATH_MAX];
    int n;

    n = snprintf(path, sizeof(path), "%s/%ld/%ld/script.sh", jobs_dir,
                 (job->job_id % JOB_BUCKETS), job->job_id);
    if (n < 0 || n >= (int) sizeof(path))
        return -1;

    struct stat st;
    if (stat(path, &st) < 0) {
        LL_ERR("stat=%s failed", path);
        return -1;
    }

    if (st.st_size == 0) {
        LL_ERRX("job=%ld script is empty", job->job_id);
        return -1;
    }

    script->data = malloc((size_t) st.st_size + 1);
    if (script->data == NULL) {
        LL_ERR("malloc failed size=%ld", (long) st.st_size);
        return -1;
    }

    FILE *fp = fopen(path, "r");
    if (fp == NULL) {
        LL_ERR("fopen=%s failed", path);
        free(script->data);
        script->data = NULL;
        return -1;
    }

    size_t nr = fread(script->data, 1, (size_t) st.st_size, fp);
    fclose(fp);

    if (nr != (size_t) st.st_size) {
        LL_ERR("job=%ld script read short %zu/%ld", job->job_id, nr,
               (long) st.st_size);
        free(script->data);
        script->data = NULL;
        return -1;
    }

    script->data[nr] = 0;
    script->len = (uint32_t) nr;

    return 0;
}

/*
 * Build the hosts string "host:cpus,host:cpus" from the sched plan.
 */
static void build_hosts_str(const struct job_data *job, char *buf,
                            size_t bufsiz)
{
    buf[0] = 0;
    for (int i = 0; i < job->run_nhosts; i++) {
        char entry[MAXHOSTNAMELEN + 16];
        snprintf(entry, sizeof(entry), "%s%s:%d", i > 0 ? "," : "",
                 job->run_hosts[i]->net.name, job->res.num_cpus);
        ll_strlcat(buf, entry, bufsiz);
    }
}

static void build_gpu_assigned_str(struct mbd_gpu *g, int num_gpus,
                                   char *buf, size_t bufsz)
{
    int assigned = 0;
    int i;

    buf[0] = 0;
    assert(g->count >= num_gpus);

    for (i = 0; i < g->count; i++) {
        if (g->ids[i].in_use)
            continue;
        /* Mark in use when building the sbd data buffer.
         * The ids must not be udpated again in host_update_resource
         */
        g->ids[i].in_use = 1;
        if (assigned > 0)
            ll_strlcat(buf, ",", bufsz);
        char entry[16];
        snprintf(entry, sizeof(entry), "%d", g->ids[i].id);
        ll_strlcat(buf, entry, bufsz);
        assigned++;
        if (assigned == num_gpus)
            break;
    }
}

int mbd_dispatch_job(struct job_data *job)
{
    struct mbd_host *h = job->run_hosts[0];

    assert(h->sbd_chan > 0);
    if (h->sbd_chan < 0) {
        LL_ERRX("job=%ld exec_host=%s sbd not connected", job->job_id,
                h->net.name);
        return -1;
    }

    struct wire_job_start ws;
    memset(&ws, 0, sizeof(ws));

    /* read file redirections from sidecar */
    if (read_sidecar(job, &ws) < 0) {
        LL_ERRX("job=%ld read_sidecar failed", job->job_id);
        abort();
        return -1;
    }

    /* read script from disk into ws.script */
    if (read_script(job, &ws.script) < 0) {
        LL_ERRX("job=%ld read_script failed", job->job_id);
        abort();
        return -1;
    }

    /* fill wire_job_start from job_data and sched_plan */
    ws.job_id = job->job_id;
    ws.uid = job->uid;
    ws.gid = job->gid;
    ws.term_time = (int64_t) job->term_time;
    ws.gpus_per_host = job->res.num_gpus;
    ws.ncpus = job->res.num_cpus;
    ws.mem_mb = job->res.mem_mb;

    ll_strlcpy(ws.job_name, job->name, sizeof(ws.job_name));
    ll_strlcpy(ws.queue, job->queue->name, sizeof(ws.queue));
    ll_strlcpy(ws.username, job->user, sizeof(ws.username));
    ll_strlcpy(ws.gpu_type, job->res.gpu_type, sizeof(ws.gpu_type));

    build_hosts_str(job, ws.hosts, sizeof(ws.hosts));

    if (job->res.num_gpus > 0) {
        struct mbd_gpu *g = &h->res.gpu;
        build_gpu_assigned_str(g, job->res.num_gpus,
                               ws.gpu_assigned, sizeof(ws.gpu_assigned));
        ll_strlcpy(job->gpu_assigned, ws.gpu_assigned, sizeof(job->gpu_assigned));
    }

    struct protocol_header hdr;
    init_protocol_header(&hdr);
    hdr.operation = BATCH_NEW_JOB;
    hdr.status = MBD_OK;

    if (auth_sign_header(&hdr) < 0) {
        LL_ERRX("job=%ld auth_sign_header failed", job->job_id);
        free(ws.script.data);
        return -1;
    }

    /* buffer size: fixed struct + script payload + XDR overhead */
    size_t bufsz = PACKET_HEADER_SIZE + sizeof(struct wire_job_start) +
                   ws.script.len + LL_BUFSIZ_64;

    if (enqueue_payload(h->sbd_chan, &hdr, &ws, bufsz,
                        (bool_t(*)()) xdr_wire_job_start) < 0) {
        LL_ERRX("job=%ld enqueue_payload failed", job->job_id);
        free(ws.script.data);
        return -1;
    }

    free(ws.script.data);

    job->dispatch_time = time(NULL);
    job->state = JOB_RUNNING;

    event_job_start(job);

    job_move_list(job, &pend_jobs_list, &run_jobs_list, JOB_LIST_RUN);

    LL_INFO("job=%ld dispatched to host=%s", job->job_id, h->net.name);

    return 0;
}

void mbd_job_orphan(struct mbd_host *n, XDR *xdrs)
{
    struct wire_job_state s;

    memset(&s, 0, sizeof(s));
    if (!xdr_wire_job_state(xdrs, &s)) {
        LL_ERR("xdr_wire_job_state decode failed from=%s",
               chan_addr_str(n->sbd_chan));
        return;
    }

    struct job_data *job = job_find(s.job_id);
    if (job == NULL) {
        LL_ERRX("cannot find job=%ld", s.job_id);
        return;
    }

    // Add the guard for the state
    if (job->state == JOB_ORPHAN) {
        LL_INFO("job=%ld already orphan", job->job_id);
        return;
    }
    if (job->run_nhosts <= 0 || job->run_hosts[0] != n) {
        LL_ERRX("job=%ld orphan report from wrong host=%s", s.job_id, n->net.name);
        return;
    }

    if (job->state != JOB_RUNNING && job->state != JOB_SUSPENDED) {
        LL_ERRX("job=%ld state=%s cannot become orphan",
                job->job_id, job_state_str(job->state));
        return;
    }

    // Free host resources
    reset_host_resources(job);
    token_free(job);

    // Update the queue counters before resetting the job state
    if (job->state == JOB_RUNNING)
        job->queue->num_run--;

    if (job->state == JOB_SUSPENDED)
        job->queue->num_susp--;

    job->queue->num_cpus_used -= job->res.num_cpus * job->run_nhosts;
    job->queue->num_hosts_used -= job->run_nhosts;
    job->queue->num_jobs--;

    LL_INFO("job=%ld reported as orphan by sbd=%s", s.job_id, n->net.name);
    job->state = JOB_ORPHAN;
}
