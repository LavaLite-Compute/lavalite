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

#include "lsbatch/daemons/sbatchd.h"

extern int sbd_mbd_chan;      /* defined in sbatchd.main.c */

static int sbd_handle_mbd_new_job(int, XDR *, struct packet_header *);
static sbdReplyType sbd_spawn_job(struct jobSpecs *, struct jobReply *);
static void sbd_child_exec_job(struct jobSpecs *);
static void sbd_child_open_log(const struct jobSpecs *);
static int sbd_set_job_env(const struct jobSpecs *);
static int sbd_set_ids(const struct jobSpecs *);
static void sbd_reset_signals(void);
static int sbd_run_qpre(const struct jobSpecs *);
static int sbd_run_upre(const struct jobSpecs *);
static int sbd_make_work_dir(const struct jobSpecs *, char *, size_t);
static int sbd_enter_work_dir(const struct jobSpecs *, const char *);
static int sbd_redirect_stdio(const struct jobSpecs *);
static int sbd_materialize_jobfile(struct jobSpecs *, const char *,
                                   char *, size_t);
static int sbd_handle_mbd_new_job_ack(int, XDR *, struct packet_header *);
static int sbd_handle_mbd_job_execute(int, XDR *, struct packet_header *);
static int sbd_handle_mbd_job_finish(int, XDR *, struct packet_header *);
static struct sbd_job *sbd_find_job_by_jid(int64_t);

// the ch_id in input is the channel we have opened with mbatchd
//
int sbd_handle_mbd(int ch_id)
{
    struct chan_data *chan = &channels[ch_id];

    LS_DEBUG("processing mbd request");

    if (chan->chan_events == CHAN_EPOLLERR) {
        LS_ERR("lost connection with mbd on channel %d", ch_id);
        chan_close(ch_id);
        // change the state of our connections to mbd
        sbd_mbd_chan = -1;
        return -1;
    }

    if (chan->chan_events != CHAN_EPOLLIN) {
        // channel is not ready
        return 0;
    }

    // Get the packet header from the channel first
    struct Buffer *buf;
    if (chan_dequeue(ch_id, &buf) < 0) {
        LS_ERR("chan_dequeue() failed");
        return -1;
    }

    if (!buf || buf->len < PACKET_HEADER_SIZE) {
        LS_ERR("short header from mbd on channel %d: len=%zu",
               ch_id, buf ? buf->len : 0);
        return -1;
    }

    XDR xdrs;
    struct packet_header hdr;
    // Allocate the buffer data based on what was sent
    xdrmem_create(&xdrs, buf->data, buf->len, XDR_DECODE);
    if (!xdr_pack_hdr(&xdrs, &hdr)) {
        LS_ERR("xdr_pack_hdr failed");
        xdr_destroy(&xdrs);
        return -1;
    }

    LS_DEBUG("mbd requesting operation %d", hdr.operation);

    // sbd handler
    switch (hdr.operation) {
    case MBD_NEW_JOB:
        // a new job from mbd has arrived
        sbd_handle_mbd_new_job(ch_id, &xdrs, &hdr);
        break;
    case BATCH_NEW_JOB_ACK:
        // this indicate the ack of the previous job_reply
        // has reached the mbd who logged in the events
        // we can send a new event sbd_enqueue_execute
        sbd_handle_mbd_new_job_ack(ch_id, &xdrs, &hdr);
        break;
    case BATCH_JOB_EXECUTE:
        sbd_handle_mbd_job_execute(ch_id, &xdrs, &hdr);
        break;
    case BATCH_JOB_FINISH:
        sbd_handle_mbd_job_finish(ch_id, &xdrs, &hdr);
        break;
    default:
        break;
    }

    xdr_destroy(&xdrs);
    return 0;
}


int  sbd_handle_mbd_new_job(int chfd, XDR *xdrs,
                            struct packet_header *req_hdr)
{
    sbdReplyType reply_code;

    // jobSpecs comes from mb d
    struct jobSpecs spec;
    memset(&spec, 0, sizeof(spec));

    // reply constructed by sbd
    struct jobReply job_reply;
    memset(&job_reply, 0, sizeof(job_reply));

