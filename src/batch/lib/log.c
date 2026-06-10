/* Copyright (C) LavaLite Contributors
 * GPL v2
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <time.h>
#include <assert.h>
#include <sys/param.h>

#include "base/lib/ll.bufsiz.h"
#include "base/lib/ll.sys.h"
#include "batch/lib/log.h"

static const char *event_names[] = {
    [EVENT_NULL] = "NULL",
    [EVENT_JOB_NEW] = "JOB_NEW",
    [EVENT_JOB_START] = "JOB_START",
    [EVENT_JOB_FORK] = "JOB_FORK",
    [EVENT_JOB_SIGNAL] = "JOB_SIGNAL",
    [EVENT_JOB_FINISH] = "JOB_FINISH",
    [EVENT_JOB_PEND_SUSP] = "JOB_PEND_SUSP",
    [EVENT_JOB_PEND_RESUME] = "JOB_PEND_RESUME",
    [EVENT_JOB_SUSP] = "JOB_SUSP",
    [EVENT_JOB_MOVE] = "JOB_MOVE",
    [EVENT_JOB_PRIORITY] = "JOB_PRIORITY",
    [EVENT_JOB_PEND] = "JOB_PEND",
    [EVENT_COUNT] = NULL,
};

_Static_assert(sizeof(event_names) / sizeof(event_names[0]) == EVENT_COUNT + 1,
               "event_names[] out of sync with enum event_type");

static int write_hdr(FILE *fp, enum event_type type, time_t t)
{
    assert(t > 0);
    if (fprintf(fp, "%s %d %ld", event_names[type], LOG_VERSION, (long) t) < 0)
        return -1;
    return 0;
}

static int write_qstr(FILE *fp, const char *s)
{
    if (s == NULL)
        s = "";
    if (fprintf(fp, " \"%s\"", s) < 0)
        return -1;
    return 0;
}

static int read_qstr(const char **p, char *dst, int maxlen)
{
    const char *s = *p;

    while (*s == ' ')
        s++;
    if (*s != '"') {
        errno = EINVAL;
        return -1;
    }
    s++;
    const char *e = strchr(s, '"');
    if (e == NULL) {
        errno = EINVAL;
        return -1;
    }
    int len = (int) (e - s);
    if (len >= maxlen) {
        errno = EINVAL;
        return -1;
    }
    memcpy(dst, s, len);
    dst[len] = '\0';
    *p = e + 1;
    return 0;
}

static enum event_type parse_event_type(const char *name)
{
    for (int i = 1; i < EVENT_COUNT; i++) {
        if (strcmp(name, event_names[i]) == 0)
            return (enum event_type) i;
    }
    return EVENT_NULL;
}

int log_read_hdr(FILE *fp, struct event_rec *rec)
{
    char line[LL_BUFSIZ_4K];

    if (fgets(line, sizeof(line), fp) == NULL)
        return -1;

    char etype[LL_BUFSIZ_64];
    int ver;
    long ts;
    int cc;
    if (sscanf(line, "%63s %d %ld%n", etype, &ver, &ts, &cc) != 3) {
        errno = EINVAL;
        return -1;
    }

    rec->version = ver;
    rec->event_time = (time_t) ts;
    rec->type = parse_event_type(etype);

    ll_strlcpy(rec->rest, line + cc, sizeof(rec->rest));

    return 0;
}

/* -----------------------------------------------------------------------
 * JOB_NEW
 * ----------------------------------------------------------------------- */

int log_write_job_new(FILE *fp, const struct log_job_new *j)
{
    if (write_hdr(fp, EVENT_JOB_NEW, j->submit_time) < 0)
        return -1;
    if (fprintf(fp, " %ld %u %u %d %d %ld %ld %d %d %d %lu %lu %u",
                j->job_id,
                j->uid,
                j->gid,
                j->state,
                j->priority,
                (long) j->begin_time,
                (long) j->term_time,
                j->num_cpu,
                j->num_hosts,
                j->num_gpus,
                j->mem_mb,
                j->storage_mb,
                j->flags) < 0)
        return -1;
    if (write_qstr(fp, j->username) < 0)
        return -1;
    if (write_qstr(fp, j->job_name) < 0)
        return -1;
    if (write_qstr(fp, j->queue) < 0)
        return -1;
    if (write_qstr(fp, j->project_name) < 0)
        return -1;
    // gpu type and hosts are part of the requested resources
    if (write_qstr(fp, j->gpu_model) < 0)
        return -1;
    if (write_qstr(fp, j->machines) < 0)
        return -1;
    if (write_qstr(fp, j->tokenpool) < 0)
        return -1;
    if (fprintf(fp, "\n") < 0)
        return -1;
    return 0;
}

