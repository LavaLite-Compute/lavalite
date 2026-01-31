/*
 * Copyright (C) 2007 Platform Computing Inc
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 USA
 *
 */

#include "lsf/lim/lim.h"

// LIM uses both UDP and TCP protocols
int lim_udp_chan = -1;
int lim_tcp_chan = -1;
int lim_timer_chan = -1;
uint16_t lim_udp_port;
uint16_t lim_tcp_port;

// LavaLite epoll file descriptor
static int efd;
static struct epoll_event *lim_events;
// This have the same size and chan_open_max
struct client_node **client_map;

int probeTimeout = 2;
short resInactivityCount = 0;

struct clusterNode *myClusterPtr;
struct hostNode *myHostPtr;
int masterMe;
int nClusAdmins = 0;
int *clusAdminIds = NULL;
int *clusAdminGids = NULL;
char **clusAdminNames = NULL;

int numMasterCandidates = -1;
int isMasterCandidate;
int limConfReady = false;

struct limLock limLock;
char myClusterName[MAXLSFNAMELEN];
u_int loadVecSeqNo = 0;
u_int masterAnnSeqNo = 0;
bool_t lim_debug = false;
int lim_CheckMode = 0;
int lim_CheckError = 0;
char *env_dir = NULL;
int numHostResources;
struct sharedResource **hostResources = NULL;
u_short lsfSharedCkSum = 0;

pid_t pimPid = -1;
static void startPIM(int, char **);
static inline uint64_t now_ms(void);

struct liStruct *li = NULL;
int li_len = 0;
float *extraload;

// LavaLite
static void lim_init(int);
static int lim_init_network(int);
static int lim_init_chans(void);
static int lim_create_epoll();
static int lim_create_clients();
static void term_handler(int);
static void child_handler(int);
static void usage(const char *);
static void init_signals(void);
static void periodic(void);
static struct tclLsInfo *getTclLsInfo(void);
static void initMiscLiStruct(void);

extern struct extResInfo *getExtResourcesDef(char *);
extern char *getExtResourcesLoc(char *);
extern char *getExtResourcesVal(char *);

static int process_udp_request(void);
static int accept_connection(void);

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

static struct option long_options[] = {
    {"debug", no_argument, 0, 'd'},   {"envdir", required_argument, 0, 'e'},
    {"version", no_argument, 0, 'V'}, {"check", no_argument, 0, 'C'},
    {"help", no_argument, 0, 'h'},    {0, 0, 0, 0}};

