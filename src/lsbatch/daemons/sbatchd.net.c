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
// xdr_encodeMsg() uses old-style bool_t (*xdr_func)() so we keep the same type.
static int sbd_enqueue_msg(int, int, void *, bool_t (*xdr_func)());

// Create a permanent channel to mbd
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
int sbd_mbd_register(void)
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
    hdr.operation = SBD_REGISTER;

    char request_buf[LL_BUFSIZ_4K];
    XDR xdrs_req;
    xdrmem_create(&xdrs_req, request_buf, sizeof(request_buf), XDR_ENCODE);

    if (!xdr_encodeMsg(&xdrs_req,
                       (char *)&req,
                       &hdr,
                       xdr_wire_sbd_register,
                       0,
                       NULL)) {
        lsberrno = LSBE_XDR;
        xdr_destroy(&xdrs_req);
        return -1;
    }

    struct Buffer req_buf = {
        .data = request_buf,
        .len  = (size_t)xdr_getpos(&xdrs_req),
        .forw = NULL
    };
    struct Buffer reply_buf = {0};

    // do blocking io with mbd reply_buf must be not NULL so the call
    // will wait for a reply, we only want back the header for how
    int cc = chan_rpc(sbd_mbd_chan, &req_buf, &reply_buf, &hdr,
                      _lsb_recvtimeout * 1000);
    xdr_destroy(&xdrs_req);
    if (cc < 0) {
        lsberrno = LSBE_SYS_CALL;
        return -1;
    }

    if (hdr.operation != SBD_REGISTER_REPLY ) {
        LS_ERR("mbd registration failed error: %d", lsberrno);
        if (reply_buf.data != NULL)
            free(reply_buf.data);
        return -1;
    }

    if (reply_buf.data != NULL)
        free(reply_buf.data);


    LS_INFO("sbatchd registered with mbd as host: %s", host);

    return 0;
}

// xdr_encodeMsg() uses old-style bool_t (*xdr_func)() so we keep the same type.
int sbd_enqueue_reply(int chfd, int reply_code,
                      const struct jobReply *job_reply)
{
    char *reply_struct;

    if (reply_code == ERR_NO_ERROR) {
        if (job_reply == NULL) {
            errno = EINVAL;
            return -1;
        }
        reply_struct = (char *)job_reply;
    } else {
        reply_struct = NULL;
    }

    struct Buffer *buf;
    if (chan_alloc_buf(&buf, LL_BUFSIZ_4K) < 0) {
        LS_ERR("chan_alloc_buf failed for jobReply");
        return -1;
    }

    XDR xdrs;
    xdrmem_create(&xdrs, buf->data, LL_BUFSIZ_4K, XDR_ENCODE);

    struct packet_header reply_hdr;
    init_pack_hdr(&reply_hdr);
    reply_hdr.operation = reply_code;

    if (!xdr_encodeMsg(&xdrs,
                       reply_struct,
                       &reply_hdr,
                       xdr_jobReply,
                       0,
                       NULL)) {
        LS_ERR("xdr_jobReply encode failed");
        xdr_destroy(&xdrs);
        chan_free_buf(buf);
        return -1;
    }

    buf->len = (size_t)XDR_GETPOS(&xdrs);
    xdr_destroy(&xdrs);

    if (chan_enqueue(chfd, buf) < 0) {
        LS_ERR("chan_enqueue jobReply failed (len=%zu)", buf->len);
        chan_free_buf(buf);
        return -1;
    }

    return 0;
}

