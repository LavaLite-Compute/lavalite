/*
 * Copyright (C) LavaLite Contributors
 * GPL v2
 */

#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <syslog.h>
#include <assert.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>

#include "base/lib/auth.h"
#include "base/lib/ll.syslog.h"
#include "base/lib/ll.conf.h"
#include "base/lib/ll.list.h"
#include "base/lib/ll.hash.h"
#include "batch/mbd/mbd.h"
#include "batch/lib/wire.h"
#include "batch/lib/log.h"

struct ll_list pend_jobs_list;
struct ll_list run_jobs_list;
struct ll_list finish_jobs_list;

int64_t job_id_seq = 0;
struct ll_hash job_id_hash;
int assert_counters = 0;

static int64_t next_job_id(void)
{
    do {
        job_id_seq++;
    } while (job_find(job_id_seq) != NULL);

    return job_id_seq;
}

void job_free(struct job_data *job)
{
    free(job->run_hosts);
    ll_hash_clear(&job->res.machines, NULL);
    free(job);
}

static int queue_user_allowed(const struct mbd_queue *q, const char *user)
{
    // 0 means all... sigh
    if (q->user_hash.nentries == 0)
        return 1;

    return ll_hash_contains(&q->user_hash, user);
}

static struct job_data *job_alloc(struct wire_job_submit *ws)
{
    struct job_data *job = calloc(1, sizeof(struct job_data));
    if (job == NULL) {
        LL_ERR("calloc failed");
        return NULL;
    }

    job->job_id = next_job_id();
    job->priority = 0;
    job->flags = ws->flags;
    job->state = JOB_PENDING;
    if (job->flags & JOB_FLAG_HOLD)
        job->state = JOB_HELD;
    job->submit_time = time(NULL);
    ll_strlcpy(job->user, ws->username, sizeof(job->user));
    job->begin_time = (time_t) ws->begin_time;
    job->term_time = (time_t) ws->term_time;
    ll_strlcpy(job->project, ws->project, sizeof(job->project));

    ll_strlcpy(job->res.gpu_type, ws->gpu_type, sizeof(job->res.gpu_type));
    job->res.num_cpus = ws->num_cpus;
    job->res.num_hosts = ws->num_hosts;
    job->res.num_gpus = ws->num_gpus;
    job->res.mem_mb = ws->mem_mb;
    job->res.storage_mb = ws->storage_mb;
    machines_hash_populate(&job->res.machines, ws->machines);
    ll_strlcpy(job->res.machines_str, ws->machines,
               sizeof(job->res.machines_str));

    if (ws->name[0] == 0) {
        ll_strlcpy(job->name, "-", sizeof(job->name));
        ll_strlcpy(ws->name, "-", sizeof(ws->name));
    } else {
        ll_strlcpy(job->name, ws->name, sizeof(job->name));
    }

    char *queue;
    if (ws->queue[0] == 0) {
        queue = ll_params[LL_DEFAULT_QUEUE].val;
        ll_strlcpy(ws->queue, queue, sizeof(ws->queue));
    } else {
        queue = ws->queue;
    }

    job->queue = ll_hash_search(&queue_name_hash, queue);
    if (job->queue == NULL) {
        LL_ERRX("queue='%s' not found", queue);
        free(job);
        return NULL;
    }
    job->priority = job->queue->priority;

    if (!queue_user_allowed(job->queue, job->user)) {
        LL_ERRX("job=%ld user=%s not allowed in queue=%s",
                job->job_id, job->user, job->queue->name);
        free(job);
        return NULL;
    }

    if (job->res.num_hosts < 1) {
        LL_WARNING("job=%ld num_hosts set to 1", job->job_id);
        job->res.num_hosts = 1;
    }

    job->run_hosts = calloc(job->res.num_hosts, sizeof(struct mbd_host *));
    if (job->run_hosts == NULL) {
        LL_ERR("calloc failed");
        free(job);
        return NULL;
    }
    return job;
}

void machines_hash_populate(struct ll_hash *h, const char *machines)
{
    char buf[LL_BUFSIZ_4K];

    ll_hash_init(h, 101);
    if (machines[0] == 0)
        return;

    ll_strlcpy(buf, machines, sizeof(buf));
    char *tok = strtok(buf, " \t,");
    while (tok) {
        ll_hash_insert(h, tok, NULL, 0);
        tok = strtok(NULL, " \t,");
    }
}

static int write_script(const struct job_data *job,
                        const struct wire_job_script *script)
{
    char dir[PATH_MAX];
    int n = snprintf(dir, sizeof(dir), "%s/%ld/%ld", jobs_dir,
                     (job->job_id % JOB_BUCKETS), job->job_id);
    if (n < 0 || n >= (int) sizeof(dir))
        return -1;

    if (mkdir(dir, 0755) < 0 && errno != EEXIST) {
        LL_ERR("mkdir=%s", dir);
        return -1;
    }

    char path[PATH_MAX];
    n = snprintf(path, sizeof(path), "%s/%ld/%ld/script.sh", jobs_dir,
                 (job->job_id % JOB_BUCKETS), job->job_id);
    if (n < 0 || n >= (int) sizeof(path))
        return -1;

    char tmp[PATH_MAX];
    n = snprintf(tmp, sizeof(tmp), "%s/%ld/%ld/script.sh.tmp", jobs_dir,
                 (job->job_id % JOB_BUCKETS), job->job_id);
    if (n < 0 || n >= (int) sizeof(tmp))
        return -1;

    int fd = open(tmp, O_WRONLY | O_CREAT | O_TRUNC, 0700);
    if (fd < 0) {
        LL_ERR("open %s", tmp);
        return -1;
    }

    ssize_t nw = write(fd, script->data, script->len);
    if (nw < 0 || (uint32_t) nw != script->len) {
        LL_ERR("write %s", tmp);
        close(fd);
        return -1;
    }

    if (fsync(fd) < 0) {
        LL_ERR("fsync %s", tmp);
        close(fd);
        return -1;
    }
    close(fd);

    if (rename(tmp, path) < 0) {
        LL_ERR("rename %s -> %s", tmp, path);
        return -1;
    }

    return 0;
}

