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
static struct sbd_job *sbd_job_find_by_pid(pid_t);
static void job_status_checking(void);
static void job_status_report(void);
static void job_finish_checking(void);

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
static volatile sig_atomic_t sbd_terminate = 0;

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

        if (sbd_terminate) {
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

             // True skip partually read channels
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
                 job_status_report();
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

static void terminate_handler(int sig)
{
    (void)sig;
    sbd_terminate = 1;
}

static void child_handler(int sig)
{
    (void)sig;
    sbd_got_sigchld = 1;
}

static void sbd_init_signals(void)
{
    LS_INFO("initializing signals");
    signal_set(SIGTERM, terminate_handler);
    signal_set(SIGINT, terminate_handler);
    signal_set(SIGCHLD, child_handler);
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

        struct sbd_job *job = sbd_job_find_by_pid(pid);
        if (job == NULL) {
            LS_WARNING("reaped unknown child pid=%d status=0x%x",
                       (int)pid, (unsigned)status);
            continue;
        }

        job->exit_status = status;
        job->exit_status_valid = TRUE;

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

        if (job->not_reported == 0)
            job->not_reported = 1;

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

static void job_status_report(void)
{
    time_t now = time(NULL);
    struct ll_list_entry *e;

    // NOTE: ll_list_entry must remain the first field (list base object idiom)
    for (e = sbd_job_list.head; e; e = e->next) {
        struct sbd_job *job = (struct sbd_job *)e;

        if (job->missing)
            continue;

        if (job->not_reported <= 0)
            continue;

        if (job->spec.startTime > 0 &&
            now < job->spec.startTime + 10)
            continue;

        /*
         * Try resend status snapshot.
         * Skeleton for now.
         *
         * if (sbd_send_status(job) == 0) {
         *     job->not_reported = 0;
         *     job->last_status_mbd_time = now;
         * } else {
         *     job->not_reported++;
         * }
         */
    }
}

static void job_finish_checking(void)
{
    struct ll_list_entry *e, *next;

    for (e = sbd_job_list.head; e; e = next) {
        next = e->next;
        struct sbd_job *job = (struct sbd_job *)e;

        if (!job->exit_status_valid)
            continue;

        /*
         * Try to report finish to mbd.
         * Skeleton for now.
         *
         * if (sbd_report_job_finish(job) == 0) {
         *     sbd_job_remove(job);
         *     sbd_job_free(job);
         * }
         */
    }
}

// We reaped a child find to which sbd_job does belong
static struct sbd_job *sbd_job_find_by_pid(pid_t pid)
{
    struct ll_list_entry *e;

    for (e = sbd_job_list.head; e; e = e->next) {
        struct sbd_job *job = (struct sbd_job *)e;
        if (job->pid == pid)
            return job;
    }
    return NULL;
}
