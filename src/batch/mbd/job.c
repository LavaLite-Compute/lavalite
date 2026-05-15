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

static int64_t next_job_id(void)
{
    do {
        job_id_seq++;
    } while (job_find(job_id_seq) != NULL);

    return job_id_seq;
}

static void job_free(struct job_data *job)
{
    free(job->run_hosts);
    ll_hash_clear(&job->res.machines, NULL);
    free(job);
}

static struct job_data *job_alloc(struct wire_job_submit *ws)
{
    struct job_data *job = calloc(1, sizeof(struct job_data));
    if (job == NULL) {
        LS_ERR("calloc failed");
        return NULL;
    }

    job->job_id = next_job_id();
    job->uid  = (uid_t)ws->uid;
    job->gid = (gid_t)ws->gid;
    job->priority = 0;
    job->flags = ws->flags;
    job->state = JOB_PENDING;
    if (job->flags & JOB_FLAG_HOLD)
        job->state = JOB_HELD;
    job->submit_time = time(NULL);
    ll_strlcpy(job->user, ws->username, sizeof(job->user));
    job->begin_time = (time_t)ws->begin_time;
    job->term_time  = (time_t)ws->term_time;
    ll_strlcpy(job->project,  ws->project, sizeof(job->project));

    ll_strlcpy(job->res.gpu_type, ws->gpu_type, sizeof(job->res.gpu_type));
    job->res.wall_seconds = ws->wall_seconds;
    job->res.num_cpus = ws->num_cpus;
    job->res.num_hosts = ws->num_hosts;
    job->res.num_gpus = ws->num_gpus;
    job->res.mem_mb = ws->mem_mb;
    job->res.storage_mb = ws->storage_mb;
    machines_hash_populate(&job->res.machines, ws->machines);

    if (ws->name[0] == 0) {
        ll_strlcpy(job->name, "-" , sizeof(job->name));
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
        LS_ERRX("queue='%s' not found", queue);
        free(job);
        return NULL;
    }

    if (job->res.num_hosts < 1) {
        LS_DEBUG("job=%ld num_hosts set to 1", job->job_id);
        job->res.num_hosts = 1;
    }

