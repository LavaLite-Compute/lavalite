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

static int dup_str_array(char ***, char *const *, int);
static int dup_len_data(struct lenData *, const struct lenData *);
static int sbd_job_prepare_exec_fields(struct sbd_job *);

/*
 * Create and initialize a new sbatchd job object from a received jobSpecs.
 *
 * This function allocates a struct sbd_job, performs a deep copy of the
 * job specification as received from mbd, initializes all sbatchd-local
 * execution and pipeline state, and prepares derived execution fields
 * (such as decoded execution working directory).
 *
 * On success, the returned job object is fully initialized, validated,
 * and ready to be inserted into sbatchd internal data structures.
 *
 * On failure, all allocated resources are released and NULL is returned.
 *
 * This function does not start execution, spawn processes, or change
 * working directory; it only prepares the in-memory job representation.
 */
struct sbd_job *sbd_job_create(const struct jobSpecs *spec)
{
    struct sbd_job *job = calloc(1, sizeof(*job));
    if (job == NULL) {
        LS_ERR("calloc sbd_job failed: %m");
        return NULL;
    }

    // Copy job specifications as received from mbd.
    // These fields remain stable for the lifetime of the job.
    if (jobSpecs_deep_copy(&job->spec, spec) < 0) {
        LS_ERR("jobSpecs_deep_copy failed for jobId=%"PRId64": %m",
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

    memset(&job->job_reply, 0, sizeof(job->job_reply));
    memset(&job->lsf_rusage, 0, sizeof(job->lsf_rusage));

    job->state = SBD_JOB_NEW;
    job->reply_code = ERR_NO_ERROR;
    job->reply_sent = false;

    job->pid_acked = false;
    job->pid_ack_time = 0;
    job->execute_acked = false;
    job->execute_sent = false;
    job->finish_acked = false;
    job->finish_sent = false;

    job->exit_status = 0;
    job->exit_status_valid = false;

    job->missing = false;
    job->step = SBD_STEP_NONE;

    // Initialize execution identity fields explicitly
    job->exec_username[0] = 0;
    job->exec_cwd[0] = 0;

    strlcpy(job->exec_username, spec->userName,
            sizeof(job->exec_username));

    // Prepare decoded execution fields (cwd reconstruction, validation).
    // This must succeed for the job to be runnable.
    if (sbd_job_prepare_exec_fields(job) < 0) {
        LS_ERR("job %"PRId64" failed to prepare execution fields: %m",
               spec->jobId);
        jobSpecs_free(&job->spec);
        free(job);
        return NULL;
    }

    // Execution invariants: must hold after preparation
    assert(job->exec_username[0] != 0);
    assert(job->spec.subHomeDir[0] != 0);
    assert(job->exec_cwd[0] != 0);

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

/*
 * Duplicate a length-prefixed data buffer.
 *
 * This function deep-copies the contents of a lenData structure,
 * including allocation of a new data buffer of the specified length.
 *
 * The destination lenData is fully independent from the source and
 * may be safely modified or freed without affecting the source.
 *
 * On failure, no partial state is left in the destination.
 */
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

/*
 * Release all dynamically allocated resources associated with a jobSpecs.
 *
 * This function frees any heap-allocated members contained within the
 * jobSpecs structure (such as string arrays and data buffers) and
 * resets the structure to a safe state.
 *
 * The jobSpecs structure itself is not freed.
 *
 * It is safe to call this function on a partially initialized jobSpecs.
 */
void jobSpecs_free(struct jobSpecs *spec)
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

    free(spec->jobFileData.data);
    spec->jobFileData.data = NULL;
    spec->jobFileData.len = 0;

    free(spec->eexec.data);
    spec->eexec.data = NULL;
    spec->eexec.len = 0;
}

/*
 * Deep-copy helpers for jobSpecs decoded by XDR.
 * Safe for zero-initialized structs.
 */
int jobSpecs_deep_copy(struct jobSpecs *dst, const struct jobSpecs *src)
{
    if (dst == NULL || src == NULL) {
        LS_ERR("%s: invalid arguments", __func__);
        errno = EINVAL;
        return -1;
    }

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
    dst->jobFileData.data = NULL;
    dst->eexec.data = NULL;

    if (dup_str_array(&dst->toHosts, src->toHosts, src->numToHosts) < 0) {
        LS_ERR("%s: failed to copy toHosts: %m", __func__);
        goto fail;
    }

    if (dup_str_array(&dst->env, src->env, src->numEnv) < 0) {
        LS_ERR("%s: failed to copy env: %m", __func__);
        goto fail;
    }

    if (dup_len_data(&dst->jobFileData, &src->jobFileData) < 0) {
        LS_ERR("%s: failed to copy jobFileData: %m", __func__);
        goto fail;
    }

    if (dup_len_data(&dst->eexec, &src->eexec) < 0) {
        LS_ERR("%s: failed to copy eexec: %m", __func__);
        goto fail;
    }

    return 0;

fail:
    jobSpecs_free(dst);
    return -1;
}

/*
 * Initialize the base directory used for sbatchd job records.
 *
 * This function ensures that the directory hierarchy used to persist
 * sbatchd job records exists and is ready for use.
 *
 * It is intended to be called once during sbatchd startup.
 *
 * On failure, sbatchd should treat this as a fatal initialization error.
 */

char sbd_state_dir[PATH_MAX];

int sbd_job_record_dir_init(void)
{
    const char *sharedir;
    char sbd_root[PATH_MAX];
    struct stat st;

    sharedir = lsbParams[LSB_SHAREDIR].paramValue;
    if (!sharedir || sharedir[0] == '\0') {
        errno = EINVAL;
        LS_ERR("sbd_init_state: LSB_SHAREDIR is not set");
        return -1;
    }

    if (snprintf(sbd_root, sizeof(sbd_root), "%s/sbatchd", sharedir) >=
        (int)sizeof(sbd_root)) {
        errno = ENAMETOOLONG;
        LS_ERR("sbd_init_state: sbatchd root path too long");
        return -1;
    }

    if (snprintf(sbd_state_dir, sizeof(sbd_state_dir), "%s/state", sbd_root) >=
        (int)sizeof(sbd_state_dir)) {
        errno = ENAMETOOLONG;
        LS_ERR("sbd_init_state: state dir path too long");
        return -1;
    }

    // Create <sharedir>/sbatchd and <sharedir>/sbatchd/state (ignore EEXIST).
    if (mkdir(sbd_root, 0700) < 0 && errno != EEXIST) {
        LS_ERR("sbd_init_state: mkdir(%s) failed: %s",
               sbd_root, strerror(errno));
        return -1;
    }

    if (mkdir(sbd_state_dir, 0700) < 0 && errno != EEXIST) {
        LS_ERR("sbd_init_state: mkdir(%s) failed: %s",
               sbd_state_dir, strerror(errno));
        return -1;
    }

    if (stat(sbd_state_dir, &st) < 0) {
        LS_ERR("sbd_init_state: stat(%s) failed: %s",
               sbd_state_dir, strerror(errno));
        return -1;
    }

    if (!S_ISDIR(st.st_mode)) {
        errno = ENOTDIR;
        LS_ERR("sbd_init_state: %s exists but is not a directory",
               sbd_state_dir);
        return -1;
    }

    LS_INFO("sbd state dir: %s", sbd_state_dir);
    return 0;
}

/*
 * Construct the filesystem path for a sbatchd job record directory.
 *
 * This function formats the path corresponding to the given job ID
 * into the provided buffer.
 *
 * The resulting path identifies the directory or file location used
 * by sbatchd to store persistent job state for the specified job.
 *
 * The buffer must be large enough to hold the full path including
 * the terminating NUL character.
 *
 * On success, the buffer contains a NUL-terminated path string.
 */
int sbd_job_record_path(int64_t job_id, char *buf, size_t bufsz)
{
    if (!buf || bufsz == 0) {
        errno = EINVAL;
        return -1;
    }
    if (sbd_state_dir[0] == '\0') {
        errno = EINVAL;
        return -1;
    }

    // <state>/job.<jobid>
    if (snprintf(buf, bufsz, "%s/job.%"PRId64, sbd_state_dir, job_id) >=
        (int)bufsz) {
        errno = ENAMETOOLONG;
        return -1;
    }

    LS_INFO("job record %s", buf);

    return 0;
}

int sbd_job_record_remove(int64_t job_id)
{
    char path[PATH_MAX];

    LS_INFO("removing job %ld", job_id);

    if (sbd_job_record_path(job_id, path, sizeof(path)) < 0) {
        LS_ERR("failed to remove path %s for job %ld", path, job_id);
        return -1;
    }

    if (unlink(path) < 0)
        return -1;

    return 0;
}

int sbd_job_record_write(struct sbd_job *job)
{
    if (!job) {
        errno = EINVAL;
        LS_ERRX("sbd_job_record_write: invalid job pointer");
        return -1;
    }

    char final_path[PATH_MAX];
    if (sbd_job_record_path(job->job_id, final_path, sizeof(final_path)) < 0) {
        LS_ERRX("job %"PRId64": record path build failed", job->job_id);
        return -1;
    }

    char tmp_path[PATH_MAX];
    if (snprintf(tmp_path, sizeof(tmp_path), "%s.tmp.%ld",
                 final_path, (long)getpid()) >= (int)sizeof(tmp_path)) {
        errno = ENAMETOOLONG;
        LS_ERRX("job %"PRId64": record tmp path too long", job->job_id);
        return -1;
    }

    LS_INFO("job %"PRId64": record write start: %s", job->job_id, final_path);

    char buf[LL_BUFSIZ_2K];
    int n = snprintf(buf, sizeof(buf),
                     "version=1\n"
                     "job_id=%ld\n"
                     "pid=%ld\n"
                     "pgid=%ld\n"
                     "state=%d\n"
                     "step=%d\n"
                     "reply_sent=%d\n"
                     "pid_acked=%d\n"
                     "execute_sent=%d\n"
                     "execute_acked=%d\n"
                     "finish_sent=%d\n"
                     "finish_acked=%d\n"
                     "exit_status_valid=%d\n"
                     "exit_status=%d\n"
                     "end_time=%ld\n"
                     "missing=%d\n",
                     job->job_id,
                     (long)job->pid,
                     (long)job->pgid,
                     (int)job->state,
                     (int)job->step,
                     job->reply_sent ? 1 : 0,
                     job->pid_acked ? 1 : 0,
                     job->execute_sent ? 1 : 0,
                     job->execute_acked ? 1 : 0,
                     job->finish_sent ? 1 : 0,
                     job->finish_acked ? 1 : 0,
                     job->exit_status_valid ? 1 : 0,
                     job->exit_status,
                     job->end_time,
                     job->missing ? 1 : 0);

    if (n < 0) {
        errno = EINVAL;
        LS_ERRX("job %"PRId64": record format failed", job->job_id);
        return -1;
    }
    if ((size_t)n >= sizeof(buf)) {
        errno = ENAMETOOLONG;
        LS_ERRX("job %"PRId64": record buffer too small", job->job_id);
        return -1;
    }

    int fd = open(tmp_path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd < 0) {
        LS_ERRX("job %"PRId64": open(%s) failed: errno=%d (%s)",
                job->job_id, tmp_path, errno, strerror(errno));
        return -1;
    }

    // Typicall C pattern for reliability writing, the rename
    // is atomic on POSIX systems
    // write to temp file → fsync() → close → rename(temp, final)

    if (write_all(fd, buf, (size_t)n) < 0) {
        int eno = errno;
        close(fd);
        unlink(tmp_path);
        errno = eno;
        LS_ERRX("job %"PRId64": write(%s) failed: errno=%d (%s)",
                job->job_id, tmp_path, errno, strerror(errno));
        return -1;
    }

    if (fsync(fd) < 0) {
        int eno = errno;
        close(fd);
        unlink(tmp_path);
        errno = eno;
        LS_ERRX("job %"PRId64": fsync(%s) failed: errno=%d (%s)",
                job->job_id, tmp_path, errno, strerror(errno));
        return -1;
    }

    if (close(fd) < 0) {
        int eno = errno;
        unlink(tmp_path);
        errno = eno;
        LS_ERRX("job %"PRId64": close(%s) failed: errno=%d (%s)",
                job->job_id, tmp_path, errno, strerror(errno));
        return -1;
    }

    if (rename(tmp_path, final_path) < 0) {
        int eno = errno;
        unlink(tmp_path);
        errno = eno;
        LS_ERRX("job %"PRId64": rename(%s -> %s) failed: errno=%d (%s)",
                job->job_id, tmp_path, final_path, errno, strerror(errno));
        return -1;
    }

    LS_INFO("job %"PRId64": record written "
            "pid=%ld pgid=%ld "
            "state=%d step=%d "
            "reply_sent=%d pid_acked=%d "
            "execute_sent=%d finish_sent=%d "
            "execute_acked=%d finish_acked=%d "
            "exit_status_valid=%d exit_status=%d "
            "end_time=%ld "
            "missing=%d",
            job->job_id,
            (long)job->pid,
            (long)job->pgid,
            (int)job->state,
            (int)job->step,
            job->reply_sent ? 1 : 0, job->pid_acked ? 1 : 0,
            job->execute_sent ? 1 : 0, job->finish_sent ? 1 : 0,
            job->execute_acked ? 1 : 0, job->finish_acked ? 1 : 0,
            job->exit_status_valid ? 1 : 0, job->exit_status,
            job->end_time,
            job->missing ? 1 : 0);

    return 0;
}

int sbd_job_record_load_all(void)
{
    DIR *dir;
    dir = opendir(sbd_state_dir);
    if (!dir) {
        LS_ERR("opendir(%s) failed", sbd_state_dir);
        return -1;
    }

    int count = 0;
    struct dirent *de;
    while ((de = readdir(dir)) != NULL) {
        int64_t job_id;
        struct sbd_job *job;

        if (strncmp(de->d_name, "job.", 4) != 0)
            continue;

        const char *s = de->d_name + 4;
        char *end = NULL;

        errno = 0;
        job_id = strtoll(s, &end, 10);
        if (errno != 0 || end == s || *end != 0 || job_id <= 0)
            continue;

        job = calloc(1, sizeof(*job));
        if (!job) {
            LS_ERR("calloc failed while loading job %"PRId64, job_id);
            continue;
        }

        job->job_id = job_id;

        if (sbd_job_record_read(job_id, job) < 0) {
            LS_ERR("failed to read job record for job %"PRId64, job_id);
            free(job);
            continue;
        }

        /*
         * At this point:
         *  - job contains last known sbatchd-local state
         *  - mbd is the source of truth; this is only to survive restart
         */
        sbd_job_insert(job);
        count++;
    }

    closedir(dir);

    LS_INFO("loaded %d job(s) from sbd state directory", count);
    return 0;
}

int
sbd_job_record_read(int64_t job_id, struct sbd_job *job)
{
    char path[PATH_MAX];
    FILE *fp;
    char line[LL_BUFSIZ_512];
    int version = 0;
    int64_t file_job_id = 0;

    if (!job) {
        errno = EINVAL;
        return -1;
    }

    if (sbd_job_record_path(job_id, path, sizeof(path)) < 0) {
        errno = EINVAL;
        return -1;
    }

    fp = fopen(path, "r");
    if (!fp)
        return -1;

    while (fgets(line, sizeof(line), fp) != NULL) {
        char *eq = strchr(line, '=');
        char *key;
        char *val;

        if (!eq)
            continue;

        *eq = 0;
        key = line;
        val = eq + 1;

        if (strcmp(key, "version") == 0) {
            version = atoi(val);
            continue;
        }

        if (strcmp(key, "job_id") == 0) {
            file_job_id = (int64_t)strtoll(val, NULL, 10);
            continue;
        }

        if (strcmp(key, "pid") == 0) {
            job->pid = (pid_t)atol(val);
            job->spec.jobPid = (int)job->pid;
            continue;
        }

        if (strcmp(key, "pgid") == 0) {
            job->pgid = (pid_t)atol(val);
            job->spec.jobPGid = (int)job->pgid;
            continue;
        }

        if (strcmp(key, "state") == 0) {
            job->state = (enum sbd_job_state)atoi(val);
            continue;
        }

        if (strcmp(key, "step") == 0) {
            job->step = (enum sbd_job_step)atoi(val);
            continue;
        }

        if (strcmp(key, "pid_acked") == 0) {
            job->pid_acked = atoi(val);
            continue;
        }

        if (strcmp(key, "reply_sent") == 0) {
            job->reply_sent = atoi(val);
            continue;
        }

        if (strcmp(key, "execute_sent") == 0) {
            job->execute_sent = atoi(val);
            continue;
        }

        if (strcmp(key, "execute_acked") == 0) {
            job->execute_acked = atoi(val);
            continue;
        }

        if (strcmp(key, "finish_sent") == 0) {
            job->finish_sent = atoi(val);
            continue;
        }

        if (strcmp(key, "finish_acked") == 0) {
            job->finish_acked = atoi(val);
            continue;
        }

        if (strcmp(key, "exit_status_valid") == 0) {
            job->exit_status_valid = atoi(val);
            continue;
        }

        if (strcmp(key, "exit_status") == 0) {
            job->exit_status = atoi(val);
            continue;
        }

        if (strcmp(key, "missing") == 0) {
            job->missing = atoi(val);
            continue;
        }
    }

    fclose(fp);

    if (version != 1) {
        errno = EINVAL;
        return -1;
    }

    if (file_job_id != job_id) {
        errno = EINVAL;
        return -1;
    }

    job->job_id = job_id;
    job->spec.jobId = job_id;

    return 0;
}

// After we replayed the jobs some jobs have been ack finished
// already we dont want to keep those job in the working list.
// Do it here so we separate the responsibilities between reading
// the job from disk and the list/hash cleaning.
//
// ensure jobs with finish_acked set never reach job_finish_drive
void sbd_cleanup_job_list(void)
{
    struct ll_list_entry *e;
    struct ll_list_entry *e2;

    for (e = sbd_job_list.head; e;) {
        e2 = e->next;
        struct sbd_job *job = (struct sbd_job *)e;

        // the status finished was not deliver to mbd
        // so in the main loop we check if the pid
        // is still around
        if (! job->finish_acked) {
            assert(job->finish_sent == 0);
            assert(job->exit_status_valid == false);
            // advance
            e = e2;
            continue;
        }
        // this means we delivered the job status exit
        // to mbatch alread, but we keep the file around
        // for some times before cleaning it
        LS_INFO("job %ld was acked to mbd already", job->job_id);
        assert(job->finish_sent == 1);
        assert(job->exit_status_valid == true);
        // remove the job from the disk and free it from memory
        // later we may want to keep the job around for some time...
        // perhaps...
        sbd_job_record_remove(job->job_id);
        sbd_job_destroy(job);
        // next!
        e = e2;
    }
}

int sbd_read_exit_status_file(int job_id, int *exit_code, time_t *done_time)
{
    char path[PATH_MAX];
    char buf[LL_BUFSIZ_128];
    int fd;
    ssize_t n;
    int code;
    // time_t may not be an int
    int64_t ts;

    int l =  snprintf(path, sizeof(path),
                      "%s/exit.status.%d",
                      sbd_state_dir, job_id);
    if (l < 0 || (size_t)l >= sizeof(path)) {
        errno = ENAMETOOLONG;
        return -1;
    }

    fd = open(path, O_RDONLY);
    if (fd < 0)
        return -1;

    n = read(fd, buf, sizeof(buf) - 1);
    close(fd);

    if (n <= 0)
        return -1;

    buf[n] = 0;

    if (sscanf(buf, "%d %ld", &code, &ts) != 2)
        return -1;

    if (exit_code)
        *exit_code = code;

    if (done_time)
        *done_time = (time_t)ts;

    return 0;
}

/*
 * Prepare execution-related fields for a newly created sbatchd job.
 *
 * This function validates mandatory identity fields and reconstructs
 * the execution working directory from the encoded cwd representation
 * received from mbd.
 *
 * CWD encoding rules:
 *   - spec.cwd == ""        -> execution cwd is the user's home directory
 *   - spec.cwd is relative -> execution cwd is subHomeDir + "/" + spec.cwd
 *   - spec.cwd is absolute -> execution cwd is spec.cwd
 *
 * This function does not chdir(). The caller is responsible for changing
 * directory and applying fallback logic (/tmp) in the execution path.
 */
static int
sbd_job_prepare_exec_fields(struct sbd_job *job)
{
    const char *cwd;
    const char *home;
    int n;

    if (job == NULL) {
        LS_ERR("job is NULL (bug)");
        errno = EINVAL;
        return -1;
    }

    // exec_username is required for status reporting and execution context
    if (job->exec_username[0] == 0) {
        LS_ERR("job %"PRId64" missing exec_username (bug)",
               job->job_id);
        errno = EINVAL;
        return -1;
    }

    // subHomeDir is required to decode home-relative cwd encodings
    if (job->spec.subHomeDir[0] == 0) {
        LS_ERR("job %"PRId64" missing subHomeDir (bug)",
               job->job_id);
        errno = EINVAL;
        return -1;
    }

    cwd = job->spec.cwd;
    home = job->spec.subHomeDir;

    // Decode the cwd encoding into an execution directory path
    // An empty spec.cwd is valid and means "home"
    if (cwd[0] == 0) {
        n = snprintf(job->exec_cwd, sizeof(job->exec_cwd), "%s", home);
        if (n < 0 || n >= (int)sizeof(job->exec_cwd)) {
            LS_ERR("job %"PRId64" exec_cwd overflow for home=%s (bug)",
                   job->job_id, home);
            errno = ENAMETOOLONG;
            return -1;
        }
        return 0;
    }

    // Absolute cwd: use as-is
    if (cwd[0] == '/') {
        n = snprintf(job->exec_cwd, sizeof(job->exec_cwd), "%s", cwd);
        if (n < 0 || n >= (int)sizeof(job->exec_cwd)) {
            LS_ERR("job %"PRId64" exec_cwd overflow for cwd=%s (bug)",
                   job->job_id, cwd);
            errno = ENAMETOOLONG;
            return -1;
        }
        return 0;
    }

    // Relative cwd: interpret as relative to user's home directory
    n = snprintf(job->exec_cwd, sizeof(job->exec_cwd),
                 "%s/%s", home, cwd);
    if (n < 0 || n >= (int)sizeof(job->exec_cwd)) {
        LS_ERR("job %"PRId64" exec_cwd overflow for home=%s cwd=%s (bug)",
               job->job_id, home, cwd);
        errno = ENAMETOOLONG;
        return -1;
    }

    return 0;
}
