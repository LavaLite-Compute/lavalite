*
 * Copyright (C) LavaLite Contributors
 * GPL v2
 */
#pragma once

#include "llbatch.h"

enum daemon_op {
    BATCH_NEW_JOB,
    BATCH_NEW_JOB_REPLY_ACK,
    BATCH_JOB_EXECUTE,
    BATCH_JOB_EXECUTE_ACK,
    BATCH_JOB_FINISH,
    BATCH_JOB_FINISH_ACK,
    BATCH_JOB_SIGNAL,
    BATCH_JOB_SIGNAL_ACK,
    BATCH_COMPACT_DONE,
    BATCH_COMPACT_ACK,
};
