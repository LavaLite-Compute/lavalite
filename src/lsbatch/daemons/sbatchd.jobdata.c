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

struct sbd_job *sbd_job_create(const struct jobSpecs *spec)
{
    struct sbd_job *job = calloc(1, sizeof(*job));
    if (job == NULL) {
        LS_ERR("%s: calloc sbd_job failed: %m", __func__);
        return NULL;
    }

    // Copy the job specs as we need then during the lifetime
    // of the job to advance its pipeline
    if (jobSpecs_deep_copy(&job->spec, spec) < 0) {
        LS_ERR("%s: jobSpecs_deep_copy failed for jobId=%" PRId64,
               __func__, spec->jobId);
        free(job);
        return NULL;
    }

    // even if we calloc, let's do explicit initialization
    job->job_id = spec->jobId;
    job->pid = -1;
    job->pgid = -1;
    // calloc did it but we want to explicitly init all members
    memset(&job->job_reply, 0, sizeof(struct jobReply));
    job->reply_sent = false;
    job->reply_code = ERR_NO_ERROR;
    job->state = SBD_JOB_NEW;
    job->pid_acked = false;
    job->pid_ack_time = 0;
    job->execute_acked = false;
    job->execute_sent = false;
    job->finish_acked = false;
    job->finish_sent = false;
    job->exit_status_valid = false;
    job->exit_status = 0;
    job->exit_status_valid = false;
    memset(&job->lsf_rusage, 0, sizeof(job->lsf_rusage));
    job->missing = false;
    job->step = SBD_STEP_NONE;

    strlcpy(job->exec_username, spec->userName,
            sizeof(job->exec_username));

    return job;
}

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

// LavaLite sbd saved state record processing
// state dir is where the image of running jobs on this
// sbd are
static char sbd_state_dir[PATH_MAX];

int
sbd_job_record_dir_init(void)
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

int
sbd_job_record_remove(int64_t job_id)
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
                     "job_id=%"PRId64"\n"
                     "pid=%ld\n"
                     "pgid=%ld\n"
                     "state=%d\n"
                     "step=%d\n"
                     "pid_acked=%d\n"
                     "reply_sent=%d\n"
                     "execute_sent=%d\n"
                     "execute_acked=%d\n"
                     "finish_sent=%d\n"
                     "finish_acked=%d\n"
                     "exit_status_valid=%d\n"
                     "exit_status=%d\n"
                     "missing=%d\n",
                     job->job_id,
                     (long)job->pid,
                     (long)job->pgid,
                     (int)job->state,
                     (int)job->step,
                     job->pid_acked ? 1 : 0,
                     job->reply_sent ? 1 : 0,
                     job->execute_sent ? 1 : 0,
                     job->execute_acked ? 1 : 0,
                     job->finish_sent ? 1 : 0,
                     job->finish_acked ? 1 : 0,
                     job->exit_status_valid ? 1 : 0,
                     job->exit_status,
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
            "pid_acked=%d "
            "reply_sent=%d execute_sent=%d finish_sent=%d "
            "execute_acked=%d finish_acked=%d "
            "exit_status_valid=%d exit_status=%d "
            "missing=%d",
            job->job_id,
            (long)job->pid,
            (long)job->pgid,
            (int)job->state,
            (int)job->step,
            job->pid_acked ? 1 : 0,
            job->reply_sent ? 1 : 0,
            job->execute_sent ? 1 : 0,
            job->finish_sent ? 1 : 0,
            job->execute_acked ? 1 : 0,
            job->finish_acked ? 1 : 0,
            job->exit_status_valid ? 1 : 0,
            job->exit_status,
            job->missing ? 1 : 0);

    return 0;
}

int
sbd_job_record_load_all(void)
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

        job_id = atoll(de->d_name + 4);
        if (job_id <= 0)
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
