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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA  02110-1301, USA
 */

//
// Sbatchd-native RPC protocol: XDR encode/decode helpers.
//
// This TU is shared by:
//   - sbatchd (server): decodes requests, encodes replies
//   - sbjobs  (client): encodes requests, decodes replies
//
// Keep the wire structs stable and small. Do not include daemon-private types.

#include "lsbatch/lib/lsb.sbdproto.h"

// defined in lsb.init.c but never declared in any header
// prehistoric C
extern int _lsb_conntimeout;
extern int _lsb_recvtimeout;

bool_t
xdr_sbdJobsListReq(XDR *xdrs, struct sbdJobsListReq *p,
                   struct packet_header *hdr)
{
    (void)hdr;

    if (!xdrs || !p)
        return false;

    if (!xdr_int32_t(xdrs, &p->flags))
        return false;

    return true;
}

bool_t xdr_sbdJobInfo(XDR *xdrs, struct sbdJobInfo *p,
                      struct packet_header *hdr)
{
    (void)hdr;

    if (!xdrs || !p)
        return false;

    if (!xdr_int64_t(xdrs, &p->job_id))
        return false;

    if (!xdr_int32_t(xdrs, &p->pid))
        return false;

    if (!xdr_int32_t(xdrs, &p->pgid))
        return false;

    if (!xdr_int32_t(xdrs, &p->state))
        return false;

    if (!xdr_int32_t(xdrs, &p->step))
        return false;

    if (!xdr_int32_t(xdrs, &p->pid_acked))
        return false;

    if (!xdr_int32_t(xdrs, &p->execute_acked))
        return false;

    if (!xdr_int32_t(xdrs, &p->finish_acked))
        return false;

    if (!xdr_int32_t(xdrs, &p->reply_sent))
        return false;

    if (!xdr_int32_t(xdrs, &p->execute_sent))
        return false;

    if (!xdr_int32_t(xdrs, &p->finish_sent))
        return false;

    if (!xdr_int32_t(xdrs, &p->exit_status_valid))
        return false;

    if (!xdr_int32_t(xdrs, &p->exit_status))
        return false;

    if (! xdr_opaque(xdrs, &p->cwd, LL_BUFSIZ_32))
        return false;

    if (! xdr_opaque(xdrs, &p->cwd, PATH_MAX))
        return false;

    if (! xdr_opaque(xdrs, &p->cwd, PATH_MAX))
        return false;

    return true;
}

bool_t xdr_sbdJobsListReply(XDR *xdrs, struct sbdJobsListReply *p,
                            struct packet_header *hdr)
{
    (void)hdr;

    if (!xdrs || !p)
        return false;

    // Variable-length array of sbdJobInfo
    if (!xdr_array(xdrs,
                   (char **)&p->jobs_val,
                   &p->jobs_len,
                   SBD_JOBS_MAX,
                   sizeof(struct sbdJobInfo),
                   (xdrproc_t)xdr_sbdJobInfo)) {
        return false;
    }

    return true;
}

int sbd_job_info(const char *host, struct sbdJobInfo **jobs, int *num)
{
    if (!host || !*host || !jobs || !num) {
        lsberrno = LSBE_PROTOCOL;
        return -1;
    }

    *jobs = NULL;
    *num = 0;

    struct packet_header hdr;
    init_pack_hdr(&hdr);
    hdr.operation = SBD_JOBS_LIST;

    XDR xdrs;
    char reqbuf[LL_BUFSIZ_8K];
    xdrmem_create(&xdrs, reqbuf, sizeof(reqbuf), XDR_ENCODE);

    struct sbdJobsListReq req;
    req.flags = 0;

    if (!xdr_sbdJobsListReq(&xdrs, &req, &hdr)) {
        xdr_destroy(&xdrs);
        lsberrno = LSBE_PROTOCOL;
        return -1;
    }

    struct sbdJobsListReply rep;
    memset(&rep, 0, sizeof(rep));

    char *reply = NULL;
    int reply_len = call_sbd_host(host, reqbuf, (size_t)XDR_GETPOS(&xdrs),
                                  &reply, &hdr, NULL);
    xdr_destroy(&xdrs);

    if (reply_len < 0)
        return -1;

    if (hdr.operation != LSBE_NO_ERROR) {
        // sbatchd should reply with LSBE_* like other daemons
        // so the caller can inspect lsb_sysmsg().
        lsberrno = hdr.operation;
        free(reply);
        return -1;
    }

    xdrmem_create(&xdrs, reply, (uint32_t)reply_len, XDR_DECODE);
    if (!xdr_sbdJobsListReply(&xdrs, &rep, &hdr)) {
        xdr_destroy(&xdrs);
        free(reply);
        lsberrno = LSBE_PROTOCOL;
        return -1;
    }
    xdr_destroy(&xdrs);
    free(reply);

    *jobs = rep.jobs_val;
    *num = (int)rep.jobs_len;

    return 0;
}

void sbd_job_info_free(struct sbdJobInfo *jobs, int num)
{
    struct sbdJobsListReply rep;

    if (!jobs || num < 0)
        return;

    memset(&rep, 0, sizeof(rep));
    rep.jobs_val = jobs;
    rep.jobs_len = (u_int)num;

    xdr_free((xdrproc_t)xdr_sbdJobsListReply, (char *)&rep);
}

// API
int call_sbd_host(const char *host, void *req, size_t req_len,
                  char **reply, struct packet_header *hdr,
                  struct lenData *extra)
{
    if (!host || host[0] == '\0' || !req || req_len == 0 || !reply || !hdr) {
        errno = EINVAL;
        lsberrno = LSBE_PROTOCOL;
        return -1;
    }

    uint16_t port = get_sbd_port();
    if (port == 0) {
        lsberrno = LSBE_PROTOCOL;
        return -1;
    }

    int ch_id = serv_connect(host, port, _lsb_conntimeout);
    if (ch_id < 0) {
        lsberrno = LSBE_CONN_REFUSED;
        return -1;
    }

    struct Buffer e;
    struct Buffer req_buf = {.data = req, .len = req_len, .forw = NULL};
    if (extra && extra->len > 0) {
        e.data = extra->data;
        e.len = extra->len;
        e.forw = NULL;
        req_buf.forw = &e;
    }

    struct Buffer reply_buf = {0};
    int rc = chan_rpc(ch_id, &req_buf, &reply_buf, hdr, _lsb_recvtimeout * 1000);
    chan_close(ch_id);

    if (rc < 0) {
        lsberrno = LSBE_PROTOCOL;
        return -1;
    }

    *reply = reply_buf.data;
    return hdr->length;
}
