/* Copyright (C) LavaLite Contributors
 * GPL v2
 */

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
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

#include "batch/lib/proto.h"
#include "llbatch.h"

static int mbd_chan = -1;
static int conntimeout;
static int recvtimeout;
static struct ll_host mbd_host;
static uint16_t mbd_port;

static int initialized;

static int mbd_init(void)
{
    if (initialized)
        return 0;

    if (ll_init() < 0)
        return -1;

    if (!ll_atoi(ll_params[LL_MBD_PORT].val, (int *)&mbd_port))
        return -1;

    get_host_by_name(ll_params[LL_MBD_HOST].val, &mbd_host);
        return -1;

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
int call_mbd(const void *req, size_t req_len,
             void **rep, struct protocol_header *reply_hdr)
{
    if (mbd_init() < 0)
        return -1;

    if (mbd_chan < 0) {

        mbd_chan = chan_tcp_client();
        if (mbd_chan < 0) {
            llberrno = LLBE_PROTOCOL;
            return -1;
        }

        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        get_host_addrv4(&mbd_host, &addr);
        addr.sin_family = AF_INET;
        addr.sin_port   = htons((uint16_t)mbd_port);

        if (chan_connect(mbd_chan, &addr, conntimeout * 1000, 0) < 0) {
            llberrno = LLBE_PROTOCOL;
            chan_close(mbd_chan);
            mbd_chan = -1;
            return -1;
        }
    }

    struct chan_buffer sndbuf = {.data = (void *)req, .len = req_len};
    struct chan_buffer rcvbuf = {0};

    if (chan_rpc(mbd_chan, &sndbuf, &rcvbuf, reply_hdr, recvtimeout) < 0) {
        llberrno = LLBE_PROTOCOL;
        chan_close(mbd_chan);
        mbd_chan = -1;
        return -1;
    }

    if (reply_hdr->status != MBD_OK) {
        llberrno = LLBE_PROTOCOL;
        free(rcvbuf.data);
        chan_close(mbd_chan);
        mbd_chan = -1;
        return -1;
    }

    *rep = rcvbuf.data;
    return 0;
}
