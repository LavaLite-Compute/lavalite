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
#include <stdarg.h>
#include <errno.h>

#include "base/lib/ll.conf.h"
#include "base/lib/ll.syslog.h"
#include "batch/sbd/sbd.h"

char sbd_root_dir[PATH_MAX];
char sbd_job_dir[PATH_MAX];
char sbd_state_dir[PATH_MAX];

static int prune_interval;
static int num_jobs_retain = 0;

static int make_path(char *buf, size_t bufsz, const char *fmt, ...)
{
    va_list ap;
    int n;

    va_start(ap, fmt);
    n = vsnprintf(buf, bufsz, fmt, ap);
    va_end(ap);

    if (n < 0 || (size_t) n >= bufsz) {
        errno = ENAMETOOLONG;
        return -1;
    }

    return 0;
}

static int mkdir_chmod(const char *path, mode_t mode)
{
    if (mkdir(path, mode) < 0 && errno != EEXIST) {
        LL_ERR("mkdir(%s) failed", path);
        return -1;
    }

    if (chmod(path, mode) < 0) {
        LL_ERR("chmod(%s) failed", path);
        return -1;
    }

    return 0;
}

static int check_dir(const char *path)
{
    struct stat st;

    if (stat(path, &st) < 0) {
        LL_ERR("stat(%s) failed", path);
        return -1;
    }

    if (!S_ISDIR(st.st_mode)) {
        errno = ENOTDIR;
        LL_ERR("%s is not a directory", path);
        return -1;
    }

    return 0;
}

static int job_dir_path(char *buf, size_t bufsz, int64_t job_id)
{
    // /opt/lavalite/var/sbd/jobs/job_id
    return make_path(buf, bufsz, "%s/%ld", sbd_job_dir, job_id);
}

static int state_dir_path(char *buf, size_t bufsz, int64_t job_id)
{
    // /opt/lavalite/var/sbd/state/job_id
    return make_path(buf, bufsz, "%s/%ld", sbd_state_dir, job_id);
}

static int state_file_path(char *buf, size_t bufsz, int64_t job_id)
{
    // /opt/lavalite/var/sbd/state/job_id/state
    return make_path(buf, bufsz, "%s/%ld/state", sbd_state_dir, job_id);
}

static int tmp_state_file_path(char *buf, size_t bufsz, int64_t job_id)
{
    return make_path(buf, bufsz, "%s/%ld/tmp.state", sbd_state_dir, job_id);
}

