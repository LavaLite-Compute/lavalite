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
static int sbd_signal_job(struct sbd_job *, int);

int sbd_handle_signal_job(int ch_id, XDR *xdr, struct packet_header *hdr)
{
    if (!xdr) {
        lserrno = LSBE_BAD_ARG;
        return -1;
    }
    if (!hdr) {
        lserrno = LSBE_BAD_ARG;
        return -1;
    }

    struct wire_job_sig_req req;
    memset(&req, 0, sizeof(req));
    int ok = xdr_wire_job_sig_req(xdr, &req);
    if (!ok) {
        LS_ERR("decode BATCH_JOB_SIGNAL failed");
        lserrno = LSBE_XDR;
        return -1;
    }

    struct wire_job_sig_reply rep;
    memset(&rep, 0, sizeof(rep));
    rep.job_id = req.job_id;
    rep.rc = LSBE_NO_ERROR;
    rep.detail_errno = 0;

    struct sbd_job *job = sbd_job_lookup(req.job_id);
    if (!job) {
        LS_INFO("signal for unknown job_id=%"PRId64, req.job_id);
        rep.rc = LSBE_NO_JOB;
        sbd_enqueue_signal_job_reply(ch_id, hdr, &rep);
        return 0;
    }

    int rc = sbd_signal_job(job, req.sig);
    if (rc < 0) {
        rep.detail_errno = errno;

        if (errno == ESRCH)
            rep.rc = LSBE_NO_JOB;
        else
            rep.rc = LSBE_SYS_CALL;

        LS_ERR("signal failed job_id=%"PRId64" sig=%d pid=%d pgid=%d rc=%d errno=%d",
               req.job_id, req.sig, job->pid, job->pgid, rep.rc, rep.detail_errno);

        sbd_enqueue_signal_job_reply(ch_id, hdr, &rep);
        return 0;
    }

    LS_INFO("signal delivered job_id=%"PRId64" sig=%d pid=%d pgid=%d",
            req.job_id, req.sig, job->pid, job->pgid);

    if (sbd_enqueue_signal_job_reply(ch_id, hdr, &rep) < 0) {
        // the call logs already in the case of failure
        return -1;
    }

    return 0;
}

static int sbd_signal_job(struct sbd_job *job, int sig)
{
    if (job->pgid > 0) {
        if (killpg(job->pgid, sig) < 0)
            return -1;
        return 0;
    }

    if (job->pid <= 0) {
        LS_ERR("signal invariant violated job_id=%"PRId64" pid=%d pgid=%d state=%d",
               job->job_id, job->pid, job->pgid, job->state);
        assert(0);
        errno = EINVAL;
        return -1;   /* keeps compiler happy if NDEBUG */
    }

    if (kill(job->pid, sig) < 0)
        return -1;

    return 0;
}
