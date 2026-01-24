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
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 *
 */

#include "lsbatch/daemons/mbd.h"

#define POLL_INTERVAL MAX(msleeptime / 10, 1)

char errbuf[MAXLINELEN];

bool_t mbd_debug = 0;
int lsb_CheckMode = 0;
int lsb_CheckError = 0;

#define MAX_THRNUM 3000

time_t lastForkTime;
int statusChanged = 0;

int nextJobId = 1;
char masterme = TRUE;
uint16_t sbd_port;
int connTimeout;
int glMigToPendFlag = FALSE;

int requeueToBottom = FALSE;
int arraySchedOrder = FALSE;
int jobTerminateInterval = DEF_JTERMINATE_INTERVAL;
int msleeptime = DEF_MSLEEPTIME;
int sbdSleepTime = DEF_SSLEEPTIME;
int preemPeriod = DEF_PREEM_PERIOD;
int pgSuspIdleT = DEF_PG_SUSP_IT;
int rusageUpdateRate = DEF_RUSAGE_UPDATE_RATE;
int rusageUpdatePercent = DEF_RUSAGE_UPDATE_PERCENT;
int clean_period = DEF_CLEAN_PERIOD;
int max_retry = DEF_MAX_RETRY;
int retryIntvl = DEF_RETRY_INTVL;
int max_sbdFail = DEF_MAXSBD_FAIL;
int sendEvMail = 0;
int maxJobId = DEF_MAX_JOBID;

int maxJobArraySize = DEF_JOB_ARRAY_SIZE;
int jobRunTimes = INFINIT_INT;
int jobDepLastSub = 0;
int maxjobnum = DEF_MAX_JOB_NUM;
int accept_intvl = DEF_ACCEPT_INTVL;
int preExecDelay = DEF_PRE_EXEC_DELAY;
int slotResourceReserve = FALSE;
int maxAcctArchiveNum = -1;
int acctArchiveInDays = -1;
int acctArchiveInSize = -1;
int numofqueues = 0;
int numofhosts = 0;
int numofprocs = 0;
int numofusers = 0;
int numofugroups = 0;
int numofhgroups = 0;
int mSchedStage = 0;
int maxSchedStay = DEF_SCHED_STAY;
int freshPeriod = DEF_FRESH_PERIOD;
int qAttributes = 0;
int **hReasonTb = NULL;
int **cReasonTb = NULL;
time_t now;
long schedSeqNo = 0;
struct hData **hDataPtrTb = NULL;
UDATA_TABLE_T *uDataPtrTb;
struct hTab uDataList;
struct hTab hDataList;

struct qData *qDataList = NULL;
struct jData *jDataList[ALLJLIST];
struct jData *chkJList;

struct hTab cpuFactors;
struct gData *usergroups[MAX_GROUPS];
struct gData *hostgroups[MAX_GROUPS];
static struct mbd_client_node *clientList;

struct lsInfo *allLsInfo;
struct hTab calDataList;
struct hTab condDataList;

char *masterHost = NULL;
char *clusterName = NULL;
char *defaultQueues = NULL;
char *defaultHostSpec = NULL;
char *env_dir = NULL;
char *lsfDefaultProject = NULL;
char *pjobSpoolDir = NULL;
time_t condCheckTime = DEF_COND_CHECK_TIME;
bool_t mcSpanClusters = FALSE;
int readNumber = 0;
int dispatch = FALSE;
int maxJobPerSession = INFINIT_INT;

int maxUserPriority = -1;
int jobPriorityValue = -1;
int jobPriorityTime = -1;
static int jobPriorityUpdIntvl = -1;

int nSbdConnections = 0;
int maxSbdConnections = DEF_MAX_SBD_CONNS;
int numResources = 0;
struct hostInfo *host_list = NULL;
int host_count = 0;

float maxCpuFactor = 0.0;
struct sharedResource **sharedResources = NULL;

int sharedResourceUpdFactor = INFINIT_INT;

long schedSeqNo;

int schedule;
int scheRawLoad;

int lsbModifyAllJobs = FALSE;

