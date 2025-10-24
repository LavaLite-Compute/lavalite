/* $Id: lim.main.c,v 1.18 2007/08/15 22:18:54 tmizan Exp $
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 *
 */

#include "lsf/lim/lim.h"

int    limSock = -1;
int    limTcpSock = -1;
ushort  lim_port;
ushort  lim_tcp_port;
int probeTimeout = 2;
short  resInactivityCount = 0;

struct clusterNode *myClusterPtr;
struct hostNode *myHostPtr;
int   masterMe;
int   nClusAdmins = 0;
int   *clusAdminIds = NULL;
int   *clusAdminGids = NULL;
char  **clusAdminNames = NULL;

int numMasterCandidates = -1;
int isMasterCandidate;
int limConfReady = false;

struct limLock limLock;
char   myClusterName[MAXLSFNAMELEN];
u_int  loadVecSeqNo=0;
u_int  masterAnnSeqNo=0;
bool lim_debug = false;
int lim_CheckMode = 0;
int lim_CheckError = 0;
char *env_dir = NULL;
int  numHostResources;
struct sharedResource **hostResources = NULL;
u_short lsfSharedCkSum = 0;

pid_t pimPid = -1;
static void startPIM(int, char **);
static inline uint64_t now_ms(void);

struct liStruct *li = NULL;
int li_len = 0;

struct config_param limParams[] =
{
    {"LSF_CONFDIR", NULL},
    {"LSF_LIM_DEBUG", NULL},
    {"LSF_SERVERDIR", NULL},
    {"LSF_LOGDIR", NULL},
    {"LSF_LIM_PORT", NULL},
    {"LSF_RES_PORT", NULL},
    {"LSF_DEBUG_LIM",NULL},
    {"LSF_TIME_LIM",NULL},
    {"LSF_LOG_MASK",NULL},
    {"LSF_LIM_IGNORE_CHECKSUM", NULL},
    {"LSF_MASTER_LIST", NULL},
    {"LSF_REJECT_NONLSFHOST", NULL},
    {NULL, NULL},
};

extern int chanIndex;

static void initAndConfig(int);
static void term_handler(int);
static void child_handler(int);
static void usage(const char *);
static void doAcceptConn(void);
static void initSignals(void);
static void periodic(void);
static struct tclLsInfo * getTclLsInfo(void);
static void initMiscLiStruct(void);

extern struct extResInfo *getExtResourcesDef(char *);
extern char *getExtResourcesLoc(char *);
extern char *getExtResourcesVal(char *);

static int process_udp_request(void);

static void
usage(const char *cmd)
{
    fprintf(stderr,
            "Usage: %s [OPTIONS]\n"
            "  -d, --debug Run in foreground (no daemonize)\n"
            "  -C, --check Configuration check (prints version, sets check mode)\n"
            "  -V, --version     Print version and exit\n"
            "  -e, --envdir DIR  Path to env dir \n"
            "  -h, --help Show this help\n",
            cmd);
}

static struct option long_options[] = {
    {"debug", no_argument,        0, 'd'},
    {"envdir", required_argument, 0, 'e'},
    {"version", no_argument,      0, 'V'},
    {"check", no_argument,        0, 'C'},
    {"help", no_argument,         0, 'h'},
    {0, 0, 0, 0}
};


