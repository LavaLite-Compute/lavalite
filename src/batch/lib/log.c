// Copyright (C) LavaLite Contributors
// GPL v2

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
    dst[len] = 0;
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

static int read_job_new(const char *p, struct log_job *j)
{
    int cc;
    int n;

    n = sscanf(p, " %ld %d %d %ld %ld %ld %d %d %lu%n",
               &j->job_id, &j->uid, &j->status,
               &j->submit_time, &j->begin_time, &j->term_time,
               &j->num_cpu, &j->num_hosts, &j->mem_mb, &cc);
    if (n != 9)
        return -1;
    p += cc;

    if (read_qstr(&p, j->job_name, sizeof(j->job_name)) < 0)
        return -1;
    if (read_qstr(&p, j->queue, sizeof(j->queue)) < 0)
        return -1;
    if (read_qstr(&p, j->from_host, sizeof(j->from_host)) < 0)
        return -1;
    if (read_qstr(&p, j->cwd, sizeof(j->cwd)) < 0)
        return -1;
    if (read_qstr(&p, j->command, sizeof(j->command)) < 0)
        return -1;
    if (read_qstr(&p, j->in_file, sizeof(j->in_file)) < 0)
        return -1;
    if (read_qstr(&p, j->out_file, sizeof(j->out_file)) < 0)
        return -1;
    if (read_qstr(&p, j->err_file, sizeof(j->err_file)) < 0)
        return -1;
    if (read_qstr(&p, j->project_name, sizeof(j->project_name)) < 0)
        return -1;
    if (read_qstr(&p, j->hosts, sizeof(j->hosts)) < 0)
        return -1;

    return 0;
}

static int read_job_start(const char *p, struct log_job *j)
{
    int n, cc;

    n = sscanf(p, " %ld %d %d %d%n",
               &j->job_id, &j->status, &j->job_pid, &j->num_exec_hosts, &cc);
    if (n != 4)
        return -1;
    p += cc;

    if (read_qstr(&p, j->exec_hosts, sizeof(j->exec_hosts)) < 0)
        return -1;

    return 0;
}

static int read_job_accept(const char *p, struct log_job *j)
{
    int n;

    n = sscanf(p, " %ld %d", &j->job_id, &j->job_pid);
    if (n != 2)
        return -1;
    return 0;
}

static int read_job_execute(const char *p, struct log_job *j)
{
    int n, cc;

    n = sscanf(p, " %ld %d%n", &j->job_id, &j->job_pid, &cc);
    if (n != 2)
        return -1;
    p += cc;

    if (read_qstr(&p, j->cwd, sizeof(j->cwd)) < 0)
        return -1;

    return 0;
}

static int read_job_status(const char *p, struct log_job *j)
{
    int n;

    n = sscanf(p, " %ld %d %lf %ld %d",
               &j->job_id, &j->status,
               &j->cpu_time, &j->end_time, &j->exit_status);
    if (n != 5)
        return -1;
    return 0;
}

static int read_job_finish(const char *p, struct log_job *j)
{
    int n, cc;

    n = sscanf(p, " %ld %d %d %ld %ld %ld %lf %d%n",
               &j->job_id, &j->uid, &j->status,
               &j->submit_time, &j->start_time, &j->end_time,
               &j->cpu_time, &j->exit_status, &cc);
    if (n != 8)
        return -1;
    p += cc;

    if (read_qstr(&p, j->job_name, sizeof(j->job_name)) < 0)
        return -1;
    if (read_qstr(&p, j->queue, sizeof(j->queue)) < 0)
        return -1;
    if (read_qstr(&p, j->from_host, sizeof(j->from_host)) < 0)
        return -1;
    if (read_qstr(&p, j->exec_hosts, sizeof(j->exec_hosts)) < 0)
        return -1;
    if (read_qstr(&p, j->cwd, sizeof(j->cwd)) < 0)
        return -1;
    if (read_qstr(&p, j->command, sizeof(j->command)) < 0)
        return -1;

    return 0;
}

