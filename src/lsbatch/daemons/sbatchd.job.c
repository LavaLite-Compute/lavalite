/*
 * Copyright (C) LavaLite Contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#include "lsbatch/daemons/sbd.h"

extern int sbd_mbd_chan;      /* defined in sbatchd.main.c */

static sbdReplyType spawn_job(struct sbd_job *);
static void child_exec_job(struct sbd_job *);
static int set_job_env(const struct sbd_job *);
static int set_user_id(const struct sbd_job *);
static void reset_signals(void);
static int run_qpre(const struct jobSpecs *);
static int run_upre(const struct jobSpecs *);
static int cd_work_dir(const struct sbd_job *);
static int redirect_stdio(const struct jobSpecs *);
static struct sbd_job *find_job_by_jid(int64_t);
static int expand_stdio_path(const struct jobSpecs *, const char *,
                                 char *, size_t);
static void killpg_job(struct sbd_job *, int, struct wire_job_sig_reply *);
static int build_runtime_spec(struct sbd_job *);
static int dup_str_array(char ***, char *const *, int);
static int dup_len_data(struct lenData *, const struct lenData *);
static int dup_job_file_data(struct wire_job_file *,
                             const struct wire_job_file *);
static int copy_job_specs(struct jobSpecs *, const struct jobSpecs *);
static void free_job_specs(struct jobSpecs *);
static int make_job_dir(struct sbd_job *);
static int materialize_jobfile(struct sbd_job *);
static int set_job_identity(struct sbd_job *);
static int make_jobfile(const struct sbd_job *, char *, size_t);
static int make_job_state_dir(struct sbd_job *);

struct sbd_job *sbd_job_create(const struct jobSpecs *spec)
{
    struct sbd_job *job = calloc(1, sizeof(*job));
    if (job == NULL) {
        LS_ERR("calloc sbd_job failed: %m");
        return NULL;
    }

    // Copy job specifications as received from mbd.
    // These fields remain stable for the lifetime of the job.
    if (copy_job_specs(&job->specs, spec) < 0) {
        LS_ERR("copy_job_specs failed for jobId=%ld %m",
               spec->jobId);
        free(job);
        return NULL;
    }

    /*
     * Explicit initialization of execution and pipeline state.
     * calloc() already zeroed the structure, but we want this to
     * remain correct even if initialization changes in the future.
     */
    job->job_id = spec->jobId;
    job->pid = -1;
    job->pgid = -1;

    memset(&job->lsf_rusage, 0, sizeof(job->lsf_rusage));

    job->reply_last_send = 0;
    job->pid_acked = false;
    job->time_pid_acked = 0;

    job->execute_acked = false;
    job->time_execute_acked = 0;
    job->execute_last_send = 0;

    job->finish_acked = false;
    job->time_finish_acked = 0;
    job->finish_last_send = 0;

    job->exit_status = 0;
    job->exit_status_valid = false;

    // Initialize execution identity fields explicitly
    job->exec_user[0] = 0;
    job->exec_cwd[0] = 0;

    if (build_runtime_spec(job) < 0) {
        LS_ERR("job %ld failed build_runtime_spec", spec->jobId);
        free_job_specs(&job->specs);
        free(job);
        return NULL;
    }

    // Execution invariants: must hold after preparation
    assert(job->exec_user[0] != 0);
    assert(job->specs.subHomeDir[0] != 0);
    assert(job->exec_cwd[0] != 0);

    if (make_job_dir(job) < 0) {
        LS_ERR("failed to make working dir for job %ld", job->job_id);
        free(job);
        return NULL;
    }

    if (materialize_jobfile(job) < 0) {
        LS_ERR("job %ld materialize jobfile failed", job->job_id);
        free(job);
        return NULL;
    }

    if (make_job_state_dir(job) < 0) {
        LS_ERR("job %ld make job state dir failed", job->job_id);
        free(job);
        return NULL;
    }

    return job;
}

/*
 * This function validates mandatory identity fields and reconstructs
 * the execution working directory from the encoded cwd representation
 * received from mbd.
 *
 * CWD encoding rules:
 *   - spec.cwd == ""        -> execution cwd is the user's home directory
 *   - spec.cwd is relative -> execution cwd is subHomeDir + "/" + spec.cwd
 *   - spec.cwd is absolute -> execution cwd is spec.cwd
 */