static int write_sidecar(const struct job_data *job,
                         const struct wire_job_submit *ws)
{
    char path[PATH_MAX];
    int n = snprintf(path, sizeof(path), "%s/%ld/%ld/submit", jobs_dir,
                     (job->job_id % JOB_BUCKETS), job->job_id);
    if (n < 0 || n >= (int) sizeof(path))
        return -1;

    char tmp[PATH_MAX];
    n = snprintf(tmp, sizeof(tmp), "%s/%ld/%ld/submit.tmp", jobs_dir,
                 (job->job_id % JOB_BUCKETS), job->job_id);
    if (n < 0 || n >= (int) sizeof(tmp))
        return -1;

    FILE *fp = fopen(tmp, "w");
    if (fp == NULL) {
        LL_ERR("fopen %s: %m", tmp);
        return -1;
    }

    fprintf(fp, "JOB_ID=%ld\n", job->job_id);
    fprintf(fp, "UID=%u\n", job->uid);
    fprintf(fp, "GID=%u\n", job->gid);
    fprintf(fp, "USERNAME=%s\n", ws->username);
    fprintf(fp, "NAME=%s\n", ws->name);
    fprintf(fp, "QUEUE=%s\n", ws->queue);
    fprintf(fp, "PROJECT=%s\n", ws->project);
    fprintf(fp, "FROM_HOST=%s\n", ws->from_host);
    fprintf(fp, "HOME_DIR=%s\n", ws->home_dir);
    fprintf(fp, "CWD=%s\n", ws->cwd);
    fprintf(fp, "COMMAND=%s\n", ws->command);
    fprintf(fp, "IN_FILE=%s\n", ws->in_file);
    fprintf(fp, "OUT_FILE=%s\n", ws->out_file);
    fprintf(fp, "ERR_FILE=%s\n", ws->err_file);
    fprintf(fp, "NUM_CPUS=%d\n", ws->num_cpus);
    fprintf(fp, "NUM_NHOSTS=%d\n", ws->num_hosts);
    fprintf(fp, "NUM_GPUS=%d\n", ws->num_gpus);
    fprintf(fp, "MEM_MB=%lu\n", ws->mem_mb);
    fprintf(fp, "UMASK=%o\n", ws->umask);
    fprintf(fp, "FLAGS=%u\n", ws->flags);
    fprintf(fp, "BEGIN_TIME=%ld\n", ws->begin_time);
    fprintf(fp, "TERM_TIME=%ld\n", ws->term_time);
    fprintf(fp, "DEPEND_COND=%s\n", ws->depend_cond);
    fprintf(fp, "MACHINES=%s\n", ws->machines);
    fprintf(fp, "GPU_TYPE=%s\n", ws->gpu_type);
    fprintf(fp, "COMMENT=%s\n", ws->comment);
    fprintf(fp, "TOKENPOOL=%s\n", ws->tokenpool);

    if (fflush(fp) != 0 || ferror(fp)) {
        LL_ERR("write error %s: %m", tmp);
        fclose(fp);
        return -1;
    }
    fclose(fp);

    if (rename(tmp, path) < 0) {
        LL_ERR("rename %s -> %s: %m", tmp, path);
        return -1;
    }

    if (chmod(path, 0644) < 0) {
        LL_ERR("job=%ld chmod=%s to 644 failed", job->job_id, path);
        return -1;
    }

    return 0;
}

static int job_uses_host(struct job_data *job, struct mbd_host *h)
{
    for (int i = 0; i < job->run_nhosts; i++) {
        if (job->run_hosts[i] == h)
            return 1;
    }

    return 0;
}

static int job_parse_tokens(struct job_data *job, const char *tokenpool)
{
    if (tokenpool[0] == 0)
        return 0;

    char buf[LL_BUFSIZ_1K];
    ll_strlcpy(buf, tokenpool, sizeof(buf));

    char *tok = strtok(buf, ",");
    while (tok != NULL) {
        char *eq = strchr(tok, '=');
        if (eq == NULL) {
            LL_ERRX("job=%ld invalid token spec=%s", job->job_id, tok);
            return -1;
        }
        *eq = 0;
        int count;
        if (ll_atoi(eq + 1, &count) < 0 || count <= 0) {
            LL_ERRX("job=%ld invalid token count=%s", job->job_id, eq + 1);
            return -1;
        }
        struct mbd_token_pool *p;
        p = ll_hash_search(&token_pool_name_hash, tok);
        if (p == NULL) {
            LL_ERRX("job=%ld token pool=%s not found", job->job_id, tok);
            return -1;
        }
        struct job_token *t = calloc(1, sizeof(*t));
        if (t == NULL) {
            LL_ERR("calloc job_token failed");
            return -1;
        }
        ll_strlcpy(t->name, tok, sizeof(t->name));
        t->count = count;
        ll_list_append(&job->res.tokens, &t->ent);
        tok = strtok(NULL, ",");
    }
    return 0;
}

static int job_write_usage(const struct job_data *job,
                           const struct wire_job_finish *s)
{
    char dir[PATH_MAX + LL_BUFSIZ_32];
    int n = snprintf(dir, sizeof(dir), "%s/%ld/%ld", jobs_dir,
                     (job->job_id % JOB_BUCKETS), job->job_id);
    if (n < 0 || n >= (int) sizeof(dir))
        return -1;

    char path[PATH_MAX + LL_BUFSIZ_64];
    snprintf(path, sizeof(path), "%s/usage", dir);

    FILE *f = fopen(path, "w");
    if (f == NULL) {
        LL_ERR("job=%ld open usage failed: %m", job->job_id);
        return -1;
    }
    fprintf(f, "pid=%d\n", s->pid);
    fprintf(f, "mem_mb=%lu\n", s->mem_mb);
    fprintf(f, "swap_mb=%lu\n", s->swap_mb);
    fprintf(f, "cpu_time=%.2f\n", s->cpu_time);
    fclose(f);

    LL_INFO("job=%ld usage written", job->job_id);
    return 0;
}

