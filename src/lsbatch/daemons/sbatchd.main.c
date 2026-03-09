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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA  02110-1301, USA
 */

#include "lsbatch/daemons/sbd.h"

// some vars
int sbd_debug;

// sbd main loop
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
static void job_new_drive(void);
static void job_finish_drive(void);
static void job_execute_drive(void);
static void mbd_reconnect_try(void);
static bool_t sbd_pid_alive(struct sbd_job *);
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
pid_t pruner_pid = -1;
static struct epoll_event *sbd_events;
static int max_events;
static int sbd_resend_timer;

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
        "  -d, --debug Run in foreground\n"
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

    if (sbd_debug == false
        && geteuid() != 0) {
        openlog("sbd", LOG_CONS |LOG_NOWAIT|LOG_PERROR| LOG_PID, LOG_AUTH);
        syslog(LOG_ERR, "Only root wants to run sbd.");
        closelog();
        return -1;
    }

    if (!getenv("LSF_ENVDIR")) {
        fprintf(stderr, "sbd: LSF_ENVDIR must be defined\n");
        return -1;
    }

    int rc = sbd_init("sbd");
    if (rc < 0) {
        LS_ERR("sbd: fatal error during initialization - see previous messages");
        return -1;
    }

    // Run sbd run
    sbd_run_daemon();

    return 0;
}
// Exit only if the daemon cannot run its main loop.
// Never exit for control-plane/network unavailability.
static int sbd_init(const char *sbatch)
{
    umask(0077);

    // initenv will load genParams as well
    if (initenv_(lsbParams, NULL) < 0) {
        return -1;
    }

    // first thing first open the log so we can see messages
    // from the daemon right from the start
    sbd_init_log();

    if (check_sharedir_access(lsbParams[LSB_SHAREDIR].paramValue) < 0) {
        LS_ERR("cannot access working directory %s",
               lsbParams[LSB_SHAREDIR].paramValue);
        return -1;
    }

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

    // global channel to mbd
    sbd_mbd_chan = sbd_mbd_connect();
    if (sbd_mbd_chan < 0) {
        LS_ERR("mbd link: initial connect attempt failed");
        sbd_mbd_chan = -1;
    } else {
        if (sbd_register(sbd_mbd_chan) < 0) {
            LS_ERR("mbd link: register failed at init");
            sbd_mbd_link_down();
        }
    }

    // initialize the lists and hashes
    if (sbd_job_init() < 0) {
        LS_ERRX("sbd_job_init failed");
        return -1;
    }

    // Initialize persistent job state storage early.
    // If we can't create/validate the state directory, we cannot guarantee
    // restart-safe job tracking, so fail fast before allocating resources.
    if (sbd_storage_init() < 0) {
        LS_ERR("failed to initialize persistent state storage");
        return -1;
    }
    // Reconstruct jobs seen before last shutdown.
    if (sbd_job_state_load_all() < 0) {
        LS_ERR("failed to load persistent job state");
        return -1;
    }

    // Green light we can start to operate

    return 0;
}

