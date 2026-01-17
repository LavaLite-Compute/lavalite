/*
 * Copyright (C) LavaLite Contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#include "lsbatch/daemons/sbatchd.h"

extern int sbd_mbd_chan;      /* defined in sbatchd.main.c */
extern int efd;               /* epoll fd from sbatchd.main.c */

// Library stuff not in any header file
extern int _lsb_conntimeout;
extern int _lsb_recvtimeout;
// LavaLite define it as external for now as I dont want to include
// all the stuff from lib.h
extern char *resolve_master_with_retry(void);

/*
 * sbd_nb_connect_mbd()
 *
 * Create a TCP client channel to MBD and initiate a non-blocking connect.
 *
 * Master name is obtained via resolve_master_try() and resolved to IPv4
 * using get_host_by_name(). IPv6 is intentionally out of scope for now.
 *
 * On success, the channel socket is registered with epoll:
 * - If connect completes immediately, EPOLLIN|EPOLLERR|EPOLLRDHUP is used
 *   and *connected is set to TRUE.
 * - If connect is in progress, EPOLLOUT|EPOLLERR|EPOLLRDHUP is used and
 *   *connected is set to FALSE (caller will finish on EPOLLOUT).
 *
 * Return values:
 *   >=0  channel id on success
 *   -1   on failure; lsberrno/lserrno is set
 */
int
sbd_nb_connect_mbd(bool_t *connected)
{
    if (connected != NULL)
        *connected = false;

    char *master = resolve_master_try();
    if (master == NULL)
        return -1;

    uint16_t port = get_mbd_port();
    if (port == 0) {
        lsberrno = LSBE_PROTOCOL;
        return -1;
    }

    struct ll_host hp;
    memset(&hp, 0, sizeof(hp));
    if (get_host_by_name(master, &hp) < 0) {
        // Keep mapping simple for now; get_host_by_name() can set lserrno.
        lsberrno = LSBE_LSLIB;
        return -1;
    }

    if (hp.family != AF_INET) {
        // IPv6 out of scope for now
        lsberrno = LSBE_PROTOCOL;
        return -1;
    }

    // Convert ll_host sockaddr_storage -> sockaddr_in and set port.
    struct sockaddr_in peer;
    memset(&peer, 0, sizeof(peer));
    peer = *(struct sockaddr_in *)&hp.sa;
    peer.sin_port = htons(port);

    int ch_id = chan_client_socket(AF_INET, SOCK_STREAM, 0);
    if (ch_id < 0)
        return -1;

    // LavaLite gives the client the buffers
    channels[ch_id].send = chan_make_buf();
    channels[ch_id].recv = chan_make_buf();

    if (channels[ch_id].send == NULL || channels[ch_id].recv == NULL) {
        lserrno = LSE_NO_MEM;
        chan_close(ch_id);
        return -1;
    }

    int rc = connect_begin(chan_sock(ch_id),
                           (struct sockaddr *)&peer,
                           sizeof(peer));
    if (rc < 0) {
        lserrno = LSE_CONN_SYS;
        chan_close(ch_id);
        return -1;
    }

    struct epoll_event ev;
    if (rc == 0) {
        if (connected != NULL)
            *connected = true;

        ev.events = EPOLLIN | EPOLLERR | EPOLLRDHUP;
        ev.data.u32 = (uint32_t)ch_id;

        if (epoll_ctl(sbd_efd, EPOLL_CTL_ADD, chan_sock(ch_id), &ev) < 0) {
            LS_ERR("epoll_ctl() failed to add mbd chan");
            chan_close(ch_id);
            return -1;
        }

        return ch_id;
    }

    ev.events = EPOLLOUT | EPOLLERR | EPOLLRDHUP;
    ev.data.u32 = (uint32_t)ch_id;

    if (epoll_ctl(sbd_efd, EPOLL_CTL_ADD, chan_sock(ch_id), &ev) < 0) {
        LS_ERR("epoll_ctl() failed to add mbd connecting chan");
        chan_close(ch_id);
        return -1;
    }

    return ch_id;
}