static void reset_host_resources(struct job_data *job)
{
    for (int i = 0; i < job->run_nhosts; i++) {
        struct mbd_host *h = job->run_hosts[i];

        h->res.free_cpu += job->res.num_cpus;
        h->res.free_mem_mb += job->res.mem_mb;
        h->res.free_storage_mb += job->res.storage_mb;
        h->num_jobs--;
        h->num_cpus_used -= job->res.num_cpus;

        if (job->state == JOB_RUNNING)
            h->num_run--;

        if (job->state == JOB_SUSPENDED)
            h->num_susp--;

        if (job->flags & JOB_FLAG_EXCLUSIVE)
            h->exclusive = 0;

        if (job->res.num_gpus > 0) {
            h->res.free_gpu += job->res.num_gpus;
        }

        if (job->res.gpu_type[0] != 0) {
            assert(job->res.num_gpus > 0);
            struct mbd_gpu *g =
                ll_hash_search(&h->res.gpu_type_hash, job->res.gpu_type);
            if (g == NULL) {
                LL_ERRX("job=%ld host=%s gpu_type=%s not found", job->job_id,
                        h->net.name, job->res.gpu_type);
                assert(0);
                continue;
            }
            g->free += job->res.num_gpus;
        }

        LL_DEBUG("host=%s free_cpu=%d free_mem_mb=%lu free_storage_mb=%lu "
                 "free_gpu=%d num_jobs=%d",
                 h->net.name, h->res.free_cpu, h->res.free_mem_mb,
                 h->res.free_storage_mb, h->res.free_gpu, h->num_jobs);
    }
}

// Undo the optimistic dispatch.
static void mbd_job_reject_dispatch(struct job_data *job)
{
    assert(job->state == JOB_RUNNING);
    assert(job->list_id == JOB_LIST_RUN);

    struct mbd_host *h = job->run_hosts[0];
    int sbd_chan = h->sbd_chan;

    LL_ERR("job=%ld rejected by sbd=%s, returning to pending",
           job->job_id, chan_addr_str(sbd_chan));

    reset_host_resources(job);
    token_free(job);

    if (job->state == JOB_RUNNING)
        job->queue->num_run--;
    else
        job->queue->num_susp--;

    job->queue->num_pend++;
    job->queue->num_cpus_used -= job->res.num_cpus * job->run_nhosts;
    job->queue->num_hosts_used -= job->run_nhosts;

    job->pid = 0;
    job->fork_time = 0;
    job->dispatch_time = 0;
    job->state = JOB_PENDING;
    job->pend_reason = PEND_NONE;

    memset(job->run_hosts, 0, job->res.num_hosts * sizeof(job->run_hosts[0]));
    job->run_nhosts = 0;

    job_move_list(job, &run_jobs_list, &pend_jobs_list, JOB_LIST_PEND);

    LL_INFO("job=%ld back to pending", job->job_id);

    mbd_assert_counters();

    event_job_pend(job);

    LL_INFO("job=%ld returned to pending after sbd reject from=%s",
            job->job_id, chan_addr_str(sbd_chan));

    /* Now ack the new job reply
     */
    struct wire_job_ack ack;
    memset(&ack, 0, sizeof(ack));
    ack.job_id = job->job_id;
    ack.ack_op = BATCH_NEW_JOB_REPLY;

    struct protocol_header hdr;
    init_protocol_header(&hdr);
    hdr.operation = BATCH_NEW_JOB_REPLY_ACK;
    hdr.status = MBD_OK;

    if (auth_sign_header(&hdr) < 0) {
        LL_ERR("job=%ld failed to sign header for host=%s",
               job->job_id, h->net.name);
        return;
    }

    if (enqueue_payload(sbd_chan, &hdr, &ack, LL_BUFSIZ_1K,
                        xdr_wire_job_ack) < 0) {
        LL_ERR("job=%ld enqueue_payload failed", job->job_id);
        return;
    }
}

