/*
 * Copyright (C) LavaLite Contributors
 * GPL v2
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <syslog.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <limits.h>

#include "base/lib/ll.conf.h"
#include "base/lib/ll.syslog.h"
#include "batch/sbd/sbd.h"

char sbd_root_dir[PATH_MAX];
char sbd_job_dir[PATH_MAX];
char sbd_state_dir[PATH_MAX];
char sbd_archive_dir[PATH_MAX];

int sbd_storage_init(void)
{
    char root_dir[PATH_MAX];

    /*
     * Non-root mode (sbd -n): use /tmp so we don't need to fight
     * ownership on LL_STATE_DIR which is typically root-owned.
     * Production (root): use LL_STATE_DIR from ll.conf.
     */
    if (non_root) {
        int l = snprintf(root_dir, sizeof(root_dir),
                         "/tmp/lavalite-sbd.%ld", (long)getuid());
        if (l < 0 || l >= (int)sizeof(root_dir)) {
            errno = ENAMETOOLONG;
            LS_ERR("non-root tmp state path too long");
            return -1;
        }
        if (mkdir(root_dir, 0700) < 0 && errno != EEXIST) {
            LS_ERR("mkdir(%s) failed", root_dir);
            return -1;
        }
    } else {
        ll_strlcpy(root_dir, ll_params[LL_STATE_DIR].val, sizeof(root_dir));
    }

    LS_INFO("sbd state root=%s", root_dir);

    if (snprintf(sbd_root_dir, sizeof(sbd_root_dir),
                 "%s/sbd", root_dir) >= (int)sizeof(sbd_root_dir)) {
        errno = ENAMETOOLONG;
        LS_ERR("sbd root path too long");
        return -1;
    }
    if (mkdir(sbd_root_dir, 0755) < 0 && errno != EEXIST) {
        LS_ERR("mkdir(%s) failed", sbd_root_dir);
        return -1;
    }
    chmod(sbd_root_dir, 0755);

    if (snprintf(sbd_job_dir, sizeof(sbd_job_dir),
                 "%s/jobs", sbd_root_dir) >= (int)sizeof(sbd_job_dir)) {
        errno = ENAMETOOLONG;
        LS_ERR("sbd jobs path too long");
        return -1;
    }
    if (mkdir(sbd_job_dir, 0755) < 0 && errno != EEXIST) {
        LS_ERR("mkdir(%s) failed", sbd_job_dir);
        return -1;
    }
    chmod(sbd_job_dir, 0755);

    if (snprintf(sbd_state_dir, sizeof(sbd_state_dir),
                 "%s/state", sbd_root_dir) >= (int)sizeof(sbd_state_dir)) {
        errno = ENAMETOOLONG;
        LS_ERR("sbd state path too long");
        return -1;
    }
    if (mkdir(sbd_state_dir, 0755) < 0 && errno != EEXIST) {
        LS_ERR("mkdir(%s) failed", sbd_state_dir);
        return -1;
    }
    chmod(sbd_state_dir, 0755);

    if (snprintf(sbd_archive_dir, sizeof(sbd_archive_dir),
                 "%s/.archive", sbd_root_dir) >= (int)sizeof(sbd_archive_dir)) {
        errno = ENAMETOOLONG;
        LS_ERR("archive path too long");
        return -1;
    }
    if (mkdir(sbd_archive_dir, 0755) < 0 && errno != EEXIST) {
        LS_ERR("mkdir(%s) failed", sbd_archive_dir);
        return -1;
    }
    chmod(sbd_archive_dir, 0755);

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

    int l = snprintf(dir, sizeof(dir), "%s/%ld", sbd_job_dir, job->job_id);
    if (l < 0 || (size_t)l >= sizeof(dir)) {
        errno = ENAMETOOLONG;
        LS_ERR("job dir path too long job=%ld", job->job_id);
        return;
    }

    for (int i = 0; job_files[i]; i++) {
        l = snprintf(path, sizeof(path), "%s/%s", dir, job_files[i]);
        if (l < 0 || (size_t)l >= sizeof(path)) {
            errno = ENAMETOOLONG;
            LS_ERR("job file path too long job=%ld file=%s",
                   job->job_id, job_files[i]);
            continue;
        }
        if (unlink(path) < 0 && errno != ENOENT)
            LS_ERR("unlink(%s) failed job=%ld", path, job->job_id);
    }

    if (rmdir(dir) < 0 && errno != ENOENT)
        LS_ERR("rmdir(%s) failed job=%ld", dir, job->job_id);
}

