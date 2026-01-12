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

// some vars
int sbd_debug;

// sbatchd main loop
static void sbd_run_daemon(void);
static int sbd_init(const char *);
static void sbd_init_log(void);
static int sbd_init_network(void);
static int sbd_job_init(void);
static void sbd_init_signals(void);
static void sbd_reap_children(void);
static void sbd_maybe_reap_children(void);
static struct sbd_job *sbd_find_job_by_pid(pid_t);
static void job_status_checking(void);
static void job_finish_drive(void);
static void job_execute_drive(void);
static void sbd_reply_drive(void);
static void sbd_mbd_reconnect_try(void);
static bool_t sbd_drive_mbd_link(int, struct epoll_event *);
static bool_t sbd_pid_alive(pid_t);
static void sbd_cleanup(void);

// List and table of all jobs
struct ll_list sbd_job_list;
struct ll_hash *sbd_job_hash;

static uint16_t sbd_port;
// sbd_can to talk to external clients like monitors
int sbd_listen_chan = -1;
static int sbd_timer;
int sbd_timer_chan;
int sbd_mbd_chan = -1;
int sbd_efd;
static struct epoll_event *sbd_events;
static int max_events;
// Connection variables
bool_t connected = false;
bool_t sbd_mbd_connecting = false;

// Handler sets these variables to signal events
static volatile sig_atomic_t sbd_got_sigchld = 0;
static volatile sig_atomic_t sbd_croak = 0;

static struct option long_options[] = {
    {"debug", no_argument, 0, 'd'},
    {"envdir", required_argument, 0, 'e'},
    {"version", no_argument, 0, 'V'},
    {"help", no_argument, 0, 'h'},
    {0, 0, 0, 0}
};

static void usage(const char *cmd)
{
    fprintf(
        stderr,
        "Usage: %s [OPTIONS]\n"
        "  -d, --debug Run in foreground (no daemonize)\n"
        "  -V, --version     Print version and exit\n"
        "  -e, --envdir DIR  Path to env dir \n"
        "  -h, --help Show this help\n",
        cmd);
}

int main(int argc, char **argv)
{
    int cc;

    while ((cc = getopt_long(argc, argv, "de:Vh", long_options, NULL)) != EOF) {
        switch (cc) {
        case 'd':
            sbd_debug = true;
            break;
        case 'e':
            setenv("LSF_ENVDIR", optarg, 1);
            fprintf(stderr, "[lavalite] overriding LSF_ENVDIR=%s\n", optarg);
            break;
        case 'V':
            fprintf(stderr, "%s\n", LAVALITE_VERSION_STR);
            return 0;
        default:
            usage(argv[0]);
            return -1;
        }
    }

    if (!getenv("LSF_ENVDIR")) {
        fprintf(stderr, "sbatchd: LSF_ENVDIR must be defined\n");
        return -1;
    }

    int rc = sbd_init("sbatchd");
    if (rc < 0) {
        // print also the lserrno just in case
        LS_ERRX("sbatchd: sbd_init() failed %s", ls_sysmsg());
        sbd_cleanup();
        return 1;
    }

    // Run sbd run
    sbd_run_daemon();

    return 0;
}
// Exit only if the daemon cannot run its main loop.
// Never exit for control-plane/network unavailability.
static int sbd_init(const char *sbatch)
{
    // initenv will load genParams as well
    if (initenv_(lsbParams, NULL) < 0) {
        return -1;
    }

    // first thing first open the log so we can see messages
    // from the daemon right from the start
    sbd_init_log();

    sbd_init_signals();

    if (sbd_init_network() < 0) {
        LS_ERR("failed to initialize my network");
        return -1;
    }

    // The appname is useless but the api still wants it
    if (lsb_init("sbd") < 0) {
        LS_ERR("failed to initialize the batch library");
        return -1;
    }

    bool_t connected = false;
    // global channel to mbd
    sbd_mbd_chan = sbd_nb_connect_mbd(&connected);
    if (sbd_mbd_chan < 0) {
        LS_ERR("mbd link: initial connect attempt failed");
        sbd_mbd_chan = -1;
        sbd_mbd_connecting = false;
    } else {
        sbd_mbd_connecting = connected ? false : true;
        LS_INFO("mbd link: init ch=%d connecting=%d",
                sbd_mbd_chan, sbd_mbd_connecting);
    }

    // initialize the lists and hashes
    if (sbd_job_init() < 0) {
        // Bug handle this
    }

    // Initialize persistent job state storage early.
    // If we can't create/validate the state directory, we cannot guarantee
    // restart-safe job tracking, so fail fast before allocating resources.
    if (sbd_job_record_dir_init() < 0) {
        LS_ERR("failed to initialize persistent state storage");
        return -1;
    }
    // Reconstruct jobs seen before last shutdown.
    if (sbd_job_record_load_all() < 0) {
        LS_ERR("failed to load persistent job state");
        return -1;
    }

    // Remove from the internal data structure jobs that
    // were already reported to mbd. We don't want to carry
    // duplicate status for now as we assert it in multiple
    // places
    sbd_cleanup_job_list();

    // Green light we can start to operate

    return 0;
}