static int build_runtime_spec(struct sbd_job *job)
{
    if (job->specs.userName[0] == 0) {
        LS_ERR("job %ld missing exec_user (bug)",
               job->job_id);
        errno = EINVAL;
        return -1;
    }

    // subHomeDir is required to decode home-relative cwd encodings
    if (job->specs.subHomeDir[0] == 0) {
        LS_ERR("job %ld missing subHomeDir (bug)",
               job->job_id);
        errno = EINVAL;
        return -1;
    }

    const char *cwd = job->specs.cwd;
    const char *home = job->specs.subHomeDir;
    int n;

    // Decode the cwd encoding into an execution directory path
    // An empty spec.cwd is valid and means "home"
    if (cwd[0] == 0) {
        n = snprintf(job->exec_cwd, sizeof(job->exec_cwd), "%s", home);
        if (n < 0 || n >= (int)sizeof(job->exec_cwd)) {
            LS_ERR("job %ld exec_cwd overflow for home=%s (bug)",
                   job->job_id, home);
            errno = ENAMETOOLONG;
            return -1;
        }

    } else if (cwd[0] == '/') {
        // Absolute cwd: use as-is
        n = snprintf(job->exec_cwd, sizeof(job->exec_cwd), "%s", cwd);
        if (n < 0 || n >= (int)sizeof(job->exec_cwd)) {
            LS_ERR("job %ld exec_cwd overflow for cwd=%s (bug)",
                   job->job_id, cwd);
            errno = ENAMETOOLONG;
            return -1;
        }

    } else {
        // Relative cwd: interpret as relative to user's home directory
        n = snprintf(job->exec_cwd, sizeof(job->exec_cwd),
                     "%s/%s", home, cwd);
        if (n < 0 || n >= (int)sizeof(job->exec_cwd)) {
            LS_ERR("job %ld exec_cwd overflow for home=%s cwd=%s (bug)",
                   job->job_id, home, cwd);
            errno = ENAMETOOLONG;
            return -1;
        }
    }

    if (set_job_identity(job) < 0) {
        LS_ERR("failed to set job %d identity uid %d", job->job_id,
               job->specs.userId);
        return -1;
    }

    ll_strlcpy(job->exec_home, home, PATH_MAX);
    ll_strlcpy(job->jobfile, job->specs.job_file, PATH_MAX);

    return 0;
}

static int set_job_identity(struct sbd_job *job)
{
    struct passwd pwbuf;
    char buf[LL_BUFSIZ_16K];
    struct passwd *pw = NULL;

    int cc = getpwuid_r(job->specs.userId,
                        &pwbuf,
                        buf,
                        sizeof(buf),
                        &pw);
    if (cc != 0) {
        errno = cc;
        LS_ERR("getpwuid_r(uid=%d) failed", job->specs.userId);
        return -1;
    }

    if (pw == NULL) {
        errno = ENOENT;
        LS_ERR("unknown uid=%d", job->specs.userId);
        return -1;
    }

    job->exec_uid = job->specs.userId;
    job->exec_gid = pw->pw_gid;
    ll_strlcpy(job->exec_user, job->specs.userName, LL_BUFSIZ_32);

    LS_INFO("job=%ld exec_uid=%d exec_gid=%d user=%s sbd_debug=%d",
            job->job_id, job->exec_uid, job->exec_gid,
            job->exec_user, sbd_debug);

    return 0;
}

static void free_job_specs(struct jobSpecs *spec)
{
    int i;

    if (spec == NULL)
        return;

    if (spec->toHosts != NULL) {
        for (i = 0; i < spec->numToHosts; i++)
            free(spec->toHosts[i]);
        free(spec->toHosts);
        spec->toHosts = NULL;
    }
    spec->numToHosts = 0;

    if (spec->env != NULL) {
        for (i = 0; i < spec->numEnv; i++)
            free(spec->env[i]);
        free(spec->env);
        spec->env = NULL;
    }
    spec->numEnv = 0;

    free(spec->job_file_data.data);
    spec->job_file_data.data = NULL;
    spec->job_file_data.len = 0;

    free(spec->eexec.data);
    spec->eexec.data = NULL;
    spec->eexec.len = 0;
}

/*
 * Deep-copy helpers for jobSpecs decoded by XDR.
 * Safe for zero-initialized structs.
 */
static int copy_job_specs(struct jobSpecs *dst,
                          const struct jobSpecs *src)
{
    if (src->numToHosts > 0 && src->toHosts == NULL) {
        LS_ERR("numToHosts=%d but toHosts is NULL", src->numToHosts);
        errno = EINVAL;
        return -1;
    }

    if (src->numEnv > 0 && src->env == NULL) {
        LS_ERR("numEnv=%d but env is NULL", src->numEnv);
        errno = EINVAL;
        return -1;
    }

    *dst = *src;

    dst->toHosts = NULL;
    dst->env = NULL;
    dst->job_file_data.data = NULL;
    dst->eexec.data = NULL;

    if (dup_str_array(&dst->toHosts, src->toHosts, src->numToHosts) < 0) {
        LS_ERR("%s: failed to copy toHosts: %m", __func__);
        goto fail;
    }

    if (dup_str_array(&dst->env, src->env, src->numEnv) < 0) {
        LS_ERR("%s: failed to copy env: %m", __func__);
        goto fail;
    }

    if (dup_job_file_data(&dst->job_file_data, &src->job_file_data) < 0) {
        LS_ERR("%s: failed to copy job_file_data: %m", __func__);
        goto fail;
    }

    if (dup_len_data(&dst->eexec, &src->eexec) < 0) {
        LS_ERR("%s: failed to copy eexec: %m", __func__);
        goto fail;
    }

    return 0;

fail:
    free_job_specs(dst);
    return -1;
}

