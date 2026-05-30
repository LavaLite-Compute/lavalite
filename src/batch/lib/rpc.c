/* Copyright (C) LavaLite Contributors
 * GPL v2
 */

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/epoll.h>

#include "base/lib/ll.protocol.h"
#include "base/lib/ll.bufsiz.h"
#include "base/lib/ll.conf.h"
#include "base/lib/ll.list.h"
#include "base/lib/ll.channel.h"
#include "base/lib/ll.host.h"
#include "base/lib/ll.sys.h"

#include "batch/lib/rpc.h"
#include "llbatch.h"

static int chan_mbd = -1;
// defaults set in ll_params already
static int conntimeout;
static int recvtimeout;
static struct ll_host mbd_node;
static uint16_t mbd_port;

static int initialized;

static int mbd_rpc_init(void)
{
    if (initialized)
        return 0;

    if (ll_init() < 0) {
        errno = EINVAL;
        return -1;
    }

    if (!ll_atoi(ll_params[LL_MBD_PORT].val, (int *) &mbd_port)) {
        errno = EINVAL;
        return -1;
    }

    if (!ll_atoi(ll_params[LL_API_CONNTIMEOUT].val, &conntimeout)) {
        errno = EINVAL;
        return -1;
    }

    if (!ll_atoi(ll_params[LL_API_RECVTIMEOUT].val, &recvtimeout)) {
        errno = EINVAL;
        return -1;
    }

    if (get_host_by_name(ll_params[LL_MBD_HOST].val, &mbd_node) < 0) {
        errno = EINVAL;
        return -1;
    }
    chan_init();

    initialized = 1;
    return 0;
}

/*
 * Send req_len bytes to MBD via TCP, receive reply.
 * On success: *rep points to allocated payload (caller must free),
 *             reply_hdr is filled.
 * On error: returns -1, sets lserrno.
 * Keeps a persistent TCP connection; reconnects on failure.
 */
int call_mbd(const void *req, size_t req_len, void **rep,
             struct protocol_header *reply_hdr)
{
    if (mbd_rpc_init() < 0)
        return -1;

    if (chan_mbd < 0) {
        chan_mbd = chan_tcp_client();
        if (chan_mbd < 0) {
            return -1;
        }

        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        get_host_addrv4(&mbd_node, &addr);
        addr.sin_family = AF_INET;
        addr.sin_port = htons((uint16_t) mbd_port);

        if (chan_connect(chan_mbd, &addr, conntimeout) < 0) {
            chan_close(chan_mbd);
            chan_mbd = -1;
            return -1;
        }
    }

    struct chan_buffer sndbuf = {.data = (void *) req, .len = req_len};
    struct chan_buffer rcvbuf = {0};

    if (chan_rpc(chan_mbd, &sndbuf, &rcvbuf, reply_hdr, recvtimeout) < 0) {
        chan_close(chan_mbd);
        chan_mbd = -1;
        return -1;
    }

    if (reply_hdr->status != MBD_OK) {
        errno = reply_hdr->status;
        free(rcvbuf.data);
        chan_close(chan_mbd);
        chan_mbd = -1;
        return -1;
    }

    *rep = rcvbuf.data;
    return 0;
}

const char *batch_op_str(enum batch_lib_op op)
{
    static const char *names[] = {
        [BATCH_JOB_SUBMIT] = "BATCH_JOB_SUBMIT",
        [BATCH_JOB_SUBMIT_ACK] = "BATCH_JOB_SUBMIT_ACK",
        [BATCH_JOB_SIGNAL] = "BATCH_JOB_SIGNAL",
        [BATCH_JOB_SIGNAL_ACK] = "BATCH_JOB_SIGNAL_ACK",
        [BATCH_JOB_INFO] = "BATCH_JOB_INFO",
        [BATCH_JOB_INFO_ACK] = "BATCH_JOB_INFO_ACK",
        [BATCH_HOST_INFO] = "BATCH_HOST_INFO",
        [BATCH_HOST_INFO_ACK] = "BATCH_HOST_INFO_ACK",
        [BATCH_QUEUE_INFO] = "BATCH_QUEUE_INFO",
        [BATCH_QUEUE_INFO_ACK] = "BATCH_QUEUE_INFO_ACK",
        [BATCH_GROUP_INFO] = "BATCH_GROUP_INFO",
        [BATCH_GROUP_INFO_ACK] = "BATCH_GROUP_INFO_ACK",
        // sbd <-> mbd
        [BATCH_NEW_JOB] = "BATCH_NEW_JOB",
        [BATCH_NEW_JOB_REPLY] = "BATCH_NEW_JOB_REPLY",
        [BATCH_NEW_JOB_REPLY_ACK] = "BATCH_NEW_JOB_REPLY_ACK",
        [BATCH_JOB_FINISH] = "BATCH_JOB_FINISH",
        [BATCH_JOB_FINISH_ACK] = "BATCH_JOB_FINISH_ACK",
        [BATCH_SBD_JOB_SIGNAL] = "BATCH_SBD_JOB_SIGNAL",
        [BATCH_SBD_REGISTER] = "BATCH_SBD_REGISTER",
        [BATCH_SBD_REGISTER_ACK] = "BATCH_SBD_REGISTER_ACK",
        [BATCH_TOKEN_INFO] = "BATCH_TOKEN_INFO",
        [BATCH_TOKEN_INFO_ACK] = "BATCH_TOKEN_INFO_ACK",
    };
    static const size_t nnames = sizeof(names) / sizeof(names[0]);

    if ((size_t) op >= nnames || names[op] == NULL)
        return "UNKNOWN";

    return names[op];
}