int job_register(XDR *xdrs, int chan_id, const struct protocol_header *hdr)
{
    struct wire_job_submit ws;
    memset(&ws, 0, sizeof(ws));
    if (!xdr_wire_job_submit(xdrs, &ws)) {
        LL_ERRX("xdr_wire_job_submit failed");
        return -1;
    }

    struct wire_job_script script;
    memset(&script, 0, sizeof(script));
    if (!xdr_wire_job_script(xdrs, &script)) {
        LL_ERRX("xdr_wire_job_script failed");
        return -1;
    }

    struct job_data *job = job_alloc(&ws);
    if (job == NULL) {
        LL_ERR("job_alloc failed");
        free(script.data);
        return -1;
    }
    // read from the HAMC header
    job->uid = (uid_t)hdr->uid;
    job->gid = (gid_t)hdr->gid;

    if (write_script(job, &script) < 0) {
        LL_ERR("write_script failed job_id=%ld", job->job_id);
        free(script.data);
        job_free(job);
        return -1;
    }
    free(script.data);

    if (write_sidecar(job, &ws) < 0) {
        LL_ERR("write_sidecar failed job_id=%ld", job->job_id);
        job_free(job);
        return -1;
    }

    if (job_parse_tokens(job, ws.tokenpool) < 0) {
        LL_ERRX("job=%ld invalid token pool spec", job->job_id);
        /* free job and return error */
        return -1;
    }
    ll_strlcpy(job->res.tokenpool_str, ws.tokenpool,
               sizeof(job->res.tokenpool_str));

    char key[LL_BUFSIZ_32];
    sprintf(key, "%ld", job->job_id);
    enum ll_hash_status hs = ll_hash_insert(&job_id_hash, key, job, 0);
    assert(hs == LL_HASH_INSERTED);

    job_set_list(job, &pend_jobs_list, JOB_LIST_PEND);

    struct wire_job_submit_reply reply;
    memset(&reply, 0, sizeof(reply));
    reply.job_id = job->job_id;

    struct protocol_header rep_hdr;
    init_protocol_header(&rep_hdr);
    rep_hdr.operation = BATCH_JOB_SUBMIT_ACK;
    rep_hdr.status = MBD_OK;

    size_t siz = PACKET_HEADER_SIZE + sizeof(struct wire_job_submit_reply) +
                 LL_BUFSIZ_64;

    if (enqueue_payload(chan_id, &rep_hdr, &reply,
                        siz, xdr_wire_job_submit_reply) < 0) {
        LL_ERR("enqueue_payload failed job_id=%ld", job->job_id);
        ll_list_remove(&pend_jobs_list, &job->ent);
        ll_hash_remove(&job_id_hash, key);
        job_free(job);
        return -1;
    }

    event_job_new(job, &ws);
    job_id_seq_write();  /* persist before ack -- seq must never go backwards */
    if (job->state == JOB_PENDING)
        job->queue->num_pend++;
    else if (job->state == JOB_HELD)
        job->queue->num_held++;

    job->queue->num_jobs++;

    LL_INFO("job_id=%ld user=%s queue=%s num_jobs=%d num_pend=%d", job->job_id,
            job->user, job->queue->name, job->queue->num_jobs,
            job->queue->num_pend);

    return 0;
}

/*
 * job_set_list - append job to list and record which list it is on.
 * Always use this instead of bare ll_list_append for job lists.
 */
void job_set_list(struct job_data *job, struct ll_list *list,
                  enum job_list_id list_id)
{
    ll_list_append(list, &job->ent);
    job->list_id = list_id;
}

/*
 * job_move_list - move job from one list to another atomically.
 * Always use this instead of bare ll_list_remove + ll_list_append.
 */
void job_move_list(struct job_data *job, struct ll_list *from,
                   struct ll_list *to, enum job_list_id list_id)
{
    ll_list_remove(from, &job->ent);
    ll_list_append(to, &job->ent);
    job->list_id = list_id;
}

struct job_data *job_find(int64_t job_id)
{
    char buf[LL_BUFSIZ_32];
    sprintf(buf, "%ld", job_id);
    return ll_hash_search(&job_id_hash, buf);
}

int job_init(void)
{
    ll_hash_init(&job_id_hash, 1021);
    ll_list_init(&pend_jobs_list);
    ll_list_init(&run_jobs_list);
    ll_list_init(&finish_jobs_list);

    LL_INFO("job structures initialized");

    int nj = jobs_replay();
    LL_INFO("num=%d jobs replayed", nj);

    assert_counters = 0;
    if (! ll_atoi(ll_params[LL_ASSERT_COUNTERS].val, &assert_counters)) {
        LL_ERRX("failed set assert_counters");
        assert_counters = 0;
    }
    LL_DEBUG("mbd asserting counters assert_counters=%d", assert_counters);

    return 0;
}

void mbd_new_job_reply(struct mbd_host *n, XDR *xdrs,
                       struct protocol_header *hdr)
{
    struct wire_job_reply r;
    memset(&r, 0, sizeof(r));
    if (!xdr_wire_job_reply(xdrs, &r)) {
        LL_ERR("xdr_wire_job_reply decode failed from=%s",
               chan_addr_str(n->sbd_chan));
        return;
    }

    struct job_data *job = job_find(r.job_id);
    if (job == NULL) {
        LL_ERR("job=%ld not found from=%s - admin intervention required",
               r.job_id, chan_addr_str(n->sbd_chan));
        return;
    }

    // Something went really wrong
    if (r.state == JOB_PENDING) {

        if (job->state == JOB_PENDING || job->state == JOB_HELD) {
            assert(job->list_id == JOB_LIST_PEND);
            LL_INFO("job=%ld duplicated event received", r.job_id);
            return;
        }

        LL_ERR("job=%ld rejected by sbd=%s status=%d (%s)",
               r.job_id, chan_addr_str(n->sbd_chan),
               hdr->status, strerror(hdr->status));

        mbd_job_reject_dispatch(job);
        return;
    }

    int duplicate = 0;
    /* duplicate: skip event log, sbd resends if it restarts before ack
     */
    if (job->fork_time > 0) {
        LL_INFO("job=%ld fork duplicate from=%s", r.job_id,
                chan_addr_str(n->sbd_chan));
        duplicate = 1;
        // fall through
    }

    struct wire_job_ack ack;
    memset(&ack, 0, sizeof(ack));
    ack.job_id = r.job_id;
    ack.ack_op = BATCH_NEW_JOB_REPLY;

    struct protocol_header rep_hdr;
    init_protocol_header(&rep_hdr);
    rep_hdr.operation = BATCH_NEW_JOB_REPLY_ACK;
    rep_hdr.status = MBD_OK;

    if (auth_sign_header(&rep_hdr) < 0) {
        LL_ERR("job=%ld failed to sign header for host=%s", job->job_id,
               n->net.name);
        return;
    }

    if (enqueue_payload(n->sbd_chan, &rep_hdr, &ack, LL_BUFSIZ_1K,
                        xdr_wire_job_ack) < 0) {
        LL_ERR("job=%ld enqueue_payload failed", r.job_id);
        return;
    }

    if (duplicate)
        return;

    job->pid = (pid_t) r.pid;
    job->fork_time = time(NULL);
    job->state = JOB_RUNNING;
    event_job_fork(job);
    LL_INFO("job=%ld pid=%d acked", r.job_id, r.pid);
}