int
main(int argc, char **argv)
{
    int cc;

    while ((cc = getopt_long(argc, argv, "de:VCh", long_options, NULL)) != EOF) {
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
            fprintf(stderr, "%s\n", LAVALITE_VERSION_STR);
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
            env_dir = LSETCDIR;
        }
    }

    if (lim_debug)
        fprintf(stderr, "Reading configuration from %s/lsf.conf\n", env_dir);

    if (initenv_(limParams, env_dir) < 0) {

        char *sp = getenv("LSF_LOGDIR");
        if (sp != NULL)
            limParams[LSF_LOGDIR].paramValue = sp;
        ls_openlog("lim", limParams[LSF_LOGDIR].paramValue, lim_debug,
                   limParams[LSF_LOG_MASK].paramValue);
        ls_syslog(LOG_ERR, "lim: initenv_ %s", env_dir);
        open_log("lim", limParams[LSF_LOG_MASK].paramValue, true);
        syslog(LOG_ERR, "lim: initenv_() failed %s", env_dir);
        lim_Exit("main");
    }

    if (limParams[LSF_LIM_DEBUG].paramValue) {
        lim_debug = true;
    }

    if (lim_debug == false) {
        for (int i = sysconf(_SC_OPEN_MAX); i >= 0 ; i--)
            close(i);
        daemonize_();
    }

    getLogClass_(limParams[LSF_DEBUG_LIM].paramValue,
                 limParams[LSF_TIME_LIM].paramValue);

    if (lim_debug) {
        ls_openlog("lim", limParams[LSF_LOGDIR].paramValue, true, "LOG_DEBUG");
        open_log("lim", limParams[LSF_LOG_MASK].paramValue, true);
    } else {
        ls_openlog("lim",
                limParams[LSF_LOGDIR].paramValue, false,
                limParams[LSF_LOG_MASK].paramValue);
        open_log("lim", limParams[LSF_LOG_MASK].paramValue, false);
    }

    if (initMasterList_() < 0) {
        if (lserrno == LSE_NO_HOST) {
            syslog(LOG_ERR, "%s: There is no valid host in LSF_MASTER_LIST",
                   __func__);
        }
        lim_Exit("initMasterList");
    }
    if (lserrno == LSE_LIM_IGNORE) {
        ls_syslog(LOG_WARNING, "Invalid or duplicated hostname in LSF_MASTER_LIST. Ignoring host");
    }
    isMasterCandidate = getIsMasterCandidate_();
    numMasterCandidates = getNumMasterCandidates_();

    initAndConfig(lim_CheckMode);

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

    syslog(LOG_INFO, "%s: Daemon running (tcp_port %d %s)",
           __func__, ntohs(myHostPtr->statInfo.portno), LAVALITE_VERSION_STR);

    if (logclass & LC_COMM)
        ls_syslog(LOG_DEBUG, "%s: sampleIntvl=%f exchIntvl=%f "
                  "hostInactivityLimit=%d masterInactivityLimit=%d retryLimit=%d",
                  __func__, sampleIntvl, exchIntvl, hostInactivityLimit,
                  masterInactivityLimit, retryLimit);

    if (! lim_debug)
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

        if (FD_ISSET(limSock, &chanmask.rmask)) {
            cc = process_udp_request();
            if (cc < 0) {
                syslog(LOG_ERR, "%s: process_udp_request() failed: %m", __func__);
                // continue the loop checking other descriptors
            }
        }

        if (FD_ISSET(limTcpSock, &chanmask.rmask)) {
            doAcceptConn();
        }

        clientIO(&chanmask);
    }
}


static uint64_t
now_ms(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);

    return (uint64_t)ts.tv_sec * 1000ULL + ts.tv_nsec / 1000000ULL;
}

