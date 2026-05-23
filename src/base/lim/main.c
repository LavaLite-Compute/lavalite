// Copyright (C) 2007 Platform Computing Inc
// Copyright (C) LavaLite Contributors
// GPL v2

#include "base/lib/auth.h"
#include "base/lim/lim.h"

int udp_chan = -1;
int tcp_chan = -1;
static int timer_chan = -1;
uint16_t lim_port;
int lim_efd;
static struct epoll_event lim_events[CHAN_MAX];

struct ll_list node_list;
struct ll_hash node_name_hash;
struct ll_hash node_addr_hash;
struct cluster lim_cluster;
static int croaked;
int lim_efd;
int n_master_candidates = 0;
struct lim_node **master_candidates;
struct lim_node *me;
struct current_master current_master;

static void light_house(void)
{
    if (current_master.node == me)
        master();
    else
        slave();
}
static int init_chans(void)
{
    if (!ll_atoi(ll_params[LL_LIM_PORT].val, (int *) &lim_port)) {
        errno = EINVAL;
        LS_ERRX("invalid LL_LIM_PORT=%s", ll_params[LL_LIM_PORT].val);
        return -1;
    }

    chan_init();

    udp_chan = chan_udp_server(lim_port);
    if (udp_chan < 0) {
        LS_ERRX("chan_udp_socket failed port=%d", lim_port);
        return -1;
    }

    tcp_chan = chan_tcp_server(lim_port);
    if (tcp_chan < 0) {
        LS_ERRX("chan_tcp_listen_socket failed");
        chan_close(tcp_chan);
        chan_close(udp_chan);
        return -1;
    }

    // This is not a channel, just a fake name for the time
    // that goes in the data structure
    timer_chan = chan_create_timer(5);
    if (timer_chan < 0) {
        chan_close(udp_chan);
        chan_close(tcp_chan);
        return -1;
    }

    return 0;
}

static int add_listener(int lim_efd, int fd, int ch_id)
{
    struct epoll_event ev = {.events = EPOLLIN, .data.u32 = ch_id};

    if (epoll_ctl(lim_efd, EPOLL_CTL_ADD, fd, &ev) < 0)
        return -1;

    return 0;
}

static int init_network(void)
{
    if (init_chans() < 0) {
        LS_ERR("init_chans failed");
        return -1;
    }

    // epoll file descriptor
    lim_efd = epoll_create1(0);
    if (lim_efd < 0) {
        LS_ERR("%s: epoll_create1() failed: %m", __func__);
        chan_close(tcp_chan);
        chan_close(udp_chan);
        chan_close(timer_chan);
        return -1;
    }

    if (add_listener(lim_efd, chan_sock(udp_chan), udp_chan) < 0) {
        syslog(LOG_ERR, "Failed to add UDP listener: %m");
        goto cleanup;
    }

    if (add_listener(lim_efd, chan_sock(tcp_chan), tcp_chan) < 0) {
        LS_ERR("%s: Failed to add TCP listener: %m", __func__);
        goto cleanup;
    }

    if (add_listener(lim_efd, chan_sock(timer_chan), timer_chan) < 0) {
        LS_ERR("%s: Failed to add timer: %m", __func__);
        goto cleanup;
    }

    return 0;

cleanup:
    chan_close(udp_chan);
    chan_close(tcp_chan);
    close(timer_chan);
    return -1;

    return 0;
}

static void croak_handler(int sig)
{
    (void) sig;
    croaked = 1;
}

static int lim_init(const char *conf_dir)
{
    auth_set_required(0);

    ll_list_init(&node_list);
    ll_hash_init(&node_name_hash, 1021);
    ll_hash_init(&node_addr_hash, 1021);

    LS_DEBUG("tables initialized");

    char path[PATH_MAX];
    int cc = snprintf(path, PATH_MAX, "%s/ll.conf", conf_dir);
    if (cc < 0 || cc > PATH_MAX) {
        LS_ERR("path too long or sprintf error");
        return -1;
    }

    cc = load_conf(path);
    if (cc < 0) {
        LS_ERR("failed loading config from=%s", path);
        return -1;
    }

    cc = snprintf(path, PATH_MAX, "%s/ll.cluster.%s", conf_dir,
                  ll_params[LL_CLUSTER_NAME].val);
    if (cc < 0 || cc > PATH_MAX) {
        LS_ERR("path too long or sprintf error");
        return -1;
    }
    cc = make_cluster(path);
    if (cc < 0) {
        LS_ERRX("make_cluster failed");
        return -1;
    }
    LS_DEBUG("configuration loaded");

    install_signal_handler(SIGTERM, croak_handler, 0);
    install_signal_handler(SIGINT, croak_handler, 0);
    LS_DEBUG("signals initialized");

    cc = init_network();
    if (cc < 0) {
        LS_ERR("lim_make_cluster failed");
        return -1;
    }
    LS_DEBUG("network  initialized");

    return 0;
}