// Create a permanent channel to mbd using a blocking connect
int sbd_connect_mbd(void)
{
    char *master = resolve_master_with_retry();
    if (master == NULL) {
        return -1;
    }

    uint16_t port = get_mbd_port();
    if (port == 0) {
        lsberrno = LSBE_PROTOCOL;
        return -1;
    }

    int ch_id = serv_connect(master, port, _lsb_conntimeout);
    if (ch_id < 0) {
        lsberrno = LSBE_CONN_REFUSED;
        return -1;
    }
    // LavaLite give the client the buffers
    channels[ch_id].send = chan_make_buf();
    channels[ch_id].recv = chan_make_buf();

    struct epoll_event ev = {.events = EPOLLIN, .data.u32 = (uint32_t)ch_id};

    if (epoll_ctl(sbd_efd, EPOLL_CTL_ADD, chan_sock(ch_id), &ev) < 0) {
        LS_ERR("epoll_ctl() failed to add sbd chan");
        chan_close(ch_id);
        return -1;
    }

    return ch_id;
}

/*
 * Simple wire struct for registration.
 * Put this in sbatchd.h if not already there:
 *
 * struct wire_sbd_register {
 *     char hostname[256];
 * };
 *
 * bool_t xdr_wire_sbd_register(XDR *xdrs, struct wire_sbd_register *msg)
 * {
 *     return xdr_opaque(xdrs, msg->hostname, sizeof(msg->hostname));
 * }
 */
/*
 * sbd_enqueue_register()
 *
 * Enqueue an SBD registration request to MBD on an already-connected channel.
 *
 * This is non-blocking: it encodes the request into a heap-backed Buffer and
 * queues it on the channel send path. The epoll-driven dowrite() will flush it.
 *
 * The reply (BATCH_SBD_REGISTER_REPLY) is handled asynchronously in sbd_handle_mbd().
 */
int sbd_enqueue_register(int ch_id)
{
    char host[MAXHOSTNAMELEN];

    if (gethostname(host, sizeof(host)) < 0) {
        LS_ERR("cannot get local hostname: %m");
        snprintf(host, sizeof(host), "unknown");
    }

    struct wire_sbd_register req;
    memset(&req, 0, sizeof(req));
    snprintf(req.hostname, sizeof(req.hostname), "%s", host);

    struct packet_header hdr;
    init_pack_hdr(&hdr);
    hdr.operation = BATCH_SBD_REGISTER;

    struct Buffer *buf = NULL;
    if (chan_alloc_buf(&buf, LL_BUFSIZ_4K) < 0) {
        LS_ERR("sbd register: chan_alloc_buf failed");
        return -1;
    }

    XDR xdrs;
    xdrmem_create(&xdrs, buf->data, LL_BUFSIZ_4K, XDR_ENCODE);

    if (!xdr_encodeMsg(&xdrs,
                       (char *)&req,
                       &hdr,
                       xdr_wire_sbd_register,
                       0,
                       NULL)) {
        LS_ERR("sbd register: xdr_encodeMsg failed");
        xdr_destroy(&xdrs);
        chan_free_buf(buf);
        lsberrno = LSBE_XDR;
        return -1;
    }

    buf->len = (size_t)xdr_getpos(&xdrs);
    xdr_destroy(&xdrs);

    /*
     * Queue for send. This must append buf to the channel send queue and
     * ensure EPOLLOUT interest is enabled so dowrite() will run.
     */
    if (chan_enqueue(ch_id, buf) < 0) {
        LS_ERR("sbd register: send enqueue failed");
        chan_free_buf(buf);
        return -1;
    }

    // Always rememeber to enable EPOLLOUT on the main sbd_efd
    // to have dowrite() to send out the request
    chan_set_write_interest(ch_id, true);

    LS_INFO("sbd register: enqueued request as host: %s", host);

    return 0;
}

