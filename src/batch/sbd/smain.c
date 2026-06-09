/*
 * Copyright (C) LavaLite Contributors
 * GPL v2
 */

#include <string.h>
#include <errno.h>
#include <assert.h>
#include <syslog.h>
#include <getopt.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include "base/lib/auth.h"
#include "base/lib/ll.conf.h"
#include "base/lib/ll.protocol.h"
#include "base/lib/ll.syslog.h"
#include "base/lib/ll.channel.h"
#include "batch/sbd/sbd.h"

// some vars
int non_root = 0;

// sbd main loop
static void sbd_run_daemon(void);
static int sbd_init(void);
static int sbd_init_network(void);
static int sbd_job_init(void);
static int sbd_init_signals(void);
static void sbd_reap_children(void);
static void sbd_maybe_reap_children(void);
static struct sbd_job *sbd_find_job_by_pid(pid_t);
static void job_status_checking(void);
static void job_new_drive(void);
static void job_finish_drive(void);
static void mbd_reconnect_try(void);
static bool_t sbd_pid_alive(struct sbd_job *);
static void sbd_cleanup(void);
static int opt_op_timer = SBD_OPERATION_TIMER;
static int opt_resend_timer = SBD_RESEND_ACK_TIMEOUT;

char sim_name[MAXHOSTNAMELEN]; /* empty = not in sim mode */
int sim_port = 0;              /* 0 = use LL_SBD_PORT    */

// List and table of all jobs
struct ll_list sbd_job_list;
struct ll_hash *sbd_job_hash;
struct ll_host mbd_node;

static uint16_t sbd_port;
static int sbd_timer;
int sbd_listen_chan = -1;
int sbd_timer_chan = -1;
int sbd_mbd_chan = -1;
int sbd_efd = -1;
pid_t pruner_pid = -1;
static struct epoll_event sbd_events[CHAN_MAX];
static int sbd_resend_timer;

// Handler sets these variables to signal events
static volatile sig_atomic_t sbd_got_sigchld = 0;
static volatile sig_atomic_t sbd_croak = 0;

static int sbd_ll_check_conf(void)
{
    if (ll_conf_param_missing("LL_STATE_DIR", ll_params[LL_STATE_DIR].val)) {
        LL_ERRX("LL_STATE_DIR missing from ll.conf");
        return -1;
    }
    if (ll_conf_param_missing("LL_MBD_PORT", ll_params[LL_MBD_PORT].val)) {
        LL_ERRX("LL_MBD_PORT missing from ll.conf");
        return -1;
    }

    int port;
    if (!ll_atoi(ll_params[LL_MBD_PORT].val, &port)) {
        LL_ERRX("ll_atoi of LL_MBD_PORT=%s failed", ll_params[LL_MBD_PORT].val);
        return -1;
    }
    if (ll_conf_param_missing("LL_MBD_HOST", ll_params[LL_MBD_HOST].val)) {
        LL_ERRX("LL_MBD_HOST missing from ll.conf");
        return -1;
    }

    return 0;
}
// Exit only if the daemon cannot run its main loop.
// Never exit for control-plane/network unavailability.
static int sbd_init(void)
{
    if (ll_init() < 0) {
        LL_ERRX("ll_init failed cannot run");
        return -1;
    }

    // open the log as soon as possible
    int cc;
    if (sim_name[0] != 0) {
        cc = ll_openlog(sim_name, ll_params[LL_LOG_DIR].val,
                        ll_params[LL_LOG_MASK].val);
    } else {
        cc = ll_openlog("sbd", ll_params[LL_LOG_DIR].val,
                        ll_params[LL_LOG_MASK].val);
    }
    if (cc < 0) {
        fprintf(stderr, "sbd: ll_openlog failed lodir=%s mask=%s %m\n",
                ll_params[LL_LOG_DIR].val, ll_params[LL_LOG_MASK].val);
        return -1;
    }
    LL_DEBUG("sbd using LL_CONF_DIR=%s", getenv("LL_CONF_DIR"));

    int auth_age;
    ll_atoi(ll_params[LL_AUTH_MAX_AGE].val, &auth_age);
    if (auth_init(1, auth_age) < 0) {
        LL_ERRX("auth_load_key failed");
        return -1;
    }

    if (sbd_ll_check_conf()) {
        LL_ERRX("sbd_check_conf failed cannot run");
        return -1;
    }

    umask(0077);

    if (sbd_init_signals() < 0) {
        LL_ERRX("sbd_job_init failed cannot run");
        return -1;
    }
    // initialize the lists and hashes
    if (sbd_job_init() < 0) {
        LL_ERRX("sbd_job_init failed cannot run");
        return -1;
    }

    if (sbd_init_network() < 0) {
        LL_ERR("failed to initialize sbd network cannot run");
        return -1;
    }

    // global channel to mbd
    if (sbd_mbd_connect() < 0) {
        LL_ERRX("mbd link: initial connect attempt failed");
    } else {
        sbd_register();
    }

    // Initialize persistent job state storage early.
    // If we can't create/validate the state directory, we cannot guarantee
    // restart-safe job tracking, so fail fast before allocating resources.
    if (sbd_storage_init() < 0) {
        LL_ERR("failed to initialize persistent state storage");
        return -1;
    }

    if (cgroup_init() < 0)
        LL_WARNING("cgroup init failed, resource enforcement disabled");

    // Reconstruct jobs seen before last shutdown.
    if (sbd_job_state_load_all() < 0) {
        LL_ERR("failed to load persistent job state");
        return -1;
    }

    // Green light we can start to operate

    return 0;
}

