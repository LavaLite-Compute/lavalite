// Copyright (C) LavaLite Contributors
// GPL V2

#pragma once

#include "base/lib/ll.protocol.h"

enum mbd_err {
    MBD_OK = 0,
    /* non-zero status is an errno value */
};

enum batch_lib_op {
    BATCH_JOB_SUBMIT = 1,
    BATCH_JOB_SUBMIT_ACK,

    BATCH_JOB_SIGNAL,
    BATCH_JOB_SIGNAL_ACK,

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

    BATCH_TOKEN_INFO,
    BATCH_TOKEN_INFO_ACK,

    // sbd - mbd messages
    BATCH_NEW_JOB,
    BATCH_NEW_JOB_REPLY,
    BATCH_NEW_JOB_REPLY_ACK,
    BATCH_JOB_EXECUTE,
    BATCH_JOB_EXECUTE_ACK,
    BATCH_JOB_FINISH,
    BATCH_JOB_FINISH_ACK,
    // no ack to signal has not reply but triggers a job status change
    BATCH_SBD_JOB_SIGNAL,
    BATCH_SBD_JOB_SIGNAL_REPLY,
    BATCH_JOB_UNKNOWN,

    BATCH_QUEUE_ADMIN,
    BATCH_QUEUE_ADMIN_ACK,

    BATCH_HOST_ADMIN,
    BATCH_HOST_ADMIN_ACK,
};

int call_mbd(const void *, size_t, void **, struct protocol_header *);
const char *batch_op_str(enum batch_lib_op);
