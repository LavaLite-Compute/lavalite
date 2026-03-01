/*
 * mbd.compact.mbd_side.c
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

/* -----------------------------------------------------------------------
 * daemonout.h - add before BATCH_LAST_OP:
 *
 *   BATCH_COMPACT_DONE,
 *   BATCH_COMPACT_ACK,
 *
 * ----------------------------------------------------------------------- */

/* -----------------------------------------------------------------------
 * wire struct - add to appropriate wire header:
 * ----------------------------------------------------------------------- */

struct wire_compact_notify {
    int32_t status;     /* 0 = ok */
};

bool_t xdr_wire_compact_notify(XDR *xdrs, struct wire_compact_notify *msg)
{
    if (!xdrs || !msg)
        return false;
    return xdr_int32_t(xdrs, &msg->status);
}

/* -----------------------------------------------------------------------
 * mbatchd.handler.c - add to the request dispatch switch:
 * ----------------------------------------------------------------------- */

/*
 * mbd_handle_compact_done - called when mbd_compact sends BATCH_COMPACT_DONE.
 * Reopen lsb.events and send BATCH_COMPACT_ACK.
 */
void mbd_handle_compact_done(int ch_id, struct packet_header *hdr)
{
    struct wire_compact_notify req;
    XDR xdrs;

    /* decode - nothing interesting in payload for now */
    memset(&req, 0, sizeof(req));

    LS_INFO("compact done received - reopening lsb.events");
    mbd_reopen_elog();

    /* send ack */
    char buf[PACKET_HEADER_SIZE + 64];
    struct packet_header ack_hdr;
    struct wire_compact_notify ack_payload;

    memset(&ack_payload, 0, sizeof(ack_payload));
    init_pack_hdr(&ack_hdr);
    ack_hdr.operation = BATCH_COMPACT_ACK;

    xdrmem_create(&xdrs, buf, sizeof(buf), XDR_ENCODE);
    if (!xdr_encodeMsg(&xdrs, (char *)&ack_payload, &ack_hdr,
                       xdr_wire_compact_notify, 0, NULL)) {
        LS_ERR("xdr_encodeMsg compact ack failed");
        xdr_destroy(&xdrs);
        return;
    }

    struct Buffer *out;
    int len = XDR_GETPOS(&xdrs);
    xdr_destroy(&xdrs);

    if (chan_alloc_buf(&out, len) < 0) {
        LS_ERR("chan_alloc_buf compact ack failed");
        return;
    }
    memcpy(out->data, buf, len);
    out->len = len;

    chan_enqueue(ch_id, out);
    chan_set_write_interest(ch_id, true);

    LS_INFO("compact ack sent");
}

/* -----------------------------------------------------------------------
 * mbd.main.c - launch mbd_compact after log is initialized:
 * ----------------------------------------------------------------------- */

static pid_t compact_pid = -1;

void mbd_compact_start(void)
{
    char events_path[PATH_MAX];
    char threshold_str[32];
    char interval_str[16];
    char clean_str[16];

    (void)snprintf(events_path, sizeof(events_path), "%s/lsb.events",
                   lsbParams[LSB_SHAREDIR].paramValue);
    (void)snprintf(threshold_str, sizeof(threshold_str), "%d",
                   COMPACT_DEFAULT_THRESHOLD);
    (void)snprintf(interval_str, sizeof(interval_str), "%d",
                   COMPACT_CHECK_INTERVAL);
    (void)snprintf(clean_str, sizeof(clean_str), "%d", clean_period);

    compact_pid = fork();
    if (compact_pid < 0) {
        LS_ERR("fork mbd_compact failed");
        return;
    }

    if (compact_pid == 0) {
        execl(LAVALITE_SBINDIR "/mbd_compact", "mbd_compact",
              "--events",       events_path,
              "--threshold",    threshold_str,
              "--interval",     interval_str,
              "--clean-period", clean_str,
              NULL);
        LS_ERR("execl mbd_compact failed");
        _exit(1);
    }

    LS_INFO("mbd_compact started pid=%d", compact_pid);
}

/*
 * mbd_compact_shutdown - reap compact child at mbd shutdown.
 * mbd exiting closes all fds — mbd_compact will fail its next
 * call_mbd() and exit on its own. waitpid with WNOHANG avoids blocking.
 */
void mbd_compact_shutdown(void)
{
    if (compact_pid > 0) {
        waitpid(compact_pid, NULL, WNOHANG);
        compact_pid = -1;
    }
}

/* -----------------------------------------------------------------------
 * mbd.log.c - add mbd_reopen_elog():
 * ----------------------------------------------------------------------- */

/*
 * mbd_reopen_elog - close and reopen lsb.events after compact rename.
 * log_fp and elogFname are static in mbd.log.c — this function lives there.
 */
void mbd_reopen_elog(void)
{
    if (log_fp) {
        fclose(log_fp);
        log_fp = NULL;
    }
    log_fp = fopen(elogFname, "a+");
    if (!log_fp)
        LS_ERR("fopen(%s) failed after compact", elogFname);
    else
        LS_INFO("lsb.events reopened");
}
