/*
 * mbd.compact.c - LavaLite event log compaction daemon
 *
 * Launched by mbd at startup via fork/exec.
 * Runs as a permanent child for the lifetime of mbd.
 *
 * Two-pass compact:
 *   Pass 1: collect jobids of expired finished jobs into a hash.
 *           A job is expired if its terminal JOB_STATUS (DONE or EXIT)
 *           eventTime is older than clean_period seconds.
 *   Pass 2: write all records whose jobid is NOT in the expired hash.
 *           Drop MBD_START, MBD_DIE, MBD_UNFULFILL, LOAD_INDEX entirely.
 *
 * Invariant: input lsb.events is always consistent — every job starts with
 * JOB_NEW and ends with JOB_STATUS DONE or EXIT. The compact preserves this.
 *
 * Copyright (C) LavaLite Contributors
 * GPL v2
 */

#include "lsbatch.h"
#include "lsf/lib/ll.hash.h"
#include "lsf/lib/ll.bufsiz.h"
#include "lsf/lib/ll.sys.h"
#include "lsbatch/daemons/daemonout.h"

enum compact_policy {
    COMPACT_CHECK_INTERVAL    = 60,
    COMPACT_DEFAULT_THRESHOLD = 64 * 1024 * 1024
};

static char events_path[PATH_MAX];
static char vapor_path[PATH_MAX];

static void     usage(const char *);
static int64_t  jobid_from_rec(struct eventRec *);
static int      should_drop(struct eventRec *);
static void     pass1(FILE *, struct ll_hash *, int, time_t);
static int      pass2(FILE *, FILE *, struct ll_hash *);
static int      compact(int);
static int      notify_mbd(void);

int main(int argc, char **argv)
{
    static struct option longopts[] = {
        { "events",       required_argument, 0, 'e' },
        { "threshold",    required_argument, 0, 't' },
        { "interval",     required_argument, 0, 'i' },
        { "clean-period", required_argument, 0, 'c' },
        { "help",         no_argument,       0, 'h' },
        { 0, 0, 0, 0 }
    };

    off_t threshold    = COMPACT_DEFAULT_THRESHOLD;
    int   interval     = COMPACT_CHECK_INTERVAL;
    int   clean_period = DEF_CLEAN_PERIOD;

    events_path[0] = 0;

    int cc;
    while ((cc = getopt_long(argc, argv, "e:t:i:c:h", longopts, NULL)) != -1) {
        switch (cc) {
        case 'e':
            snprintf(events_path, sizeof(events_path), "%s", optarg);
            break;
        case 't':
            threshold = (off_t)atol(optarg);
            break;
        case 'i':
            interval = atoi(optarg);
            break;
        case 'c':
            clean_period = atoi(optarg);
            break;
        case 'h':
        default:
            usage(argv[0]);
            return -1;
        }
    }

    if (events_path[0] == '\0') {
        usage(argv[0]);
        return -1;
    }

    snprintf(vapor_path, sizeof(vapor_path), "%s.vapor", events_path);

    if (lsb_init("mbd_compact") < 0) {
        ls_syslog(LOG_ERR, "mbd_compact: lsb_init failed");
        return 1;
    }

    // logdir comes from lsf.conf LSF_LOGDIR after lsb_init
    openlog("mbd_compact", LOG_NDELAY | LOG_PID, LOG_DAEMON);

    syslog(LOG_INFO, "started events=%s threshold=%ld interval=%d "
           "clean_period=%d", events_path, threshold, interval, clean_period);

    for (;;) {
        sleep(interval);

        struct stat st;
        if (stat(events_path, &st) < 0)
            continue;
        if (st.st_size < threshold)
            continue;

        syslog(LOG_INFO, "mbd_compact: size=%ld threshold=%ld compacting",
               st.st_size, threshold);

        if (compact(clean_period) < 0) {
            ls_syslog(LOG_ERR, "mbd_compact: compact failed, will retry");
            continue;
        }

        if (notify_mbd() < 0) {
            ls_syslog(LOG_ERR, "mbd_compact: notify failed");
            continue;
        }
    }

    return 0;
}

