/* Copyright (C) LavaLite Contributors
 * GPL v2
 */

#pragma once

#include "base/lib/ll.host.h"

struct lim_master {
    struct ll_host host;
};

enum lim_err {
    LIM_OK = 0,
    LIM_ERROR = -1
};

enum lim_proto {
    LIM_GET_CLUSTER_NAME = 1,
    LIM_REPLY_CLUSTER_NAME,

    LIM_GET_MASTER_NAME,
    LIM_REPLY_MASTER_NAME,

    LIM_GET_LOAD,
    LIM_REPLY_LOAD,

    LIM_GET_HOSTS,
    LIM_REPLY_HOSTS,
};
