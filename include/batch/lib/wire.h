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
    int64_t job_id;
    int32_t pid;
};

struct wire_sbd_register {
    char hostname[MAXHOSTNAMELEN];
    int32_t num_jobs;
    struct wire_sbd_job *jobs;
};

/* -----------------------------------------------------------------------
 * job signal  (mbd -> sbd)
 * ----------------------------------------------------------------------- */

struct wire_job_sig {
    int64_t job_id;
    int32_t sig;
    uint32_t uid;
};

/* -----------------------------------------------------------------------
 * job script  (client -> mbd, then mbd -> sbd)
 *
 * len:  bytes on wire, no trailing NUL
 * data: after decode, data[len] is guaranteed NUL for local convenience
 * ----------------------------------------------------------------------- */

struct wire_job_script {
    uint32_t len;
    char *data;
};

/* -----------------------------------------------------------------------
 * job state  (sbd -> mbd)
 * ----------------------------------------------------------------------- */

struct wire_job_state {
    int64_t job_id;
    int32_t state;
};

/* -----------------------------------------------------------------------
 * job start reply  (sbd -> mbd)
 * ----------------------------------------------------------------------- */

struct wire_job_reply {
    int64_t job_id;
    int32_t pid;
    int32_t pgid;
    int32_t state;
};

/* -----------------------------------------------------------------------
 * job start  (mbd -> sbd)
 *
 * Fixed buffers, xdr_opaque throughout.
 * Followed in the same XDR stream by wire_job_script, wire_job_sidecar.
 * ----------------------------------------------------------------------- */

struct wire_job_start {
    int64_t job_id;
    uint32_t uid;
    uint32_t gid;
    uint32_t umask;
    char job_name[LL_BUFSIZ_256];
    char queue[LL_BUFSIZ_64];
    char username[LL_BUFSIZ_64];
    char home_dir[PATH_MAX];
    char cwd[PATH_MAX];
    char command[LL_BUFSIZ_512];
    char in_file[PATH_MAX];
    char out_file[PATH_MAX];
    char err_file[PATH_MAX];
    char hosts[LL_BUFSIZ_4K]; /* sched allocation: "hostA:4,hostB:4" */
    int64_t term_time;
    int32_t gpus_per_host;
    int32_t ncpus;
    uint64_t mem_mb;
    char gpu_type[LL_BUFSIZ_64];
    char gpu_assigned[LL_BUFSIZ_64]; /* e.g. "0,1" — assigned CUDA device IDs */
    struct wire_job_script script; /* job script, encoded last */
};

/* job finish sbd -> mbd
 */
struct wire_job_finish {
    int64_t job_id;
    int32_t state;
    int32_t exit_status;

    int32_t pid;
    uint64_t mem_mb;
    uint64_t swap_mb;
    double cpu_time;
};

struct wire_job_ack {
    int64_t job_id;
    int32_t ack_op;
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
    char submit_host[MAXHOSTNAMELEN];
    char username[LL_BUFSIZ_256];
    char home_dir[PATH_MAX];
    int32_t num_cpus;
    int32_t num_hosts;
    int32_t num_gpus;
    uint64_t mem_mb;
    uint64_t storage_mb;
    char tokenpool[LL_BUFSIZ_256]; /* "name=N[,name=N]..." empty=none */
    uint32_t umask;
    uint32_t flags;
    int64_t begin_time;
    int64_t term_time;
    int64_t susp_time;
    int64_t resume_time;
};

/* -----------------------------------------------------------------------
 * submit reply  (mbd -> client)
 * ----------------------------------------------------------------------- */

struct wire_job_submit_reply {
    int64_t job_id;
};

/* -----------------------------------------------------------------------
 * job info request  (client -> mbd)
 * ----------------------------------------------------------------------- */

struct wire_job_info_req {
    int64_t job_id;
    int32_t flags;
};

/* -----------------------------------------------------------------------
 * job info  (mbd -> client)
 *
 * time fields are int64_t to avoid 2038 on 32-bit.
 * ----------------------------------------------------------------------- */

struct wire_job_info {
    int64_t job_id;
    uint32_t uid;
    int32_t pid;
    int32_t state;
    int32_t exit_status;
    int32_t priority;
    int32_t pend_reason;
    int64_t submit_time;
    int64_t dispatch_time;
    int64_t end_time;
    int64_t susp_time;
    char name[LL_BUFSIZ_64];
    char queue[LL_BUFSIZ_64];
    char submit_host[MAXHOSTNAMELEN];
    char run_hosts[LL_BUFSIZ_4K];
    char comment[LL_BUFSIZ_512];
};

struct wire_job_info_array {
    int32_t njobs;
    struct wire_job_info *jobs;
};

struct wire_job_query {
    int64_t  job_id;
    int32_t  flags;
    int32_t uid;    /* -1 = all */
};

/* -----------------------------------------------------------------------
 * host info  (mbd -> client)
 * ----------------------------------------------------------------------- */

