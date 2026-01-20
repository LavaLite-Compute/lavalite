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

static void sbd_handle_new_job(int, XDR *, struct packet_header *);
static sbdReplyType sbd_spawn_job(struct sbd_job *, struct jobReply *);
static void sbd_child_exec_job(struct sbd_job *);
static void sbd_child_open_log(const struct jobSpecs *);
static int sbd_set_job_env(const struct jobSpecs *);
static int sbd_set_ids(const struct jobSpecs *);
static void sbd_reset_signals(void);
static int sbd_run_qpre(const struct jobSpecs *);
static int sbd_run_upre(const struct jobSpecs *);
static int sbd_enter_work_dir(const struct sbd_job *);
static int sbd_redirect_stdio(const struct jobSpecs *);
static int sbd_materialize_jobfile(struct jobSpecs *, const char *,
                                   char *, size_t);
static void sbd_handle_new_job_ack(int, XDR *, struct packet_header *);
static void sbd_handle_job_execute(int, XDR *, struct packet_header *);
static void sbd_handle_job_finish(int, XDR *, struct packet_header *);
static struct sbd_job *sbd_find_job_by_jid(int64_t);
static int sbd_expand_stdio_path(const struct jobSpecs *, const char *,
                                 char *, size_t);

// the ch_id in input is the channel we have opened with mbatchd
//
int sbd_handle_mbd(int ch_id)
{
    struct chan_data *chan = &channels[ch_id];

    LS_DEBUG("processing mbd request");

    if (chan->chan_events == CHAN_EPOLLERR) {
        LS_ERRX("lost connection with mbd on channel %d socket err %d",
                ch_id, chan_sock_error(ch_id));
        sbd_mbd_link_down();
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

    LS_DEBUG("mbd requesting operation %s", batch_op2str(hdr.operation));

    // sbd handler
    switch (hdr.operation) {
    case MBD_NEW_JOB:
        // a new job from mbd has arrived
        sbd_handle_new_job(ch_id, &xdrs, &hdr);
        break;
    case BATCH_NEW_JOB_ACK:
        // this indicate the ack of the previous job_reply
        // has reached the mbd who logged in the events
        // we can send a new event sbd_enqueue_execute
        sbd_handle_new_job_ack(ch_id, &xdrs, &hdr);
        break;
    case BATCH_JOB_EXECUTE:
        sbd_handle_job_execute(ch_id, &xdrs, &hdr);
        break;
    case BATCH_JOB_FINISH:
        sbd_handle_job_finish(ch_id, &xdrs, &hdr);
        break;
    case BATCH_JOB_SIGNAL:
        sbd_handle_signal_job(ch_id, &xdrs, &hdr);
        break;
    case BATCH_SBD_REGISTER_REPLY:
        // informational only; no action required
        LS_INFO("received %s from mbd", batch_op2str(hdr.operation));
        break;
    default:
        break;
    }

    xdr_destroy(&xdrs);
    return 0;
}


static void  sbd_handle_new_job(int chfd, XDR *xdrs,
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
        sbd_mbd_shutdown();
        return;
    }

    LS_DEBUG("MBD_NEW_JOB: jobId=%ld job_file=%s", spec.jobId, spec.job_file);

    // 2) duplicate NEW_JOB? just echo our view
    struct sbd_job *job;
    job = sbd_job_lookup(spec.jobId);
    if (job != NULL) {

        LS_WARNING("MBD_NEW_JOB duplicate: job=%ld state=%d "
                   "pid=%ld pgid=%ld jStatus=%d",
                   (int64_t)job->job_id,
                   (int)job->state,
                   (long)job->pid,
                   (long)job->pgid,
                   (int)job->spec.jStatus);

        job_reply.jobId   = job->job_id;
        job_reply.jobPid  = job->pid;
        job_reply.jobPGid = job->pgid;
        job_reply.jStatus = job->spec.jStatus;
        job->job_reply = job_reply;

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
    reply_code = sbd_spawn_job(job, &job_reply);

    /* free heap members inside spec that xdr allocated */
    xdr_lsffree(xdr_jobSpecs, (char *)&spec, req_hdr);

    // Copy the structures
    job->job_reply = job_reply;
    // This is fundamental for mbd to tell if the job
    // started running or there was some sbd problem
    job->reply_code = reply_code;
    job->spec.runTime = time(NULL);

    // send the reply to mbd, note the child has been forked and
    // presumed running at this stage
    int cc = sbd_enqueue_new_job_reply(job);
    if (cc < 0) {
        LS_ERR("job %ld enqueue jobReply failed", job->job_id);
        sbd_mbd_shutdown();
        return;
    }

    if (sbd_job_record_write(job) < 0) {
        LS_ERRX("job %ld record write failed at start", job->job_id);
        sbd_mbd_shutdown();
        return;
    }
}

static sbdReplyType sbd_spawn_job(struct sbd_job *job, struct jobReply *reply_out)
{
    // use posix_spawn
    pid_t pid = fork();
    if (pid < 0) {
        LS_ERR("fork failed for job %ld", job->spec.jobId);
        sbd_job_free(job);
        return ERR_FORK_FAIL;
    }

    if (pid == 0) {
        // child goes and runs the job
        chan_close(sbd_listen_chan);
        chan_close(sbd_timer_chan);
        chan_close(sbd_mbd_chan);
        // child becomes leader of its own group
        setpgid(0, 0);
        // Now run...
        sbd_child_exec_job(job);
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

/*
 * Execute a job in the sbatchd child process.
 *
 * This function runs exclusively in the forked child created by sbatchd
 * to execute a job. It performs all per-job execution setup that must
 * occur after fork() and before exec().
 *
 * Responsibilities:
 *   - Initialize child-side logging.
 *   - Record job PID and process group ID for protocol reporting.
 *   - Drop privileges and switch to the execution user.
 *   - Construct and populate the job execution environment.
 *   - Apply the job umask.
 *   - Enter the decoded execution working directory (job->exec_cwd),
 *     with a fallback to /tmp if the directory is not accessible.
 *   - Materialize the job wrapper/script under the sbatchd local
 *     execution area.
 *   - Redirect standard input, output, and error according to job
 *     specifications.
 *   - Reset signal handlers to defaults.
 *   - Run administrator and user pre-exec hooks.
 *   - Exec the materialized job script.
 *
 * Any failure during setup is treated as fatal and causes the child
 * process to exit immediately with a non-zero status.
 *
 * This function never returns on success.
 */
static void
sbd_child_exec_job(struct sbd_job *job)
{
    struct jobSpecs *specs = &job->spec;

    sbd_child_open_log(specs);

    // Track pid/pgid in the spec for the parent/mbd protocol.
    specs->jobPid = getpid();
    specs->jobPGid = specs->jobPid;

    LS_INFO("job %ld starting: command=<%s> job_file=<%s>",
            job->job_id, specs->command, specs->job_file);

    LS_INFO("job %ld switching to uid=%d user=%s",
            job->job_id, specs->userId, specs->userName);

    // Drop privileges / set ids first.
    if (sbd_set_ids(specs) < 0) {
        LS_ERR("set ids failed for job %ld, job->job_id");
        _exit(127);
    }

    // Populate the job environment (LSB_*, user env, etc).
    if (sbd_set_job_env(specs) < 0) {
        LS_ERR("set job env failed for job %ld", job->job_id);
        _exit(127);
    }

    // Materialize the job script under the sbatchd local working directory.
    {
        const char *sharedir = lsbParams[LSB_SHAREDIR].paramValue;
        char sbd_root[PATH_MAX];
        char sbd_jfiles[PATH_MAX];
        char sbd_local_wkdir[PATH_MAX];
        char jobpath[PATH_MAX];

        if (snprintf(sbd_root, sizeof(sbd_root), "%s/sbatchd", sharedir) >=
            (int)sizeof(sbd_root)) {
            LS_ERR("job %ld sbatchd root path too long", job->job_id);
            _exit(127);
        }

        if (snprintf(sbd_jfiles, sizeof(sbd_jfiles), "%s/jfiles", sbd_root) >=
            (int)sizeof(sbd_jfiles)) {
            LS_ERR("job %ld sbatchd jfiles path too long", job->job_id);
            _exit(127);
        }

        if (snprintf(sbd_local_wkdir, sizeof(sbd_local_wkdir),
                     "%s/%s", sbd_jfiles, specs->job_file) >=
            (int)sizeof(sbd_local_wkdir)) {
            LS_ERR("job %ld local workdir path too long", job->job_id);
            _exit(127);
        }

        // Create parent directories (ignore EEXIST).
        if (mkdir(sbd_root, 0700) < 0 && errno != EEXIST) {
            LS_ERR("job %ld mkdir(%s) failed: %m",
                   job->job_id, sbd_root);
            _exit(127);
        }

        if (mkdir(sbd_jfiles, 0700) < 0 && errno != EEXIST) {
            LS_ERR("job %ld mkdir(%s) failed: %m",
                   job->job_id, sbd_jfiles);
            _exit(127);
        }

        LS_INFO("job %ld sbd_local_wkdir=<%s>",
                job->job_id, sbd_local_wkdir);

        if (mkdir(sbd_local_wkdir, 0700) < 0 && errno != EEXIST) {
            LS_ERR("job %ld mkdir(%s) failed: %m",
                   job->job_id, sbd_local_wkdir);
            _exit(127);
        }

        if (sbd_materialize_jobfile(specs,
                                    sbd_local_wkdir,
                                    jobpath,
                                    sizeof(jobpath)) < 0) {
            LS_ERR("job %ld materialize jobfile failed: %m",
                   job->job_id);
            _exit(127);
        }

        // Apply requested umask for the job.
        umask(specs->umask);

        // Enter the execution working directory (or /tmp fallback).
        LS_INFO("job %ld child setup starting cwd %s",
                job->job_id, job->exec_cwd);

        if (sbd_enter_work_dir(job) < 0) {
            LS_ERR("job %ld failed to enter cwd %s (and /tmp fallback)",
                   job->job_id, job->exec_cwd);
            _exit(127);
        }

        // Exec the materialized script. We keep cwd as set above.
        LS_INFO("job %ld exec %s",
                job->job_id, jobpath);

        // Reset signals before running hooks and before exec.
        sbd_reset_signals();

        // Queue pre-exec hook (admin-side).
        if (sbd_run_qpre(specs) < 0) {
            LS_ERR("qpre failed for job %ld", job->job_id);
            _exit(127);
        }

        // User pre-exec hook (submission-side).
        if ((specs->options & SUB_PRE_EXEC) != 0) {
            if (sbd_run_upre(specs) < 0) {
                LS_ERR("upre failed for job %ld", job->job_id);
                _exit(127);
            }
        }

        {
            // After stdio redirection, STDERR_FILENO becomes the user's
            // stderr file. Disable stderr mirroring so daemon logs do
            // not leak into job output.
            ls_set_log_to_stderr(0);

            if (sbd_redirect_stdio(specs) < 0)
                _exit(127);

            char *argv[2];

            argv[0] = jobpath;
            argv[1] = NULL;

            execv(argv[0], argv);

            LS_ERR("job %ld execv(%s) failed: %m",
                   job->job_id, argv[0]);
            _exit(127);
        }
    }
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
static void sbd_handle_new_job_ack(int ch_id, XDR *xdrs,
                                   struct packet_header *hdr)
{
    struct job_status_ack ack;
    memset(&ack, 0, sizeof(ack));

    if (!xdr_job_status_ack(xdrs, &ack, hdr)) {
        LS_ERR("xdr_new_job_ack decode failed");
        return;
    }

    // check the status of the operation
    if (hdr->operation != BATCH_NEW_JOB_ACK) {
        LS_ERR("job %ld new_job_ack error rc=%d seq=%d",
               ack.job_id, hdr->operation, ack.seq);
        // For now keep job around; retry policy later.
        return;
    }

    // go and retrieve the job_id base in its hash
    struct sbd_job *job = sbd_find_job_by_jid(ack.job_id);
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
    // write the job record
    if (sbd_job_record_write(job) < 0) {
         LS_ERRX("job %ld record write failed after pid_acked",
                 job->job_id);
        // force sbd re transmission of the reply data
        job->pid_ack_time = 0;
        return;
    }

    if (sbd_go_write(job->job_id) < 0) {
        LS_ERR("job %ld go file write failed", job->job_id);
        sbd_mbd_shutdown();
        return;
    }
    LS_INFO("job %ld go file written", job->job_id);
    job->pid_ack_time = time(NULL);

    // PID/PGID acknowledged by mbd.
    // EXECUTE will be enqueued later by the main loop (job_execute_drive).
    LS_INFO("job %ld pid/pgid acked by mbd", job->job_id);

    assert(job->execute_acked == false);
}

static void sbd_handle_job_execute(int ch_id, XDR *xdrs,
                                   struct packet_header *hdr)
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
    job = sbd_find_job_by_jid(ack.job_id);
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
        LS_ERR("job=%ld BATCH_JOB_EXECUTE ack before PID ack state=0x%x",
               job->job_id, job->state);
        return;
    }

    if (job->execute_acked) {
        LS_DEBUG("job=%ld duplicate EXECUTE ack ignored", job->job_id);
        return;
    }

    job->execute_acked = true;

    if (sbd_job_record_write(job) < 0)
        LS_ERRX("job %ld record write failed after execute_acked",
                job->job_id);

    LS_INFO("job=%ld BATCH_JOB_EXECUTE committed to mbd", job->job_id);

    return;
}

static void sbd_handle_job_finish(int ch_id, XDR *xdrs,
                                  struct packet_header *hdr)
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
    job = sbd_find_job_by_jid(ack.job_id);
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
        LS_DEBUG("job=%ld duplicate FINISH ack ignored", job->job_id);
        return;
    }

    /*
     * FINISH is only eligible once we have a terminal status locally.
     * If this triggers, it means we emitted FINISH too early or state got lost.
     */
    if (!job->exit_status_valid && !job->missing) {
        LS_ERR("job=%ld BATCH_JOB_FINISH committed but exit_status not captured",
               job->job_id);
        abort();
        // later on we can mark this job as missing and continue the
        // clean up process
    }

    job->finish_acked = true;
    LS_INFO("job=%ld BATCH_JOB_FINISH committed by mbd; cleaning up",
            job->job_id);

    // wite the acked
    if (sbd_job_record_write(job) < 0)
        LS_ERRX("job %ld record write failed after finish_acked", job->job_id);

    if (sbd_job_record_remove(job->job_id) < 0)
        LS_ERR("job %ld record remove failed", job->job_id);

    /*
     * Now it is safe to destroy the sbatchd-side job record/spool:
     * mbd event log is the source of truth, and FINISH is committed.
     */
    sbd_job_destroy(job);
}