// Job managers
void sbd_new_job(int chfd, XDR *xdrs, struct packet_header *req_hdr)
{
    sbdReplyType reply_code;

    // jobSpecs comes from mb d
    struct jobSpecs spec;
    memset(&spec, 0, sizeof(spec));

    // 1) decode jobSpecs from mbd
    if (!xdr_jobSpecs((XDR *)xdrs, &spec, req_hdr)) {
        LS_ERR("xdr_jobSpecs failed");
        sbd_mbd_shutdown();
        return;
    }

    LS_DEBUG("MBD_NEW_JOB: jobId=%ld job_file=%s", spec.jobId, spec.job_file);

    // 2) duplicate NEW_JOB? just echo our view
    struct sbd_job *job;
    job = sbd_job_lookup(spec.jobId);
    if (job != NULL) {

        LS_WARNING("MBD_NEW_JOB duplicate: job=%ld "
                   "pid=%ld pgid=%ld jStatus=%d",
                   (int64_t)job->job_id,
                   (long)job->pid,
                   (long)job->pgid,
                   (int)job->specs.jStatus);

        if (sbd_enqueue_new_job_reply(job) < 0) {
            LS_ERR("job %ld enqueue duplicate new job reply failed", job->job_id);
            sbd_mbd_shutdown();
        }
        return;
    }

    // allocate sbatchd-local job object
    job = sbd_job_create(&spec);
    if (job == NULL) {
        xdr_lsffree(xdr_jobSpecs, (char *)&spec, req_hdr);
        LS_ERR("MBD_NEW_JOB: job %ld create failed", (long)spec.jobId);
        sbd_mbd_shutdown();
        return;
    }

    // 3) new job: spawn child, register it, fill job_reply
    reply_code = spawn_job(job);
    if (reply_code != ERR_NO_ERROR) {
        // Bug return enqueue to mbd
        assert(0);
    }

    // free heap members inside spec that xdr allocated
    xdr_lsffree(xdr_jobSpecs, (char *)&spec, req_hdr);

    // send the reply to mbd, note the child has been forked and
    // presumed running at this stage
    int cc = sbd_enqueue_new_job_reply(job);
    if (cc < 0) {
        LS_ERR("job %ld enqueue jobReply failed", job->job_id);
        sbd_mbd_shutdown();
        return;
    }

    if (sbd_job_state_write(job) < 0) {
        LS_ERRX("job %ld state write failed at start", job->job_id);
        sbd_mbd_shutdown();
        return;
    }
}

void sbd_new_job_reply_ack(int ch_id, XDR *xdrs, struct packet_header *hdr)
{
    struct job_status_ack ack;
    memset(&ack, 0, sizeof(ack));

    if (!xdr_job_status_ack(xdrs, &ack, hdr)) {
        LS_ERR("xdr_new_job_ack decode failed");
        return;
    }

    // check the status of the operation
    if (hdr->operation != BATCH_NEW_JOB_REPLY_ACK) {
        LS_ERR("job %ld new_job_ack error rc=%d seq=%d",
               ack.job_id, hdr->operation, ack.seq);
        // For now keep job around; retry policy later.
        return;
    }

    // go and retrieve the job_id base in its hash
    struct sbd_job *job = find_job_by_jid(ack.job_id);
    if (job == NULL) {
        LS_WARNING("new_job_ack for unknown job %ld", ack.job_id);
        return;
    }

    // the sequence number is not used for now
    if (job->pid_acked == true) {
        LS_DEBUG("job %ld duplicate pid ack (seq=%d)",
                 job->job_id, ack.seq);
        return;
    }

    // This ack means: mbd has recorded pid/pgid for this job.
    job->pid_acked = true;
    job->time_pid_acked = time(NULL);
    // write the job state
    if (sbd_job_state_write(job) < 0) {
         LS_ERRX("job %ld state write failed after pid_acked",
                 job->job_id);
        // force sbd re transmission of the reply data
         job->reply_last_send = 0;
        return;
    }

    if (sbd_go_write(job) < 0) {
        LS_ERR("job %ld go file write failed", job->job_id);
        sbd_mbd_shutdown();
        return;
    }
    LS_INFO("job %ld go file written", job->job_id);
    job->time_pid_acked = time(NULL);

    // PID/PGID acknowledged by mbd.
    // EXECUTE will be enqueued later by the main loop (job_execute_drive).
    LS_INFO("job %ld pid=%d pgi=%d BATCH_JOB_REPLY acked by mbd", job->job_id,
            job->pid, job->pgid);

    assert(job->execute_acked == false);
}

void sbd_job_execute_ack(int ch_id, XDR *xdrs, struct packet_header *hdr)
{
    if (xdrs == NULL || hdr == NULL) {
        errno = EINVAL;
        return;
    }

    struct job_status_ack ack;
    memset(&ack, 0, sizeof(ack));
    /*
     * ACK payload: mbd echoes the committed stage.
     * Here we expect ack.acked_op == BATCH_JOB_EXECUTE.
     */
    if (!xdr_job_status_ack(xdrs, &ack, hdr)) {
        LS_ERR("xdr_job_status_ack failed for EXECUTE ack");
        return;
    }

    if (ack.acked_op != BATCH_JOB_EXECUTE) {
        LS_ERR("EXECUTE ack mismatch: hdr.op=%d acked_op=%d job=%ld",
               hdr->operation, ack.acked_op, ack.job_id);
        return;
    }

    struct sbd_job *job;
    job = find_job_by_jid(ack.job_id);
    if (job == NULL) {
        /*
         * This can happen after a restart or if the job was already cleaned.
         * Treat as non-fatal: mbd has committed; we just have nothing to do.
         */
        LS_INFO("EXECUTE ack for unknown job=%ld (seq=%d) ignored",
                ack.job_id, ack.seq);
        return;
    }

    if (!job->pid_acked) {
        /*
         * Strict ordering violation: execute_acked implies pid_acked.
         * This should never happen if mbd is enforcing the pipeline.
         */
        LS_ERR("job=%ld BATCH_JOB_EXECUTE acked before PID ack state=0x%x",
               job->job_id);
        return;
    }

    if (job->execute_acked) {
        LS_DEBUG("job=%ld duplicate EXECUTE ack ignored", job->job_id);
        return;
    }

    job->execute_acked = true;
    job->time_execute_acked = time(NULL);

    if (sbd_job_state_write(job) < 0)
        LS_ERRX("job %ld state write failed after execute_acked",
                job->job_id);

    LS_INFO("job=%ld pid=%d pgid=%d op=%s acked mbd", job->job_id,
            job->pid, job->pgid, mbd_op_str(BATCH_JOB_EXECUTE_ACK));

    return;
}