static void
sbd_run_daemon(void)
{
    job_status_checking();

    LS_INFO("sbd enter main loop");

    while (1) {

        if (sbd_croak) {
            break;
        }

        // We pass -1 as the timer channel will ring
        int nready = chan_epoll(sbd_efd, sbd_events, max_events, -1);
        // save the epoll errno as the coming reap children can change it
        int epoll_errno = errno;

        // In case SIGCHLD arrived while we were blocked in epoll.
        sbd_maybe_reap_children();

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
             timer_maintenance:
                 sbd_reap_children();
                 mbd_reconnect_try();
                 job_new_drive();
                 job_execute_drive();
                 job_finish_drive();
                 job_status_checking();
                 sbd_prune_archive_try();
                 // rest the state
                 channels[ch_id].chan_events = CHAN_EPOLLNONE;
                 continue;
             }

             // True skip partially read channels
             if (channels[ch_id].chan_events == CHAN_EPOLLNONE)
                 continue;

             // There is an event on the permament channel
             // connection with mbd
             if (ch_id == sbd_mbd_chan) {
                 sbd_mbd_handle(ch_id);
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

    LS_INFO("terminate requested, exiting");
    chan_close(sbd_listen_chan);
    chan_close(sbd_timer_chan);
    chan_close(sbd_mbd_chan);
}

static void
mbd_reconnect_try(void)
{
    static time_t last_try = 0;

    if (sbd_mbd_chan >= 0)
        return;

    time_t t = time(NULL);
    if (last_try > 0 && (t - last_try) < 5)
        return;

    last_try = t;
    LS_ERRX("lost connection with mbd");

    sbd_mbd_chan = sbd_mbd_connect();
    if (sbd_mbd_chan < 0) {
        LS_ERR("timeout connecting to mbd, retry...");
        return;
    }
    sbd_register(sbd_mbd_chan);
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
        ls_openlog("sbd", log_dir, true, 0, log_mask);
        LS_INFO("Starting sbd in debug mode");
    } else {
        // Normal production daemon case
        ls_openlog("sbd", log_dir, false, 0, log_mask);
    }

    ls_setlogtag("");

    LS_INFO("sbd logging initialized: dir=%s mask=%s debug=%d",
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

    int t;
    // create the listining port and the sbd channel
    if (! ll_atoi(genParams[LSB_SBD_PORT].paramValue, &t)) {
        LS_ERR("the LSB_SBD_PORT is not defined correcly %s, cannot run",
               genParams[LSB_SBD_PORT].paramValue);
        return -1;
    }
    sbd_port = (uint16_t)t;

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
    chan_epoll_register(sbd_efd);
    LS_INFO("chan_epoll_register sbd_efd=%d", sbd_efd);

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
    struct epoll_event ev = {.events = EPOLLIN,
        .data.u32 =(uint32_t)sbd_listen_chan};

    if (epoll_ctl(sbd_efd, EPOLL_CTL_ADD, chan_sock(sbd_listen_chan), &ev) < 0) {
        LS_ERR("epoll_ctl() failed to add sbd chan");
        free(sbd_events);
        close(sbd_efd);
        chan_close(sbd_listen_chan);
        return -1;
    }

    // for now
    t = 0;
    sbd_timer = SBD_OPERATION_TIMER; // seconds
    // this function is in the channel library.
    sbd_timer_chan = chan_create_timer(sbd_timer);
    if (sbd_timer_chan < 0) {
        free(sbd_events);
        close(sbd_efd);
        chan_close(sbd_listen_chan);
        return -1;
    }

    // timout is in second
    // Bug do: LSB_OPERATION_TIMER
    sbd_resend_timer = SBD_RESEND_ACK_TIMEOUT;
    LS_INFO("sbd_resend_timer set to %d secs", sbd_resend_timer);

    //sbd timer channel
    ev.events = EPOLLIN;
    ev.data.u32 = sbd_timer_chan;
    if (epoll_ctl(sbd_efd, EPOLL_CTL_ADD, chan_sock(sbd_timer_chan), &ev) < 0) {
        LS_ERR("epoll_ctl() failed to add sbd chan");
        close(sbd_efd);
        chan_close(sbd_listen_chan);
        return -1;
    }

    LS_INFO("sbd listening on port=%d sbd_listen_chan=%d, epoll_fd=%d "
            "sbd_timer_chan=%d timer=%dsec sbd_resend_timer=%d",
            sbd_port, sbd_listen_chan, sbd_efd, sbd_timer_chan, sbd_timer,
            sbd_resend_timer);

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

        if (pid == pruner_pid) {
            pruner_pid = -1;
            continue;
        }

        struct sbd_job *job = sbd_find_job_by_pid(pid);
        if (job == NULL) {
            LS_WARNING("reaped unknown child pid=%d status=0x%x",
                       (int)pid, (unsigned)status);
            continue;
        }

        // this should impossible
        if (job->pid != pid) {
            LS_ERR("job=%ld pid mismatch: job->pid=%d waitpid=%d",
                   job->job_id, (int)job->pid, (int)pid);
            assert(0);
            continue;
        }

        // fill the job exit status value from wait
        // into the job structure
        job->exit_status = status;
        job->exit_status_valid = true;
        job->end_time = time(NULL);

        LS_INFO("job=%ld reaped pid=%d exit_status=0x%x",
                job->job_id, (int)pid, job->exit_status);
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

        if (job->exit_status_valid)
            continue;

        if (job->finish_acked > 0)
            continue;

        if (sbd_pid_alive(job))
            continue;

        int exit_code;
        time_t done_time;
        int cc = sbd_read_exit_status_file(job, &exit_code, &done_time);
        if (cc < 0) {

            job->retry_exit_count++;

            if (job->retry_exit_count < 3)
                continue;

            job->finish_last_send = 0;
            job->time_finish_acked = now;
            job->exit_status_valid = true;
            job->exit_status = 1;
            LS_ERR("job=%ld sbd_read_exit_status_file %d time declare exited "
                   "exit_status_valid=%d exit_stats=0x%x", job->job_id,
                   job->retry_exit_count, job->exit_status_valid,
                   job->exit_status);
            continue;
        }

        job->finish_last_send = 0;
        job->time_finish_acked = now;
        job->exit_status_valid = true;
        job->exit_status = exit_code;
        LS_ERRX("job=%ld read exit file exit_status_valid=%d exit_stats=0x%x",
                job->job_id, job->exit_status_valid, job->exit_status);
    }
}