    // 1) decode jobSpecs from mbd
    if (!xdr_jobSpecs((XDR *)xdrs, &spec, req_hdr)) {
        LS_ERR("xdr_jobSpecs failed");
        reply_code = ERR_BAD_REQ;
        goto send_reply;
    }

    LS_DEBUG("MBD_NEW_JOB: jobId=%ld jobFile=%s", spec.jobId, spec.jobFile);

    // 2) duplicate NEW_JOB? just echo our view
    struct sbd_job *job;
    job = sbd_job_lookup(spec.jobId);
    if (job != NULL) {

        job_reply.jobId   = job->job_id;
        job_reply.jobPid  = job->pid;
        job_reply.jobPGid = job->pgid;
        job_reply.jStatus = job->spec.jStatus;

        reply_code = ERR_NO_ERROR;
        goto send_reply;
    }

    // 3) new job: spawn child, register it, fill job_reply
    reply_code = sbd_spawn_job(&spec, &job_reply);

send_reply:
    /* free heap members inside spec that xdr allocated */
    xdr_lsffree(xdr_jobSpecs, (char *)&spec, req_hdr);

    // send the reply to mbd
    sbd_enqueue_reply(reply_code, &job_reply);
    /*
    int cc = chan_write(sbd_mbd_chan, &job_reply, LL_BUFSIZ_4K);
    if (cc < 0) {
        LS_ERR("chan_write fails");
        return -1;
    }
    */
    return 0;
}

static sbdReplyType sbd_spawn_job(struct jobSpecs *specs,
                                  struct jobReply *reply_out)
{
    // allocate sbatchd-local job object
    struct sbd_job *job = sbd_job_create(specs);
    if (job == NULL) {
        return ERR_MEM;
    }

    // use posix_spawn
    pid_t pid = fork();
    if (pid < 0) {
        LS_ERR("fork failed for job %ld", specs->jobId);
        sbd_job_free(job);
        return ERR_FORK_FAIL;
    }

    if (pid == 0) {
        // child goes and runs the job
        chan_close(sbd_chan);
        chan_close(sbd_timer_chan);
        chan_close(sbd_mbd_chan);
        // child becomes leader of its own group
        setpgid(0, 0);
        // Now run...
        sbd_child_exec_job(specs);
        _exit(127);   /* not reached unless exec fails */
    }

    // parent
    job->pid = pid;
    job->pgid  = pid;
    job->state = SBD_JOB_RUNNING;
    job->spec.jobPid  = pid;
    job->spec.jobPGid = pid;
    job->spec.jStatus = JOB_STAT_RUN;
    job->spec.startTime = 0;

    // register job locally BEFORE telling mbd
    sbd_job_insert(job);

    memset(reply_out, 0, sizeof(*reply_out));
    reply_out->jobId   = job->job_id;
    reply_out->jobPid  = job->pid;
    reply_out->jobPGid = job->pgid;
    reply_out->jStatus = job->spec.jStatus;

    LS_INFO("spawned job <%d> pid=%d", job->job_id, job->pid);

    return ERR_NO_ERROR;
}

