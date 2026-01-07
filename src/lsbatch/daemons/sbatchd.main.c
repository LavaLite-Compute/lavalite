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
static void job_finish_checking(void);
static void job_pipeline_drive(void);

// List and table of all jobs
struct ll_list sbd_job_list;
struct ll_hash *sbd_job_hash;

static uint16_t sbd_port;
// sbd_can to talk to external clients like monitors
int sbd_chan;
static int sbd_timer;
int sbd_timer_chan;
int sbd_mbd_chan = -1;
int sbd_efd;
static struct epoll_event *sbd_events;
static int max_events;

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
        fprintf(stderr, "sbatchd: lsb_init() failed: %s\n", ls_sysmsg());
        return 1;
    }

    // Run sbd run
    sbd_run_daemon();

    return 0;
}

static void
sbd_run_daemon(void)
{
    while (1) {

        if (sbd_croak) {
            LS_INFO("terminate requested, exiting");
            exit(0);
        }

        if (sbd_got_sigchld) {
            sbd_got_sigchld = 0;
            sbd_reap_children();
        }
        sbd_maybe_reap_children();

        // SIGCHLD-driven fast path: try to finish/report immediately.
        job_finish_checking();

        // We pass -1 as the timer channel will ring
        int nready = chan_epoll(sbd_efd, sbd_events, max_events, -1);

        // In case SIGCHLD arrived while we were blocked in epoll.
        sbd_maybe_reap_children();

        // Another quick finish pass after epoll wakeup.
        job_finish_checking();

        if (nready < 0) {
            if (errno == EINTR)
                continue;
            LS_ERR("network I/O");
            sleep(1);
            continue;
        }

        for (int i = 0; i < nready; i++) {
            struct epoll_event *ev = &sbd_events[i];
            int ch_id = (int)ev->data.u32;

             LS_DEBUG("epoll: ch_id=%d chan_events=%d kernel_events 0x%x",
                      ch_id, channels[ch_id].chan_events, ev->events);

             // True skip partially read channels
             if (channels[ch_id].chan_events == CHAN_EPOLLNONE)
                 continue;

             if (ch_id == sbd_timer_chan) {
                 uint64_t expirations;
                 ssize_t cc = read(chan_sock(ch_id), &expirations,
                                   sizeof(expirations));
                 if (cc < 0) {
                      if (errno != EINTR)
                          LS_ERR("timer read failed");
                      // fall through: still do maintenance
                 }
                 if ((size_t)cc != sizeof(expirations)) {
                     LS_ERR("timer short read: %zd bytes", cc);
                     // fall through: still do maintenance
                 }
                 LS_DEBUG("timer run %s", ctime2(NULL));
                 // Periodic maintenance: legacy job_checking + status_report.
                 // always run
                 sbd_reap_children();
                 job_finish_checking();
                 job_status_checking();
                 job_pipeline_drive();
                 // rest the state
                 channels[ch_id].chan_events = CHAN_EPOLLNONE;
                 continue;
             }

             if (ch_id == sbd_mbd_chan) {
                 sbd_handle_mbd(ch_id);
                 // reset the channel state
                 channels[ch_id].chan_events = CHAN_EPOLLNONE;
                 continue;
             }

         }
    }
}

