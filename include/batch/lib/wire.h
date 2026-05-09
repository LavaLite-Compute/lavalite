/*
 * Copyright (C) LavaLite Contributors
 * GPL v2
 */
#pragma once

#include <stdint.h>
#include <limits.h>
#include <netdb.h>
#include <rpc/xdr.h>

#include "base/lib/ll.bufsiz.h"
#include "base/lib/ll.host.h"

#include "llbatch.h"

/* -----------------------------------------------------------------------
 * sbd registration
 * ----------------------------------------------------------------------- */

struct wire_sbd_job {
    int64_t  job_id;
    int32_t  pid;
};

struct wire_sbd_register {
    char                 hostname[MAXHOSTNAMELEN];
    int32_t              num_jobs;
    struct wire_sbd_job *jobs;
};

/* -----------------------------------------------------------------------
 * job signal  (mbd -> sbd)
 * ----------------------------------------------------------------------- */

struct wire_job_sig {
    int64_t  job_id;
    int32_t  sig;
    uint32_t uid;
};

/* -----------------------------------------------------------------------
 * job script  (client -> mbd, then mbd -> sbd)
 *
 * len:  bytes on wire, no trailing NUL
 * data: after decode, data[len] is guaranteed NUL for local convenience
 * ----------------------------------------------------------------------- */

struct wire_job_script {
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
 * job start reply  (sbd -> mbd)
 * ----------------------------------------------------------------------- */

struct wire_job_reply {
    int64_t  job_id;
    int32_t  pid;
    int32_t  pgid;
    int32_t  status;
};

/* -----------------------------------------------------------------------
 * job start  (mbd -> sbd)
 *
 * Fixed buffers, xdr_opaque throughout.
 * Followed in the same XDR stream by wire_job_script, wire_job_sidecar.
 * ----------------------------------------------------------------------- */

struct wire_job_start {
    int64_t  job_id;
    uint32_t uid;
    uint32_t gid;
    uint32_t umask;

    char     job_name[LL_BUFSIZ_256];
    char     queue[LL_BUFSIZ_64];
    char     username[LL_BUFSIZ_64];
    char     home_dir[PATH_MAX];
    char     cwd[PATH_MAX];
    char     command[LL_BUFSIZ_512];
    char     in_file[PATH_MAX];
    char     out_file[PATH_MAX];
    char     err_file[PATH_MAX];
    char     hosts[LL_BUFSIZ_4K]; /* sched allocation: "hostA:4,hostB:4" */

    int64_t  term_time;
    int32_t  gpus_per_host;
    char     gpu_type[LL_BUFSIZ_64];

    struct wire_job_script script;   /* job script, encoded last */
};

struct wire_job_ack {
    int64_t job_id;
    int32_t ack_op;
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
 *
 * Fixed buffers encoded with xdr_opaque.
 * Submission context (cwd, uid, ...) filled by llb_submit(),
 * not by the caller.
 * The job script is sent in the same XDR stream immediately after,
 * encoded with xdr_wire_job_script.
 * ----------------------------------------------------------------------- */

struct wire_job_submit {
    char name[LL_BUFSIZ_256];
    char queue[LL_BUFSIZ_256];
    char project[LL_BUFSIZ_64];
    char comment[LL_BUFSIZ_1K];
    char machines[LL_BUFSIZ_4K];
    char in_file[PATH_MAX];
    char out_file[PATH_MAX];
    char err_file[PATH_MAX];
    char cwd[PATH_MAX];
    char depend_cond[LL_BUFSIZ_4K];
    char command[PATH_MAX];
    char gpu_type[LL_BUFSIZ_256];
    char from_host[MAXHOSTNAMELEN];
    char username[LL_BUFSIZ_256];
    char home_dir[PATH_MAX];
    int32_t  num_cpus;
    int32_t  num_hosts;
    int32_t  num_gpus;
    int32_t  wall_seconds;
    uint64_t mem_mb;
    uint64_t storage_mb;
    char     tokenpool[LL_BUFSIZ_256]; /* "name=N[,name=N]..." empty=none */
    uint32_t uid;
    uint32_t gid;
    uint32_t umask;
    uint32_t flags;