static int
process_udp_request(void)
{
    static char buf[BUFSIZ];
    struct sockaddr_in from;

    memset(&from, 0, sizeof(from));

    struct LSFHeader reqHdr;
    int cc = chanRcvDgram_(limSock, buf, sizeof(buf), &from, -1);
    if (cc < 0) {
        syslog(LOG_ERR, "%s: Error receiving data on limSock %d, cc=%d: %m",
               __func__, limSock, cc);
        return -1;
    }

    XDR  xdrs;
    xdrmem_create(&xdrs, buf, sizeof(buf), XDR_DECODE);
    initLSFHeader_(&reqHdr);
    if (! xdr_LSFHeader(&xdrs, &reqHdr)) {
        syslog(LOG_ERR, "%s: xdr_LSFHeader() failed %m", __func__);
        xdr_destroy(&xdrs);
        return -1;
    }


    struct hostNode *node = findHostbyAddr(&from, "main");
    if (node == NULL) {
        syslog(LOG_WARNING, "%s: received request <%d> from unknown host %s",
               __func__, reqHdr.opCode, sockAdd2Str_(&from));
        return -1;
    }

    switch (reqHdr.opCode) {
    case LIM_PLACEMENT:
        placeReq(&xdrs, &from, &reqHdr, -1);
        break;
    case LIM_LOAD_REQ:
        loadReq(&xdrs, &from, &reqHdr, -1);
        break;
    case LIM_GET_CLUSNAME:
        clusNameReq(&xdrs, &from, &reqHdr);
        break;
    case LIM_GET_MASTINFO:
        masterInfoReq(&xdrs, &from, &reqHdr);
        break;
    case LIM_GET_CLUSINFO:
        clusInfoReq(&xdrs, &from, &reqHdr);
        break;
    case LIM_PING:
        pingReq(&xdrs, &from, &reqHdr);
        break;
    case LIM_GET_HOSTINFO:
        hostInfoReq(&xdrs, node, &from, &reqHdr, -1);
        break;
    case LIM_GET_INFO:
        infoReq(&xdrs, &from, &reqHdr, -1);
        break;
    case LIM_GET_CPUF:
        cpufReq(&xdrs, &from, &reqHdr);
        break;
    case LIM_CHK_RESREQ:
        chkResReq(&xdrs, &from, &reqHdr);
        break;
    case LIM_GET_RESOUINFO:
        resourceInfoReq(&xdrs, &from, &reqHdr, -1);
        break;
    case LIM_REBOOT:
        reconfigReq(&xdrs, &from, &reqHdr);
        break;
    case LIM_SHUTDOWN:
        shutdownReq(&xdrs, &from, &reqHdr);
        break;
    case LIM_LOCK_HOST:
        lockReq(&xdrs, &from, &reqHdr);
        break;
    case LIM_DEBUGREQ:
        limDebugReq(&xdrs, &from, &reqHdr);
        break;
    case LIM_SERV_AVAIL:
        servAvailReq(&xdrs, node, &from, &reqHdr);
        break;
    case LIM_LOAD_UPD:
        rcvLoad(&xdrs, &from, &reqHdr);
        break;
    case LIM_JOB_XFER:
        jobxferReq(&xdrs, &from, &reqHdr);
        break;
    case LIM_MASTER_ANN:
        masterRegister(&xdrs, &from, &reqHdr);
        break;
    case LIM_CONF_INFO:
        rcvConfInfo(&xdrs, &from, &reqHdr);
        break;
    default:
        if (reqHdr.version <= _XDR_VERSION_0_1_0) {
            errorBack(&from, &reqHdr, LIME_BAD_REQ_CODE, -1);
            syslog(LOG_ERR, "%s: Unknown request code %d vers %d from %s",
                   __func__, reqHdr.opCode, reqHdr.version,
                   sockAdd2Str_(&from));
            break;
        }
    }

    xdr_destroy(&xdrs);

    return 0;
}



static void
doAcceptConn(void)
{
    static char fname[] = "doAcceptConn";
    int  ch;
    struct sockaddr_in from;
    struct hostNode *fromHost;
    struct clientNode *client;
    int deny = false;

    if (logclass & (LC_TRACE | LC_COMM))
        ls_syslog(LOG_DEBUG1, "%s: Entering this routine...", fname);

    ch = chanAccept_(limTcpSock, &from);
    if (ch < 0) {
        ls_syslog(LOG_ERR, I18N_FUNC_D_FAIL_MM, fname, "chanAccept_",
                limTcpSock);
        return;
    }

    if ((fromHost = findHostbyAddr(&from, fname)) == NULL) {

        if (!lim_debug) {
            if (ntohs(from.sin_port) >= IPPORT_RESERVED ||
                    ntohs(from.sin_port) <  IPPORT_RESERVED/2) {
                deny = true;
            }
        }
    }

    if (deny == true) {
        ls_syslog(LOG_ERR, "%s: Intercluster request from host <%s> "
                  "not using privileged port", fname, sockAdd2Str_(&from));
        chanClose_(ch);
        return;
    }

    if (fromHost != NULL) {

        client = (struct clientNode *)malloc(sizeof(struct clientNode));
        if (!client) {
            ls_syslog(LOG_ERR, "%s: Connection from %s dropped",
                      fname, sockAdd2Str_(&from));
            chanClose_(ch);
            return;
        }
        client->chanfd = ch;
        if (client->chanfd < 0) {
            ls_syslog(LOG_ERR, I18N_FUNC_FAIL_ENO_D, fname, "chanOpenSock",
                    cherrno);
            ls_syslog(LOG_ERR, "%s: Connection from %s dropped",
                    fname,
                    sockAdd2Str_(&from));
            chanClose_(ch);
            free(client);
            return;
        } else if (client->chanfd >= 2*MAXCLIENTS) {

            chanClose_(ch);
            ls_syslog(LOG_ERR, "%s: Can't maintain this big clientMap[%d], Connection from %s dropped",
                    fname, client->chanfd,
                    sockAdd2Str_(&from));
            free(client);

            return;
        }

        clientMap[client->chanfd] = client;
        client->inprogress = false;
        client->fromHost   = fromHost;
        client->from       = from;
        client->clientMasks = 0;
        client->reqbuf = NULL;
    }
}