static void
sbd_run_daemon(void)
{
     /*
     * One-time status reconciliation after restart.
     *
     * After reloading job records from disk, some jobs may have already
     * finished while sbatchd was down. In that case their PIDs are no longer
     * alive, but their final status (DONE vs EXIT) has not yet been derived.
     *
     * job_status_checking() performs this reconciliation:
     *   - detects jobs whose PID is no longer alive,
     *   - reads the sidecar exit-status file written by the job wrapper,
     *   - sets exit_status / exit_status_valid,
     *   - derives JOB_STAT_DONE or JOB_STAT_EXIT in job->spec.jStatus.
     *
     * This must run once before entering the main loop so that
     * job_finish_drive() never attempts to enqueue a FINISH message for
     * a job whose final status has not been decided yet.
     */
    job_status_checking();

    while (1) {

        if (sbd_croak) {
            LS_INFO("terminate requested, exiting");
            exit(0);
        }

        sbd_maybe_reap_children();

        // SIGCHLD-driven fast path: try to finish/report immediately.
        job_finish_drive();

        // We pass -1 as the timer channel will ring
        int nready = chan_epoll(sbd_efd, sbd_events, max_events, -1);
        // save the epoll errno as the coming reap children can change it
        int epoll_errno = errno;

        // In case SIGCHLD arrived while we were blocked in epoll.
        sbd_maybe_reap_children();

        // Another quick finish pass after epoll wakeup.
        job_finish_drive();

        if (nready < 0) {
            if (epoll_errno == EINTR)
                continue;
            errno = epoll_errno;
            LS_ERR("network I/O");
            sleep(1);
            continue;
        }

        for (int i = 0; i < nready; i++) {
            struct epoll_event *ev = &sbd_events[i];
            int ch_id = (int)ev->data.u32;

             LS_DEBUG("epoll: ch_id=%d chan_events=%d kernel_events 0x%x",
                      ch_id, channels[ch_id].chan_events, ev->events);


             if (ch_id == sbd_timer_chan) {
                 uint64_t expirations;
                 ssize_t cc = read(chan_sock(ch_id), &expirations,
                                   sizeof(expirations));
                 if (cc < 0) {
                     if (errno == EINTR) {
                         LS_ERR("timer interrepted, do maintainance");
                         goto timer_maintenance;
                     }
                     LS_ERR("timer read failed, do maintenance");
                     goto timer_maintenance;
                     // fall through: still do maintenance
                 }
                 if ((size_t)cc != sizeof(expirations)) {
                     LS_ERR("timer short read: %zd bytes", cc);
                     // fall through: still do maintenance
                 }
                 LS_DEBUG("timer run %s", ctime2(NULL));
                 // Periodic maintenance: legacy job_checking + status_report.
                 // always run
             timer_maintenance:
                 sbd_reap_children();
                 sbd_mbd_reconnect_try();
                 sbd_reply_drive();
                 job_execute_drive();
                 job_finish_drive();
                 job_status_checking();
                 // rest the state
                 channels[ch_id].chan_events = CHAN_EPOLLNONE;
                 continue;
             }

             /* If the event is not for sbd_mbd_chan -> return false
              * If not connecting -> return false (let the existing
              * sbd_handle_mbd() path handle it)
              *
              * If connecting ->handle EPOLLOUT/ERR and return true
              */
             if (sbd_drive_mbd_link(ch_id, ev))
                 continue;

             // True skip partially read channels
             if (channels[ch_id].chan_events == CHAN_EPOLLNONE)
                 continue;

             // There is an event on the permament channel
             // connection with mbd
             if (ch_id == sbd_mbd_chan) {
                 sbd_handle_mbd(ch_id);
                 // reset the channel state
                 channels[ch_id].chan_events = CHAN_EPOLLNONE;
                 continue;
             }
             // There is an event on the sbd listening channel
             if (ch_id == sbd_listen_chan) {
                 handle_sbd_accept(ch_id);
                 channels[ch_id].chan_events = CHAN_EPOLLNONE;
                 continue;
             }
             // just to do it a bit different let's use a switch
             switch (channels[ch_id].chan_events) {

             case CHAN_EPOLLIN:
             case CHAN_EPOLLERR:
                 // Handle the even on an accepted sbd channel
                 handle_sbd_client(ch_id);
                 channels[ch_id].chan_events = CHAN_EPOLLNONE;
                 break;

             case CHAN_EPOLLNONE:
             default:
                 break;
             }

        }
    }
}
/*
 * sbd_drive_mbd_link()
 *
 * Drive the MBD link state machine from the main epoll loop.
 *
 * While connecting:
 * - EPOLLERR/HUP/RDHUP => connect failed: close channel, mark link down.
 * - EPOLLOUT           => connect finished: verify SO_ERROR, then switch
 *                         epoll interest to EPOLLIN and proceed to registration.
 *
 * While connected:
 * - Delegate to sbd_handle_mbd() to read/process messages.
 *
 * Reconnect is NOT performed here. On failure we mark the link down and
 * the periodic timer maintenance (or reconnect timer) will start a new attempt.
 *
 * Return values:
 *   true  -> event was handled (caller should continue)
 *   false -> not the MBD channel
 */