int sbd_storage_init(void)
{
    char root_dir[PATH_MAX];

    /*
     * Non-root mode (sbd -n): use /tmp so we don't need to fight
     * ownership on LL_STATE_DIR which is typically root-owned.
     * Production (root): use LL_STATE_DIR from ll.conf.
     */
    if (sim_name[0] != 0) {
        int n = snprintf(root_dir, sizeof(root_dir), "%s/%s",
                         ll_params[LL_STATE_DIR].val, sim_name);
        if (n < 0 || n >= (int) sizeof(root_dir)) {
            LL_ERR("simulator state path too long");
            return -1;
        }
        /* root_dir with sim_name is created fresh each run */
        if (mkdir_chmod(root_dir, 0755) < 0)
            return -1;
    } else if (non_root) {
        if (make_path(root_dir, sizeof(root_dir), "/tmp/lavalite-sbd.%ld",
                      (long) getuid()) < 0) {
            LL_ERR("non-root tmp state path too long");
            return -1;
        }
        /* non_root is created fresh each run */
        if (mkdir_chmod(root_dir, 0755) < 0)
            return -1;
    } else {
        // /opt/lavalite/var/state
        ll_strlcpy(root_dir, ll_params[LL_STATE_DIR].val, sizeof(root_dir));
    }

    // /opt/lavalite/var/state/sbd
    if (make_path(sbd_root_dir, sizeof(sbd_root_dir), "%s/sbd", root_dir) < 0) {
        LL_ERR("sbd root path too long");
        return -1;
    }
    if (mkdir_chmod(sbd_root_dir, 0755) < 0)
        return -1;

    // /opt/lavalite/var/state/sbd/jobs
    if (make_path(sbd_job_dir, sizeof(sbd_job_dir), "%s/jobs", sbd_root_dir) <
        0) {
        LL_ERR("sbd jobs path too long");
        return -1;
    }
    if (mkdir_chmod(sbd_job_dir, 0755) < 0)
        return -1;

    // /opt/lavalite/var/state/sbd/state
    if (make_path(sbd_state_dir, sizeof(sbd_state_dir), "%s/state",
                  sbd_root_dir) < 0) {
        LL_ERR("sbd state path too long");
        return -1;
    }
    if (mkdir_chmod(sbd_state_dir, 0755) < 0)
        return -1;

    if (check_dir(sbd_root_dir) < 0)
        return -1;
    if (check_dir(sbd_job_dir) < 0)
        return -1;
    if (check_dir(sbd_state_dir) < 0)
        return -1;

    LL_INFO("sbd_root_dir=%s", sbd_root_dir);
    LL_INFO("sbd_job_dir=%s", sbd_job_dir);
    LL_INFO("sbd_state_dir=%s", sbd_state_dir);

    prune_interval=900;
    if (! ll_atoi(ll_params[LL_SBD_PRUNE_INTERVAL].val, &prune_interval)) {
        LL_ERRX("failed parsing LL_PRUNE_INTERVAL=%s set to default 900sec",
                ll_params[LL_SBD_PRUNE_INTERVAL].val);
        prune_interval=900;
    }
    if (prune_interval <= 0) {
        LL_ERRX("LL_SBD_PRUNE_INTERVAL invalid=%d reset to default=900",
                prune_interval);
        prune_interval = 900;
    }

    num_jobs_retain = 0;
    if (! ll_atoi(ll_params[LL_SBD_JOB_FINISH_RETAIN].val, &num_jobs_retain)) {
        LL_ERRX("failed parsing LL_SBD_JOB_FINISH_RETAIN=%s set to default 100",
                ll_params[LL_SBD_JOB_FINISH_RETAIN].val);
        num_jobs_retain = 100;
    }

    if (num_jobs_retain <= 0) {
        LL_ERRX("LL_SBD_JOB_FINISH_RETAIN invalid=%d reset to default=100",
               num_jobs_retain);
        num_jobs_retain = 100;
    }
    LL_INFO("LL_SBD_PRUNE_INTERVAL=%d LL_SBD_JOB_FINISH_RETAIN=%d", prune_interval,
            num_jobs_retain);

    return 0;
}

void sbd_job_file_remove(struct sbd_job *job)
{
    static const char *const job_files[] = {"job.sh", "exit", NULL};

    char dir[PATH_MAX];
    char path[PATH_MAX];

    // /opt/lavalite/var/sbd/jobs/job_id
    if (job_dir_path(dir, sizeof(dir), job->job_id) < 0) {
        LL_ERR("job dir path too long job=%ld", job->job_id);
        return;
    }

    for (int i = 0; job_files[i] != NULL; i++) {
        if (make_path(path, sizeof(path), "%s/%s", dir, job_files[i]) < 0) {
            LL_ERR("job file path too long job=%ld file=%s", job->job_id,
                   job_files[i]);
            continue;
        }

        if (unlink(path) < 0 && errno != ENOENT)
            LL_ERR("unlink(%s) failed job=%ld", path, job->job_id);
    }

    if (rmdir(dir) < 0 && errno != ENOENT)
        LL_ERR("rmdir(%s) failed job=%ld", dir, job->job_id);
}

void sbd_job_state_remove(struct sbd_job *job)
{
    char state_path[PATH_MAX];
    char dir_path[PATH_MAX];

    if (make_path(state_path, sizeof(state_path), "%s/%ld/state",
                  sbd_state_dir, job->job_id) < 0)
        return;
    if (make_path(dir_path, sizeof(dir_path), "%s/%ld",
                  sbd_state_dir, job->job_id) < 0)
        return;

    if (unlink(state_path) < 0 && errno != ENOENT)
        LL_ERR("unlink(%s): %s", state_path, strerror(errno));
    if (rmdir(dir_path) < 0 && errno != ENOENT)
        LL_ERR("rmdir(%s): %s", dir_path, strerror(errno));
}