    int64_t  begin_time;
    int64_t  term_time;
    int64_t  susp_time;
    int64_t  resume_time;
};

/* -----------------------------------------------------------------------
 * submit reply  (mbd -> client)
 * ----------------------------------------------------------------------- */

struct wire_job_submit_reply {
    int64_t  job_id;
};

/* -----------------------------------------------------------------------
 * job info request  (client -> mbd)
 * ----------------------------------------------------------------------- */

struct wire_job_info_req {
    int64_t  job_id;
    int32_t  flags;
    uint32_t uid;
};

/* -----------------------------------------------------------------------
 * job resources  (embedded in wire_job_info)
 * ----------------------------------------------------------------------- */

struct wire_job_resources {
    int32_t  pid;
    uint64_t mem_mb;
    double   cpu_time;
};

/* -----------------------------------------------------------------------
 * job info  (mbd -> client)
 *
 * time fields are int64_t to avoid 2038 on 32-bit.
 * ----------------------------------------------------------------------- */

struct wire_job_info {
    int64_t  job_id;
    uint32_t uid;
    int32_t  status;
    int32_t  exit_status;
    int32_t  priority;
    int64_t  submit_time;
    int64_t  dispatch_time;
    int64_t  end_time;
    int64_t  susp_time;
    char     name[LL_BUFSIZ_64];
    char     queue[LL_BUFSIZ_64];
    char     from_host[MAXHOSTNAMELEN];
    char     exec_host[MAXHOSTNAMELEN];
    char     comment[LL_BUFSIZ_512];
    struct wire_job_resources res;
};

struct wire_job_info_array {
    int32_t               njobs;
    struct wire_job_info *jobs;
};

/* -----------------------------------------------------------------------
 * host info  (mbd -> client)
 * ----------------------------------------------------------------------- */

struct wire_host_info {
    char     name[MAXHOSTNAMELEN];
    int32_t  status;
    int32_t  max_jobs;
    int32_t  total_cpu;
    int32_t  total_gpu;
    uint64_t total_mem_mb;
    uint64_t total_storage_mb;
    int32_t  num_jobs;
    int32_t  num_run;
    int32_t  num_susp;
};

struct wire_host_info_array {
    int32_t               nhosts;
    struct wire_host_info *hosts;
};

/* -----------------------------------------------------------------------
 * host group info  (mbd -> client)
 * ----------------------------------------------------------------------- */

struct wire_group_info {
    char    name[LL_BUFSIZ_64];
    int32_t num_members;
    char    members[LL_BUFSIZ_1K];  /* space-separated */
};

struct wire_group_info_array {
    int32_t                ngroups;
    struct wire_group_info *groups;
};

/* -----------------------------------------------------------------------
 * queue info  (mbd -> client)
 * ----------------------------------------------------------------------- */

struct wire_queue_info {
    char    name[LL_BUFSIZ_64];
    char    description[LL_BUFSIZ_256];
    char    hosts[LL_BUFSIZ_256];
    int32_t priority;
    int32_t max_jobs;
    int32_t num_pend;
    int32_t num_run;
    int32_t num_susp;
};

struct wire_queue_info_array {
    int32_t                nqueues;
    struct wire_queue_info *queues;
};

/* -----------------------------------------------------------------------
 * XDR serializers
 * ----------------------------------------------------------------------- */

/* control */
bool_t xdr_wire_compact_notify(XDR *, struct wire_compact_notify *);
bool_t xdr_wire_sbd_register(XDR *, struct wire_sbd_register *);
bool_t xdr_wire_sbd_job(XDR *, struct wire_sbd_job *);

/* job */
bool_t xdr_wire_job_state(XDR *, struct wire_job_state *);
bool_t xdr_wire_job_script(XDR *, struct wire_job_script *);
bool_t xdr_wire_job_sig(XDR *, struct wire_job_sig *);
bool_t xdr_wire_job_submit(XDR *, struct wire_job_submit *);
bool_t xdr_wire_job_submit_reply(XDR *, struct wire_job_submit_reply *);
bool_t xdr_wire_job_info_req(XDR *, struct wire_job_info_req *);
bool_t xdr_wire_job_resources(XDR *, struct wire_job_resources *);
bool_t xdr_wire_job_info(XDR *, struct wire_job_info *);
bool_t xdr_wire_job_info_array(XDR *, struct wire_job_info_array *);
bool_t xdr_wire_job_start(XDR *, struct wire_job_start *);
bool_t xdr_wire_job_reply(XDR *, struct wire_job_reply *);
bool_t xdr_wire_job_ack(XDR *, struct wire_job_ack *);

/* host */
bool_t xdr_wire_host_info(XDR *, struct wire_host_info *);
bool_t xdr_wire_host_info_array(XDR *, struct wire_host_info_array *);

/* queue */
bool_t xdr_wire_queue_info(XDR *, struct wire_queue_info *);
bool_t xdr_wire_queue_info_array(XDR *, struct wire_queue_info_array *);

/* group */
bool_t xdr_wire_group_info(XDR *, struct wire_group_info *);
bool_t xdr_wire_group_info_array(XDR *, struct wire_group_info_array *);
