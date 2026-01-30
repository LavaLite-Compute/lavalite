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
static int sbd_go_path(int64_t, char *, size_t);

void sbd_child_open_log(const struct jobSpecs *specs)
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
char sbd_jfiles_dir[PATH_MAX];

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

    int l = snprintf(sbd_jfiles_dir, sizeof(sbd_jfiles_dir),
                     "%s/jfiles", sbd_root);
    if (l < 0 || (size_t)l >= sizeof(sbd_jfiles_dir)) {
        errno = ENAMETOOLONG;
        LS_ERR("sbd_init_state: job files path too long");
        return -1;
    }

    // Create <sharedir>/sbatchd and <sharedir>/sbatchd/state (ignore EEXIST).
    if (mkdir(sbd_root, 0700) < 0 && errno != EEXIST) {
        LS_ERR("sbd_init_state: mkdir(%s) failed", sbd_root);
        return -1;
    }

    if (mkdir(sbd_state_dir, 0700) < 0 && errno != EEXIST) {
        LS_ERR("sbd_init_state: mkdir(%s) failed", sbd_state_dir);
        return -1;
    }

    if (stat(sbd_state_dir, &st) < 0) {
        LS_ERR("sbd_init_state: stat(%s) failed", sbd_state_dir);
        return -1;
    }

    if (!S_ISDIR(st.st_mode)) {
        errno = ENOTDIR;
        LS_ERR("sbd_init_state: %s exists but is not a directory",
               sbd_state_dir);
        return -1;
    }

    if (mkdir(sbd_jfiles_dir, 0700) < 0 && errno != EEXIST) {
        LS_ERR("sbd_jfiles_state: mkdir(%s) failed", sbd_jfiles_dir);
        return -1;
    }

    if (stat(sbd_jfiles_dir, &st) < 0) {
        LS_ERR("sbd_jfiles_state: stat(%s) failed", sbd_jfiles_dir);
        return -1;
    }

    if (!S_ISDIR(st.st_mode)) {
        errno = ENOTDIR;
        LS_ERR("sbd_init_state: %s exists but is not a directory",
               sbd_jfiles_dir);
        return -1;
    }

    LS_INFO("sbd_state_dir=%s sbd_jfile_dir=%s", sbd_state_dir, sbd_jfiles_dir);
    return 0;
}

int sbd_job_record_remove(struct sbd_job *job)
{
    char path[PATH_MAX];
    // Leave the files now for debugging

    int64_t job_id = job->job_id;
    LS_INFO("removing job %ld", job_id);

    // state file
    int l = snprintf(path, sizeof(path), "%s/job.%ld", sbd_state_dir, job_id);
    if (l < 0 || (size_t)l >= sizeof(path)) {
        errno = ENAMETOOLONG;
        LS_ERR("sprint %s failed", path);
        return -1;
    }
    if (unlink(path) < 0) {
        LS_ERR("failed remove state file %s for job %ld", path, job_id);
        return -1;
    }

    // remove the exit status file
    l =  snprintf(path, sizeof(path), "%s/exit.status.%ld",
                  sbd_state_dir, job_id);
    if (l < 0 || (size_t)l >= sizeof(path)) {
        errno = ENAMETOOLONG;
        LS_ERR("sprint %s failed", path);
        return -1;
    }
    if (unlink(path) < 0) {
        LS_ERR("failed remove exit file %s for job %ld", path, job_id);
        return -1;
    }

    // remove the go file
    l =  snprintf(path, sizeof(path), "%s/go.%ld",
                  sbd_state_dir, job_id);
    if (l < 0 || (size_t)l >= sizeof(path)) {
        errno = ENAMETOOLONG;
        LS_ERR("sprint %s failed", path);
        return -1;
    }
    if (unlink(path) < 0) {
        LS_ERR("failed remove go file %s for job %ld", path, job_id);
        return -1;
    }

    return 0;
}