static int schedule1;
static struct jData *jobData = NULL;
static time_t lastSchedTime = 0;
static time_t nextSchedTime = 0;

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
static int mbd_accept_connection(int);
static void mbd_handle_client(int);
static int mbd_dispatch_client(struct mbd_client_node *);
static int mbd_auth_client_request(struct lsfAuth *, XDR *,
                                   struct packet_header *, struct sockaddr_in *);
static bool_t mbd_should_fork(mbdReqType);
static bool_t is_mbd_read_only_req(mbdReqType);

void setJobPriUpdIntvl(void);
static void updateJobPriorityInPJL(void);
static void houseKeeping(int *);
static void periodicCheck(void);
static void processSbdNode(struct sbdNode *, int);

extern int do_chunkStatusReq(XDR *, int, struct sockaddr_in *, int *,
                             struct packet_header *);
extern int do_setJobAttr(XDR *, int, struct sockaddr_in *, char *,
                         struct packet_header *, struct lsfAuth *);


extern void chanCloseAllBut_(int);
extern int initLimSock_(void);

static void usage(void)
{
    fprintf(stderr,
            "Usage: mbatchd [options]\n"
            "Options:\n"
            "  -h, --help         Show this help message and exit\n"
            "  -V, --version      Show version information and exit\n"
            "  -C, --reconfig     Check configuration\n"
            "  -d, --debug        Run in debug mode, no daemonize\n"
            "  -E, --env VAR      Set environment variable LSF_ENVDIR\n");
}
static struct option longopts[] = {{"help", no_argument, NULL, 'h'},
                                   {"version", no_argument, NULL, 'V'},
                                   {"debug", no_argument, NULL, 'd'},
                                   {"envdir", required_argument, NULL, 'E'},
                                   {"reconfig", no_argument, NULL, 'C'},
                                   {NULL, 0, NULL, 0}};

