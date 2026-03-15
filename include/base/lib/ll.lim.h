/* Copyright (C) LavaLite Contributors
 * GPL v2
 */

#pragma once

#include "include/ll.h"
#include "include/base/lib/ll.host.h"
#include "include/base/lib/ll.sys.h"
#include "include/base/lib/ll.channel.h"
#include "include/base/lib/ll.conf.h"

struct lim_master {
    struct ll_host host;
    uint16_t tcp_port; // host byte order
};

enum lim_err {
    LIM_OK = 0,
    LIM_ERROR = -1
};

enum lim_proto {
    LIM_GET_CLUSTER_NAME,
    LIM_REPLY_CLUSTER_NAME,

    LIM_GET_MASTER_NAME,
    LIM_REPLY_MASTER_NAME,

    LIM_GET_LOAD,
    LIM_REPLY_LOAD,

    LIM_GET_HOSTS,
    LIM_REPLY_HOSTS,
};