void sbd_job_finish_ack(int ch_id, XDR *xdrs, struct packet_header *hdr)
{
    if (xdrs == NULL || hdr == NULL) {
        errno = EINVAL;
        return;
    }

    struct job_status_ack ack;
    memset(&ack, 0, sizeof(ack));

    if (!xdr_job_status_ack(xdrs, &ack, hdr)) {
        LS_ERR("xdr_job_status_ack failed for FINISH ack");
        return;
    }

    if (ack.acked_op != BATCH_JOB_FINISH) {
        LS_ERR("FINISH ack mismatch: hdr.op=%d acked_op=%d job=%ld",
               hdr->operation, ack.acked_op, ack.job_id);
        return;
    }

    struct sbd_job *job;
    job = find_job_by_jid(ack.job_id);
    if (job == NULL) {
        LS_INFO("FINISH ack for unknown job=%ld ignored", ack.job_id);
        return;
    }

    if (!job->pid_acked || !job->execute_acked) {
        LS_ERR("job=%ld FINISH ack out of order (pid_acked=%d execute_acked=%d)",
               job->job_id, job->pid_acked, job->execute_acked);
        // catch early problems
        assert(0);
        return;
    }

    if (job->finish_acked) {
        LS_DEBUG("job=%ld pid=%d pgid=%d duplicate FINISH ack ignored",
                 job->job_id, job->pid, job->pgid);
        return;
    }

    /*
     * FINISH is only eligible once we have a terminal status locally.
     * If this triggers, it means we emitted FINISH too early or state got lost.
     */
    if (!job->exit_status_valid) {
        LS_ERR("job=%ld pid=%d pgid=%d BATCH_JOB_FINISH acked exit_status "
               "not captured", job->job_id, job->pid, job->pgid);
        abort();
    }

    job->finish_acked = true;
    job->time_finish_acked = time(NULL);
    LS_INFO("job=%ld pid=%d pgid=%d BATCH_JOB_FINISH acked by mbd; cleaning up",
            job->job_id, job->pid, job->pgid);

    // wite the acked
    if (sbd_job_state_write(job) < 0)
        LS_ERRX("job %ld state write failed after finish_acked", job->job_id);

}

int sbd_signal_job(int ch_id, XDR *xdr, struct packet_header *hdr)
{
    if (!xdr || !hdr) {
        lserrno = LSBE_BAD_ARG;
        return -1;
    }

    struct wire_job_sig_req req;
    memset(&req, 0, sizeof(req));
    if (!xdr_wire_job_sig_req(xdr, &req)) {
        LS_ERR("decode BATCH_JOB_SIGNAL failed");
        lserrno = LSBE_XDR;
        return -1;
    }

    struct wire_job_sig_reply rep;
    memset(&rep, 0, sizeof(rep));
    // send back the jobid and signal we are delivering
    rep.job_id = req.job_id;
    rep.sig = req.sig;

    struct sbd_job *job = sbd_job_lookup(req.job_id);
    if (!job) {
        LS_INFO("signal for unknown job_id=%ld", req.job_id);
        rep.rc = LSBE_NO_JOB;
        rep.detail_errno = 0;
        sbd_enqueue_signal_job_reply(ch_id, hdr, &rep);
        return 0;
    }

    // Go and POSIX signal him
    killpg_job(job, req.sig, &rep);

    if (rep.rc == LSBE_NO_ERROR) {
        LS_INFO("signal delivered job_id=%ld sig=%d pid=%d pgid=%d",
                req.job_id, req.sig, job->pid, job->pgid);
    } else {
        LS_ERR("signal failed job_id=%ld sig=%d pid=%d pgid=%d rc=%d errno=%d",
               req.job_id, req.sig, job->pid, job->pgid,
               rep.rc, rep.detail_errno);
    }

    if (sbd_enqueue_signal_job_reply(ch_id, hdr, &rep) < 0)
        return -1;

    LS_INFO("signal enqueued job_id=%ld sig=%d pid=%d pgid=%d",
            req.job_id, req.sig, job->pid, job->pgid);

    return 0;
}

static void killpg_job(struct sbd_job *job, int sig,
                       struct wire_job_sig_reply *rep)
{
    rep->rc = LSBE_NO_ERROR;
    rep->detail_errno = 0;

    if (job->pgid > 0) {
        if (killpg(job->pgid, sig) < 0) {
            // detail_errno is the return code from the system call
            // the rc is the return code that goes to is logged by
            // mbd, the return code that would expected by the library
            rep->detail_errno = errno;
            if (errno == ESRCH)
                rep->rc = LSBE_NO_JOB;
            else
                rep->rc = LSBE_SYS_CALL;
        }
        return;
    }

    if (job->pid <= 0) {
        LS_ERR("signal invariant violated job_id=%ld pid=%d pgid=%d",
               job->job_id, job->pid, job->pgid);
        assert(0);
        rep->rc = LSBE_SYS_CALL;
        rep->detail_errno = EINVAL;
        return;
    }

