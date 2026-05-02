/* Copyright (C) LavaLite Contributors
 * GPL v2
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <sys/param.h>

#include "base/lib/ll.bufsiz.h"
#include "base/lib/ll.sys.h"
#include "batch/lib/log.h"

static const char *event_names[] = {
    [EVENT_NULL]            = "NULL",
    [EVENT_JOB_NEW]         = "JOB_NEW",
    [EVENT_JOB_START]       = "JOB_START",
    [EVENT_JOB_ACCEPT]      = "JOB_ACCEPT",
    [EVENT_JOB_EXECUTE]     = "JOB_EXECUTE",
    [EVENT_JOB_SIGNAL]      = "JOB_SIGNAL",
    [EVENT_JOB_FINISH]      = "JOB_FINISH",
    [EVENT_JOB_PEND_SUSP]   = "JOB_PEND_SUSP",
    [EVENT_JOB_PEND_RESUME] = "JOB_PEND_RESUME",
    [EVENT_JOB_SUSP]        = "JOB_SUSP",
    [EVENT_COUNT]           = NULL,
};

_Static_assert(
    sizeof(event_names) / sizeof(event_names[0]) == EVENT_COUNT + 1,
    "event_names[] out of sync with enum event_type"
);

/* -----------------------------------------------------------------------
 * helpers
 * ----------------------------------------------------------------------- */

static int write_hdr(FILE *fp, enum event_type type, time_t t)
{
    if (fprintf(fp, "%s %d %ld", event_names[type], LOG_VERSION,
                (long)t) < 0)
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
    if (*s != '"')
        return -1;
    s++;
    const char *e = strchr(s, '"');
    if (e == NULL)
        return -1;
    int len = (int)(e - s);
    if (len >= maxlen)
        return -1;
    memcpy(dst, s, len);
    dst[len] = '\0';
    *p = e + 1;
    return 0;
}

static enum event_type parse_event_type(const char *name)
{
    for (int i = 1; i < EVENT_COUNT; i++) {
        if (strcmp(name, event_names[i]) == 0)
            return (enum event_type)i;
    }
    return EVENT_NULL;
}

/* -----------------------------------------------------------------------
 * header
 * ----------------------------------------------------------------------- */

