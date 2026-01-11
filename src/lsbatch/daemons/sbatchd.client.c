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
#include "lsbatch/lib/lsb.sbdproto.h"

static int do_sbd_jobs_list(int, XDR *, struct packet_header *);
static void sbd_jobinfo_fill(struct sbdJobInfo *, struct sbd_job *);

int handle_sbd_accept(int listen_chan)
{
    struct sockaddr_in from;

    int ch_id = chan_accept(listen_chan, (struct sockaddr_in *)&from);
    if (ch_id < 0) {
        LS_ERR("chan_accept() failed");
        return -1;
    }
    struct ll_host hs;
    memset(&hs, 0, sizeof(struct ll_host));
    get_host_by_sockaddr_in(&from, &hs);
    if (hs.name[0] == 0) {
        LS_ERR("request from unknown host %s:", sockAdd2Str_(&from));
        chan_close(ch_id);
        return -1;
    }

    // Make sure the new socket is accepted by chan_epoll
    // the channel identifies the connection
    struct epoll_event ev;
    ev.events =  EPOLLIN | EPOLLRDHUP | EPOLLERR;
    ev.data.u32 = ch_id;

    int cc = epoll_ctl(sbd_efd, EPOLL_CTL_ADD, chan_sock(ch_id), &ev);
    if (cc < 0) {
        LS_ERR("epoll_ctl() failed");
        chan_close(ch_id);
        return -1;
    }

    return ch_id;
}

/*
 * handle_sbd_client()
 *
 * Handle a single sbatchd client request (e.g. sbjobs).
 *
 * Contract:
 *  - One request per connection.
 *  - chan_epoll() has already indicated readiness or error.
 *  - Uses chan_dequeue() to obtain a complete message buffer.
 *  - Decodes packet_header + payload via XDR.
 *  - Dispatches based on hdr.operation.
 *  - Sends exactly one reply (or error).
 *  - Closes the channel before returning.
 *
 * No per-client state is kept; epoll + channel id are sufficient.
 */
int handle_sbd_client(int ch_id)
{
    struct Buffer *buf = NULL;
    XDR xdrs;
    struct packet_header hdr;
    int rc = 0;

    /* Error condition already detected by channel layer */
    if (channels[ch_id].chan_events == CHAN_EPOLLERR) {
        LS_INFO("sbd client disconnected on channel %d", ch_id);
        chan_close(ch_id);
        return -1;
    }

    /*
     * Dequeue a complete request buffer from the channel.
     * chan_dequeue() returns a fully assembled message.
     */
    if (chan_dequeue(ch_id, &buf) < 0) {
        LS_ERR("chan_dequeue() failed for sbd client ch=%d", ch_id);
        chan_close(ch_id);
        return -1;
    }

    if (!buf || buf->len < PACKET_HEADER_SIZE) {
        LS_ERR("short request from sbd client ch=%d: len=%zu",
               ch_id, buf ? buf->len : 0);
        if (buf)
            chan_free_buf(buf);
        chan_close(ch_id);
        return -1;
    }

    /* Decode packet header */
    xdrmem_create(&xdrs, buf->data, buf->len, XDR_DECODE);
    if (!xdr_pack_hdr(&xdrs, &hdr)) {
        LS_ERR("xdr_pack_hdr failed for sbd client ch=%d", ch_id);
        xdr_destroy(&xdrs);
        chan_free_buf(buf);
        chan_close(ch_id);
        return -1;
    }

    LS_DEBUG("sbd client requesting operation %d on channel %d",
             hdr.operation, ch_id);

    /* Dispatch request */
    switch (hdr.operation) {

    case SBD_JOBS_LIST:
        rc = do_sbd_jobs_list(ch_id, &xdrs, &hdr);
        break;

    default:
        LS_ERRX("unknown sbd client operation %d on channel %d",
                hdr.operation, ch_id);
        /*
         * Reply with protocol error header only.
         * This mirrors existing mbd/sbd error handling.
         */
        sbd_reply_hdr_only(ch_id, LSBE_PROTOCOL, &hdr);
        rc = -1;
        break;
    }

    xdr_destroy(&xdrs);
    chan_free_buf(buf);

    /*
     * One request per connection: close after reply.
     */
    chan_close(ch_id);
    return rc;
}
static int do_sbd_jobs_list(int ch_id, XDR *xdrs, struct packet_header *hdr)
{
    struct sbdJobsListReq req;
    memset(&req, 0, sizeof(req));
    struct sbdJobsListReply rep;
    memset(&rep, 0, sizeof(rep));

    if (!xdr_sbdJobsListReq(xdrs, &req, hdr)) {
        LS_ERRX("SBD_JOBS_LIST: bad request");
        sbd_reply_hdr_only(ch_id, LSBE_PROTOCOL, hdr);
        return -1;
    }

    // Count jobs
    int n = ll_list_count(&sbd_job_list);
    if (n > SBD_JOBS_MAX)
        n = SBD_JOBS_MAX;

    struct sbdJobInfo *arr = NULL;
    if (n > 0) {
        arr = calloc(n, sizeof(*arr));
        if (!arr) {
            sbd_reply_hdr_only(ch_id, LSBE_SYS_CALL, hdr);  // or LSBE_NO_MEM
            return -1;
        }

        uint32_t i = 0;
        struct ll_list_entry *e;
        for (e = sbd_job_list.head; e && i < n; e = e->next, i++) {
            struct sbd_job *job = (struct sbd_job *)e;
            sbd_jobinfo_fill(&arr[i], job);
        }
    }

    rep.jobs_len = n;
    rep.jobs_val = arr;

    // Send reply with XDR; on success, free XDR-managed memory.
    if (sbd_reply_payload(ch_id,
                          LSBE_NO_ERROR,
                          hdr,
                          &rep,
                          xdr_sbdJobsListReply) < 0) {
        LS_ERR("failed in sbd_send_reply to client");
        // Free nested strings/array using XDR free.
        xdr_free((xdrproc_t)xdr_sbdJobsListReply, (char *)&rep);
        return -1;
    }

    // Free nested strings/array using XDR free.
    xdr_free((xdrproc_t)xdr_sbdJobsListReply, (char *)&rep);
    return 0;
}