void mbd_job_finish(struct mbd_host *n, XDR *xdrs)
{
    struct wire_job_finish f;

    memset(&f, 0, sizeof(f));
    if (!xdr_wire_job_finish(xdrs, &f)) {
        LL_ERR("xdr_wire_job_finish decode failed from=%s",
               chan_addr_str(n->sbd_chan));
        return;
    }

    struct job_data *job = job_find(f.job_id);
    if (job == NULL) {
        // Distinguish missing from duplicate for code and logical clarity
        LL_ERR("job=%ld not found from=%s", f.job_id,
               chan_addr_str(n->sbd_chan));
        struct wire_job_ack ack;
        memset(&ack, 0, sizeof(ack));
        ack.job_id = f.job_id;
        ack.ack_op = BATCH_JOB_FINISH;
        struct protocol_header hdr;
        init_protocol_header(&hdr);
        hdr.operation = BATCH_JOB_FINISH_ACK;
        hdr.status = MBD_OK;
        if (auth_sign_header(&hdr) < 0) {
            LL_ERR("job=%ld failed to sign header", f.job_id);
            return;
        }
        enqueue_payload(n->sbd_chan, &hdr, &ack, LL_BUFSIZ_1K,
                        xdr_wire_job_ack);
        return;
    }

    if (strcmp(n->net.name, job->run_hosts[0]->net.name) != 0) {
        LL_WARNING("job=%ld finish reported by host=%s but dispatched to "
                   " host=%s — ignoring", job->job_id, n->net.name,
                   job->run_hosts[0]->net.name);
        return;
    }

    int duplicate = 0;
    if (job->end_time > 0) {
        LL_INFO("job=%ld finish duplicate from=%s", f.job_id,
                chan_addr_str(n->sbd_chan));
        duplicate = 1;
        goto send_ack;
    }
    // run the assert only the first time sbd is reporting job finished
    assert((job->state == JOB_RUNNING) || (job->state == JOB_SUSPENDED));

send_ack:
    struct wire_job_ack ack;
    memset(&ack, 0, sizeof(ack));
    ack.job_id = f.job_id;
    ack.ack_op = BATCH_JOB_FINISH;

    struct protocol_header hdr;
    init_protocol_header(&hdr);
    hdr.operation = BATCH_JOB_FINISH_ACK;
    hdr.status = MBD_OK;

    if (auth_sign_header(&hdr) < 0) {
        LL_ERR("job=%ld failed to sign header for host=%s", job->job_id,
               n->net.name);
        return;
    }

    if (enqueue_payload(n->sbd_chan, &hdr, &ack, LL_BUFSIZ_1K,
                        xdr_wire_job_ack) < 0) {
        LL_ERR("job=%ld enqueue_payload failed", f.job_id);
        return;
    }

    if (duplicate)
        return;

    job->end_time = time(NULL);
    job->exit_status = f.exit_status;

    LL_INFO("job=%ld usage mem=%luMB swap=%luMB cpu=%.2fs", f.job_id, f.mem_mb,
            f.swap_mb, f.cpu_time);

    // this function depends on the state of the job not
    // being DONE|EXIT yet
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

    if (f.state == 0)
        job->state = JOB_DONE;
    else
        job->state = JOB_EXITED;

    job_move_list(job, &run_jobs_list, &finish_jobs_list, JOB_LIST_FINISH);

    LL_INFO("job=%ld finish acked state=%s", f.job_id,
            job_state_str(job->state));

    // We free the job only when we compact the events file
    event_job_finish(job);
    if (job_write_usage(job, &f) < 0) {
        LL_ERR("job=%ld failed job_write_usage", job->job_id);
    }

    LL_DEBUG("queue=%s num_pend=%d num_run=%d num_susp=%d", job->queue->name,
             job->queue->num_pend, job->queue->num_run, job->queue->num_susp);

    // debug code
    mbd_assert_counters();
}

char *job_state_str(int state)
{
    switch (state) {
    case JOB_PENDING:
        return "PEND";
    case JOB_HELD:
        return "HELD";
    case JOB_RUNNING:
        return "RUN";
    case JOB_SUSPENDED:
        return "SUSP";
    case JOB_EXITED:
        return "EXIT";
    case JOB_DONE:
        return "DONE";
    case JOB_ORPHAN:
        return "ORPHAN";
    default:
        return "BADSTATE";
    }
}

void mbd_job_signal_reply(struct mbd_host *n, XDR *xdrs,
                          struct protocol_header *hdr)
{
    struct wire_job_sig sig;

    memset(&sig, 0, sizeof(sig));

    if (!xdr_wire_job_sig(xdrs, &sig)) {
        LL_ERR("xdr_wire_job_sig decode failed from=%s",
               chan_addr_str(n->sbd_chan));
        return;
    }

    struct job_data *job = job_find(sig.job_id);
    if (job == NULL) {
        LL_ERRX("signal reply for unknown job=%ld sig=%d errno=%d from=%s",
                sig.job_id, sig.sig, hdr->status, chan_addr_str(n->sbd_chan));
        return;
    }

    if (hdr->status != MBD_OK) {
        LL_ERRX("job=%ld signal=%d failed status=%d host=%s", job->job_id,
                sig.sig, hdr->status, n->net.name);
        return;
    }

    if (sig.sig == SIGSTOP || sig.sig == SIGTSTP) {
        job->state = JOB_SUSPENDED;
        job->queue->num_run--;
        job->queue->num_susp++;
        n->num_susp++;
        n->num_run--;
    } else if (sig.sig == SIGCONT) {
        job->state = JOB_RUNNING;
        job->queue->num_susp--;
        job->queue->num_run++;
        n->num_susp--;
        n->num_run++;
    }

    LL_DEBUG("queue=%s num_pend=%d num_run=%d num_susp=%d", job->queue->name,
             job->queue->num_pend, job->queue->num_run, job->queue->num_susp);

