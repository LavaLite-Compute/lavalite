/*
 * mbd_compact.c - LavaLite event log compaction daemon
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

#include "lsbatch/lib/lsb.h"
#include "lsf/lib/ll.hash.h"

enum compact_policy {
    // how often to check
    COMPACT_CHECK_INTERVAL    = 60,
    // how big the lsb.events file has to be
    // COMPACT_DEFAULT_THRESHOLD = 64 * 1024 * 1024,
    // one simple sleep job is ~1K
    COMPACT_DEFAULT_THRESHOLD = 10 * 1024 * 1024,
    // how old must be the DONE||EXIT record before the whole
    // job is vaporized
    COMPACT_CLEAN_PERIOD  = 3600
};

static char events_path[PATH_MAX];
static char vapor_path[PATH_MAX + LL_BUFSIZ_16];
static int  debug_mode;
static time_t compact_time;

static void usage(const char *);
static int should_skip(struct eventRec *, struct ll_hash *);
static int jobid_from_rec(struct eventRec *);
static int pass1(FILE *, struct ll_hash *, int, time_t);
static int pass2(FILE *, FILE *, struct ll_hash *);
static int rotate_events(void);
static int compact(int);
static int notify_mbd(int);

int main(int argc, char **argv)
{
    static struct option longopts[] = {
        { "events",       required_argument, 0, 'e' },
        { "threshold",    required_argument, 0, 't' },
        { "interval",     required_argument, 0, 'i' },
        { "clean-period", required_argument, 0, 'c' },
        { "logdir",       required_argument, 0, 'l' },
        { "log-mask",     required_argument, 0, 'm' },
        { "help",         no_argument,       0, 'h' },
        { 0, 0, 0, 0 }
    };

    off_t threshold    = COMPACT_DEFAULT_THRESHOLD;
    int   interval     = COMPACT_CHECK_INTERVAL;
    int   clean_period = COMPACT_CLEAN_PERIOD;
    const char *logdir   = "/tmp";
    const char *log_mask = "LOG_INFO";

    events_path[0] = 0;

    int opt;
    while ((opt = getopt_long(argc, argv, "e:t:i:c:l:m:h", longopts, NULL)) != -1) {
        switch (opt) {
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
        case 'l':
            logdir = optarg;
            break;
        case 'm':
            log_mask = optarg;
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

    if (strcmp(log_mask, "LOG_DEBUG") == 0)
        debug_mode = true;

    snprintf(vapor_path, sizeof(vapor_path), "%s.vapor", events_path);

    if (lsb_init("mbd_compact") < 0) {
        fprintf(stderr, "mbd_compact: lsb_init failed\n");
        return 1;
    }

    ls_openlog("compactor", logdir, debug_mode, 0, (char *)log_mask);

    LS_INFO("started events=%s threshold=%ld interval=%d clean_period=%d",
            events_path, (long)threshold, interval, clean_period);

    int mbd_pid = -1;
    char *p = getenv("MBD_PID");
    if (p)
        mbd_pid = atoi(p);

    // we just started
    time_t next_compact_time = time(NULL) + interval;
    for (;;) {

        sleep(2);

        if (mbd_pid != -1) {
            int cc = kill(mbd_pid, 0);
            if (cc < 0) {
                LS_INFO("mbd pid %d gone, exiting...", mbd_pid);
                break;
            }
        }

        time_t current_time = time(NULL);
        if (current_time < next_compact_time)
            continue;
        next_compact_time = current_time + interval;

        struct stat st;
        if (stat(events_path, &st) < 0) {
            LS_ERR("stat(%s)", events_path);
            continue;
        }

        LS_INFO("size=%ld threshold=%ld try compacting",
                (long)st.st_size, (long)threshold);

        if (st.st_size < threshold)
            continue;

        int cc = compact(clean_period);
        if (cc < 0) {
            LS_ERRX("compact failed, notifying mbd");
        }
        if (notify_mbd(cc) < 0)
            LS_ERRX("notify_mbd failed");

        // always continue - retry next interval
    }

    return 0;
}

static void usage(const char *cmd)
{
    fprintf(stderr,
            "usage: %s --events PATH"
            " [--threshold BYTES]"
            " [--interval SECS]"
            " [--clean-period SECS]"
            " [--logdir DIR]"
            " [--log-mask MASK]", cmd);
}

/*
 * rotate_events - rotate lsb.events history before installing compacted file.
 * lsb.events.1 is most recent, lsb.events.N is oldest.
 * Rotates N→N+1 ... 1→2, then current→1.
 * Returns 0 on success, -1 on failure.
 * On failure the original lsb.events is untouched.
 */