    if (kill(job->pid, sig) < 0) {
        rep->detail_errno = errno;
        if (errno == ESRCH)
            rep->rc = LSBE_NO_JOB;
        else
            rep->rc = LSBE_SYS_CALL;
        return;
    }
}

static sbdReplyType spawn_job(struct sbd_job *job)
{
    // use posix_spawn
    pid_t pid = fork();
    if (pid < 0) {
        LS_ERR("fork failed for job %ld", job->specs.jobId);
        sbd_job_free(job);
        return ERR_FORK_FAIL;
    }

    if (pid == 0) {
        // child goes and runs the job
        chan_close(sbd_listen_chan);
        chan_close(sbd_timer_chan);
        chan_close(sbd_mbd_chan);
        // Now run... the spec used by the job is a copy
        // of the spec sent by mbd
        child_exec_job(job);
        _exit(127);   /* not reached unless exec fails */
    }

    // parent
    job->pid = pid;
    job->pgid  = pid;

    // register job locally BEFORE telling mbd
    sbd_job_insert(job);

    LS_INFO("spawned job <%d> pid=%d", job->job_id, job->pid);

    return ERR_NO_ERROR;
}


void sbd_job_insert(struct sbd_job *job)
{
    char keybuf[32];
    enum ll_hash_status rc;

    snprintf(keybuf, sizeof(keybuf), "%ld", job->job_id);

    rc = ll_hash_insert(sbd_job_hash, keybuf, job, 0);
    if (rc != LL_HASH_INSERTED) {
        LS_ERR("ll_hash_insert failed for job_id=%d", job->job_id);
        return;
    }

    ll_list_append(&sbd_job_list, &job->list);

    LS_DEBUG("inserted job_id=%d", job->job_id);
}

struct sbd_job *sbd_job_lookup(int job_id)
{
    char keybuf[LL_BUFSIZ_32];

    snprintf(keybuf, sizeof(keybuf), "%d", job_id);
    return ll_hash_search(sbd_job_hash, keybuf);
}

void sbd_job_destroy(struct sbd_job *job)
{
    char keybuf[32];

    snprintf(keybuf, sizeof(keybuf), "%ld", job->job_id);

    ll_hash_remove(sbd_job_hash, keybuf);
    ll_list_remove(&sbd_job_list, &job->list);

    sbd_job_free(job);
}

void sbd_job_free(void *e)
{
    struct sbd_job *job = e;
    if (job == NULL)
        return;

    free(job);
}

static void child_exec_job(struct sbd_job *job)
{
    // child becomes leader of its own group
    setpgid(0, 0);

    struct jobSpecs *specs = &job->specs;
    sbd_child_open_log(specs);

    // Track pid/pgid in the spec for the parent/mbd protocol.
    specs->jobPid = getpid();
    specs->jobPGid = specs->jobPid;

    LS_INFO("job %ld starting: command=<%s> job_file=<%s>",
            job->job_id, specs->command, specs->job_file);

    // Populate the job environment (LSB_*, user env, etc).
    if (set_job_env(job) < 0) {
        LS_ERR("set job env failed for job %ld", job->job_id);
        _exit(127);
    }

    if (cd_work_dir(job) < 0) {
        LS_ERR("job %ld failed to enter cwd %s (and /tmp fallback)",
               job->job_id, job->exec_cwd);
        _exit(127);
    }

    // Drop privileges / set ids first.
    if (set_user_id(job) < 0) {
        LS_ERR("set ids failed job=%ld pid=%d pgid=%d", job->job_id, job->pid,
               job->pgid);
        _exit(127);
    }

    // Apply umask for the job.
    umask(job->specs.umask);

    // Reset signals before running hooks and before exec.
    reset_signals();

    // Queue pre-exec hook (admin-side).
    if (run_qpre(specs) < 0) {
        LS_ERR("qpre failed for job %ld", job->job_id);
        _exit(127);
    }

    // User pre-exec hook (submission-side).
    if ((specs->options & SUB_PRE_EXEC) != 0) {
        if (run_upre(specs) < 0) {
            LS_ERR("upre failed for job %ld", job->job_id);
            _exit(127);
        }
    }

    char jobfile_path[PATH_MAX];
    if (make_jobfile(job, jobfile_path, PATH_MAX) < 0) {
        LS_ERR("job=%ld failed to make_jobfile", job->job_id);
        _exit(127);
    }

    // After stdio redirection, STDERR_FILENO becomes the user's
    // stderr file. Disable stderr mirroring so daemon logs do
    // not leak into job output.
    ls_set_log_to_stderr(0);

    if (redirect_stdio(specs) < 0)
        _exit(127);

    char *argv[2];
    argv[0] = jobfile_path;
    argv[1] = NULL;

    execv(argv[0], argv);

    LS_ERR("job %ld execv(%s) failed: %m",
           job->job_id, argv[0]);
    _exit(127);
}

