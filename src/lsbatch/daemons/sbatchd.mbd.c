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

static int sbd_handle_mbd_new_job(int, XDR *, struct packet_header *);
static sbdReplyType sbd_spawn_job(struct jobSpecs *, struct jobReply *);
static void sbd_child_exec_job(struct jobSpecs *);

// the ch_id in input is the channel we have opened with mbatchd
//
int sbd_handle_mbd(int ch_id)
{
    struct chan_data *chan = &channels[ch_id];

    LS_DEBUG("processing mbd request");

    if (chan->chan_events == CHAN_EPOLLERR) {
        LS_ERR("lost connection with mbd on channel %d", ch_id);
        chan_close(ch_id);
        // change the state of our connections to mbd
        sbd_mbd_chan = -1;
        return -1;
    }

    if (chan->chan_events != CHAN_EPOLLIN) {
        // channel is not ready
        return 0;
    }

    // Get the packet header from the channel first
    struct Buffer *buf;
    if (chan_dequeue(ch_id, &buf) < 0) {
        LS_ERR("chan_dequeue() failed");
        return -1;
    }

    if (!buf || buf->len < PACKET_HEADER_SIZE) {
        LS_ERR("short header from mbd on channel %d: len=%zu",
               ch_id, buf ? buf->len : 0);
        return -1;
    }

    XDR xdrs;
    struct packet_header hdr;
    // Allocate the buffer data based on what was sent
    xdrmem_create(&xdrs, buf->data, buf->len, XDR_DECODE);
    if (!xdr_pack_hdr(&xdrs, &hdr)) {
        LS_ERR("xdr_pack_hdr failed");
        xdr_destroy(&xdrs);
        return -1;
    }

    LS_DEBUG("mbd requesting operation %d", hdr.operation);

    switch (hdr.operation) {
    case MBD_NEW_JOB:
        sbd_handle_mbd_new_job(ch_id, &xdrs, &hdr);
        break;
    default:
        break;
    }

    xdr_destroy(&xdrs);
    return 0;
}


int  sbd_handle_mbd_new_job(int chfd, XDR *xdrs,
                            struct packet_header *req_hdr)
{
    sbdReplyType reply_code;

    // jobSpecs comes from mb d
    struct jobSpecs spec;
    memset(&spec, 0, sizeof(spec));

    // reply constructed by sbd
    struct jobReply job_reply;
    memset(&job_reply, 0, sizeof(job_reply));

    // 1) decode jobSpecs from mbd
    if (!xdr_jobSpecs((XDR *)xdrs, &spec, req_hdr)) {
        LS_ERR("xdr_jobSpecs failed");
        reply_code = ERR_BAD_REQ;
        goto send_reply;
    }

    LS_DEBUG("MBD_NEW_JOB: jobId=%u jobFile=%s", spec.jobId, spec.jobFile);

    // 2) duplicate NEW_JOB? just echo our view
    struct sbd_job *job;
    job = sbd_job_lookup(spec.jobId);
    if (job != NULL) {

        job_reply.jobId   = job->job_id;
        job_reply.jobPid  = job->pid;
        job_reply.jobPGid = job->pgid;
        job_reply.jStatus = job->spec.jStatus;

        reply_code = ERR_NO_ERROR;
        goto send_reply;
    }

    // 3) new job: spawn child, register it, fill job_reply
    reply_code = sbd_spawn_job(&spec, &job_reply);

send_reply:
    /* free heap members inside spec that xdr allocated */
    xdr_lsffree(xdr_jobSpecs, (char *)&spec, req_hdr);

    XDR xdrs2;
    char reply_buf[LL_BUFSIZ_4K];
    xdrmem_create(&xdrs2, reply_buf, LL_BUFSIZ_4K, XDR_ENCODE);

    struct packet_header reply_hdr;
    init_pack_hdr(&reply_hdr);
    // back to mbd
    reply_hdr.operation = reply_code;