struct wire_host_info {
    char name[MAXHOSTNAMELEN];
    int32_t state;
    int32_t max_jobs;
    int32_t total_cpu;
    int32_t free_cpu;
    int32_t total_gpu;
    int32_t free_gpu;
    uint64_t total_mem_mb;
    uint64_t free_mem_mb;
    uint64_t total_storage_mb;
    uint64_t free_storage_mb;
    int32_t num_jobs;
    int32_t num_run;
    int32_t num_susp;
    char gpu_type[LL_BUFSIZ_64];
    char gpu_ids[LL_BUFSIZ_64];
};

struct wire_host_info_array {
    int32_t nhosts;
    struct wire_host_info *hosts;
};

struct wire_host_admin {
    char name[MAXHOSTNAMELEN];
    int32_t op; /* HOST_CLOSED or 0 for open */
};

/* -----------------------------------------------------------------------
 * host group info  (mbd -> client)
 * ----------------------------------------------------------------------- */

struct wire_group_info {
    char name[LL_BUFSIZ_64];
    int32_t num_members;
    char members[LL_BUFSIZ_1K]; /* space-separated */
};

struct wire_group_info_array {
    int32_t ngroups;
    struct wire_group_info *groups;
};

/* -----------------------------------------------------------------------
 * queue info  (mbd -> client)
 * ----------------------------------------------------------------------- */

struct wire_queue_info {
    char name[LL_BUFSIZ_64];
    char description[LL_BUFSIZ_256];
    int32_t num_hosts;
    char **hosts;
    int32_t num_users;
    char **users;
    int32_t priority;
    int32_t max_jobs;
    int32_t num_jobs;
    int32_t num_pend;
    int32_t num_run;
    int32_t num_susp;
    int32_t num_held;
    int32_t num_cpus_used;
    int32_t num_hosts_used;
    int32_t status;
};

struct wire_queue_info_array {
    int32_t nqueues;
    struct wire_queue_info *queues;
};

/* after wire_queue_info_array */
struct wire_queue_admin {
    char name[LL_BUFSIZ_64];
    int32_t op;   /* QUEUE_OPEN | QUEUE_CLOSED */
};

struct wire_token_info {
    char name[LL_BUFSIZ_64];
    int32_t total;
    int32_t free;
};

struct wire_token_info_array {
    int32_t ntokens;
    struct wire_token_info *tokens;
};

struct wire_job_move {
    int64_t job_id;
    char to_queue[LL_BUFSIZ_64];
};

struct wire_job_priority {
    int64_t job_id;
    int32_t priority;
};

/* -----------------------------------------------------------------------
 * XDR serializers
 * ----------------------------------------------------------------------- */

/* control */
bool_t xdr_wire_sbd_register(XDR *, struct wire_sbd_register *);
bool_t xdr_wire_sbd_job(XDR *, struct wire_sbd_job *);

/* in XDR serializers, admin section */
bool_t xdr_wire_queue_admin(XDR *, struct wire_queue_admin *);
bool_t xdr_wire_host_admin(XDR *xdrs, struct wire_host_admin *);

/* job */
bool_t xdr_wire_job_state(XDR *, struct wire_job_state *);
bool_t xdr_wire_job_script(XDR *, struct wire_job_script *);
bool_t xdr_wire_job_sig(XDR *, struct wire_job_sig *);
bool_t xdr_wire_job_submit(XDR *, struct wire_job_submit *);
bool_t xdr_wire_job_submit_reply(XDR *, struct wire_job_submit_reply *);
bool_t xdr_wire_job_info_req(XDR *, struct wire_job_info_req *);
bool_t xdr_wire_job_info(XDR *, struct wire_job_info *);
bool_t xdr_wire_job_info_array(XDR *, struct wire_job_info_array *);
bool_t xdr_wire_job_start(XDR *, struct wire_job_start *);
bool_t xdr_wire_job_reply(XDR *, struct wire_job_reply *);
bool_t xdr_wire_job_ack(XDR *, struct wire_job_ack *);
bool_t xdr_wire_job_finish(XDR *, struct wire_job_finish *);
bool_t xdr_wire_job_query(XDR *, struct wire_job_query *);

/* host */
bool_t xdr_wire_host_info(XDR *, struct wire_host_info *);
bool_t xdr_wire_host_info_array(XDR *, struct wire_host_info_array *);

/* queue */
bool_t xdr_wire_queue_info(XDR *, struct wire_queue_info *);
bool_t xdr_wire_queue_info_array(XDR *, struct wire_queue_info_array *);

/* group */
bool_t xdr_wire_group_info(XDR *, struct wire_group_info *);
bool_t xdr_wire_group_info_array(XDR *, struct wire_group_info_array *);

bool_t xdr_wire_token_info(XDR *, struct wire_token_info *);
bool_t xdr_wire_token_info_array(XDR *, struct wire_token_info_array *);

bool_t xdr_wire_job_move(XDR *, struct wire_job_move *);
bool_t xdr_wire_job_priority(XDR *, struct wire_job_priority *);