static void sbd_child_exec_job(struct jobSpecs *specs)
{
    sbd_child_open_log(specs);

    // Track pid/pgid in the spec for the parent/mbd protocol.
    specs->jobPid = getpid();
    specs->jobPGid = specs->jobPid;

    LS_INFO("job <%s> starting: command=<%s> jobfile=<%s>",
            lsb_jobid2str(specs->jobId),
            specs->command,
            specs->jobFile);

    LS_INFO("job <%s> switching to uid=%d user=%s",
            lsb_jobid2str(specs->jobId),
            specs->userId,
            specs->userName);

    // Drop privileges / set ids first.
    if (sbd_set_ids(specs) < 0) {
        LS_ERR("set ids failed for job <%s>", lsb_jobid2str(specs->jobId));
        _exit(127);
    }

    // Populate the job environment (LSB_*, user env, etc).
    if (sbd_set_job_env(specs) < 0) {
        LS_ERR("set job env failed for job <%s>", lsb_jobid2str(specs->jobId));
        _exit(127);
    }

    // Apply requested umask for the job.
    umask(specs->umask);

    // 1) Enter the execution working directory (today: $HOME/.lsbatch/...).
    {
        char work_dir[PATH_MAX];

        if (sbd_make_work_dir(specs, work_dir, sizeof(work_dir)) < 0)
            _exit(127);

        LS_INFO("job <%s>: child setup starting path %s",
                lsb_jobid2str(specs->jobId), work_dir);

        if (sbd_enter_work_dir(specs, work_dir) < 0)
            _exit(127);
    }

    // 2) Materialize the job script under the sbatchd local working directory
    //    (LSB_SHAREDIR/sbatchd/<unixtime.jobid>/job.sh) and remember its path.
    {
        const char *sharedir = lsbParams[LSB_SHAREDIR].paramValue;
        char sbd_local_wkdir[PATH_MAX];
        char jobpath[PATH_MAX];

        // Create per-job directory under LSB_SHAREDIR (ignore EEXIST).
        // <sharedir>/sbatchd/<specs->jobFile> where jobFile is "unixtime.jobid".
        if (snprintf(sbd_local_wkdir, sizeof(sbd_local_wkdir),
                     "%s/sbatchd/%s", sharedir, specs->jobFile) >=
            (int)sizeof(sbd_local_wkdir)) {
            LS_ERR("job <%s>: local workdir path too long",
                   lsb_jobid2str(specs->jobId));
            _exit(127);
        }

        LS_INFO("job <%s>: sbd_local_wkdir=<%s> jobpath=<%s>",
                lsb_jobid2str(specs->jobId), sbd_local_wkdir, jobpath);

        if (mkdir(sbd_local_wkdir, 0700) < 0 && errno != EEXIST) {
            LS_ERR("job <%s>: mkdir(%s) failed: %s",
                   lsb_jobid2str(specs->jobId),
                   sbd_local_wkdir,
                   strerror(errno));
            _exit(127);
        }

        if (sbd_materialize_jobfile(specs,
                                    sbd_local_wkdir,
                                    jobpath,
                                    sizeof(jobpath)) < 0) {
            LS_ERR("job <%s>: materialize jobfile failed: %s",
                   lsb_jobid2str(specs->jobId),
                   strerror(errno));
            _exit(127);
        }

        // 3) Redirect stdio after jobfile creation so system errors don't land
        //    in user stdout/stderr in early setup.
        if (sbd_redirect_stdio(specs) < 0)
            _exit(127);

        // Reset signals after stdio setup, before running any hooks/exec.
        sbd_reset_signals();

        // Queue pre-exec hook (admin-side).
        if (sbd_run_qpre(specs) < 0) {
            LS_ERR("qpre failed for job <%s>", lsb_jobid2str(specs->jobId));
            _exit(127);
        }

        // User pre-exec hook (submission-side).
        if ((specs->options & SUB_PRE_EXEC) != 0) {
            if (sbd_run_upre(specs) < 0) {
                LS_ERR("upre failed for job <%s>", lsb_jobid2str(specs->jobId));
                _exit(127);
            }
        }

        // Exec the materialized script. We keep cwd as set above.
        LS_INFO("job <%s>: exec /bin/sh %s",
                lsb_jobid2str(specs->jobId), jobpath);

        {
            char *argv[2];

            argv[0] = jobpath;
            argv[1] = NULL;

            execv(argv[0], argv);

            LS_ERR("job <%s>: execv(%s) failed: errno=%d (%s)",
                   lsb_jobid2str(specs->jobId),
                   argv[0],
                   errno,
                   strerror(errno));
            _exit(127);
        }
    }
}

void sbd_job_sync_jstatus(struct sbd_job *job)
{

}

/* ----------------------------------------------------------------------
 * insert job into global list + hash
 * assumes caller already allocated job
 * -------------------------------------------------------------------- */
void
sbd_job_insert(struct sbd_job *job)
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

// Process the BATCH_NEW_JOB_ACK the fact that mbd has received the pid
// and log into the lsb.events
static int sbd_handle_mbd_new_job_ack(int ch_id, XDR *xdrs,
                                      struct packet_header *hdr)
{
    struct job_status_ack ack;
    memset(&ack, 0, sizeof(ack));