static void
initSignals(void)
{
    sigset_t mask;

    Signal_(SIGHUP,  term_handler);
    Signal_(SIGINT,  term_handler);
    Signal_(SIGTERM,  term_handler);
    Signal_(SIGXCPU,  term_handler);
    Signal_(SIGXFSZ,  term_handler);
    Signal_(SIGPROF,  term_handler);
    Signal_(SIGPWR,  term_handler);
    Signal_(SIGUSR1,  term_handler);
    Signal_(SIGUSR2,  term_handler);
    Signal_(SIGCHLD,  child_handler);
    Signal_(SIGPIPE, SIG_IGN);

    /* LIM is compiled with define NO_SIGALARM
    */
    Signal_(SIGALRM, SIG_IGN);

    sigemptyset(&mask);
    sigprocmask(SIG_SETMASK, &mask, NULL);
}

static void
initAndConfig(int checkMode)
{
    static char fname[] = "initAndConfig";
    int i;
    struct tclLsInfo *tclLsInfo;

    if (logclass & (LC_TRACE))
        ls_syslog(LOG_DEBUG, "%s: Entering this routine...; checkMode=%d",
                fname, checkMode);

    initLiStruct();
    if (readShared() < 0) {
        lim_Exit("initAndConfig");
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

    if (! checkMode) {
        initConfInfo();
        satIndex();
        loadIndex();
    }
    if (chanInit_() < 0)
        lim_Exit("chanInit_");

    for(i=0; i < 2*MAXCLIENTS; i++) {
        clientMap[i]=NULL;
    }

    {
        // Bug what is the purpose of this, set in the env
        // while processing the API so child picks it up?
        char *lsfLimLock = getenv("LSF_LIM_LOCK");
        int  flag = -1;
        time_t  lockTime =-1;

        if (lsfLimLock != NULL && lsfLimLock[0] != 0) {

            if (logclass & LC_TRACE) {
                ls_syslog(LOG_DEBUG2, "%s: LSF_LIM_LOCK=<%s>", fname, lsfLimLock);
            }
            sscanf(lsfLimLock, "%d %ld", &flag, &lockTime);
            if (flag > 0) {

                limLock.on = flag;
                limLock.time = lockTime;
                if ( LOCK_BY_USER(limLock.on)) {
                    myHostPtr->status[0] |= LIM_LOCKEDU;
                }
                if ( LOCK_BY_MASTER(limLock.on)) {
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


static void
periodic(void)
{

    syslog(LOG_DEBUG, "%s: Entering this routine...", __func__);

    TIMEIT(0, readLoad(), "readLoad()");

    if (masterMe) {
        announceMaster(myClusterPtr, 1, false);
    }

    if (0)
        checkHostWd();
}


static void
term_handler(int signum)
{
    static char fname[] = "term_handler";

    if (logclass & (LC_TRACE))
        ls_syslog(LOG_DEBUG, "%s: Entering this routine...", fname);

    Signal_(signum, SIG_DFL);

    ls_syslog(LOG_ERR, "%s: Received signal %d, exiting",
            fname,
            signum);
    chanClose_(limSock);
    chanClose_(limTcpSock);

    if (elim_pid > 0) {
        kill(elim_pid, SIGTERM);
        millisleep_(2000);
    }

    kill(getpid(), signum);
    exit(0);
}

static void
child_handler (int sig)
{
    static char fname[] = "child_handler";
    int pid;
    int status;

    if (logclass & (LC_TRACE))
        ls_syslog(LOG_DEBUG1, "%s: Entering this routine...", fname);

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        if (pid == elim_pid) {
            ls_syslog(LOG_ERR, "%s: elim (pid=%d died (exit_code=%d,exit_sig=%d)",
                    fname,
                    (int)elim_pid,
                    WEXITSTATUS (status),
                    WIFSIGNALED (status) ? WTERMSIG (status) : 0);
            elim_pid = -1;
        }
        if (pid == pimPid) {
            if (logclass & LC_PIM)
                ls_syslog(LOG_DEBUG, "child_handler: pim (pid=%d) died", pid);
            pimPid = -1;
        }
    }
}

int
initSock(int checkMode)
{
    struct sockaddr_in limTcpSockId;

    lim_port = atoi(limParams[LSF_LIM_PORT].paramValue);
    if (lim_port <= 0) {
        syslog(LOG_ERR, "%s: LSF_LIM_PORT <%s> must be a positive number",
               __func__, limParams[LSF_LIM_PORT].paramValue);
        return -1;
    }


    limSock = chanServSocket_(SOCK_DGRAM, lim_port, -1,  0);
    if (limSock < 0) {
        syslog(LOG_ERR, "%s: unable to create datagram socket port %d "
               "another LIM running?: %m ", __func__, lim_port);
        return -1;
    }

    lim_port = htons(lim_port);

    limTcpSock = chanServSocket_(SOCK_STREAM, 0, 10, 0);
    if (limTcpSock < 0) {
        syslog(LOG_ERR, "%s: unable to create tcp socket port %d "
               "another LIM running?: %m ", __func__, lim_port);
        chanClose_(limTcpSock);
        return -1;
    }
    socklen_t size = sizeof(limTcpSockId);
    int cc = getsockname(chanSock_(limTcpSock),
                         (struct sockaddr *)&limTcpSockId, &size);
    if (cc < 0) {
        syslog(LOG_ERR, "%s: getsocknamed(%d) failed: %m", __func__,
               limTcpSock);
        chanClose_(limTcpSock);
        return -1;
    }
    lim_tcp_port = limTcpSockId.sin_port;

    return 0;
}

void
errorBack(struct sockaddr_in *from, struct LSFHeader *reqHdr,
          enum limReplyCode replyCode, int chan)
{
    static char fname[] = "errorBack()";
    char buf[MSGSIZE/4];
    struct LSFHeader replyHdr;
    XDR  xdrs2;
    int cc;

    initLSFHeader_(&replyHdr);
    replyHdr.opCode  = (short) replyCode;
    replyHdr.refCode = reqHdr->refCode;
    replyHdr.length = 0;
    xdrmem_create(&xdrs2, buf, MSGSIZE/4, XDR_ENCODE);
    if (!xdr_LSFHeader(&xdrs2, &replyHdr)) {
        ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "xdr_LSFHeader");
        xdr_destroy(&xdrs2);
        return;
    }

    if (chan < 0)
        cc = chanSendDgram_(limSock, buf, XDR_GETPOS(&xdrs2), from);
    else
        cc = chanWrite_(chan, buf, XDR_GETPOS(&xdrs2));

    if (cc < 0)
        ls_syslog(LOG_ERR, I18N_FUNC_D_FAIL_M, fname,
                "chanSendDgram_/chanWrite_",
                limSock);

    xdr_destroy(&xdrs2);
    return;
}
static struct tclLsInfo *
getTclLsInfo(void)
{
    static char fname[] = "getTclLsInfo";
    static struct tclLsInfo *tclLsInfo;
    int i;

    if ((tclLsInfo = (struct tclLsInfo *) malloc (sizeof (struct tclLsInfo )))
            == NULL) {
        ls_syslog(LOG_ERR, I18N_FUNC_FAIL_M, fname, "malloc");
        return NULL;
    }

    if ((tclLsInfo->indexNames = (char **)malloc (allInfo.numIndx *
                    sizeof (char *))) == NULL) {
        ls_syslog(LOG_ERR, I18N_FUNC_FAIL_M, fname, "malloc");
        return NULL;
    }
    for (i=0; i < allInfo.numIndx; i++) {
        tclLsInfo->indexNames[i] = allInfo.resTable[i].name;
    }
    tclLsInfo->numIndx = allInfo.numIndx;
    tclLsInfo->nRes = shortInfo.nRes;
    tclLsInfo->resName = shortInfo.resName;
    tclLsInfo->stringResBitMaps = shortInfo.stringResBitMaps;
    tclLsInfo->numericResBitMaps = shortInfo.numericResBitMaps;

    return tclLsInfo;

}

static void
startPIM(int argc, char **argv)
{
    int i;
    static time_t lastTime = 0;

    if (time(NULL) - lastTime < 60*2)
        return;

    lastTime = time(NULL);

    if ((pimPid = fork())) {
        if (pimPid < 0)
            syslog(LOG_ERR, "%s: failed %m", __func__);
        return;
    }


    for(i = sysconf(_SC_OPEN_MAX); i>= 0; i--)
        close(i);

    for (i = 1; i < NSIG; i++)
        Signal_(i, SIG_DFL);

    char daemonPath[PATH_MAX];
    snprintf(daemonPath, sizeof(daemonPath), "%s/pim",
             limParams[LSF_SERVERDIR].paramValue);
    char *pargv[] = {daemonPath, NULL };

    execv(pargv[0], pargv);
    syslog(LOG_ERR, "%s: failed %m", __func__);
    exit(-1);
}


void
initLiStruct(void)
{
    if (!li) {
        li_len=16;

        li=(struct liStruct *)malloc(sizeof(struct liStruct)*li_len);
    }

    li[0].name="R15S"; li[0].increasing=1; li[0].delta[0]=0.30;
    li[0].delta[1]=0.10; li[0].extraload[0]=0.20; li[0].extraload[1]=0.40;
    li[0].valuesent=0.0; li[0].exchthreshold=0.25; li[0].sigdiff=0.10;

    li[1].name="R1M"; li[1].increasing=1; li[1].delta[0]=0.15;
    li[1].delta[1]=0.10; li[1].extraload[0]=0.20; li[1].extraload[1]=0.40;
    li[1].valuesent=0.0; li[1].exchthreshold=0.25;  li[1].sigdiff=0.10;

    li[2].name="R15M"; li[2].increasing=1; li[2].delta[0]=0.15;
    li[2].delta[1]=0.10; li[2].extraload[0]=0.20; li[2].extraload[1]=0.40;
    li[2].valuesent=0.0; li[2].exchthreshold=0.25; li[2].sigdiff=0.10;

    li[3].name="UT"; li[3].increasing=1; li[3].delta[0]=1.00;
    li[3].delta[1]=1.00; li[3].extraload[0]=0.10; li[3].extraload[1]=0.20;
    li[3].valuesent=0.0; li[3].exchthreshold=0.15; li[3].sigdiff=0.10;

    li[4].name="PG"; li[4].increasing=1; li[4].delta[0]=2.5;
    li[4].delta[1]=1.5; li[4].extraload[0]=0.8; li[4].extraload[1]=1.5;
    li[4].valuesent=0.0; li[4].exchthreshold=1.0; li[4].sigdiff=5.0;

    li[5].name="IO"; li[5].increasing=1; li[5].delta[0]=80;
    li[5].delta[1]=40; li[5].extraload[0]=15; li[5].extraload[1]=25.0;
    li[5].valuesent=0.0; li[5].exchthreshold=25.0; li[5].sigdiff=5.0;

    li[6].name="LS"; li[6].increasing=1; li[6].delta[0]=3;
    li[6].delta[1]=3; li[6].extraload[0]=0; li[6].extraload[1]=0;
    li[6].valuesent=0.0; li[6].exchthreshold=0.0; li[6].sigdiff=1.0;

    li[7].name="IT"; li[7].increasing=0; li[7].delta[0]=6000;
    li[7].delta[1]=6000; li[7].extraload[0]=0; li[7].extraload[1]=0;
    li[7].valuesent=0.0; li[7].exchthreshold=1.0; li[7].sigdiff=5.0;

    li[8].name="TMP"; li[8].increasing=0; li[8].delta[0]=2;
    li[8].delta[1]=2; li[8].extraload[0]=-0.2; li[8].extraload[1]=-0.5;
    li[8].valuesent=0.0; li[8].exchthreshold=1.0; li[8].sigdiff=2.0;

    li[9].name="SMP"; li[9].increasing=0; li[9].delta[0]=10;
    li[9].delta[1]=10; li[9].extraload[0]=-0.5; li[9].extraload[1]=-1.5;
    li[9].valuesent=0.0; li[9].exchthreshold=1.0; li[9].sigdiff=2.0;

    li[10].name="MEM"; li[10].increasing=0; li[10].delta[0]=9000;
    li[10].delta[1]=9000; li[10].extraload[0]=-0.5; li[10].extraload[1]=-1.0;
    li[10].valuesent=0.0; li[10].exchthreshold=1.0; li[10].sigdiff=3.0;
}

static void
initMiscLiStruct(void)
{
    int i;

    extraload=(float *)malloc(allInfo.numIndx*sizeof(float));
    memset((char *)extraload, 0, allInfo.numIndx*sizeof(float));
    li=(struct liStruct *)realloc(li,sizeof(struct liStruct)*allInfo.numIndx);
    li_len = allInfo.numIndx;
    for (i = NBUILTINDEX; i < allInfo.numIndx; i++) {
        li[i].delta[0]=9000;
        li[i].delta[1]=9000; li[i].extraload[0]=0; li[i].extraload[1]=0;
        li[i].valuesent=0.0; li[i].exchthreshold=0.0001; li[i].sigdiff=0.0001;
    }
}
