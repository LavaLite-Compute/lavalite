/*
 * Copyright (C) LavaLite Contributors
 * GPL v2
 *
 * cgroup v2 support for sbd job resource enforcement.
 *
 * Hierarchy:
 *   /sys/fs/cgroup/lavalite/          <- fixed base, created by sbd at init
 *       cgroup.subtree_control        <- we write "+memory +cpu" here
 *       job_123/
 *           cgroup.procs              <- parent and child write the job
 *                                        pid after fork, we read it to
 *                                        signal every job process
 *           cgroup.events             <- we read "populated": the job is
 *                                        alive until its last process ends
 *           cgroup.freeze             <- SIGSTOP / SIGCONT
 *           cgroup.kill               <- SIGKILL
 *           memory.max                <- we write limit here (mem_mb > 0)
 *           cpu.max                   <- we write quota here (ncpus > 0)
 *       job_124/
 *           ...
 *
 * The base path defaults to /sys/fs/cgroup/lavalite and can be overridden
 * via LL_CGROUP_ROOT in ll.conf. A fixed path is used instead of
 * /proc/self/cgroup so that sbd works correctly whether launched from
 * a terminal or via systemd -- terminal sessions place sbd inside a
 * transient leaf cgroup where sub-cgroup creation fails.
 *
 * mem_mb == 0  -> no memory.max written (unlimited)
 * ncpus  == 0  -> no cpu.max written   (unlimited)
 */

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "base/lib/ll.conf.h"
#include "base/lib/ll.syslog.h"
#include "batch/sbd/sbd.h"

/*
 * cg_base is set once by cgroup_init() to a fixed path that does not
 * depend on how sbd was launched. Works from terminal and systemd alike.
 *
 * Default: /sys/fs/cgroup/lavalite
 * Override: LL_CGROUP_ROOT in ll.conf
 */
#define CG_BASE_MAX LL_BUFSIZ_4K /* fits any realistic cgroup path */
#define CG_PATH_MAX LL_BUFSIZ_8K /* base + job suffix */

static char cg_base[CG_BASE_MAX];

/*
 * Write a NUL-terminated string to a cgroup control file.
 */
static int cg_write(const char *path, const char *val)
{
    int fd = open(path, O_WRONLY);
    if (fd < 0) {
        LL_ERR("cgroup open(%s) failed: %m", path);
        return -errno;
    }

    size_t len = strlen(val);
    ssize_t n = write(fd, val, len);
    int err = errno;
    close(fd);

    if (n < 0) {
        LL_ERR("cgroup write(%s, %s) failed: %s", path, val, strerror(err));
        return -err;
    }

    return 0;
}

/*
 * Read a single integer from a cgroup file.
 * Returns 0 on success, -1 on error.
 */
static int cg_read_u64(const char *path, uint64_t *out)
{
    char buf[LL_BUFSIZ_32];
    int fd = open(path, O_RDONLY);
    if (fd < 0)
        return -1;
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n <= 0)
        return -1;
    buf[n] = 0;
    *out = (uint64_t) strtoull(buf, NULL, 10);
    return 0;
}

/*
 * Parse usage_usec from cpu.stat.
 * Format is key/value lines: "usage_usec 123456"
 */
static int cg_read_cpu_usec(const char *path, uint64_t *usec)
{
    FILE *f = fopen(path, "r");
    if (f == NULL)
        return -1;
    char line[LL_BUFSIZ_64];
    int found = 0;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "usage_usec ", 11) != 0)
            continue;
        *usec = (uint64_t) strtoull(line + 11, NULL, 10);
        found = 1;
        break;
    }
    fclose(f);
    if (found)
        return 0;
    return -1;
}

int cgroup_init(void)
{
    /*
     * Use a fixed path independent of how sbd was launched.
     * Running from a terminal puts sbd inside a transient gnome scope
     * cgroup which is a leaf -- creating children there fails with
     * ENOENT because the no-internal-process rule prevents sub-cgroups
     * under a cgroup that already contains processes.
     *
     * A fixed top-level path sidesteps this entirely.
     */
    const char *root = ll_params[LL_CGROUP_ROOT].val;

    int n = snprintf(cg_base, sizeof(cg_base), "%s", root);
    if (n <= 0 || n >= (int) sizeof(cg_base)) {
        LL_ERR("cgroup: LL_CGROUP_ROOT path too long");
        return -1;
    }

    /* create the base cgroup if it does not exist */
    if (mkdir(cg_base, 0755) < 0 && errno != EEXIST) {
        LL_ERR("cgroup mkdir(%s) failed: %m", cg_base);
        return -1;
    }

    char knob[CG_PATH_MAX];
    snprintf(knob, sizeof(knob), "%s/cgroup.subtree_control", cg_base);

    if (cg_write(knob, "+memory +cpu") < 0) {
        LL_ERR("cgroup enable controllers failed path=%s", cg_base);
        return -1;
    }

    LL_INFO("cgroup init ok base=%s", cg_base);
    return 0;
}