static void usage(const char *cmd)
{
    fprintf(stderr,
            "usage: %s --events PATH"
            " [--threshold BYTES]"
            " [--interval SECS]"
            " [--clean-period SECS]\n", cmd);
}

/*
 * compact - two-pass compact of lsb.events.
 */
static int compact(int clean_period)
{
    FILE *efp;
    FILE *vfp;
    struct ll_hash expired;
    time_t now = time(NULL);
    int rc;

    FILE *efp = fopen(events_path, "r");
    if (!efp) {
        syslog(LOG_ERR, "fopen(%s): %m", events_path);
        return -1;
    }

    if (ll_hash_init(&expired, 11) < 0) {
        syslog(LOG_ERR, "mbd_compact: ll_hash_init failed");
        fclose(efp);
        return -1;
    }

    pass1(efp, &expired, clean_period, now);

    syslog(LOG_INFO, "mbd_compact: pass1 done expired_jobs=%zu",
           expired.nentries);

    if (expired.nentries == 0) {
        ll_hash_free(&expired, NULL);
        fclose(efp);
        return 0;
    }

    vfp = fopen(vapor_path, "w");
    if (!vfp) {
        ls_syslog(LOG_ERR, "mbd_compact: fopen(%s): %m", vapor_path);
        ll_hash_free(&expired, NULL);
        fclose(efp);
        return -1;
    }
    fchmod(fileno(vfp), 0644);

    rc = pass2(efp, vfp, &expired);

    ll_hash_free(&expired, NULL);
    fclose(efp);

    if (fclose(vfp) != 0) {
        ls_syslog(LOG_ERR, "mbd_compact: fclose(%s): %m", vapor_path);
        unlink(vapor_path);
        return -1;
    }

    if (rc < 0) {
        unlink(vapor_path);
        return -1;
    }

    if (rename(vapor_path, events_path) < 0) {
        ls_syslog(LOG_ERR, "mbd_compact: rename: %m");
        unlink(vapor_path);
        return -1;
    }
    chmod(events_path, 0644);

    ls_syslog(LOG_INFO, "mbd_compact: compact done");
    return 0;
}

/*
 * jobid_from_rec - extract job_id from an event record.
 * Returns job_id or -1 if the event type carries no job_id.
 * No job arrays — access field directly, no LSB_JOBID macro.
 */
static int64_t jobid_from_rec(struct eventRec *rec)
{
    switch (rec->type) {
    case EVENT_JOB_NEW:
        return rec->eventLog.jobNewLog.jobId;
    case EVENT_JOB_START:
        return rec->eventLog.jobStartLog.jobId;
    case EVENT_JOB_START_ACCEPT:
        return rec->eventLog.jobStartAcceptLog.jobId;
    case EVENT_JOB_EXECUTE:
        return rec->eventLog.jobExecuteLog.jobId;
    case EVENT_JOB_STATUS:
        return rec->eventLog.jobStatusLog.jobId;
    case EVENT_JOB_SIGNAL:
        return rec->eventLog.signalLog.jobId;
    case EVENT_JOB_SWITCH:
        return rec->eventLog.jobSwitchLog.jobId;
    case EVENT_JOB_MOVE:
        return rec->eventLog.jobMoveLog.jobId;
    case EVENT_JOB_REQUEUE:
        return rec->eventLog.jobRequeueLog.jobId;
    default:
        return -1;
    }
}

/*
 * should_drop - returns 1 if this record type is always dropped.
 * MBD_START/DIE tracked by systemd journal.
 * MBD_UNFULFILL is a legacy hack not needed in LavaLite.
 * LOAD_INDEX not relevant post-replay.
 */