static void mbd_reconnect_try(void)
{
    static time_t last_try = 0;

    if (sbd_mbd_chan >= 0)
        return;

    time_t t = time(NULL);
    if (last_try > 0 && (t - last_try) < 5)
        return;

    last_try = t;
    LL_ERRX("lost connection with mbd");

    if (sbd_mbd_connect() < 0) {
        LL_ERRX("failed connecting to mbd, retry...");
        return;
    }
    sbd_register();
}

static int sbd_init_network(void)
{
    // create the listening port and the sbd channel
    if (ll_params[LL_SBD_PORT].val == NULL) {
        LL_ERR("LSB_SBD_PORT is not defined");
        return -1;
    }

    if (get_host_by_name(ll_params[LL_MBD_HOST].val, &mbd_node) < 0) {
        LL_ERR("cannot resolve LL_MBD_HOST=%s", ll_params[LL_MBD_HOST].val);
        return -1;
    }

    int t;
    // create the listining port and the sbd channel
    if (!ll_atoi(ll_params[LL_SBD_PORT].val, &t)) {
        LL_ERR("the LSB_SBD_PORT is not defined correcly %s, cannot run",
               ll_params[LL_SBD_PORT].val);
        return -1;
    }
    sbd_port = (sim_port > 0) ? (uint16_t) sim_port : (uint16_t) t;

    chan_init();

    // now open the sbd server channel
    sbd_listen_chan = chan_tcp_server(sbd_port);
    if (sbd_listen_chan < 0) {
        LL_ERR("Failed to initialize the sbd channel, another sbd running?");
        return -1;
    }

    // init the epoll
    sbd_efd = epoll_create1(EPOLL_CLOEXEC);
    if (sbd_efd < 0) {
        LL_ERR("epoll_create1() failed: %m");
        chan_close(sbd_listen_chan);
        return -1;
    }

    struct epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.events = EPOLLIN | EPOLLRDHUP;
    ev.data.u32 = sbd_listen_chan;
    if (epoll_ctl(sbd_efd, EPOLL_CTL_ADD, chan_sock(sbd_listen_chan), &ev) <
        0) {
        LL_ERR("epoll_ctl add chan_mbd=%d failed", sbd_listen_chan);
        chan_close(sbd_listen_chan);
        sbd_listen_chan = -1;
        close(sbd_efd);
        sbd_efd = -1;
        return -1;
    }

    LL_INFO("chan_epoll_register sbd_efd=%d", sbd_efd);

    sbd_timer = opt_op_timer;
    // this function is in the channel library.
    sbd_timer_chan = chan_create_timer(sbd_timer);
    if (sbd_timer_chan < 0) {
        close(sbd_efd);
        chan_close(sbd_listen_chan);
        return -1;
    }

    // timout is in second
    sbd_resend_timer = opt_resend_timer;
    LL_INFO("sbd_resend_timer set to %d secs", sbd_resend_timer);

    // sbd timer channel
    ev.events = EPOLLIN;
    ev.data.u32 = sbd_timer_chan;
    if (epoll_ctl(sbd_efd, EPOLL_CTL_ADD, chan_sock(sbd_timer_chan), &ev) < 0) {
        LL_ERR("epoll_ctl() failed to add sbd chan");
        close(sbd_efd);
        chan_close(sbd_listen_chan);
        return -1;
    }

    LL_INFO("sbd listening on port=%d sbd_listen_chan=%d, epoll_fd=%d "
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
    sbd_job_hash = ll_hash_create(0); // 0 → default (e.g. 11 buckets)
    if (!sbd_job_hash) {
        LL_ERR("failed to create job hash table");
        return -1;
    }

    return 0;
}

static void sbd_croak_handler(int sig)
{
    (void) sig;
    sbd_croak = 1;
}

static void sbd_child_handler(int sig)
{
    (void) sig;
    sbd_got_sigchld = 1;
}

static int sbd_init_signals(void)
{
    LL_INFO("initializing signals");
    if (install_signal_handler(SIGTERM, sbd_croak_handler, 0) < 0) {
        LL_ERR("failed to install SIGTERM handler");
        return -1;
    }
    if (install_signal_handler(SIGINT, sbd_croak_handler, 0) < 0) {
        LL_ERR("failed to install SIGINT handler");
        return -1;
    }
    if (install_signal_handler(SIGCHLD, sbd_child_handler, 0) < 0) {
        LL_ERR("failed to install SIGCHLD handler");
        return -1;
    }

    return 0;
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
                    LL_ERR("waitpid(-1) failed");
            }
            break;
        }

        if (pid == pruner_pid) {
            pruner_pid = -1;
            continue;
        }

        struct sbd_job *job = sbd_find_job_by_pid(pid);
        if (job == NULL) {
            LL_WARNING("reaped unknown child pid=%d status=0x%x", (int) pid,
                       (unsigned) status);
            continue;
        }

        // this should impossible
        if (job->pid != pid) {
            LL_ERR("job=%ld pid mismatch: job->pid=%d waitpid=%d", job->job_id,
                   (int) job->pid, (int) pid);
            assert(0);
            continue;
        }

        // fill the job exit status value from wait
        // into the job structure
        job->exit_status = status;
        job->exit_status_valid = true;
        job->end_time = time(NULL);

        LL_INFO("job=%ld reaped pid=%d exit_status=0x%x", job->job_id,
                (int) pid, job->exit_status);
    }
}

