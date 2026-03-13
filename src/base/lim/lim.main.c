// Copyright (C) 2007 Platform Computing Inc
// Copyright (C) LavaLite Contributors
// GPL v2


#include "base/lim/lim.h"

int lim_udp_chan = -1;
int tcp_chan = -1;
int lim_timer_chan = -1;
uint16_t lim_udp_port;
uint16_t lim_tcp_port;
int lim_debug;

int lim_efd;
static struct epoll_event *lim_events;

struct ll_list node_list;
struct ll_hash node_name_hash;
struct ll_hash node_addr_hash;
struct cluster lim_cluster;
static int croaked;
int lim_efd;
int n_master_candidates= 0;
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
    if (! ll_atoi(ll_params[LSF_LIM_PORT].val, (int *) &lim_udp_port)) {
        errno = EINVAL;
        LS_ERR("invalid LSF_LIM_PORT=%s", ll_params[LSF_LIM_PORT].val);
        return -1;
    }

    lim_udp_chan = chan_udp_socket(lim_udp_port);
    if (lim_udp_chan < 0) {
        syslog(LOG_ERR,
               "%s: unable to create datagram socket port %d "
               "another LIM running?: %m ",
               __func__, lim_udp_port);
        return -1;
    }

    tcp_chan = chan_tcp_listen_socket(0);
    if (tcp_chan < 0) {
        syslog(LOG_ERR,
               "%s: unable to create tcp socket port %d "
               "another LIM running?: %m ",
               __func__, lim_udp_port);
        chan_close(tcp_chan);
        chan_close(lim_udp_chan);
        return -1;
    }

    struct sockaddr_in lim_addr;
    socklen_t size = sizeof(struct sockaddr_in);
    int cc = getsockname(chan_sock(tcp_chan), (struct sockaddr *)&lim_addr,
                         &size);
    if (cc < 0) {
        syslog(LOG_ERR, "%s: getsocknamed(%d) failed: %m", __func__,
               tcp_chan);
        chan_close(tcp_chan);
        chan_close(tcp_chan);
        return -1;
    }

    // dynamic tcp port sent to slave lims and library which need
    // to find the master. keep it in network order
    me->tcp_port = lim_addr.sin_port;

    // This is not a channel, just a fake name for the time
    // that goes in the data structure
    lim_timer_chan = chan_create_timer(5);
    if (lim_timer_chan < 0) {
        chan_close(lim_udp_chan);
        chan_close(tcp_chan);
        return -1;
    }

    return 0;
}