static int rotate_events(void)
{
    char src[PATH_MAX + LL_BUFSIZ_16];
    char dst[PATH_MAX + LL_BUFSIZ_16];
    struct stat st;
    int i = 0;

    /* find highest existing numbered file */
    while (1) {
        snprintf(src, sizeof(src), "%s.%d", events_path, i + 1);
        if (stat(src, &st) < 0)
            break;
        i++;
    }

    /* rotate from top down: N→N+1, ..., 1→2 */
    for (; i >= 1; i--) {
        snprintf(src, sizeof(src), "%s.%d", events_path, i);
        snprintf(dst, sizeof(dst), "%s.%d", events_path, i + 1);
        if (rename(src, dst) < 0) {
            LS_ERR("rename(%s, %s)", src, dst);
            return -1;
        }
    }

    /* current → .1 */
    snprintf(dst, sizeof(dst), "%s.1", events_path);
    if (rename(events_path, dst) < 0) {
        LS_ERR("rename(%s, %s)", events_path, dst);
        return -1;
    }

    return 0;
}

/*
 * compact - two-pass compact of lsb.events.
 * On any failure: vapor is unlinked, lsb.events is untouched.
 */
static int compact(int clean_period)
{
    // Bug how to handle failure in the previous run
    struct stat st;
    if (stat(vapor_path, &st) == 0) {
        LS_WARNING("stale vapor found — previous compact incomplete, removing");
        unlink(vapor_path);
    }

    /*
     * table_size ~ N/0.7. At 64M we have ~200K records if they were
     * all JOB_NEW (~300 bytes each). Finished jobs are a fraction.
     * 1601 is a prime that fits comfortably.
     */
    struct ll_hash expired;
    if (ll_hash_init(&expired, 1601) < 0) {
        LS_ERRX("ll_hash_init failed");
        return -1;
    }

    // tick tock
    compact_time = time(NULL);
    FILE *efp = fopen(events_path, "r");
    if (!efp) {
        LS_ERR("fopen(%s)", events_path);
        return -1;
    }

    if (pass1(efp, &expired, clean_period, compact_time) < 0) {
        LS_ERRX("pass1 failed");
        ll_hash_clear(&expired, NULL);
        fclose(efp);
        return -1;
    }

    LS_INFO("pass1 done expired_jobs=%zu", expired.nentries);

    if (expired.nentries == 0) {
        ll_hash_clear(&expired, NULL);
        fclose(efp);
        LS_INFO("nothing to compact");
        return 0;
    }

    FILE *vfp = fopen(vapor_path, "w");
    if (!vfp) {
        LS_ERR("fopen(%s)", vapor_path);
        ll_hash_clear(&expired, NULL);
        fclose(efp);
        return -1;
    }
    fchmod(fileno(vfp), 0644);

    if (pass2(efp, vfp, &expired) < 0) {
        LS_ERRX("pass2 failed");
        ll_hash_clear(&expired, NULL);
        fclose(efp);
        fclose(vfp);
        unlink(vapor_path);
        return -1;
    }

    if (fclose(efp) < 0) {
        LS_ERR("fclose(%s) — disk full or I/O error", events_path);
        ll_hash_clear(&expired, NULL);
        unlink(vapor_path);
        return -1;
    }

    if (fclose(vfp) != 0) {
        LS_ERR("fclose(%s) — disk full or I/O error", vapor_path);
        ll_hash_clear(&expired, NULL);
        unlink(vapor_path);
        return -1;
    }

    if (rotate_events() < 0) {
        LS_ERRX("rotate failed — vapor discarded, lsb.events untouched");
        ll_hash_clear(&expired, NULL);
        unlink(vapor_path);
        return -1;
    }

    if (rename(vapor_path, events_path) < 0) {
        LS_ERR("rename(%s, %s) — CRITICAL", vapor_path, events_path);
        ll_hash_clear(&expired, NULL);
        return -1;
    }
    chmod(events_path, 0644);
    ll_hash_clear(&expired, NULL);

    LS_INFO("compact done");

    return 0;
}

/*
 * pass1 - collect jobids of expired finished jobs.
 * Only EVENT_JOB_STATUS DONE/EXIT older than clean_period goes in the hash.
 * Returns 0 on success (including clean EOF), -1 on read error.
 */