static void sbd_maybe_reap_children(void)
{
    if (!sbd_got_sigchld)
        return;
    sbd_got_sigchld = 0;
    sbd_reap_children();
}

static void job_terminate_check(struct sbd_job *job)
{
    if (job->terminate_time <= 0)
        return;
    if (job->exit_status_valid)
        return;

    time_t now = time(NULL);
    if (now >= job->terminate_time
        && job->terminate_kill_time == 0) {
        LL_INFO("job=%ld terminate_time reached, sending SIGUSR2", job->job_id);
        killpg(job->pgid, SIGUSR2);
        job->terminate_time = 0;  /* arm SIGKILL after grace */
        job->terminate_kill_time = now + 60;
        return;
    }

    if (job->terminate_kill_time > 0 && now >= job->terminate_kill_time) {
        LL_INFO("job=%ld grace expired, sending SIGKILL", job->job_id);
        cgroup_job_kill(job->job_id);
        job->terminate_kill_time = 0;
    }
}

static void job_status_checking(void)
{
    struct ll_list_entry *e;

    // NOTE: ll_list_entry must remain the first field (list base object idiom)
    for (e = sbd_job_list.head; e; e = e->next) {
        struct sbd_job *job = (struct sbd_job *) e;

        if (job->exit_status_valid)
            continue;

        if (job->finish_acked > 0) {
            LL_ERRX("job=%ld finish_acked still on list (bug)", job->job_id);
            assert(0);
            continue;
        }

        if (!job->pid_acked)
            continue;

        job_terminate_check(job);

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
            job->exit_status_valid = true;
            job->exit_status = 1;
            LL_ERR("job=%ld sbd_read_exit_status_file %d time declare exited "
                   "exit_status_valid=%d exit_stats=0x%x",
                   job->job_id, job->retry_exit_count, job->exit_status_valid,
                   job->exit_status);
            continue;
        }

        job->finish_last_send = 0;
        job->exit_status_valid = true;
        job->exit_status = exit_code;
        LL_ERRX("job=%ld read exit file exit_status_valid=%d exit_stats=0x%x",
                job->job_id, job->exit_status_valid, job->exit_status);
    }
}