static void job_new_drive(void)
{
    static time_t last_time;

    // Check it we are connected to mbd
    if (! sbd_mbd_link_ready()) {
        time_t t = time(NULL);
        if (t - last_time >= SECS_PER_MIN) {
            LS_INFO("mbd link not ready, skip and sbd_mbd_reconnect_try");
            last_time = t;
        }
        return;
    }

    time_t now = time(NULL);
    int resend_sec = sbd_timer * sbd_resend_timer;
    struct ll_list_entry *e;

    for (e = sbd_job_list.head; e; e = e->next) {
        struct sbd_job *job = (struct sbd_job *)e;

        if (job->pid_acked)
            continue;

        // retry sending the job reply
        if ((now - job->reply_last_send) < resend_sec) {
            continue;
        }

        struct jobReply reply;
        memset(&reply, 0, sizeof(struct jobReply));
        reply.jobId   = job->job_id;
        reply.jobPid  = job->pid;
        reply.jobPGid = job->pgid;
        reply.jStatus = job->specs.jStatus = JOB_STAT_RUN;

        if (sbd_job_new_reply(sbd_mbd_chan, &reply) < 0) {
            LS_ERR("job=%ld sbd_job_new_reply enqueue failed", job->job_id);
            continue;
        }
        LS_INFO("job=%ld pid=%d", job->job_id, job->pid);

        job->reply_last_send = now;
    }

}

static void job_execute_drive(void)
{
    // Check it we are connected to mbd
    if (! sbd_mbd_link_ready()) {
        return;
    }

    int resend_sec = sbd_timer * sbd_resend_timer;
    time_t now = time(NULL);
    struct ll_list_entry *e;
    for (e = sbd_job_list.head; e; e = e->next) {
        struct sbd_job *job = (struct sbd_job *)e;

        if (!job->pid_acked) {
            LS_DEBUG("job=%ld not ready: pid not acked yet", job->job_id);
            continue;
        }

        if (job->execute_acked)
            continue;

        if ((now - job->execute_last_send) < resend_sec) {
            continue;
        }

        if (sbd_job_execute(sbd_mbd_chan, job) < 0) {
            LS_ERR("job=%ld enqueue BATCH_JOB_EXECUTE failed", job->job_id);
            continue;
        }

        LS_INFO("job=%ld BATCH_JOB_EXECUTE enqueued", job->job_id);

        // after persist
        job->execute_last_send = now;
    }
}