// xdr_encodeMsg() uses old-style bool_t (*xdr_func)() so we keep the same type.
// this function runs right upona job start we will be async waiting to
// mbd to reply BATCH_NEW_JOB_ACK so that we know mbd got the data and logged
// the pid to the events file
int sbd_enqueue_reply(int reply_code, const struct jobReply *job_reply)
{
    char *reply_struct;

    // Check it we are connected to mbd
    if (! sbd_mbd_link_ready()) {
        LS_INFO("mbd link not ready, skip job %ld and sbd_mbd_reconnect_try",
                job_reply->jobId);
        return -1;
    }

    reply_struct = NULL;
    if (reply_code == ERR_NO_ERROR) {
        if (job_reply == NULL) {
            errno = EINVAL;
            return -1;
        }
        reply_struct = (char *)job_reply;
    }

    int cc = enqueue_payload(sbd_mbd_chan,
                             reply_code,
                             reply_struct,
                             xdr_jobReply);
    if (cc < 0) {
        LS_ERR("enqueue_payload for job %"PRId64" reply failed",
               job_reply->jobId);
        return -1;
    }

    chan_set_write_interest(sbd_mbd_chan, true);

    LS_INFO("sent job=%"PRId64" pid=%d to mbd", job_reply->jobId,
            job_reply->jobPid);

    return 0;
}

// This is invoke at afer mbd ack the pid with BATCH_NEW_JOB_ACK
int sbd_enqueue_execute(struct sbd_job *job)
{
    if (job == NULL) {
        errno = EINVAL;
        return -1;
    }

    // Check it we are connected to mbd
    if (! sbd_mbd_link_ready()) {
        LS_INFO("mbd link not ready, skip job %ld and sbd_mbd_reconnect_try",
                job->job_id);
        return -1;
    }

    if (!job->pid_acked) {
        LS_ERR("job %"PRId64" execute before pid_acked (bug)", job->job_id);
        assert(0);
        return -1;
    }

    if (job->execute_acked) {
        LS_ERR("job %"PRId64" execute already sent (bug)", job->job_id);
        assert(0);
        return -1;
    }

    if (job->spec.jobPid <= 0 || job->spec.jobPGid <= 0) {
        LS_ERR("job %ld bad pid/pgid pid=%d pgid=%d (bug)",
               job->job_id, job->spec.jobPid, job->spec.jobPGid);
        assert(0);
        return -1;
    }

    if (job->exec_username[0] == 0 || job->spec.cwd[0] == 0
        || job->spec.subHomeDir[0] == 0) {
        LS_ERR("job %ld missing execute fields user/cwd/home (sbd restarted?)",
               job->job_id);
        return -1;
    }

    struct statusReq req;
    memset(&req, 0, sizeof(req));

    req.jobId     = job->job_id;
    req.jobPid    = job->spec.jobPid;
    req.jobPGid   = job->spec.jobPGid;
    // this is now fundamental for mbd that has acked the pid
    // the job now must transition to JOB_STAT_EXECUTE
    req.newStatus = JOB_STAT_RUN;

    req.reason     = 0;
    req.subreasons = 0;

    req.execUid = job->spec.userId;

    // statusReq uses pointers: point at stable storage in job/spec
    req.execHome     = job->spec.subHomeDir;
    req.execCwd      = job->spec.cwd;
    req.execUsername = job->exec_username;

    req.queuePreCmd  = "";
    req.queuePostCmd = "";
    req.msgId        = 0;

    req.sbdReply   = ERR_NO_ERROR;
    req.actPid     = 0;
    req.numExecHosts = 0;
    req.execHosts  = NULL;
    req.exitStatus = 0;
    req.sigValue   = 0;
    req.actStatus  = 0;

    // seq: keep 1 for now; wire per-job seq later if needed
    req.seq = 1;

    int cc = enqueue_payload(sbd_mbd_chan,
                             BATCH_JOB_EXECUTE,
                             &req,
                             xdr_statusReq);
    if (cc < 0) {
        LS_ERR("enqueue_payload for job %"PRId64" BATCH_JOB_EXECUTE failed",
               job->job_id);
        return -1;
    }

    chan_set_write_interest(sbd_mbd_chan, true);

    LS_INFO("job %"PRId64" execute enqueued pid=%d user=%s cwd=%s",
            job->job_id, job->spec.jobPid, job->exec_username, job->spec.cwd);

    return 0;
}

