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
    BATCH_JOB_FINISH,
    BATCH_JOB_FINISH_ACK,
    // no ack to signal has not reply but triggers a job status change
    BATCH_SBD_JOB_SIGNAL,
    BATCH_SBD_JOB_SIGNAL_REPLY,
    BATCH_JOB_MISSING,
    BATCH_QUEUE_ADMIN,
    BATCH_QUEUE_ADMIN_ACK,
    BATCH_HOST_ADMIN,
    BATCH_HOST_ADMIN_ACK,
    BATCH_JOB_MOVE,
    BATCH_JOB_MOVE_ACK,
    BATCH_JOB_PRIORITY,
    BATCH_JOB_PRIORITY_ACK,
    BATCH_SERVICE_START,
    BATCH_SERVICE_START_ACK,
    BATCH_SERVICE_INFO,
    BATCH_SERVICE_INFO_ACK,
    BATCH_SERVICE_STOP,
    BATCH_SERVICE_STOP_ACK,
    // spd (service_proxy) registration -- spd connects in as an
    // ordinary client, same accept path as sbd, so it gets the same
    // register/register_ack shape.
    BATCH_SP_REGISTER,
    BATCH_SP_REGISTER_ACK,
    // mbd -> spd: per-instance port grammar, same request/ack shape
    // as everything else on this channel (chan_enqueue/chan_dequeue,
    // XDR, no separate socket or text protocol).
    BATCH_SVC_ADD,
    BATCH_SVC_ADD_ACK,
    BATCH_SVC_UPDATE,
    BATCH_SVC_UPDATE_ACK,
    BATCH_SVC_REMOVE,
    BATCH_SVC_REMOVE_ACK,
};

int call_mbd(const void *, size_t, void **, struct protocol_header *);
int call_mbd_timeout(const void *, size_t, void **,
                     struct protocol_header *, int);
const char *batch_op_str(enum batch_lib_op);
