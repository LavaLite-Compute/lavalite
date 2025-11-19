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
int lim_udp_sock = -1;
int lim_tcp_sock = -1;
ushort lim_udp_port;
ushort lim_tcp_port;
// BUG use a list
long max_clients;
struct client_node **clientMap;

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
extern int chanIndex;

static void lim_init(int);
static void term_handler(int);
static void child_handler(int);
static void usage(const char *);
static void initSignals(void);
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
                   genParams[LSF_LOG_MASK].paramValue);
        ls_syslog(LOG_ERR, "lim: initenv_ %s", env_dir);
        open_log("lim", genParams[LSF_LOG_MASK].paramValue, true);
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

    getLogClass_(genParams[LSF_DEBUG_LIM].paramValue,
                 genParams[LSF_TIME_LIM].paramValue);

    if (lim_debug) {
        ls_openlog("lim", genParams[LSF_LOGDIR].paramValue, true, "LOG_DEBUG");
        open_log("lim", genParams[LSF_LOG_MASK].paramValue, true);
    } else {
        ls_openlog("lim", genParams[LSF_LOGDIR].paramValue, false,
                   genParams[LSF_LOG_MASK].paramValue);
        open_log("lim", genParams[LSF_LOG_MASK].paramValue, false);
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
    initSignals();

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

    // This is the 5 seconds counter to next periodic
    uint64_t tick_ms = 5000;
    uint64_t next_tick = now_ms() + tick_ms;

    struct Masks sockmask;
    struct Masks chanmask;
    fd_set allMask;
    FD_ZERO(&allMask);

    for (;;) {
        sockmask.rmask = allMask;

        if (pimPid == -1) {
            startPIM(argc, argv);
        }

        // select timeout ever 2 seconds if no network io
        struct timeval timeout = {.tv_sec = 2, .tv_usec = 0};

        int nfiles = chanSelect_(&sockmask, &chanmask, &timeout);
        if (nfiles < 0) {
            if (errno != EINTR) {
                syslog(LOG_ERR, "%s: network I/O %m", __func__);
            }
        }

        uint64_t now = now_ms();
        if (now >= next_tick) {
            periodic();
            next_tick += tick_ms;
        }

        if (nfiles <= 0)
            continue;

        if (FD_ISSET(lim_udp_sock, &chanmask.rmask)) {
            cc = process_udp_request();
            if (cc < 0) {
                syslog(LOG_ERR, "%s: process_udp_request() failed: %m",
                       __func__);
            }
        }

        if (FD_ISSET(lim_tcp_sock, &chanmask.rmask)) {
            accept_connection();
        }

        handle_tcp_client(&chanmask);
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
    int cc = chanRcvDgram_(lim_udp_sock, buf, sizeof(buf),
                           (struct sockaddr_storage *) &from, -1);
    if (cc < 0) {
        syslog(LOG_ERR,
               "%s: Error receiving data on lim_udp_sock %d, cc=%d: %m",
               __func__, lim_udp_sock, cc);
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
        clusNameReq(&xdrs, &from, &reqHdr);
        break;
    case LIM_GET_MASTINFO:
        masterInfoReq(&xdrs, &from, &reqHdr);
        break;
    case LIM_SERV_AVAIL:
        servAvailReq(&xdrs, node, &from, &reqHdr);
        break;
    case LIM_MASTER_ANN:
        masterRegister(&xdrs, &from, &reqHdr);
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

    int ch = chanAccept_(lim_tcp_sock, &from);
    if (ch < 0) {
        ls_syslog(LOG_ERR, "%s: chanAccept_() failed: %m", __func__);
        return -1;
    }

    struct ll_host hs;
    get_host_by_sockaddr_in(&from, &hs);
    if (hs.name[0] == 0) {
        ls_syslog(LOG_ERR, "%s: unknown host from %s dropped", __func__,
                  sockAdd2Str_(&from));
        chanClose_(ch);
        return -1;
    }

    host = find_node_by_sockaddr_in(&from);
    if (host == NULL) {
        ls_syslog(LOG_WARNING, "\
%s: Received request from non-LSF host %s",
                  __func__, sockAdd2Str_(&from));
        chanClose_(ch);
        return -1;
    }

    struct client_node *client = calloc(1, sizeof(struct client_node));
    if (!client) {
        ls_syslog(LOG_ERR, "%s: Connection from %s dropped", __func__,
                  sockAdd2Str_(&from));
        chanClose_(ch);
        return -1;
    }
    client->chanfd = ch;
    // Bug create a list
    clientMap[client->chanfd] = client;
    client->fromHost = host;
    client->from = from;
    client->clientMasks = 0;
    client->reqbuf = NULL;

    return 0;
}

static void initSignals(void)
{
    sigset_t mask;

    Signal_(SIGHUP, term_handler);
    Signal_(SIGINT, term_handler);
    Signal_(SIGTERM, term_handler);
    Signal_(SIGXCPU, term_handler);
    Signal_(SIGXFSZ, term_handler);
    Signal_(SIGPROF, term_handler);
    Signal_(SIGPWR, term_handler);
    Signal_(SIGUSR1, term_handler);
    Signal_(SIGUSR2, term_handler);
    Signal_(SIGCHLD, child_handler);
    Signal_(SIGPIPE, SIG_IGN);

    /* LIM is compiled with define NO_SIGALARM
     */
    Signal_(SIGALRM, SIG_IGN);

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

    if (getenv("RECONFIG_CHECK") == NULL)
        if (initSock(checkMode) < 0)
            lim_Exit("initSock");

    if (readCluster(checkMode) < 0)
        lim_Exit("readCluster");

    if (reCheckClass() < 0)
        lim_Exit("readCluster");

    if ((tclLsInfo = getTclLsInfo()) == NULL)
        lim_Exit("getTclLsInfo");

    if (initTcl(tclLsInfo) < 0)
        lim_Exit("initTcl");
    initParse(&allInfo);

    initReadLoad(checkMode);
    initTypeModel(myHostPtr);

    if (!checkMode) {
        initConfInfo();
        satIndex();
        loadIndex();
    }
    if (chanInit_() < 0)
        lim_Exit("chanInit_");

    max_clients = sysconf(_SC_OPEN_MAX);
    clientMap = calloc(1, sizeof(struct client_node **));
    *clientMap = calloc(max_clients, sizeof(struct client_node *));
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
    // syslog(LOG_DEBUG, "%s: Entering this routine...", __func__);

    TIMEIT(0, readLoad(), "readLoad()");

    if (masterMe) {
        announceMaster(myClusterPtr, 1, false);
    }

    if (0)
        checkHostWd();
}

// Set an atomic variable to signal the exit
// do nohing else in the handler
static void term_handler(int signum)
{
    Signal_(signum, SIG_DFL);

    ls_syslog(LOG_ERR, "%s: Received signal %d, exiting", __func__, signum);

    chanClose_(lim_udp_sock);
    chanClose_(lim_tcp_sock);

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

int initSock(int checkMode)
{
    struct sockaddr_in lim_addr;

    lim_udp_port = atoi(genParams[LSF_LIM_PORT].paramValue);
    if (lim_udp_port <= 0) {
        syslog(LOG_ERR, "%s: LSF_LIM_PORT <%s> must be a positive number",
               __func__, genParams[LSF_LIM_PORT].paramValue);
        return -1;
    }

    // LIM UDP channel
    lim_udp_sock = chanServSocket_(SOCK_DGRAM, lim_udp_port, -1, 0);
    if (lim_udp_sock < 0) {
        syslog(LOG_ERR,
               "%s: unable to create datagram socket port %d "
               "another LIM running?: %m ",
               __func__, lim_udp_port);
        return -1;
    }
    lim_udp_port = htons(lim_udp_port);

    // LIM TCP socket with
    lim_tcp_sock = chanServSocket_(SOCK_STREAM, 0, SOMAXCONN, 0);
    if (lim_tcp_sock < 0) {
        syslog(LOG_ERR,
               "%s: unable to create tcp socket port %d "
               "another LIM running?: %m ",
               __func__, lim_udp_port);
        chanClose_(lim_tcp_sock);
        return -1;
    }

    socklen_t size = sizeof(struct sockaddr_in);
    int cc = getsockname(chanSock_(lim_tcp_sock), (struct sockaddr *) &lim_addr,
                         &size);
    if (cc < 0) {
        syslog(LOG_ERR, "%s: getsocknamed(%d) failed: %m", __func__,
               lim_tcp_sock);
        chanClose_(lim_tcp_sock);
        return -1;
    }
    // LIM dynamic TCP port sent to slave lims and library which need
    // to find the master lim
    lim_tcp_port = lim_addr.sin_port;

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
        cc = chanSendDgram_(lim_udp_sock, buf, XDR_GETPOS(&xdrs2), from);
    else
        cc = chanWrite_(chan, buf, XDR_GETPOS(&xdrs2));

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
        Signal_(i, SIG_DFL);

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