static bool_t sbd_drive_mbd_link(int ch_id, struct epoll_event *ev)
{
    // First thing first, check it is a sbd_mbd_chan
    if (ch_id != sbd_mbd_chan)
        return false;

    // Bail if we are not in the connecting process
    if (!sbd_mbd_connecting)
        return false;

    // During connect, errors/hup mean "attempt failed".
    if (ev->events & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) {
        LS_ERR("mbd link: connect failed (kernel_events=0x%x)", ev->events);

        chan_close(ch_id);
        sbd_mbd_chan = -1;
        sbd_mbd_connecting = false;

        channels[ch_id].chan_events = CHAN_EPOLLNONE;
        return true;
    }

    // Connect completion: must verify SO_ERROR via chan_connect_finish().
    if (ev->events & EPOLLOUT) {
        LS_DEBUG("mbd link: connect completion (EPOLLOUT)");

        if (chan_connect_finish(ch_id) < 0) {
            LS_ERR("mbd link: connect_finish failed: %m");

            chan_close(ch_id);
            sbd_mbd_chan = -1;
            sbd_mbd_connecting = false;

            channels[ch_id].chan_events = CHAN_EPOLLNONE;
            return true;
        }

        struct epoll_event ev2;
        ev2.events = EPOLLIN | EPOLLERR | EPOLLHUP | EPOLLRDHUP;
        ev2.data.u32 = (uint32_t)ch_id;

        if (epoll_ctl(sbd_efd, EPOLL_CTL_MOD, chan_sock(ch_id), &ev2) < 0) {
            LS_ERR("mbd link: epoll_ctl MOD failed after connect: %m");

            chan_close(ch_id);
            sbd_mbd_chan = -1;
            sbd_mbd_connecting = false;

            channels[ch_id].chan_events = CHAN_EPOLLNONE;
            return true;
        }
        // change the gloabal status sbd connecting
        sbd_mbd_connecting = false;

        LS_INFO("mbd link: connected ch=%d, enqueue registration", ch_id);

        // Enqueue registration request here (async).
        sbd_enqueue_register(ch_id);

        channels[ch_id].chan_events = CHAN_EPOLLNONE;
        return true;
    }

    // Still connecting, nothing to do for this event.
    LS_DEBUG("mbd link: still connecting (kernel_events=0x%x)", ev->events);

    channels[ch_id].chan_events = CHAN_EPOLLNONE;
    return true;
}