/* ----------------------------------------------------------------------
 * simple lookup by jobId
 * -------------------------------------------------------------------- */
struct sbd_job *sbd_job_lookup(int job_id)
{
    char keybuf[LL_BUFSIZ_32];

    snprintf(keybuf, sizeof(keybuf), "%d", job_id);
    return ll_hash_search(sbd_job_hash, keybuf);
}

/* ----------------------------------------------------------------------
 * unlink (remove) a job from list + hash
 * does NOT free job memory
 * -------------------------------------------------------------------- */
void sbd_job_destroy(struct sbd_job *job)
{
    char keybuf[32];

    snprintf(keybuf, sizeof(keybuf), "%ld", job->job_id);

    ll_hash_remove(sbd_job_hash, keybuf);
    ll_list_remove(&sbd_job_list, &job->list);

    sbd_job_free(job);
}

/* ----------------------------------------------------------------------
 * free job memory
 * -------------------------------------------------------------------- */
void
sbd_job_free(void *e)
{
    struct sbd_job *job = e;
    if (job == NULL)
        return;

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

static void sbd_child_open_log(const struct jobSpecs *specs)
{
     /*
      * We inherit log_fd and the log configuration from the parent (fork).
      * Do NOT call ls_openlog() here, otherwise you re-open and/or change
      * the log identity and create per-job log files again.
      */
    char tag[LL_BUFSIZ_64];

    snprintf(tag, sizeof(tag),
             "child job=%ld", (long)specs->jobId);
    // tag the logfile messages saying who we are as we share
    // the log file with parent and other children
    ls_setlogtag(tag);
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

    if (setenv("LAVALITE_JOB_ID", val, 1) < 0) {
        return -1;
    }

    if (setenv("LSB_QUEUE", specs->queue, 1) < 0)
        return -1;

    if (setenv("LSB_JOBNAME", specs->jobName, 1) < 0)
        return -1;

    snprintf(val, sizeof(val), "%d", (int)getpid());
    if (setenv("LS_JOBPID", val, 1) < 0)
        return -1;

    if (setenv("LAVALITE_JOB_STATE_DIR", sbd_state_dir, 1) < 0)
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

/*
 * Enter the execution working directory for a job.
 *
 * This function attempts to change the current working directory to the
 * decoded execution directory stored in job->exec_cwd.
 *
 * If the directory is not accessible, it falls back to /tmp, as documented
 * in the job execution semantics.
 *
 * On success, the process current working directory is set either to
 * job->exec_cwd or to /tmp.
 *
 * On failure (both chdir attempts fail), an error is returned and the
 * caller is expected to terminate the child process.
 */
static int
sbd_enter_work_dir(const struct sbd_job *job)
{
    if (job == NULL) {
        LS_ERR("job is NULL (bug)");
        errno = EINVAL;
        return -1;
    }

    // Attempt to enter the decoded execution working directory
    LS_INFO("job <%s>: entering work directory %s",
            lsb_jobid2str(job->spec.jobId), job->exec_cwd);

    if (chdir(job->exec_cwd) < 0) {
        // Primary cwd failed: fall back to /tmp
        LS_ERR("job <%s>: chdir(%s) failed, trying /tmp: %m",
               lsb_jobid2str(job->spec.jobId), job->exec_cwd);

        if (chdir("/tmp") < 0) {
            // Fallback also failed: cannot establish a safe working directory
            LS_ERR("job <%s>: chdir(/tmp) failed: %m",
                   lsb_jobid2str(job->spec.jobId));
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
static int
sbd_redirect_stdio(const struct jobSpecs *specs)
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
        if (sbd_expand_stdio_path(specs, specs->inFile,
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
        if (sbd_expand_stdio_path(specs, specs->outFile,
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
        if (sbd_expand_stdio_path(specs, specs->errFile,
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
static int
sbd_expand_stdio_path(const struct jobSpecs *specs,
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

static int sbd_materialize_jobfile(struct jobSpecs *specs,
                                   const char *work_dir,
                                   char *jobpath,
                                   size_t jobpath_sz)
{
    int fd;
    char tmp[PATH_MAX];
    size_t len;

    if (!specs || !work_dir || !jobpath || jobpath_sz == 0) {
        errno = EINVAL;
        return -1;
    }
    if (!specs->job_file_data.data || specs->job_file_data.len <= 1) {
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

    // jobFileData.len is the size of the blob we got from
    // mnd
    len = (size_t)specs->job_file_data.len;

    if (write_all(fd, specs->job_file_data.data, len) < 0) {
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

/*
 * sbd_mbd_link_down()
 *
 * Handle loss of the mbd connection.
 *
 * This function is the single authority for transitioning sbatchd into
 * "disconnected" state with respect to mbd. It:
 *
 *   - Closes and invalidates the mbd channel.
 *   - Clears per-job "sent" flags (reply/execute/finish) for any protocol
 *     steps that have not yet been ACKed by mbd, making them eligible
 *     for resend once the connection is re-established.
 *   - Persists the updated per-job state immediately to disk so that
 *     resend eligibility survives sbatchd restart.
 *
 * Important invariants:
 *   - ACKed protocol steps (pid_acked, execute_acked, finish_acked) are
 *     never reverted.
 *   - Only non-ACKed "sent" flags are cleared.
 *   - Job records are rewritten for every affected job to keep on-disk
 *     state consistent with in-memory resend logic.
 *
 * This function may be called multiple times; it is idempotent with
 * respect to protocol state.
 */
void
sbd_mbd_link_down(void)
{
    if (sbd_mbd_chan >= 0)
        chan_close(sbd_mbd_chan);

    sbd_mbd_chan = -1;
    sbd_mbd_connecting = false;

    struct ll_list_entry *e;
    for (e = sbd_job_list.head; e; e = e->next) {
        struct sbd_job *job = (struct sbd_job *)e;

        job->reply_last_send = job->execute_last_send
            = job->finish_last_send = 0;
        // Write the latest job record
        if (sbd_job_record_write(job) < 0)
            LS_ERRX("job %ld record write failed after link_down", job->job_id);
    }

    LS_ERR("mbd link down: cleared pending sent flags for resend and record");
}