int main(int argc, char **argv)
{
    int i;
    int hsKeeping = FALSE;
    time_t lastPeriodicCheckTime = 0;
    time_t lastElockTouch;

    int cc;
    while ((cc = getopt_long(argc, argv, "hVCdE:", longopts, NULL)) != EOF) {
        switch (cc) {
        case 'd':
            mbd_debug = true;
            break;
        case 'E':
            putEnv("LSF_ENVDIR", optarg);
            break;
        case 'C':
            putEnv("RECONFIG_CHECK", "YES");
            fputs("\n", stderr);
            lsb_CheckMode = 1;
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

    if (initenv_(lsbParams, env_dir) < 0) {
        // That's the most we can do...
        ls_openlog("mbatchd", "/var/log/lavalite", true, 0, "LOG_ERR");
        LS_ERR("mbatchd initenv failed, "
               "Bad configuration environment, missing in lsf.conf?");
        return -1;
    }

    // init log
    mbd_init_log();
    // mbd don't want root
    mbd_check_not_root();

    if (lsb_CheckMode == TRUE) {
        TIMEIT(0, (cc = mbd_init(FIRST_START)), "mbd_init");
        if (cc < 0) {
            LS_ERR("mbd_init() failed");
            mbdDie(MASTER_FATAL);
        }
    }

    if (gethostname(mbd_host, sizeof(mbd_host)) < 0) {
        LS_ERR("gethostname failed: %m");
        mbdDie(MASTER_FATAL);
    }
    // Bug for now point to the static buffer
    masterHost = mbd_host;

    LS_INFO("mbatchd starting as master on host <%s>", mbd_host);

    if (lsb_CheckMode) {
        ls_syslog(LOG_INFO, ("Checking Done"));
        exit(lsb_CheckError);
    }

    TIMEIT(0, (cc = mbd_init(FIRST_START)), "mbd_init");
    if (cc < 0) {
        syslog(LOG_ERR, "%s: mbd_init() failed %m", __func__);
        mbdDie(MASTER_FATAL);
    }

    log_mbdStart();
    ls_syslog(LOG_INFO, ("%s: re-started"), "main");

    if ((clientList = (struct mbd_client_node *) listCreate("")) == NULL) {
        ls_syslog(LOG_ERR, "%s", __func__, "listCreate");
        mbdDie(MASTER_FATAL);
    }

    pollSbatchds(FIRST_START);
    lastSchedTime = 0;
    nextSchedTime = time(0) + msleeptime;
    lastElockTouch = time(0) - msleeptime;
    schedulerInit();
    setJobPriUpdIntvl();

    for (;;) {
        int nevents;

        time_t now = time(NULL);
        if ((now - lastElockTouch) >= msleeptime) {
            // create a timer fd
            touchElogLock();
            lastElockTouch = now;
        }
        // Schedule now
        scheduleAndDispatchJobs();

        int mbd_timeout = 10 * 1000;
        nevents = chan_epoll(mbd_efd, mbd_events, mbd_max_events,
                             mbd_timeout);
        if (nevents < 0) {
              if (errno == EINTR)
                  continue;
              LS_ERR("chan_epoll(%d) failed", mbd_efd);
              continue;
        }
        for (i = 0; i < nevents; i++) {
            struct epoll_event *ev = &mbd_events[i];
            int ch_id = (int)ev->data.u32;

            //houseKeeping(&hsKeeping);
            //periodicCheck();
             LS_DEBUG("epoll: ch_id=%d chan_events=%d kernel_events 0x%x",
                      ch_id, channels[ch_id].chan_events, ev->events);

             // True skip partually read channels
             if (channels[ch_id].chan_events == CHAN_EPOLLNONE)
                 continue;

            if (ch_id == mbd_chan) {
                mbd_accept_connection(mbd_chan);
                continue;
            }

            // chan_epoll() must call doread() for EPOLLIN,
            // which updates channels[ch_id].chan_events
            if (channels[ch_id].chan_events == CHAN_EPOLLIN
                || channels[ch_id].chan_events == CHAN_EPOLLERR) {
                mbd_handle_client(ch_id);
                // we have consumed the packet
                channels[ch_id].chan_events = CHAN_EPOLLNONE;
            }

        }
    }
}

static int mbd_accept_connection(int socket)
{
    struct sockaddr_in from;

    int ch_id = chan_accept(socket, (struct sockaddr_in *)&from);
    if (ch_id < 0) {
        LS_ERR("chan_accept() failed");
        return -1;
    }
    struct ll_host hs;
    memset(&hs, 0, sizeof(struct ll_host));
    get_host_by_sockaddr_in(&from, &hs);
    if (hs.name[0] == 0) {
        ls_syslog(LOG_WARNING, "%s: request from unknown host %s:", __func__,
                  sockAdd2Str_(&from));
        errorBack(ch_id, LSBE_PERMISSION, &from);
        chan_close(ch_id);
        return -1;
    }

    // Make sure the new socket is accepted by chan_epoll
    struct epoll_event ev = {.events = EPOLLIN, .data.u32 = ch_id};

    int cc = epoll_ctl(mbd_efd, EPOLL_CTL_ADD, chan_sock(ch_id), &ev);
    if (cc < 0) {
        LS_ERR("epoll_ctl() failed");
        chan_close(ch_id);
        return -1;
    }

    struct mbd_client_node *client;
    client = calloc(1, sizeof(struct mbd_client_node));
    client->chanfd = ch_id;
    memcpy(&client->host, &hs, sizeof(struct ll_host));
    client->reqType = 0;
    client->lastTime = 0;

    inList((struct listEntry *)clientList, (struct listEntry *)client);

    LS_DEBUG("accepted connection from: %s on channel: %d",
             sockAdd2Str_(&from), ch_id);
    return ch_id;
}

static void mbd_handle_client(int ch_id)
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
    // sbd we have to handle it inside this calle because beside
    // just shutting down the client we have to do other operations
    // like clean up caches, jobs states etc
    // so the asymmetry is intentional and correct.
    if (host_data) {
        // handle the sbd client
        LS_DEBUG("the client is an sbd %s", host_data->host);
        // dispatch the payload decoder
        mbd_dispatch_sbd(host_data->sbd_node);
        return;
    }

    for (struct mbd_client_node *client = clientList->forw;
         client != clientList;
         client = client->forw) {

        if (client->chanfd != ch_id)
            continue;

        LS_DEBUG("found the client %p ch_id=%d sock=%d events=0x%x",
                 client, ch_id, channels[ch_id].sock, channels[ch_id].chan_events);

        // This is an ordinary client so we can just shut him down
        if (channels[ch_id].chan_events == CHAN_EPOLLERR) {
            LS_DEBUG("the client is a being shutdown");
            shutdown_mbd_client(client);
            return;
        }

        // Now dispatch the current client request,
        mbd_dispatch_client(client);
        return;
    }

    // No client found for this channel id: internal inconsistency.
    LS_ERR("mbd_handle_client: no client found for chanfd=%d", ch_id);
}

static int mbd_dispatch_client(struct mbd_client_node *client)
{
    struct Buffer *buf;
    struct bucket *bucket;
    struct lsfAuth auth;
    struct packet_header req_hdr;
    XDR xdrs;
    int statusReqCC = 0;

    memset(&auth, 0, sizeof(auth));
    int ch_id = client->chanfd;

    if (chan_dequeue(client->chanfd, &buf) < 0) {
        ls_syslog(LOG_ERR, "%s: chan_dequeue failed :%m", __func__);
        shutdown_mbd_client(client);
        return -1;
    }

    xdrmem_create(&xdrs, buf->data, buf->len, XDR_DECODE);
    if (!xdr_pack_hdr(&xdrs, &req_hdr)) {
        ls_syslog(LOG_ERR, "%s", __func__, "xdr_pack_hdr");
        xdr_destroy(&xdrs);
        chan_free_buf(buf);
        shutdown_mbd_client(client);
        return -1;
    }

    struct sockaddr_in from;
    get_host_addrv4(&client->host, &from);

    //    if (logclass & (LC_COMM | LC_TRACE)) {
    if (1) {
        LS_DEBUG("Received request %d from %s channel <%d>",
                 req_hdr.operation, sockAdd2Str_(&from), ch_id);
    }

    // Bug write a function
    bool_t ok = 1;

    switch (ok) {
    case -1:
        LS_WARNING("Request from non-LSF host %s", sockAdd2Str_(&from));
        chan_close(ch_id);
        goto endLoop;
    default:
        break;
    }

    int cc = mbd_auth_client_request(&auth, &xdrs, &req_hdr, &from);
    if (cc != LSBE_NO_ERROR) {
        //Log locally — do not reveal anything to the attacker
        LS_WARNING("authentication failed (%d) from %s", cc,
                   client->host.name);

        // Immediately terminate the connection — NO protocol reply
        chan_close(ch_id);

        goto endLoop;
    }

    // Bug we avoid fork for now for easy debug
    if (mbd_should_fork(req_hdr.operation)) {
        pid_t pid;

        pid = fork();
        if (pid < 0) {
            LS_ERR("fork() failed");
            errorBack(ch_id, LSBE_NO_FORK, &from);
        }
        // parent
        if (pid != 0) {
            goto endLoop;
        }

        // we dont fork for now
        //if (mbd_debug)
            // closeExceptFD(chan_sock(ch_id));
    }

    switch (req_hdr.operation) {

    case BATCH_JOB_SUB:
        jobData = NULL;
        TIMEIT(0,
               do_submitReq(&xdrs, ch_id, &from, NULL, &req_hdr, NULL,
                            &auth, &schedule1, dispatch, &jobData),
               "do_submitReq()");
        break;
    case BATCH_JOB_SIG:
        cc = mbd_handle_signal_req(&xdrs, client, &req_hdr, &auth);
        xdr_destroy(&xdrs);
        chan_free_buf(buf);
        if (cc < 0) {
            mbd_sbd_disconnect(client);
        }
        return 0;
        break;
    case BATCH_JOB_MSG:
        NEW_BUCKET(bucket, buf);
        if (bucket) {
            TIMEIT(0,
                   do_jobMsg(bucket, &xdrs, ch_id, &from, NULL, &req_hdr,
                             &auth),
                   "do_jobMsg()");
        } else {
            ls_syslog(LOG_ERR, "%s", __func__, "NEW_BUCKET");
        }
        break;
    case BATCH_QUE_CTRL:
        TIMEIT(0,
               do_queueControlReq(&xdrs, ch_id, &from, NULL, &req_hdr,
                                  &auth),
               "do_queueControlReq()");
        break;
    case BATCH_RECONFIG:
        TIMEIT(0, do_reconfigReq(&xdrs, ch_id, &from, NULL, &req_hdr),
               "do_reconfigReq()");
        break;
    case BATCH_JOB_MIG:
        TIMEIT(0, do_migReq(&xdrs, ch_id, &from, NULL, &req_hdr, &auth),
               "do_migReq()");
        break;
    case BATCH_STATUS_MSG_ACK:
    case BATCH_STATUS_JOB:
    case BATCH_RUSAGE_JOB:
        TIMEIT(
            0,
            (statusReqCC = do_statusReq(&xdrs, ch_id, &from, &schedule1, &req_hdr)),
            "do_statusReq()");
        if (client->lastTime == 0)
            nSbdConnections++;
        break;
    case BATCH_STATUS_CHUNK:
        TIMEIT(0,
               (statusReqCC =
                    do_chunkStatusReq(&xdrs, ch_id, &from, &schedule1, &req_hdr)),
               "do_chunkStatusReq()");
        if (client->lastTime == 0)
            nSbdConnections++;
        break;
    case BATCH_SLAVE_RESTART:
        TIMEIT(0, do_restartReq(&xdrs, ch_id, &from, &req_hdr), "do_restartReq()");
        break;
    case BATCH_HOST_CTRL:
        TIMEIT(0,
               do_hostControlReq(&xdrs, ch_id, &from, NULL, &req_hdr,
                                 &auth),
               "do_hostControlReq()");
        break;
    case BATCH_JOB_SWITCH:
        TIMEIT(
            3,
            do_jobSwitchReq(&xdrs, ch_id, &from, NULL, &req_hdr, &auth),
            "do_jobSwitchReq()");
        break;
    case BATCH_JOB_MOVE:
        TIMEIT(3,
               do_jobMoveReq(&xdrs, ch_id, &from, NULL, &req_hdr, &auth),
               "do_jobMoveReq()");
        break;
    case BATCH_SET_JOB_ATTR:
        do_setJobAttr(&xdrs, ch_id, &from, NULL, &req_hdr, &auth);
        break;
    case BATCH_JOB_MODIFY:
        TIMEIT(3,
               do_modifyReq(&xdrs, ch_id, &from, NULL, &req_hdr, &auth),
               "do_modifyReq()");
        break;

    case BATCH_JOB_PEEK:
        TIMEIT(0,
               do_jobPeekReq(&xdrs, ch_id, &from, NULL, &req_hdr, &auth),
               "do_jobPeekReq()");
        break;
    case BATCH_USER_INFO:
        TIMEIT(0, do_userInfoReq(&xdrs, ch_id, &from, &req_hdr), "do_userInfoReq()");
        break;
    case BATCH_PARAM_INFO:
        TIMEIT(0, do_paramInfoReq(&xdrs, ch_id, &from, &req_hdr),
               "do_paramInfoReq()");
        break;
    case BATCH_GRP_INFO:
        TIMEIT(3, do_groupInfoReq(&xdrs, ch_id, &from, &req_hdr),
               "do_groupInfoReq()");
        break;
    case BATCH_QUE_INFO:
        TIMEIT(3, do_queueInfoReq(&xdrs, ch_id, &from, &req_hdr),
               "do_queueInfoReq()");
        break;
    case BATCH_JOB_INFO:
        TIMEIT(3, do_jobInfoReq(&xdrs, ch_id, &from, &req_hdr, schedule),
               "do_jobInfoReq()");
        break;
    case BATCH_HOST_INFO:
        TIMEIT(3, do_hostInfoReq(&xdrs, ch_id, &from, &req_hdr), "do_hostInfoReq()");
        break;
    case BATCH_RESOURCE_INFO:
        TIMEIT(3, do_resourceInfoReq(&xdrs, ch_id, &from, &req_hdr),
               "do_resourceInfoReq()");
        break;
    case BATCH_JOB_FORCE:
        TIMEIT(0, do_runJobReq(&xdrs, ch_id, &from, &auth, &req_hdr),
               "do_runJobReq()");
        break;
    case BATCH_SBD_REGISTER:
        int cc = mbd_sbd_register(&xdrs, client, &req_hdr);
        xdr_destroy(&xdrs);
        chan_free_buf(buf);
        if (cc < 0) {
            struct hData *host_data = client->host_node;
            shutdown_mbd_client(client);
            if (host_data)
                host_data->sbd_node = NULL;
        }
        return 0;
    default:
        // No error back to unkown client
        if (req_hdr.version <= CURRENT_PROTOCOL_VERSION)
            LS_ERR("Unknown request %d from %s", req_hdr.operation,
                   sockAdd2Str_(&from));
        break;
    }

    // Bug we avoid fork for now for easy debug
    if (mbd_should_fork(req_hdr.operation)) {
        chan_free_buf(buf);
        exit(0);
    }
endLoop:
    xdr_destroy(&xdrs);
    chan_free_buf(buf);
    shutdown_mbd_client(client);

    return 0;
}

void shutdown_mbd_client(struct mbd_client_node *client)
{
    if (! client)
        return;

    chan_close(client->chanfd);
    offList((struct listEntry *) client);
    free(client);
}

static void houseKeeping(int *hsKeeping)
{
    static char fname[] = "houseKeeping";
    static int resignal = FALSE;
    static time_t lastAcctSched = 0;

#define SCHED 1
#define DISPT 2
#define RESIG 3
#define T15MIN (60 * 15)
    static int myTurn = RESIG;

    if (logclass & (LC_TRACE | LC_SCHED)) {
        ls_syslog(LOG_DEBUG3,
                  "%s: mSchedStage=%x schedule=%d eventPending=%d now=%d "
                  "lastSchedTime=%d nextSchedTime=%d",
                  fname, mSchedStage, schedule, eventPending, (int) now,
                  (int) lastSchedTime, (int) nextSchedTime);
    }

    if (lastAcctSched == 0) {
        lastAcctSched = now;
    } else {
        if ((now - lastAcctSched) > T15MIN) {
            lastAcctSched = now;
            checkAcctLog();
        }
    }

    if (myTurn == RESIG)
        myTurn = SCHED;
    if (schedule && myTurn == SCHED) {
        if (eventPending) {
            resignal = TRUE;
        }
        now = time(0);
        if (schedule) {
            lastSchedTime = now;
            nextSchedTime = now + msleeptime;
            TIMEIT(0, schedule = scheduleAndDispatchJobs(),
                   "scheduleAndDispatchJobs");
            if (schedule == 0) {
                schedule = FALSE;
            } else {
                schedule = TRUE;
            }
            return;
        }
    }

    if (myTurn == SCHED)
        myTurn = RESIG;
    if (resignal && myTurn == RESIG) {
        RESET_CNT();
        TIMEIT(0, resigJobs(&resignal), "resigJobs()");
        DUMP_CNT();
        return;
    }

    *hsKeeping = FALSE;
    return;
}

static void periodicCheck(void)
{
    static char fname[] = "periodicCheck";
    char *myhostnm;
    static time_t last_chk_time = 0;
    static int winConf = FALSE;
    static time_t lastPollTime = 0, last_checkConf = 0;
    static time_t last_hostInfoRefreshTime = 0;
    static time_t last_tryControlJobs = 0;
    static time_t last_jobPriUpdTime = 0;

    if (logclass & (LC_TRACE))
        ls_syslog(LOG_DEBUG3, "%s: Entering this routine...", fname);

    if (last_chk_time == 0) {
        last_hostInfoRefreshTime = now;
    }

    switchELog();

    if (jobPriorityUpdIntvl > 0) {
        if (now - last_jobPriUpdTime >= jobPriorityUpdIntvl * 60) {
            TIMEIT(0, updateJobPriorityInPJL(), "updateJobPriorityInPJL()");
            last_jobPriUpdTime = now;
        }
    }

    if (now - lastPollTime > POLL_INTERVAL) {
        TIMEIT(0, pollSbatchds(NORMAL_RUN), "pollSbatchds()");
        lastPollTime = now;
    }

    if (now - last_chk_time > msleeptime) {
        masterHost = ls_getmastername();
        if (masterHost == NULL) {
            ls_syslog(LOG_ERR, "%s: Unable to contact LIM: %M; quit master",
                      fname);
            mbdDie(MASTER_RESIGN);
        }
        if ((myhostnm = ls_getmyhostname()) == NULL) {
            ls_syslog(LOG_ERR, "%s", __func__, "ls_getmyhostname");
            if (!lsb_CheckMode)
                mbdDie(MASTER_FATAL);
        }
        if (!equal_host(masterHost, myhostnm)) {
            masterHost = myhostnm;
            mbdDie(MASTER_RESIGN);
        }

        clean(now);
        checkQWindow();
        checkHWindow();

        TIMEIT(0, checkJgrpDep(), "checkJgrpDep");

        now = time(0);
        last_chk_time = now;
    }
    if (now - last_tryControlJobs > sbdSleepTime) {
        last_tryControlJobs = now;

        TIMEIT(0, tryResume(), "tryResume()");
    }
    if (now - last_hostInfoRefreshTime > 10 * 60) {
        getLsbHostInfo();
        last_hostInfoRefreshTime = now;
    }
}

void terminate_handler(int sig)
{
    sigset_t newmask, oldmask;

    sigemptyset(&newmask);
    sigaddset(&newmask, SIGTERM);
    sigaddset(&newmask, SIGINT);
    sigaddset(&newmask, SIGCHLD);
    sigprocmask(SIG_BLOCK, &newmask, &oldmask);

    exit(sig);
}

void child_handler(int sig)
{
    int pid;
    int status;
    sigset_t newmask, oldmask;

    sigemptyset(&newmask);
    sigaddset(&newmask, SIGTERM);
    sigaddset(&newmask, SIGINT);
    sigaddset(&newmask, SIGCHLD);
    sigprocmask(SIG_BLOCK, &newmask, &oldmask);

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
    }
    sigprocmask(SIG_SETMASK, &oldmask, NULL);
}