    LL_INFO("job=%ld signal=%d delivered host=%s", job->job_id, sig.sig,
            n->net.name);
    // debug
    mbd_assert_counters();
}

// cross check counters computationally expensive

void mbd_assert_counters(void)
{
    struct ll_list_entry *e;
    struct ll_list_entry *je;

    if (! assert_counters)
        return;

    for (e = host_list.head; e != NULL; e = e->next) {
        struct mbd_host *h = (struct mbd_host *) e;
        int num_jobs = 0;
        int num_run = 0;
        int num_susp = 0;
        int num_cpus_used = 0;
        int num_gpu_used = 0;

        assert(h->num_jobs <= h->res.max_jobs);

        for (je = run_jobs_list.head; je != NULL; je = je->next) {
            struct job_data *job = (struct job_data *) je;

            if (!job_uses_host(job, h))
                continue;

            num_jobs++;
            num_cpus_used += job->res.num_cpus;
            if (job->res.num_gpus > 0)
                num_gpu_used += job->res.num_gpus;
            if (job->state == JOB_SUSPENDED)
                num_susp++;
            else if (job->state == JOB_RUNNING)
                num_run++;
            else
                assert(0);
        }

        if (h->num_jobs != num_jobs || h->num_run != num_run ||
            h->num_susp != num_susp || h->num_cpus_used != num_cpus_used) {
            LL_ERRX("host=%s bad counters jobs=%d/%d run=%d/%d susp=%d/%d "
                    "cpus_used=%d/%d",
                    h->net.name, h->num_jobs, num_jobs, h->num_run, num_run,
                    h->num_susp, num_susp, h->num_cpus_used, num_cpus_used);
            assert(0);
        }

        if (h->res.free_gpu != h->res.total_gpu - num_gpu_used) {
            LL_ERRX("host=%s bad gpu counter free=%d expected=%d",
                    h->net.name, h->res.free_gpu,
                    h->res.total_gpu - num_gpu_used);
            assert(0);
        }

        /* per gpu_type check */
        struct ll_list_entry *ge;
        for (ge = h->res.gpu_list.head; ge != NULL; ge = ge->next) {
            struct mbd_gpu *g = (struct mbd_gpu *) ge;
            int type_used = 0;
            for (je = run_jobs_list.head; je != NULL; je = je->next) {
                struct job_data *job = (struct job_data *) je;
                if (!job_uses_host(job, h))
                    continue;
                if (job->res.gpu_type[0] != 0
                    && strcmp(job->res.gpu_type, g->gpu_type) == 0)
                    type_used += job->res.num_gpus;
            }
            if (g->free != g->count - type_used) {
                LL_ERRX("host=%s gpu_type=%s bad counter free=%d expected=%d",
                        h->net.name, g->gpu_type, g->free,
                        g->count - type_used);
                assert(0);
            }
        }
    }

    for (e = queue_list.head; e != NULL; e = e->next) {
        struct mbd_queue *q = (struct mbd_queue *) e;
        int num_jobs = 0;
        int num_pend = 0;
        int num_run = 0;
        int num_susp = 0;
        int num_held = 0;
        int num_cpus_used = 0;
        int num_hosts_used = 0;

        for (je = pend_jobs_list.head; je != NULL; je = je->next) {
            struct job_data *job = (struct job_data *) je;
            if (job->queue != q)
                continue;
            num_jobs++;
            if (job->state == JOB_HELD)
                num_held++;
            else if (job->state == JOB_PENDING)
                num_pend++;
            else
                assert(0);
        }

        for (je = run_jobs_list.head; je != NULL; je = je->next) {
            struct job_data *job = (struct job_data *) je;
            if (job->queue != q)
                continue;
            num_jobs++;
            num_cpus_used += job->res.num_cpus * job->run_nhosts;
            num_hosts_used += job->run_nhosts;
            if (job->state == JOB_SUSPENDED)
                num_susp++;
            else if (job->state == JOB_RUNNING)
                num_run++;
            else
                assert(0);
        }

        if (q->num_jobs != num_jobs || q->num_pend != num_pend ||
            q->num_run != num_run || q->num_susp != num_susp ||
            q->num_held != num_held || q->num_cpus_used != num_cpus_used ||
            q->num_hosts_used != num_hosts_used) {
            LL_ERRX("queue=%s bad counters jobs=%d/%d pend=%d/%d run=%d/%d "
                    "susp=%d/%d held=%d/%d cpus_used=%d/%d hosts_used=%d/%d",
                    q->name, q->num_jobs, num_jobs, q->num_pend, num_pend,
                    q->num_run, num_run, q->num_susp, num_susp, q->num_held,
                    num_held, q->num_cpus_used, num_cpus_used,
                    q->num_hosts_used, num_hosts_used);
            assert(0);
        }
    }
}