// Check if mbd is connected
bool_t sbd_mbd_link_ready(void)
{
    return (sbd_mbd_chan >= 0 && !sbd_mbd_connecting);
}

/*
 * sbd_mbd_reconnect_try()
 *
 * Attempt a single non-blocking reconnect to MBD.
 *
 * This function never blocks and never sleeps. It is safe to call from
 * the periodic timer maintenance path.
 *
 * If a connect attempt is started, sbd_mbd_chan is set and
 * sbd_mbd_connecting reflects whether we are waiting for EPOLLOUT.
 *
 * If the connect completes immediately, the registration request can be
 * enqueued here.
 */
static void
sbd_mbd_reconnect_try(void)
{
    if (sbd_mbd_chan >= 0)
        return;

    if (sbd_mbd_connecting)
        return;

    bool_t connected = false;
    int ch = sbd_nb_connect_mbd(&connected);
    if (ch < 0) {
        LS_DEBUG("mbd link: reconnect try failed");
        return;
    }

    sbd_mbd_chan = ch;
    sbd_mbd_connecting = connected ? false : true;

    LS_INFO("mbd link: reconnect started ch=%d connecting=%d",
            sbd_mbd_chan, sbd_mbd_connecting);

    if (connected) {
        // Connected immediately: enqueue registration request now (async).
        sbd_enqueue_register(sbd_mbd_chan);
    }
}

static void sbd_init_log(void)
{
    const char *log_dir = genParams[LSF_LOGDIR].paramValue;
    const char *log_mask = genParams[LSF_LOG_MASK].paramValue;

    bool_t debug = sbd_debug;

    if (!log_dir)
        log_dir = "/var/log/lavalite"; /* fallback */

    if (!log_mask)
        log_mask = "LOG_INFO"; /* sane default */

    // Initialize LavaLite logging
    if (debug) {
        ls_openlog("sbatchd", log_dir, true, 0, log_mask);
        LS_INFO("Starting sbatchd in debug mode");
    } else {
        // Normal production daemon case
        ls_openlog("sbatchd", log_dir, false, 0, log_mask);
    }

    // tag the log messages as we share on log file with
    // children
    ls_setlogtag("parent");

    LS_INFO("sbatchd logging initialized: dir=%s mask=%s debug=%d",
            log_dir, log_mask, debug);

}

