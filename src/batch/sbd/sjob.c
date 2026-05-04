/*
 * Copyright (C) LavaLite Contributors
 * GPL v2
 */

#include <string.h>
#include <assert.h>
#include <errno.h>
#include <syslog.h>
#include <fcntl.h>
#include <grp.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "base/lib/auth.h"
#include "base/lib/ll.syslog.h"
#include "batch/lib/rpc.h"
#include "batch/sbd/sbd.h"
#include "batch/lib/wire.h"

static struct sbd_job *sbd_job_create(const struct wire_job_start *ws)
{
    struct sbd_job *job = calloc(1, sizeof(struct sbd_job));
    if (job == NULL) {
        LS_ERR("calloc sbd_job failed");
        return NULL;
    }

    job->job_id   = ws->job_id;
    job->pid      = -1;
    job->pgid     = -1;
    job->exec_uid = (uid_t)ws->uid;
    job->exec_gid = (gid_t)ws->gid;
    job->umask = ws->umask;

    ll_strlcpy(job->exec_user, ws->username, sizeof(job->exec_user));
    ll_strlcpy(job->exec_home, ws->home_dir, sizeof(job->exec_home));
    ll_strlcpy(job->command,   ws->command,  sizeof(job->command));
    ll_strlcpy(job->job_name,  ws->job_name, sizeof(job->job_name));
    ll_strlcpy(job->queue,     ws->queue,    sizeof(job->queue));
    ll_strlcpy(job->hosts,     ws->hosts,    sizeof(job->hosts));
    ll_strlcpy(job->in_file,   ws->in_file,  sizeof(job->in_file));
    ll_strlcpy(job->out_file,  ws->out_file, sizeof(job->out_file));
    ll_strlcpy(job->err_file,  ws->err_file, sizeof(job->err_file));
    if (ws->cwd[0] == '\0')
        ll_strlcpy(job->exec_cwd, ws->home_dir, sizeof(job->exec_cwd));
    else
        ll_strlcpy(job->exec_cwd, ws->cwd, sizeof(job->exec_cwd));

    /* pipeline state */
    job->pid_acked          = FALSE;
    job->time_pid_acked     = 0;
    job->reply_last_send    = 0;
    job->execute_acked      = FALSE;
    job->time_execute_acked = 0;
    job->execute_last_send  = 0;
    job->finish_acked       = FALSE;
    job->time_finish_acked  = 0;
    job->finish_last_send   = 0;
    job->exit_status        = 0;
    job->exit_status_valid  = FALSE;

    /* invariants */
    assert(job->exec_user[0] != 0);
    assert(job->exec_home[0] != 0);
    assert(job->exec_cwd[0]  != 0);

    return job;
}

/* -----------------------------------------------------------------------
 * internal: build, sign and enqueue one message to mbd
 * ----------------------------------------------------------------------- */

static int sbd_send_msg(int32_t op, void *payload, size_t siz,
                        bool_t (*xdr_func)())
{
    struct protocol_header hdr;

    init_protocol_header(&hdr);
    hdr.operation = op;
    hdr.status = MBD_OK;

    if (auth_sign_header(&hdr) < 0) {
        LS_ERR("auth_sign_header failed op=%d", op);
        return -1;
    }

    return sbd_enqueue_payload(sbd_mbd_chan, &hdr, payload, siz, xdr_func);
}

static int sbd_job_new_reply_err(int64_t job_id)
{
    struct wire_job_reply r;

    memset(&r, 0, sizeof(r));
    r.job_id = job_id;
    r.pid    = 0;
    r.pgid   = 0;
    r.status = JOB_STAT_PEND;

    if (sbd_send_msg(BATCH_NEW_JOB_ACK, &r, sizeof(r),
                     (bool_t (*)())xdr_wire_job_reply) < 0) {
        LS_ERR("job=%ld error reply enqueue failed", job_id);
        return -1;
    }
    return 0;
}