static int create_epoll(void)
{
    // epoll file descriptor
    lim_efd = epoll_create1(0);
    if (lim_efd < 0) {
        LS_ERR("%s: epoll_create1() failed: %m", __func__);
        chan_close(tcp_chan);
        chan_close(tcp_chan);
        return -1;
    }

    // The global array of epoll_event
    lim_events = calloc(chan_open_max, sizeof(struct epoll_event));
    if (lim_events == NULL) {
        LS_ERR("%s: calloc failed %m", __func__);
        chan_close(tcp_chan);
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
    init_chans();
    create_epoll();

    if (add_listener(lim_efd, chan_sock(lim_udp_chan), lim_udp_chan) < 0) {
        syslog(LOG_ERR, "Failed to add UDP listener: %m");
        goto cleanup;
    }

    if (add_listener(lim_efd, chan_sock(tcp_chan), tcp_chan) < 0) {
        LS_ERR("%s: Failed to add TCP listener: %m", __func__);
        goto cleanup;
    }

    if (add_listener(lim_efd, chan_sock(lim_timer_chan), lim_timer_chan) < 0) {
        LS_ERR("%s: Failed to add timer: %m", __func__);
        goto cleanup;
    }

    return 0;

cleanup:
    chan_close(lim_udp_chan);
    chan_close(tcp_chan);
    close(lim_timer_chan);
    return -1;

    return 0;
}

static void croak_handler(int sig)
{
    croaked = 1;
}

static void child_handler(int sig)
{
    while (waitpid(-1, NULL, WNOHANG) > 0)
        ;
}

static int init_daemon(const char *conf_dir)
{
    ll_list_init(&node_list);
    ll_hash_init(&node_name_hash, 1021);
    ll_hash_init(&node_addr_hash, 1021);

    char path[PATH_MAX];
    int cc = snprintf(path, PATH_MAX, "%s/lsf.conf", conf_dir);
    if (cc < 0 || cc > PATH_MAX) {
        LS_ERR("path too long or sprintf error");
        return -1;
    }

    cc = load_conf(path);
    if (cc < 0) {
        LS_ERR("failed loading config");
        return -1;
    }

    ls_closelog();
    ls_openlog("lim", ll_params[LSF_LOGDIR].val,
               lim_debug, 0, ll_params[LSF_LOG_MASK].val);

    cc = make_cluster(path);
    if (cc < 0) {
        LS_ERR("make_cluster failed");
        return -1;
    }

    LS_INFO("initializing signals");
    install_signal_handler(SIGTERM, croak_handler, 0);
    install_signal_handler(SIGINT, croak_handler, 0);
    install_signal_handler(SIGCHLD, child_handler, SA_RESTART);

    cc = init_network();
    if (cc < 0) {
        LS_ERR("lim_make_cluster failed");
        return -1;
    }

    return 0;
}

int is_master_candidate(struct lim_node *n)
{
    if (n->is_candidate)
        return 1;
    return 0;
}

static void is_master(void)
{
    if (! is_master_candidate(me)) {
        LS_INFO("lim=%s host_no=%d is not master candidate",  me->host->name,
                me->host_no);
        current_master.node = NULL;
        return;
    }

    current_master.node = NULL;
    current_master.inactivity = 0;
    if (me->host_no == 0) {
        current_master.node = me;
        LS_INFO("lim=%s host_no=%d is now master", me->host->name,
                me->host_no);
        return;
    }

    // I am not the master so I have to wait for the beacon
    LS_INFO("I am master candidate lim=%s host_no=%d master_tolerance=%d",
            me->host->name, me->host_no, me->host_no * MISSED_BEACON_TOLERANCE);
}

static void usage(const char *cmd)
{
    fprintf(
        stderr,
        "Usage: %s [OPTIONS]\n"
        "  -d, --debug Run in foreground (no daemonize)\n"
        "  -C, --check Configuration check (prints version, sets check mode)\n"
        "  -V, --version     Print version and exit\n"
        "  -e, --envdir DIR  Path to env dir \n"
        "  -h, --help Show this help\n",
        cmd);
}

int main(int argc, char **argv)
{
    struct option long_options[] = {
        {"debug", no_argument, 0, 'd'},
        {"envdir", required_argument, 0, 'e'},
        {"version", no_argument, 0, 'V'},
        {"check", no_argument, 0, 'C'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };

    int cc;
    char *conf_dir;

    while ((cc = getopt_long(argc, argv, "de:VCh", long_options, NULL)) !=
           EOF) {
        switch (cc) {
        case 'd':
            lim_debug = 1;
            break;
        case 'e':
            conf_dir = strdup(optarg);
            fprintf(stderr, "[lavalite] overriding LSF_ENVDIR=%s\n", optarg);
            break;
        case 'V':
            fprintf(stderr, "%s\n", LAVALITE_VERSION_STR);
            return -1;
        default:
            usage(argv[0]);
            return -1;
        }
    }

    // first open to capture eventual startup failures
    ls_openlog("lim", "/tmp", 1, 1, "LOG_DEBUG");

    if (conf_dir == NULL) {
        if ((conf_dir = getenv("LSF_ENVDIR")) == NULL) {
            fprintf(stderr, "lim: LSF_ENVDIR must be defined, cannot run\n");
            return -1;
        }
    }

    cc = init_daemon(conf_dir);
    if (cc < 0) {
        LS_ERR("lim_init failed. cannot run");
        return -1;
    }

    LS_INFO("lim started: %s", LAVALITE_VERSION_STR);

    init_read_proc();
    is_master();
    croaked = 0;

    while (1) {

        if (croaked)
            break;

        int nfd = chan_epoll(lim_efd, lim_events, 1024, -1);
        if (nfd < 0) {
            if (errno != EINTR) {
                syslog(LOG_ERR, "chan_epoll");
                millisleep(1000);
            }
            continue;
        }

        if (nfd <= 0)
            continue;

        for (int i = 0; i < nfd; i++) {
            struct epoll_event *e = &lim_events[i];
            int ch_id = e->data.u32;

            if (ch_id == lim_timer_chan) {
                uint64_t expirations;
                static time_t last_timer;
                time_t t = time(NULL);

                read(chan_sock(ch_id), &expirations, sizeof(expirations));
                if (t - last_timer > 60 * 15) {
                    LS_DEBUG("timer run %s", ctime2(NULL));
                    last_timer = t;
                }
                light_house();
                continue;
            }

            if (ch_id == lim_udp_chan) {
                udp_message();
                continue;
            }

            if (ch_id == tcp_chan) {
                tcp_accept();
                continue;
            }

            tcp_message(ch_id);
        }
    }

    LS_INFO("lim exited");
}
