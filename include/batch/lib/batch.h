// Copyright (C) LavaLite Contributors
// GPL V2


#include "llbatch.h"

#include "base/lib/ll.sys.h"
#include "base/lib/ll.wire.h"
#include "base/lib/ll.channel.h"
#include "base/lib/ll.syslog.h"
#include "base/lib/ll.conf.h"
#include "base/lib/ll.host.h"
#include "base/lib/ll.hash.h"
#include "base/lib/ll.list.h"

#include "batch/lib/wire.h"

enum mbd_err {
    MBD_OK = 0,
    MBD_ERR = -1,
};

enum batch_op {
    BATCH_JOB_SUB = 1,
    BATCH_JOB_SUB_ACK,

    BATCH_JOB_SIG,
    BATCH_JOB_SIG_ACK,

    BATCH_JOB_INFO,
    BATCH_JOB_INFO_ACK,

    BATCH_HOST_INFO,
    BATCH_HOST_INFO_ACK,

    BATCH_QUEUE_INFO,
    BATCH_QUEUE_INFO_ACK,

    BATCH_GROUP_INFO,
    BATCH_GROUP_INFO_ACK,

    BATCH_SBD_REGISTER,
    BATCH_SBD_REGISTER_ACK,

    BATCH_COMPACT_DONE,
    BATCH_COMPACT_FAILED,
    BATCH_COMPACT_ACK,
};

/* batch/mbd/job.h - internal, fixed buffers, lives on disk */
struct job_sub {
    char     job_name[LL_BUFSIZ_64];
    char     queue[LL_BUFSIZ_64];
    char     hosts[LL_BUFSIZ_1K];
    char     group[LL_BUFSIZ_64];
    int32_t  num_cpu;
    int32_t  num_hosts;
    uint64_t mem_mb;
    int32_t  num_gpu;
    time_t   begin_time;
    time_t   term_time;
    char     in_file[PATH_MAX];
    char     out_file[PATH_MAX];
    char     err_file[PATH_MAX];
    char     command[LL_BUFSIZ_1K];
    char     project_name[LL_BUFSIZ_64];
};

struct host_info {
    char    host[MAXHOSTNAMELEN];
    int32_t status;
    int32_t max_jobs;
    int32_t num_jobs;
    int32_t num_run;
    int32_t num_susp;
};

struct queue_info {
    char    queue[LL_BUFSIZ_64];
    char    description[LL_BUFSIZ_256];
    int32_t priority;
    int32_t num_pend;
    int32_t num_run;
    int32_t num_susp;
};

struct job_signal {
    int64_t job_id;
    int32_t signal;
};

struct host_group {
    char name[LL_BUFSIZ_64];
    char members[LL_BUFSIZ_1K];  /* space-separated */
};

int call_mbd(const void *, size_t, void **, struct protocol_header *);
