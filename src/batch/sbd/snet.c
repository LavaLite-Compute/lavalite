/*
 * Copyright (C) LavaLite Contributors
 * GPL v2
 */

#include <string.h>
#include <syslog.h>
#include <assert.h>
#include <errno.h>

#include "base/lib/auth.h"
#include "base/lib/ll.conf.h"
#include "base/lib/ll.syslog.h"
#include "base/lib/ll.channel.h"
#include "batch/sbd/sbd.h"
#include "batch/lib/rpc.h"

static int32_t sbd_enqueue_payload(int chan_id, struct protocol_header *hdr,
                                   void *payload, size_t siz,
                                   bool_t (*xdr_func)())
{
    struct chan_buffer *buf;
    XDR xdrs;

    if (chan_alloc_buf(&buf, siz) < 0) {
        LL_ERR("chan_alloc_buf failed op=%d siz=%ld", hdr->operation,
               (long) siz);
        return -1;
    }

    xdrmem_create(&xdrs, buf->data, siz, XDR_ENCODE);

    if (!ll_encode_msg(&xdrs, (char *) payload, xdr_func, hdr)) {
        LL_ERRX("ll_encode_msg failed op=%d", hdr->operation);
        xdr_destroy(&xdrs);
        chan_free_buf(buf);
        return -1;
    }

    buf->len = (size_t) xdr_getpos(&xdrs);
    xdr_destroy(&xdrs);

    if (chan_enqueue(chan_id, buf) < 0) {
        LL_ERR("chan_enqueue failed op=%d len=%d", hdr->operation,
               (int) buf->len);
        chan_free_buf(buf);
        return -1;
    }

    if (chan_set_write_interest(chan_id, sbd_efd, 1) < 0) {
        LL_ERR("chan_set_write_interest failed");
        chan_free_buf(buf);
        return -1;
    }

    return 0;
}

// Create a permanent channel to mbd, then switch it to nonblocking I/O.
int sbd_mbd_connect(void)
{
    int port;
    ll_atoi(ll_params[LL_MBD_PORT].val, &port);

    sbd_mbd_chan = chan_tcp_client();
    if (sbd_mbd_chan < 0) {
        LL_ERR("failed to get channel to mbd");
        return -1;
    }
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    get_host_addrv4(&mbd_node, &addr);
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t) port);

    // 3s: datacenter LAN; if mbd doesn't answer it's down
    if (chan_connect(sbd_mbd_chan, &addr, 3) < 0) {
        LL_ERR("cannot connect to mbd on chan=%d", sbd_mbd_chan);
        if (errno == EPROTO)
            LL_ERR("rejected TCP self-connect while connecting to mbd");
        sbd_chan_shutdown(sbd_mbd_chan);
        sbd_mbd_chan = -1;
        return -1;
    }

    // Now we set the socket as non blocking for all our
    // communication with mbd
    if (io_non_block(chan_sock(sbd_mbd_chan)) < 0) {
        LL_ERR("failed to set mbd socket nonblocking");
        sbd_chan_shutdown(sbd_mbd_chan);
        sbd_mbd_chan = -1;
        return -1;
    }

    struct epoll_event ev = {.events = EPOLLIN,
                             .data.u32 = (uint32_t) sbd_mbd_chan};

    if (epoll_ctl(sbd_efd, EPOLL_CTL_ADD, chan_sock(sbd_mbd_chan), &ev) < 0) {
        LL_ERR("epoll_ctl() failed to add mbd connection to epoll");
        sbd_chan_shutdown(sbd_mbd_chan);
        sbd_mbd_chan = -1;
        return -1;
    }
    LL_INFO("connected to mbd chan=%d", sbd_mbd_chan);

    return sbd_mbd_chan;
}

void sbd_mbd_link_down(void)
{
    if (sbd_mbd_chan >= 0)
        chan_close(sbd_mbd_chan);

    sbd_mbd_chan = -1;

    struct ll_list_entry *e;
    for (e = sbd_job_list.head; e; e = e->next) {
        struct sbd_job *job = (struct sbd_job *) e;

        job->reply_last_send = job->finish_last_send = 0;
        // Write the latest job state
        if (sbd_job_state_write(job) < 0) {
            LL_ERRX("job=%ld state write failed", job->job_id);
            sbd_fatal(SBD_FATAL_STORAGE);
        }
    }

    LL_ERRX("mbd link down: cleared pending sent flags for resend and state");
}

// Check if mbd is connected
bool_t sbd_mbd_link_ready(void)
{
    return (sbd_mbd_chan >= 0);
}