int log_parse_job_new(const struct event_rec *rec, struct log_job_new *j)
{
    const char *p = rec->rest;
    int cc;
    int n = sscanf(p, " %ld %u %u %d %d %ld %ld %d %d %d %lu %lu %u%n",
                   &j->job_id,
                   &j->uid,
                   &j->gid,
                   &j->state,
                   &j->priority,
                   &j->begin_time,
                   &j->term_time,
                   &j->num_cpu,
                   &j->num_hosts,
                   &j->num_gpus,
                   &j->mem_mb,
                   &j->storage_mb,
                   &j->flags, &cc);
    if (n != 13) {
        errno = EINVAL;
        return -1;
    }
    p += cc;

    j->submit_time = rec->event_time;
    if (read_qstr(&p, j->username, sizeof(j->username)) < 0)
        return -1;
    if (read_qstr(&p, j->job_name, sizeof(j->job_name)) < 0)
        return -1;
    if (read_qstr(&p, j->queue, sizeof(j->queue)) < 0)
        return -1;
    if (read_qstr(&p, j->project_name, sizeof(j->project_name)) < 0)
        return -1;
    if (read_qstr(&p, j->gpu_model, sizeof(j->gpu_model)) < 0)
        return -1;
    if (read_qstr(&p, j->machines, sizeof(j->machines)) < 0)
        return -1;
    if (read_qstr(&p, j->tokenpool, sizeof(j->tokenpool)) < 0)
        return -1;

    return 0;
}

/* -----------------------------------------------------------------------
 * JOB_START
 *
 * Full scheduling plan recorded for observability/accounting.
 * Format: job_id nhosts cpus_per_host gpus_per_host "exec_host" "gpu_model" "h1
 * h2 ..." replay only needs exec_host; bhist/ebd use the rest.
 * ----------------------------------------------------------------------- */

int log_write_job_start(FILE *fp, const struct log_job_start *j)
{
    if (write_hdr(fp, EVENT_JOB_START, j->dispatch_time) < 0)
        return -1;
    if (fprintf(fp, " %ld %d %d %d", j->job_id, j->nhosts, j->cpus_per_host,
                j->gpus_per_host) < 0)
        return -1;
    if (write_qstr(fp, j->gpu_model) < 0)
        return -1;
    if (write_qstr(fp, j->gpu_assigned) < 0)
        return -1;
    if (write_qstr(fp, j->hosts) < 0)
        return -1;
    if (fprintf(fp, "\n") < 0)
        return -1;
    return 0;
}

int log_parse_job_start(const struct event_rec *rec, struct log_job_start *j)
{
    const char *p = rec->rest;
    int cc;
    int n = sscanf(p, " %ld %d %d %d%n", &j->job_id, &j->nhosts,
                   &j->cpus_per_host, &j->gpus_per_host, &cc);
    if (n != 4) {
        errno = EINVAL;
        return -1;
    }
    p += cc;

    if (read_qstr(&p, j->gpu_model, sizeof(j->gpu_model)) < 0)
        return -1;
    if (read_qstr(&p, j->gpu_assigned, sizeof(j->gpu_assigned)) < 0)
        return -1;
    if (read_qstr(&p, j->hosts, sizeof(j->hosts)) < 0)
        return -1;

    j->dispatch_time = rec->event_time;

    return 0;
}

/* -----------------------------------------------------------------------
 * JOB_FORK
 * ----------------------------------------------------------------------- */

int log_write_job_fork(FILE *fp, const struct log_job_fork *j)
{
    if (write_hdr(fp, EVENT_JOB_FORK, j->fork_time) < 0)
        return -1;
    if (fprintf(fp, " %ld %d\n", (long) j->job_id, j->job_pid) < 0)
        return -1;
    return 0;
}

int log_parse_job_fork(const struct event_rec *rec, struct log_job_fork *j)
{
    int n = sscanf(rec->rest, " %ld %d", &j->job_id, &j->job_pid);
    if (n != 2) {
        errno = EINVAL;
        return -1;
    }

    j->fork_time = rec->event_time;

    return 0;
}

/* -----------------------------------------------------------------------
 * JOB_SIGNAL
 * ----------------------------------------------------------------------- */

int log_write_job_signal(FILE *fp, const struct log_job_signal *j)
{
    if (write_hdr(fp, EVENT_JOB_SIGNAL, j->signal_time) < 0)
        return -1;
    if (fprintf(fp, " %ld %d %u\n", (long) j->job_id, j->signal_num, j->uid) <
        0)
        return -1;
    return 0;
}

int log_parse_job_signal(const struct event_rec *rec, struct log_job_signal *j)
{
    int n =
        sscanf(rec->rest, " %ld %d %u", &j->job_id, &j->signal_num, &j->uid);
    if (n != 3) {
        errno = EINVAL;
        return -1;
    }

    j->signal_time = rec->event_time;

    return 0;
}

/* -----------------------------------------------------------------------
 * JOB_FINISH
 * ----------------------------------------------------------------------- */

int log_write_job_finish(FILE *fp, const struct log_job_finish *j)
{
    if (write_hdr(fp, EVENT_JOB_FINISH, j->end_time) < 0)
        return -1;
    if (fprintf(fp, " %ld %u %d %d %ld", (long) j->job_id, (unsigned) j->uid,
                j->state, j->exit_status, (long) j->end_time) < 0)
        return -1;
    if (fprintf(fp, "\n") < 0)
        return -1;
    return 0;
}