    if (!xdr_job_status_ack(xdrs, &ack, hdr)) {
        LS_ERR("xdr_new_job_ack decode failed");
        return -1;
    }

    // check the status of the operation
    if (hdr->operation != BATCH_NEW_JOB_ACK) {
        LS_ERR("job %"PRId64" new_job_ack error rc=%d seq=%d",
               ack.job_id, hdr->operation, ack.seq);
        // For now keep job around; retry policy later.
        return -1;
    }

    // go and retrieve the job_id base in its hash
    struct sbd_job *job = sbd_find_job_by_jid(ack.job_id);
    if (job == NULL) {
        LS_WARNING("new_job_ack for unknown job %"PRId64, ack.job_id);
        return -1;
    }

    // the sequence number is not used for now
    if (job->pid_acked == true) {
        LS_DEBUG("job %"PRId64" duplicate pid ack (seq=%d)",
                 job->job_id, ack.seq);
        return -1;
    }

    // This ack means: mbd has recorded pid/pgid for this job.
    job->pid_acked = true;
    job->pid_ack_time = time(NULL);

    job->step = SBD_STEP_PID_COMMITTED;
    LS_INFO("job %"PRId64" SBD_STEP_PID_COMMITTED pid/pgid acked by mbd seq=%d",
            job->job_id, ack.seq);

    assert(job->execute_acked == false);
    LS_DEBUG("job %"PRId64" BATCH_JOB_EXECUTE enqueue to mbd", job->job_id);

    return 0;
}

static int
sbd_handle_mbd_job_execute(int ch_id, XDR *xdrs, struct packet_header *hdr)
{
    if (xdrs == NULL || hdr == NULL) {
        errno = EINVAL;
        return -1;
    }

    struct job_status_ack ack;
    memset(&ack, 0, sizeof(ack));
    /*
     * ACK payload: mbd echoes the committed stage.
     * Here we expect ack.acked_op == BATCH_JOB_EXECUTE.
     */
    if (!xdr_job_status_ack(xdrs, &ack, hdr)) {
        LS_ERR("xdr_job_status_ack failed for EXECUTE ack");
        return -1;
    }

    if (ack.acked_op != BATCH_JOB_EXECUTE) {
        LS_ERR("EXECUTE ack mismatch: hdr.op=%d acked_op=%d job=%"PRId64,
               hdr->operation, ack.acked_op, ack.job_id);
        return -1;
    }

    struct sbd_job *job;
    job = sbd_find_job_by_jid(ack.job_id);
    if (job == NULL) {
        /*
         * This can happen after a restart or if the job was already cleaned.
         * Treat as non-fatal: mbd has committed; we just have nothing to do.
         */
        LS_INFO("EXECUTE ack for unknown job=%"PRId64" (seq=%d) ignored",
                ack.job_id, ack.seq);
        return 0;
    }

    if (!job->pid_acked) {
        /*
         * Strict ordering violation: execute_acked implies pid_acked.
         * This should never happen if mbd is enforcing the pipeline.
         */
        LS_ERR("job=%"PRId64" EXECUTE ack before PID ack (state=%d step=%d)",
               job->job_id, job->state, job->step);
        return -1;
    }

    if (job->execute_acked) {
        LS_DEBUG("job=%"PRId64" duplicate EXECUTE ack ignored", job->job_id);
        return 0;
    }

    job->execute_acked = TRUE;
    job->step = SBD_STEP_EXECUTE_COMMITTED;

    LS_INFO("job=%"PRId64" BATCH_JOB_EXECUTE committed to mbd", job->job_id);

    return 0;
}