int main(int argc, char **argv)
{
    int cc;

    while ((cc = getopt_long(argc, argv, "de:VCh", long_options, NULL)) !=
           EOF) {
        switch (cc) {
        case 'd':
            lim_debug = true;
            break;
        case 'e':
            putEnv("LSF_ENVDIR", optarg);
            fprintf(stderr, "[lavalite] overriding LSF_ENVDIR=%s\n", optarg);
            break;
        case 'C':
            putEnv("RECONFIG_CHECK", "YES");
            fprintf(stderr, "lim: %s\n", LAVALITE_VERSION_STR);
            lim_CheckMode = 1;
            lim_debug = true;
            break;
        case 'V':
            fprintf(stderr, "%s\n", LAVALITE_VERSION_STR);
            return 9;
        default:
            usage(argv[0]);
            return -1;
        }
    }

    if (env_dir == NULL) {
        if ((env_dir = getenv("LSF_ENVDIR")) == NULL) {
            env_dir = LL_CONF;
        }
    }

    if (lim_debug)
        fprintf(stderr, "lim: reading configuration from %s/lsf.conf\n",
                env_dir);

    if (initenv_(genParams, env_dir) < 0) {
        char *sp = getenv("LSF_LOGDIR");
        if (sp != NULL)
            genParams[LSF_LOGDIR].paramValue = sp;
        ls_openlog("lim", genParams[LSF_LOGDIR].paramValue, lim_debug,
                   0, genParams[LSF_LOG_MASK].paramValue);
        ls_syslog(LOG_ERR, "lim: initenv_ %s", env_dir);
        syslog(LOG_ERR, "lim: initenv_() failed %s", env_dir);
        lim_Exit("main");
    }

    if (genParams[LSF_LIM_DEBUG].paramValue) {
        lim_debug = true;
    }

    if (lim_debug == false) {
        for (int i = sysconf(_SC_OPEN_MAX); i >= 0; i--)
            close(i);
        daemonize_();
    }

    if (lim_debug) {
        ls_openlog("lim", genParams[LSF_LOGDIR].paramValue, true, 0, "LOG_DEBUG");
    } else {
        ls_openlog("lim", genParams[LSF_LOGDIR].paramValue, false, 0,
                   genParams[LSF_LOG_MASK].paramValue);
    }

    if (initMasterList_() < 0) {
        if (lserrno == LSE_NO_HOST) {
            syslog(LOG_ERR, "%s: There is no valid host in LSF_MASTER_LIST",
                   __func__);
        }
        lim_Exit("initMasterList");
    }
    if (lserrno == LSE_LIM_IGNORE) {
        ls_syslog(
            LOG_WARNING,
            "Invalid or duplicated hostname in LSF_MASTER_LIST. Ignoring host");
    }
    isMasterCandidate = getIsMasterCandidate_();
    numMasterCandidates = getNumMasterCandidates_();

    lim_init(lim_CheckMode);

    masterMe = (myHostPtr->hostNo == 0);

    myHostPtr->hostInactivityCount = 0;

    if (lim_CheckMode) {
        if (lim_CheckError == EXIT_WARNING_ERROR) {
            ls_syslog(LOG_WARNING, "Checking Done. Warning(s/error(s) found.");
            return EXIT_WARNING_ERROR;
        } else {
            ls_syslog(LOG_INFO, ("Checking Done."));
            return 0;
        }
    }

    if (masterMe) {
        initNewMaster();
    }

    initMiscLiStruct();
    init_signals();

    syslog(LOG_INFO, "%s: Daemon running (tcp_port %d %s)", __func__,
           ntohs(myHostPtr->statInfo.portno), LAVALITE_VERSION_STR);

    if (logclass & LC_COMM)
        ls_syslog(
            LOG_DEBUG,
            "%s: sampleIntvl=%f exchIntvl=%f "
            "hostInactivityLimit=%d masterInactivityLimit=%d retryLimit=%d",
            __func__, sampleIntvl, exchIntvl, hostInactivityLimit,
            masterInactivityLimit, retryLimit);

    if (!lim_debug)
        chdir("/tmp");

    // Every 5 seconds
    periodic();

    for (;;) {
        if (pimPid == -1) {
            startPIM(argc, argv);
        }

        // select timeout ever 5 seconds if no network io
        int timeout = 5 * 1000;

        int nready = chan_epoll(efd, lim_events, chan_open_max, timeout);
        if (nready < 0) {
            if (errno != EINTR) {
                syslog(LOG_ERR, "%s: network I/O %m", __func__);
            }
        }

        if (nready <= 0)
            continue;

        for (int i = 0; i < nready; i++) {
            struct epoll_event *e = &lim_events[i];
            int ch_id = e->data.u32;

            if (ch_id == lim_timer_chan) {
                uint64_t expirations;
                static time_t last_timer;
                time_t t = time(NULL);

                read(chan_sock(ch_id), &expirations, sizeof(expirations));
                if (t  - last_timer > 60) {
                    LS_DEBUG("timer run %s", ctime2(NULL));
                    last_timer = t;
                }
                periodic();
                continue;
            }

            if (ch_id == lim_udp_chan) {
                process_udp_request();
                continue;
            }

            if (ch_id == lim_tcp_chan) {
                accept_connection();
                continue;
            }

            handle_tcp_client(ch_id);
        }
    }
}

static uint64_t now_ms(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);

    return (uint64_t) ts.tv_sec * 1000ULL + ts.tv_nsec / 1000000ULL;
}