/*
 * sbd_reply_hdr_only()
 *
 * Send a reply that contains only a packet_header (no payload).
 * Used for simple error returns (LSBE_* in hdr.operation).
 */
int sbd_reply_hdr_only(int ch_id, int rc, struct packet_header *req_hdr)
{
    struct Buffer *buf;
    XDR xdrs;
    struct packet_header out;

    if (chan_alloc_buf(&buf, sizeof(struct packet_header)) < 0) {
        LS_ERR("chan_alloc_buf failed for sbd reply hdr-only ch=%d", ch_id);
        return -1;
    }

    xdrmem_create(&xdrs, buf->data, sizeof(struct packet_header), XDR_ENCODE);

    init_pack_hdr(&out);
    out.operation = rc;

    // preserve sequence for correlation, if you use it
    out.sequence = req_hdr->sequence;

    if (!xdr_pack_hdr(&xdrs, &out)) {
        LS_ERR("xdr_pack_hdr failed for sbd reply hdr-only ch=%d", ch_id);
        xdr_destroy(&xdrs);
        chan_free_buf(buf);
        return -1;
    }

    buf->len = XDR_GETPOS(&xdrs);
    xdr_destroy(&xdrs);

    if (chan_enqueue(ch_id, buf) < 0) {   // <-- rename if needed
        LS_ERR("chan_enqueue_buf failed for sbd reply hdr-only ch=%d", ch_id);
        chan_free_buf(buf);
        return -1;
    }

    return 0;
}

/*
 * sbd_reply_payload()
 *
 * Send a reply with packet_header + XDR payload encoded by xdr_func().
 * rc goes into hdr.operation (LSBE_NO_ERROR or LSBE_*).
 */
int sbd_reply_payload(int ch_id, int rc, struct packet_header *req_hdr,
                      void *payload, bool_t (*xdr_func)())
{
    if (!req_hdr || !xdr_func) {
        errno = EINVAL;
        return -1;
    }

    /*
     * Conservative approach: allocate a reasonably sized buffer.
     * If you already have a buf-sizing helper (like chan_alloc_buf + grow),
     * use that. For now, just allocate based on hdr.length if you have it.
     */
    size_t cap = LL_BUFSIZ_8K;
    struct Buffer *buf;

    if (chan_alloc_buf(&buf, cap) < 0) {
        LS_ERR("chan_alloc_buf failed for sbd reply payload ch=%d", ch_id);
        return -1;
    }

    XDR xdrs;
    xdrmem_create(&xdrs, buf->data, buf->len, XDR_ENCODE);

    struct packet_header out;
    init_pack_hdr(&out);
    out.operation = rc;
    out.sequence = req_hdr->sequence;

    if (!xdr_pack_hdr(&xdrs, &out)) {
        LS_ERR("xdr_pack_hdr failed for sbd reply payload ch=%d", ch_id);
        xdr_destroy(&xdrs);
        chan_free_buf(buf);
        return -1;
    }

    if (!xdr_func(&xdrs, payload, &out)) {
        LS_ERR("xdr payload encode failed for sbd reply payload ch=%d", ch_id);
        xdr_destroy(&xdrs);
        chan_free_buf(buf);
        return -1;
    }

    buf->len = XDR_GETPOS(&xdrs);

    /*
     * Patch header length after payload encode, if your protocol uses hdr.length.
     * If init_pack_hdr sets length=0 and receiver ignores it, you can skip.
     */
    {
        struct packet_header *h = (struct packet_header *)buf->data;
        h->length = (int32_t)(buf->len - sizeof(struct packet_header));
    }

    xdr_destroy(&xdrs);

    if (chan_enqueue(ch_id, buf) < 0) {
        LS_ERR("chan_enqueue_buf failed for sbd reply payload ch=%d", ch_id);
        chan_free_buf(buf);
        return -1;
    }

    return 0;
}

static void
sbd_jobinfo_fill(struct sbdJobInfo *out, struct sbd_job *job)
{
    memset(out, 0, sizeof(*out));

    out->job_id = job->job_id;

    out->pid  = (int32_t)job->pid;
    out->pgid = (int32_t)job->pgid;

    out->state = (int32_t)job->state;
    out->step  = (int32_t)job->step;

    out->pid_acked     = job->pid_acked ? 1 : 0;
    out->execute_acked = job->execute_acked ? 1 : 0;
    out->finish_acked  = job->finish_acked ? 1 : 0;

    out->reply_sent   = job->reply_sent ? 1 : 0;
    out->execute_sent = job->execute_sent ? 1 : 0;
    out->finish_sent  = job->finish_sent ? 1 : 0;

    out->exit_status_valid = job->exit_status_valid ? 1 : 0;
    out->exit_status       = (int32_t)job->exit_status;

    out->missing = job->missing ? 1 : 0;

    out->job_file = strdup(job->spec.jobFile);
}
