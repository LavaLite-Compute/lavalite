// Copyright (C) LavaLite Contributors
// GPL v2

#include <string.h>
#include <assert.h>
#include <syslog.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "base/lib/auth.h"
#include "batch/lib/wire.h"
#include "batch/lib/rpc.h"
#include "base/lib/ll.conf.h"
#include "batch/mbd/mbd.h"

void service_init(void)
{
    ll_list_init(&service_list);
    LL_INFO("Subsystem service initialized");
}
