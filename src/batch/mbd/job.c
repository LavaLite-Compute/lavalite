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

#include "base/lib/ll.syslog.h"
#include "base/lib/ll.conf.h"
#include "base/lib/ll.list.h"
#include "base/lib/ll.hash.h"
#include "batch/mbd/mbd.h"
#include "batch/lib/wire.h"


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

static struct job_data *job_alloc(const struct wire_job_submit *ws)
{
    struct job_data *job = calloc(1, sizeof(struct job_data));
    if (job == NULL) {
        LS_ERR("calloc failed");
        return NULL;
    }

    job->job_id = next_job_id();
    job->uid  = (uid_t)ws->uid;
    job->priority = 0;
    job->status = JOB_STAT_PEND;
    job->submit_time = (time_t)ws->submit_time;
    job->pend_sig = 0;
    ll_strlcpy(job->user, ws->username, sizeof(job->user));
    job->num_cpus  = ws->num_cpus;
    job->num_nhosts = ws->num_nhosts;
    job->num_gpus  = ws->num_gpus;
    job->mem_mb    = ws->mem_mb;
    job->flags     = ws->flags;
    job->begin_time = (time_t)ws->begin_time;
    job->term_time  = (time_t)ws->term_time;

    ll_strlcpy(job->project,  ws->project, sizeof(job->project));
    ll_strlcpy(job->gpu_type, ws->gpu_type, sizeof(job->gpu_type));
    ll_strlcpy(job->machines, ws->machines, sizeof(job->machines));
    if (ws->name[0] != 0)
        ll_strlcpy(job->name,  ws->name, sizeof(job->name));
    else
        ll_strlcpy(job->name, "-" , sizeof(job->name));
    ll_strlcpy(job->comment, ws->comment, sizeof(job->comment));
    ll_strlcpy(job->from_host, ws->from_host, sizeof(job->from_host));

    const char *queue = ll_params[LL_DEFAULT_QUEUE].val;
    if (ws->queue[0] != 0)
        queue = ws->queue;

    job->queue = ll_hash_search(&queue_name_hash, queue);
    if (job->queue == NULL) {
        LS_ERRX("queue='%s' not found", queue);
        free(job);
        return NULL;
    }

    return job;
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
        LS_ERR("open %s: %m", tmp);
        return -1;
    }

    ssize_t nw = write(fd, script->data, script->len);
    if (nw < 0 || (uint32_t)nw != script->len) {
        LS_ERR("write %s: %m", tmp);
        close(fd);
        return -1;
    }

    if (fsync(fd) < 0) {
        LS_ERR("fsync %s: %m", tmp);
        close(fd);
        return -1;
    }
    close(fd);

    if (rename(tmp, path) < 0) {
        LS_ERR("rename %s -> %s: %m", tmp, path);
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
    fprintf(fp, "CWD=%s\n", ws->cwd);
    fprintf(fp, "COMMAND=%s\n", ws->command);
    fprintf(fp, "IN_FILE=%s\n", ws->in_file);
    fprintf(fp, "OUT_FILE=%s\n", ws->out_file);
    fprintf(fp, "ERR_FILE=%s\n", ws->err_file);
    fprintf(fp, "NUM_CPUS=%d\n", ws->num_cpus);
    fprintf(fp, "NUM_NHOSTS=%d\n", ws->num_nhosts);
    fprintf(fp, "NUM_GPUS=%d\n", ws->num_gpus);
    fprintf(fp, "MEM_MB=%lu\n", ws->mem_mb);
    fprintf(fp, "WALL_SECONDS=%d\n", ws->wall_seconds);
    fprintf(fp, "UMASK=%o\n", ws->umask);
    fprintf(fp, "FLAGS=%u\n", ws->flags);
    fprintf(fp, "BEGIN_TIME=%ld\n", ws->begin_time);
    fprintf(fp, "TERM_TIME=%ld\n", ws->term_time);
    fprintf(fp, "SUBMIT_TIME=%ld\n", ws->submit_time);
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

int job_accept(XDR *xdrs, int chan_id)
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
        free(job);
        return -1;
    }
    free(script.data);

    if (write_sidecar(job, &ws) < 0) {
        LS_ERR("write_sidecar failed job_id=%ld", job->job_id);
        free(job);
        return -1;
    }

    if (log_job_new(job, &ws) < 0) {
        LS_ERR("log_job_new failed job_id=%ld", job->job_id);
        free(job);
        return -1;
    }

    char key[LL_BUFSIZ_32];
    sprintf(key, "%ld", job->job_id);
    enum ll_hash_status hs = ll_hash_insert(&job_id_hash, key, job, 0);
    assert(hs == LL_HASH_INSERTED);

    ll_list_append(&pend_jobs_list, &job->ent);

    LS_INFO("job_id=%ld user=%s queue=%s",
            job->job_id, job->user, job->queue->name);

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
        return -1;
    }

    return 0;
}

/*
 * job_find - look up a job by job_id.
 */
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

    LS_INFO("job's structures initialized");
    return 0;
}
