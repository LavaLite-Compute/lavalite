/*
 * Copyright (C) 2007 Platform Computing Inc
 * Copyright (C) LavaLite Contributors
 * GPL v2
 */

#include "batch/mbd/mbd.h"
#include "batch/daemons/mbatchd.h"

struct ll_list host_list;
struct ll_hash host_name_hash;
struct ll_hash host_addr_hash;
struct ll_list group_list;
struct ll_hash group_name_hash;
struct ll_list queue_list;
struct ll_hash queue_name_hash;

struct mbd_manager *mbd_mgr;

int mbd_efd;
struct epoll_event mbd_events[CHAN_MAX];
int mbd_chan;
uint16_t mbd_port;
char mbd_host[MAXHOSTNAMELEN];

static void mbd_check_not_root(void);
static void mbd_init_log(void);
static int mbd_accept(int);
static void mbd_client_handle(int);
static int client_dispatch(struct mbd_client_node *);

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

static int mbd_init(const char *path)
{
    ll_list_init(&host_list);
    ll_hash_init(&host_name_hash, 1021);
    ll_hash_init(&host_addr_hash, 1021);
    ll_list_init(&group_list);
    ll_hash_init(&group_name_hash, 127);
    ll_list_init(&queue_list);
    ll_hash_init(&queue_name_hash, 31);

    if (conf_init() < 0) {
        LS_ERRX("conf_init failed");
        return -1;
    }

    if (events_init() < 0) {
        LS_ERRX("event_init failed");
        return -1;
    }

    if (network_init() < 0) {
        LS_ERRX("event_init failed");
        return -1;
    }

    // start compact
    compact_start();
}


void shutdown_chan(struct mbd_client_node *client)
{
    epoll_ctl(lim_efd, EPOLL_CTL_DEL, chan_sock(ch_id), NULL);
    chan_close(ch_id);
}

static void check_not_root(void)
{
    if (getuid() == 0 || geteuid() == 0) {
        LS_ERR("mbatchd must not run as root (ruid=%u, euid=%u)",
               getuid(), geteuid());
        mbdDie(MASTER_FATAL);
    }
}

static void init_log(void)
{
    const char *log_dir = genParams[LSF_LOGDIR].paramValue;
    const char *log_mask = genParams[LSF_LOG_MASK].paramValue;

    bool debug = mbd_debug;
    bool check = lsb_CheckMode;

    if (!log_dir)
        log_dir = "/tmp";

    if (!log_mask)
        log_mask = "LOG_INFO";

    // Initialize LavaLite logging
    if (check) {
        ls_openlog("mbd", log_dir, true, 0, (char *)log_mask);
    } else if (debug) {
        ls_openlog("mbd", log_dir, true, 0, (char *)log_mask);
    } else {
        /* Normal production daemon case */
        ls_openlog("mbd", log_dir, false, 0, (char *)log_mask);
    }

    LS_INFO("mbd uid=%d dir=%s mask=%s debug=%d check=%d",
            getuid(), log_dir, log_mask, debug, check);
}

static void usage(void)
{
    fprintf(stderr, "mbd: --help\n"
            "--version \n"
            "--envdir set environment variable LL_ENVDIR\n");
}
static struct option longopts[] = {{"help", no_argument, NULL, 'h'},
                                   {"version", no_argument, NULL, 'V'},
                                   {"envdir", required_argument, NULL, 'E'},
                                   {NULL, 0, NULL, 0}};

int main(int argc, char **argv)
{
    int cc;
    char *env_dir = NULL;

    while ((cc = getopt_long(argc, argv, "hVe:", longopts, NULL)) != EOF) {
        switch (cc) {
        case 'e':
            env_dir = optarg;
            break;
        case 'V':
            fprintf(stderr, "%s\n", LAVALITE_VERSION_STR);
            return -1;
        case 'h':
        default:
            usage();
            return -1;
        }
    }

    ls_openlog("mbd", "/tmp", "LOG_DEBUG");

    if (conf_dir == NULL) {
        if ((conf_dir = getenv("LL_ENVDIR")) == NULL) {
            fprintf(stderr, "mbd: LL_ENVDIR must be defined, cannot run\n");
            return -1;
        }
    }

    check_not_root();

    if (mbd_init() < 0) {
        LS_ERRX("mbd_init failed. cannot run");
        return -1;
    }

    ls_closelog();
    cc = ls_openlog("mbd", ll_params[LL_LOGDIR].val, ll_params[LL_LOG_MASK].val);
    if (cc < 0) {
        fprintf(stderr, "mbd: ls_openlog failed lodir=%s mask=%s %m\n",
                ll_params[LL_LOGDIR].val,  ll_params[LL_LOG_MASK].val);
        return -1;
    }

    LS_INFO("mbatchd starting as master on host <%s>", mbd_host);

    for (;;) {

        schedule();

        nevents = chan_epoll(mbd_efd, mbd_events, CHAN_MAX, -1);
        if (nevents < 0) {
            if (errno != EINTR) {
                LS_ERR("chan_epoll(%d) failed", mbd_efd);
                millisleep(1000);
                continue;
            }
        }
        for (i = 0; i < nevents; i++) {
            struct epoll_event *ev = &mbd_events[i];
            int ch_id = (int)ev->data.u32;

             // True skip partually read channels
             if (channels[ch_id].chan_events == CHAN_EPOLLNONE)
                 continue;

            if (ch_id == mbd_chan) {
                mbd_accept(mbd_chan);
                continue;
            }

            if (chan_is_readable(ch))
                mbd_message(ch_id);
        }
    }
}

void mbd_die(enum mbd_exit e)
{
    LS_INFO("exiting with reason: %s", mbd_exit_str(e));
    mbd_compact_shutdown();
    exit(-1);
}
