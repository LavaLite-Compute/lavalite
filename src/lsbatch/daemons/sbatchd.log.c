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

static int get_root_dir(char *root_dir, size_t len)
{
    int l;

    if (!root_dir || len == 0) {
        errno = EINVAL;
        return -1;
    }

    if (sbd_debug) {
        const char *tmp = getenv("TMPDIR");
        if (!tmp || tmp[0] == 0)
            tmp = "/tmp";

        l = snprintf(root_dir, len, "%s/lavalite-sbd.%ld",
                     tmp, (long)getuid());
        if (l < 0 || l >= (int)len) {
            errno = ENAMETOOLONG;
            LS_ERR("get_root_dir: tmp root path too long");
            return -1;
        }

        if (mkdir(root_dir, 0700) < 0 && errno != EEXIST) {
            LS_ERR("mkdir(%s) failed", root_dir);
            return -1;
        }

        return 0;
    }

    const char *share_dir = lsbParams[LSB_SHAREDIR].paramValue;
    if (!share_dir || share_dir[0] == 0) {
        errno = EINVAL;
        LS_ERR("get_root_dir: LSB_SHAREDIR is not set");
        return -1;
    }

    l = snprintf(root_dir, len, "%s", share_dir);
    if (l < 0 || l >= (int)len) {
        errno = ENAMETOOLONG;
        LS_ERR("get_root_dir: sbd root path too long");
        return -1;
    }

    return 0;
}

char sbd_root_dir[PATH_MAX];
char sbd_job_dir[PATH_MAX];
char sbd_state_dir[PATH_MAX];
char sbd_archive_dir[PATH_MAX];

int sbd_storage_init(void)
{
    char root_dir[PATH_MAX];

    if (get_root_dir(root_dir, PATH_MAX) < 0) {
        LS_ERR("get_root_dir failed");
        return -1;
    }

    // make <sharedir>/sbd
    if (snprintf(sbd_root_dir, sizeof(sbd_root_dir), "%s/sbd", root_dir) >=
        (int)sizeof(sbd_root_dir)) {
        errno = ENAMETOOLONG;
        LS_ERR("sbd root path too long");
        return -1;
    }
    if (mkdir(sbd_root_dir, 0755) < 0 && errno != EEXIST) {
        LS_ERR("mkdir(%s) failed", sbd_root_dir);
        return -1;
    }
    chmod(sbd_root_dir, 0755);

    // make <sharedir>/sbd/jobs
    if (snprintf(sbd_job_dir, sizeof(sbd_job_dir), "%s/jobs", sbd_root_dir) >=
        (int)sizeof(sbd_job_dir)) {
        errno = ENAMETOOLONG;
        LS_ERR("sbd jobs path too long");
        return -1;
    }
    if (mkdir(sbd_job_dir, 0755) < 0 && errno != EEXIST) {
        LS_ERR("mkdir(%s) failed", sbd_job_dir);
        return -1;
    }
    chmod(sbd_job_dir, 0755);

    // make <sharedir>/sbd/state
    if (snprintf(sbd_state_dir, sizeof(sbd_state_dir), "%s/state", sbd_root_dir) >=
        (int)sizeof(sbd_state_dir)) {
        errno = ENAMETOOLONG;
        LS_ERR("sbd state path too long");
        return -1;
    }
    if (mkdir(sbd_state_dir, 0755) < 0 && errno != EEXIST) {
        LS_ERR("mkdir(%s) failed", sbd_state_dir);
        return -1;
    }
    chmod(sbd_state_dir, 0755);

    // make <sharedir>/sbd/.archive
    if (snprintf(sbd_archive_dir, sizeof(sbd_archive_dir), "%s/.archive",
                 sbd_root_dir) >= (int)sizeof(sbd_archive_dir)) {
        errno = ENAMETOOLONG;
        LS_ERR("archive path too long");
        return -1;
    }

    if (mkdir(sbd_archive_dir, 0700) < 0 && errno != EEXIST) {
        LS_ERR("mkdir(%s) failed", sbd_archive_dir);
        return -1;
    }
    chmod(sbd_archive_dir, 0700);
    LS_INFO("sbd_archive_dir=%s", sbd_archive_dir);

    // sanity check
    struct stat s;
    if (stat(sbd_root_dir, &s) < 0 || !S_ISDIR(s.st_mode)) {
        LS_ERR("%s is not a directory", sbd_root_dir);
        return -1;
    }
    if (stat(sbd_job_dir, &s) < 0 || !S_ISDIR(s.st_mode)) {
        LS_ERR("%s is not a directory", sbd_job_dir);
        return -1;
    }
    if (stat(sbd_state_dir, &s) < 0 || !S_ISDIR(s.st_mode)) {
        LS_ERR("%s is not a directory", sbd_state_dir);
        return -1;
    }

    LS_INFO("sbd_root_dir=%s", sbd_root_dir);
    LS_INFO("sbd_job_dir=%s", sbd_job_dir);
    LS_INFO("sbd_state_dir=%s", sbd_state_dir);
    LS_INFO("sbd_archive_dir=%s", sbd_archive_dir);

    return 0;
}