int log_parse_job_finish(const struct event_rec *rec, struct log_job_finish *j)
{
    const char *p = rec->rest;
    int cc;
    int n = sscanf(p, " %ld %u %d %d %n", &j->job_id, (unsigned *) &j->uid,
                   &j->state, &j->exit_status, &cc);
    if (n != 4) {
        errno = EINVAL;
        return -1;
    }
    p += cc;
    j->end_time = rec->event_time;

    return 0;
}

/* -----------------------------------------------------------------------
 * JOB_PEND_SUSP / JOB_PEND_RESUME / JOB_SUSP
 * ----------------------------------------------------------------------- */

int log_write_job_pend_susp(FILE *fp, const struct log_job_pend_susp *j)
{
    if (write_hdr(fp, EVENT_JOB_PEND_SUSP, j->event_time) < 0)
        return -1;
    if (fprintf(fp, " %ld\n", (long) j->job_id) < 0)
        return -1;
    return 0;
}

int log_parse_job_pend_susp(const struct event_rec *rec,
                            struct log_job_pend_susp *j)
{
    int n = sscanf(rec->rest, " %ld", &j->job_id);
    if (n != 1) {
        errno = EINVAL;
        return -1;
    }

    j->event_time = rec->event_time;

    return 0;
}

int log_write_job_pend_resume(FILE *fp, const struct log_job_pend_resume *j)
{
    if (write_hdr(fp, EVENT_JOB_PEND_RESUME, j->event_time) < 0)
        return -1;
    if (fprintf(fp, " %ld\n", (long) j->job_id) < 0)
        return -1;
    return 0;
}

int log_parse_job_pend_resume(const struct event_rec *rec,
                              struct log_job_pend_resume *j)
{
    int n = sscanf(rec->rest, " %ld", &j->job_id);
    if (n != 1) {
        errno = EINVAL;
        return -1;
    }

    j->event_time = rec->event_time;

    return 0;
}

int log_write_job_susp(FILE *fp, const struct log_job_susp *j)
{
    if (write_hdr(fp, EVENT_JOB_SUSP, j->event_time) < 0)
        return -1;
    if (fprintf(fp, " %ld\n", (long) j->job_id) < 0)
        return -1;
    return 0;
}

int log_parse_job_susp(const struct event_rec *rec, struct log_job_susp *j)
{
    int n = sscanf(rec->rest, " %ld", &j->job_id);
    if (n != 1) {
        errno = EINVAL;
        return -1;
    }

    j->event_time = rec->event_time;

    return 0;
}

/* -----------------------------------------------------------------------
 * JOB_MOVE
 * -----------------------------------------------------------------------
 */

int log_write_job_move(FILE *fp, const struct log_job_move *j)
{
    if (write_hdr(fp, EVENT_JOB_MOVE, j->event_time) < 0)
        return -1;
    if (fprintf(fp, " %ld", (long) j->job_id) < 0)
        return -1;
    if (write_qstr(fp, j->from_queue) < 0)
        return -1;
    if (write_qstr(fp, j->to_queue) < 0)
        return -1;
    if (fprintf(fp, "\n") < 0)
        return -1;
    return 0;
}

int log_parse_job_move(const struct event_rec *rec, struct log_job_move *j)
{
    const char *p = rec->rest;
    int cc;
    int n = sscanf(p, " %ld%n", &j->job_id, &cc);
    if (n != 1) {
        errno = EINVAL;
        return -1;
    }
    p += cc;

    if (read_qstr(&p, j->from_queue, sizeof(j->from_queue)) < 0)
        return -1;
    if (read_qstr(&p, j->to_queue, sizeof(j->to_queue)) < 0)
        return -1;

    j->event_time = rec->event_time;

    return 0;
}

/* -----------------------------------------------------------------------
 * JOB_PRIORITY
 * ----------------------------------------------------------------------- */

int log_write_job_priority(FILE *fp, const struct log_job_priority *j)
{
    if (write_hdr(fp, EVENT_JOB_PRIORITY, j->event_time) < 0)
        return -1;
    if (fprintf(fp, " %ld %d %d\n", (long) j->job_id,
                j->old_priority, j->new_priority) < 0)
        return -1;
    return 0;
}

int log_parse_job_priority(const struct event_rec *rec, struct log_job_priority *j)
{
    int n = sscanf(rec->rest, " %ld %d %d",
                   &j->job_id, &j->old_priority, &j->new_priority);
    if (n != 3) {
        errno = EINVAL;
        return -1;
    }
    j->event_time = rec->event_time;
    return 0;
}

// Job goes back to pend since it failed to dispatch
int log_write_job_pend(FILE *fp, const struct log_job_pend *j)
{
    if (write_hdr(fp, EVENT_JOB_PEND, j->event_time) < 0)
        return -1;
    if (fprintf(fp, " %ld\n", (long) j->job_id) < 0)
        return -1;
    return 0;
}

int log_parse_job_pend(const struct event_rec *rec, struct log_job_pend *j)
{
    int n = sscanf(rec->rest, " %ld", &j->job_id);
    if (n != 1) {
        errno = EINVAL;
        return -1;
    }

    j->event_time = rec->event_time;
    return 0;
}