int is_master_candidate(struct lim_node *n)
{
    if (n->is_candidate)
        return 1;
    return 0;
}

static void is_master_me(void)
{
    if (!is_master_candidate(me)) {
        LS_INFO("lim=%s host_no=%d is not master candidate", me->host->name,
                me->host_no);
        current_master.node = NULL;
        return;
    }

    current_master.node = NULL;
    current_master.inactivity = 0;
    if (me->host_no == 0) {
        current_master.node = me;
        LS_INFO("lim=%s host_no=%d is now master", me->host->name, me->host_no);
        return;
    }

    // I am not the master so I have to wait for the beacon
    LS_INFO("I am master candidate lim=%s host_no=%d master_tolerance=%d",
            me->host->name, me->host_no, me->host_no * MISSED_BEACON_TOLERANCE);
}

static void usage(void)
{
    fprintf(stderr, "lim:  --version lim version\n"
                    " --confdir path to configuration directory\n"
                    " --help itself\n");
}

int main(int argc, char **argv)
{
    struct option long_options[] = {{"confdir", required_argument, 0, 'e'},
                                    {"version", no_argument, 0, 'V'},
                                    {"check", no_argument, 0, 'C'},
                                    {"help", no_argument, 0, 'h'},
                                    {0, 0, 0, 0}};

    int cc;
    char *conf_dir = NULL;

    while ((cc = getopt_long(argc, argv, ":VCh", long_options, NULL)) != EOF) {
        switch (cc) {
        case 'c':
            conf_dir = strdup(optarg);
            fprintf(stderr, "[lavalite] overriding LL_CONF_DIR=%s\n", optarg);
            break;
        case 'V':
            fprintf(stderr, "%s\n", LAVALITE_VERSION_STR);
            return 0;
        case 'h':
            usage();
            break;
        default:
            usage();
            return -1;
        }
    }

    // first open to capture eventual startup failures
    ls_openlog("lim", "/tmp", "LOG_DEBUG");

    if (conf_dir == NULL) {
        if ((conf_dir = getenv("LL_CONF_DIR")) == NULL) {
            fprintf(stderr, "lim: LL_CONF_DIR must be defined, cannot run\n");
            return -1;
        }
    }

    cc = lim_init(conf_dir);
    if (cc < 0) {
        LS_ERRX("lim_init failed. cannot run");
        return -1;
    }

    ls_closelog();
    cc = ls_openlog("lim", ll_params[LL_LOG_DIR].val,
                    ll_params[LL_LOG_MASK].val);
    if (cc < 0) {
        fprintf(stderr, "lim: ls_openlog failed lodir=%s mask=%s %m\n",
                ll_params[LL_LOG_DIR].val, ll_params[LL_LOG_MASK].val);
        return -1;
    }

    LS_INFO("lim started: %s", LAVALITE_VERSION_STR);

    init_read_proc();
    is_master_me();
    light_house();
    croaked = 0;

    while (1) {
        if (croaked)
            break;

        int nfd = chan_epoll(lim_efd, lim_events, CHAN_MAX, -1);
        if (nfd < 0) {
            if (errno != EINTR) {
                LS_ERR("chan_epoll");
                millisleep(1000);
            }
            continue;
        }

        if (nfd <= 0)
            continue;

        for (int i = 0; i < nfd; i++) {
            struct epoll_event *e = &lim_events[i];
            int ch_id = e->data.u32;

            if (ch_id == timer_chan) {
                uint64_t expirations;
                static time_t last_timer;
                time_t t = time(NULL);

                ssize_t n =
                    read(chan_sock(ch_id), &expirations, sizeof(expirations));
                if (n < 0 && errno != EINTR)
                    LS_ERR("timer read failed");

                if (t - last_timer > 15) {
                    LS_DEBUG("timer run %s", ctime2(NULL));
                    last_timer = t;
                }
                light_house();
                continue;
            }

            if (ch_id == udp_chan) {
                udp_message();
                continue;
            }

            if (ch_id == tcp_chan) {
                tcp_accept();
                continue;
            }

            if (chan_is_readable(ch_id))
                tcp_message(ch_id);
        }
    }

    LS_INFO("lim exited");
}