static int make_job_dir(struct sbd_job *job)
{
    char job_dir[PATH_MAX];

    int l = snprintf(job_dir, sizeof(job_dir),
                     "%s/%s", sbd_job_dir, job->specs.job_file);
    if (l < 0 || l >= (size_t)sizeof(job_dir)) {
        LS_ERR("job %ld job_dir too long", job->job_id);
        return -1;
    }

    if (mkdir(job_dir, 0700) < 0) {
        LS_ERR("job mkdir(%s) failed:", job_dir);
        return -1;
    }

    if (chmod(job_dir, 0700) < 0) {
        LS_ERR("job chmod(%s) failed:", job_dir);
        return -1;
    }

    if (chown(job_dir, job->exec_uid, job->exec_gid) < 0) {
        LS_ERR("chown to uid %d of %s failed", job->exec_uid, job_dir);
        return -1;
    }

    LS_INFO("job=%ld jobdir=%s uid=%d gid=%d", job->job_id, job_dir,
            job->exec_uid, job->exec_gid);

    return 0;
}

static int make_jobfile(const struct sbd_job *job, char *jfile, size_t len)
{
    int l = snprintf(jfile, PATH_MAX, "%s/%s/job.sh", sbd_job_dir,
                     job->jobfile);
    if (l < 0 || (size_t)l >= len) {
        errno = ENAMETOOLONG;
        jfile[0] = 0;
        return -1;
    }

    return l;
}

static int materialize_jobfile(struct sbd_job *job)
{
    // build the job file to execute one here and use it all
    // the wat to execvjob
    // This is what we want to exec
    char jobfile_path[PATH_MAX];
    if (make_jobfile(job, jobfile_path, PATH_MAX) < 0) {
        LS_ERR("job=%ld failed to make_jobfile", job->job_id);
        return -1;
    }

    // LSB_SHAREDIR/work/unixtime.jobid/job.sh
    char tmp[PATH_MAX];
    if (snprintf(tmp, sizeof(tmp), "%s.tmp", jobfile_path) >= PATH_MAX) {
        errno = ENAMETOOLONG;
        return -1;
    }

    int fd = open(tmp, O_WRONLY | O_CREAT | O_TRUNC| O_CLOEXEC, 0600);
    if (fd < 0) {
        // dramma
        LS_ERR("fopen file=%s failed", tmp);
        return -1;
    }

    size_t len = (size_t)job->specs.job_file_data.len;

    if (write_all(fd, job->specs.job_file_data.data, len) < 0) {
        LS_ERR("failed write=%s len=%ld", tmp, len);
        close(fd);
        unlink(tmp);
        return -1;
    }

    if (fsync(fd) < 0) {
        close(fd);
        unlink(tmp);
        return -1;
    }

    if (close(fd) < 0) {
        unlink(tmp);
        return -1;
    }

    if (chmod(tmp, 0700) < 0) {
        unlink(tmp);
        return -1;
    }

    if (chown(tmp, job->exec_uid, job->exec_gid) < 0) {
        LS_ERR("chown to uid %d of %s failed", job->exec_uid, tmp);
        return -1;
    }

    if (rename(tmp, jobfile_path) < 0) {
        unlink(tmp);
        return -1;
    }

    return 0;
}

static int set_job_env(const struct sbd_job *job)
{
    const struct jobSpecs *specs = &job->specs;

    if (setenv("LSB_SUB_HOST", specs->fromHost, 1) < 0)
        return -1;

    char val[LL_BUFSIZ_64];
    snprintf(val, sizeof(val), "%ld", (long)specs->jobId);
    if (setenv("LSB_JOBID", val, 1) < 0)
        return -1;

    char jobdir[PATH_MAX];
    int l = snprintf(jobdir, sizeof(jobdir), "%s/%s",
                     sbd_job_dir, job->jobfile);
    if (l < 0 || (size_t)l >= sizeof(jobdir)) {
        errno = ENAMETOOLONG;
        return -1;
    }

    if (setenv("LSB_JOBDIR", jobdir, 1) < 0)
        return -1;

    if (setenv("LSB_QUEUE", specs->queue, 1) < 0)
        return -1;

    if (setenv("LSB_JOBNAME", specs->jobName, 1) < 0)
        return -1;

    snprintf(val, sizeof(val), "%ld", (long)getpid());
    if (setenv("LSB_JOBPID", val, 1) < 0)
        return -1;

    for (int i = 0; i < specs->numEnv; i++) {
        const char *e = specs->env[i];
        const char *eq;
        size_t klen;
        char key[LL_BUFSIZ_128];

        if (e == NULL)
            continue;

        eq = strchr(e, '=');
        if (eq == NULL)
            continue;

        klen = (size_t)(eq - e);
        if (klen == 0 || klen >= sizeof(key))
            continue;

        memcpy(key, e, klen);
        key[klen] = '\0';

        if (setenv(key, eq + 1, 1) < 0)
            return -1;
    }

    return 0;
}

static int set_user_id(const struct sbd_job *job)
{
    LS_INFO("job=%ld switching to uid=%d gid=%d user=%s sbd_debd=%d",
            job->job_id, job->exec_uid, job->exec_gid, job->exec_user, sbd_debug);

    if (sbd_debug)
        return 0;

    if (initgroups(job->exec_user, job->exec_gid) < 0) {
        LS_ERR("initgroups job=%ld failed uid=%d name=%s group=%d",
               job->job_id, job->exec_uid, job->exec_user, job->exec_gid);
        return -1;
    }

    if (setgid(job->exec_gid) < 0) {
        LS_ERR("setgid job=%ld failed user=%d name=%s group=%d",
               job->job_id, job->exec_uid, job->exec_user, job->exec_gid);
        return -1;
    }

    if (setuid(job->exec_uid) < 0) {
        LS_ERR("setuid job=%ld failed user=%d name=%s group=%d",
               job->job_id, job->exec_uid, job->exec_user, job->exec_gid);
        return -1;
    }

    return 0;
}