static int sbd_init(const char *sbatch)
{
    // initenv will load genParams as well
    if (initenv_(lsbParams, NULL) < 0) {
        return -1;
    }

    sbd_init_signals();

    sbd_init_log();

    if (sbd_init_network() < 0) {
        LS_ERR("failed to initialize my network");
        return -1;
    }

    // The appname is useless but the api still wants it
    if (lsb_init("sbd") < 0) {
        LS_ERR("failed to initialize the batch library");
        return -1;
    }

    // global channel to mbd
    sbd_mbd_chan = sbd_connect_mbd();
    if (sbd_mbd_chan < 0) {
        LS_ERR("failed to connect to mbd");
        return -1;
    }

    // initialize the lists and hashes
    if (sbd_job_init() < 0) {
        // Bug handle this
    }

    if (sbd_mbd_register() < 0) {
        LS_ERR("failed to register with mbd");
        return -1;
        sbd_mbd_chan = -1;
        // could almost abort()
    }
    // Green light we can start to operate

    return 0;
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
    sbd_chan = chan_listen_socket(SOCK_STREAM, sbd_port,
                                  SOMAXCONN, CHAN_OP_SOREUSE);
    if (sbd_chan < 0) {
        LS_ERR("Failed to initialize the sbd channel, another sbd running?");
        return -1;
    }

    // init the epoll
    sbd_efd = epoll_create1(EPOLL_CLOEXEC);
    if (sbd_efd < 0) {
        LS_ERR("epoll_create1() failed: %m");
        chan_close(sbd_chan);
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
        chan_close(sbd_chan);
        return -1;
    }

    // sbd main channel
    struct epoll_event ev = {.events = EPOLLIN, .data.u32 = (uint32_t)sbd_chan};
    if (epoll_ctl(sbd_efd, EPOLL_CTL_ADD, chan_sock(sbd_chan), &ev) < 0) {
        LS_ERR("epoll_ctl() failed to add sbd chan");
        free(sbd_events);
        close(sbd_efd);
        chan_close(sbd_chan);
        return -1;
    }

    // for now
    sbd_timer = 30 * 1000;
    // this function is in the channel library.
    sbd_timer_chan = chan_create_timer(sbd_timer);
    if (sbd_timer_chan < 0) {
        free(sbd_events);
        close(sbd_efd);
        chan_close(sbd_chan);
        return -1;
    }

    //sbd timer channel
    ev.events = EPOLLIN;
    ev.data.u32 = sbd_timer_chan;
    if (epoll_ctl(sbd_efd, EPOLL_CTL_ADD, chan_sock(sbd_timer_chan), &ev) < 0) {
        LS_ERR("epoll_ctl() failed to add sbd chan");
        close(sbd_efd);
        chan_close(sbd_chan);
        return -1;
    }

    LS_INFO("sbatchd listening on port %d chan: %d, epoll_fd: %d timer: %dsec",
            sbd_port, sbd_chan, sbd_efd, sbd_timer);

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
            abort();
        }

        // fill the job exit status value from wait
        // into the job structure
        job->exit_status = status;
        job->exit_status_valid = true;

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

        LS_INFO("job %"PRId64 "finished pid=%d jStatus=0x%x status=0x%x",
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

        if (job->state == SBD_JOB_RUNNING &&
            job->spec.startTime > 0) {

            job->spec.runTime = (int)(now - job->spec.startTime);
            if (job->spec.runTime < 0)
                job->spec.runTime = 0;
        }
    }
}

static void job_pipeline_drive(void)
{
    struct ll_list_entry *e;
    time_t now = time(NULL);

    for (e = sbd_job_list.head; e; e = e->next) {
        struct sbd_job *job = (struct sbd_job *)e;

        if (job->missing)
            continue;

        // Stage gate: until MBD has ACKed the NEW_JOB reply (pid/pgid committed),
        // do not send execute/finish. This preserves ordering and avoids "ghost"
        // execute/finish for jobs MBD hasn't recorded yet.
        if (!job->pid_acked)
            continue;

        // Send execute exactly once.
        if (!job->execute_acked) {
            if (sbd_enqueue_execute(sbd_mbd_chan, job) < 0) {
                LS_ERR("job %"PRId64" enqueue execute failed", job->job_id);
                // Your current recovery policy: kill channel so it re-registers.
                // Returning here is enough if caller closes channel on error.
                return;
            }
            job->execute_acked = TRUE;
            LS_INFO("job %"PRId64" execute enqueued", job->job_id);

            // Do not send finish in same tick; preserve ordering for short jobs.
            continue;
        }

        // After execute, send finish once we have exit status.
        if (job->exit_status_valid && !job->finish_acked) {
            if (sbd_enqueue_finish(sbd_mbd_chan, job) < 0) {
                LS_ERR("job %"PRId64" enqueue finish failed", job->job_id);
                return;
            }
            job->finish_acked = TRUE;
            LS_INFO("job %"PRId64" finish enqueued", job->job_id);
        }

        (void)now; // silence unused if you keep 'now' for later extensions
    }
}

static void
job_finish_checking(void)
{
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

        // Send job execute once we have pid ack and we have not emitted execute.
        // This is what causes mbd to log JOB_EXECUTE and store exec metadata.
        if (!job->execute_acked) {
            int rc = sbd_enqueue_execute(sbd_mbd_chan, job);
            if (rc == 0) {
                job->execute_acked = TRUE;
                LS_INFO("job %"PRId64" execute enqueued", job->job_id);
            } else {
                LS_WARNING("job %"PRId64" execute enqueue failed", job->job_id);
            }
            // Do not attempt finish in the same pass unless execute is sent.
            // This preserves event ordering even for very short jobs.
            continue;
        }

        // After execute is sent, we can send finish once we have an exit status.
        if (job->exit_status_valid && !job->finish_acked) {
            int rc = sbd_enqueue_finish(sbd_mbd_chan, job);
            if (rc == 0) {
                job->finish_acked = TRUE;
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