int
sbd_enqueue_execute(int ch_id, struct sbd_job *job)
{
    if (job == NULL) {
        errno = EINVAL;
        return -1;
    }

    if (!job->pid_acked) {
        LS_ERR("job %"PRId64" execute before pid_acked (bug)", job->job_id);
        abort();
    }

    if (job->execute_sent) {
        LS_ERR("job %"PRId64" execute already sent (bug)", job->job_id);
        abort();
    }

    if (job->spec.jobPid <= 0 || job->spec.jobPGid <= 0) {
        LS_ERR("job %"PRId64" bad pid/pgid pid=%d pgid=%d (bug)",
               job->job_id, job->spec.jobPid, job->spec.jobPGid);
        abort();
    }

    if (job->exec_username[0] == 0 || job->spec.cwd[0] == 0
        || job->spec.subHomeDir[0] == 0) {
        LS_ERR("job %"PRId64" missing execute fields user/cwd/home (bug)",
               job->job_id);
        abort();
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

    struct Buffer *buf;
    if (chan_alloc_buf(&buf, LL_BUFSIZ_4K) < 0) {
        LS_ERR("chan_alloc_buf failed for execute");
        return -1;
    }

    XDR xdrs;
    xdrmem_create(&xdrs, buf->data, LL_BUFSIZ_4K, XDR_ENCODE);

    struct packet_header hdr;
    init_pack_hdr(&hdr);
    hdr.operation = BATCH_STATUS_JOB;

    if (!xdr_encodeMsg(&xdrs,
                       (char *)&req,
                       &hdr,
                       xdr_statusReq,
                       0,
                       NULL)) {
        LS_ERR("xdr_statusReq encode failed for job %"PRId64, job->job_id);
        xdr_destroy(&xdrs);
        chan_free_buf(buf);
        return -1;
    }

    buf->len = (size_t)XDR_GETPOS(&xdrs);
    xdr_destroy(&xdrs);

    if (chan_enqueue(ch_id, buf) < 0) {
        LS_ERR("chan_enqueue execute failed for job %"PRId64, job->job_id);
        chan_free_buf(buf);
        return -1;
    }

    LS_INFO("job %"PRId64" execute enqueued pid=%d user=%s cwd=%s",
            job->job_id, job->spec.jobPid, job->exec_username, job->spec.cwd);

    return 0;
}

int
sbd_enqueue_finish(int ch_id, struct sbd_job *job)
{
    if (job == NULL) {
        errno = EINVAL;
        return -1;
    }

    if (!job->pid_acked) {
        LS_ERR("job %"PRId64" finish before pid_acked (bug)", job->job_id);
        abort();
    }

    if (!job->execute_sent) {
        LS_ERR("job %"PRId64" finish before execute_sent (bug)", job->job_id);
        abort();
    }

    if (job->finish_sent) {
        LS_ERR("job %"PRId64" finish already sent (bug)", job->job_id);
        abort();
    }

    if (!job->exit_status_valid) {
        LS_ERR("job %"PRId64" finish without exit_status (bug)", job->job_id);
        abort();
    }

    int new_status;
    if (job->spec.jStatus & JOB_STAT_DONE) {
        new_status = JOB_STAT_DONE;
    } else if (job->spec.jStatus & JOB_STAT_EXIT) {
        new_status = JOB_STAT_EXIT;
    } else {
        LS_ERR("job %"PRId64" finish with bad jStatus=0x%x (bug)",
               job->job_id, job->spec.jStatus);
        abort();
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

    // raw waitpid() status like old jp->w_status
    req.exitStatus = job->exit_status;

    // rusage later
    req.lsfRusage = job->lsf_rusage;

    // seq: keep 1 for now
    req.seq = 1;

    req.sigValue  = 0;
    req.actStatus = 0;

    int cc = sbd_enqueue_msg(ch_id,
                             BATCH_STATUS_JOB,
                             &req,
                             xdr_statusReq);
    if (cc < 0) {
        LS_ERR("sbd_enqueue_msg for job %"PRId64" finish failed",
               job->job_id);
        return -1;
    }

    LS_INFO("job %"PRId64" finish enqueued newStatus=0x%x exitStatus=0x%x",
            job->job_id, new_status, (unsigned)job->exit_status);

    return 0;
}

// enqueue helper
static int
sbd_enqueue_msg(int ch_id, int op, void *payload,
                bool_t (*xdr_func)())
{
    struct Buffer *buf;

    if (chan_alloc_buf(&buf, LL_BUFSIZ_4K) < 0) {
        LS_ERR("chan_alloc_buf failed op=%d", op);
        return -1;
    }

    XDR xdrs;
    xdrmem_create(&xdrs, buf->data, LL_BUFSIZ_4K, XDR_ENCODE);

    struct packet_header hdr;
    init_pack_hdr(&hdr);
    hdr.operation = op;

    // xdr_encodeMsg() uses old-style
    // bool_t (*xdr_func)() so we keep the same type.
    if (!xdr_encodeMsg(&xdrs,
                       (char *)payload,
                       &hdr,
                       xdr_func,
                       0,
                       NULL)) {
        LS_ERR("xdr_encodeMsg failed op=%d", op);
        xdr_destroy(&xdrs);
        chan_free_buf(buf);
        return -1;
    }

    buf->len = (size_t)XDR_GETPOS(&xdrs);
    xdr_destroy(&xdrs);

    if (chan_enqueue(ch_id, buf) < 0) {
        LS_ERR("chan_enqueue failed op=%d len=%zu", op, buf->len);
        chan_free_buf(buf);
        return -1;
    }

    // Ensure epoll wakes up and dowrite() drains the queue.
    chan_enable_write(sbd_mbd_chan);

    return 0;
}

int
chan_enable_write(int ch_id)
{
    struct epoll_event ev;
    uint32_t want;

    want = EPOLLIN | EPOLLRDHUP;

    // if you track this: only add EPOLLOUT when send queue is non-empty
    want |= EPOLLOUT;

    memset(&ev, 0, sizeof(ev));
    ev.events = want;
    ev.data.u32 = (uint32_t)ch_id;

    // add to sbd_efd EPOLLOUT so dowrite() will dispatch the message
    if (epoll_ctl(sbd_efd, EPOLL_CTL_MOD, chan_sock(ch_id), &ev) < 0) {
        LS_ERR("epoll_ctl MOD enable write failed ch_id=%d sock=%d",
               ch_id, chan_sock(ch_id));
        return -1;
    }

    return 0;
}