static int
sbd_init_network(void)
{
    // create the listening port and the sbd channel
    if (genParams[LSB_SBD_PORT].paramValue == NULL) {
        LS_ERR("LSB_SBD_PORT is not defined");
        return -1;
    }

    // create the listining port and the sbd channel
    sbd_port = (uint16_t)atoi(genParams[LSB_SBD_PORT].paramValue);
    if (sbd_port <= 0) {
        LS_ERR("the LSB_SBD_PORT is not defined correcly %s",
               genParams[LSB_SBD_PORT].paramValue);
        return -1;
    }

    // now open the sbd server channel
    sbd_listen_chan = chan_listen_socket(SOCK_STREAM, sbd_port,
                                  SOMAXCONN, CHAN_OP_SOREUSE);
    if (sbd_listen_chan < 0) {
        LS_ERR("Failed to initialize the sbd channel, another sbd running?");
        return -1;
    }

    // init the epoll
    sbd_efd = epoll_create1(EPOLL_CLOEXEC);
    if (sbd_efd < 0) {
        LS_ERR("epoll_create1() failed: %m");
        chan_close(sbd_listen_chan);
        return -1;
    }

    long max_open = sysconf(_SC_OPEN_MAX);
    if (max_open <= 0) {
        max_open = 1024;
    }
    max_events = (int)max_open;

    // epoll_event array used as part of the interface to chan_epoll
    sbd_events = calloc(max_events, sizeof(struct epoll_event));
    if (sbd_events == NULL) {
        LS_ERR("faild to calloc epoll_events");
        close(sbd_efd);
        chan_close(sbd_listen_chan);
        return -1;
    }

    // sbd main channel
    struct epoll_event ev = {.events = EPOLLIN, .data.u32 = (uint32_t)sbd_listen_chan};
    if (epoll_ctl(sbd_efd, EPOLL_CTL_ADD, chan_sock(sbd_listen_chan), &ev) < 0) {
        LS_ERR("epoll_ctl() failed to add sbd chan");
        free(sbd_events);
        close(sbd_efd);
        chan_close(sbd_listen_chan);
        return -1;
    }

    // for now
    sbd_timer = 10; // seconds
    // this function is in the channel library.
    sbd_timer_chan = chan_create_timer(sbd_timer);
    if (sbd_timer_chan < 0) {
        free(sbd_events);
        close(sbd_efd);
        chan_close(sbd_listen_chan);
        return -1;
    }

    //sbd timer channel
    ev.events = EPOLLIN;
    ev.data.u32 = sbd_timer_chan;
    if (epoll_ctl(sbd_efd, EPOLL_CTL_ADD, chan_sock(sbd_timer_chan), &ev) < 0) {
        LS_ERR("epoll_ctl() failed to add sbd chan");
        close(sbd_efd);
        chan_close(sbd_listen_chan);
        return -1;
    }

    LS_INFO("sbatchd listening on port %d chan: %d, epoll_fd: %d timer: %dsec",
            sbd_port, sbd_listen_chan, sbd_efd, sbd_timer);

    return 0;
}

static int sbd_job_init(void)
{
    // list is a plain global struct; just initialize its fields
    ll_list_init(&sbd_job_list);

    // hash is a pointer; allocate a table for it
    sbd_job_hash = ll_hash_create(0);   // 0 → default (e.g. 11 buckets)
    if (!sbd_job_hash) {
        LS_ERR("failed to create job hash table");
        return -1;
    }

    return 0;
}

static void sbd_croak_handler(int sig)
{
    (void)sig;
    sbd_croak = 1;
}

static void sbd_child_handler(int sig)
{
    (void)sig;
    sbd_got_sigchld = 1;
}

static void sbd_init_signals(void)
{
    LS_INFO("initializing signals");
    signal_set(SIGTERM, sbd_croak_handler);
    signal_set(SIGINT, sbd_croak_handler);
    signal_set(SIGCHLD, sbd_child_handler);
}