void sbd_job_file_remove(struct sbd_job *job)
{
    static const char *const job_files[] = {
        "job.sh",
        "exit",
        NULL
    };

    char dir[PATH_MAX];
    char path[PATH_MAX];

    int l = snprintf(dir, sizeof(dir), "%s/%s", sbd_job_dir, job->jobfile);
    if (l < 0 || (size_t)l >= sizeof(dir)) {
        errno = ENAMETOOLONG;
        LS_ERR("sprint %s failed", dir);
        return;
    }

    for (int i = 0; job_files[i]; i++) {
        l = snprintf(path, sizeof(path), "%s/%s", dir, job_files[i]);
        if (l < 0 || (size_t)l >= sizeof(path)) {
            errno = ENAMETOOLONG;
            LS_ERR("sprint %s failed", path);
        }

        if (unlink(path) < 0 && errno != ENOENT) {
            LS_ERR("unlink(%s) failed, job=%ld", dir, job->job_id);
        }
    }

    if (rmdir(dir) < 0 && errno != ENOENT) {
        LS_ERR("rmdir(%s) failed, job=%ld", dir, job->job_id);
    }
}

void sbd_job_state_archive(struct sbd_job *job)
{
    // archive state dir instead of deleting it
    char src[PATH_MAX];
    char dst[PATH_MAX];

    int l = snprintf(src, sizeof(src), "%s/%s", sbd_state_dir, job->jobfile);
    if (l < 0 || (size_t)l >= sizeof(src)) {
        errno = ENAMETOOLONG;
        LS_ERR("sprint %s failed", src);
        return;
    }

    l = snprintf(dst, sizeof(dst), "%s/%s", sbd_archive_dir, job->jobfile);
    if (l < 0 || (size_t)l >= sizeof(dst)) {
        errno = ENAMETOOLONG;
        LS_ERR("sprint %s failed", dst);
        return;
    }

    if (rename(src, dst) < 0 && errno != ENOENT) {
        LS_ERR("rename(%s, %s) failed, job=%ld", src, dst, job->job_id);
    }
}