static int mbd_auth_client_request(struct lsfAuth *auth,
                                   XDR *xdrs,
                                   struct packet_header *req_hdr,
                                   struct sockaddr_in *from)
{
    static char fname[] = "authRequest";
    mbdReqType reqType = req_hdr->operation;
    char buf[1024];

    if (!(reqType == BATCH_JOB_SUB || reqType == BATCH_JOB_PEEK ||
          reqType == BATCH_JOB_SIG || reqType == BATCH_QUE_CTRL ||
          reqType == BATCH_RECONFIG || reqType == BATCH_JOB_MIG ||
          reqType == BATCH_HOST_CTRL || reqType == BATCH_JOB_SWITCH ||
          reqType == BATCH_JOB_MOVE || reqType == BATCH_JOB_MODIFY ||
          reqType == BATCH_JOB_FORCE || reqType == BATCH_SET_JOB_ATTR))
        return LSBE_NO_ERROR;

    if (!xdr_lsfAuth(xdrs, auth, req_hdr)) {
        ls_syslog(LOG_ERR, "%s", __func__, "xdr_lsfAuth");
        return LSBE_XDR;
    }

    putEauthClientEnvVar("user");
    sprintf(buf, "mbatchd@%s", clusterName);
    putEauthServerEnvVar(buf);