static void sbd_reap_children(void)
{
    int status;
    pid_t pid;

    for (;;) {
        pid = waitpid(-1, &status, WNOHANG);
        if (pid <= 0) {
            if (pid < 0) {

                if (errno == EINTR)
                    continue;
                if (errno != ECHILD)
                    LS_ERR("waitpid(-1) failed");
            }
            break;
        }

        struct sbd_job *job = sbd_find_job_by_pid(pid);
        if (job == NULL) {
            LS_WARNING("reaped unknown child pid=%d status=0x%x",
                       (int)pid, (unsigned)status);
            continue;
        }

        // this should impossible
        if (job->pid != pid) {
            LS_ERR("job %"PRId64" pid mismatch: job->pid=%d waitpid=%d",
                   job->job_id, (int)job->pid, (int)pid);
            assert(0);
            continue;
        }

        // fill the job exit status value from wait
        // into the job structure
        job->exit_status = status;
        job->exit_status_valid = true;
        job->end_time = time(NULL);

        if (WIFEXITED(status)) {
            job->state = SBD_JOB_EXITED;
            if (WEXITSTATUS(status) == 0)
                job->spec.jStatus = JOB_STAT_DONE;
            else
                job->spec.jStatus = JOB_STAT_EXIT;
        } else if (WIFSIGNALED(status)) {
            job->state = SBD_JOB_KILLED;
            job->spec.jStatus = JOB_STAT_EXIT;
        }

        LS_INFO("job %"PRId64 " finished pid=%d jStatus=0x%x status=0x%x",
                job->job_id, (int)pid, job->spec.jStatus, (unsigned)status);
    }
}

static void sbd_maybe_reap_children(void)
{
    if (!sbd_got_sigchld)
        return;
    sbd_got_sigchld = 0;
    sbd_reap_children();
}

static void job_status_checking(void)
{
    time_t now = time(NULL);
    struct ll_list_entry *e;

    // NOTE: ll_list_entry must remain the first field (list base object idiom)
    for (e = sbd_job_list.head; e; e = e->next) {
        struct sbd_job *job = (struct sbd_job *)e;

        if (job->state != SBD_JOB_RUNNING)
            continue;

        job->spec.runTime = (int)(now - job->spec.startTime);
        if (job->spec.runTime < 0)
            job->spec.runTime = 0;

        if (!sbd_pid_alive(job->pid)) {
            LS_WARNING("job %"PRId64": pid %ld not alive after restart?",
                       job->job_id, (long)job->pid);
             job->finish_sent = false;  // ensue finish_drive enqueues
             // read the exit code from the exit status file of the job
             // created by the job file
             int exit_code;
             time_t done_time;
             int cc = sbd_read_exit_status_file(job->job_id,
                                                &exit_code,
                                                &done_time);
             if (cc < 0) {
                 LS_ERR("failed to read job %ld exist status assume "
                        "JOB_STAT_EXIT", job->job_id);
                 exit_code = 1;
             }
             job->exit_status_valid = true;
             job->exit_status = exit_code;
             // Derive final status bits from exit code
             job->spec.jStatus &= ~(JOB_STAT_DONE | JOB_STAT_EXIT);
             if (exit_code == 0)
                 job->spec.jStatus |= JOB_STAT_DONE;
             else
                 job->spec.jStatus |= JOB_STAT_EXIT;
        }
    }
}

// Drive the jobReply to mbd after we have sbd created a new job
static void sbd_reply_drive(void)
{
    // Check it we are connected to mbd
    if (! sbd_mbd_link_ready()) {
        LS_INFO("mbd link not ready, skip and sbd_mbd_reconnect_try");
        return;
    }

    struct ll_list_entry *e;

    for (e = sbd_job_list.head; e; e = e->next) {
        struct sbd_job *job = (struct sbd_job *)e;

        if (job->pid_acked)
            continue;

        if (job->reply_sent)
            continue;

        if (sbd_enqueue_reply(job->reply_code, &job->job_reply) < 0) {
            LS_ERR("job %"PRId64" enqueue PID snapshot failed",
                   job->job_id);
            continue;
        }

        job->reply_sent = TRUE;
        LS_DEBUG("job %"PRId64" PID snapshot enqueued (awaiting ACK)",
                 job->job_id);
    }

}

