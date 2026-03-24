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

// LavaLite
// bmgr of the entire batch system
// has to be equal to the base manager as returned by
// ls_clusterinfo()
// One user, fixed identity, zero UID gymnastics.
struct mbd_manager *mbd_mgr;
// epoll interface for chan_epoll()
int mbd_efd;
int mbd_max_events;
struct epoll_event *mbd_events;
int mbd_chan;
uint16_t mbd_port;
char mbd_host[MAXHOSTNAMELEN];
// hash hData by channel id
struct ll_hash hdata_by_chan;

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

    conf_init(path);

    events_init();

    network_init();

    // start compact
    mbd_compact_start();

}

static int mbd_accept(int ch_id)
{
    struct sockaddr_in from;

    int ch_id = chan_accept(tcp_chan, &from);
    if (ch_id < 0) {
        LS_ERR("%s: chan_accept() failed: %m", __func__);
        return -1;
    }

    char addr[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &from.sin_addr, addr, sizeof(addr));
    struct mbd_node *n = ll_hash_search(&node_addr_hash, addr);
    if (n == NULL) {
        LS_ERR("rejected accept from unknown host %s", addr_to_str(&from));
        chan_close(ch_id);
        return -1;
    }

    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLRDHUP;
    ev.data.u32 = ch_id;
    epoll_ctl(mbd_efd, EPOLL_CTL_ADD, chan_sock(ch_id), &ev);

    return ch_id;
}

static void mbd_message(int ch_id)
{
    LS_DEBUG("ch_id=%d sock=%d events=0x%x",
             ch_id, channels[ch_id].sock, channels[ch_id].chan_events);

    // See if it is an sbd node
    char key[LL_BUFSIZ_32];
    struct hData *host_data;

    snprintf(key, sizeof(key), "%d", ch_id);
    host_data = ll_hash_search(&hdata_by_chan, key);
    // Here its a little assymetric with non sbd client
    // handling. If there is an exception on the channel with
    // sbd we have to handle it inside this call because beside
    // just shutting down the client we have to do other operations
    // like clean up caches, jobs states etc
    // so the asymmetry is intentional and correct.
    if (host_data) {
        // handle the sbd client
        LS_DEBUG("the client is an sbd %s", host_data->host);
        // dispatch the payload decoder
        sbd_dispatch(host_data->sbd_node);
        return;
    }

    client_dispatch(client);
    // No client found for this channel id: internal inconsistency.
    LS_ERR("no client found for chanfd=%d", ch_id);
}

static int client_dispatch(int ch_id)
{
    if (chan_has_error(ch_id)) {
        LS_DEBUG("channel=%d from=%s closed connection", ch_id,
                 chan_addr_str(ch_id));
        shutdown_tcp_chan(ch_id);
        return -1;
    }

    struct chan_buffer *buf;
    struct protocol_header hdr;
    XDR xdrs;

    if (chan_dequeue(ch_id, &buf) < 0) {
        LS_ERR("chan_dequeue() failed");
        shutdown_tcp_chan(ch_id);
        return;
    }

    xdrmem_create(&xdrs, buf->data, buf->len, XDR_DECODE);
    if (!xdr_pack_hdr(&xdrs, &hdr)) {
        LS_ERR("xdr_pack_hdr failed");
        xdr_destroy(&xdrs);
        shutdown_tcp_chan(ch_id);
        return;
    }

    LS_DEBUG("protocol=%s", proto_to_str(hdr.operation));

    switch (hdr.operation) {
    case BATCH_JOB_SUB:
        job_submit(&xdrs, ch_id);
        break;
    case BATCH_JOB_SIG:
        job_signal(&xdrs, ch_id);
        xdr_destroy(&xdrs);
        break;
    case BATCH_GROUP_INFO:
        host_group_info(&xdrs, ch_id);
        break;
    case BATCH_QUEUE_INFO:
        queue_info(&xdrs, ch_id);
        break;
    case BATCH_JOB_INFO:
        job_info(&xdrs, ch_id);
        break;
    case BATCH_HOST_INFO:
        host_info(&xdrs, ch_id);
        break;
    case BATCH_SBD_REGISTER:
        int cc = mbd_sbd_register(&xdrs, client, &req_hdr);
        xdr_destroy(&xdrs);
        chan_free_buf(buf);
        if (cc < 0) {
            assert(client->host_node != NULL);
            struct hData *host_data = client->host_node;
            shutdown_mbd_client(client);
            if (host_data)
                host_data->sbd_node = NULL;
        }
        return 0;
    case BATCH_COMPACT_DONE:
        mbd_handle_compact_done(&xdrs, ch_id, &req_hdr);
        chan_free_buf(buf);
    default:
        LS_ERR("unknown request=%d from=%s", req_hdr.operation,
               chan_addr_str(ch_id));
        if (req_hdr.version <= CURRENT_PROTOCOL_VERSION)
            LS_ERRX("protocol version=0x%x error", hdr.version);
            break;
    }

    xdr_destroy(&xdrs);
    return 0;
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