int sbd_register(void)
{
    if (!sbd_mbd_link_ready())
        return -1;

    char host[MAXHOSTNAMELEN];

    if (gethostname(host, sizeof(host)) < 0) {
        LL_ERR("cannot get local hostname: %m");
        abort();
    }

    /*
     * Sim mode: --simulator name:port overrides gethostname.
     * mbd strips the :port suffix to look up the host in its hash.
     */
    if (sim_name[0] != 0)
        ll_strlcpy(host, sim_name, MAXHOSTNAMELEN);

    struct wire_sbd_register reg;
    memset(&reg, 0, sizeof(reg));
    ll_strlcpy(reg.hostname, host, sizeof(reg.hostname));

    if (sbd_send_msg(BATCH_SBD_REGISTER, MBD_OK, &reg, LL_BUFSIZ_1K,
                     xdr_wire_sbd_register) < 0) {
        LL_ERR("sbd on host=%s registration failed", host);
        return -1;
    }

    LL_INFO("sbd registered sent host=%s", host);

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
        LL_ERRX("lost connection with mbd on channel=%d socket err=%d", chan_id,
                chan_sock_error(chan_id));
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
        LL_ERR("chan_dequeue() failed");
        return -1;
    }

    if (!buf || buf->len < PACKET_HEADER_SIZE) {
        LL_ERR("short header from mbd on channel=%d: len=%d", chan_id,
               buf ? buf->len : 0);
        chan_free_buf(buf);
        sbd_mbd_link_down();
        return -1;
    }

    XDR xdrs;
    struct protocol_header hdr;
    // Allocate the buffer data based on what was sent
    xdrmem_create(&xdrs, buf->data, buf->len, XDR_DECODE);
    if (!xdr_pack_hdr(&xdrs, &hdr)) {
        LL_ERR("xdr_pack_hdr failed");
        xdr_destroy(&xdrs);
        return -1;
    }

    if (auth_verify_header(&hdr) < 0) {
        LL_ERR("failed validate header opcode=%s from=%s",
               batch_op_str(hdr.operation), chan_addr_str(chan_id));
        xdr_destroy(&xdrs);
        chan_free_buf(buf);
        sbd_mbd_link_down();
        return -1;
    }

    LL_DEBUG("mbd requesting operation=%s", batch_op_str(hdr.operation));

    // sbd handler
    switch (hdr.operation) {
    case BATCH_NEW_JOB:
        // a new job from mbd has arrived
        sbd_job_new(&xdrs);
        break;
    case BATCH_NEW_JOB_REPLY_ACK:
        // this indicate the ack of the previous job_reply
        // has reached the mbd who logged in the events
        // we can send a new event sbd_enqueue_execute
        sbd_job_new_reply_ack(&xdrs);
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
        LL_ERRX("unknown protocol operation=%s on chan=%d",
                batch_op_str(hdr.operation), chan_id);
        sbd_mbd_link_down();
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
        LL_ERR("xdr_wire_sbd_register decode failed");
        sbd_mbd_link_down();
        return;
    }

    if (reg_ack.num_jobs == 0) {
        LL_INFO("no jobs registered on this host");
        free(reg_ack.jobs);
        return;
    }

    for (int i = 0; i < reg_ack.num_jobs; i++) {
        struct wire_sbd_job *wj = &reg_ack.jobs[i];

        struct sbd_job *job = sbd_job_lookup(wj->job_id);
        if (job == NULL) {
            if (sbd_enqueue_job_unknown(wj->job_id) < 0)
                sbd_fatal(SBD_FATAL_ENQUEUE);
            continue;
        }

        if (job->pid <= 0) {
            LL_ERRX("register: invariant violation: job=%ld exists but pid=%d",
                    job->job_id, (int) job->pid);
            sbd_fatal(SBD_FATAL_INVARIANT);
            /* not reached */
        }

        if (wj->pid > 0) {
            if (wj->pid != job->pid) {
                LL_ERRX("register: pid mismatch job=%ld mbd_pid=%d sbd_pid=%d",
                        (long) wj->job_id, (int) wj->pid, (int) job->pid);
                sbd_fatal(SBD_FATAL_INVARIANT);
                /* not reached */
            }
            LL_INFO("mbd got the pid job=%ld pid=%d pid_acked=%d",
                    job->job_id, job->pid, job->pid_acked);
            continue;
        }

        /* wj->pid == 0: mbd lost pid, force resend */
        LL_INFO("mbd missing pid job=%ld sbd_pid=%d pid_acked=%d",
                wj->job_id, job->pid, job->pid_acked);
        job->pid_acked = 0;
        job->reply_last_send = 0;
    }

    free(reg_ack.jobs);
}

int sbd_send_msg(int32_t op, int32_t status, void *payload, size_t siz,
                 bool_t (*xdr_func)())
{
    struct protocol_header hdr;

    init_protocol_header(&hdr);
    hdr.operation = op;
    hdr.status = status;

    if (auth_sign_header(&hdr) < 0) {
        LL_ERR("auth_sign_header failed op=%d", op);
        return -1;
    }

    int cc = sbd_enqueue_payload(sbd_mbd_chan, &hdr, payload, siz, xdr_func);
    if (cc < 0) {
        LL_ERR("sbd_enqueue_payload failed closing chan=%d to mbd",
               sbd_mbd_chan);
        sbd_mbd_link_down();
        return -1;
    }

    return 0;
}