static int pass1(FILE *efp, struct ll_hash *expired, int clean_period,
                  time_t compact_time)
{
    int linenum = 0;
    char keybuf[LL_BUFSIZ_64];

    rewind(efp);
    lsberrno = LSBE_NO_ERROR;

    while (lsberrno != LSBE_EOF) {
        struct eventRec *rec = lsb_geteventrec(efp, &linenum);
        if (!rec) {
            if (lsberrno != LSBE_EOF) {
                LS_ERRX("read error line %d", linenum);
                return -1;
            }
            break;
        }

        if (rec->type != EVENT_JOB_STATUS)
            continue;

        int jst = rec->eventLog.jobStatusLog.jStatus;
        if (!(jst & (JOB_STAT_DONE | JOB_STAT_EXIT)))
            continue;

        if ((compact_time - rec->eventTime) <= clean_period)
            continue;

        sprintf(keybuf, "%d", rec->eventLog.jobStatusLog.jobId);
        ll_hash_insert(expired, keybuf, NULL, 0);

        if (debug_mode)
            LS_DEBUG("job=%d expired", rec->eventLog.jobStatusLog.jobId);
    }

    return 0;
}

static int pass2(FILE *efp, FILE *vfp, struct ll_hash *expired)
{
    int linenum = 0;

    rewind(efp);
    lsberrno = LSBE_NO_ERROR;

    while (lsberrno != LSBE_EOF) {
        struct eventRec *rec = lsb_geteventrec(efp, &linenum);
        if (!rec) {
            if (lsberrno != LSBE_EOF) {
                LS_ERRX("read error line %d", linenum);
                return -1;
            }
            break;
        }

        if (should_skip(rec, expired)) {
            if (debug_mode)
                LS_DEBUG("line=%d type=%d vaporized", linenum, rec->type);
            continue;
        }

        if (lsb_puteventrec(vfp, rec) < 0) {
            LS_ERRX("lsb_puteventrec failed line %d", linenum);
            return -1;
        }
    }

    return 0;
}

static int should_skip(struct eventRec *rec, struct ll_hash *expired)
{
    switch (rec->type) {
    case EVENT_MBD_START:
    case EVENT_MBD_DIE:
    case EVENT_LOAD_INDEX:
        return 1;
    default:
        break;
    }

    int job_id = jobid_from_rec(rec);
    if (job_id < 0)
        return 0;

    char keybuf[LL_BUFSIZ_64];
    sprintf(keybuf, "%d", job_id);
    return ll_hash_contains(expired, keybuf);
}

/*
 * jobid_from_rec - extract job_id from an event record.
 * Returns job_id or -1 if the event type carries no job_id.
 * No job arrays — access field directly, no LSB_JOBID macro.
 */
static int jobid_from_rec(struct eventRec *rec)
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
        LS_ERRX("unknown event");
        return -1;
    }
}

/*
 * notify_mbd - send BATCH_COMPACT_DONE or BATCH_COMPACT_FAILED,
 * wait for BATCH_COMPACT_ACK.
 */
static int notify_mbd(int status)
{
    struct wire_compact_notify req;
    memset(&req, 0, sizeof(req));

    // set the result of the compacting and the time
    // at which it happened
    req.status = status;
    req.compact_time = compact_time;

    struct packet_header hdr;
    init_pack_hdr(&hdr);
    hdr.operation = BATCH_COMPACT_DONE;
    if (status < 0)
        hdr.operation = BATCH_COMPACT_FAILED;

    XDR xdrs;
    char req_buf[LL_BUFSIZ_512];
    xdrmem_create(&xdrs, req_buf, sizeof(req_buf), XDR_ENCODE);
    if (!xdr_encodeMsg(&xdrs, (char *)&req, &hdr,
                       xdr_wire_compact_notify, 0, NULL)) {
        LS_ERRX("xdr_encodeMsg failed");
        xdr_destroy(&xdrs);
        return -1;
    }

    LS_INFO("compactor time %ld", compact_time);

    char *reply_buf = NULL;
    int cc = call_mbd(req_buf, XDR_GETPOS(&xdrs), &reply_buf, &hdr, NULL);
    xdr_destroy(&xdrs);
    free(reply_buf);

    if (cc < 0) {
        LS_ERRX("call_mbd failed: %s", lsb_sysmsg());
        return -1;
    }

    if (hdr.operation != BATCH_COMPACT_ACK) {
        LS_ERRX("unexpected reply op=%d", hdr.operation);
        return -1;
    }

    if (debug_mode)
        LS_DEBUG("ack received");

    return 0;
}