static int set_job_env(const struct sbd_job *job)
{
    char val[LL_BUFSIZ_64];
    char jobdir[PATH_MAX];
    char first_host[MAXHOSTNAMELEN];

    /* LL_JOBID */
    snprintf(val, sizeof(val), "%ld", job->job_id);
    if (setenv("LL_JOBID", val, 1) < 0)
        return -1;

    /* LL_JOBPID */
    snprintf(val, sizeof(val), "%d", getpid());
    if (setenv("LL_JOBPID", val, 1) < 0)
        return -1;

    /* LL_JOBDIR */
    int n = snprintf(jobdir, sizeof(jobdir), "%s/%s",
                     sbd_job_dir, job->jobfile);
    if (n < 0 || n >= (int)sizeof(jobdir)) {
        errno = ENAMETOOLONG;
        return -1;
    }
    if (setenv("LL_JOBDIR", jobdir, 1) < 0)
        return -1;

    /* LL_JOBNAME */
    if (setenv("LL_JOBNAME", job->job_name, 1) < 0)
        return -1;

    /* LL_QUEUE */
    if (setenv("LL_QUEUE", job->queue, 1) < 0)
        return -1;

    /* LL_SUB_HOST */
    if (setenv("LL_SUB_HOST", job->from_host, 1) < 0)
        return -1;

    /* LL_FIRST_HOST: the host sbd is running on */
    if (gethostname(first_host, sizeof(first_host)) < 0) {
        LS_ERR("job=%ld gethostname failed: %m", job->job_id);
        return -1;
    }
    if (setenv("LL_FIRST_HOST", first_host, 1) < 0)
        return -1;

    /* LL_HOSTS: scheduler allocation string "hostA 4,hostB 4" */
    if (job->hosts[0] != 0) {
        if (setenv("LL_HOSTS", job->hosts, 1) < 0)
            return -1;
    }

    LS_DEBUG("job=%ld LL_JOBID=%ld LL_JOBPID=%d LL_SUB_HOST=%s "
             "LL_FIRST_HOST=%s LL_QUEUE=%s LL_JOBNAME=%s LL_HOSTS=%s",
             job->job_id, job->job_id, getpid(), job->from_host,
             first_host, job->queue, job->job_name, job->hosts);

    return 0;
}

