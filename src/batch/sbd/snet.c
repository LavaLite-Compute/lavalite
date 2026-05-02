/*
 * Copyright (C) LavaLite Contributors
 * GPL v2
 */

#include <string.h>
#include <syslog.h>
#include <assert.h>

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

int sbd_register(void)
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
    hdr.status = MBD_OK;

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
    if (chan_enqueue(sbd_mbd_chan, buf) < 0) {
        LS_ERR("sbd register: send enqueue failed");
        chan_free_buf(buf);
        return -1;
    }

    // Always rememeber to enable EPOLLOUT on the main sbd_efd
    // have dowrite() to send out the request
    if (chan_set_write_interest(sbd_mbd_chan, sbd_efd, true) < 0) {
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

// the chan_id in input is the channel we have opened with mbatchd
//
int sbd_mbd_route(int chan_id)
{
    struct chan_data *chan = &channels[chan_id];

    if (chan->chan_events == CHAN_EPOLLERR) {
        LS_ERRX("lost connection with mbd on channel=%d socket err=%d",
                chan_id, chan_sock_error(chan_id));
        sbd_mbd_link_down();
        return -1;
    }

    if (chan->chan_events != CHAN_EPOLLIN) {
        // channel is not ready
        return 0;
    }

    // Get the packet header from the channel first
    struct chan_buffer *buf;
    if (chan_dequeue(chan_id, &buf) < 0) {
        LS_ERR("chan_dequeue() failed");
        return -1;
    }

    if (!buf || buf->len < PACKET_HEADER_SIZE) {
        LS_ERR("short header from mbd on channel=%d: len=%d",
               chan_id, buf ? buf->len : 0);
        return -1;
    }

    XDR xdrs;
    struct protocol_header hdr;
    // Allocate the buffer data based on what was sent
    xdrmem_create(&xdrs, buf->data, buf->len, XDR_DECODE);
    if (!xdr_pack_hdr(&xdrs, &hdr)) {
        LS_ERR("xdr_pack_hdr failed");
        xdr_destroy(&xdrs);
        return -1;
    }

    if (auth_verify_header(&hdr) < 0) {
        LS_ERR("failed validate header opcode=%s from=%s",
               batch_op_str(hdr.operation), chan_addr_str(chan_id));
        xdr_destroy(&xdrs);
        chan_free_buf(buf);
        sbd_chan_shutdown(chan_id);
        return -1;
    }

    LS_DEBUG("mbd requesting operation=%s", batch_op_str(hdr.operation));

    // sbd handler
    switch (hdr.operation) {
    case BATCH_NEW_JOB:
        // a new job from mbd has arrived
        sbd_job_new(&xdrs);
        break;
    case BATCH_NEW_JOB_ACK:
        // this indicate the ack of the previous job_reply
        // has reached the mbd who logged in the events
        // we can send a new event sbd_enqueue_execute
        sbd_job_new_ack(&xdrs);
        break;
    case BATCH_JOB_EXECUTE_ACK:
        sbd_job_execute_ack(&xdrs);
        break;
    case BATCH_JOB_FINISH_ACK:
        sbd_job_finish_ack(&xdrs);
        break;
    case BATCH_SBD_JOB_SIGNAL:
        sbd_job_signal(&xdrs);
        break;
    case BATCH_SBD_REGISTER_ACK:
        sbd_register_ack(&xdrs);
        break;
    default:
        break;
    }

    xdr_destroy(&xdrs);
    chan_free_buf(buf);

    return 0;
}

void sbd_register_ack(XDR *xdrs)
{
    struct wire_sbd_register reg_ack;

    memset(&reg_ack, 0, sizeof(struct wire_sbd_register));
    if (!xdr_wire_sbd_register(xdrs, &reg_ack)) {
        LS_ERR("xdr_wire_sbd_register decode failed");
        return;
    }

    if (reg_ack.num_jobs == 0) {
        LS_INFO("no jobs registered on this host");
        return;
    }

    for (int i = 0; i < reg_ack.num_jobs; i++) {
        struct sbd_job *job;
        struct wire_sbd_job *wj = &reg_ack.jobs[i];

        job = sbd_job_lookup(wj->job_id);
        if (job == NULL) {
            // job state is not known to sbd
            if (sbd_enqueue_job_unknown(wj->job_id) < 0) {
                sbd_fatal(SBD_FATAL_ENQUEUE);
            }
            continue;
        }
        // job must have a pid
        assert(job->pid > 0);
        // job exists on sbd
        if (job->pid <= 0) {
            LS_ERRX("register: invariant violation: job=%ld exists but pid=%d",
                    job->job_id, (int)job->pid);
            sbd_fatal(SBD_FATAL_INVARIANT);
            continue;
        }

        if (wj->pid > 0) {
            if (wj->pid != job->pid) {
                LS_ERRX("register: pid mismatch job=%ld mbd_pid=%d sbd_pid=%d",
                        (long)wj->job_id, (int)wj->pid, (int)job->pid);
                sbd_fatal(SBD_FATAL_INVARIANT);
                return;
            }
            // common steady-state
            LS_INFO("mbd got the pid job=%ld pid=%d pid_acked=%d "
                    "execute_acked=%d", job->job_id, job->pid,
                    job->pid_acked, job->execute_acked);

            continue;
        }
        // wj->pid == 0
        // MBD lost pid knowledge (restart/packet loss/etc). Force resend.
        LS_INFO("mbd missing pid job=%ld sbd_pid=%d pid_acked=%d "
                "replay_acked=%d", wj->job_id, job->pid,
                job->pid_acked, job->execute_acked);

        job->pid_acked = 0;
        job->reply_last_send = 0;
        continue;
    }
}

int32_t sbd_enqueue_payload(int chan_id, struct protocol_header *hdr,
                            void *payload, size_t siz, bool_t (*xdr_func)())
{
    struct chan_buffer *buf;
    XDR xdrs;

    if (chan_alloc_buf(&buf, siz) < 0) {
        LS_ERR("chan_alloc_buf failed op=%d siz=%ld",
               hdr->operation, (long)siz);
        return -1;
    }

    xdrmem_create(&xdrs, buf->data, siz, XDR_ENCODE);

    if (! ll_encode_msg(&xdrs, (char *)payload, xdr_func, hdr)) {
        LS_ERRX("ll_encode_msg failed op=%d", hdr->operation);
        xdr_destroy(&xdrs);
        chan_free_buf(buf);
        return -1;
    }

    buf->len = (size_t)xdr_getpos(&xdrs);
    xdr_destroy(&xdrs);

    if (chan_enqueue(chan_id, buf) < 0) {
        LS_ERR("chan_enqueue failed op=%d len=%d",
               hdr->operation, (int)buf->len);
        chan_free_buf(buf);
        return -1;
    }

    if (chan_set_write_interest(chan_id, sbd_efd, 1) < 0) {
        LS_ERR("chan_set_write_interest failed");
        chan_free_buf(buf);
        return -1;
    }

    return 0;
}