int sbd_job_state_write(struct sbd_job *job)
{
    // Single-writer invariant: sbd writes job state from the main loop only.
    // tmp name is job-addressable for post-mortem; no O_EXCL to avoid stale
    // tmp bricking after SIGKILL.
    char path[PATH_MAX];
    int l = snprintf(path, sizeof(path), "%s/%s/state", sbd_state_dir,
                     job->jobfile);
    if (l < 0 || (size_t)l >= sizeof(path)) {
        errno = ENAMETOOLONG;
        LS_ERR("sprint %s failed", path);
        return -1;
    }

    char tmp_path[PATH_MAX];
    l = snprintf(tmp_path, sizeof(tmp_path), "%s/%s/tmp.state", sbd_state_dir,
                 job->jobfile);
    if (l < 0 || (size_t)l >= (int)sizeof(tmp_path)) {
        errno = ENAMETOOLONG;
        LS_ERR("job %ld state tmp path too long", job->job_id);
        return -1;
    }

    int pid_acked         = (job->pid_acked != 0);
    int execute_acked     = (job->execute_acked != 0);
    int finish_acked      = (job->finish_acked != 0);
    int exit_status_valid = (job->exit_status_valid != 0);

    char buf[LL_BUFSIZ_2K];
    int n = snprintf(buf, sizeof(buf),
                     "version=1\n"
                     "job_id=%ld\n"
                     "pid=%d\n"
                     "pgid=%d\n"
                     "pid_acked=%d\n"
                     "time_pid_acked=%ld\n"
                     "execute_acked=%d\n"
                     "time_execute_acked=%ld\n"
                     "finish_acked=%d\n"
                     "time_finish_acked=%ld\n"
                     "exit_status_valid=%d\n"
                     "exit_status=%d\n"
                     "end_time=%ld\n"
                     "exec_cwd=%s\n"
                     "exec_home=%s\n"
                     "exec_uid=%u\n"
                     "exec_user=%s\n"
                     "jobfile=%s\n",
                     job->job_id,
                     job->pid,
                     job->pgid,
                     pid_acked,
                     job->time_pid_acked,
                     execute_acked,
                     job->time_execute_acked,
                     finish_acked,
                     job->time_finish_acked,
                     exit_status_valid,
                     job->exit_status,
                     job->end_time,
                     job->exec_cwd,
                     job->exec_home,
                     job->exec_uid,
                     job->exec_user,
                     job->jobfile);
    if (n < 0) {
        errno = EINVAL;
        LS_ERRX("job %ld state format failed", job->job_id);
        return -1;
    }
    if ((size_t)n >= sizeof(buf)) {
        errno = ENAMETOOLONG;
        LS_ERR("job %ld state buffer too small", job->job_id);
        return -1;
    }

    int fd = open(tmp_path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd < 0) {
        LS_ERR("job %ld open(%s) failed", job->job_id, tmp_path);
        return -1;
    }

    // Typicall C pattern for reliability writing, the rename
    // is atomic on POSIX systems
    // write to temp file → fsync() → close → rename(temp, final)

    if (write_all(fd, buf, (size_t)n) < 0) {
        LS_ERR("job %ld write(%s) failed", job->job_id, tmp_path);
        close(fd);
        unlink(tmp_path);
        return -1;
    }

    if (fsync(fd) < 0) {
        LS_ERRX("job %ld fsync(%s) failed", job->job_id, tmp_path);
        close(fd);
        unlink(tmp_path);
        return -1;
    }

    if (close(fd) < 0) {
        LS_ERR("job %ld close(%s) failed", job->job_id, tmp_path);
        unlink(tmp_path);
        return -1;
    }

    if (rename(tmp_path, path) < 0) {
        LS_ERR("job %ld rename(%s -> %s) failed", job->job_id,
               tmp_path, path);
        unlink(tmp_path);
        return -1;
    }

    // sync up the parent directory as well
    int dfd = open(sbd_root_dir, O_RDONLY | O_DIRECTORY);
    if (dfd >= 0) {
        fsync(dfd);
        close(dfd);
    }

    // exact copy of what is printed in the buffer
    LS_INFO("job=%ld pid=%d pgid=%d pid_acked=%d "
            "execute_acked=%d finish_acked=%d exit_status_valid=%d "
            "exit_status=%d end_time=%ld exec_cwd=%s exec_home=%s "
            "exec_uid=%d exec_user=%s jobfile=%s",
            job->job_id,
            job->pid,
            job->pgid,
            pid_acked,
            execute_acked,
            finish_acked,
            exit_status_valid,
            job->exit_status,
            job->end_time,
            job->exec_cwd,
            job->exec_home,
            job->exec_uid,
            job->exec_user,
            job->jobfile);

    return 0;
}

int sbd_job_state_load_all(void)
{
    DIR *dir = opendir(sbd_state_dir);
    if (!dir) {
        LS_ERR("opendir(%s) failed", sbd_state_dir);
        return -1;
    }

    int count = 0;
    struct dirent *de;

    while ((de = readdir(dir)) != NULL) {

        if (strcmp(de->d_name, ".") == 0 ||
            strcmp(de->d_name, "..") == 0)
            continue;

        char state_path[PATH_MAX];
        int l = snprintf(state_path, sizeof(state_path),
                         "%s/%s/state", sbd_state_dir, de->d_name);
        if (l < 0 || (size_t)l >= sizeof(state_path))
            continue;

        struct sbd_job *job = calloc(1, (sizeof(struct sbd_job)));
        if (!job)
            continue;

        if (sbd_job_state_read(job, state_path) < 0) {
            free(job);
            continue;
        }

        sbd_job_insert(job);
        count++;
    }

    closedir(dir);

    LS_INFO("loaded %d job(s) from sbd work directory", count);
    return 0;
}

