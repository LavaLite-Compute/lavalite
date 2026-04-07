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
    [EVENT_NULL]        = "NULL",
    [EVENT_JOB_NEW]     = "JOB_NEW",
    [EVENT_JOB_START]   = "JOB_START",
    [EVENT_JOB_ACCEPT]  = "JOB_ACCEPT",
    [EVENT_JOB_EXECUTE] = "JOB_EXECUTE",
    [EVENT_JOB_STATUS]  = "JOB_STATUS",
    [EVENT_JOB_FINISH]  = "JOB_FINISH",
    [EVENT_COUNT]       = NULL,
};

/* -----------------------------------------------------------------------
 * helpers
 * ----------------------------------------------------------------------- */

static int write_hdr(FILE *fp, enum event_type type)
{
    if (fprintf(fp, "\"%s\" \"%d\" %ld",
                event_names[type], LOG_VERSION, (long)time(NULL)) < 0)
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
    const char *s;
    const char *e;
    int len;

    s = *p;
    while (*s == ' ')
        s++;
    if (*s != '"')
        return -1;
    s++;
    e = strchr(s, '"');
    if (e == NULL)
        return -1;
    len = (int)(e - s);
    if (len >= maxlen)
        return -1;
    memcpy(dst, s, len);
    dst[len] = '\0';
    *p = e + 1;
    return 0;
}

static enum event_type parse_event_type(const char *name)
{
    int i;

    for (i = 1; i < EVENT_COUNT; i++) {
        if (strcmp(name, event_names[i]) == 0)
            return (enum event_type)i;
    }
    return EVENT_NULL;
}

/* -----------------------------------------------------------------------
 * header
 * ----------------------------------------------------------------------- */

int log_read_hdr(FILE *fp, int *lineno, struct event_rec *rec)
{
    char line[LL_BUFSIZ_4K];
    char etype[LL_BUFSIZ_64];
    char ver[16];
    long ts;
    int cc;

    for (;;) {
        if (fgets(line, sizeof(line), fp) == NULL)
            return -1;
        (*lineno)++;
        if (line[0] != '#' && line[0] != '\n')
            break;
    }

    if (sscanf(line, " \"%63[^\"]\" \"%15[^\"]\" %ld%n",
               etype, ver, &ts, &cc) != 3)
        return -1;

    rec->version    = atoi(ver);
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
    if (write_hdr(fp, EVENT_JOB_NEW) < 0)
        return -1;
    if (fprintf(fp, " %ld %d %d %ld %ld %ld %d %d %lu",
                (long)j->job_id, (int)j->uid, j->status,
                (long)j->submit_time, (long)j->begin_time, (long)j->term_time,
                j->num_cpu, j->num_hosts, (unsigned long)j->mem_mb) < 0)
        return -1;
    if (write_qstr(fp, j->job_name) < 0)
        return -1;
    if (write_qstr(fp, j->queue) < 0)
        return -1;
    if (write_qstr(fp, j->from_host) < 0)
        return -1;
    if (write_qstr(fp, j->cwd) < 0)
        return -1;
    if (write_qstr(fp, j->command) < 0)
        return -1;
    if (write_qstr(fp, j->in_file) < 0)
        return -1;
    if (write_qstr(fp, j->out_file) < 0)
        return -1;
    if (write_qstr(fp, j->err_file) < 0)
        return -1;
    if (write_qstr(fp, j->project_name) < 0)
        return -1;
    if (write_qstr(fp, j->hosts) < 0)
        return -1;
    if (fprintf(fp, "\n") < 0)
        return -1;
    return 0;
}

int log_parse_job_new(const struct event_rec *rec, struct log_job_new *j)
{
    const char *p;
    int cc;
    int n;

    p = rec->rest;
    n = sscanf(p, " %ld %d %d %ld %ld %ld %d %d %lu%n",
               &j->job_id, (int *)&j->uid, &j->status,
               &j->submit_time, &j->begin_time, &j->term_time,
               &j->num_cpu, &j->num_hosts, &j->mem_mb, &cc);
    if (n != 9)
        return -1;
    p += cc;

    if (read_qstr(&p, j->job_name,     sizeof(j->job_name)) < 0)
        return -1;
    if (read_qstr(&p, j->queue,        sizeof(j->queue)) < 0)
        return -1;
    if (read_qstr(&p, j->from_host,    sizeof(j->from_host)) < 0)
        return -1;
    if (read_qstr(&p, j->cwd,          sizeof(j->cwd)) < 0)
        return -1;
    if (read_qstr(&p, j->command,      sizeof(j->command)) < 0)
        return -1;
    if (read_qstr(&p, j->in_file,      sizeof(j->in_file)) < 0)
        return -1;
    if (read_qstr(&p, j->out_file,     sizeof(j->out_file)) < 0)
        return -1;
    if (read_qstr(&p, j->err_file,     sizeof(j->err_file)) < 0)
        return -1;
    if (read_qstr(&p, j->project_name, sizeof(j->project_name)) < 0)
        return -1;
    if (read_qstr(&p, j->hosts,        sizeof(j->hosts)) < 0)
        return -1;

    return 0;
}

/* -----------------------------------------------------------------------
 * JOB_START
 * ----------------------------------------------------------------------- */