static int write_job_new(FILE *fp, struct log_job *j)
{
    if (fprintf(fp, " %ld %d %d %ld %ld %ld %d %d %lu",
                j->job_id, j->uid, j->status,
                j->submit_time, j->begin_time, j->term_time,
                j->num_cpu, j->num_hosts, j->mem_mb) < 0)
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

static int write_job_start(FILE *fp, struct log_job *j)
{
    if (fprintf(fp, " %ld %d %d %d",
                j->job_id, j->status, j->job_pid, j->num_exec_hosts) < 0)
        return -1;
    if (write_qstr(fp, j->exec_hosts) < 0)
        return -1;
    if (fprintf(fp, "\n") < 0)
        return -1;
    return 0;
}

static int write_job_accept(FILE *fp, struct log_job *j)
{
    if (fprintf(fp, " %ld %d\n", j->job_id, j->job_pid) < 0)
        return -1;
    return 0;
}

static int write_job_execute(FILE *fp, struct log_job *j)
{
    if (fprintf(fp, " %ld %d", j->job_id, j->job_pid) < 0)
        return -1;
    if (write_qstr(fp, j->cwd) < 0)
        return -1;
    if (fprintf(fp, "\n") < 0)
        return -1;
    return 0;
}

static int write_job_status(FILE *fp, struct log_job *j)
{
    if (fprintf(fp, " %ld %d %.4f %ld %d\n",
                j->job_id, j->status,
                j->cpu_time, j->end_time, j->exit_status) < 0)
        return -1;
    return 0;
}

static int write_job_finish(FILE *fp, struct log_job *j)
{
    if (fprintf(fp, " %ld %d %d %ld %ld %ld %.4f %d",
                j->job_id, j->uid, j->status,
                j->submit_time, j->start_time, j->end_time,
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

int log_read(FILE *fp, int *lineno, struct event_rec *rec)
{
    char line[LL_BUFSIZ_4K];
    for (;;) {
        if (fgets(line, sizeof(line), fp) == NULL)
            return -1;
        (*lineno)++;
        if (line[0] != '#' && line[0] != '\n')
            break;
    }

    char etype[LL_BUFSIZ_64];
    char ver[16];
    long ts;
    int cc;
    if (sscanf(line, " \"%63[^\"]\" \"%15[^\"]\" %ld%n",
               etype, ver, &ts, &cc) != 3)
        return -1;

    rec->version    = atoi(ver);
    rec->event_time = (time_t)ts;
    rec->type       = parse_event_type(etype);

    const char *p;
    p = line + cc;

    int rc = 0;
    switch (rec->type) {
    case EVENT_JOB_NEW:
        rc = read_job_new(p, &rec->job);
        break;
    case EVENT_JOB_START:
        rc = read_job_start(p, &rec->job);
        break;
    case EVENT_JOB_ACCEPT:
        rc = read_job_accept(p, &rec->job);
        break;
    case EVENT_JOB_EXECUTE:
        rc = read_job_execute(p, &rec->job);
        break;
    case EVENT_JOB_STATUS:
        rc = read_job_status(p, &rec->job);
        break;
    case EVENT_JOB_FINISH:
        rc = read_job_finish(p, &rec->job);
        break;
    default:
        rc = -1;
        break;
    }

    return rc;
}

int log_write(FILE *fp, struct event_rec *rec)
{
    int rc;

    if (rec->type <= EVENT_NULL || rec->type >= EVENT_COUNT)
        return -1;

    if (fprintf(fp, "\"%s\" \"%d\" %ld",
                event_names[rec->type],
                LOG_VERSION,
                rec->event_time) < 0)
        return -1;

    switch (rec->type) {
    case EVENT_JOB_NEW:
        rc = write_job_new(fp, &rec->job);
        break;
    case EVENT_JOB_START:
        rc = write_job_start(fp, &rec->job);
        break;
    case EVENT_JOB_ACCEPT:
        rc = write_job_accept(fp, &rec->job);
        break;
    case EVENT_JOB_EXECUTE:
        rc = write_job_execute(fp, &rec->job);
        break;
    case EVENT_JOB_STATUS:
        rc = write_job_status(fp, &rec->job);
        break;
    case EVENT_JOB_FINISH:
        rc = write_job_finish(fp, &rec->job);
        break;
    default:
        rc = -1;
        break;
    }

    return rc;
}