int sbd_job_state_write(struct sbd_job *job)
{
    char path[PATH_MAX];
    char tmp_path[PATH_MAX];
    char state_dir[PATH_MAX];

    if (state_file_path(path, sizeof(path), job->job_id) < 0) {
        LL_ERR("state path too long job=%ld", job->job_id);
        return -1;
    }

    if (tmp_state_file_path(tmp_path, sizeof(tmp_path), job->job_id) < 0) {
        LL_ERR("tmp state path too long job=%ld", job->job_id);
        return -1;
    }

    if (state_dir_path(state_dir, sizeof(state_dir), job->job_id) < 0) {
        LL_ERR("state dir path too long job=%ld", job->job_id);
        return -1;
    }

    int pid_acked = (job->pid_acked != 0);
    int finish_acked = (job->finish_acked != 0);
    int exit_status_valid = (job->exit_status_valid != 0);

    char buf[LL_BUFSIZ_4K];
    int n = snprintf(buf, sizeof(buf),
                     "version=1\n"
                     "job_id=%ld\n"
                     "pid=%d\n"
                     "pgid=%d\n"
                     "pid_acked=%d\n"
                     "time_pid_acked=%ld\n"
                     "finish_acked=%d\n"
                     "time_finish_acked=%ld\n"
                     "exit_status_valid=%d\n"
                     "exit_status=%d\n"
                     "end_time=%ld\n"
                     "exec_cwd=%s\n"
                     "exec_home=%s\n"
                     "exec_uid=%u\n"
                     "exec_gid=%u\n"
                     "exec_user=%s\n",
                     job->job_id, (int) job->pid, (int) job->pgid,
                     pid_acked, (long) job->time_pid_acked,
                     finish_acked, (long) job->time_finish_acked,
                     exit_status_valid, job->exit_status, (long) job->end_time,
                     job->exec_cwd, job->exec_home, (unsigned) job->exec_uid,
                     (unsigned) job->exec_gid, job->exec_user);
    if (n < 0) {
        errno = EINVAL;
        LL_ERRX("state format failed job=%ld", job->job_id);
        return -1;
    }

    if ((size_t) n >= sizeof(buf)) {
        errno = ENAMETOOLONG;
        LL_ERR("state buffer too small job=%ld", job->job_id);
        return -1;
    }

    int fd = open(tmp_path, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);
    if (fd < 0) {
        LL_ERR("open(%s) failed job=%ld", tmp_path, job->job_id);
        return -1;
    }

    if (write_all(fd, buf, (size_t) n) < 0) {
        LL_ERR("write(%s) failed job=%ld", tmp_path, job->job_id);
        close(fd);
        unlink(tmp_path);
        return -1;
    }

    if (fsync(fd) < 0) {
        LL_ERR("fsync(%s) failed job=%ld", tmp_path, job->job_id);
        close(fd);
        unlink(tmp_path);
        return -1;
    }

    if (close(fd) < 0) {
        LL_ERR("close(%s) failed job=%ld", tmp_path, job->job_id);
        unlink(tmp_path);
        return -1;
    }

    if (rename(tmp_path, path) < 0) {
        LL_ERR("rename(%s -> %s) failed job=%ld", tmp_path, path, job->job_id);
        unlink(tmp_path);
        return -1;
    }

    fsync_dir(state_dir);

    LL_INFO("job=%ld pid=%d pgid=%d pid_acked=%d "
            "finish_acked=%d exit_status_valid=%d "
            "exit_status=%d end_time=%ld exec_cwd=%s exec_home=%s "
            "exec_uid=%u exec_gid=%u exec_user=%s",
            job->job_id, (int) job->pid, (int) job->pgid, pid_acked,
            finish_acked, exit_status_valid, job->exit_status,
            (long) job->end_time, job->exec_cwd, job->exec_home,
            (unsigned) job->exec_uid, (unsigned) job->exec_gid, job->exec_user);

    return 0;
}