int sbd_job_state_read(struct sbd_job *job, char *state_path)
{
    int version = 0;

    FILE *fp = fopen(state_path, "r");
    if (!fp)
        return -1;

    bool_t got_job_id = false;
    char line[LL_BUFSIZ_512];
    while (fgets(line, sizeof(line), fp) != NULL) {
        char *eq = strchr(line, '=');
        char *key;
        char *val;

        if (!eq)
            continue;

        *eq = 0;
        key = line;
        val = eq + 1;

        // hose the \n fgets keeps it
        val[strcspn(val, "\r\n")] = 0;

        if (strcmp(key, "version") == 0) {
            version = atoi(val);
            continue;
        }

        if (strcmp(key, "job_id") == 0) {
            job->job_id = (int64_t)strtoll(val, NULL, 10);
            got_job_id = true;
            continue;
        }

        if (strcmp(key, "pid") == 0) {
            job->pid = (pid_t)atol(val);
            continue;
        }

        if (strcmp(key, "pgid") == 0) {
            job->pgid = (pid_t)atol(val);
            continue;
        }

        if (strcmp(key, "pid_acked") == 0) {
            job->pid_acked = atoi(val);
            continue;
        }

        if (strcmp(key, "execute_acked") == 0) {
            job->execute_acked = atoi(val);
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

        if (strcmp(key, "exec_cwd") == 0) {
            ll_strlcpy(job->exec_cwd, val, PATH_MAX);
            continue;
        }

        if (strcmp(key, "exec_home") == 0) {
            ll_strlcpy(job->exec_home, val, PATH_MAX);
            continue;
        }

        if (strcmp(key, "exec_uid") == 0) {
            job->exec_uid = atoi(val);
            continue;
        }

        if (strcmp(key, "exec_user") == 0) {
            ll_strlcpy(job->exec_user, val, LL_BUFSIZ_32);
            continue;
        }

        if (strcmp(key, "jobfile") == 0) {
            ll_strlcpy(job->jobfile, val, PATH_MAX);
            continue;
        }

        if (strcmp(key, "time_pid_acked") == 0) {
            job->time_pid_acked = atol(val);
            continue;
        }

        if (strcmp(key, "time_execute_acked") == 0) {
            job->time_execute_acked = atol(val);
            continue;
        }

        if (strcmp(key, "time_finish_acked") == 0) {
            job->time_finish_acked = atol(val);
            continue;
        }
    }

    fclose(fp);

    if (version != 1 || got_job_id == false) {
        LS_ERRX("wrong version %d or we did not get the job_id=%ld?",
                version, job->job_id);
        errno = EINVAL;
        return -1;
    }

    return 0;
}

int sbd_read_exit_status_file(struct sbd_job *job,
                              int *exit_code,
                              time_t *done_time)
{
    // time_t may not be an int

    char path[PATH_MAX];
    int l =  snprintf(path, sizeof(path), "%s/%s/exit",
                      sbd_job_dir, job->jobfile);
    if (l < 0 || (size_t)l >= sizeof(path)) {
        errno = ENAMETOOLONG;
        return -1;
    }

    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        LS_ERR("job=%ld open=%s failed", job->job_id, path);
        return -1;
    }
    char buf[LL_BUFSIZ_128];
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    if (n < 0) {
        LS_ERR("job=%ld read=%ld failed", job->job_id, n);
    }
    close(fd);

    if (n <= 0)
        return -1;

    buf[n] = 0;
    int code;
    int64_t ts;
    if (sscanf(buf, "%d %ld", &code, &ts) != 2) {
        LS_ERR("job=%ld sscanf failed", job->job_id);
        return -1;
    }

    if (exit_code)
        *exit_code = code;

    if (done_time)
        *done_time = (time_t)ts;

    return 0;
}