    char *reply_struct;
    if (reply_code == ERR_NO_ERROR) {
        reply_struct = (char *)&job_reply;
    } else {
        reply_struct = NULL;
    }

    if (!xdr_encodeMsg(&xdrs2,
                       reply_struct,
                       &reply_hdr,
                       xdr_jobReply,
                       0,
                       NULL)) {
        LS_ERR("xdr_jobReply encode failed");
        xdr_destroy(&xdrs2);
        return -1;
    }

    if (chan_write(chfd, reply_buf, XDR_GETPOS(&xdrs2)) <= 0) {
        LS_ERR("chan_write jobReply (len=%d) failed",
               XDR_GETPOS(&xdrs2));
        xdr_destroy(&xdrs2);
        return -1;
    }

    xdr_destroy(&xdrs2);
    return 0;
}

static sbdReplyType sbd_spawn_job(struct jobSpecs *specs,
                                  struct jobReply *reply_out)
{
    // allocate sbatchd-local job object
    struct sbd_job *job = sbd_job_create(specs);
    if (job == NULL) {
        return ERR_MEM;
    }

    // use posix_spawn
    pid_t pid = fork();
    if (pid < 0) {
        LS_ERR("fork failed for job <%s>",
               lsb_jobid2str(specs->jobId));
        sbd_job_free(job);
        return ERR_FORK_FAIL;
    }

    if (pid == 0) {
        // child goes and runs the job
        chan_close(sbd_chan);
        chan_close(sbd_timer_chan);
        // child becomes leader of its own group
        setpgid(0, 0);
        sbd_child_exec_job(specs);
        _exit(127);   /* not reached unless exec fails */
    }

    // parent
    job->pid = pid;
    job->pgid  = pid;
    job->start_time = time(NULL);
    job->state = SBD_JOB_RUNNING;
    job->spec.jobPid  = pid;
    job->spec.jobPGid = pid;
    job->spec.jStatus = JOB_STAT_RUN;

    // register job locally BEFORE telling mbd
    sbd_job_insert(job);

    memset(reply_out, 0, sizeof(*reply_out));
    reply_out->jobId   = job->job_id;
    reply_out->jobPid  = job->pid;
    reply_out->jobPGid = job->pgid;
    reply_out->jStatus = job->spec.jStatus;

    LS_INFO("spawned job <%s> pid=%d", job->job_id, job->pid);

    return ERR_NO_ERROR;
}

static void sbd_child_exec_job(struct jobSpecs *specs)
{
    struct lenData jf;
    char script_path[PATH_MAX];

    memset(&jf, 0, sizeof(jf));

    specs->jobPid  = getpid();
    specs->jobPGid = specs->jobPid;

    // Bogus read the job file as part of jobSpec
#if 0
    if (rcvJobFile(chfd, &jf) == -1) {
        LS_ERR("rcvJobFile failed for job <%s>",
               lsb_jobid2str(specs->jobId));
        _exit(1);
    }
#endif
    snprintf(script_path,
             sizeof(script_path),
             "%s",
             specs->jobFile);

    char *argv[3];

    argv[0] = (char *)"/bin/sh";
    argv[1] = script_path;
    argv[2] = NULL;

    execv("/bin/sh", argv);

    fprintf(stderr,
            "sbatchd: unable to exec jobfile <%s> for job <%s>: %s\n",
            script_path,
            lsb_jobid2str(specs->jobId),
            strerror(errno));

    _exit(127);
}


void sbd_job_sync_jstatus(struct sbd_job *job)
{

}
/* ----------------------------------------------------------------------
 * allocate + init a new sbd_job from specs
 * does NOT insert into list/hash
 * -------------------------------------------------------------------- */