static void job_new_drive(void)
{
    static time_t last_time;

    // Check it we are connected to mbd
    if (!sbd_mbd_link_ready()) {
        time_t t = time(NULL);
        if (t - last_time >= 60) {
            LL_INFO("mbd link not ready, skip and sbd_mbd_reconnect_try");
            last_time = t;
        }
        return;
    }

    time_t now = time(NULL);
    int resend_sec = sbd_timer * sbd_resend_timer;
    struct ll_list_entry *e;

    for (e = sbd_job_list.head; e; e = e->next) {
        struct sbd_job *job = (struct sbd_job *) e;

        if (job->pid_acked)
            continue;

        // retry sending the job reply
        if ((now - job->reply_last_send) < resend_sec) {
            continue;
        }

        if (sbd_job_new_reply(job) < 0) {
            LL_ERR("job=%ld sbd_job_new_reply failed", job->job_id);
            continue;
        }

        LL_INFO("job=%ld pid=%d", job->job_id, job->pid);

        job->reply_last_send = now;
    }
}

static void job_finish_drive(void)
{
    // Check it we are connected to mbd
    if (!sbd_mbd_link_ready()) {
        return;
    }

    int resend_sec = sbd_timer * sbd_resend_timer;
    time_t now = time(NULL);
    struct ll_list_entry *e;

    for (e = sbd_job_list.head; e; e = e->next) {
        struct sbd_job *job = (struct sbd_job *) e;

        // pid_sent is the hard gate: mbd must have recorded pid/pgid first.
        if (!job->pid_acked)
            continue;

        if (!job->exit_status_valid)
            continue;

        if (job->finish_acked) {
            LL_ERRX("job=%ld finish_acked still on list (bug)", job->job_id);
            assert(0);
        }

        if ((now - job->finish_last_send) < resend_sec)
            continue;

        int cc = sbd_job_finish(job);
        if (cc < 0) {
            LL_WARNING("job=%ld finish enqueue failed", job->job_id);
            continue;
        }

        LL_INFO("job=%ld BATCH_JOB_FINISH enqueued", job->job_id);

        job->finish_last_send = now;
    }
}

// We reaped a child find to which sbd_job does belong
static struct sbd_job *sbd_find_job_by_pid(pid_t pid)
{
    struct ll_list_entry *e;

    for (e = sbd_job_list.head; e; e = e->next) {
        struct sbd_job *job = (struct sbd_job *) e;
        if (job->pid == pid)
            return job;
    }
    return NULL;
}