int job_move(XDR *xdrs, int chan_id, const struct protocol_header *hdr)
{
    struct wire_job_move wm;
    memset(&wm, 0, sizeof(wm));
    if (!xdr_wire_job_move(xdrs, &wm)) {
        LL_ERRX("xdr_wire_job_move failed");
        return -1;
    }

    struct job_data *job = job_find(wm.job_id);
    if (job == NULL) {
        LL_ERRX("job=%ld not found", wm.job_id);
        enqueue_header(chan_id, BATCH_JOB_MOVE_ACK, ESRCH);
        return 0;
    }

    if (job->state != JOB_PENDING && job->state != JOB_HELD) {
        LL_ERRX("job=%ld state=%s not movable", wm.job_id,
                job_state_str(job->state));
        enqueue_header(chan_id, BATCH_JOB_MOVE_ACK, EINVAL);
        return 0;
    }

    struct mbd_queue *to = ll_hash_search(&queue_name_hash, wm.to_queue);
    if (to == NULL) {
        LL_ERRX("job=%ld queue=%s not found", wm.job_id, wm.to_queue);
        enqueue_header(chan_id, BATCH_JOB_MOVE_ACK, ESRCH);
        return 0;
    }


    if (job->uid != hdr->uid && !is_manager(hdr->uid)) {
        LL_ERRX("job=%ld of uid=%d cannot be moved by uid=%d", job->job_id,
                job->uid, hdr->uid);
        enqueue_header(chan_id, BATCH_JOB_MOVE_ACK, EPERM);
        return 0;
    }

    if (!queue_user_allowed(to, job->user)) {
        LL_ERRX("job=%ld user=%s not allowed in queue=%s",
                wm.job_id, job->user, to->name);
        enqueue_header(chan_id, BATCH_JOB_MOVE_ACK, EPERM);
        return 0;
    }

    /* update counters on from queue */
    struct mbd_queue *from = job->queue;
    if (job->state == JOB_PENDING)
        from->num_pend--;
    else
        from->num_held--;
    from->num_jobs--;

    event_job_move(job, to->name);

    job->queue = to;

    /* update counters on to queue */
    if (job->state == JOB_PENDING)
        to->num_pend++;
    else
        to->num_held++;
    to->num_jobs++;

    LL_INFO("job=%ld moved from=%s to=%s", wm.job_id, from->name, to->name);

    enqueue_header(chan_id, BATCH_JOB_MOVE_ACK, MBD_OK);

    mbd_assert_counters();
    return 0;
}

/* -----------------------------------------------------------
 * job signal
 * -----------------------------------------------------------
 */
static int finish_pending_job(struct job_data *job,
                              const struct wire_job_sig *ws)
{
    LL_INFO("finish_pending_job: job_id=%ld sig=%d -> EXIT", (long) job->job_id,
            ws->sig);
    job->signal_time = job->end_time = time(NULL);

    if (job->state == JOB_PENDING)
        job->queue->num_pend--;

    if (job->state == JOB_HELD)
        job->queue->num_held--;

    job->queue->num_jobs--;

    job->state = JOB_EXITED;
    event_job_signal(job, ws);
    event_job_finish(job);
    job_move_list(job, &pend_jobs_list, &finish_jobs_list, JOB_LIST_FINISH);

    return MBD_OK;
}

static int stop_pending_job(struct job_data *job, const struct wire_job_sig *ws)
{
    if (job->state == JOB_HELD)
        return MBD_OK;

    job->state = JOB_HELD;
    job->signal_time = time(NULL);
    LL_INFO("stop_pending_job: job_id=%ld sig=%d -> PSUSP", (long) job->job_id,
            ws->sig);
    event_job_signal(job, ws);
    event_job_pend_susp(job);

    job->queue->num_pend--;
    job->queue->num_held++;
    LL_DEBUG("queue=%s num_pend=%d num_run=%d num_susp=%d num_held=%d",
             job->queue->name, job->queue->num_pend, job->queue->num_run,
             job->queue->num_susp, job->queue->num_held);

    return MBD_OK;
}

static int resume_pending_job(struct job_data *job,
                              const struct wire_job_sig *ws)
{
    if (!(job->state == JOB_HELD))
        return MBD_OK;

    job->state = JOB_PENDING;
    job->signal_time = time(NULL);
    LL_INFO("resume_pending_job: job_id=%ld sig=%d -> PEND", (long) job->job_id,
            ws->sig);
    event_job_signal(job, ws);
    event_job_pend_resume(job);

    job->queue->num_held--;
    job->queue->num_pend++;
    LL_DEBUG("queue=%s num_pend=%d num_run=%d num_susp=%d num_held=%d",
             job->queue->name, job->queue->num_pend, job->queue->num_run,
             job->queue->num_susp, job->queue->num_held);

    return MBD_OK;
}

static int signal_pending_job(struct job_data *job,
                              const struct wire_job_sig *ws)
{
    switch (ws->sig) {
    case SIGTERM:
    case SIGINT:
    case SIGKILL:
        return finish_pending_job(job, ws);
    case SIGSTOP:
    case SIGTSTP:
        return stop_pending_job(job, ws);
    case SIGCONT:
        return resume_pending_job(job, ws);
    default:
        LL_DEBUG("signal_pending_job: job_id=%ld sig=%d unsupported",
                 (long) job->job_id, ws->sig);
        return EINVAL;
    }
}

static int signal_running_job(struct job_data *job,
                              const struct wire_job_sig *ws)
{
    struct protocol_header hdr;
    init_protocol_header(&hdr);
    hdr.operation = BATCH_SBD_JOB_SIGNAL;
    hdr.status = MBD_OK;

    if (auth_sign_header(&hdr) < 0) {
        LL_ERR("job=%ld failed to sign header for host=%s", job->job_id,
               job->run_hosts[0]->net.name);
        return EAGAIN;
    }

    if (enqueue_payload(job->run_hosts[0]->sbd_chan, &hdr, (void *) ws,
                        LL_BUFSIZ_1K, xdr_wire_job_sig) < 0) {
        LL_ERR("job=%ld enqueue_payload failed", job->job_id);
        return EAGAIN;
    }

    job->signal_time = time(NULL);
    event_job_signal(job, ws);

    LL_INFO("job=%ld sig=%d sent to sbd=%s", job->job_id, ws->sig,
            job->run_hosts[0]->net.name);

    return MBD_OK;
}