int sbd_enqueue_finish(struct sbd_job *job)
{
    if (job == NULL) {
        errno = EINVAL;
        return -1;
    }

    // Check it we are connected to mbd
    if (! sbd_mbd_link_ready()) {
        LS_INFO("mbd link not ready, skip job %ld and sbd_mbd_reconnect_try",
                job->job_id);
        return -1;
    }

    if (! job->reply_sent) {
        LS_ERRX("job %"PRId64" not reply_sent before? (bug)", job->job_id);
        assert(0);
        return -1;
    }

    if (!job->pid_acked) {
        LS_ERR("job %"PRId64" not pid_acked before? (bug)", job->job_id);
        assert(0);
        return -1;
    }

    if (!job->execute_acked) {
        LS_ERRX("job %"PRId64" not executed_acked before ? (bug)", job->job_id);
        assert(0);
        return -1;
    }

    if (job->finish_sent) {
        LS_ERRX("job %"PRId64" sending finish_sent again? (bug)", job->job_id);
        assert(0);
        return -1;
    }

    if (job->finish_acked) {
        LS_ERR("job %"PRId64" finish_acked already sent (bug)", job->job_id);
        assert(0);
        return -1;
    }

    if (!job->exit_status_valid) {
        LS_ERR("job %"PRId64" finish without exit_status (bug)", job->job_id);
        assert(0);
        return -1;
    }

    int new_status;
    if (job->spec.jStatus & JOB_STAT_DONE) {
        new_status = JOB_STAT_DONE;
    } else if (job->spec.jStatus & JOB_STAT_EXIT) {
        new_status = JOB_STAT_EXIT;
    } else {
        LS_ERR("job %"PRId64" finish with bad jStatus=0x%x (bug)",
               job->job_id, job->spec.jStatus);
        assert(0);
        return -1;
    }

    struct statusReq req;
    memset(&req, 0, sizeof(req));

    req.jobId     = job->job_id;
    req.jobPid    = job->spec.jobPid;
    req.jobPGid   = job->spec.jobPGid;
    req.newStatus = new_status;

    req.reason     = 0;
    req.subreasons = 0;

    req.execUid = job->spec.userId;

    // optional but harmless: keep exec fields, they are stable storage
    req.execHome     = job->spec.subHomeDir;
    req.execCwd      = job->spec.cwd;
    req.execUsername = job->exec_username;

    req.queuePreCmd  = "";
    req.queuePostCmd = "";
    req.msgId        = 0;

    req.sbdReply   = ERR_NO_ERROR;
    req.actPid     = 0;
    req.numExecHosts = 0;
    req.execHosts  = NULL;

    // plain exit code (0..255) from job wrapper
    req.exitStatus = job->exit_status;

    // rusage later
    req.lsfRusage = job->lsf_rusage;

    // seq: keep 1 for now
    req.seq = 1;

    req.sigValue  = 0;
    req.actStatus = 0;

    int cc = enqueue_payload(sbd_mbd_chan,
                             BATCH_JOB_FINISH,
                             &req,
                             xdr_statusReq);
    if (cc < 0) {
        LS_ERR("enqueue_payload for job %"PRId64" BATCH_JOB_FINISH failed",
               job->job_id);
        return -1;
    }

    // Ensure epoll wakes up and dowrite() drains the queue.
    chan_set_write_interest(sbd_mbd_chan, true);

    LS_INFO("job %"PRId64" finish enqueued newStatus=0x%x exitStatus=0x%x",
            job->job_id, new_status, (unsigned)job->exit_status);

    return 0;
}

int sbd_enqueue_signal_job_reply(int ch_id, struct packet_header *hdr,
                                 struct wire_job_sig_reply *rep)
{
    if (!rep) {
        lserrno = LSBE_BAD_ARG;
        return -1;
    }

    int rc = enqueue_payload(ch_id,
                             BATCH_JOB_SIGNAL_REPLY,
                             rep,
                             xdr_wire_job_sig_reply);
    if (rc < 0) {
        LS_ERR("enqueue signal job reply failed job_id=%"PRId64,
               rep->job_id);
        lserrno = LSBE_PROTOCOL;
        return -1;
    }

    // Ensure epoll wakes up and dowrite() drains the queue.
    chan_set_write_interest(sbd_mbd_chan, true);

    return 0;
}