static int process_udp_request(void)
{
    static char buf[BUFSIZ];
    struct sockaddr_in from;

    memset(&from, 0, sizeof(from));

    struct packet_header reqHdr;
    int cc = chan_recv_dgram(lim_udp_chan, buf, sizeof(buf),
                             (struct sockaddr_storage *) &from, -1);
    if (cc < 0) {
        syslog(LOG_ERR,
               "%s: Error receiving data on lim_udp_chan %d, cc=%d: %m",
               __func__, lim_udp_chan, cc);
        return -1;
    }

    XDR xdrs;
    xdrmem_create(&xdrs, buf, sizeof(buf), XDR_DECODE);
    init_pack_hdr(&reqHdr);
    if (!xdr_pack_hdr(&xdrs, &reqHdr)) {
        syslog(LOG_ERR, "%s: xdr_pack_hdr() failed %m", __func__);
        xdr_destroy(&xdrs);
        return -1;
    }

    struct hostNode *node = find_node_by_sockaddr_in(&from);
    if (node == NULL) {
        syslog(LOG_ERR, "%s: received request %d from unknown host %s",
               __func__, reqHdr.operation, sockAdd2Str_(&from));
        xdr_destroy(&xdrs);
        return -1;
    }

    switch (reqHdr.operation) {
    case LIM_GET_CLUSNAME:
        cluster_name_req(&xdrs, &from, &reqHdr);
        break;
    case LIM_GET_MASTINFO:
        master_info_req(&xdrs, &from, &reqHdr);
        break;
    case LIM_SERV_AVAIL:
        servAvailReq(&xdrs, node, &from, &reqHdr);
        break;
    case LIM_MASTER_ANN:
        masterRegister(&xdrs, &from, &reqHdr);
        break;
    case LIM_MASTER_REGISTER:
        master_register_recv(&xdrs, &from, &reqHdr);
        break;
    case LIM_LOAD_UPD:
        rcvLoad(&xdrs, &from, &reqHdr);
        break;
    case LIM_LOAD_UPD2:
        rcv_load_update(&xdrs, &from, &reqHdr);
        break;
    default:
        if (reqHdr.version <= CURRENT_PROTOCOL_VERSION) {
            errorBack(&from, &reqHdr, LIME_BAD_REQ_CODE, -1);
            syslog(LOG_ERR, "%s: Unknown request code %d vers %d from %s",
                   __func__, reqHdr.operation, reqHdr.version,
                   sockAdd2Str_(&from));
            break;
        }
        ls_syslog(LOG_ERR, "%s: UDP request %d not supported anymore", __func__,
                  reqHdr.operation);
    }

    xdr_destroy(&xdrs);

    return 0;
}

static int accept_connection(void)
{
    struct sockaddr_in from;
    struct hostNode *host;

    if (logclass & (LC_TRACE | LC_COMM))
        ls_syslog(LOG_DEBUG1, "%s: Entering this routine...", __func__);

    int ch_id = chan_accept(lim_tcp_chan, &from);
    if (ch_id < 0) {
        ls_syslog(LOG_ERR, "%s: chan_accept() failed: %m", __func__);
        return -1;
    }

    host = find_node_by_sockaddr_in(&from);
    if (host == NULL) {
        ls_syslog(LOG_WARNING, "\
%s: Received request from non-LSF host %s",
                  __func__, sockAdd2Str_(&from));
        chan_close(ch_id);
        return -1;
    }

    // set the accepted socket in the epoll fd
    struct client_node *client = calloc(1, sizeof(struct client_node));
    if (!client) {
        ls_syslog(LOG_ERR, "%s: Connection from %s dropped", __func__,
                  sockAdd2Str_(&from));
        chan_close(ch_id);
        return -1;
    }

    // Bug create a list
    client_map[ch_id] = client;
    // We only need the channel id and the host node
    client->ch_id = ch_id;
    client->from_host = host;

    // Set the socket under efd and map the client pointer
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.u32 = ch_id;
    epoll_ctl(efd, EPOLL_CTL_ADD, chan_sock(ch_id), &ev);

    return 0;
}

static void init_signals(void)
{
    sigset_t mask;

    signal_set(SIGHUP, term_handler);
    signal_set(SIGINT, term_handler);
    signal_set(SIGTERM, term_handler);
    signal_set(SIGXCPU, term_handler);
    signal_set(SIGXFSZ, term_handler);
    signal_set(SIGPROF, term_handler);
    signal_set(SIGPWR, term_handler);
    signal_set(SIGUSR1, term_handler);
    signal_set(SIGUSR2, term_handler);
    signal_set(SIGCHLD, child_handler);
    signal_set(SIGPIPE, SIG_IGN);
    signal_set(SIGALRM, SIG_IGN);

    sigemptyset(&mask);
    sigprocmask(SIG_SETMASK, &mask, NULL);
}

