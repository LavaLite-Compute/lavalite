/*
 * Copyright (C) LavaLite Contributors
 * GPL v2
 */
#include "config.h"
#include <string.h>
#include <errno.h>
#include <getopt.h>
#include <unistd.h>
#include <sys/stat.h>

#include "base/lib/auth.h"
#include "base/lib/ll.conf.h"
#include "base/lib/ll.protocol.h"
#include "base/lib/ll.syslog.h"
#include "base/lib/ll.channel.h"
#include "base/lib/ll.sys.h"
#include "batch/service_proxy/service_proxy.h"

static void sp_run_daemon(void);
static int sp_init(void);
static int sp_init_network(void);
static void mbd_reconnect_try(void);
static void sp_cleanup(void);

char sim_name[MAXHOSTNAMELEN]; /* unused for now, kept for symmetry with sbd */
struct ll_host mbd_node;
struct ll_list sp_instance_list;

int sp_efd = -1;
int sp_mbd_chan = -1;
int sp_timer_chan = -1;
static struct epoll_event sp_events[CHAN_MAX];
static volatile sig_atomic_t sp_croak = 0;

static int sp_ll_check_conf(void)
{
    if (ll_conf_param_missing("LL_MBD_PORT", ll_params[LL_MBD_PORT].val)) {
        LL_ERRX("LL_MBD_PORT missing from ll.conf");
        return -1;
    }
    if (ll_conf_param_missing("LL_MBD_HOST", ll_params[LL_MBD_HOST].val)) {
        LL_ERRX("LL_MBD_HOST missing from ll.conf");
        return -1;
    }
    return 0;
}

// Exit only if the daemon cannot run its main loop.
// Never exit for control-plane/network unavailability -- mirrors sbd_init().
static int sp_init(void)
{
    reset_signals();
    install_signal_handler(SIGPIPE, SIG_IGN, 0);

    if (ll_init() < 0) {
        LL_ERRX("ll_init failed cannot run");
        return -1;
    }

    if (ll_openlog("spd", ll_params[LL_LOG_DIR].val,
                   ll_params[LL_LOG_MASK].val) < 0) {
        fprintf(stderr, "spd: ll_openlog failed lodir=%s mask=%s %m\n",
                ll_params[LL_LOG_DIR].val, ll_params[LL_LOG_MASK].val);
        return -1;
    }
    LL_DEBUG("spd using LL_CONF_DIR=%s", getenv("LL_CONF_DIR"));

    int auth_age;
    ll_atoi(ll_params[LL_AUTH_MAX_AGE].val, &auth_age);
    if (auth_init(1, auth_age) < 0) {
        LL_ERRX("auth_load_key failed");
        return -1;
    }

    if (sp_ll_check_conf() < 0) {
        LL_ERRX("sp_ll_check_conf failed cannot run");
        return -1;
    }

    umask(0077);

    if (sp_init_network() < 0) {
        LL_ERR("failed to initialize spd network cannot run");
        return -1;
    }

    // global channel to mbd
    if (sp_mbd_connect() < 0) {
        LL_ERRX("mbd link: initial connect attempt failed");
    } else {
        sp_register();
    }

    // Green light, we can start to operate

    return 0;
}

static int sp_init_network(void)
{
    if (get_host_by_name(ll_params[LL_MBD_HOST].val, &mbd_node) < 0) {
        LL_ERR("cannot resolve LL_MBD_HOST=%s", ll_params[LL_MBD_HOST].val);
        return -1;
    }

    chan_init();
    ll_list_init(&sp_instance_list);

    sp_efd = epoll_create1(EPOLL_CLOEXEC);
    if (sp_efd < 0) {
        LL_ERR("epoll_create1() failed: %m");
        return -1;
    }

    sp_timer_chan = chan_create_timer(SP_OPERATION_TIMER);
    if (sp_timer_chan < 0) {
        close(sp_efd);
        sp_efd = -1;
        return -1;
    }

    struct epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.events = EPOLLIN;
    ev.data.u32 = sp_timer_chan;
    if (epoll_ctl(sp_efd, EPOLL_CTL_ADD, chan_sock(sp_timer_chan), &ev) < 0) {
        LL_ERR("epoll_ctl() failed to add sp timer chan");
        close(sp_efd);
        sp_efd = -1;
        return -1;
    }

    LL_INFO("spd epoll_fd=%d timer=%dsec", sp_efd,
            SP_OPERATION_TIMER);

    return 0;
}

static void mbd_reconnect_try(void)
{
    static time_t last_try = 0;

    if (sp_mbd_chan >= 0)
        return;

    time_t t = time(NULL);
    if (last_try > 0 && (t - last_try) < 5)
        return;

    last_try = t;
    LL_ERRX("attempting mbd reconnect");

    if (sp_mbd_connect() < 0) {
        LL_ERRX("failed connecting to mbd, retry...");
        return;
    }
    sp_register();
}