    job->run_hosts = calloc(job->res.num_hosts, sizeof(struct mbd_host *));
    if (job->run_hosts == NULL) {
        LS_ERR("calloc failed");
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
    int n = snprintf(dir, sizeof(dir), "%s/%ld/%ld",
                     jobs_dir, (job->job_id % JOB_BUCKETS), job->job_id);
    if (n < 0 || n >= (int)sizeof(dir))
        return -1;

    if (mkdir(dir, 0755) < 0 && errno != EEXIST) {
        LS_ERR("mkdir=%s", dir);
        return -1;
    }

    char path[PATH_MAX];
    n = snprintf(path, sizeof(path), "%s/%ld/%ld/script.sh",
                 jobs_dir, (job->job_id % JOB_BUCKETS), job->job_id);
    if (n < 0 || n >= (int)sizeof(path))
        return -1;

    char tmp[PATH_MAX];
    n = snprintf(tmp, sizeof(tmp), "%s/%ld/%ld/script.sh.tmp",
                 jobs_dir, (job->job_id % JOB_BUCKETS), job->job_id);
    if (n < 0 || n >= (int)sizeof(tmp))
        return -1;

    int fd = open(tmp, O_WRONLY|O_CREAT|O_TRUNC, 0700);
    if (fd < 0) {
        LS_ERR("open %s", tmp);
        return -1;
    }

    ssize_t nw = write(fd, script->data, script->len);
    if (nw < 0 || (uint32_t)nw != script->len) {
        LS_ERR("write %s", tmp);
        close(fd);
        return -1;
    }

    if (fsync(fd) < 0) {
        LS_ERR("fsync %s", tmp);
        close(fd);
        return -1;
    }
    close(fd);

    if (rename(tmp, path) < 0) {
        LS_ERR("rename %s -> %s", tmp, path);
        return -1;
    }

    return 0;
}

static int write_sidecar(const struct job_data *job,
                         const struct wire_job_submit *ws)
{
    char path[PATH_MAX];
    int n = snprintf(path, sizeof(path), "%s/%ld/%ld/submit",
                     jobs_dir, (job->job_id % JOB_BUCKETS), job->job_id);
    if (n < 0 || n >= (int)sizeof(path))
        return -1;

    char tmp[PATH_MAX];
    n = snprintf(tmp, sizeof(tmp), "%s/%ld/%ld/submit.tmp",
                 jobs_dir, (job->job_id % JOB_BUCKETS), job->job_id);
    if (n < 0 || n >= (int)sizeof(tmp))
        return -1;

    FILE *fp = fopen(tmp, "w");
    if (fp == NULL) {
        LS_ERR("fopen %s: %m", tmp);
        return -1;
    }

    fprintf(fp, "JOB_ID=%ld\n", job->job_id);
    fprintf(fp, "UID=%u\n", ws->uid);
    fprintf(fp, "GID=%u\n", ws->gid);
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
    fprintf(fp, "WALL_SECONDS=%d\n", ws->wall_seconds);
    fprintf(fp, "UMASK=%o\n", ws->umask);
    fprintf(fp, "FLAGS=%u\n", ws->flags);
    fprintf(fp, "BEGIN_TIME=%ld\n", ws->begin_time);
    fprintf(fp, "TERM_TIME=%ld\n", ws->term_time);
    fprintf(fp, "DEPEND_COND=%s\n", ws->depend_cond);
    fprintf(fp, "MACHINES=%s\n", ws->machines);
    fprintf(fp, "GPU_TYPE=%s\n", ws->gpu_type);
    fprintf(fp, "COMMENT=%s\n", ws->comment);

    if (fflush(fp) != 0 || ferror(fp)) {
        LS_ERR("write error %s: %m", tmp);
        fclose(fp);
        return -1;
    }
    fclose(fp);

    if (rename(tmp, path) < 0) {
        LS_ERR("rename %s -> %s: %m", tmp, path);
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

int job_register(XDR *xdrs, int chan_id)
{
    struct wire_job_submit ws;
    memset(&ws, 0, sizeof(ws));
    if (!xdr_wire_job_submit(xdrs, &ws)) {
        LS_ERRX("xdr_wire_job_submit failed");
        return -1;
    }

    struct wire_job_script script;
    memset(&script, 0, sizeof(script));
    if (!xdr_wire_job_script(xdrs, &script)) {
        LS_ERRX("xdr_wire_job_script failed");
        return -1;
    }

    struct job_data *job = job_alloc(&ws);
    if (job == NULL) {
        LS_ERR("job_alloc failed");
        free(script.data);
        return -1;
    }

    if (write_script(job, &script) < 0) {
        LS_ERR("write_script failed job_id=%ld", job->job_id);
        free(script.data);
        job_free(job);
        return -1;
    }
    free(script.data);

    if (write_sidecar(job, &ws) < 0) {
        LS_ERR("write_sidecar failed job_id=%ld", job->job_id);
        job_free(job);
        return -1;
    }

    char key[LL_BUFSIZ_32];
    sprintf(key, "%ld", job->job_id);
    enum ll_hash_status hs = ll_hash_insert(&job_id_hash, key, job, 0);
    assert(hs == LL_HASH_INSERTED);

    job_set_list(job, &pend_jobs_list, JOB_LIST_PEND);

    struct wire_job_submit_reply reply;
    memset(&reply, 0, sizeof(reply));
    reply.job_id = job->job_id;

    struct protocol_header hdr;
    init_protocol_header(&hdr);
    hdr.operation = BATCH_JOB_SUBMIT_ACK;
    hdr.status    = MBD_OK;

    size_t siz = PACKET_HEADER_SIZE + sizeof(struct wire_job_submit_reply)
        + LL_BUFSIZ_64;

    if (enqueue_payload(chan_id, &hdr, &reply, siz,
                        xdr_wire_job_submit_reply) < 0) {
        LS_ERR("enqueue_payload failed job_id=%ld", job->job_id);
        ll_list_remove(&pend_jobs_list, &job->ent);
        ll_hash_remove(&job_id_hash, key);
        job_free(job);
        return -1;
    }

    event_job_new(job, &ws);
    job->queue->num_pend++;
    job->queue->num_jobs++;

    LS_INFO("job_id=%ld user=%s queue=%s num_jobs=%d num_pend=%d",
            job->job_id, job->user, job->queue->name,
            job->queue->num_jobs, job->queue->num_pend);

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

    LS_INFO("job structures initialized");

    int nj = jobs_replay();
    LS_INFO("num=%d jobs replayed", nj);

    return 0;
}

void mbd_new_job_reply(struct mbd_host *n, XDR *xdrs)
{
    struct wire_job_reply r;
    memset(&r, 0, sizeof(r));
    if (!xdr_wire_job_reply(xdrs, &r)) {
        LS_ERR("xdr_wire_job_reply decode failed from=%s",
               chan_addr_str(n->sbd_chan));
        return;
    }

    struct job_data *job = job_find(r.job_id);
    if (job == NULL) {
        LS_ERR("job=%ld not found from=%s - admin intervention required",
               r.job_id, chan_addr_str(n->sbd_chan));
        return;
    }
    if (r.state != JOB_RUNNING) {
        LS_ERR("job=%ld unexpected state=%d from=%s - admin intervention required",
               r.job_id, r.state, chan_addr_str(n->sbd_chan));
        return;
    }

    int duplicate = 0;
    /* duplicate: skip event log, sbd resends if it restarts before ack
     */
    if (job->fork_time > 0) {
        LS_INFO("job=%ld fork duplicate from=%s", r.job_id,
                chan_addr_str(n->sbd_chan));
        duplicate = 1;
        // fall through
    }

    struct wire_job_ack ack;
    memset(&ack, 0, sizeof(ack));
    ack.job_id = r.job_id;
    ack.ack_op = BATCH_NEW_JOB_REPLY;

    struct protocol_header hdr;
    init_protocol_header(&hdr);
    hdr.operation = BATCH_NEW_JOB_REPLY_ACK;
    hdr.status = MBD_OK;

    if (auth_sign_header(&hdr) < 0) {
        LS_ERR("job=%ld failed to sign header for host=%s", job->job_id,
               n->net.name);
        return;
    }

    if (enqueue_payload(n->sbd_chan, &hdr, &ack,
                        LL_BUFSIZ_1K, xdr_wire_job_ack) < 0) {
        LS_ERR("job=%ld enqueue_payload failed", r.job_id);
        return;
    }

    if (duplicate)
        return;

    job->usage.pid = (pid_t)r.pid;
    job->fork_time = time(NULL);
    job->state = JOB_RUNNING;
    event_job_fork(job);
    LS_INFO("job=%ld pid=%d acked", r.job_id, r.pid);
}

void mbd_job_execute(struct mbd_host *n, XDR *xdrs)
{
    struct wire_job_state s;
    memset(&s, 0, sizeof(s));

    if (!xdr_wire_job_state(xdrs, &s)) {
        LS_ERR("xdr_wire_job_state decode failed from=%s",
               chan_addr_str(n->sbd_chan));
        return;
    }

    struct job_data *job = job_find(s.job_id);
    if (job == NULL) {
        LS_ERR("job=%ld not found from=%s", s.job_id, chan_addr_str(n->sbd_chan));
        return;
    }
    assert(job->state == JOB_RUNNING);

    int duplicate = 0;
    /* duplicate: skip event log, sbd resends if it restarts before ack
     */
    if (job->execute_time > 0) {
        LS_INFO("job=%ld execute duplicate from=%s", s.job_id,
                chan_addr_str(n->sbd_chan));
        duplicate = 1;
        // fall through
    }

    struct wire_job_ack ack;
    memset(&ack, 0, sizeof(ack));
    ack.job_id = s.job_id;
    ack.ack_op = BATCH_JOB_EXECUTE;

    struct protocol_header hdr;
    init_protocol_header(&hdr);
    hdr.operation = BATCH_JOB_EXECUTE_ACK;
    hdr.status = MBD_OK;

    if (auth_sign_header(&hdr) < 0) {
        LS_ERR("job=%ld failed to sign header for host=%s", job->job_id,
               n->net.name);
        return;
    }

    if (enqueue_payload(n->sbd_chan, &hdr, &ack,
                        LL_BUFSIZ_1K, xdr_wire_job_ack) < 0) {
        LS_ERR("job=%ld enqueue_payload failed", s.job_id);
        return;
    }

    if (duplicate)
        return;

    // now we can commit the event and the time
    job->execute_time = time(NULL);
    event_job_execute(job, n->net.name);
    LS_INFO("job=%ld execute acked", s.job_id);
}

static void reset_host_resources(struct job_data *job)
{
    for (int i = 0; i < job->run_nhosts; i++) {
        struct mbd_host *h = job->run_hosts[i];

        h->res.free_cpu += job->res.num_cpus;
        h->res.free_mem_mb += job->res.mem_mb;
        h->res.free_storage_mb += job->res.storage_mb;
        h->num_jobs--;

        if (job->state == JOB_RUNNING)
            h->num_run--;

        if (job->state == JOB_SUSPENDED)
            h->num_susp--;

        if (job->flags & JOB_FLAG_EXCLUSIVE)
            h->exclusive = 0;

        if (job->res.num_gpus > 0) {
            struct mbd_gpu *g = ll_hash_search(&h->res.gpu_hash,
                                               job->res.gpu_type);
            if (g == NULL) {
                LS_ERRX("job=%ld host=%s gpu_type=%s not found",
                        job->job_id, h->net.name, job->res.gpu_type);
                assert(0);
                continue;
            }
            g->free += job->res.num_gpus;
            h->res.free_gpu += job->res.num_gpus;
        }

        LS_DEBUG("host=%s free_cpu=%d free_mem_mb=%lu free_storage_mb=%lu "
                 "free_gpu=%d num_jobs=%d",
                 h->net.name, h->res.free_cpu, h->res.free_mem_mb,
                 h->res.free_storage_mb, h->res.free_gpu, h->num_jobs);
    }
}

void mbd_job_finish(struct mbd_host *n, XDR *xdrs)
{
    struct wire_job_state s;
    memset(&s, 0, sizeof(s));

    if (!xdr_wire_job_state(xdrs, &s)) {
        LS_ERR("xdr_wire_job_state decode failed from=%s",
               chan_addr_str(n->sbd_chan));
        return;
    }

    struct job_data *job = job_find(s.job_id);
    if (job == NULL) {
        LS_ERR("job=%ld not found from=%s", s.job_id, chan_addr_str(n->sbd_chan));
        return;
    }

    int duplicate = 0;
    if (job->end_time > 0) {
        LS_INFO("job=%ld finish duplicate from=%s", s.job_id,
                chan_addr_str(n->sbd_chan));
        duplicate = 1;
        goto send_ack;
    }
    // run the assert only the first time sbd is reporting job finished
    assert((job->state == JOB_RUNNING) || (job->state == JOB_SUSPENDED));

send_ack:
    struct wire_job_ack ack;
    memset(&ack, 0, sizeof(ack));
    ack.job_id = s.job_id;
    ack.ack_op = BATCH_JOB_FINISH;

    struct protocol_header hdr;
    init_protocol_header(&hdr);
    hdr.operation = BATCH_JOB_FINISH_ACK;
    hdr.status = MBD_OK;

    if (auth_sign_header(&hdr) < 0) {
        LS_ERR("job=%ld failed to sign header for host=%s", job->job_id,
               n->net.name);
        return;
    }

    if (enqueue_payload(n->sbd_chan, &hdr, &ack,
                        LL_BUFSIZ_1K, xdr_wire_job_ack) < 0) {
        LS_ERR("job=%ld enqueue_payload failed", s.job_id);
        return;
    }

    if (duplicate)
        return;

    job->end_time = time(NULL);
    job->exit_status = s.state;

    // this function depends on the state of the job not
    // being DONE|EXIT yet
    reset_host_resources(job);

    if (s.state == 0)
        job->state = JOB_DONE;
    else
        job->state = JOB_EXITED;

    job_move_list(job, &run_jobs_list, &finish_jobs_list, JOB_LIST_FINISH);

    LS_INFO("job=%ld finish acked state=%s", s.job_id,
            job_state_str(job->state));

    event_job_finish(job);

    if (job->state == JOB_RUNNING)
        job->queue->num_run--;

    if (job->state == JOB_SUSPENDED)
        job->queue->num_susp--;

    job->queue->num_jobs--;

    assert(job->queue->num_run >= 0);
    assert(job->queue->num_susp >= 0);
    assert(job->queue->num_jobs >= 0);

    LS_DEBUG("queue=%s num_pend=%d num_run=%d num_susp=%d",
             job->queue->name, job->queue->num_pend,
             job->queue->num_run, job->queue->num_susp);

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
        LS_ERR("xdr_wire_job_sig decode failed from=%s",
               chan_addr_str(n->sbd_chan));
        return;
    }

    struct job_data *job = job_find(sig.job_id);
    if (job == NULL) {
        LS_ERRX("signal reply for unknown job=%ld sig=%d errno=%d from=%s",
                sig.job_id, sig.sig, hdr->status, chan_addr_str(n->sbd_chan));
        return;
    }

    if (hdr->status != MBD_OK) {
        LS_ERRX("job=%ld signal=%d failed status=%d host=%s",
                job->job_id, sig.sig, hdr->status, n->net.name);
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

    LS_DEBUG("queue=%s num_pend=%d num_run=%d num_susp=%d",
             job->queue->name, job->queue->num_pend,
             job->queue->num_run, job->queue->num_susp);

    LS_INFO("job=%ld signal=%d delivered host=%s",
            job->job_id, sig.sig, n->net.name);
    // debug
    mbd_assert_counters();
}

// cross check counters computationally expensive
void mbd_assert_counters(void)
{
    struct ll_list_entry *e;
    struct ll_list_entry *je;

    for (e = host_list.head; e != NULL; e = e->next) {
        struct mbd_host *h = (struct mbd_host *)e;
        int num_jobs = 0;
        int num_run = 0;
        int num_susp = 0;

        for (je = run_jobs_list.head; je != NULL; je = je->next) {
            struct job_data *job = (struct job_data *)je;

            if (!job_uses_host(job, h))
                continue;

            num_jobs++;

            if (job->state == JOB_SUSPENDED)
                num_susp++;
            else if (job->state == JOB_RUNNING)
                num_run++;
            else
                assert(0);
        }

        if (h->num_jobs != num_jobs || h->num_run != num_run
            || h->num_susp != num_susp) {
            LS_ERRX("host=%s bad counters jobs=%d/%d run=%d/%d susp=%d/%d",
                    h->net.name,
                    h->num_jobs, num_jobs,
                    h->num_run, num_run,
                    h->num_susp, num_susp);
            assert(0);
        }
    }

    for (e = queue_list.head; e != NULL; e = e->next) {
        struct mbd_queue *q = (struct mbd_queue *)e;
        int num_jobs = 0;
        int num_pend = 0;
        int num_run = 0;
        int num_susp = 0;
        int num_held = 0;

        for (je = pend_jobs_list.head; je != NULL; je = je->next) {
            struct job_data *job = (struct job_data *)je;

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
            struct job_data *job = (struct job_data *)je;

            if (job->queue != q)
                continue;

            num_jobs++;

            if (job->state == JOB_SUSPENDED)
                num_susp++;
            else if (job->state == JOB_RUNNING)
                num_run++;
            else
                assert(0);
        }

        if (q->num_jobs != num_jobs || q->num_pend != num_pend
            || q->num_run != num_run || q->num_susp != num_susp
            || q->num_held != num_held) {
            LS_ERRX("queue=%s bad counters jobs=%d/%d pend=%d/%d run=%d/%d "
                    "susp=%d/%d held=%d/%d",
                    q->name,
                    q->num_jobs, num_jobs,
                    q->num_pend, num_pend,
                    q->num_run, num_run,
                    q->num_susp, num_susp,
                    q->num_held, num_held);
            assert(0);
        }
    }
}