static void lim_init(int checkMode)
{
    struct tclLsInfo *tclLsInfo;

    if (logclass & (LC_TRACE))
        ls_syslog(LOG_DEBUG, "%s: Entering this routine...; checkMode=%d",
                  __func__, checkMode);

    initLiStruct();
    if (readShared() < 0) {
        lim_Exit("lim_init");
    }

    reCheckRes();

    setMyClusterName();

    if (getenv("RECONFIG_CHECK") == NULL) {
        if (lim_init_network(checkMode) < 0)
            lim_Exit("lim_init_network");
    }

    if (readCluster(checkMode) < 0)
        lim_Exit("readCluster");

    if (reCheckClass() < 0)
        lim_Exit("readCluster");

    if ((tclLsInfo = getTclLsInfo()) == NULL)
        lim_Exit("getTclLsInfo");

    if (initTcl(tclLsInfo) < 0)
        lim_Exit("initTcl");
    initParse(&allInfo);

    // Initliaze the load indexes
    lim_proc_init_read_load(checkMode);
    initTypeModel(myHostPtr);

    if (!checkMode) {
        initConfInfo();
        satIndex();
        loadIndex();
    }
    if (chan_init() < 0)
        lim_Exit("chan_init");

    {
        // Bug what is the purpose of this, set in the env
        // while processing the API so child picks it up?
        char *lsfLimLock = getenv("LSF_LIM_LOCK");
        int flag = -1;
        time_t lockTime = -1;

        if (lsfLimLock != NULL && lsfLimLock[0] != 0) {
            if (logclass & LC_TRACE) {
                ls_syslog(LOG_DEBUG2, "%s: LSF_LIM_LOCK=<%s>", __func__,
                          lsfLimLock);
            }
            sscanf(lsfLimLock, "%d %ld", &flag, &lockTime);
            if (flag > 0) {
                limLock.on = flag;
                limLock.time = lockTime;
                if (LOCK_BY_USER(limLock.on)) {
                    myHostPtr->status[0] |= LIM_LOCKEDU;
                }
                if (LOCK_BY_MASTER(limLock.on)) {
                    myHostPtr->status[0] |= LIM_LOCKEDM;
                }
            }
        } else {
            limLock.on = false;
            limLock.time = 0;

            myHostPtr->status[0] &= ~LIM_LOCKEDU;
        }
    }

    getLastActiveTime();

    limConfReady = true;

    return;
}

static void periodic(void)
{
    readLoad();

    if (masterMe) {
        // announceMaster(myClusterPtr, 1, false);
        announce_master_register(myClusterPtr);
    }

}

// Set an atomic variable to signal the exit
// do nohing else in the handler
static void term_handler(int signum)
{
    signal_set(signum, SIG_DFL);

    ls_syslog(LOG_ERR, "%s: Received signal %d, exiting", __func__, signum);

    chan_close(lim_udp_chan);
    chan_close(lim_tcp_chan);

    if (elim_pid > 0) {
        kill(elim_pid, SIGTERM);
        millisleep_(2000);
    }

    kill(getpid(), signum);
    exit(0);
}

static void child_handler(int sig)
{
    int pid;
    int saved_errno = errno;

    while ((pid = waitpid(-1, NULL, WNOHANG)) > 0) {
        ; // get the pid and check if it is MBD
    }
    // waitpid() can reset the errno so whatever code that ran
    // when SIGCHLD triggered may find unexpected errno
    errno = saved_errno;
}

static int add_listener(int efd, int fd, int ch_id)
{
    struct epoll_event ev = {.events = EPOLLIN, .data.u32 = ch_id};

    if (epoll_ctl(efd, EPOLL_CTL_ADD, fd, &ev) < 0)
        return -1;

    return 0;
}

