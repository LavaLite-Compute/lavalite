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
 *           cgroup.procs              <- we write pid here after fork
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
#include <stdio.h>
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
#define CG_BASE_MAX 4096 /* fits any realistic cgroup path */
#define CG_PATH_MAX 8192 /* base + job suffix */

static char cg_base[CG_BASE_MAX];

/*
 * Write a NUL-terminated string to a cgroup control file.
 */
static int cg_write(const char *path, const char *val)
{
    int fd = open(path, O_WRONLY);
    if (fd < 0) {
        LS_ERR("cgroup open(%s) failed: %m", path);
        return -errno;
    }

    size_t len = strlen(val);
    ssize_t n = write(fd, val, len);
    int err = errno;
    close(fd);

    if (n < 0) {
        LS_ERR("cgroup write(%s, %s) failed: %s", path, val, strerror(err));
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
    char buf[32];
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
    char line[64];
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
    if (n <= 0 || n >= (int)sizeof(cg_base)) {
        LS_ERR("cgroup: LL_CGROUP_ROOT path too long");
        return -1;
    }

    /* create the base cgroup if it does not exist */
    if (mkdir(cg_base, 0755) < 0 && errno != EEXIST) {
        LS_ERR("cgroup mkdir(%s) failed: %m", cg_base);
        return -1;
    }

    char knob[CG_PATH_MAX];
    snprintf(knob, sizeof(knob), "%s/cgroup.subtree_control", cg_base);

    if (cg_write(knob, "+memory +cpu") < 0) {
        LS_ERR("cgroup enable controllers failed path=%s", cg_base);
        return -1;
    }

    LS_INFO("cgroup init ok base=%s", cg_base);
    return 0;
}

int cgroup_job_create(int64_t job_id, uint64_t mem_mb, int32_t ncpus)
{
    char path[CG_PATH_MAX];
    char knob[CG_PATH_MAX];
    char val[32];

    snprintf(path, sizeof(path), "%s/job_%ld", cg_base, job_id);

    if (mkdir(path, 0755) < 0 && errno != EEXIST) {
        LS_ERR("job=%ld cgroup mkdir(%s) failed: %m", job_id, path);
        return -1;
    }

    if (mem_mb > 0) {
        snprintf(knob, sizeof(knob), "%s/job_%ld/memory.max", cg_base, job_id);
        snprintf(val, sizeof(val), "%llu",
                 (unsigned long long) mem_mb * 1024 * 1024);

        if (cg_write(knob, val) < 0)
            LS_ERR("job=%ld cgroup set memory.max=%s failed", job_id, val);
        else
            LS_INFO("job=%ld cgroup memory.max=%s bytes", job_id, val);
    }

    if (ncpus > 0) {
        /*
         * cpu.max format: "quota period"
         * quota = ncpus * period -> ncpus CPUs worth of time per period.
         * period = 100000 us (kernel default).
         */
        snprintf(knob, sizeof(knob), "%s/job_%ld/cpu.max", cg_base, job_id);
        snprintf(val, sizeof(val), "%d 100000", ncpus * 100000);

        if (cg_write(knob, val) < 0)
            LS_ERR("job=%ld cgroup set cpu.max=%s failed", job_id, val);
        else
            LS_INFO("job=%ld cgroup cpu.max=%s", job_id, val);
    }

    return 0;
}

int cgroup_job_assign(int64_t job_id, pid_t pid)
{
    char knob[CG_PATH_MAX];
    char val[32];

    snprintf(knob, sizeof(knob), "%s/job_%ld/cgroup.procs", cg_base, job_id);
    snprintf(val, sizeof(val), "%d", (int) pid);

    if (cg_write(knob, val) < 0) {
        LS_ERR("job=%ld cgroup assign pid=%d failed", job_id, (int) pid);
        return -1;
    }

    LS_INFO("job=%ld cgroup assigned pid=%d", job_id, (int) pid);
    return 0;
}

void cgroup_job_destroy(int64_t job_id)
{
    char path[CG_PATH_MAX];

    snprintf(path, sizeof(path), "%s/job_%ld", cg_base, job_id);

    if (rmdir(path) < 0) {
        LS_ERR("job=%ld cgroup rmdir(%s) failed: %m", job_id, path);
        return;
    }

    LS_INFO("job=%ld cgroup destroyed", job_id);
}

int cgroup_job_freeze(int64_t job_id)
{
    char path[CG_PATH_MAX];

    snprintf(path, sizeof(path), "%s/job_%ld/cgroup.freeze", cg_base, job_id);

    if (cg_write(path, "1") < 0) {
        LS_ERR("job=%ld cgroup freeze failed", job_id);
        return -1;
    }

    LS_INFO("job=%ld cgroup frozen", job_id);
    return 0;
}

int cgroup_job_thaw(int64_t job_id)
{
    char path[CG_PATH_MAX];

    snprintf(path, sizeof(path), "%s/job_%ld/cgroup.freeze", cg_base, job_id);

    if (cg_write(path, "0") < 0) {
        LS_ERR("job=%ld cgroup thaw failed", job_id);
        return -1;
    }

    LS_INFO("job=%ld cgroup thawed", job_id);
    return 0;
}

int cgroup_job_kill(int64_t job_id)
{
    char path[CG_PATH_MAX];

    snprintf(path, sizeof(path), "%s/job_%ld/cgroup.kill", cg_base, job_id);

    if (cg_write(path, "1") < 0) {
        LS_ERR("job=%ld cgroup kill failed", job_id);
        return -1;
    }

    LS_INFO("job=%ld cgroup killed", job_id);
    return 0;
}

int cgroup_job_collect(int64_t job_id, struct job_res_usage *ru)
{
    char path[CG_PATH_MAX];
    uint64_t val;

    memset(ru, 0, sizeof(*ru));

    /*
     * Prefer peak usage if supported (kernel >= 5.19).
     * Fallback to current on older systems (Rocky/Alma 9).
     */
    val = 0;
    snprintf(path, sizeof(path), "%s/job_%ld/memory.peak", cg_base, job_id);
    if (cg_read_u64(path, &val) < 0) {
        snprintf(path, sizeof(path), "%s/job_%ld/memory.current", cg_base,
                 job_id);
        cg_read_u64(path, &val);
    }
    ru->mem_mb = val / (1024 * 1024);

    val = 0;
    snprintf(path, sizeof(path), "%s/job_%ld/memory.swap.peak", cg_base,
             job_id);
    if (cg_read_u64(path, &val) < 0) {
        snprintf(path, sizeof(path), "%s/job_%ld/memory.swap.current", cg_base,
                 job_id);
        cg_read_u64(path, &val);
    }
    ru->swap_mb = val / (1024 * 1024);

    val = 0;
    snprintf(path, sizeof(path), "%s/job_%ld/cpu.stat", cg_base, job_id);
    if (cg_read_cpu_usec(path, &val) == 0)
        ru->cpu_time = (double) val / 1000000.0;

    return 0;
}
