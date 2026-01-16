/*
 * Copyright (C) 2007 Platform Computing Inc
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 USA
 *
 */

#include "lsbatch/lib/lsb.h"

#include "lsbatch/lib/lsb.h"

static int lsb_send_job_signal_req(int sig_value, int64_t job_id);

int
lsb_signaljob(int64_t job_id, int sig_value)
{
    if (sig_value <= 0 || sig_value >= NSIG) {
        lsberrno = LSBE_BAD_SIGNAL;
        return -1;
    }

    if (job_id <= 0) {
        lsberrno = LSBE_BAD_ARG;
        return -1;
    }

    return lsb_send_job_signal_req(sig_value, job_id);
}

static int
lsb_send_job_signal_req(int sig_value, int64_t job_id)
{
    struct signalReq signal_req;
    char request_buf[LL_BUFSIZ_4K];
    char *reply_buf = NULL;
    XDR xdrs;
    int cc;
    struct packet_header packet_hdr;
    struct lsfAuth auth;

    memset(&signal_req, 0, sizeof(signal_req));

    signal_req.jobId = job_id;
    signal_req.sigValue = sig_encode(sig_value);
    signal_req.chkPeriod = 0;
    signal_req.actFlags = 0;

    if (authTicketTokens_(&auth, NULL) == -1)
        return -1;

    xdrmem_create(&xdrs, request_buf, sizeof(request_buf), XDR_ENCODE);

    init_pack_hdr(&packet_hdr);
    packet_hdr.operation = BATCH_JOB_SIG;

    if (!xdr_encodeMsg(&xdrs,
                       (char *)&signal_req,
                       &packet_hdr,
                       xdr_signalReq,
                       0,
                       &auth)) {
        lsberrno = LSBE_XDR;
        xdr_destroy(&xdrs);
        return -1;
    }

    cc = call_mbd(request_buf,
                  XDR_GETPOS(&xdrs),
                  &reply_buf,
                  &packet_hdr,
                  NULL);

    xdr_destroy(&xdrs);

    if (cc < 0) {
        if (reply_buf)
            free(reply_buf);
        return -1;
    }

    if (reply_buf)
        free(reply_buf);

    lsberrno = packet_hdr.operation;

    if (lsberrno == LSBE_NO_ERROR || lsberrno == LSBE_JOB_DEP)
        return 0;

    return -1;
}

int
lsb_requeuejob(struct jobrequeue *req)
{
    int cc;

    if (req == NULL || req->jobId <= 0) {
        lsberrno = LSBE_BAD_ARG;
        return -1;
    }

    /*
     * Normalize status: only PEND or PSUSP are allowed.
     */
    if (req->status != JOB_STAT_PEND
        && req->status != JOB_STAT_PSUSP)
        req->status = JOB_STAT_PEND;

    /*
     * Normalize options: must include at least one valid flag.
     */
    if (req->options != REQUEUE_DONE
        && req->options != REQUEUE_EXIT
        && req->options != REQUEUE_RUN)
        req->options |= (REQUEUE_DONE | REQUEUE_EXIT | REQUEUE_RUN);

    cc = lsb_send_job_signal_req(SIG_ARRAY_REQUEUE,
                                 req->jobId);

    if (cc == 0)
        return 0;

    return -1;
}