static bool_t sbd_pid_alive(struct sbd_job *job)
{
    pid_t target;

    if (job->pgid > 1)
        target = -job->pgid; // probe process group
    else if (job->pid > 1)
        target = job->pid; // fallback
    else
        return true; // unknown -> do not claim dead

    if (kill(target, 0) == 0)
        return true;

    int err = errno;
    LL_DEBUG("job=%ld pid=%d pgid=%d target=%d errno=%d %s", job->job_id,
             job->pid, job->pgid, target, err, strerror(err));

    if (err == ESRCH)
        return false;

    if (err == EPERM)
        return true;

    return true; // not proof of death
}

// sbd_init() failed so as a good coding hygiene we clean up file
// descriptors and buffers
static void sbd_cleanup(void)
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
        struct sbd_job *job = (struct sbd_job *) e;
        free(job);
    }
    ll_list_init(&sbd_job_list);
    ll_closelog();
}

void sbd_fatal(enum sbd_fatal_cause cause)
{
    switch (cause) {
    case SBD_FATAL_STORAGE:
        // Disk / FS / permission / ENOSPC / IO error.
        // We cannot guarantee restart-safe semantics anymore.
        LL_ERRX("FATAL: storage durability failure; refusing to continue");
        break;
    case SBD_FATAL_INVARIANT:
        LL_ERRX("FATAL: internal invariant violated");
        break;
    case SBD_FATAL_PROTO:
        LL_ERRX("FATAL: protocol violation");
        break;
    case SBD_FATAL_OOM:
        LL_ERRX("FATAL: out of memory");
        break;
    case SBD_FATAL_ENQUEUE:
        LL_ERRX("FATAL: failed enqueue message to mbd");
        break;
    default:
        LL_ERRX("FATAL: unknown cause=%d", (int) cause);
        break;
    }

    // Optional: best-effort close mbd channel so logs are clear.
    sbd_cleanup();

    // Fail-fast: let systemd restart; avoids half-working daemon.
    _exit(1);
}

static void sbd_run_daemon(void)
{
    job_status_checking();

    LL_INFO("sbd enter main loop");

    while (1) {
        if (sbd_croak) {
            break;
        }

        // We pass -1 as the timer channel will ring
        int nready = chan_epoll(sbd_efd, sbd_events, CHAN_MAX, -1);
        // save the epoll errno as the coming reap children can change it
        int epoll_errno = errno;

        // In case SIGCHLD arrived while we were blocked in epoll.
        sbd_maybe_reap_children();

        if (nready < 0) {
            if (epoll_errno == EINTR)
                continue;
            errno = epoll_errno;
            LL_ERR("network I/O");
            sleep(1);
            continue;
        }

        for (int i = 0; i < nready; i++) {
            struct epoll_event *ev = &sbd_events[i];
            int ch_id = (int) ev->data.u32;

            if (ch_id == sbd_timer_chan) {
                uint64_t expirations;
                ssize_t cc =
                    read(chan_sock(ch_id), &expirations, sizeof(expirations));
                if (cc < 0) {
                    if (errno == EINTR) {
                        LL_ERR("timer interrepted, do maintainance");
                        goto timer_maintenance;
                    }
                    LL_ERR("timer read failed, do maintenance");
                    goto timer_maintenance;
                    // fall through: still do maintenance
                }
                if ((size_t) cc != sizeof(expirations)) {
                    LL_ERR("timer short read: %zd bytes", cc);
                    // fall through: still do maintenance
                }
            timer_maintenance:
                sbd_reap_children();
                mbd_reconnect_try();
                job_new_drive();
                job_finish_drive();
                job_status_checking();
                sbd_prune_jobs_try();
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
                sbd_mbd_route(ch_id);
                // reset the channel state
                channels[ch_id].chan_events = CHAN_EPOLLNONE;
                continue;
            }
        }
    }

    LL_INFO("terminate requested, exiting");
    chan_close(sbd_listen_chan);
    chan_close(sbd_timer_chan);
    chan_close(sbd_mbd_chan);
}