static int set_user_id(const struct sbd_job *job)
{
    LS_INFO("job=%ld switching to uid=%d gid=%d user=%s sbd_debd=%d",
            job->job_id, job->exec_uid, job->exec_gid, job->exec_user, non_root);

    if (non_root)
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

static int cd_work_dir(const struct sbd_job *job)
{
    // Attempt to enter the decoded execution working directory
    LS_INFO("job %ld: entering work directory %s", job->job_id, job->exec_cwd);

    if (chdir(job->exec_cwd) < 0) {
        // Primary cwd failed fall back to /tmp
        LS_ERR("job=%ld chdir=%s failed, trying /tmp", job->job_id, job->exec_cwd);

        if (chdir("/tmp") < 0) {
            // Fallback also failed cannot establish a safe working directory
            LS_ERR("job=%ld chdir(/tmp) failed", job->job_id);
            return -1;
        }
    }

    return 0;
}
static int expand_stdio_path(int64_t job_id, const char *tmpl,
                             char *out, size_t outsz)
{
    if (tmpl == NULL || out == NULL || outsz == 0) {
        LS_ERR("expand_stdio_path: invalid arguments");
        errno = EINVAL;
        return -1;
    }

    size_t pos = 0;
    for (size_t i = 0; tmpl[i] != 0; i++) {
        if (tmpl[i] != '%') {
            if (pos + 1 >= outsz) {
                errno = ENAMETOOLONG;
                return -1;
            }
            out[pos++] = tmpl[i];
            continue;
        }
        if (tmpl[i + 1] == 0) {
            if (pos + 1 >= outsz) {
                errno = ENAMETOOLONG;
                return -1;
            }
            out[pos++] = '%';
            continue;
        }
        if (tmpl[i + 1] == 'J' || tmpl[i + 1] == 'I') {
            int n = snprintf(out + pos, outsz - pos, "%ld", job_id);
            if (n < 0 || (size_t)n >= outsz - pos) {
                errno = ENAMETOOLONG;
                return -1;
            }
            pos += (size_t)n;
            i++;
            continue;
        }
        if (pos + 2 >= outsz) {
            errno = ENAMETOOLONG;
            return -1;
        }
        out[pos++] = '%';
        out[pos++] = tmpl[i + 1];
        i++;
    }
    if (pos >= outsz) {
        errno = ENAMETOOLONG;
        return -1;
    }
    out[pos] = 0;
    return 0;
}


static int redirect_stdio(const struct sbd_job *job)
{
    LS_INFO("job=%ld redirecting stdin/stdout/stderr", job->job_id);

    /* ---------- stdin ---------- */
    char stdin_path[PATH_MAX];
    snprintf(stdin_path, sizeof(stdin_path), "%s",
             job->in_file[0] ? job->in_file : "/dev/null");

    LS_DEBUG("job=%ld stdin=%s", job->job_id, stdin_path);

    int fd = open(stdin_path, O_RDONLY);
    if (fd < 0) {
        LS_ERR("job=%ld open(stdin=%s) failed: %m", job->job_id, stdin_path);
        return -1;
    }
    if (dup2(fd, STDIN_FILENO) < 0) {
        LS_ERR("job=%ld dup2(stdin) failed: %m", job->job_id);
        close(fd);
        return -1;
    }
    close(fd);

    /* ---------- stdout ---------- */
    char expanded[PATH_MAX];
    char stdout_path[PATH_MAX];
    snprintf(stdout_path, sizeof(stdout_path), "stdout.%ld", job->job_id);
    if (job->out_file[0] != 0) {
        if (expand_stdio_path(job->job_id, job->out_file,
                              expanded, sizeof(expanded)) < 0) {
            LS_ERR("job=%ld stdout path expansion failed file=%s",
                   job->job_id, job->out_file);
            return -1;
        }
        snprintf(stdout_path, sizeof(stdout_path), "%s", expanded);
    }

    /* ---------- stderr ---------- */
    char stderr_path[PATH_MAX];
    snprintf(stderr_path, sizeof(stderr_path), "stderr.%ld", job->job_id);
    if (job->err_file[0] != 0) {
        if (expand_stdio_path(job->job_id, job->err_file,
                              expanded, sizeof(expanded)) < 0) {
            LS_ERR("job=%ld stderr path expansion failed file=%s",
                   job->job_id, job->err_file);
            return -1;
        }
        snprintf(stderr_path, sizeof(stderr_path), "%s", expanded);
    }

    LS_DEBUG("job=%ld stdout=%s stderr=%s", job->job_id, stdout_path,
             stderr_path);

    /* ---------- redirect stdout ---------- */
    fd = open(stdout_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        LS_ERR("job=%ld open(stdout=%s) failed: %m", job->job_id, stdout_path);
        return -1;
    }
    if (dup2(fd, STDOUT_FILENO) < 0) {
        LS_ERR("job=%ld dup2(stdout) failed: %m", job->job_id);
        close(fd);
        return -1;
    }

    /* ---------- redirect stderr ---------- */
    if (strcmp(stdout_path, stderr_path) == 0) {
        if (dup2(fd, STDERR_FILENO) < 0) {
            LS_ERR("job=%ld dup2(stderr) failed: %m", job->job_id);
            close(fd);
            return -1;
        }
        close(fd);
        return 0;
    }
    close(fd);

    fd = open(stderr_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        LS_ERR("job=%ld open(stderr=%s) failed: %m", job->job_id, stderr_path);
        return -1;
    }
    if (dup2(fd, STDERR_FILENO) < 0) {
        LS_ERR("job=%ld dup2(stderr) failed: %m", job->job_id);
        close(fd);
        return -1;
    }
    close(fd);
    return 0;
}

static void child_exec_job(struct sbd_job *job)
{
    char tag[LL_BUFSIZ_64];
    snprintf(tag, sizeof(tag), "child job=%ld", job->job_id);
    // tag the logfile messages saying who we are as we share
    // the log file with parent and other children
    ls_setlogtag(tag);

    LS_INFO("job=%ld starting: command=<%s> job_file=<%s>",
            job->job_id, job->command, job->jobfile);

    // Populate the job environment (LSB_*, user env, etc).
    if (set_job_env(job) < 0) {
        LS_ERR("set job env failed for job=%ld", job->job_id);
        _exit(127);
    }

    if (cd_work_dir(job) < 0) {
        LS_ERR("job=%ld failed to enter cwd %s (and /tmp fallback)",
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
    umask(job->umask);

    // Queue pre-exec hook (admin-side).
    char jobfile_path[PATH_MAX];
    int l = snprintf(jobfile_path, sizeof(jobfile_path), "%s/%s/job.sh",
                     sbd_job_dir, job->jobfile);
    if (l < 0) {
        LS_ERR("create jobfile path buffer %s failed", jobfile_path);
        _exit(127);
    }

    // After stdio redirection, STDERR_FILENO becomes the user's
    // stderr file. Disable stderr mirroring so daemon logs do
    // not leak into job output.
    //ls_set_log_to_stderr(0);

    if (redirect_stdio(job) < 0)
        _exit(127);

    char *argv[2];
    argv[0] = jobfile_path;
    argv[1] = NULL;

    execv(argv[0], argv);

    LS_ERR("job=%ld execv(%s) failed", job->job_id, argv[0]);
    _exit(127);
}

static void reset_except_fd(int except_fd)
{
    DIR *d = opendir("/proc/self/fd");
    if (!d) {
        //* fallback
        long maxfd = sysconf(_SC_OPEN_MAX);
        if (maxfd < 0) maxfd = 1024;
        for (int fd = 3; fd < (int)maxfd; fd++) {
            if (fd != except_fd)
                close(fd);
        }
        return;
    }
    int dfd = dirfd(d);
    struct dirent *e;
    while ((e = readdir(d))) {
        if (e->d_name[0] == '.')
            continue;
        int fd = atoi(e->d_name);
        if (fd >= 3 && fd != except_fd && fd != dfd)
            close(fd);
    }
    closedir(d);
}

static void reset_signals(void)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_DFL;

    for (int i = 1; i < NSIG; i++)
        sigaction(i, &sa, NULL);

    sigset_t newmask;
    sigemptyset(&newmask);
    sigprocmask(SIG_SETMASK, &newmask, NULL);

    alarm(0);
}

static int spawn_job(struct sbd_job *job)
{
    // use posix_spawn
    pid_t pid = fork();
    if (pid < 0) {
        LS_ERR("fork failed for job=%ld", job->job_id);
        return -1;
    }

    if (pid == 0) {
        // child becomes leader of its own group
        setpgid(0, 0);
        job->pid = job->pgid = getgid();

        // child goes and runs the job
        chan_close(sbd_listen_chan);
        chan_close(sbd_timer_chan);
        chan_close(sbd_mbd_chan);
        int log_fd = ls_getlogfd();
        reset_except_fd(log_fd);
        reset_signals();

        // Now run... the spec used by the job is a copy
        // of the spec sent by mbd
        child_exec_job(job);
        _exit(127);   /* not reached unless exec fails */
    }

    // parent
    job->pid = pid;
    job->pgid  = pid;

    LS_INFO("spawned job=%ld pid=%d jobfile=%s command=%s",
            job->job_id, job->pid, job->jobfile, job->command);

    return 0;
}

static int make_job_dir(struct sbd_job *job)
{
    char job_dir[PATH_MAX];

    int l = snprintf(job_dir, sizeof(job_dir),
                     "%s/%s", sbd_job_dir, job->jobfile);
    if (l < 0 || l >= (int)sizeof(job_dir)) {
        LS_ERR("job=%ld job_dir too long", job->job_id);
        return -1;
    }

    if (mkdir(job_dir, 0700) < 0 && errno != EEXIST) {
        LS_ERR("job mkdir(%s) failed", job_dir);
        return -1;
    }

    if (chmod(job_dir, 0700) < 0) {
        LS_ERR("job chmod(%s) failed", job_dir);
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

void sbd_job_new(XDR *xdrs)
{

    struct wire_job_start   ws;
    memset(&ws, 0, sizeof(ws));

    if (!xdr_wire_job_start(xdrs, &ws)) {
        LS_ERRX("xdr_wire_job_start failed");
        /* can't trust job_id, mbd will timeout and requeue */
        return;
    }

    struct wire_job_script  script;
    memset(&script, 0, sizeof(script));
    if (!xdr_wire_job_script(xdrs, &script)) {
        LS_ERRX("job=%ld xdr_wire_job_script failed", ws.job_id);
        sbd_job_new_reply_err(ws.job_id);
        return;
    }

    /* duplicate NEW_JOB: echo our current view back */
    struct sbd_job *job = sbd_job_lookup(ws.job_id);
    if (job != NULL) {
        LS_WARNING("duplicate BATCH_NEW_JOB job=%ld pid=%d",
                   job->job_id, job->pid);
        if (sbd_job_new_reply(job) < 0)
            LS_ERR("job=%ld enqueue duplicate reply failed", job->job_id);
        goto out;
    }

    job = sbd_job_create(&ws);
    if (job == NULL) {
        LS_ERRX("job=%ld sbd_job_create failed", ws.job_id);
        sbd_job_new_reply_err(ws.job_id);
        goto out;
    }

    if (make_job_dir(job) < 0) {
        LS_ERR("job=%ld failed to make working dir", job->job_id);
        free(job);
        goto out;
    }

    if (sbd_job_script_write(job, &script) < 0) {
        LS_ERRX("job=%ld script write failed", ws.job_id);
        sbd_job_new_reply_err(ws.job_id);
        free(job);
        goto out;
    }

    if (spawn_job(job) < 0) {
        LS_ERR("job=%ld spawn failed", ws.job_id);
        sbd_job_new_reply_err(ws.job_id);
        //sbd_job_file_remove(job);
        free(job);
        goto out;
    }

    sbd_job_insert(job);

    if (sbd_job_state_write(job) < 0) {
        LS_ERRX("job=%ld state write failed", job->job_id);
        sbd_fatal(SBD_FATAL_STORAGE);
    }

    if (sbd_job_new_reply(job) < 0) {
        LS_ERR("job=%ld enqueue reply failed", job->job_id);
        return;
    }

    job->reply_last_send = time(NULL);

out:
    xdr_free((xdrproc_t)xdr_wire_job_script,  (char *)&script);
}

void sbd_job_insert(struct sbd_job *job)
{
    char keybuf[32];
    enum ll_hash_status rc;

    snprintf(keybuf, sizeof(keybuf), "%ld", job->job_id);

    rc = ll_hash_insert(sbd_job_hash, keybuf, job, 0);
    if (rc != LL_HASH_INSERTED) {
        LS_ERR("ll_hash_insert failed for job_id=%ld", job->job_id);
        return;
    }

    ll_list_append(&sbd_job_list, &job->list);
}

int sbd_job_script_write(struct sbd_job *job, const struct wire_job_script *script)
{
    char path[PATH_MAX];
    char tmp[PATH_MAX];

    int n = snprintf(path, sizeof(path), "%s/%s/job.sh",
                     sbd_job_dir, job->jobfile);
    if (n < 0 || n >= (int)sizeof(path)) {
        errno = ENAMETOOLONG;
        return -1;
    }

    n = snprintf(tmp, sizeof(tmp), "%s.tmp", path);
    if (n < 0 || n >= (int)sizeof(tmp)) {
        errno = ENAMETOOLONG;
        return -1;
    }

    int fd = open(tmp, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);
    if (fd < 0) {
        LS_ERR("open %s failed: %m", tmp);
        return -1;
    }

    if (write_all(fd, script->data, (size_t)script->len) < 0) {
        LS_ERR("write %s failed: %m", tmp);
        close(fd);
        unlink(tmp);
        return -1;
    }

    if (fsync(fd) < 0) {
        LS_ERR("fsync %s failed: %m", tmp);
        close(fd);
        unlink(tmp);
        return -1;
    }

    if (close(fd) < 0) {
        unlink(tmp);
        return -1;
    }

    if (chmod(tmp, 0700) < 0) {
        LS_ERR("chmod %s failed: %m", tmp);
        unlink(tmp);
        return -1;
    }

    if (chown(tmp, job->exec_uid, job->exec_gid) < 0) {
        LS_ERR("chown uid=%d %s failed: %m", job->exec_uid, tmp);
        unlink(tmp);
        return -1;
    }

    if (rename(tmp, path) < 0) {
        LS_ERR("rename %s -> %s failed: %m", tmp, path);
        unlink(tmp);
        return -1;
    }

    return 0;
}

struct sbd_job *sbd_job_lookup(int64_t job_id)
{
    char job_key[LL_BUFSIZ_32];

    sprintf(job_key, "%ld", job_id);
    struct sbd_job *job = ll_hash_search(sbd_job_hash, job_key);
    if (!job) {
        LS_ERRX("job=%ld not found in sbd", job_id);
        return NULL;
    }
    return job;

}

/* -----------------------------------------------------------------------
 * job new reply  (sbd -> mbd: pid/pgid after fork)
 * ----------------------------------------------------------------------- */

int sbd_job_new_reply(struct sbd_job *job)
{
    struct wire_job_reply r;

    memset(&r, 0, sizeof(r));
    r.job_id = job->job_id;
    r.pid    = job->pid;
    r.pgid   = job->pgid;
    r.status = JOB_STAT_RUN;

    if (sbd_send_msg(BATCH_NEW_JOB_ACK, &r, sizeof(r),
                     (bool_t (*)())xdr_wire_job_reply) < 0) {
        LS_ERR("job=%ld sbd_send_msg failed", job->job_id);
        return -1;
    }

    LS_INFO("job=%ld pid=%d pgid=%d enqueued", job->job_id, job->pid,
            job->pgid);
    return 0;
}

/* -----------------------------------------------------------------------
 * job new ack  (mbd -> sbd: mbd committed pid/pgid)
 * ----------------------------------------------------------------------- */
void sbd_job_new_ack(XDR *xdrs)
{
    struct wire_job_state ack;

    memset(&ack, 0, sizeof(ack));

    if (!xdr_wire_job_state(xdrs, &ack)) {
        LS_ERR("xdr_wire_job_state decode failed");
        return;
    }

    struct sbd_job *job = sbd_job_lookup(ack.job_id);
    if (job == NULL) {
        LS_ERR("job_new_ack: unknown job=%ld", ack.job_id);
        return;
    }

    if (job->pid_acked) {
        LS_DEBUG("job=%ld duplicate pid ack ignored", job->job_id);
        return;
    }

    job->pid_acked      = TRUE;
    job->time_pid_acked = time(NULL);

    if (sbd_job_state_write(job) < 0) {
        LS_ERRX("job=%ld state write failed", job->job_id);
        sbd_fatal(SBD_FATAL_STORAGE);
        return;
    }

    LS_INFO("job=%ld pid_acked", job->job_id);
}

/* -----------------------------------------------------------------------
 * job execute  (sbd -> mbd: job is running)
 * ----------------------------------------------------------------------- */

int sbd_job_execute(struct sbd_job *job)
{
    struct wire_job_state s;

    memset(&s, 0, sizeof(s));
    s.job_id = job->job_id;
    s.state  = JOB_STAT_RUN;

    if (sbd_send_msg(BATCH_JOB_EXECUTE, &s, sizeof(s),
                     (bool_t (*)())xdr_wire_job_state) < 0) {
        LS_ERR("job=%ld sbd_job_execute sbd_send_msg failed", job->job_id);
        return -1;
    }

    LS_INFO("job=%ld execute enqueued", job->job_id);
    return 0;
}

/* -----------------------------------------------------------------------
 * job execute ack  (mbd -> sbd: mbd committed execute)
 * ----------------------------------------------------------------------- */

void sbd_job_execute_ack(XDR *xdrs)
{
    struct wire_job_state ack;

    memset(&ack, 0, sizeof(ack));

    if (!xdr_wire_job_state(xdrs, &ack)) {
        LS_ERR("xdr_wire_job_state decode failed");
        return;
    }

    struct sbd_job *job = sbd_job_lookup(ack.job_id);
    if (job == NULL) {
        LS_ERR("job_execute_ack: unknown job=%ld", ack.job_id);
        return;
    }

    job->execute_acked      = TRUE;
    job->time_execute_acked = time(NULL);
    LS_INFO("job=%ld execute_acked", job->job_id);
}

/* -----------------------------------------------------------------------
 * job finish  (sbd -> mbd: job exited)
 * ----------------------------------------------------------------------- */

int sbd_job_finish(struct sbd_job *job)
{
    struct wire_job_state s;

    memset(&s, 0, sizeof(s));
    s.job_id = job->job_id;
    s.state  = job->exit_status;

    if (sbd_send_msg(BATCH_JOB_FINISH, &s, sizeof(s),
                     (bool_t (*)())xdr_wire_job_state) < 0) {
        LS_ERR("job=%ld sbd_job_finish sbd_send_msg failed", job->job_id);
        return -1;
    }

    LS_INFO("job=%ld finish enqueued exit_status=%d", job->job_id,
            job->exit_status);
    return 0;
}

/* -----------------------------------------------------------------------
 * job finish ack  (mbd -> sbd: mbd committed finish)
 * ----------------------------------------------------------------------- */

void sbd_job_finish_ack(XDR *xdrs)
{
    struct wire_job_state ack;
    memset(&ack, 0, sizeof(ack));

    if (!xdr_wire_job_state(xdrs, &ack)) {
        LS_ERR("xdr_wire_job_state decode failed");
        return;
    }

    struct sbd_job *job = sbd_job_lookup(ack.job_id);
    if (job == NULL) {
        LS_ERR("job_finish_ack: unknown job=%ld", ack.job_id);
        return;
    }

    if (job->finish_acked) {
        LS_DEBUG("job=%ld duplicate finish ack ignored", job->job_id);
        return;
    }

    if (!job->exit_status_valid) {
        LS_ERRX("job=%ld finish ack but exit_status not captured (bug)",
                job->job_id);
        sbd_fatal(SBD_FATAL_INVARIANT);
        return;
    }

    job->finish_acked      = TRUE;
    job->time_finish_acked = time(NULL);

    if (sbd_job_state_write(job) < 0) {
        LS_ERRX("job=%ld state write failed", job->job_id);
        sbd_fatal(SBD_FATAL_STORAGE);
        return;
    }

    sbd_job_file_remove(job);
    sbd_job_state_archive(job);

    char keybuf[LL_BUFSIZ_32];
    snprintf(keybuf, sizeof(keybuf), "%ld", job->job_id);
    ll_hash_remove(sbd_job_hash, keybuf);
    ll_list_remove(&sbd_job_list, &job->list);

    LS_INFO("job=%ld finish_acked cleaned up", job->job_id);
    free(job);
}

/* -----------------------------------------------------------------------
 * job signal  (mbd -> sbd: send signal to job)
 * ----------------------------------------------------------------------- */

int sbd_job_signal(XDR *xdrs)
{
    struct wire_job_sig sig;

    memset(&sig, 0, sizeof(sig));

    if (!xdr_wire_job_sig(xdrs, &sig)) {
        LS_ERR("xdr_wire_job_sig decode failed");
        return -1;
    }

    struct sbd_job *job = sbd_job_lookup(sig.job_id);
    if (job == NULL) {
        LS_ERR("job_signal: unknown job=%ld", sig.job_id);
        return -1;
    }

    if (job->pgid <= 0) {
        LS_ERR("job=%ld signal=%d but pgid not set", job->job_id, sig.sig);
        return -1;
    }

    if (killpg(job->pgid, sig.sig) < 0) {
        LS_ERR("job=%ld killpg pgid=%d sig=%d failed: %m",
               job->job_id, (int)job->pgid, sig.sig);
        return -1;
    }

    LS_INFO("job=%ld sig=%d sent to pgid=%d", job->job_id, sig.sig,
            (int)job->pgid);
    return 0;
}

int sbd_enqueue_job_unknown(int64_t job_id)
{
    struct wire_job_state js;

    memset(&js, 0, sizeof(js));
    js.job_id = job_id;
    js.state  = -1;

    if (sbd_send_msg(BATCH_JOB_UNKNOWN, &js, sizeof(js),
                     (bool_t (*)())xdr_wire_job_state) < 0) {
        LS_ERR("unknown job=%ld enqueue failed", job_id);
        return -1;
    }

    LS_INFO("unknown job=%ld reported to mbd", job_id);
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