int cgroup_job_create(struct sbd_job *job)
{
    char path[CG_PATH_MAX];
    char knob[CG_PATH_MAX];
    char val[LL_BUFSIZ_32];

    snprintf(path, sizeof(path), "%s/job_%ld", cg_base, job->job_id);

    if (mkdir(path, 0755) < 0 && errno != EEXIST) {
        LL_ERR("job=%ld cgroup mkdir(%s) failed: %m", job->job_id, path);
        return -1;
    }

    if (job->mem_mb > 0) {
        snprintf(knob, sizeof(knob), "%s/job_%ld/memory.max", cg_base, job->job_id);
        snprintf(val, sizeof(val), "%lu", job->mem_mb * 1024 * 1024);

        if (cg_write(knob, val) < 0)
            LL_ERR("job=%ld cgroup set memory.max=%s failed", job->job_id, val);
        else
            LL_INFO("job=%ld cgroup memory.max=%s bytes", job->job_id, val);
    }

    if (job->ncpus > 0) {
        /*
         * cpu.max format: "quota period"
         * quota = ncpus * period -> ncpus CPUs worth of time per period.
         * period = 100000 us (kernel default).
         */
        snprintf(knob, sizeof(knob), "%s/job_%ld/cpu.max", cg_base, job->job_id);
        snprintf(val, sizeof(val), "%d 100000", job->ncpus * 100000);

        if (cg_write(knob, val) < 0)
            LL_ERR("job=%ld cgroup set cpu.max=%s failed", job->job_id, val);
        else
            LL_INFO("job=%ld cgroup cpu.max=%s", job->job_id, val);
    }

    return 0;
}

/*
 * cgroup_job_assign - put the job process into its cgroup. Called by
 * both sides of fork(), like setpgid(), whichever runs first wins and
 * the second write is a no-op:
 *  - the child, still root, before it execs job.sh: job.sh and all it
 *    forks start in the cgroup
 *  - the parent, before sbd handles another message: a signal for the
 *    job always finds it in the cgroup
 * cgroup.procs is not a data file, each write() asks the kernel to
 * move the pid, the kernel serializes them.
 */
int cgroup_job_assign(struct sbd_job *job, pid_t pid)
{
    char knob[CG_PATH_MAX];
    char val[LL_BUFSIZ_32];

    snprintf(knob, sizeof(knob), "%s/job_%ld/cgroup.procs", cg_base, job->job_id);
    snprintf(val, sizeof(val), "%d", (int) pid);

    if (cg_write(knob, val) < 0) {
        LL_ERR("job=%ld cgroup assign pid=%d failed", job->job_id, (int) pid);
        return -1;
    }

    LL_INFO("job=%ld cgroup assigned pid=%d", job->job_id, (int) pid);
    return 0;
}

/*
 * cgroup_job_populated - "populated" from the job cgroup.events, kept
 * by the kernel: 1 while a process of the job is in the cgroup, 0 once
 * the last one exited. The job.sh pid does not matter, job.sh can be
 * gone and its children still running.
 *
 *  1  processes in the cgroup
 *  0  cgroup empty: an answer, not an error. Also the job that could
 *     not be assigned, the caller asks its process group too
 * -1  cannot tell: no cgroup, open, read or parse failed
 *
 * Logged at debug only: on a host without cgroup this is every job
 * at every tick.
 */
int cgroup_job_populated(struct sbd_job *job)
{
    char path[CG_PATH_MAX];
    snprintf(path, sizeof(path), "%s/job_%ld/cgroup.events", cg_base,
             job->job_id);

    FILE *fp = fopen(path, "r");
    if (fp == NULL) {
        LL_DEBUG("job=%ld cgroup fopen(%s) failed: %m", job->job_id, path);
        return -1;
    }

    int populated = -1;
    char line[LL_BUFSIZ_64];
    while (fgets(line, sizeof(line), fp) != NULL) {
        if (strncmp(line, "populated ", 10) != 0)
            continue;
        if (line[10] == '1')
            populated = 1;
        if (line[10] == '0')
            populated = 0;
        break;
    }
    fclose(fp);

    if (populated < 0)
        LL_DEBUG("job=%ld no populated in %s", job->job_id, path);

    return populated;
}

