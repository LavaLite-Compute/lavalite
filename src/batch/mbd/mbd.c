/* Copyright (C) LavaLite Contributors
 * GPL v2
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <getopt.h>
#include <errno.h>
#include <syslog.h>
#include <sys/epoll.h>

#include "base/lib/ll.syslog.h"
#include "base/lib/ll.conf.h"
#include "base/lib/ll.channel.h"
#include "base/lib/auth.h"

#include "batch/mbd/mbd.h"

struct ll_list host_list;
struct ll_hash host_name_hash;
struct ll_hash host_addr_hash;
struct ll_list group_list;
struct ll_hash group_name_hash;
struct ll_list queue_list;
struct ll_hash queue_name_hash;
struct ll_hash sbd_chan_hash;
struct ll_list token_pool_list;
struct ll_hash token_pool_name_hash;

struct mbd_manager mbd_mgr;

int mbd_efd;
struct epoll_event mbd_events[CHAN_MAX];
uint16_t mbd_port;
int chan_mbd;
int chan_timer;
int sched_timer;

static const char *mbd_exit_str(enum mbd_exit e)
{
    switch (e) {
    case MBD_EXIT_CONF:
        return "configuration error";
    case MBD_EXIT_NET:
        return "network error";
    case MBD_EXIT_EVENTS:
        return "events log error";
    case MBD_EXIT_JOBS:
        return "job directory error";
    case MBD_EXIT_MEM:
        return "out of memory";
    case MBD_EXIT_FATAL:
        return "fatal error";
    }
    return "unknown";
}

static int mbd_init(void)
{
    ll_list_init(&host_list);
    ll_hash_init(&host_name_hash, 1021);
    ll_hash_init(&host_addr_hash, 1021);
    ll_list_init(&group_list);
    ll_hash_init(&group_name_hash, 127);
    ll_list_init(&queue_list);
    ll_hash_init(&queue_name_hash, 31);
    ll_hash_init(&sbd_chan_hash, 1021);
    ll_list_init(&token_pool_list);
    ll_hash_init(&token_pool_name_hash, 1021);

    if (conf_init() < 0) {
        LS_ERRX("conf_init failed");
        return -1;
    }

    int auth_age;
    // AUTH_MAX_AGE is build with default 60 seconds
    ll_atoi(ll_params[LL_AUTH_MAX_AGE].val, &auth_age);
    if (auth_init(1, auth_age) < 0) {
        LS_ERRX("auth_init failed");
        return -1;
    }

    if (network_init() < 0) {
        LS_ERRX("event_init failed");
        return -1;
    }

    if (events_init() < 0) {
        LS_ERRX("event_init failed");
        return -1;
    }

    if (job_init() < 0) {
        LS_ERRX("job_init failed");
    }

    if (queue_state_init() < 0) {
        LS_ERRX("queue_state_init failed");
        return -1;
    }

    if (host_state_init() < 0) {
        LS_ERRX("host_state_init failed");
        return -1;
    }

    return 0;
}

static void check_not_root(void)
{
    if (getuid() == 0 || geteuid() == 0) {
        LS_ERR("mbd does not want to run as root ruid=%u, euid=%u", getuid(),
               geteuid());
        mbd_die(MBD_EXIT_FATAL);
    }
}

static void usage(void)
{
    fprintf(stderr, "mbd: --help\n"
                    "--version \n"
                    "--confdir set environment variable LL_CONF_DIR\n"
                    "--timer_sched\n");
}

static struct option longopts[] = {
    {"help", no_argument, NULL, 'h'},
    {"version", no_argument, NULL, 'V'},
    {"confdir", required_argument, NULL, 'c'},
    {"sched_timer", required_argument, NULL, 't'},
    {NULL, 0, NULL, 0}};

int main(int argc, char **argv)
{
    int cc;
    char *conf_dir = NULL;

    sched_timer = SCHED_TIMER;
    while ((cc = getopt_long(argc, argv, "hVt:c:", longopts, NULL)) != EOF) {
        switch (cc) {
        case 'c':
            conf_dir = optarg;
            break;
        case 'V':
            fprintf(stderr, "%s\n", LAVALITE_VERSION_STR);
            return 0;
        case 't':
            if (!ll_atoi(optarg, &sched_timer) || sched_timer <= 0) {
                fprintf(stderr, "mbd: invalid sched_timer value=%s\n", optarg);
                return -1;
            }
            break;
        case 'h':
        default:
            usage();
            return -1;
        }
    }

    ls_openlog("mbd", "/tmp", "LOG_DEBUG");

    if (conf_dir == NULL) {
        if ((conf_dir = getenv("LL_CONF_DIR")) == NULL) {
            fprintf(stderr, "mbd: LL_CONF_DIR must be defined, cannot run\n");
            return -1;
        }
    }

    check_not_root();

    if (mbd_init() < 0) {
        LS_ERRX("mbd_init failed. cannot run");
        return -1;
    }

    ls_closelog();
    cc = ls_openlog("mbd", ll_params[LL_LOG_DIR].val,
                    ll_params[LL_LOG_MASK].val);
    if (cc < 0) {
        fprintf(stderr, "mbd: ls_openlog failed lodir=%s mask=%s %m\n",
                ll_params[LL_LOG_DIR].val, ll_params[LL_LOG_MASK].val);
        return -1;
    }

    LS_INFO("mbd uid=%d starting on host=%s sched_timer=%d", getuid(),
            ll_params[LL_MBD_HOST].val, sched_timer);

    for (;;) {
        int nevents = chan_epoll(mbd_efd, mbd_events, CHAN_MAX, -1);
        if (nevents < 0) {
            if (errno != EINTR) {
                LS_ERR("chan_epoll(%d) failed", mbd_efd);
                millisleep(1000);
                continue;
            }
        }

        for (int i = 0; i < nevents; i++) {
            struct epoll_event *ev = &mbd_events[i];
            int chan_id = (int) ev->data.u32;
            LS_DEBUG("channel=%d mask=0x%x", chan_id, ev->events);
            // True skip partually read channels
            if (channels[chan_id].chan_events == CHAN_EPOLLNONE)
                continue;

            if (chan_id == chan_timer) {
                uint64_t exp;
                read(chan_sock(chan_id), &exp, sizeof(exp));
                LS_DEBUG("sched_timer expired timer=%d", sched_timer);
                schedule();
                maybe_compact_events();
                continue;
            }

            if (chan_id == chan_mbd) {
                mbd_accept(chan_mbd);
                continue;
            }

            if (chan_is_readable(chan_id))
                mbd_message(chan_id);
        }
    }

    return 0;
}

void mbd_die(enum mbd_exit e)
{
    LS_INFO("exiting with reason: %s", mbd_exit_str(e));
    // mbd_compact_shutdown();
    exit(-1);
}