static int
sbd_handle_mbd_job_finish(int ch_id, XDR *xdrs, struct packet_header *hdr)
{
    if (xdrs == NULL || hdr == NULL) {
        errno = EINVAL;
        return -1;
    }

    struct job_status_ack ack;
    memset(&ack, 0, sizeof(ack));

    if (!xdr_job_status_ack(xdrs, &ack, hdr)) {
        LS_ERR("xdr_job_status_ack failed for FINISH ack");
        return -1;
    }

    if (ack.acked_op != BATCH_JOB_FINISH) {
        LS_ERR("FINISH ack mismatch: hdr.op=%d acked_op=%d job=%"PRId64,
               hdr->operation, ack.acked_op, ack.job_id);
        return -1;
    }

    struct sbd_job *job;
    job = sbd_find_job_by_jid(ack.job_id);
    if (job == NULL) {
        LS_INFO("FINISH ack for unknown job=%"PRId64" (seq=%d) ignored",
                ack.job_id, ack.seq);
        return 0;
    }

    if (!job->pid_acked || !job->execute_acked) {
        LS_ERR("job=%"PRId64" FINISH ack out of order (pid_acked=%d execute_acked=%d)",
               job->job_id, job->pid_acked, job->execute_acked);
        return -1;
    }

    if (job->finish_acked) {
        LS_DEBUG("job=%"PRId64" duplicate FINISH ack ignored", job->job_id);
        return 0;
    }

    /*
     * FINISH is only eligible once we have a terminal status locally.
     * If this triggers, it means we emitted FINISH too early or state got lost.
     */
    if (!job->exit_status_valid && !job->missing) {
        LS_ERR("job=%"PRId64" BATCH_JOB_FINISH committed but exit_status not captured",
               job->job_id);
        abort();
        // later on we can mark this job as missing and continue the
        // clean up process
    }

    job->finish_acked = true;
    job->step = SBD_STEP_FINISH_COMMITTED;

    LS_INFO("job=%"PRId64" BATCH_JOB_FINISH committed by mbd; cleaning up",
            job->job_id);

    /*
     * Now it is safe to destroy the sbatchd-side job record/spool:
     * mbd event log is the source of truth, and FINISH is committed.
     */
    sbd_job_free(job);
    return 0;
}

/* ----------------------------------------------------------------------
 * simple lookup by jobId
 * -------------------------------------------------------------------- */
struct sbd_job *
sbd_job_lookup(int job_id)
{
    char keybuf[LL_BUFSIZ_32];

    snprintf(keybuf, sizeof(keybuf), "%d", job_id);
    return ll_hash_search(sbd_job_hash, keybuf);
}


/* ----------------------------------------------------------------------
 * unlink (remove) a job from list + hash
 * does NOT free job memory
 * -------------------------------------------------------------------- */
void
sbd_job_unlink(struct sbd_job *job)
{
    char keybuf[32];

    snprintf(keybuf, sizeof(keybuf), "%ld", job->job_id);

    ll_hash_remove(sbd_job_hash, keybuf);
    ll_list_remove(&sbd_job_list, &job->list);
}


/* ----------------------------------------------------------------------
 * free job memory
 * -------------------------------------------------------------------- */
void
sbd_job_free(struct sbd_job *job)
{
    free(job);
}


/* ----------------------------------------------------------------------
 * foreach wrapper — executes fn(entry)
 * fn must cast entry back to struct sbd_job:
 *
 *   static void dump_job(struct ll_list_entry *e) {
 *       struct sbd_job *j = (struct sbd_job *) e;
 *       LS_INFO("job %d pid=%d state=%d", j->job_id, j->pid, j->state);
 *   }
 *
 *   sbd_job_foreach(dump_job);
 *
 * -------------------------------------------------------------------- */
void
sbd_job_foreach(void (*fn)(struct ll_list_entry *))
{
    ll_list_foreach(&sbd_job_list, fn);
}

#if 0
static const char *
sbd_state_name(enum sbd_job_state st)
{
    switch (st) {
    case SBD_JOB_PENDING:
        return "PENDING";
    case SBD_JOB_RUNNING:
        return "RUNNING";
    case SBD_JOB_EXITED:
        return "EXITED";
    case SBD_JOB_FAILED:
        return "FAILED";
    case SBD_JOB_KILLED:
        return "KILLED";
    default:
        return "UNKNOWN";
    }
}

