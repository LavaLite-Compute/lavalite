/*
 * mbd.compact.c
 *
 * Additions to integrate mbd_compact into mbd.
 * These snippets belong in the following files:
 *
 *   daemonout.h       - add BATCH_COMPACT_DONE / BATCH_COMPACT_ACK to enum
 *   mbatchd.h         - declare mbd_compact_start / mbd_compact_shutdown
 *   mbatchd.handler.c - add BATCH_COMPACT_DONE case in request dispatch
 *   mbd.main.c        - call mbd_compact_start() at startup, shutdown at exit
 *   mbd.log.c         - add mbd_reopen_elog()
 *
 * wire struct and xdr go in the appropriate wire_*.c / lsb.xdr.c
 *
 * Copyright (C) LavaLite Contributors
 * GPL v2
 */

#include "lsbatch/daemons/mbd.h"
#include "lsbatch/daemons/mbatchd.h"

static pid_t compact_pid = -1;
static int compact_fail_count;

void mbd_compact_start(void)
{
    char events_arg[PATH_MAX];
    char compact_bin[PATH_MAX];
    const char *serverdir = genParams[LSF_SERVERDIR].paramValue;
    const char *logdir    = genParams[LSF_LOGDIR].paramValue;
    const char *log_mask  = genParams[LSF_LOG_MASK].paramValue;

    if (!logdir)
        logdir = "/tmp";
    if (!log_mask)
        log_mask = "LOG_INFO";

    char pid_buf[LL_BUFSIZ_64];
    sprintf(pid_buf, "%d", getpid());

    setenv("MBD_PID", pid_buf, 1);
    snprintf(compact_bin, sizeof(compact_bin), "%s/mbd_compact", serverdir);
    snprintf(events_arg,  sizeof(events_arg),  "%s/mbd/lsb.events",
             lsbParams[LSB_SHAREDIR].paramValue);

    compact_pid = fork();
    if (compact_pid < 0) {
        LS_ERR("fork mbd_compact");
        mbdDie(MASTER_FATAL);
    }
    if (compact_pid == 0) {
        execl(compact_bin, "mbd_compact",
              "--events",   events_arg,
              "--logdir",   logdir,
              "--log-mask", log_mask,
              NULL);
        fprintf(stderr, "mbd_compact: execl(%s) failed: %s\n",
                compact_bin, strerror(errno));
        _exit(1);
    }

    LS_INFO("mbd_compact started pid=%d events=%s", compact_pid, events_arg);
}

void mbd_compact_shutdown(void)
{
    if (compact_pid <= 0)
        return;

    LS_INFO("last compactor pid=%d", compact_pid);
    kill(compact_pid, SIGTERM);
    compact_pid = -1;
}

/*
 * mbd_handle_compact_done - called when mbd_compact sends BATCH_COMPACT_DONE
 * or BATCH_COMPACT_FAILED. The compactor is just another client.
 */
void mbd_handle_compact_done(XDR *xdrs, int ch_id, struct packet_header *hdr)
{
    struct wire_compact_notify req;
    struct wire_compact_notify ack_payload;
    struct packet_header ack_hdr;
    struct Buffer *out;
    char buf[PACKET_HEADER_SIZE + LL_BUFSIZ_64];
    int len;

    // decode - get status from compactor
    memset(&req, 0, sizeof(req));
    if (!xdr_wire_compact_notify(xdrs, &req)) {
        LS_ERR("decode failed");
        return;
    }

    if (hdr->operation == BATCH_COMPACT_FAILED) {
        compact_fail_count++;
        LS_ERR("compact failed status=%d count=%d compact_time=%ld",
               req.status, compact_fail_count, req.compact_time);
        // TODO: after N fails kill and restart compactor child
        goto send_ack;
    }

    compact_fail_count = 0;
    LS_INFO("compact done - event log rotated at %ld", req.compact_time);

send_ack:
    memset(&ack_payload, 0, sizeof(ack_payload));
    init_pack_hdr(&ack_hdr);
    ack_hdr.operation = BATCH_COMPACT_ACK;

    XDR xdrs2;
    xdrmem_create(&xdrs2, buf, sizeof(buf), XDR_ENCODE);
    if (!xdr_encodeMsg(&xdrs2, (char *)&ack_payload, &ack_hdr,
                       xdr_wire_compact_notify, 0, NULL)) {
        LS_ERR("xdr_encodeMsg ack failed");
        xdr_destroy(&xdrs2);
        return;
    }
    len = xdr_getpos(&xdrs2);
    xdr_destroy(&xdrs2);

    if (chan_alloc_buf(&out, len) < 0) {
        LS_ERR("chan_alloc_buf failed");
        return;
    }
    memcpy(out->data, buf, len);
    out->len = len;
    chan_enqueue(ch_id, out);
    chan_set_write_interest(ch_id, true);

    // clean the jobs using the same way as the compactor
    // vaporized the events
    if (hdr->operation == BATCH_COMPACT_DONE)
        clean_jobs((time_t)req.compact_time);

    LS_INFO("compact ack sent");
}