void cgroup_job_destroy(struct sbd_job *job)
{
    char path[CG_PATH_MAX];

    snprintf(path, sizeof(path), "%s/job_%ld", cg_base, job->job_id);

    if (rmdir(path) < 0) {
        LL_ERR("job=%ld cgroup rmdir(%s) failed: %m", job->job_id, path);
        return;
    }

    LL_INFO("job=%ld cgroup destroyed", job->job_id);
}

int cgroup_job_freeze(struct sbd_job *job)
{
    char path[CG_PATH_MAX];

    snprintf(path, sizeof(path), "%s/job_%ld/cgroup.freeze", cg_base, job->job_id);

    if (cg_write(path, "1") < 0) {
        LL_ERR("job=%ld cgroup freeze failed", job->job_id);
        return -1;
    }

    LL_INFO("job=%ld cgroup frozen", job->job_id);
    return 0;
}

int cgroup_job_thaw(struct sbd_job *job)
{
    char path[CG_PATH_MAX];

    snprintf(path, sizeof(path), "%s/job_%ld/cgroup.freeze", cg_base, job->job_id);

    if (cg_write(path, "0") < 0) {
        LL_ERR("job=%ld cgroup thaw failed", job->job_id);
        return -1;
    }

    LL_INFO("job=%ld cgroup thawed", job->job_id);
    return 0;
}

int cgroup_job_kill(struct sbd_job *job)
{
    char path[CG_PATH_MAX];

    snprintf(path, sizeof(path), "%s/job_%ld/cgroup.kill", cg_base, job->job_id);

    if (cg_write(path, "1") < 0) {
        LL_ERR("job=%ld cgroup kill failed", job->job_id);
        return -1;
    }

    LL_INFO("job=%ld cgroup killed", job->job_id);
    return 0;
}

/*
 * cgroup_job_signal - signal every process in the job cgroup, also
 * those that left the job process group with setsid().
 *
 * SIGSTOP  -> cgroup.freeze 1
 * SIGCONT  -> cgroup.freeze 0
 * SIGKILL  -> cgroup.kill, the kernel kills all, forks included
 * others   -> kill() each pid in cgroup.procs. Not atomic: a process
 *             forked after the read misses the signal. Accepted.
 *
 * 0 ok, -1 failed, the caller falls back to the process group.
 */
int cgroup_job_signal(struct sbd_job *job, int sig)
{
    if (sig == SIGSTOP)
        return cgroup_job_freeze(job);
    if (sig == SIGCONT)
        return cgroup_job_thaw(job);
    if (sig == SIGKILL)
        return cgroup_job_kill(job);

    char path[CG_PATH_MAX];
    snprintf(path, sizeof(path), "%s/job_%ld/cgroup.procs", cg_base,
             job->job_id);

    FILE *fp = fopen(path, "r");
    if (fp == NULL) {
        LL_ERR("job=%ld cgroup fopen(%s) failed: %m", job->job_id, path);
        return -1;
    }

    int pid;
    while (fscanf(fp, "%d", &pid) == 1) {
        /* ESRCH: exited since the read */
        if (kill(pid, sig) < 0 && errno != ESRCH)
            LL_ERR("job=%ld kill(%d, %d) failed: %m", job->job_id, pid, sig);
    }
    fclose(fp);

    LL_INFO("job=%ld cgroup signal %d sent", job->job_id, sig);
    return 0;
}

int cgroup_job_collect(struct sbd_job *job)
{
    char path[CG_PATH_MAX];

    memset(&job->res_usage, 0, sizeof(job->res_usage));

    /*
     * Prefer peak usage if supported (kernel >= 5.19).
     * Fallback to current on older systems (Rocky/Alma 9).
     */
    uint64_t val = 0;
    snprintf(path, sizeof(path), "%s/job_%ld/memory.peak", cg_base, job->job_id);
    if (cg_read_u64(path, &val) < 0) {
        snprintf(path, sizeof(path), "%s/job_%ld/memory.current", cg_base,
                 job->job_id);
        cg_read_u64(path, &val);
    }
    job->res_usage.mem_mb = val / (1024 * 1024);

    val = 0;
    snprintf(path, sizeof(path), "%s/job_%ld/memory.swap.peak", cg_base,
             job->job_id);
    if (cg_read_u64(path, &val) < 0) {
        snprintf(path, sizeof(path), "%s/job_%ld/memory.swap.current", cg_base,
                 job->job_id);
        cg_read_u64(path, &val);
    }
    job->res_usage.swap_mb = val / (1024 * 1024);

    val = 0;
    snprintf(path, sizeof(path), "%s/job_%ld/cpu.stat", cg_base, job->job_id);
    if (cg_read_cpu_usec(path, &val) == 0)
        job->res_usage.cpu_time = (double) val / 1000000.0;

    return 0;
}