static int should_drop(struct eventRec *rec)
{
    switch (rec->type) {
    case EVENT_MBD_START:
    case EVENT_MBD_DIE:
    case EVENT_MBD_UNFULFILL:
    case EVENT_LOAD_INDEX:
        return 1;
    default:
        return 0;
    }
}

/*
 * pass1 - collect jobids of expired finished jobs.
 * Only EVENT_JOB_STATUS DONE/EXIT older than clean_period goes in the hash.
 */
static void pass1(FILE *efp, struct ll_hash *expired, int clean_period,
                  time_t now)
{
    int linenum = 0;
    char keybuf[LL_BUFSIZ_64];

    rewind(efp);
    lsberrno = LSBE_NO_ERROR;

    while (lsberrno != LSBE_EOF) {
        struct eventRec *rec;

        rec = lsb_geteventrec(efp, &linenum);
        if (!rec) {
            if (lsberrno != LSBE_EOF)
                syslog(LOG_ERR, "pass1 read error line %d", linenum);
            break;
        }

        if (rec->type != EVENT_JOB_STATUS)
            continue;

        int st = rec->eventLog.jobStatusLog.jStatus;
        if (!(st & (JOB_STAT_DONE | JOB_STAT_EXIT)))
            continue;

        if ((now - rec->eventTime) <= clean_period)
            continue;

        sprintf(keybuf, "%ld", rec->eventLog.jobStatusLog.jobId);
        ll_hash_insert(expired, keybuf, NULL, 0);
    }
}

/*
 * pass2 - write survivors to vapor.
 */
static int pass2(FILE *efp, FILE *vfp, struct ll_hash *expired)
{
    struct eventRec *rec;
    int linenum = 0;
    char keybuf[LL_BUFSIZ_64];

    rewind(efp);
    lsberrno = LSBE_NO_ERROR;

    while (lsberrno != LSBE_EOF) {
        rec = lsb_geteventrec(efp, &linenum);
        if (!rec) {
            if (lsberrno != LSBE_EOF)
                ls_syslog(LOG_ERR, "mbd_compact: pass2 read error line %d",
                          linenum);
            break;
        }

        if (should_drop(rec))
            continue;

        int64_t job_id = jobid_from_rec(rec);
        if (job_id >= 0) {
            sprintf(keybuf, "%ld", job_id);
            if (ll_hash_search(expired, keybuf) != NULL)
                continue;
        }

        if (lsb_puteventrec(vfp, rec) < 0) {
            ls_syslog(LOG_ERR, "mbd_compact: lsb_puteventrec failed line %d",
                      linenum);
            return -1;
        }
    }

    return 0;
}

/*
 * notify_mbd - connect, send BATCH_COMPACT_DONE, wait BATCH_COMPACT_ACK.
 */
static int notify_mbd(void)
{
    char req_buf[LL_BUFSIZ_512];
    char *reply_buf = NULL;
    struct packet_header hdr;
    struct wire_compact_notify req;
    XDR xdrs;
    int cc;

    memset(&req, 0, sizeof(req));
    init_pack_hdr(&hdr);
    hdr.operation = BATCH_COMPACT_DONE;

    xdrmem_create(&xdrs, req_buf, sizeof(req_buf), XDR_ENCODE);
    if (!xdr_encodeMsg(&xdrs, (char *)&req, &hdr,
                       xdr_wire_compact_notify, 0, NULL)) {
        xdr_destroy(&xdrs);
        return -1;
    }

    cc = call_mbd(req_buf, XDR_GETPOS(&xdrs), &reply_buf, &hdr, NULL);
    xdr_destroy(&xdrs);

    if (reply_buf)
        free(reply_buf);

    if (cc < 0)
        return -1;

    if (hdr.operation != BATCH_COMPACT_ACK) {
        ls_syslog(LOG_ERR, "mbd_compact: unexpected reply op=%d",
                  hdr.operation);
        return -1;
    }

    return 0;
}
