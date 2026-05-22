/*
 * Copyright (C) LavaLite Contributors
 * GPL v2
 */

#include "batch/mbd/mbd.h"

static FILE        *events_fp;
static char         events_path[PATH_MAX];
static off_t        events_threshold;
static unsigned int events_seq;   /* monotonic, persisted across switch */

void
events_init(const char *path, off_t threshold)
{
    snprintf(events_path, sizeof(events_path), "%s", path);
    events_threshold = threshold;
    events_seq = events_scan_seq(events_path); /* find highest .NNNNNN present */
    events_fp = fopen(events_path, "a");
    if (!events_fp)
        mbd_die(MBD_EXIT_EVENTS);
}

/*
 * maybe_switch_events - called from main loop on every timer tick.
 * Does nothing unless the file exceeds the threshold.
 * On switch: rename → dump live jobs → resume.
 * No child involvement. No rotation history. Fully synchronous.
 */
void maybe_switch_events(void)
{
    struct stat st;

    if (fstat(fileno(events_fp), &st) < 0)
        return;
    if (st.st_size < events_threshold)
        return;

    events_switch();
}

static void events_compact(void)
{
    char archived[PATH_MAX];

    events_seq++;
    snprintf(archived, sizeof(archived), "%s.%06u", events_path, events_seq);

    /* pause: close before rename so no buffered writes land in wrong file */
    if (fclose(events_fp) != 0)
        mbd_die(MBD_EXIT_EVENTS);
    events_fp = NULL;

    if (rename(events_path, archived) < 0) {
        LS_ERR("rename(%s, %s)", events_path, archived);
        mbd_die(MBD_EXIT_EVENTS);
    }

    /* open new file before dump — dump writes go here */
    events_fp = fopen(events_path, "a");
    if (!events_fp)
        mbd_die(MBD_EXIT_EVENTS);

    events_dump_live();

    if (fflush(events_fp) != 0 || fsync(fileno(events_fp)) != 0)
        mbd_die(MBD_EXIT_EVENTS);

    LS_INFO("events switched seq=%u archived=%s", events_seq, archived);
}

/*
 * events_dump_live - write minimal replay state for all live jobs.
 * PEND: JOB_NEW only.
 * RUN/SUSP: JOB_NEW + JOB_START (exec_host is enough to reconstruct).
 * Called only from events_switch(), file is already open.
 */
static void
events_dump_live(void)
{
    struct ll_list_entry *e;

    for (e = pend_jobs_list.head; e; e = e->next)
        event_write_job_new(events_fp, (struct job_data *)e);

    for (e = run_jobs_list.head; e; e = e->next) {
        struct job_data *job = (struct job_data *)e;
        event_write_job_new(events_fp, job);
        event_write_job_start(events_fp, job);
    }

    for (e = susp_jobs_list.head; e; e = e->next) {
        struct job_data *job = (struct job_data *)e;
        event_write_job_new(events_fp, job);
        event_write_job_start(events_fp, job);
        /* susp state reconstructed from JOB_START + absence of FINISH */
        /* or add EVENT_JOB_SUSP if replay needs to distinguish */
    }
}
