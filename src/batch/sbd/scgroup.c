/*
 * Copyright (C) LavaLite Contributors
 * GPL v2
 *
 * cgroup v2 support for sbd job resource enforcement.
 *
 * Hierarchy:
 *   /sys/fs/cgroup/system.slice/lavalite-sbd.service/   <- sbd's delegated cgroup
 *     job_<jobid>/                                       <- one per running job
 *
 * /sys/fs/cgroup/system.slice/lavalite-sbd.service/
 *     cgroup.subtree_control     <- we write "+memory +cpu" here at init
 *     job_123/
 *         cgroup.procs           <- we write pid here after fork
 *         memory.max             <- we write limit here (mem_mb > 0)
 *         cpu.max                <- we write quota here (ncpus > 0)
 *     job_124/
 *         ...
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

#include "base/lib/ll.syslog.h"
#include "batch/sbd/sbd.h"

/*
 * cg_base is filled once by cgroup_init() from /proc/self/cgroup.
 * All path construction appends to it so we need room for the suffix.
 * Use a generous fixed size to avoid truncation warnings from -Werror.
 */
#define CG_BASE_MAX  4096
#define CG_PATH_MAX  8192

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
    ssize_t n  = write(fd, val, len);
    int err    = errno;
    close(fd);

    if (n < 0) {
        LS_ERR("cgroup write(%s, %s) failed: %s", path, val, strerror(err));
        return -err;
    }

    return 0;
}

static int read_self_cgroup(void)
{
    FILE *f = fopen("/proc/self/cgroup", "r");
    if (f == NULL) {
        LS_ERR("open /proc/self/cgroup failed: %m");
        return -1;
    }

    char line[CG_BASE_MAX];
    int found = 0;
    while (fgets(line, sizeof(line), f)) {
        /* cgroup v2 has a single line: "0::/<path>" */
        if (strncmp(line, "0::", 3) != 0)
            continue;
        char *p = line + 3;
        p[strcspn(p, "\n")] = 0;
        snprintf(cg_base, sizeof(cg_base), "/sys/fs/cgroup%s", p);
        found = 1;
        break;
    }
    fclose(f);
    if (found)
        return 0;
    LS_ERR("cgroup: no v2 entry in /proc/self/cgroup");
    return -1;
}

int cgroup_init(void)
{
    if (read_self_cgroup() < 0) {
        LS_ERR("cgroup: cannot determine own cgroup path");
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
        snprintf(val,  sizeof(val),  "%llu",
                 (unsigned long long)mem_mb * 1024 * 1024);

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
        snprintf(val,  sizeof(val),  "%d 100000", ncpus * 100000);

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
    snprintf(val,  sizeof(val),  "%d", (int)pid);

    if (cg_write(knob, val) < 0) {
        LS_ERR("job=%ld cgroup assign pid=%d failed", job_id, (int)pid);
        return -1;
    }

    LS_INFO("job=%ld cgroup assigned pid=%d", job_id, (int)pid);
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