void sbd_job_state_archive(struct sbd_job *job)
{
    char src[PATH_MAX];
    char dst[PATH_MAX];
    char stpath[PATH_MAX];

    int l = snprintf(src, sizeof(src), "%s/%ld", sbd_state_dir, job->job_id);
    if (l < 0 || (size_t)l >= sizeof(src)) {
        errno = ENAMETOOLONG;
        LS_ERR("src path too long job=%ld", job->job_id);
        return;
    }

    l = snprintf(dst, sizeof(dst), "%s/%ld", sbd_archive_dir, job->job_id);
    if (l < 0 || (size_t)l >= sizeof(dst)) {
        errno = ENAMETOOLONG;
        LS_ERR("dst path too long job=%ld", job->job_id);
        return;
    }

    if (rename(src, dst) < 0) {
        LS_ERR("rename(%s, %s) failed job=%ld", src, dst, job->job_id);
        return;
    }

    if (chmod(dst, 0755) < 0)
        LS_ERR("chmod(%s, 0755) failed job=%ld", dst, job->job_id);

    l = snprintf(stpath, sizeof(stpath), "%s/state", dst);
    if (l < 0 || (size_t)l >= sizeof(stpath)) {
        errno = ENAMETOOLONG;
        LS_ERR("stpath too long job=%ld", job->job_id);
        return;
    }

    if (chmod(stpath, 0644) < 0 && errno != ENOENT)
        LS_ERR("chmod(%s, 0644) failed job=%ld", stpath, job->job_id);
}

int sbd_job_state_write(struct sbd_job *job)
{
    char path[PATH_MAX];
    int l = snprintf(path, sizeof(path), "%s/%ld/state",
                     sbd_state_dir, job->job_id);
    if (l < 0 || (size_t)l >= sizeof(path)) {
        errno = ENAMETOOLONG;
        LS_ERR("state path too long job=%ld", job->job_id);
        return -1;
    }

    char tmp_path[PATH_MAX];
    l = snprintf(tmp_path, sizeof(tmp_path), "%s/%ld/tmp.state",
                 sbd_state_dir, job->job_id);
    if (l < 0 || (size_t)l >= sizeof(tmp_path)) {
        errno = ENAMETOOLONG;
        LS_ERR("tmp state path too long job=%ld", job->job_id);
        return -1;
    }

    int pid_acked         = (job->pid_acked != 0);
    int execute_acked     = (job->execute_acked != 0);
    int finish_acked      = (job->finish_acked != 0);
    int exit_status_valid = (job->exit_status_valid != 0);

    char buf[LL_BUFSIZ_4K];
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
                     "exec_user=%s\n",
                     job->job_id,
                     job->pid,
                     job->pgid,
                     pid_acked,
                     (long)job->time_pid_acked,
                     execute_acked,
                     (long)job->time_execute_acked,
                     finish_acked,
                     (long)job->time_finish_acked,
                     exit_status_valid,
                     job->exit_status,
                     (long)job->end_time,
                     job->exec_cwd,
                     job->exec_home,
                     (unsigned)job->exec_uid,
                     job->exec_user);

    if (n < 0) {
        errno = EINVAL;
        LS_ERRX("state format failed job=%ld", job->job_id);
        return -1;
    }
    if ((size_t)n >= sizeof(buf)) {
        errno = ENAMETOOLONG;
        LS_ERR("state buffer too small job=%ld", job->job_id);
        return -1;
    }

    int fd = open(tmp_path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd < 0) {
        LS_ERR("open(%s) failed job=%ld", tmp_path, job->job_id);
        return -1;
    }

    if (write_all(fd, buf, (size_t)n) < 0) {
        LS_ERR("write(%s) failed job=%ld", tmp_path, job->job_id);
        close(fd);
        unlink(tmp_path);
        return -1;
    }

    if (fsync(fd) < 0) {
        LS_ERRX("fsync(%s) failed job=%ld", tmp_path, job->job_id);
        close(fd);
        unlink(tmp_path);
        return -1;
    }

    if (close(fd) < 0) {
        LS_ERR("close(%s) failed job=%ld", tmp_path, job->job_id);
        unlink(tmp_path);
        return -1;
    }

    if (rename(tmp_path, path) < 0) {
        LS_ERR("rename(%s -> %s) failed job=%ld", tmp_path, path, job->job_id);
        unlink(tmp_path);
        return -1;
    }

    int dfd = open(sbd_root_dir, O_RDONLY | O_DIRECTORY);
    if (dfd >= 0) {
        fsync(dfd);
        close(dfd);
    }

    LS_INFO("job=%ld pid=%d pgid=%d pid_acked=%d "
            "execute_acked=%d finish_acked=%d exit_status_valid=%d "
            "exit_status=%d end_time=%ld exec_cwd=%s exec_home=%s "
            "exec_uid=%u exec_user=%s",
            job->job_id,
            job->pid,
            job->pgid,
            pid_acked,
            execute_acked,
            finish_acked,
            exit_status_valid,
            job->exit_status,
            (long)job->end_time,
            job->exec_cwd,
            job->exec_home,
            (unsigned)job->exec_uid,
            job->exec_user);

    return 0;
}