    // Bug lavalite simulate euath works
    return LSBE_NO_ERROR;

    switch (reqType) {
    case BATCH_JOB_SUB:
        if (auth->uid == 0 && genParams[LSF_ROOT_REX].paramValue == NULL) {
            ls_syslog(LOG_CRIT, "%s: Root user's job submission rejected",
                      fname);
            return LSBE_PERMISSION;
        }
        break;
    case BATCH_RECONFIG:
    case BATCH_HOST_CTRL:
        if (!isAuthManager(auth) && auth->uid != 0) {
            ls_syslog(LOG_CRIT,
                      "%s: uid <%d> not allowed to perform control operation",
                      fname, auth->uid);
            return LSBE_PERMISSION;
        }
        break;
    default:
        break;
    }

    return LSBE_NO_ERROR;
}

static bool_t mbd_should_fork(mbdReqType req)
{
    const char *no_fork = lsbParams[LSB_NO_FORK].paramValue;

    if (no_fork)
        return false;

    if (! is_mbd_read_only_req(req))
        return false;

    return true;
}

static bool_t is_mbd_read_only_req(mbdReqType req)
{

    switch (req) {
    case BATCH_JOB_INFO:       /* bjobs */
    case BATCH_QUE_INFO:       /* bqueues */
    case BATCH_HOST_INFO:      /* bhosts */
    case BATCH_GRP_INFO:
    case BATCH_RESOURCE_INFO:
    case BATCH_PARAM_INFO:
    case BATCH_USER_INFO:
    case BATCH_JOB_PEEK:
        return true;
    default:
        return false;
    }
}