int log_write_job_start(FILE *fp, const struct log_job_start *j)
{
    if (write_hdr(fp, EVENT_JOB_START) < 0)
        return -1;
    if (fprintf(fp, " %ld %d %d %d",
                (long)j->job_id, j->status,
                j->job_pid, j->num_exec_hosts) < 0)
        return -1;
    if (write_qstr(fp, j->exec_hosts) < 0)
        return -1;
    if (fprintf(fp, "\n") < 0)
        return -1;
    return 0;
}

int log_parse_job_start(const struct event_rec *rec, struct log_job_start *j)
{
    const char *p;
    int cc;
    int n;

    p = rec->rest;
    n = sscanf(p, " %ld %d %d %d%n",
               &j->job_id, &j->status, &j->job_pid, &j->num_exec_hosts, &cc);
    if (n != 4)
        return -1;
    p += cc;

    if (read_qstr(&p, j->exec_hosts, sizeof(j->exec_hosts)) < 0)
        return -1;

    return 0;
}

/* -----------------------------------------------------------------------
 * JOB_ACCEPT
 * ----------------------------------------------------------------------- */

int log_write_job_accept(FILE *fp, const struct log_job_accept *j)
{
    if (write_hdr(fp, EVENT_JOB_ACCEPT) < 0)
        return -1;
    if (fprintf(fp, " %ld %d\n", (long)j->job_id, j->job_pid) < 0)
        return -1;
    return 0;
}

int log_parse_job_accept(const struct event_rec *rec, struct log_job_accept *j)
{
    int n;

    n = sscanf(rec->rest, " %ld %d", &j->job_id, &j->job_pid);
    if (n != 2)
        return -1;
    return 0;
}

/* -----------------------------------------------------------------------
 * JOB_EXECUTE
 * ----------------------------------------------------------------------- */

int log_write_job_execute(FILE *fp, const struct log_job_execute *j)
{
    if (write_hdr(fp, EVENT_JOB_EXECUTE) < 0)
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
    const char *p;
    int cc;
    int n;

    p = rec->rest;
    n = sscanf(p, " %ld %d%n", &j->job_id, &j->job_pid, &cc);
    if (n != 2)
        return -1;
    p += cc;

    if (read_qstr(&p, j->cwd, sizeof(j->cwd)) < 0)
        return -1;

    return 0;
}

/* -----------------------------------------------------------------------
 * JOB_STATUS
 * ----------------------------------------------------------------------- */

int log_write_job_status(FILE *fp, const struct log_job_status *j)
{
    if (write_hdr(fp, EVENT_JOB_STATUS) < 0)
        return -1;
    if (fprintf(fp, " %ld %d %.4f %ld %d\n",
                (long)j->job_id, j->status,
                j->cpu_time, (long)j->end_time, j->exit_status) < 0)
        return -1;
    return 0;
}

int log_parse_job_status(const struct event_rec *rec, struct log_job_status *j)
{
    int n;

    n = sscanf(rec->rest, " %ld %d %lf %ld %d",
               &j->job_id, &j->status,
               &j->cpu_time, &j->end_time, &j->exit_status);
    if (n != 5)
        return -1;
    return 0;
}

/* -----------------------------------------------------------------------
 * JOB_FINISH
 * ----------------------------------------------------------------------- */

int log_write_job_finish(FILE *fp, const struct log_job_finish *j)
{
    if (write_hdr(fp, EVENT_JOB_FINISH) < 0)
        return -1;
    if (fprintf(fp, " %ld %d %d %ld %ld %ld %.4f %d",
                (long)j->job_id, (int)j->uid, j->status,
                (long)j->submit_time, (long)j->start_time, (long)j->end_time,
                j->cpu_time, j->exit_status) < 0)
        return -1;
    if (write_qstr(fp, j->job_name) < 0)
        return -1;
    if (write_qstr(fp, j->queue) < 0)
        return -1;
    if (write_qstr(fp, j->from_host) < 0)
        return -1;
    if (write_qstr(fp, j->exec_hosts) < 0)
        return -1;
    if (write_qstr(fp, j->cwd) < 0)
        return -1;
    if (write_qstr(fp, j->command) < 0)
        return -1;
    if (fprintf(fp, "\n") < 0)
        return -1;
    return 0;
}

int log_parse_job_finish(const struct event_rec *rec, struct log_job_finish *j)
{
    const char *p;
    int cc;
    int n;

    p = rec->rest;
    n = sscanf(p, " %ld %d %d %ld %ld %ld %lf %d%n",
               &j->job_id, (int *)&j->uid, &j->status,
               &j->submit_time, &j->start_time, &j->end_time,
               &j->cpu_time, &j->exit_status, &cc);
    if (n != 8)
        return -1;
    p += cc;

    if (read_qstr(&p, j->job_name,   sizeof(j->job_name)) < 0)
        return -1;
    if (read_qstr(&p, j->queue,      sizeof(j->queue)) < 0)
        return -1;
    if (read_qstr(&p, j->from_host,  sizeof(j->from_host)) < 0)
        return -1;
    if (read_qstr(&p, j->exec_hosts, sizeof(j->exec_hosts)) < 0)
        return -1;
    if (read_qstr(&p, j->cwd,        sizeof(j->cwd)) < 0)
        return -1;
    if (read_qstr(&p, j->command,    sizeof(j->command)) < 0)
        return -1;

    return 0;
}