int sbd_job_state_read(struct sbd_job *job, char *state_path)
{
    int version = 0;

    FILE *fp = fopen(state_path, "r");
    if (!fp)
        return -1;

    bool_t got_job_id = FALSE;
    char line[LL_BUFSIZ_512];

    while (fgets(line, sizeof(line), fp) != NULL) {
        char *eq = strchr(line, '=');
        if (!eq)
            continue;

        char *key = line;
        char *val = eq + 1;
        *eq = '\0';
        val[strcspn(val, "\r\n")] = '\0';

        if (strcmp(key, "version") == 0) {
            version = atoi(val);
            continue;
        }
        if (strcmp(key, "job_id") == 0) {
            job->job_id = (int64_t)strtoll(val, NULL, 10);
            got_job_id = TRUE;
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
            ll_strlcpy(job->exec_cwd, val, sizeof(job->exec_cwd));
            continue;
        }
        if (strcmp(key, "exec_home") == 0) {
            ll_strlcpy(job->exec_home, val, sizeof(job->exec_home));
            continue;
        }
        if (strcmp(key, "exec_uid") == 0) {
            job->exec_uid = (uid_t)atoi(val);
            continue;
        }
        if (strcmp(key, "exec_user") == 0) {
            ll_strlcpy(job->exec_user, val, sizeof(job->exec_user));
            continue;
        }
        if (strcmp(key, "time_pid_acked") == 0) {
            job->time_pid_acked = (time_t)atol(val);
            continue;
        }
        if (strcmp(key, "time_execute_acked") == 0) {
            job->time_execute_acked = (time_t)atol(val);
            continue;
        }
        if (strcmp(key, "time_finish_acked") == 0) {
            job->time_finish_acked = (time_t)atol(val);
            continue;
        }
    }

    fclose(fp);

    if (version != 1 || got_job_id == FALSE) {
        LS_ERRX("bad state file: version=%d got_job_id=%d path=%s",
                version, got_job_id, state_path);
        errno = EINVAL;
        return -1;
    }

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
        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
            continue;

        char state_path[PATH_MAX];
        int l = snprintf(state_path, sizeof(state_path),
                         "%s/%s/state", sbd_state_dir, de->d_name);
        if (l < 0 || (size_t)l >= sizeof(state_path))
            continue;

        struct sbd_job *job = calloc(1, sizeof(struct sbd_job));
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

    LS_INFO("loaded %d job(s) from persistent state", count);
    return 0;
}

int sbd_read_exit_status_file(struct sbd_job *job,
                              int *exit_code,
                              time_t *done_time)
{
    char path[PATH_MAX];
    int l = snprintf(path, sizeof(path), "%s/%ld/exit",
                     sbd_job_dir, job->job_id);
    if (l < 0 || (size_t)l >= sizeof(path)) {
        errno = ENAMETOOLONG;
        return -1;
    }

    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        LS_ERR("job=%ld open(%s) failed", job->job_id, path);
        return -1;
    }

    char buf[LL_BUFSIZ_64];
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    close(fd);

    if (n <= 0) {
        LS_ERR("job=%ld read(%s) failed", job->job_id, path);
        return -1;
    }

    buf[n] = 0;

    int code;
    int64_t ts;
    if (sscanf(buf, "%d %ld", &code, &ts) != 2) {
        LS_ERR("job=%ld sscanf failed path=%s", job->job_id, path);
        return -1;
    }

    if (exit_code)
        *exit_code = code;
    if (done_time)
        *done_time = (time_t)ts;

    return 0;
}

static void sbd_prune_archive(void)
{
    DIR *dir = opendir(sbd_archive_dir);
    if (!dir) {
        LS_ERR("opendir(%s) failed", sbd_archive_dir);
        return;
    }

    time_t t = time(NULL);
    struct dirent *de;
    while ((de = readdir(dir)) != NULL) {
        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
            continue;

        char state_path[PATH_MAX];
        int l = snprintf(state_path, sizeof(state_path),
                         "%s/%s/state", sbd_archive_dir, de->d_name);
        if (l < 0 || (size_t)l >= sizeof(state_path))
            continue;

        struct sbd_job job;
        memset(&job, 0, sizeof(job));
        if (sbd_job_state_read(&job, state_path) < 0) {
            LS_ERR("sbd_job_state_read state_path=%s failed", state_path);
            continue;
        }

        if (job.time_finish_acked <= 0)
            continue;
        if ((t - job.time_finish_acked) < SBD_ARCHIVE_RETENTION)
            continue;

        unlink(state_path);
        char dir_path[PATH_MAX];
        l = snprintf(dir_path, sizeof(dir_path),
                     "%s/%s", sbd_archive_dir, de->d_name);
        if (l < 0 || (size_t)l >= sizeof(dir_path))
            continue;

        if (rmdir(dir_path) < 0)
            LS_ERR("rmdir(%s) failed", dir_path);
    }

    closedir(dir);
}

void sbd_prune_archive_try(void)
{
    static time_t pruner_last;

    if (pruner_pid > 0)
        return;

    time_t t = time(NULL);
    if (t - pruner_last < SBD_PRUNE_INTERVAL)
        return;

    pruner_last = t;

    pruner_pid = fork();
    if (pruner_pid < 0) {
        pruner_pid = -1;
        LS_ERR("fork(prune) failed");
        return;
    }

    if (pruner_pid > 0) {
        LS_INFO("archive prune started pid=%d", (int)pruner_pid);
        return;
    }

    sbd_prune_archive();
    _exit(0);
}
