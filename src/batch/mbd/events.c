/*
 * Copyright (C) LavaLite Contributors
 * GPL v2
 */

#include <errno.h>
#include <syslog.h>
#include <sys/stat.h>

#include "base/lib/ll.conf.h"
#include "base/lib/ll.syslog.h"
#include "batch/mbd/mbd.h"

static char events_path[PATH_MAX];
static char acct_path[PATH_MAX];
static char jobs_dir[PATH_MAX];

int events_init(void)
{
    char dir[PATH_MAX];
    int n;

    n = snprintf(dir, sizeof(dir), "%s/mbd",
                 ll_params[LL_STATE_DIR].val);
    if (n < 0 || n >= (int)sizeof(dir))
        mbd_die(MBD_EXIT_CONF);

    if (mkdir(dir, 0700) == -1 && errno != EEXIST) {
        syslog(LOG_ERR, "mkdir(%s) failed: %m", dir);
        mbd_die(MBD_EXIT_FATAL);
    }
    LS_INFO("working dir initialized %s", dir);

    n = snprintf(events_path, sizeof(events_path), "%s/job.events", dir);
    if (n < 0 || n >= (int)sizeof(events_path))
        mbd_die(MBD_EXIT_CONF);

    LS_INFO("job events initialized %s", events_path);

    n = snprintf(acct_path, sizeof(acct_path), "%s/jobs.acct", dir);
    if (n < 0 || n >= (int)sizeof(acct_path))
        mbd_die(MBD_EXIT_CONF);

    LS_INFO("job accounts initialized %s", acct_path);

    n = snprintf(jobs_dir, sizeof(jobs_dir), "%s/jobs", dir);
    if (n < 0 || n >= (int)sizeof(jobs_dir))
        mbd_die(MBD_EXIT_CONF);

    if (mkdir(jobs_dir, 0700) == -1 && errno != EEXIST) {
        LS_ERRX("mkdir(%s) failed", jobs_dir);
        mbd_die(MBD_EXIT_FATAL);
    }
    LS_INFO("job working dir initialized %s", jobs_dir);

    return 0;
}

void reopen_job_events(void)
{
}
