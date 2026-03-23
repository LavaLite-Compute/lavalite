/* Copyright (C) LavaLite Contributors
 * GPL v2
 */

#include "batch/lib/batch.h"

int
batch_job_signal(int sig_value, int64_t job_id)
{
#if 0

    char *reply_buf = NULL;
    XDR xdrs;
    int cc;

    struct job_signal js;
    memset(&js, 0, sizeof(struct job_signal));
    js.job_id;
    js.sig_value = signal;

    struct protocol_header packet_hdr;
    init_pack_hdr(&packet_hdr);
    packet_hdr.operation = BATCH_JOB_SIG;


    if (!ll_encode_msg(&xdrs2, &wm, xdr_wire_master, &hdr))  {
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

#endif
    return 0;
}
