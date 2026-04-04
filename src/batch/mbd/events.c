/*
 * Copyright (C) LavaLite Contributors
 * GPL v2
 */

#include <string.h>
#include <errno.h>
#include <syslog.h>
#include <sys/stat.h>

#include "batch/lib/wire.h"
#include "base/lib/ll.conf.h"
#include "base/lib/ll.syslog.h"
#include "batch/mbd/mbd.h"
#include "batch/lib/log.h"

static char events_path[PATH_MAX];
static char acct_path[PATH_MAX];
char jobs_dir[PATH_MAX];

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
        mbd_die(MBD_EXIT_CONF);
    }
    LS_INFO("job working dir initialized %s", jobs_dir);

    for (int i = 0; i < 10; i++) {
        char bucket[PATH_MAX];
        int nb = snprintf(bucket, sizeof(bucket), "%s/%d", jobs_dir, i);
        if (nb < 0 || nb >= (int)sizeof(bucket))
            mbd_die(MBD_EXIT_CONF);

        if (mkdir(bucket, 0700) == -1 && errno != EEXIST) {
            LS_ERRX("mkdir(%s) failed", bucket);
            mbd_die(MBD_EXIT_CONF);
        }
    }
    LS_INFO("%d buckes dirs initialized", JOB_BUCKETS);

    return 0;
}

void reopen_job_events(void)
{
}

int log_job_new(const struct job_data *job, const struct wire_job_submit *ws)
{
    struct event_rec rec;
    memset(&rec, 0, sizeof(rec));
    rec.type = EVENT_JOB_NEW;
    rec.event_time = time(NULL);

    struct log_job *j;
    j = &rec.job;
    j->job_id = job->job_id;
    j->uid = (int)ws->uid;
    j->status = job->status;
    j->submit_time = (time_t)ws->submit_time;
    j->begin_time = (time_t)ws->begin_time;
    j->term_time = (time_t)ws->term_time;
    j->num_cpu = ws->num_cpus;
    j->num_hosts = ws->num_nhosts;
    j->mem_mb = ws->mem_mb;

    ll_strlcpy(j->job_name, ws->name, sizeof(j->job_name));
    ll_strlcpy(j->queue, ws->queue, sizeof(j->queue));
    ll_strlcpy(j->from_host, ws->from_host, sizeof(j->from_host));
    ll_strlcpy(j->cwd, ws->cwd, sizeof(j->cwd));
    ll_strlcpy(j->command, ws->command, sizeof(j->command));
    ll_strlcpy(j->in_file, ws->in_file, sizeof(j->in_file));
    ll_strlcpy(j->out_file, ws->out_file, sizeof(j->out_file));
    ll_strlcpy(j->err_file, ws->err_file, sizeof(j->err_file));
    ll_strlcpy(j->project_name, ws->project, sizeof(j->project_name));

    FILE *fp = fopen(events_path, "a");
    if (fp == NULL) {
        LS_ERR("fopen %s: %m", events_path);
        return -1;
    }
    if (log_write(fp, &rec) < 0) {
        LS_ERR("log_write failed job_id=%ld", job->job_id);
        fclose(fp);
        return -1;
    }
    fclose(fp);

    return 0;
}