static void processSbdNode(struct sbdNode *sbdPtr, int exception)
{
    switch (sbdPtr->reqCode) {
    case MBD_NEW_JOB:
        doNewJobReply(sbdPtr, exception);
        break;
    case MBD_PROBE:
        doProbeReply(sbdPtr, exception);
        break;
    case MBD_SWIT_JOB:
        doSwitchJobReply(sbdPtr, exception);
        break;
    case MBD_SIG_JOB:
        doSignalJobReply(sbdPtr, exception);
        break;
    case MBD_NEW_JOB_KEEP_CHAN:
        // Obsolete in LavaLite as we are now always connected
        if (logclass & LC_COMM)
            LS_DEBUG("MBD_NEW_JOB_KEEP_CHAN");
        break;
    default:
        LS_ERR("unsupported sbdNode request %d", sbdPtr->reqCode);
    }

    // We encounter an exception on the channel with sbd
    // close it down the sbd now has to reconnect again, perhaps
    // it just rebooted
    if (exception) {
        // we dont need a back ponter here as this must be sbd
        shutdown_mbd_client(sbdPtr->hData->sbd_node);
        sbdPtr->hData->sbd_node = NULL;
    }

    chan_close(sbdPtr->chanfd);
    offList((struct listEntry *) sbdPtr);
    free(sbdPtr);
    nSbdConnections--;
}