static void reset_signals(void)
{
    for (int i = 1; i < NSIG; i++)
        signal(i, SIG_DFL);

    signal(SIGHUP, SIG_IGN);

    sigset_t newmask;
    sigemptyset(&newmask);
    sigprocmask(SIG_SETMASK, &newmask, NULL);

    alarm(0);
}

static int run_qpre(const struct jobSpecs *specs)
{
    (void)specs;
    return 0;
}

static int run_upre(const struct jobSpecs *specs)
{
    (void)specs;
    return 0;
}

static int cd_work_dir(const struct sbd_job *job)
{
    // Attempt to enter the decoded execution working directory
    LS_INFO("job %s: entering work directory %s",
            lsb_jobid2str(job->specs.jobId), job->exec_cwd);

    if (chdir(job->exec_cwd) < 0) {
        // Primary cwd failed: fall back to /tmp
        LS_ERR("job <%s>: chdir(%s) failed, trying /tmp",
               lsb_jobid2str(job->specs.jobId), job->exec_cwd);

        if (chdir("/tmp") < 0) {
            // Fallback also failed: cannot establish a safe working directory
            LS_ERR("job <%s>: chdir(/tmp) failed",
                   lsb_jobid2str(job->specs.jobId));
            return -1;
        }
    }

    return 0;
}

/*
 * Redirect standard input, output, and error for a job.
 *
 * Semantics:
 *  - stdin:
 *      * If an input file was specified (-i), stdin is redirected from it.
 *      * Otherwise, stdin is redirected from /dev/null.
 *
 *  - stdout:
 *      * If an output file was specified (-o), stdout is redirected there.
 *      * Otherwise, stdout is redirected to a file named "stdout" in the
 *        current working directory.
 *
 *  - stderr:
 *      * If an error file was specified (-e), stderr is redirected there.
 *      * Otherwise, stderr is redirected to a file named "stderr" in the
 *        current working directory.
 *
 * Notes:
 *  - The child process must have already chdir()'d into the effective execution
 *    working directory (exec_cwd or fallback /tmp). All relative stdio paths
 *    and default output files are resolved relative to that directory.
 *  - Absolute paths are used as-is.
 *  - Relative paths are resolved against the current process working directory.
 *  - %J and %I are expanded in user-specified stdio file paths.
 *  - Any failure to open or duplicate file descriptors is considered fatal
 *    and causes the function to return an error.
 */
static int redirect_stdio(const struct jobSpecs *specs)
{
    int fd;
    const char *path;
    char expanded[PATH_MAX];

    if (specs == NULL) {
        LS_ERR("sbd_redirect_stdio: specs is NULL (bug)");
        errno = EINVAL;
        return -1;
    }

    LS_INFO("job <%s>: redirecting stdin/stdout/stderr",
            lsb_jobid2str(specs->jobId));

    // Redirect stdin: user-specified input file or /dev/null
    path = "/dev/null";
    if (specs->inFile[0] != 0) {
        if (expand_stdio_path(specs, specs->inFile,
                              expanded, sizeof(expanded)) < 0) {
            LS_ERR("job <%s>: stdin path expansion failed for %s: %m",
                   lsb_jobid2str(specs->jobId), specs->inFile);
            return -1;
        }
        path = expanded;
    }

    fd = open(path, O_RDONLY);
    if (fd < 0) {
        LS_ERR("job <%s>: open(stdin=%s) failed: %m",
               lsb_jobid2str(specs->jobId), path);
        return -1;
    }
    if (dup2(fd, STDIN_FILENO) < 0) {
        LS_ERR("job <%s>: dup2(stdin) failed: %m",
               lsb_jobid2str(specs->jobId));
        close(fd);
        return -1;
    }
    close(fd);

    // Redirect stdout: user-specified output file or "stdout" in cwd
    path = "stdout";
    if (specs->outFile[0] != 0) {
        if (expand_stdio_path(specs, specs->outFile,
                              expanded, sizeof(expanded)) < 0) {
            LS_ERR("job <%s>: stdout path expansion failed for %s: %m",
                   lsb_jobid2str(specs->jobId), specs->outFile);
            return -1;
        }
        path = expanded;
    }

    fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd < 0) {
        LS_ERR("job <%s>: open(stdout=%s) failed: %m",
               lsb_jobid2str(specs->jobId), path);
        return -1;
    }
    if (dup2(fd, STDOUT_FILENO) < 0) {
        LS_ERR("job <%s>: dup2(stdout) failed: %m",
               lsb_jobid2str(specs->jobId));
        close(fd);
        return -1;
    }
    close(fd);

    // Redirect stderr: user-specified error file or "stderr" in cwd
    path = "stderr";
    if (specs->errFile[0] != 0) {
        if (expand_stdio_path(specs, specs->errFile,
                              expanded, sizeof(expanded)) < 0) {
            LS_ERR("job <%s>: stderr path expansion failed for %s: %m",
                   lsb_jobid2str(specs->jobId), specs->errFile);
            return -1;
        }
        path = expanded;
    }

    fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd < 0) {
        LS_ERR("job <%s>: open(stderr=%s) failed: %m",
               lsb_jobid2str(specs->jobId), path);
        return -1;
    }
    if (dup2(fd, STDERR_FILENO) < 0) {
        LS_ERR("job <%s>: dup2(stderr) failed: %m",
               lsb_jobid2str(specs->jobId));
        close(fd);
        return -1;
    }
    close(fd);

    return 0;
}