static int lim_init_network(int checkMode)
{
    lim_init_chans();
    lim_create_epoll();
    lim_create_clients();

    if (add_listener(efd, chan_sock(lim_udp_chan), lim_udp_chan) < 0) {
        syslog(LOG_ERR, "Failed to add UDP listener: %m");
        goto cleanup;
    }

    if (add_listener(efd, chan_sock(lim_tcp_chan), lim_tcp_chan) < 0) {
        LS_ERR("%s: Failed to add TCP listener: %m", __func__);
        goto cleanup;
    }

    if (add_listener(efd, chan_sock(lim_timer_chan), lim_timer_chan) < 0) {
        LS_ERR("%s: Failed to add timer: %m", __func__);
        goto cleanup;
    }

    return 0;

cleanup:
    chan_close(lim_udp_chan);
    chan_close(lim_tcp_chan);
    close(lim_timer_chan);
    return -1;
}

static int lim_init_chans(void)
{
    struct sockaddr_in lim_addr;

    lim_udp_port = atoi(genParams[LSF_LIM_PORT].paramValue);
    if (lim_udp_port <= 0) {
        syslog(LOG_ERR, "%s: LSF_LIM_PORT <%s> must be a positive number",
               __func__, genParams[LSF_LIM_PORT].paramValue);
        return -1;
    }
    // make sure you called chan_init() before

    // LIM UDP channel
    lim_udp_chan = chan_listen_socket(SOCK_DGRAM, lim_udp_port, -1, 0);
    if (lim_udp_chan < 0) {
        syslog(LOG_ERR,
               "%s: unable to create datagram socket port %d "
               "another LIM running?: %m ",
               __func__, lim_udp_port);
        return -1;
    }

    // LIM TCP socket with
    lim_tcp_chan =
        chan_listen_socket(SOCK_STREAM, 0, SOMAXCONN, CHAN_OP_SOREUSE);
    if (lim_tcp_chan < 0) {
        syslog(LOG_ERR,
               "%s: unable to create tcp socket port %d "
               "another LIM running?: %m ",
               __func__, lim_udp_port);
        chan_close(lim_tcp_chan);
        chan_close(lim_udp_chan);
        return -1;
    }

    socklen_t size = sizeof(struct sockaddr_in);
    int cc = getsockname(chan_sock(lim_tcp_chan), (struct sockaddr *) &lim_addr,
                         &size);
    if (cc < 0) {
        syslog(LOG_ERR, "%s: getsocknamed(%d) failed: %m", __func__,
               lim_tcp_chan);
        chan_close(lim_tcp_chan);
        chan_close(lim_tcp_chan);
        return -1;
    }

    // LIM dynamic TCP port sent to slave lims and library which need
    // to find the master lim
    lim_tcp_port = lim_addr.sin_port;

    // This is not a channel, just a fake name for the time
    // that goes in the data structure
    lim_timer_chan = chan_create_timer(5);
    if (lim_timer_chan < 0) {
        chan_close(lim_udp_chan);
        chan_close(lim_tcp_chan);
        return -1;
    }

    return 0;
}

static int lim_create_epoll(void)
{
    // epoll file descriptor
    efd = epoll_create1(0);
    if (efd < 0) {
        LS_ERR("%s: epoll_create1() failed: %m", __func__);
        chan_close(lim_tcp_chan);
        chan_close(lim_tcp_chan);
        return -1;
    }

    // The global array of epoll_event
    lim_events = calloc(chan_open_max, sizeof(struct epoll_event));
    if (lim_events == NULL) {
        LS_ERR("%s: calloc failed %m", __func__);
        chan_close(lim_tcp_chan);
        chan_close(lim_tcp_chan);
        return -1;
    }

    return 0;
}

static int lim_create_clients(void)
{
    // Init the array of all clients as the same size
    // like the channels array
    client_map = calloc(chan_open_max, sizeof(struct client_node *));

    return 0;
}

void errorBack(struct sockaddr_in *from, struct packet_header *reqHdr,
               enum limReplyCode replyCode, int chan)
{
    char buf[LL_BUFSIZ_64];
    struct packet_header replyHdr;
    XDR xdrs2;
    int cc;

