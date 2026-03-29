// Copyright (C) LavaLite Contributors
// GPL V2

#pragma once

enum mbd_err {
    MBD_OK = 0,
    MBD_ERR = -1,
};

enum batch_lib_op {
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

int call_mbd(const void *, size_t, void **, struct protocol_header *);

/* Maximum size of a single environment variable stored in job spec.
* 2 MiB is practically unlimited for real systems (EDA/Lmod etc.) while
* still preventing pathological allocations caused by corrupt jobfiles.
* This matches ARG_MAX ~= 2MB on Rocky9/Ubuntu24.x.
*/
static const size_t LL_ENVVAR_MAX = 2 * 1024 * 1024;