void setJobPriUpdIntvl(void)
{
    const int MINIMAL = 5;
    int value;

    if (jobPriorityValue < 0 || jobPriorityTime < 0) {
        jobPriorityUpdIntvl = -1;
        return;
    }

    if (jobPriorityTime <= MINIMAL) {
        jobPriorityUpdIntvl = jobPriorityTime;
        return;
    }

    for (value = 16; value > 1; value /= 2) {
        if (jobPriorityTime / value >= MINIMAL) {
            jobPriorityUpdIntvl = jobPriorityTime / value;
            break;
        }
    }

    if (jobPriorityUpdIntvl < 0) {
        jobPriorityUpdIntvl = MINIMAL;
    }

    return;
}

void updateJobPriorityInPJL(void)
{
    static int count = 0;
    int term;
    int priority;
    struct jData *jp;

    if (jobPriorityTime != jobPriorityUpdIntvl) {
        term = jobPriorityTime / jobPriorityUpdIntvl;
        count = (count + 1) % term;
        priority = count * jobPriorityValue / term;
    } else {
        priority = jobPriorityValue;
    }

    for (jp = jDataList[PJL]->forw; jp != jDataList[PJL]; jp = jp->forw) {
        unsigned int newVal = jp->jobPriority + priority;
        jp->jobPriority = MIN(newVal, (unsigned int) MAX_JOB_PRIORITY);
    }

    return;
}