int sbd_jobfile_remove(struct sbd_job *job)
{
    char job_file[PATH_MAX];
    int l = snprintf(job_file, sizeof(job_file),
                     "%s/%s/job.%ld", sbd_jfiles_dir, job->spec.job_file,
                     job->job_id);
    if (l < 0 || (size_t)l >= PATH_MAX) {
        errno = ENAMETOOLONG;
        return -1;
    }

    if (unlink(job_file) < 0)
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

    int64_t job_id = job->job_id;

    // Single-writer invariant: sbatchd writes job records from the main loop only.
    // tmp name is job-addressable for post-mortem; no O_EXCL to avoid stale
    // tmp bricking after SIGKILL.
    char path[PATH_MAX];
    int l = snprintf(path, sizeof(path), "%s/job.%ld", sbd_state_dir, job_id);
    if (l < 0 || (size_t)l >= sizeof(path)) {
        errno = ENAMETOOLONG;
        LS_ERR("sprint %s failed", path);
        return -1;
    }

    char tmp_path[PATH_MAX];
    l = snprintf(tmp_path, sizeof(tmp_path), "%s/tmp.%ld", sbd_state_dir, job_id);
    if (l < 0 || (size_t)l >= (int)sizeof(tmp_path)) {
        errno = ENAMETOOLONG;
        LS_ERR("job %ld record tmp path too long", job->job_id);
        return -1;
    }

    //LS_INFO("job %ld record write start: %s", job->job_id, path);

    int pid_acked         = (job->pid_acked != 0);
    int execute_acked     = (job->execute_acked != 0);
    int finish_acked      = (job->finish_acked != 0);
    int exit_status_valid = (job->exit_status_valid != 0);
    int missing           = (job->missing != 0);

    char buf[LL_BUFSIZ_2K];
    int n = snprintf(buf, sizeof(buf),
                     "version=1\n"
                     "job_id=%ld\n"
                     "pid=%d\n"
                     "pgid=%d\n"
                     "state=%d\n"
                     "pid_acked=%d\n"
                     "execute_acked=%d\n"
                     "finish_acked=%d\n"
                     "exit_status_valid=%d\n"
                     "exit_status=%d\n"
                     "end_time=%ld\n"
                     "missing=%d\n",
                     job->job_id,
                     job->pid,
                     job->pgid,
                     job->state,
                     pid_acked,
                     execute_acked,
                     finish_acked,
                     exit_status_valid,
                     job->exit_status,
                     job->end_time,
                     missing ? 1 : 0);

    if (n < 0) {
        errno = EINVAL;
        LS_ERRX("job %ld record format failed", job->job_id);
        return -1;
    }
    if ((size_t)n >= sizeof(buf)) {
        errno = ENAMETOOLONG;
        LS_ERR("job %ld record buffer too small", job->job_id);
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

    // exact copy of what is printed in the buffer
    LS_INFO("job %ld record written pid=%d pgid=%d state=%d pid_acked=%d "
            "execute_acked=%d finish_acked=%d exit_status_valid=%d exit_status=%d "
            "end_time=%ld missing=%d",
            job->job_id,
            job->pid,
            job->pgid,
            job->state,
            pid_acked,
            execute_acked,
            finish_acked,
            exit_status_valid,
            job->exit_status,
            job->end_time,
            missing);

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
    int l = snprintf(path, sizeof(path), "%s/job.%ld", sbd_state_dir, job_id);
    if (l < 0 || (size_t)l >= sizeof(path)) {
        errno = ENAMETOOLONG;
        LS_ERR("sprint %s failed", path);
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
void sbd_prune_acked_jobs(void)
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
            LS_INFO("job: %ld not finish_acked yet", job->job_id);
            // advance
            e = e2;
            continue;
        }
        // this means we delivered the job status exit
        // to mbatch alread, but we keep the file around
        // for some times before cleaning it
        LS_INFO("job %ld was acked by mbd already", job->job_id);

        assert(job->exit_status_valid == true);
        // remove the job from the disk and free it from memory
        // later we may want to keep the job around for some time...
        // perhaps...
        sbd_job_record_remove(job);
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

int sbd_go_write(int64_t job_id)
{
    char path[PATH_MAX];
    char tmp[PATH_MAX];
    int fd;
    int n;
    time_t now;

    if (sbd_go_path(job_id, path, sizeof(path)) < 0)
        return -1;

    n = snprintf(tmp, sizeof(tmp), "%s/.go.%ld.%ld",
                 sbd_state_dir, (long)job_id, (long)getpid());
    if (n < 0 || n >= (int)sizeof(tmp)) {
        errno = ENAMETOOLONG;
        return -1;
    }

    fd = open(tmp, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);
    if (fd < 0)
        return -1;

    now = time(NULL);
    dprintf(fd, "%ld\n", (long)now);

    if (fsync(fd) < 0) {
        close(fd);
        unlink(tmp);
        return -1;
    }

    if (close(fd) < 0) {
        unlink(tmp);
        return -1;
    }

    if (rename(tmp, path) < 0) {
        unlink(tmp);
        return -1;
    }

    return 0;
}

static int sbd_go_path(int64_t job_id, char *buf, size_t buflen)
{
    int n;

    if (!buf || buflen == 0) {
        errno = EINVAL;
        return -1;
    }

    n = snprintf(buf, buflen, "%s/go.%ld", sbd_state_dir, (long)job_id);
    if (n < 0 || (size_t)n >= buflen) {
        errno = ENAMETOOLONG;
        return -1;
    }

    return 0;
}