int log_read_hdr(FILE *fp, struct event_rec *rec)
{
    char line[LL_BUFSIZ_4K];

    if (fgets(line, sizeof(line), fp) == NULL)
        return -1;

    char etype[LL_BUFSIZ_64];
    int ver;
    long ts;
    int cc;
    if (sscanf(line, "%63s %d %ld%n", etype, &ver, &ts, &cc) != 3)
        return -1;

    rec->version    = ver;
    rec->event_time = (time_t)ts;
    rec->type       = parse_event_type(etype);

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
    if (fprintf(fp, " %ld %u %u %d %ld %ld %ld %d %d %d %lu %lu %u",
                (long)j->job_id,
                (unsigned)j->uid, (unsigned)j->gid,
                j->status,
                (long)j->submit_time, (long)j->begin_time, (long)j->term_time,
                j->num_cpu, j->num_hosts, j->num_gpus,
                (unsigned long)j->mem_mb, (unsigned long)j->storage_mb,
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
    if (write_qstr(fp, j->gpu_type) < 0)
        return -1;
    if (write_qstr(fp, j->from_host) < 0)
        return -1;
    if (write_qstr(fp, j->hosts) < 0)
        return -1;
    if (write_qstr(fp, j->comment) < 0)
        return -1;
    if (fprintf(fp, "\n") < 0)
        return -1;
    return 0;
}

int log_parse_job_new(const struct event_rec *rec, struct log_job_new *j)
{
    const char *p = rec->rest;
    int cc;
    int n = sscanf(p, " %ld %u %u %d %ld %ld %ld %d %d %d %lu %lu %u%n",
                   &j->job_id,
                   (unsigned *)&j->uid, (unsigned *)&j->gid,
                   &j->status,
                   &j->submit_time, &j->begin_time, &j->term_time,
                   &j->num_cpu, &j->num_hosts, &j->num_gpus,
                   (unsigned long *)&j->mem_mb,
                   (unsigned long *)&j->storage_mb,
                   &j->flags, &cc);
    if (n != 12)
        return -1;
    p += cc;

    if (read_qstr(&p, j->username,     sizeof(j->username)) < 0)
        return -1;
    if (read_qstr(&p, j->job_name,     sizeof(j->job_name)) < 0)
        return -1;
    if (read_qstr(&p, j->queue,        sizeof(j->queue)) < 0)
        return -1;
    if (read_qstr(&p, j->project_name, sizeof(j->project_name)) < 0)
        return -1;
    if (read_qstr(&p, j->gpu_type,     sizeof(j->gpu_type)) < 0)
        return -1;
    if (read_qstr(&p, j->from_host,    sizeof(j->from_host)) < 0)
        return -1;
    if (read_qstr(&p, j->hosts,        sizeof(j->hosts)) < 0)
        return -1;
    if (read_qstr(&p, j->comment,      sizeof(j->comment)) < 0)
        return -1;

    return 0;
}

/* -----------------------------------------------------------------------
 * JOB_START
 * ----------------------------------------------------------------------- */

int log_write_job_start(FILE *fp, const struct log_job_start *j)
{
    if (write_hdr(fp, EVENT_JOB_START, j->start_time) < 0)
        return -1;
    if (fprintf(fp, " %ld", (long)j->job_id) < 0)
        return -1;
    if (write_qstr(fp, j->exec_host) < 0)
        return -1;
    if (fprintf(fp, "\n") < 0)
        return -1;
    return 0;
}

int log_parse_job_start(const struct event_rec *rec, struct log_job_start *j)
{
    const char *p = rec->rest;
    int cc;
    int n = sscanf(p, " %ld%n", &j->job_id, &cc);
    if (n != 1)
        return -1;
    p += cc;

    if (read_qstr(&p, j->exec_host, sizeof(j->exec_host)) < 0)
        return -1;

    j->start_time = rec->event_time;

    return 0;
}

/* -----------------------------------------------------------------------
 * JOB_ACCEPT
 * ----------------------------------------------------------------------- */

int log_write_job_accept(FILE *fp, const struct log_job_accept *j)
{
    if (write_hdr(fp, EVENT_JOB_ACCEPT, j->accept_time) < 0)
        return -1;
    if (fprintf(fp, " %ld %d\n", (long)j->job_id, j->job_pid) < 0)
        return -1;
    return 0;
}

int log_parse_job_accept(const struct event_rec *rec, struct log_job_accept *j)
{
    int n = sscanf(rec->rest, " %ld %d", &j->job_id, &j->job_pid);
    if (n != 2)
        return -1;

    j->accept_time = rec->event_time;

    return 0;
}

/* -----------------------------------------------------------------------
 * JOB_EXECUTE
 * ----------------------------------------------------------------------- */

int log_write_job_execute(FILE *fp, const struct log_job_execute *j)
{
    if (write_hdr(fp, EVENT_JOB_EXECUTE, j->execute_time) < 0)
        return -1;
    if (fprintf(fp, " %ld %d", (long)j->job_id, j->job_pid) < 0)
        return -1;
    if (write_qstr(fp, j->cwd) < 0)
        return -1;
    if (fprintf(fp, "\n") < 0)
        return -1;
    return 0;
}

int log_parse_job_execute(const struct event_rec *rec, struct log_job_execute *j)
{
    const char *p = rec->rest;
    int cc;
    int n = sscanf(p, " %ld %d%n", &j->job_id, &j->job_pid, &cc);
    if (n != 2)
        return -1;
    p += cc;

    if (read_qstr(&p, j->cwd, sizeof(j->cwd)) < 0)
        return -1;

    j->execute_time = rec->event_time;

    return 0;
}

/* -----------------------------------------------------------------------
 * JOB_SIGNAL
 * ----------------------------------------------------------------------- */

int log_write_job_signal(FILE *fp, const struct log_job_signal *j)
{
    if (write_hdr(fp, EVENT_JOB_SIGNAL, j->signal_time) < 0)
        return -1;
    if (fprintf(fp, " %ld %d %u\n",
                (long)j->job_id, j->signal_num, j->uid) < 0)
        return -1;
    return 0;
}

int log_parse_job_signal(const struct event_rec *rec, struct log_job_signal *j)
{
    int n = sscanf(rec->rest, " %ld %d %u",
                   &j->job_id, &j->signal_num, &j->uid);
    if (n != 3)
        return -1;

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
    if (fprintf(fp, " %ld %u %d %d %ld %ld %ld %.4f",
                (long)j->job_id, (unsigned)j->uid,
                j->status, j->exit_status,
                (long)j->submit_time, (long)j->start_time, (long)j->end_time,
                j->cpu_time) < 0)
        return -1;
    if (write_qstr(fp, j->job_name) < 0)
        return -1;
    if (write_qstr(fp, j->queue) < 0)
        return -1;
    if (write_qstr(fp, j->from_host) < 0)
        return -1;
    if (write_qstr(fp, j->exec_host) < 0)
        return -1;
    if (fprintf(fp, "\n") < 0)
        return -1;
    return 0;
}

int log_parse_job_finish(const struct event_rec *rec, struct log_job_finish *j)
{
    const char *p = rec->rest;
    int cc;
    int n = sscanf(p, " %ld %u %d %d %ld %ld %ld %lf%n",
                   &j->job_id, (unsigned *)&j->uid,
                   &j->status, &j->exit_status,
                   &j->submit_time, &j->start_time, &j->end_time,
                   &j->cpu_time, &cc);
    if (n != 8)
        return -1;
    p += cc;

    if (read_qstr(&p, j->job_name,  sizeof(j->job_name)) < 0)
        return -1;
    if (read_qstr(&p, j->queue,     sizeof(j->queue)) < 0)
        return -1;
    if (read_qstr(&p, j->from_host, sizeof(j->from_host)) < 0)
        return -1;
    if (read_qstr(&p, j->exec_host, sizeof(j->exec_host)) < 0)
        return -1;

    return 0;
}

/* -----------------------------------------------------------------------
 * JOB_PEND_SUSP / JOB_PEND_RESUME / JOB_SUSP
 * ----------------------------------------------------------------------- */

int log_write_job_pend_susp(FILE *fp, const struct log_job_pend_susp *j)
{
    if (write_hdr(fp, EVENT_JOB_PEND_SUSP, j->event_time) < 0)
        return -1;
    if (fprintf(fp, " %ld\n", (long)j->job_id) < 0)
        return -1;
    return 0;
}

int log_parse_job_pend_susp(const struct event_rec *rec,
                            struct log_job_pend_susp *j)
{
    int n = sscanf(rec->rest, " %ld", &j->job_id);
    if (n != 1)
        return -1;

    j->event_time = rec->event_time;

    return 0;
}

int log_write_job_pend_resume(FILE *fp, const struct log_job_pend_resume *j)
{
    if (write_hdr(fp, EVENT_JOB_PEND_RESUME, j->event_time) < 0)
        return -1;
    if (fprintf(fp, " %ld\n", (long)j->job_id) < 0)
        return -1;
    return 0;
}

int log_parse_job_pend_resume(const struct event_rec *rec,
                              struct log_job_pend_resume *j)
{
    int n = sscanf(rec->rest, " %ld", &j->job_id);
    if (n != 1)
        return -1;

    j->event_time = rec->event_time;

    return 0;
}

int log_write_job_susp(FILE *fp, const struct log_job_susp *j)
{
    if (write_hdr(fp, EVENT_JOB_SUSP, j->event_time) < 0)
        return -1;
    if (fprintf(fp, " %ld\n", (long)j->job_id) < 0)
        return -1;
    return 0;
}

int log_parse_job_susp(const struct event_rec *rec, struct log_job_susp *j)
{
    int n = sscanf(rec->rest, " %ld", &j->job_id);
    if (n != 1)
        return -1;

    j->event_time = rec->event_time;

    return 0;
}