int sbd_job_state_read(struct sbd_job *job, char *state_path)
{
    FILE *fp;
    char line[LL_BUFSIZ_512];
    int version = 0;
    bool_t got_job_id = FALSE;

    fp = fopen(state_path, "r");
    if (fp == NULL)
        return -1;

    while (fgets(line, sizeof(line), fp) != NULL) {
        char *eq = strchr(line, '=');
        if (eq == NULL)
            continue;

        char *key = line;
        char *val = eq + 1;

        *eq = 0;
        val[strcspn(val, "\r\n")] = 0;

        if (strcmp(key, "version") == 0) {
            version = atoi(val);
            continue;
        }

        if (strcmp(key, "job_id") == 0) {
            job->job_id = (int64_t) strtoll(val, NULL, 10);
            got_job_id = TRUE;
            continue;
        }

        if (strcmp(key, "pid") == 0) {
            job->pid = (pid_t) atol(val);
            continue;
        }

        if (strcmp(key, "pgid") == 0) {
            job->pgid = (pid_t) atol(val);
            continue;
        }

        if (strcmp(key, "pid_acked") == 0) {
            job->pid_acked = atoi(val);
            continue;
        }

        if (strcmp(key, "time_pid_acked") == 0) {
            job->time_pid_acked = (time_t) atol(val);
            continue;
        }

        if (strcmp(key, "finish_acked") == 0) {
            job->finish_acked = atoi(val);
            continue;
        }

        if (strcmp(key, "time_finish_acked") == 0) {
            job->time_finish_acked = (time_t) atol(val);
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

        if (strcmp(key, "end_time") == 0) {
            job->end_time = (time_t) atol(val);
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
            job->exec_uid = (uid_t) atoi(val);
            continue;
        }

        if (strcmp(key, "exec_gid") == 0) {
            job->exec_gid = (gid_t) atoi(val);
            continue;
        }

        if (strcmp(key, "exec_user") == 0) {
            ll_strlcpy(job->exec_user, val, sizeof(job->exec_user));
            continue;
        }
    }

    fclose(fp);

    if (version != 1 || got_job_id == FALSE) {
        LL_ERRX("bad state file: version=%d got_job_id=%d path=%s", version,
                got_job_id, state_path);
        errno = EINVAL;
        return -1;
    }

    return 0;
}

int sbd_job_state_load_all(void)
{
    DIR *dir;
    struct dirent *de;
    int count = 0;

    dir = opendir(sbd_state_dir);
    if (dir == NULL) {
        LL_ERR("opendir(%s) failed", sbd_state_dir);
        return -1;
    }

    while ((de = readdir(dir)) != NULL) {
        char state_path[PATH_MAX];
        struct sbd_job *job;

        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
            continue;

        if (make_path(state_path, sizeof(state_path), "%s/%s/state",
                      sbd_state_dir, de->d_name) < 0)
            continue;

        job = calloc(1, sizeof(struct sbd_job));
        if (job == NULL)
            continue;

        job->pid = -1;
        job->pgid = -1;

        if (sbd_job_state_read(job, state_path) < 0) {
            free(job);
            continue;
        }
        if (job->finish_acked) {
            LL_DEBUG("remove job=%ld finish_acked at time %ld", job->job_id,
                     job->time_finish_acked);
            free(job);
            continue;
        }
        sbd_job_insert(job);
        count++;
    }

    closedir(dir);

    LL_INFO("loaded %d job(s) from persistent state", count);
    return 0;
}

int sbd_read_exit_status_file(struct sbd_job *job, int *exit_code,
                              time_t *done_time)
{
    char path[PATH_MAX];
    char buf[LL_BUFSIZ_64];
    int fd;
    ssize_t n;
    int code;
    long ts;

    if (make_path(path, sizeof(path), "%s/%ld/exit", sbd_job_dir, job->job_id) <
        0) {
        errno = ENAMETOOLONG;
        return -1;
    }

    fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        LL_ERR("job=%ld open(%s) failed", job->job_id, path);
        return -1;
    }

    n = read(fd, buf, sizeof(buf) - 1);
    close(fd);

    if (n <= 0) {
        LL_ERR("job=%ld read(%s) failed", job->job_id, path);
        return -1;
    }

    buf[n] = 0;

    if (sscanf(buf, "%d %ld", &code, &ts) != 2) {
        LL_ERR("job=%ld sscanf failed path=%s", job->job_id, path);
        return -1;
    }

    if (exit_code != NULL)
        *exit_code = code;
    if (done_time != NULL)
        *done_time = (time_t) ts;

    return 0;
}

#define PRUNE_MAX_SCAN  4096   /* sanity cap on dir entries */

struct prune_entry {
    int64_t jobid;
    time_t time_finish_acked;
};

static int
prune_entry_cmp(const void *a, const void *b)
{
    const struct prune_entry *pa = a;
    const struct prune_entry *pb = b;

    if (pa->time_finish_acked < pb->time_finish_acked)
        return -1;
    if (pa->time_finish_acked > pb->time_finish_acked)
        return 1;

    return 0;
}