void sbd_print_all_jobs(void)
{
    LS_INFO("---- current jobs ----");
    sbd_job_foreach(print_one_job);
    LS_INFO("---- end ----");
}
static void
print_one_job(struct ll_list_entry *entry)
{
    struct sbd_job *job;

    job = (struct sbd_job *) entry;

    /* placeholder: use spec.jobFile as job_name
       when you add job_name to sbd_job, replace here */
    const char *job_name;

    if (job->spec.jobFile[0] != '\0') {
        job_name = job->spec.jobFile;
    } else {
        job_name = "<unnamed>";
    }

    LS_INFO("job_id=%d  name=%s  state=%s  pid=%d",
            job->job_id,
            job_name,
            sbd_state_name(job->state),
            (int) job->pid);
}
#endif

static void sbd_child_open_log(const struct jobSpecs *specs)
{
    char daemon_id[LL_BUFSIZ_64];

    snprintf(daemon_id, sizeof(daemon_id), "sbatchd-%ld", (long)specs->jobId);
    ls_openlog(daemon_id,
               genParams[LSF_LOGDIR].paramValue,
               true,
               0,
               "LOG_DEBUG");
}

static int sbd_set_job_env(const struct jobSpecs *specs)
{
    char val[LL_BUFSIZ_64];
    int i;

    if (setenv("LSB_SUB_HOST", specs->fromHost, 1) < 0)
        return -1;

    snprintf(val, sizeof(val), "%ld", (long)specs->jobId);
    if (setenv("LSB_JOBID", val, 1) < 0)
        return -1;

    if (setenv("LSB_QUEUE", specs->queue, 1) < 0)
        return -1;

    if (setenv("LSB_JOBNAME", specs->jobName, 1) < 0)
        return -1;

    snprintf(val, sizeof(val), "%d", (int)getpid());
    if (setenv("LS_JOBPID", val, 1) < 0)
        return -1;

    /*
     * User-supplied environment.
     * Expect entries in KEY=VALUE form.
     */
    for (i = 0; i < specs->numEnv; i++) {
        char *e;
        char *eq;

        e = specs->env[i];
        if (e == NULL)
            continue;

        eq = strchr(e, '=');
        if (eq == NULL)
            continue;

        *eq = '\0';
        if (setenv(e, eq + 1, 1) < 0) {
            *eq = '=';
            return -1;
        }
        *eq = '=';
    }

    return 0;
}

static int sbd_set_ids(const struct jobSpecs *specs)
{

    // Single-user debug mode: sbatchd runs as the submitting user.
    // Do not attempt initgroups/setgid/setuid (will EPERM as non-root).
    // Identity is already the local user in this mode.
    if (sbd_debug)
        return 0;

    struct passwd *pw = getpwnam(specs->userName);
    if (pw == NULL)
        pw = getpwuid((uid_t)specs->userId);
    if (pw == NULL) {
        errno = ENOENT;
        return -1;
    }

    if (initgroups(pw->pw_name, pw->pw_gid) < 0)
        return -1;

    if (setgid(pw->pw_gid) < 0)
        return -1;

    if (setuid((uid_t)specs->userId) < 0)
        return -1;

    return 0;
}

static void sbd_reset_signals(void)
{
    for (int i = 1; i < NSIG; i++)
        signal(i, SIG_DFL);

    signal(SIGHUP, SIG_IGN);

    sigset_t newmask;
    sigemptyset(&newmask);
    sigprocmask(SIG_SETMASK, &newmask, NULL);

    alarm(0);
}

static int sbd_run_qpre(const struct jobSpecs *specs)
{
    (void)specs;
    return 0;
}

static int sbd_run_upre(const struct jobSpecs *specs)
{
    (void)specs;
    return 0;
}