static void job_execute_drive(void)
{
    // Check it we are connected to mbd
    if (! sbd_mbd_link_ready()) {
        LS_INFO("mbd link not ready, skip and sbd_mbd_reconnect_try");
        return;
    }

    struct ll_list_entry *e;

    for (e = sbd_job_list.head; e; e = e->next) {
        struct sbd_job *job = (struct sbd_job *)e;

        if (job->missing)
            continue;

        if (!job->pid_acked) {
            LS_DEBUG("job %"PRId64" not ready: pid not acked yet", job->job_id);
            continue;
        }

        if (job->execute_sent) {
            LS_DEBUG("job %"PRId64" BATCH_JOB_EXECUTE sent already",
                     job->job_id);
            continue;
        }

        if (!job->execute_sent) {
            if (sbd_enqueue_execute(job) < 0) {
                LS_ERR("job %"PRId64" enqueue BATCH_JOB_EXECUTE failed",
                       job->job_id);
                return;
            }
            job->execute_sent = true;
            LS_INFO("job %"PRId64" BATCH_JOB_EXECUTE enqueued", job->job_id);
            if (sbd_job_record_write(job) < 0)
                LS_ERRX("job %"PRId64": record write failed after reply_sent",
                        job->job_id);
            continue;
        }
    }
}

static void job_finish_drive(void)
{
    // Check it we are connected to mbd
    if (! sbd_mbd_link_ready()) {
        LS_INFO("mbd link not ready, skip and sbd_mbd_reconnect_try");
        return;
    }

    struct ll_list_entry *e;

    for (e = sbd_job_list.head; e; e = e->next) {
        struct sbd_job *job = (struct sbd_job *)e;

        if (job->missing)
            continue;

        // pid_sent is the hard gate: mbd must have recorded pid/pgid first.
        if (!job->pid_acked) {
            LS_DEBUG("job %"PRId64" not ready: pid not acked yet", job->job_id);
            continue;
        }

        if (!job->execute_acked) {
            LS_DEBUG("job %"PRId64" not acked JOB_BATCH_EXECUTE yet",
                     job->job_id);
            // Do not attempt finish in the same pass unless execute is sent.
            continue;
        }

        if (! job->exit_status_valid) {
            LS_DEBUG("job %"PRId64" has not finished yet", job->job_id);
            continue;
        }

        // After execute is sent, we can send finish once we have an exit status.
        if (!job->finish_sent) {
            int rc = sbd_enqueue_finish(job);
            if (rc == 0) {
                job->finish_sent = true;
                // log the job record
                if (sbd_job_record_write(job) < 0)
                    LS_ERRX("job %"PRId64": record write failed after reap",
                            job->job_id);
                LS_INFO("job %"PRId64" finish enqueued", job->job_id);
            } else {
                LS_WARNING("job %"PRId64" finish enqueue failed", job->job_id);
            }
        }
    }
}


// We reaped a child find to which sbd_job does belong
static struct sbd_job *sbd_find_job_by_pid(pid_t pid)
{
    struct ll_list_entry *e;

    for (e = sbd_job_list.head; e; e = e->next) {
        struct sbd_job *job = (struct sbd_job *)e;
        if (job->pid == pid)
            return job;
    }
    return NULL;
}

static bool_t sbd_pid_alive(pid_t pid)
{
    if (pid <= 0)
        return false;

    if (kill(pid, 0) == 0)
        return true;

    if (errno == EPERM)
        return true;   // exists, just not permitted

    return false; // ESRCH or other -> treat as not alive
}

// sbd_init() failed so as a good coding hygiene we clean up file
// descriptors and buffers
static void
sbd_cleanup(void)
{
    chan_close(sbd_listen_chan);
    chan_close(sbd_timer_chan);
    chan_close(sbd_efd);
    // free the hash but not the job entries
    ll_hash_free(sbd_job_hash, NULL, NULL);
    // use clear so that we dont free the pointer that
    // is in the static area and not on the heap
    ll_list_clear(&sbd_job_list, sbd_job_free);
    ls_closelog();
}
