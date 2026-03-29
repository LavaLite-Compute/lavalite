/*
 * Copyright (C) LavaLite Contributors
 * GPL v2
 */
#pragma once

#include "llbatch.h"

/* -----------------------------------------------------------------------
 * sbd registration
 * ----------------------------------------------------------------------- */

struct wire_sbd_job {
    int64_t  job_id;
    int32_t  pid;
};

struct wire_sbd_register {
    char              hostname[MAXHOSTNAMELEN];
    int32_t           num_jobs;
    struct wire_sbd_job *jobs;
};

/* -----------------------------------------------------------------------
 * job signal  (mbd -> sbd)
 * ----------------------------------------------------------------------- */

struct wire_job_sig {
    int64_t  job_id;
    int32_t  sig;
};

/* -----------------------------------------------------------------------
 * job file (stdin/stdout/stderr content)
 *
 * len:  bytes on wire, no trailing NUL
 * data: after decode, data[len] is guaranteed NUL for local convenience
 * ----------------------------------------------------------------------- */

struct wire_job_file {
    uint32_t  len;
    char     *data;
};

/* -----------------------------------------------------------------------
 * job state  (sbd -> mbd)
 * ----------------------------------------------------------------------- */

struct wire_job_state {
    int64_t  job_id;
    int32_t  state;
};

/* -----------------------------------------------------------------------
 * log compactor notification
 * ----------------------------------------------------------------------- */

struct wire_compact_notify {
    int32_t  status;
    int64_t  compact_time;
};

/* -----------------------------------------------------------------------
 * job submit  (client -> mbd)
 * ----------------------------------------------------------------------- */

struct wire_job_submit {
    char     job_name[LL_BUFSIZ_64];
    char     queue[LL_BUFSIZ_64];
    char     hosts[LL_BUFSIZ_256];        /* space-separated, mbd tokenizes */
    char     command[1024];
    char     in_file[PATH_MAX];
    char     out_file[PATH_MAX];
    char     err_file[PATH_MAX];
    char     project_name[LL_BUFSIZ_64];
    int64_t  begin_time;
    int64_t  term_time;
    int32_t  num_cpu;
    int32_t  num_hosts;
    int32_t  num_gpu;
    uint64_t mem_mb;
};

/* -----------------------------------------------------------------------
 * submit reply  (mbd -> client)
 * ----------------------------------------------------------------------- */

struct wire_submit_reply {
    int64_t  job_id;        /* -1 on error */
    int32_t  error;         /* LBE_* on error, LBE_NO_ERROR on success */
};

/* -----------------------------------------------------------------------
 * job resources (runtime)
 * ----------------------------------------------------------------------- */

struct wire_job_resources {
    int32_t job_pid;
};

/* -----------------------------------------------------------------------
 * job info  (mbd -> client)
 * ----------------------------------------------------------------------- */
struct wire_job_info {
    int64_t  job_id;

    uint32_t uid;

    int32_t  status;
    int32_t  exit_status;

    int64_t  submit_time;
    int64_t  start_time;
    int64_t  end_time;

    float    cpu_time;

    char     from_host[MAXHOSTNAMELEN];

    int32_t  num_exec_hosts;
    char     exec_host[MAXHOSTNAMELEN];

    char     job_name[LL_BUFSIZ_64];
    char     queue[LL_BUFSIZ_64];

    int32_t  num_cpu;
    int32_t  num_hosts;
    int32_t  num_gpu;

    uint64_t mem_mb;

    int64_t  begin_time;
    int64_t  term_time;

    int32_t  job_pid;
    struct wire_job_resources resources;
};

/* -----------------------------------------------------------------------
 * job info array (mbd -> client)
 * ----------------------------------------------------------------------- */

struct wire_job_info_array {
    int32_t              njobs;
    struct wire_job_info *jobs;
};

/* -----------------------------------------------------------------------
 * host info  (mbd -> client, reply to lb_hostinfo)
 * ----------------------------------------------------------------------- */

struct wire_host_info {
    char     host[MAXHOSTNAMELEN];
    int32_t  status;
    int32_t  max_jobs;
    int32_t  num_jobs;
    int32_t  num_run;
    int32_t  num_susp;
    float    cpu_factor;
};

struct wire_host_info_array {
    int32_t  nhosts;
    struct wire_host_info *hosts;
};

/* -----------------------------------------------------------------------
 * host group info (mbd -> client)
 * ----------------------------------------------------------------------- */

struct wire_group_info {
    char     name[LL_BUFSIZ_64];
    int32_t  num_members;
    char     members[LL_BUFSIZ_1K];  /* space-separated */
};

struct wire_group_info_array {
    int32_t  ngroups;
    struct wire_group_info *groups;
};

/* -----------------------------------------------------------------------
 * queue info (mbd -> client)
 * ----------------------------------------------------------------------- */

struct wire_queue_info {
    char     name[LL_BUFSIZ_64];
    char     description[LL_BUFSIZ_256];
    char     hosts[LL_BUFSIZ_256];   /* host group name */
    int32_t  priority;
    int32_t  max_jobs;
    int32_t  num_pend;
    int32_t  num_run;
    int32_t  num_susp;
    int32_t  status;
};

struct wire_queue_info_array {
    int32_t nqueues;
    struct wire_queue_info *queues;
};

/* -----------------------------------------------------------------------
 * XDR serializers
 * ----------------------------------------------------------------------- */

/* control */
bool_t xdr_wire_compact_notify(XDR *, struct wire_compact_notify *);
bool_t xdr_wire_sbd_register(XDR *, struct wire_sbd_register *);

/* job */
bool_t xdr_wire_job_state(XDR *, struct wire_job_state *);
bool_t xdr_wire_job_file(XDR *, struct wire_job_file *);
bool_t xdr_wire_job_sig(XDR *, struct wire_job_sig *);
bool_t xdr_wire_job_submit(XDR *, struct wire_job_submit *);
bool_t xdr_wire_submit_reply(XDR *, struct wire_submit_reply *);

bool_t xdr_wire_job_resources(XDR *, struct wire_job_resources *);
bool_t xdr_wire_job_info(XDR *, struct wire_job_info *);
bool_t xdr_wire_job_info_array(XDR *, struct wire_job_info_array *);

/* host */
bool_t xdr_wire_host_info(XDR *, struct wire_host_info *);
bool_t xdr_wire_host_info_array(XDR *, struct wire_host_info_array *);

/* queue */
bool_t xdr_wire_queue_info(XDR *, struct wire_queue_info *);
bool_t xdr_wire_queue_info_array(XDR *, struct wire_queue_info_array *);

/* group */
bool_t xdr_wire_group_info(XDR *, struct wire_group_info *);
bool_t xdr_wire_group_info_array(XDR *, struct wire_group_info_array *);