struct sbd_job *
sbd_job_create(const struct jobSpecs *specs)
{
    struct sbd_job *job;

    job = calloc(1, sizeof(*job));
    if (job == NULL) {
        LS_ERR("calloc sbd_job failed for jobId=%u", specs->jobId);
        return NULL;
    }

    /* copy full wire spec — we own this now */
    job->job_id = specs->jobId;
    job->spec   = *specs;      /* structure copy */
    job->pid    = -1;
    job->pgid   = -1;

    job->state  = SBD_JOB_PENDING;
    job->exit_status_valid = false;
    job->exit_status = 0;
    job->start_time = 0;

    return job;
}


/* ----------------------------------------------------------------------
 * insert job into global list + hash
 * assumes caller already allocated job
 * -------------------------------------------------------------------- */
void
sbd_job_insert(struct sbd_job *job)
{
    char keybuf[32];
    enum ll_hash_status rc;

    snprintf(keybuf, sizeof(keybuf), "%d", job->job_id);

    rc = ll_hash_insert(sbd_job_hash, keybuf, job, 0);
    if (rc != LL_HASH_INSERTED) {
        LS_ERR("ll_hash_insert failed for job_id=%d", job->job_id);
        return;
    }

    ll_list_append(&sbd_job_list, &job->list);

    LS_DEBUG("inserted job_id=%d", job->job_id);
}


/* ----------------------------------------------------------------------
 * simple lookup by jobId
 * -------------------------------------------------------------------- */
struct sbd_job *
sbd_job_lookup(int job_id)
{
    char keybuf[LL_BUFSIZ_32];

    snprintf(keybuf, sizeof(keybuf), "%d", job_id);
    return ll_hash_search(sbd_job_hash, keybuf);
}


/* ----------------------------------------------------------------------
 * unlink (remove) a job from list + hash
 * does NOT free job memory
 * -------------------------------------------------------------------- */
void
sbd_job_unlink(struct sbd_job *job)
{
    char keybuf[32];

    snprintf(keybuf, sizeof(keybuf), "%d", job->job_id);

    ll_hash_remove(sbd_job_hash, keybuf);
    ll_list_remove(&sbd_job_list, &job->list);
}


/* ----------------------------------------------------------------------
 * free job memory
 * -------------------------------------------------------------------- */
void
sbd_job_free(struct sbd_job *job)
{
    free(job);
}


/* ----------------------------------------------------------------------
 * foreach wrapper — executes fn(entry)
 * fn must cast entry back to struct sbd_job:
 *
 *   static void dump_job(struct ll_list_entry *e) {
 *       struct sbd_job *j = (struct sbd_job *) e;
 *       LS_INFO("job %d pid=%d state=%d", j->job_id, j->pid, j->state);
 *   }
 *
 *   sbd_job_foreach(dump_job);
 *
 * -------------------------------------------------------------------- */
void
sbd_job_foreach(void (*fn)(struct ll_list_entry *))
{
    ll_list_foreach(&sbd_job_list, fn);
}

#if 0
static const char *
sbd_state_name(enum sbd_job_state st)
{
    switch (st) {
    case SBD_JOB_PENDING:
        return "PENDING";
    case SBD_JOB_RUNNING:
        return "RUNNING";
    case SBD_JOB_EXITED:
        return "EXITED";
    case SBD_JOB_FAILED:
        return "FAILED";
    case SBD_JOB_KILLED:
        return "KILLED";
    default:
        return "UNKNOWN";
    }
}

void sbd_print_all_jobs(void)
{
    LS_INFO("---- current jobs ----");
    sbd_job_foreach(print_one_job);
    LS_INFO("---- end ----");
}
static void
print_one_job(struct ll_list_entry *entry)
{
    struct sbd_job *job;

    job = (struct sbd_job *) entry;

    /* placeholder: use spec.jobFile as job_name
       when you add job_name to sbd_job, replace here */
    const char *job_name;

    if (job->spec.jobFile[0] != '\0') {
        job_name = job->spec.jobFile;
    } else {
        job_name = "<unnamed>";
    }

    LS_INFO("job_id=%d  name=%s  state=%s  pid=%d",
            job->job_id,
            job_name,
            sbd_state_name(job->state),
            (int) job->pid);
}
#endif