/*
 * Expand %J and %I in a stdio path template.
 *
 * Supported substitutions:
 *   - %J: replaced with the job ID (decimal)
 *   - %I: replaced with the job ID (decimal) for now (no job arrays yet)
 *
 * Any other %X sequence is copied through as-is.
 *
 * On success, writes a NUL-terminated string to out and returns 0.
 * On failure (overflow), returns -1 with errno set.
 */
static int expand_stdio_path(const struct jobSpecs *specs,
                             const char *tmpl,
                             char *out, size_t outsz)
{
    size_t i;
    size_t pos;

    if (specs == NULL || tmpl == NULL || out == NULL || outsz == 0) {
        LS_ERR("sbd_expand_stdio_path: invalid arguments (bug)");
        errno = EINVAL;
        return -1;
    }

    pos = 0;
    for (i = 0; tmpl[i] != 0; i++) {
        if (tmpl[i] != '%') {
            if (pos + 1 >= outsz) {
                errno = ENAMETOOLONG;
                return -1;
            }
            out[pos++] = tmpl[i];
            continue;
        }

        // '%' at end of string: copy literally
        if (tmpl[i + 1] == 0) {
            if (pos + 1 >= outsz) {
                errno = ENAMETOOLONG;
                return -1;
            }
            out[pos++] = '%';
            continue;
        }

        // Handle supported tokens
        if (tmpl[i + 1] == 'J' || tmpl[i + 1] == 'I') {
            int n = snprintf(out + pos, outsz - pos, "%ld", specs->jobId);
            if (n < 0 || (size_t)n >= outsz - pos) {
                errno = ENAMETOOLONG;
                return -1;
            }
            pos += (size_t)n;
            i++; // consume token char
            continue;
        }

        // Unknown token: copy '%' and the following character literally
        if (pos + 2 >= outsz) {
            errno = ENAMETOOLONG;
            return -1;
        }
        out[pos++] = '%';
        out[pos++] = tmpl[i + 1];
        i++; // consume token char
    }

    if (pos >= outsz) {
        errno = ENAMETOOLONG;
        return -1;
    }

    out[pos] = 0;
    return 0;
}

int write_all(int fd, const char *buf, size_t len)
{
    size_t off = 0;

    while (off < len) {
        ssize_t n = write(fd, buf + off, len - off);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        off += (size_t)n;
    }
    return 0;
}

static struct sbd_job *find_job_by_jid(int64_t job_id)
{
    char job_key[LL_BUFSIZ_32];

    sprintf(job_key, "%ld", job_id);
    struct sbd_job *job = ll_hash_search(sbd_job_hash, job_key);
    if (!job) {
        LS_ERR("job %ld not found in sbd");
        return NULL;
    }
    return job;
}
/*
 * Duplicate an array of strings.
 *
 * This function allocates and deep-copies an array of NUL-terminated
 * strings from the source array into a newly allocated destination
 * array.
 *
 * The destination pointer is set to a newly allocated array of string
 * pointers, each pointing to an independently allocated copy of the
 * corresponding source string.
 *
 * On failure, any partially allocated memory is released and an error
 * is returned.
 */
static int dup_str_array(char ***dstp, char *const *src, int n)
{
    int i;

    *dstp = NULL;

    if (n <= 0 || src == NULL)
        return 0;

    // Allocate one extra slot and NULL-terminate defensively.
    char **v = calloc((size_t)n + 1, sizeof(char *));
    if (v == NULL)
        return -1;

    for (i = 0; i < n; i++) {
        if (src[i] == NULL) {
            v[i] = NULL;
            continue;
        }

        v[i] = strdup(src[i]);
        if (v[i] == NULL) {
            int j;

            for (j = 0; j < i; j++)
                free(v[j]);
            free(v);
            return -1;
        }
    }

    v[n] = NULL;
    *dstp = v;
    return 0;
}

static int dup_len_data(struct lenData *dst, const struct lenData *src)
{
    dst->len = 0;
    dst->data = NULL;

    if (src == NULL)
        return 0;

    if (src->len <= 0 || src->data == NULL)
        return 0;

    dst->data = malloc((size_t)src->len);
    if (dst->data == NULL)
        return -1;

    memcpy(dst->data, src->data, (size_t)src->len);
    dst->len = src->len;
    return 0;
}

static int dup_job_file_data(struct wire_job_file *dst,
                             const struct wire_job_file *src)
{
    dst->len = 0;
    dst->data = NULL;

    if (src == NULL)
        return 0;

    if (src->len <= 0 || src->data == NULL)
        return 0;

    // dont strdup it as there is no 0, job file is no a C string
    dst->data = malloc((size_t)src->len);
    if (dst->data == NULL)
        return -1;

    memcpy(dst->data, src->data, (size_t)src->len);
    dst->len = src->len;
    return 0;
}

static int make_job_state_dir(struct sbd_job *job)
{
    char path[PATH_MAX];

    int l = snprintf(path, sizeof(path),
                     "%s/%s", sbd_state_dir, job->jobfile);
    if (l < 0 || (size_t)l >= sizeof(path)) {
        errno = ENAMETOOLONG;
        return -1;
    }

    if (mkdir(path, 0700) < 0)
        return -1;

    return 0;
}