static void sbd_prune_jobs(void)
{
    /*
     * First pass: count finished-acked jobs on disk.
     */
    DIR *dir = opendir(sbd_state_dir);
    if (dir == NULL) {
        LL_ERR("opendir(%s): %s", sbd_state_dir, strerror(errno));
        return;
    }

    int n_on_disk = 0;
    struct dirent *de;
    while ((de = readdir(dir)) != NULL) {
        char  state_path[PATH_MAX];
        struct sbd_job job;

        if (de->d_name[0] == '.')
            continue;
        if (make_path(state_path, sizeof(state_path), "%s/%s/state",
                      sbd_state_dir, de->d_name) < 0)
            continue;
        memset(&job, 0, sizeof(job));
        if (sbd_job_state_read(&job, state_path) < 0)
            continue;
        if (job.finish_acked)
            n_on_disk++;
    }
    closedir(dir);

    int n_remove = n_on_disk - num_jobs_retain;
    if (n_remove <= 0) {
        LL_DEBUG("no job dirs to prune n_on_disk=%d num_jobs_retain=%d",
                 n_on_disk, num_jobs_retain);
        return;
    }
    LL_INFO("n_on_disk=%d retain=%d n_remove=%d", n_on_disk, num_jobs_retain,
            n_remove);

    /*
     * Second pass: collect finished-acked entries, sort oldest-first, remove.
     */
    struct prune_entry entries[PRUNE_MAX_SCAN];
    int nentries = 0;

    dir = opendir(sbd_state_dir);
    if (dir == NULL) {
        LL_ERR("opendir(%s): %s", sbd_state_dir, strerror(errno));
        return;
    }
    while ((de = readdir(dir)) != NULL && nentries < PRUNE_MAX_SCAN) {
        char state_path[PATH_MAX];
        struct sbd_job job;

        if (de->d_name[0] == '.')
            continue;
        if (make_path(state_path, sizeof(state_path), "%s/%s/state",
                      sbd_state_dir, de->d_name) < 0)
            continue;
        memset(&job, 0, sizeof(job));
        if (sbd_job_state_read(&job, state_path) < 0)
            continue;
        if (!job.finish_acked || job.time_finish_acked <= 0)
            continue;
        entries[nentries].jobid = job.job_id;
        entries[nentries].time_finish_acked = job.time_finish_acked;
        nentries++;
    }
    closedir(dir);

    if (nentries == PRUNE_MAX_SCAN)
        LL_INFO("finished job prune scan capped at entries=%d", PRUNE_MAX_SCAN);

    qsort(entries, nentries, sizeof(entries[0]), prune_entry_cmp);

    if (n_remove > nentries)
        n_remove = nentries;

    for (int i = 0; i < n_remove; i++) {
        struct sbd_job job;
        memset(&job, 0, sizeof(job));
        job.job_id = entries[i].jobid;
        LL_INFO("prune job=%ld finish_acked=%ld",
                (long)job.job_id, (long)entries[i].time_finish_acked);
        sbd_job_file_remove(&job);
        sbd_job_state_remove(&job);
    }
}

void sbd_prune_jobs_try(void)
{
    static time_t last_prune;

    if (pruner_pid > 0)
        return;

    time_t now = time(NULL);
    if (now - last_prune < prune_interval)
        return;
    last_prune = now;

    pruner_pid = fork();
    if (pruner_pid < 0) {
        pruner_pid = 0;
        LL_ERR("fork(prune): %s", strerror(errno));
        return;
    }
    if (pruner_pid > 0) {
        LL_DEBUG("prune child pid=%d", (int)pruner_pid);
        return;
    }

    int log_fd = ls_getlogfd();
    reset_except_fd(log_fd);
    reset_signals();
    char tag[LL_BUFSIZ_32];
    snprintf(tag, sizeof(tag), "pruner child");
    ls_setlogtag(tag);

    LL_INFO("pruner starting num_jobs_retain=%d", num_jobs_retain);
    sbd_prune_jobs();
    LL_INFO("pruner done");

    _exit(0);
}

void fsync_dir(const char *path)
{
    int fd;

    fd = open(path, O_RDONLY | O_DIRECTORY);
    if (fd < 0) {
        LL_ERR("open directory %s failed", path);
        return;
    }

    if (fsync(fd) < 0)
        LL_ERR("fsync directory %s failed", path);

    close(fd);
}