static void job_finish_drive(void)
{
    // Check it we are connected to mbd
    if (! sbd_mbd_link_ready()) {
        return;
    }

    int resend_sec = sbd_timer * sbd_resend_timer;
    time_t now = time(NULL);
    struct ll_list_entry *e;

    for (e = sbd_job_list.head; e; e = e->next) {
        struct sbd_job *job = (struct sbd_job *)e;

        // pid_sent is the hard gate: mbd must have recorded pid/pgid first.
        if (!job->pid_acked)
            continue;

        if (!job->execute_acked)
            continue;

        if (! job->exit_status_valid)
            continue;

        if (job->finish_acked)
            continue;

        if ((now - job->finish_last_send) < resend_sec)
            continue;

        int cc = sbd_job_finish(sbd_mbd_chan, job);
        if (cc < 0) {
            LS_WARNING("job=%ld finish enqueue failed", job->job_id);
            continue;
        }

        LS_INFO("job=%ld BATCH_JOB_FINISH enqueued", job->job_id);

        job->finish_last_send = now;
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

static bool_t
sbd_pid_alive(struct sbd_job *job)
{
    pid_t target;

    if (job->pgid > 1)
        target = -job->pgid;     // probe process group
    else if (job->pid > 1)
        target = job->pid;       // fallback
    else
        return true;             // unknown -> do not claim dead

    if (kill(target, 0) == 0)
        return true;

    int err = errno;
    LS_DEBUG("job=%ld pid=%d pgid=%d target=%d errno=%d %s",
             job->job_id, job->pid, job->pgid, target,
             err, strerror(err));

    if (err == ESRCH)
        return false;

    if (err == EPERM)
        return true;

    return true;                 // not proof of death
}

// sbd_init() failed so as a good coding hygiene we clean up file
// descriptors and buffers
static void
sbd_cleanup(void)
{
    chan_close(sbd_listen_chan);
    chan_close(sbd_timer_chan);
    close(sbd_efd);
    // free the hash but not the job entries
    ll_hash_free(sbd_job_hash, NULL);
    // use clear so that we dont free the pointer that
    // is in the static area and not on the heap
    struct ll_list_entry *e;
    while ((e = ll_list_pop(&sbd_job_list))) {
        struct sbd_job *job = (struct sbd_job *)e;
        char keybuf[LL_BUFSIZ_32];
        snprintf(keybuf, sizeof(keybuf), "%ld", job->job_id);
        ll_hash_remove(sbd_job_hash, keybuf);
        ll_list_remove(&sbd_job_list, &job->list);
        free_job_specs(&job->specs);
        free(job);
    }
    ll_list_init(&sbd_job_list);
    ls_closelog();
}

void sbd_fatal(enum sbd_fatal_cause cause)
{
    switch (cause) {
    case SBD_FATAL_STORAGE:
        // Disk / FS / permission / ENOSPC / IO error.
        // We cannot guarantee restart-safe semantics anymore.
        LS_ERRX("FATAL: storage durability failure; refusing to continue");
        break;

    case SBD_FATAL_INVARIANT:
        LS_ERRX("FATAL: internal invariant violated");
        break;

    case SBD_FATAL_PROTO:
        LS_ERRX("FATAL: protocol violation");
        break;

    case SBD_FATAL_OOM:
        LS_ERRX("FATAL: out of memory");
        break;
    case SBD_FATAL_ENQUEUE:
        LS_ERRX("FATAL: failed enqueue message to mbd");
        break;

    default:
        LS_ERRX("FATAL: unknown cause=%d", (int)cause);
        break;
    }

    // Optional: best-effort close MBD channel so logs are clear.
    sbd_cleanup();

    // Fail-fast: let systemd restart; avoids half-working daemon.
    _exit(1);
}
