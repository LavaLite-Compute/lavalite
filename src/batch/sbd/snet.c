/*
 * Copyright (C) LavaLite Contributors
 * GPL v2
 */

#include <string.h>
#include <syslog.h>

#include "base/lib/auth.h"
#include "base/lib/ll.conf.h"
#include "base/lib/ll.syslog.h"
#include "base/lib/ll.channel.h"
#include "batch/sbd/sbd.h"
#include "batch/lib/rpc.h"

// Create a permanent channel to mbd using a blocking connect
int sbd_mbd_connect(void)
{
    int port;
    ll_atoi(ll_params[LL_MBD_PORT].val, &port);

    sbd_mbd_chan = chan_tcp_client();
    if (sbd_mbd_chan < 0) {
        LS_ERR("failed to get channel to mbd");
        return -1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    get_host_addrv4(&mbd_host, &addr);
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);

    // 3s: datacenter LAN; if mbd doesn't answer it's down
    if (chan_connect(sbd_mbd_chan, &addr, 3, 0) < 0) {
        sbd_chan_shutdown(sbd_mbd_chan);
        sbd_mbd_chan = -1;
        return -1;
    }

    struct epoll_event ev = {
        .events = EPOLLIN,
        .data.u32 = (uint32_t)sbd_mbd_chan
    };

    if (epoll_ctl(sbd_efd, EPOLL_CTL_ADD, chan_sock(sbd_mbd_chan), &ev) < 0) {
        LS_ERR("epoll_ctl() failed to add mbd connection to epoll");
        sbd_chan_shutdown(sbd_mbd_chan);
        sbd_mbd_chan = -1;
        return -1;
    }
    LS_INFO("connected to mbd chan=%d", sbd_mbd_chan);

    return sbd_mbd_chan;
}

int sbd_job_state_write(struct sbd_job *job)
{
    (void)job;
    return 0;
}

void sbd_mbd_link_down(void)
{
    if (sbd_mbd_chan >= 0)
        chan_close(sbd_mbd_chan);

    sbd_mbd_chan = -1;

    struct ll_list_entry *e;
    for (e = sbd_job_list.head; e; e = e->next) {
        struct sbd_job *job = (struct sbd_job *)e;

        job->reply_last_send = job->execute_last_send
            = job->finish_last_send = 0;
        // Write the latest job state
        if (sbd_job_state_write(job) < 0) {
            LS_ERRX("job=%ld state write failed", job->job_id);
            sbd_fatal(SBD_FATAL_STORAGE);
        }
    }

    LS_ERRX("mbd link down: cleared pending sent flags for resend and state");
}

// Check if mbd is connected
bool_t sbd_mbd_link_ready(void)
{
    return (sbd_mbd_chan >= 0);
}

int sbd_register(int chan_id)
{
    if (! sbd_mbd_link_ready())
        return -1;

    char host[MAXHOSTNAMELEN];

    if (gethostname(host, sizeof(host)) < 0) {
        LS_ERR("cannot get local hostname: %m");
        snprintf(host, sizeof(host), "unknown");
    }

    struct wire_sbd_register req;
    memset(&req, 0, sizeof(req));
    snprintf(req.hostname, sizeof(req.hostname), "%s", host);

    struct protocol_header hdr;
    init_protocol_header(&hdr);
    hdr.operation = BATCH_SBD_REGISTER;

    if (auth_sign_header(&hdr) < 0) {
        LS_ERR("failed to sign header failed to register with mbd");
        chan_close(sbd_mbd_chan);
        sbd_mbd_chan = -1;
        return -1;
    }

    struct chan_buffer *buf = NULL;
    if (chan_alloc_buf(&buf, LL_BUFSIZ_4K) < 0) {
        LS_ERR("sbd register: chan_alloc_buf failed");
        return -1;
    }

    XDR xdrs;
    xdrmem_create(&xdrs, buf->data, LL_BUFSIZ_4K, XDR_ENCODE);

    if (!ll_encode_msg(&xdrs, &req, xdr_wire_sbd_register, &hdr)) {
        LS_ERR("ll_encode_msg failed");
        xdr_destroy(&xdrs);
        chan_free_buf(buf);
        return -1;
    }

    buf->len = xdr_getpos(&xdrs);
    xdr_destroy(&xdrs);

    /*
     * Queue for send. This must append buf to the channel send queue and
     * ensure EPOLLOUT interest is enabled so dowrite() will run.
     */
    if (chan_enqueue(chan_id, buf) < 0) {
        LS_ERR("sbd register: send enqueue failed");
        chan_free_buf(buf);
        return -1;
    }

    // Always rememeber to enable EPOLLOUT on the main sbd_efd
    // to have dowrite() to send out the request
    if (chan_set_write_interest(chan_id, sbd_efd, true) < 0) {
        LS_ERR("sbd  chan_set_write_interest failed");
        return -1;
    }

    LS_INFO("sbd register: enqueued request as host: %s", host);

    return 0;
}

void sbd_chan_shutdown(int chan_id)
{
    epoll_ctl(sbd_efd, EPOLL_CTL_DEL, chan_sock(chan_id), NULL);
    chan_close(chan_id);
}