static int signal_all_jobs(uint32_t uid, struct wire_job_sig *req)
{
    struct ll_list_entry *e;
    struct ll_list_entry *next;
    struct job_data *job;

    for (e = pend_jobs_list.head; e != NULL; e = next) {
        next = e->next;
        job = (struct job_data *) e;
        assert(job->state == JOB_PENDING || job->state == JOB_HELD);
        if (job->uid != uid)
            continue;
        req->job_id = job->job_id;
        // Best effort, even if one failed keep going
        signal_pending_job(job, req);
    }
    for (e = run_jobs_list.head; e != NULL; e = e->next) {
        job = (struct job_data *) e;

        assert(job->run_hosts[0]);
        if (job->run_hosts[0]->sbd_chan < 0) {
            LL_DEBUG("sbd=%s is disconnected", job->run_hosts[0]->net.name);
            continue;
        }

        assert(job->state == JOB_RUNNING || job->state == JOB_SUSPENDED);
        if (job->uid != uid)
            continue;
        req->job_id = job->job_id;
        // Best effort, even if one fails keep going
        signal_running_job(job, req);
    }

    return MBD_OK;
}

int jobs_signal(XDR *xdrs, int chan_id, const struct protocol_header *hdr)
{
    struct wire_job_sig req;
    memset(&req, 0, sizeof(req));
    if (!xdr_wire_job_sig(xdrs, &req)) {
        LL_ERR("job_signal: xdr decode failed chan_id=%d", chan_id);
        return enqueue_header(chan_id, BATCH_JOB_SIGNAL_ACK, EPROTO);
    }

    LL_DEBUG("job_id=%ld by uid=%u sig=%d chan_id=%d", (long) req.job_id,
             req.uid, req.sig, chan_id);

    if (req.job_id == 0) {
        int cc = signal_all_jobs(hdr->uid, &req);
        return enqueue_header(chan_id, BATCH_JOB_SIGNAL_ACK, cc);
    }

    struct job_data *job = job_find(req.job_id);
    if (job == NULL) {
        LL_INFO("job_signal: job_id=%ld not found", (long) req.job_id);
        return enqueue_header(chan_id, BATCH_JOB_SIGNAL_ACK, ESRCH);
    }

    if (job->uid != (uid_t) hdr->uid && !is_manager(hdr->uid)) {
        LL_ERR("job=%ld uid=%d not owned by signaling uid=%d", job->job_id,
               job->uid, hdr->uid);
        return enqueue_header(chan_id, BATCH_JOB_SIGNAL_ACK, EPERM);
    }

    if (job->state == JOB_DONE || job->state == JOB_EXITED) {
        LL_DEBUG("job_signal: job_id=%ld already finished", (long) req.job_id);
        return enqueue_header(chan_id, BATCH_JOB_SIGNAL_ACK, EINVAL);
    }

    if ((req.sig == SIGSTOP || req.sig == SIGTSTP) &&
        (job->state == JOB_SUSPENDED || job->state == JOB_HELD)) {
        LL_DEBUG("job_signal: job_id=%ld already suspended no-op",
                 (long) req.job_id);
        return enqueue_header(chan_id, BATCH_JOB_SIGNAL_ACK, MBD_OK);
    }

    if (req.sig == SIGCONT &&
        (job->state == JOB_PENDING || job->state == JOB_RUNNING)) {
        LL_DEBUG("job_signal: job_id=%ld SIGCONT no-op", (long) req.job_id);
        return enqueue_header(chan_id, BATCH_JOB_SIGNAL_ACK, MBD_OK);
    }

    int cc;
    if (job->state == JOB_PENDING || job->state == JOB_HELD)
        cc = signal_pending_job(job, &req);
    else
        cc = signal_running_job(job, &req);

    return enqueue_header(chan_id, BATCH_JOB_SIGNAL_ACK, cc);
}

int job_priority(XDR *xdrs, int chan_id, const struct protocol_header *hdr)
{
    struct wire_job_priority wp;
    memset(&wp, 0, sizeof(wp));
    if (!xdr_wire_job_priority(xdrs, &wp)) {
        LL_ERRX("xdr_wire_job_priority failed");
        return -1;
    }

    struct job_data *job = job_find(wp.job_id);
    if (job == NULL) {
        LL_INFO("job=%ld not found", wp.job_id);
        return enqueue_header(chan_id, BATCH_JOB_PRIORITY_ACK, ESRCH);
    }

    if (job->state == JOB_DONE || job->state == JOB_EXITED) {
        LL_INFO("job_priority: job=%ld already finished", wp.job_id);
        return enqueue_header(chan_id, BATCH_JOB_PRIORITY_ACK, EINVAL);
    }

    /* ownership check — admin can bypass */
    if (job->uid != (uid_t)hdr->uid && !is_manager(hdr->uid)) {
        LL_INFO("job=%ld uid=%u not owner", wp.job_id, hdr->uid);
        return enqueue_header(chan_id, BATCH_JOB_PRIORITY_ACK, EPERM);
    }

    /* non-admin cannot exceed queue priority */
    if (!is_manager(hdr->uid) && wp.priority > job->queue->priority) {
        LL_INFO("job=%ld priority=%d exceeds queue=%d",
                wp.job_id, wp.priority, job->queue->priority);
        return enqueue_header(chan_id, BATCH_JOB_PRIORITY_ACK, EPERM);
    }

    if (wp.priority < 0) {
        LL_INFO("job=%ld invalid priority=%d specified", job->job_id,
                wp.priority);
        return enqueue_header(chan_id, BATCH_JOB_PRIORITY_ACK, EINVAL);
    }

    // Admin with this operation can jump queue
    int32_t old_priority = job->priority;
    if (wp.priority > old_priority && !is_manager(hdr->uid))
        return enqueue_header(chan_id, BATCH_JOB_PRIORITY_ACK, EPERM);

    if (wp.priority == old_priority)
        return enqueue_header(chan_id, BATCH_JOB_PRIORITY_ACK, MBD_OK);
    job->priority = wp.priority;

    event_job_priority(job, old_priority);

    LL_INFO("job=%ld priority old=%d new=%d", wp.job_id,
            old_priority, job->priority);

    return enqueue_header(chan_id, BATCH_JOB_PRIORITY_ACK, MBD_OK);
}