    init_pack_hdr(&replyHdr);
    replyHdr.operation = (short) replyCode;
    replyHdr.sequence = reqHdr->sequence;
    replyHdr.length = 0;

    xdrmem_create(&xdrs2, buf, MSGSIZE / 4, XDR_ENCODE);
    if (!xdr_pack_hdr(&xdrs2, &replyHdr)) {
        ls_syslog(LOG_ERR, "%s: xdr_pack() failed: %m", __func__);
        xdr_destroy(&xdrs2);
        return;
    }

    if (chan < 0)
        cc = chan_send_dgram(lim_udp_chan, buf, XDR_GETPOS(&xdrs2), from);
    else
        cc = chan_write(chan, buf, XDR_GETPOS(&xdrs2));

    if (cc < 0)
        ls_syslog(LOG_ERR, "%s: socket write failed: %m", __func__);

    xdr_destroy(&xdrs2);
    return;
}
static struct tclLsInfo *getTclLsInfo(void)
{
    static char fname[] = "getTclLsInfo";
    static struct tclLsInfo *tclLsInfo;
    int i;

    if ((tclLsInfo = (struct tclLsInfo *) malloc(sizeof(struct tclLsInfo))) ==
        NULL) {
        ls_syslog(LOG_ERR, "%s: %s failed: %m", fname, "malloc");
        return NULL;
    }

    if ((tclLsInfo->indexNames =
             (char **) malloc(allInfo.numIndx * sizeof(char *))) == NULL) {
        ls_syslog(LOG_ERR, "%s: %s failed: %m", fname, "malloc");
        return NULL;
    }
    for (i = 0; i < allInfo.numIndx; i++) {
        tclLsInfo->indexNames[i] = allInfo.resTable[i].name;
    }
    tclLsInfo->numIndx = allInfo.numIndx;
    tclLsInfo->nRes = shortInfo.nRes;
    tclLsInfo->resName = shortInfo.resName;
    tclLsInfo->stringResBitMaps = shortInfo.stringResBitMaps;
    tclLsInfo->numericResBitMaps = shortInfo.numericResBitMaps;

    return tclLsInfo;
}

static void startPIM(int argc, char **argv)
{
    int i;
    static time_t lastTime = 0;

    if (time(NULL) - lastTime < 60 * 2)
        return;

    lastTime = time(NULL);

    if ((pimPid = fork())) {
        if (pimPid < 0)
            syslog(LOG_ERR, "%s: failed %m", __func__);
        return;
    }

    for (i = sysconf(_SC_OPEN_MAX); i >= 0; i--)
        close(i);

    for (i = 1; i < NSIG; i++)
        signal_set(i, SIG_DFL);

    char daemonPath[PATH_MAX];
    snprintf(daemonPath, sizeof(daemonPath), "%s/pim",
             genParams[LSF_SERVERDIR].paramValue);
    char *pargv[] = {daemonPath, NULL};

    execv(pargv[0], pargv);
    syslog(LOG_ERR, "%s: failed %m", __func__);
    exit(-1);
}