static void
mbd_check_not_root(void)
{
    if (getuid() == 0 || geteuid() == 0) {
        LS_ERR("mbatchd must not run as root (ruid=%u, euid=%u)",
               getuid(), geteuid());
        mbdDie(MASTER_FATAL);
    }
}

static void mbd_init_log(void)
{
    const char *log_dir = genParams[LSF_LOGDIR].paramValue;
    const char *log_mask = genParams[LSF_LOG_MASK].paramValue;

    bool_t debug = mbd_debug;
    bool_t check = lsb_CheckMode;

    if (!log_dir)
        log_dir = "/var/log/lavalite"; /* fallback */

    if (!log_mask)
        log_mask = "LOG_INFO"; /* sane default */

    // Initialize LavaLite logging
    if (check) {
        ls_openlog("mbatchd", log_dir, true, 0, (char *)log_mask);
        LS_INFO("Starting mbatchd in check mode, console logging only");
    } else if (debug) {
        ls_openlog("mbatchd", log_dir, true, 0, (char *)log_mask);
        LS_INFO("Starting mbatchd in debug mode");
    } else {
        /* Normal production daemon case */
        ls_openlog("mbatchd", log_dir, false, 0, (char *)log_mask);
    }

    LS_INFO("Logging initialized: dir=%s mask=%s debug=%d check=%d",
            log_dir, log_mask, debug, check);
}