// sp_init() failed, or terminate requested: clean up file descriptors.
static void sp_cleanup(void)
{
    if (sp_mbd_chan >= 0)
        chan_close(sp_mbd_chan);
    if (sp_timer_chan >= 0)
        chan_close(sp_timer_chan);
    if (sp_efd >= 0)
        close(sp_efd);
    ll_closelog();
}

void sp_fatal(enum sp_fatal_cause cause)
{
    switch (cause) {
    case SP_FATAL_PROTO:
        LL_ERRX("FATAL: protocol violation");
        break;
    case SP_FATAL_OOM:
        LL_ERRX("FATAL: out of memory");
        break;
    case SP_FATAL_ENQUEUE:
        LL_ERRX("FATAL: failed enqueue message to mbd");
        break;
    default:
        LL_ERRX("FATAL: unknown cause=%d", (int) cause);
        break;
    }

    sp_cleanup();

    // Fail-fast: let systemd restart; avoids half-working daemon.
    _exit(1);
}

static void sp_run_daemon(void)
{
    LL_INFO("spd enter main loop");

    while (1) {
        if (sp_croak)
            break;

        // We pass -1 as the timer channel will ring
        int nready = chan_epoll(sp_efd, sp_events, CHAN_MAX, -1);
        int epoll_errno = errno;

        if (nready < 0) {
            if (epoll_errno == EINTR)
                continue;
            errno = epoll_errno;
            LL_ERR("network I/O");
            sleep(1);
            continue;
        }

        for (int i = 0; i < nready; i++) {
            struct epoll_event *ev = &sp_events[i];
            int chan_id = (int) ev->data.u32;

            if (chan_id == sp_timer_chan) {
                uint64_t expirations;
                ssize_t cc = read(chan_sock(chan_id), &expirations,
                                  sizeof(expirations));
                if (cc < 0 && errno != EINTR)
                    LL_ERR("timer read failed, do maintenance anyway");
                else if (cc >= 0 && (size_t) cc != sizeof(expirations))
                    LL_ERR("timer short read: %zd bytes", cc);

                mbd_reconnect_try();
                // reset the state
                channels[chan_id].chan_events = CHAN_EPOLLNONE;
                continue;
            }

            // True skip partially read channels
            if (channels[chan_id].chan_events == CHAN_EPOLLNONE)
                continue;

            // There is an event on the permanent channel
            // connection with mbd
            if (chan_id == sp_mbd_chan) {
                sp_mbd_route(chan_id);
                channels[chan_id].chan_events = CHAN_EPOLLNONE;
                continue;
            }

            // Everything else is either a service's listening socket
            // (accept-ready) or one leg of an active client<->backend
            // relay (read/write-ready). Linear scan is fine at the
            // scale spd operates at -- same call already made
            // mbd-side for service/instance lookups.
            struct sp_instance *inst = sp_find_instance_by_listen(chan_id);
            if (inst != NULL) {
                sp_relay_accept(inst);
                channels[chan_id].chan_events = CHAN_EPOLLNONE;
                continue;
            }

            struct sp_relay *relay = sp_find_relay(chan_id);
            if (relay != NULL) {
                sp_relay_event(relay, chan_id, ev->events);
                channels[chan_id].chan_events = CHAN_EPOLLNONE;
                continue;
            }

            LL_ERRX("sp_run_daemon: event on unknown chan=%d, ignoring",
                    chan_id);
            channels[chan_id].chan_events = CHAN_EPOLLNONE;
        }
    }

    LL_INFO("terminate requested, exiting");
    sp_cleanup();
}

static struct option long_options[] = {
    {"confdir", required_argument, 0, 'c'},
    {"version", no_argument, 0, 'V'},
    {"help", no_argument, 0, 'h'},
    {0, 0, 0, 0}};

static void usage(void)
{
    fprintf(stderr,
           "spd: [OPTIONS]\n"
           " -V, --version       Print version and exit\n"
           " -c, --confdir dir   Set LL_CONF_DIR\n"
           " -h, --help          Show this help\n");
}

int main(int argc, char **argv)
{
    int cc;
    char *conf_dir = NULL;

    while ((cc = getopt_long(argc, argv, "c:Vh", long_options, NULL)) !=
           EOF) {
        switch (cc) {
        case 'c':
            conf_dir = optarg;
            setenv("LL_CONF_DIR", conf_dir, 1);
            break;
        case 'V':
            fprintf(stderr, "%s\n", LAVALITE_VERSION_STR);
            return 0;
        default:
            usage();
            return -1;
        }
    }

    if (conf_dir == NULL) {
        if ((conf_dir = getenv("LL_CONF_DIR")) == NULL) {
            fprintf(stderr,
                   "spd: LL_CONF_DIR must be defined, cannot run\n");
            return -1;
        }
    }

    int rc = sp_init();
    if (rc < 0) {
        LL_ERRX("spd: fatal error during initialization, see "
                "previous messages");
        return -1;
    }

    char name[MAXHOSTNAMELEN];
    gethostname(name, MAXHOSTNAMELEN);
    LL_INFO("spd uid=%d starting on host=%s", getuid(), name);

    sp_run_daemon();

    return 0;
}