void initLiStruct(void)
{
    if (!li) {
        li_len = 16;

        li = (struct liStruct *) malloc(sizeof(struct liStruct) * li_len);
    }

    li[0].name = "R15S";
    li[0].increasing = 1;
    li[0].delta[0] = 0.30;
    li[0].delta[1] = 0.10;
    li[0].extraload[0] = 0.20;
    li[0].extraload[1] = 0.40;
    li[0].valuesent = 0.0;
    li[0].exchthreshold = 0.25;
    li[0].sigdiff = 0.10;

    li[1].name = "R1M";
    li[1].increasing = 1;
    li[1].delta[0] = 0.15;
    li[1].delta[1] = 0.10;
    li[1].extraload[0] = 0.20;
    li[1].extraload[1] = 0.40;
    li[1].valuesent = 0.0;
    li[1].exchthreshold = 0.25;
    li[1].sigdiff = 0.10;

    li[2].name = "R15M";
    li[2].increasing = 1;
    li[2].delta[0] = 0.15;
    li[2].delta[1] = 0.10;
    li[2].extraload[0] = 0.20;
    li[2].extraload[1] = 0.40;
    li[2].valuesent = 0.0;
    li[2].exchthreshold = 0.25;
    li[2].sigdiff = 0.10;

    li[3].name = "UT";
    li[3].increasing = 1;
    li[3].delta[0] = 1.00;
    li[3].delta[1] = 1.00;
    li[3].extraload[0] = 0.10;
    li[3].extraload[1] = 0.20;
    li[3].valuesent = 0.0;
    li[3].exchthreshold = 0.15;
    li[3].sigdiff = 0.10;

    li[4].name = "PG";
    li[4].increasing = 1;
    li[4].delta[0] = 2.5;
    li[4].delta[1] = 1.5;
    li[4].extraload[0] = 0.8;
    li[4].extraload[1] = 1.5;
    li[4].valuesent = 0.0;
    li[4].exchthreshold = 1.0;
    li[4].sigdiff = 5.0;

    li[5].name = "IO";
    li[5].increasing = 1;
    li[5].delta[0] = 80;
    li[5].delta[1] = 40;
    li[5].extraload[0] = 15;
    li[5].extraload[1] = 25.0;
    li[5].valuesent = 0.0;
    li[5].exchthreshold = 25.0;
    li[5].sigdiff = 5.0;

    li[6].name = "LS";
    li[6].increasing = 1;
    li[6].delta[0] = 3;
    li[6].delta[1] = 3;
    li[6].extraload[0] = 0;
    li[6].extraload[1] = 0;
    li[6].valuesent = 0.0;
    li[6].exchthreshold = 0.0;
    li[6].sigdiff = 1.0;

    li[7].name = "IT";
    li[7].increasing = 0;
    li[7].delta[0] = 6000;
    li[7].delta[1] = 6000;
    li[7].extraload[0] = 0;
    li[7].extraload[1] = 0;
    li[7].valuesent = 0.0;
    li[7].exchthreshold = 1.0;
    li[7].sigdiff = 5.0;

    li[8].name = "TMP";
    li[8].increasing = 0;
    li[8].delta[0] = 2;
    li[8].delta[1] = 2;
    li[8].extraload[0] = -0.2;
    li[8].extraload[1] = -0.5;
    li[8].valuesent = 0.0;
    li[8].exchthreshold = 1.0;
    li[8].sigdiff = 2.0;

    li[9].name = "SMP";
    li[9].increasing = 0;
    li[9].delta[0] = 10;
    li[9].delta[1] = 10;
    li[9].extraload[0] = -0.5;
    li[9].extraload[1] = -1.5;
    li[9].valuesent = 0.0;
    li[9].exchthreshold = 1.0;
    li[9].sigdiff = 2.0;

    li[10].name = "MEM";
    li[10].increasing = 0;
    li[10].delta[0] = 9000;
    li[10].delta[1] = 9000;
    li[10].extraload[0] = -0.5;
    li[10].extraload[1] = -1.0;
    li[10].valuesent = 0.0;
    li[10].exchthreshold = 1.0;
    li[10].sigdiff = 3.0;
}

static void initMiscLiStruct(void)
{
    int i;

    extraload = (float *) malloc(allInfo.numIndx * sizeof(float));
    memset((char *) extraload, 0, allInfo.numIndx * sizeof(float));
    li = (struct liStruct *) realloc(li,
                                     sizeof(struct liStruct) * allInfo.numIndx);
    li_len = allInfo.numIndx;
    for (i = NBUILTINDEX; i < allInfo.numIndx; i++) {
        li[i].delta[0] = 9000;
        li[i].delta[1] = 9000;
        li[i].extraload[0] = 0;
        li[i].extraload[1] = 0;
        li[i].valuesent = 0.0;
        li[i].exchthreshold = 0.0001;
        li[i].sigdiff = 0.0001;
    }
}

void shutdown_client(struct client_node *client)
{
    if (!client)
        return;

    // Remove ef from epoll listening socket
    epoll_ctl(efd, EPOLL_CTL_DEL, chan_sock(client->ch_id), NULL);

    // Close channel
    chan_close(client->ch_id);

    // Remove from client map
    client_map[client->ch_id] = NULL;

    // Free client
    free(client);
}