static int parse_simulator(const char *arg)
{
    char tmp[MAXHOSTNAMELEN];
    char *colon;
    int port;

    ll_strlcpy(tmp, arg, sizeof(tmp));

    colon = strchr(tmp, ':');
    if (colon == NULL) {
        fprintf(stderr, "sbd: --simulator: expected name:port\n");
        return -1;
    }
    *colon = 0;

    if (!ll_atoi(colon + 1, &port) || port < 1 || port > 65535) {
        fprintf(stderr, "sbd: --simulator: invalid port '%s'\n", colon + 1);
        return -1;
    }

    ll_strlcpy(sim_name, tmp, sizeof(sim_name));
    sim_port = port;
    return 0;
}

static struct option long_options[] = {
    {"non_root", no_argument, 0, 'n'},
    {"confdir", required_argument, 0, 'c'},
    {"version", no_argument, 0, 'V'},
    {"simulator", required_argument, 0, 's'},
    {"op_timer", required_argument, 0, 'o'},
    {"resend_timer", required_argument, 0, 'r'},
    {"help", no_argument, 0, 'h'},
    {0, 0, 0, 0}};

static void usage(void)
{
    fprintf(
        stderr,
        "sbd: [OPTIONS]\n"
        " -n, --non_root          Run in non-root mode\n"
        " -V, --version           Print version and exit\n"
        " -c, --confdir dir       Set LL_CONF_DIR\n"
        " -s, --simulator name:port  Sim mode: register as name on port\n"
        " -o, --op_timer N        Operation timer seconds (default: %d)\n"
        " -r, --resend_timer N    Resend ACK timeout seconds (default: %d)\n"
        " -h, --help              Show this help\n",
        SBD_OPERATION_TIMER, SBD_RESEND_ACK_TIMEOUT);
}

int main(int argc, char **argv)
{
    int cc;
    char *conf_dir = NULL;

    sim_name[0] = 0;
    while ((cc = getopt_long(argc, argv, "nc:Vs:o:r:h", long_options, NULL)) !=
           EOF) {
        switch (cc) {
        case 'n':
            non_root = 1;
            break;
        case 'c':
            conf_dir = optarg;
            setenv("LL_CONF_DIR", conf_dir, 1);
            break;
        case 'V':
            fprintf(stderr, "%s\n", LAVALITE_VERSION_STR);
            return 0;
        case 's':
            if (parse_simulator(optarg) < 0)
                return -1;
            break;
        case 'o':
            if (!ll_atoi(optarg, &opt_op_timer) || opt_op_timer < 1) {
                fprintf(stderr, "sbd: invalid op_timer=%s\n", optarg);
                return -1;
            }
            break;
        case 'r':
            if (!ll_atoi(optarg, &opt_resend_timer) || opt_resend_timer < 1) {
                fprintf(stderr, "sbd: invalid resend_timer=%s\n", optarg);
                return -1;
            }
            break;
        default:
            usage();
            return -1;
        }
    }

    if (conf_dir == NULL) {
        if ((conf_dir = getenv("LL_CONF_DIR")) == NULL) {
            fprintf(stderr, "sbd: LL_CONF_DIR must be defined, cannot run\n");
            return -1;
        }
    }

    if (sim_name[0] != 0 && non_root) {
        fprintf(stderr, "sbd: --simulator and -n are mutually exclusive\n");
        exit(1);
    }

    if (!non_root && geteuid() != 0) {
        fprintf(stderr, "Only root wants to run sbd");
        return -1;
    }

    int rc = sbd_init();
    if (rc < 0) {
        LL_ERRX(
            "sbd: fatal error during initialization, see previous messages");
        return -1;
    }

    char name[MAXHOSTNAMELEN];
    gethostname(name, MAXHOSTNAMELEN);
    LL_INFO("sbd uid=%d starting on host=%s", getuid(), name);

    // Run sbd run
    sbd_run_daemon();

    return 0;
}