static int sbd_make_work_dir(const struct jobSpecs *specs,
                             char *work_dir, size_t len)
{
    int rc;
    char base[PATH_MAX];

    LS_INFO("job <%s>: creating work directory",
            lsb_jobid2str(specs->jobId));

    if (specs->subHomeDir[0] == '\0') {
        LS_ERR("job <%s>: subHomeDir is empty",
               lsb_jobid2str(specs->jobId));
        errno = ENOENT;
        return -1;
    }

    rc = snprintf(base, sizeof(base), "%s/.lsbatch", specs->subHomeDir);
    if (rc < 0 || (size_t)rc >= sizeof(base)) {
        LS_ERR("job <%s>: .lsbatch path too long",
               lsb_jobid2str(specs->jobId));
        errno = ENAMETOOLONG;
        return -1;
    }

    if (mkdir(base, 0700) < 0 && errno != EEXIST) {
        LS_ERR("job <%s>: mkdir(%s) failed",
               lsb_jobid2str(specs->jobId), base);
        return -1;
    }

    rc = snprintf(work_dir, len, "%s/.lsbatch/%ld",
                  specs->subHomeDir, (long)specs->jobId);
    if (rc < 0 || (size_t)rc >= len) {
        LS_ERR("job <%s>: work dir path too long",
               lsb_jobid2str(specs->jobId));
        errno = ENAMETOOLONG;
        return -1;
    }

    if (mkdir(work_dir, 0700) < 0 && errno != EEXIST) {
        LS_ERR("job <%s>: mkdir(%s) failed",
               lsb_jobid2str(specs->jobId), work_dir);
        return -1;
    }

    LS_INFO("job <%s>: work directory ready: %s",
            lsb_jobid2str(specs->jobId), work_dir);

    return 0;
}

static int sbd_enter_work_dir(const struct jobSpecs *specs,
                              const char *work_dir)
{
    LS_INFO("job <%s>: entering work directory %s",
            lsb_jobid2str(specs->jobId), work_dir);

    if (chdir(work_dir) < 0) {
        LS_ERR("job <%s>: chdir(%s) failed",
               lsb_jobid2str(specs->jobId), work_dir);
        return -1;
    }

    return 0;
}

static int sbd_redirect_stdio(const struct jobSpecs *specs)
{
    int fd;

    LS_INFO("job <%s>: redirecting stdout/stderr to work directory",
            lsb_jobid2str(specs->jobId));

    fd = open("stdout", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        LS_ERR("job <%s>: open(stdout) failed",
               lsb_jobid2str(specs->jobId));
        return -1;
    }
    if (dup2(fd, STDOUT_FILENO) < 0) {
        LS_ERR("job <%s>: dup2(stdout) failed",
               lsb_jobid2str(specs->jobId));
        close(fd);
        return -1;
    }
    close(fd);

    fd = open("stderr", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        LS_ERR("job <%s>: open(stderr) failed",
               lsb_jobid2str(specs->jobId));
        return -1;
    }
    if (dup2(fd, STDERR_FILENO) < 0) {
        LS_ERR("job <%s>: dup2(stderr) failed",
               lsb_jobid2str(specs->jobId));
        close(fd);
        return -1;
    }
    close(fd);

    return 0;
}

static int write_all(int fd, const char *buf, size_t len)
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

static int sbd_materialize_jobfile(struct jobSpecs *specs,
                                   const char *work_dir,
                                   char *jobpath, size_t jobpath_sz)
{
    int fd;
    char tmp[PATH_MAX];
    size_t len;

    if (!specs || !work_dir || !jobpath || jobpath_sz == 0) {
        errno = EINVAL;
        return -1;
    }
    if (!specs->jobFileData.data || specs->jobFileData.len <= 1) {
        errno = EINVAL;
        return -1;
    }

    if (snprintf(jobpath, jobpath_sz, "%s/job.sh", work_dir) >= (int)jobpath_sz) {
        errno = ENAMETOOLONG;
        return -1;
    }
    if (snprintf(tmp, sizeof(tmp), "%s.tmp", jobpath) >= (int)sizeof(tmp)) {
        errno = ENAMETOOLONG;
        return -1;
    }

    fd = open(tmp, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd < 0)
        return -1;

    // jobFileData.len includes trailing 0; don't write it
    len = (size_t)specs->jobFileData.len - 1;

    if (write_all(fd, specs->jobFileData.data, len) < 0) {
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

    if (rename(tmp, jobpath) < 0) {
        unlink(tmp);
        return -1;
    }

    return 0;
}

static struct sbd_job *sbd_find_job_by_jid(int64_t job_id)
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
