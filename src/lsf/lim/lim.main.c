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
#include "lsf/lib/mls.h"

#define VCL_VERSION 2
#define NL_SETN 24
#define _PATH_NULL  "/dev/null"
#define SLIMCONF_WAIT_TIME  (1500)

extern char *argvmsg_(int argc, char **argv);

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
int    kernelPerm;

struct limLock limLock;
char   myClusterName[MAXLSFNAMELEN];
u_int  loadVecSeqNo=0;
u_int  masterAnnSeqNo=0;
int lim_debug = 0;
int lim_CheckMode = 0;
int lim_CheckError = 0;
char *env_dir = NULL;
static int    alarmed = 0;
char  ignDedicatedResource = false;
int  numHostResources;
struct sharedResource **hostResources = NULL;
u_short lsfSharedCkSum = 0;

pid_t pimPid = -1;
static void startPIM(int, char **);
#if defined(NO_SIGALARM)
static struct timeval periodicTimer;
static void timeToPeriodic(struct timeval *);
static int  timeForPeriodic(void);
#endif

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
    {"LSF_CONF_RETRY_MAX", NULL},
    {"LSF_CONF_RETRY_INT", NULL},
    {"LSF_LIM_RCVBUF", NULL},
    {"LSF_CROSS_UNIX_NT", NULL},
    {"LSF_LIM_IGNORE_CHECKSUM", NULL},
    {"LSF_MASTER_LIST", NULL},
    {"LSF_REJECT_NONLSFHOST", NULL},
    {"LSF_LIM_JACKUP_BUSY", NULL},
    {NULL, NULL},
};

#ifdef MEAS
char limtraceON = false;
#endif


extern int chanIndex;

static void initAndConfig(int, int *);
static void updtimer(void);
static void term_handler(int);
static void child_handler(int);
static void usage(const char *);
static void doAcceptConn(void);
static void initSignals(void);
static void periodic(int);
static struct tclLsInfo * getTclLsInfo(void);
static void initMiscLiStruct(void);
static void printTypeModel(void);
static void alrm_handler(int);

extern struct extResInfo *getExtResourcesDef(char *);
extern char *getExtResourcesLoc(char *);
extern char *getExtResourcesVal(char *);


static void
usage(const char *cmd)
{
    fprintf(stderr, I18N_Usage);
    fprintf(stderr, ": %s [-C] [-V] [-h] [-t] [-debug_level] [-d env_dir]\n",cmd);
    exit(-1);
}


int
main(int argc, char **argv)
{
    static char fname[] = "main";
    fd_set allMask;
    struct Masks sockmask, chanmask;
    enum   limReqCode limReqCode;
    struct sockaddr_in from;
    int    fromSize;
    XDR    xdrs;
    int    msgSize;
#ifdef FLEX_RCVBUF
    char   *reqBuf2;
#endif
    char   reqBuf[MSGSIZE];
    int    maxfd;
    char   *sp;
    int    i;
    struct LSFHeader reqHdr;
    int    to_background = 1;
    char   *lic_file = NULL;
    int    showTypeModel = false;
    static int eagainErrors = 0;




    kernelPerm = 0;
    saveDaemonDir_(argv[0]);


    _i18n_init(I18N_CAT_LIM);


    for (i=1; i<argc; i++) {
        if (strcmp(argv[i], "-d") == 0 && argv[i+1] != NULL) {
            env_dir = argv[i+1];
            i++;
            continue;
        }
        if (strcmp(argv[i], "-c") == 0 && argv[i+1] != NULL) {
            lic_file = argv[i+1];
            i++;
            continue;
        }
        if (strcmp(argv[i], "-1") == 0) {
            lim_debug = 1;
            continue;
        }
        if (strcmp(argv[i], "-2") == 0) {
            lim_debug = 2;
            continue;
        }
        if (strcmp(argv[i], "-3") == 0) {
            lim_debug = 3;
            continue;
        }

        if (strcmp(argv[i], "-C") == 0) {
            putEnv("RECONFIG_CHECK","YES");
            fputs("\n", stderr);
            fputs(_LAVALITE_VERSION_, stderr);
            lim_CheckMode = 1;
            lim_debug = 2;
            continue;
        }
        if (strcmp(argv[i], "-V") == 0) {
            fputs(_LAVALITE_VERSION_, stderr);
            exit(0);
        }
        if (strcmp(argv[i], "-D") == 0) {
            to_background = 0;
            continue;
        }
        if (strcmp(argv[i], "-t") == 0) {
            showTypeModel = true;

            putEnv("RECONFIG_CHECK","YES");
            lim_CheckMode = 1;
            to_background = 0;

            lim_debug = 1;
            i++;
            continue;
        }
        usage(argv[0]);
    }


    if (env_dir == NULL) {
        if ((env_dir = getenv("LSF_ENVDIR")) == NULL) {
            env_dir = LSETCDIR;
        }
    }

    if (lim_debug > 1)
        fprintf(stderr, "Reading configuration from %s/lsf.conf\n", env_dir);


    if (initenv_(limParams, env_dir) < 0) {

        sp = getenv("LSF_LOGDIR");
        if (sp != NULL)
            limParams[LSF_LOGDIR].paramValue = sp;
        ls_openlog("lim", limParams[LSF_LOGDIR].paramValue, (lim_debug == 2),
                limParams[LSF_LOG_MASK].paramValue);
        ls_syslog(LOG_ERR, I18N_FUNC_S_FAIL_MM, fname, "initenv_", env_dir);
        lim_Exit("main");
    }

    if (!lim_debug && limParams[LSF_LIM_DEBUG].paramValue) {
        lim_debug = atoi(limParams[LSF_LIM_DEBUG].paramValue);
        if (lim_debug <= 0)
            lim_debug = 1;
    }


    msgSize = MSGSIZE;
#ifdef FLEX_RCVBUF
    if (limParams[LSF_LIM_RCVBUF].paramValue){
        int tmpSize;
        tmpSize = atoi(limParams[LSF_LIM_RCVBUF].paramValue);
        if (tmpSize > MSGSIZE)
            msgSize = tmpSize;
    }
    reqBuf2 = (char *)malloc(msgSize*sizeof(char));
    if (! reqBuf2){
        fprintf(stderr, I18N_FUNC_D_FAIL_M,
                fname, "malloc", msgSize*sizeof(char));
        exit(1);
    }
#endif

    if (lim_debug == 0) {
        if(getuid() != 0) {
            fprintf(stderr, _i18n_msg_get(ls_catd, NL_SETN, 4,
                        "%s: Real uid is %d, not root\n"), /* catgets 4 */
                    argv[0], (int)getuid());
            exit(1);
        }
        if(geteuid() != 0) {
            fprintf(stderr, _i18n_msg_get(ls_catd, NL_SETN, 5,
                        "%s: Effective uid is %d, not root\n"), /* catgets 5 */
                    argv[0], (int)geteuid());
            exit(1);
        }
    }

    if (to_background && (lim_debug <= 1)) {

        for (i = sysconf(_SC_OPEN_MAX) ; i >= 0 ; i--)
            close(i);
        daemonize_();

        nice(NICE_LEAST);
    }

    if (lim_debug < 2)
        chdir("/tmp");


    getLogClass_(limParams[LSF_DEBUG_LIM].paramValue,
            limParams[LSF_TIME_LIM].paramValue);


    if (lim_debug > 1) {
        ls_openlog("lim", limParams[LSF_LOGDIR].paramValue, true, "LOG_DEBUG");
    } else {
        ls_openlog("lim",
                limParams[LSF_LOGDIR].paramValue, false,
                limParams[LSF_LOG_MASK].paramValue);
    }



    if (logclass & (LC_TRACE | LC_HANG))
        ls_syslog(LOG_DEBUG, "%s: logclass=%x", fname, logclass);

    ls_syslog(LOG_NOTICE, argvmsg_(argc, argv));


    if (initMasterList_() < 0) {
        if (lserrno == LSE_NO_HOST) {
            ls_syslog(LOG_ERR, I18N(5090,"There is no valid host in LSF_MASTER_LIST")); /* catgets 5090 */
        }
        if (lserrno == LSE_MALLOC) {
            ls_syslog(LOG_ERR, I18N_FUNC_FAIL_M, fname, "malloc");
        }
        lim_Exit("initMasterList");
    }
    if (lserrno == LSE_LIM_IGNORE) {
        ls_syslog(LOG_WARNING, I18N(5091, "Invalid or duplicated hostname in LSF_MASTER_LIST. Ignoring host")); /* catgets 5091 */
    }
    isMasterCandidate = getIsMasterCandidate_();
    numMasterCandidates = getNumMasterCandidates_();

    if (isMasterCandidate == true) {

        initAndConfig(lim_CheckMode, &kernelPerm);
    } else {

        if (lim_CheckMode) {

            if (showTypeModel) {

                printTypeModel();
                exit(0);
            }

            printf(_i18n_msg_get(ls_catd, NL_SETN, 5022, "Current host is a slave-only LIM host\n") /* catgets 5022 */);
            exit(EXIT_RUN_ERROR);
        }

        slaveOnlyInit(lim_CheckMode, &kernelPerm);
    }


    if (showTypeModel) {
        printTypeModel();
        exit(0);
    }


    masterMe = (myHostPtr->hostNo == 0);

    maxfd = sysconf(_SC_OPEN_MAX);
    myHostPtr->hostInactivityCount = 0;

    if (lim_CheckMode && isMasterCandidate) {


        if (lim_CheckError == EXIT_WARNING_ERROR) {
            ls_syslog(LOG_WARNING, _i18n_msg_get(ls_catd , NL_SETN, 5001,
                        "Checking Done. Warning(s)/error(s) found.")); /* catgets 5001 */
            exit(EXIT_WARNING_ERROR);
        } else {
            ls_syslog(LOG_INFO, (_i18n_msg_get(ls_catd , NL_SETN, 5033, "Checking Done.")));   /* catgets 5033 */
            exit(0);
        }
    }

    if (isMasterCandidate) {


        if (masterMe) {
            millisleep_(6000);
            initNewMaster();
        }

        initMiscLiStruct();

    }

    initSignals();

#if !defined(NO_SIGALARM)
    Signal_(SIGALRM, alrm_handler);
#endif

    updtimer();


    FD_ZERO(&allMask);
    ls_syslog(LOG_DEBUG, "%s: Daemon running (%d,%d,%d)",
            fname, myClusterPtr->checkSum,
            ntohs(myHostPtr->statInfo.portno), _LAVALITE_VERSION_);
    if (logclass & LC_COMM)
        ls_syslog(LOG_DEBUG,"%s: sampleIntvl=%f exchIntvl=%f hostInactivityLimit=%d masterInactivityLimit=%d retryLimit=%d",fname,sampleIntvl, exchIntvl, hostInactivityLimit, masterInactivityLimit, retryLimit);

    if (lim_debug < 2)
        chdir("/tmp");

    for (;;) {
        int nfiles;
        sigset_t oldMask, newMask;
        struct timeval *timep;
#if defined (NO_SIGALARM)
        struct timeval timeout;
#endif
        sockmask.rmask = allMask;

        if (pimPid == -1 && limConfReady) {
            startPIM(argc, argv);
        }

#if defined(NO_SIGALARM)
        timeToPeriodic(&timeout);
        timep = &timeout;
        if (lim_debug > 2)
            ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5002,
                        "%s: Before select: timeout.tv_sec=%d timeout.tv_usec=%d"),
                    fname, timeout.tv_sec, timeout.tv_usec);
#else
        timep = NULL;
#endif
        if ((nfiles = chanSelect_(&sockmask, &chanmask, timep))  < 0)  {

            if ( errno != EINTR ) {
                ls_syslog(LOG_ERR, I18N_FUNC_FAIL_M, fname, "chanSelect_");
            }
        }


        if (lim_debug > 2)
            ls_syslog(LOG_ERR,I18N(5024, "%s: Afterselect: nfiles=%d"), fname, nfiles);/*catgets 5024 */


        blockSigs_(0, &newMask, &oldMask);

#if !defined(NO_SIGALARM)
        if (alarmed) {
            periodic(kernelPerm);
            sigprocmask(SIG_SETMASK, &oldMask, NULL);
            continue;
        }
#else
        if (lim_debug > 2)
            ls_syslog(LOG_ERR,
                    I18N(5025,"main: periodicTimer.tv_sec=%d periodicTimer.tv_usec=%d "),/*catgets 5025*/
                    periodicTimer.tv_sec, periodicTimer.tv_usec);
        if (timeForPeriodic()) {
            if (lim_debug > 2)
                ls_syslog(LOG_ERR, I18N(5026,"main: Invoking periodic()"));/* catgets 5026 */
            periodic(kernelPerm);
        }
#endif


        if (nfiles <= 0) {
            sigprocmask(SIG_SETMASK, &oldMask, NULL);
            continue;
        }


        if (FD_ISSET(limSock, &chanmask.rmask)) {
            struct hostNode *fromHost = NULL;
            char deny = false;
            int cc;


            fromSize = sizeof(from);
            memset((char*)&from, 0, sizeof(from));

#ifdef FLEX_RCVBUF
            if ((cc = chanRcvDgram_(limSock, reqBuf2, msgSize, &from, -1)) < 0)
#else
                if ((cc = chanRcvDgram_(limSock, reqBuf, msgSize, &from, -1)) < 0)
#endif
                {

                    if ( cc < 0 && errno == EAGAIN ) {
                        ls_syslog(LOG_DEBUG,
                                "%s: EAGAIN limSock<%d> count<%d> : %M",
                                fname, limSock, eagainErrors);

                        sigprocmask(SIG_SETMASK, &oldMask, NULL);
                        eagainErrors++;

                        if (eagainErrors > 500) {
                            ls_syslog(LOG_ERR,
                                    "%s: There have been 500+ EAGAIN errors",
                                    fname);
                            eagainErrors = 0;
                        }
                        continue;
                    }

                    if (cc == -1 && errno == ECONNREFUSED)
                    {


                        sigprocmask(SIG_SETMASK, &oldMask, NULL);
                        continue;
                    }
                    ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5002,
                                "%s: Error receiving data on limSock %d, cc=%d: %M"), fname, limSock, cc); /* catgets 5002 */
                    reconfig();
                }

#ifdef FLEX_RCVBUF
            xdrmem_create(&xdrs, reqBuf2, msgSize, XDR_DECODE);
#else
            xdrmem_create(&xdrs, reqBuf, msgSize, XDR_DECODE);
#endif
            initLSFHeader_(&reqHdr);
            if (!xdr_LSFHeader(&xdrs, &reqHdr)) {
                ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "xdr_LSFHeader");
                xdr_destroy(&xdrs);
                sigprocmask(SIG_SETMASK, &oldMask, NULL);
                continue;
            }

            limReqCode = reqHdr.opCode;


            limReqCode &= 0xFFFF;

            if ((fromHost=findHostbyAddr(&from, "main")) == NULL) {
                deny = true;
                ls_syslog(LOG_WARNING, I18N(5003, "\
                            %s: Received request <%d> from non-LSF host %s"),
                        fname, limReqCode, sockAdd2Str_(&from)); /* catgets 5003 */
            }

            if (logclass & (LC_TRACE | LC_COMM))
                ls_syslog(LOG_DEBUG,
                        "%s: Received request <%d> from host <%s> %s",
                        fname, limReqCode,
                        ((fromHost != NULL) ? fromHost->hostName : "unknown"),
                        sockAdd2Str_(&from));

            if (deny) {
                if (fromHost == NULL) {
                    if (limParams[LSF_REJECT_NONLSFHOST].paramValue) {
                        errorBack(&from, &reqHdr, LIME_NAUTH_HOST, -1);
                    }
                }
                sigprocmask(SIG_SETMASK, &oldMask, NULL);
                xdr_destroy(&xdrs);
                continue;
            }

            if (!limConfReady && limReqCode != LIM_MASTER_ANN
                    && limReqCode != LIM_REBOOT
                    && limReqCode != LIM_SHUTDOWN
                    && limReqCode != LIM_DEBUGREQ
                    && limReqCode != LIM_PING) {

                if (fromHost)
                    errorBack(&from, &reqHdr, LIME_CONF_NOTREADY, -1);
                sigprocmask(SIG_SETMASK, &oldMask, NULL);
                xdr_destroy(&xdrs);
                continue;
            }

            switch (limReqCode) {

                case LIM_PLACEMENT:
#ifdef VENDOR
                    errorBack(&from, &reqHdr, LIME_BAD_REQ_CODE, -1);
#else
                    placeReq(&xdrs, &from, &reqHdr, -1);
#endif
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
                    hostInfoReq(&xdrs, fromHost, &from, &reqHdr, -1);
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
                    servAvailReq(&xdrs, fromHost, &from, &reqHdr);
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
                        static int lastcode;
                        errorBack(&from, &reqHdr, LIME_BAD_REQ_CODE, -1);
                        if (limReqCode != lastcode)
                            ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd, NL_SETN, 5005,
                                        "%s: Unknown request code %d vers %d from %s"), fname, limReqCode, reqHdr.version, sockAdd2Str_(&from)); /* catgets 5005 */
                        lastcode = limReqCode;
                        break;
                    }
            }

            xdr_destroy(&xdrs);
        }

        if (FD_ISSET(limTcpSock, &chanmask.rmask)) {
            doAcceptConn();
        }

        clientIO(&chanmask);

        sigprocmask(SIG_SETMASK, &oldMask, NULL);
    }


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
        ls_syslog(LOG_ERR, I18N(5006, "\
                    %s: Intercluster request from host <%s> not using privileged port"), /* catgets 5006 */
                fname,sockAdd2Str_(&from));
        chanClose_(ch);
        return;
    }

    if (fromHost != NULL) {


        client = (struct clientNode *)malloc(sizeof(struct clientNode));
        if (!client) {
            ls_syslog(LOG_ERR, I18N_FUNC_FAIL_M, fname, "malloc");
            ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5007,
                        "%s: Connection from %s dropped"), /* catgets 5007 */
                    fname,
                    sockAdd2Str_(&from));
            chanClose_(ch);
            return;
        }
        client->chanfd = ch;
        if (client->chanfd < 0) {
            ls_syslog(LOG_ERR, I18N_FUNC_FAIL_ENO_D, fname, "chanOpenSock",
                    cherrno);
            ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5019,
                        "%s: Connection from %s dropped"), /* catgets 5019 */
                    fname,
                    sockAdd2Str_(&from));
            chanClose_(ch);
            free(client);
            return;
        } else if (client->chanfd >= 2*MAXCLIENTS) {

            chanClose_(ch);
            ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5020,
                        "%s: Can't maintain this big clientMap[%d], Connection from %s dropped"), /* catgets 5020 */
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
initAndConfig(int checkMode, int *kernelPerm)
{
    static char fname[] = "initAndConfig";
    int i;
    struct tclLsInfo *tclLsInfo;

    if (logclass & (LC_TRACE | LC_HANG))
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

    initReadLoad(checkMode, kernelPerm);
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
        char *lsfLimLock;
        int     flag = -1;
        time_t  lockTime =-1;

        if ((lsfLimLock = getenv("LSF_LIM_LOCK") != NULL)
            && (lsfLimLock[0]) != 0) {

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
updtimer(void)
{
#if defined(NO_SIGALARM)
    int usec, sec;


    gettimeofday(&periodicTimer, 0);

    if (sampleIntvl >= 1.0)  {
        usec = 0;
        sec = sampleIntvl;
    } else {
        usec = sampleIntvl*1000000.0;
        sec = 0;
    }

    periodicTimer.tv_usec += usec;
    periodicTimer.tv_sec += sec;
#else
    struct itimerval timer;

    timer.it_value.tv_sec    = 5;
    timer.it_value.tv_usec   = 0;

    if (sampleIntvl >= 1.0) {
        timer.it_interval.tv_sec  = sampleIntvl;
        timer.it_interval.tv_usec = 0;
    } else {
        timer.it_interval.tv_sec  = 0;
        timer.it_interval.tv_usec = sampleIntvl*1000000.0;
    }

    if (logclass & LC_COMM)
        ls_syslog(LOG_DEBUG,"updtimer: tv_sec=%d tv_usec=%d",
                (int)timer.it_interval.tv_sec,  (int)timer.it_interval.tv_usec);

    if (setitimer(ITIMER_REAL, &timer, (struct itimerval *)0) < 0) {
        ls_syslog(LOG_ERR, "%s: setimer() failed", __func__);
    }
#endif

}

static void
periodic(int kernelPerm)
{
    static char fname[] = "periodic";
    static time_t ckWtime = 0;
    static time_t askSLIMConfTime = 0;
    static int masterCandidateNo = -1;
    time_t now = time(0);

#if !defined(NO_SIGALARM)
    struct itimerval itime;
#endif

    if (logclass & (LC_TRACE | LC_HANG))
        ls_syslog(LOG_DEBUG, "%s: Entering this routine...", fname);

    if (limConfReady) {
        TIMEIT(0, readLoad(kernelPerm), "readLoad()");
    }

#if defined(NO_SIGALARM)
    updtimer();
#else
    getitimer(ITIMER_REAL, &itime);
    Signal_(SIGALRM, alrm_handler);
    setitimer(ITIMER_REAL, &itime, NULL);
#endif

    if (masterMe) {
        announceMaster(myClusterPtr, 1, false);
    }

    if (ckWtime == 0) {
        ckWtime = now;
        askSLIMConfTime = now;
    }

    if (limConfReady) {
        if (now - ckWtime > 60) {
            checkHostWd();
            ckWtime = now;
        }
    } else {

        if (now - askSLIMConfTime > SLIMCONF_WAIT_TIME) {
            char* hname;
            struct hostNode *hPtr;

            if (++masterCandidateNo == numMasterCandidates) {

                masterCandidateNo = 0;
            }
            if ((hname = getMasterCandidateNameByNo_(masterCandidateNo))
                    != NULL) {
                if ((hPtr = findHostbyList(myClusterPtr->hostList, hname))
                        != NULL) {

                    announceMasterToHost(hPtr, SEND_MASTER_QUERY);
                    askSLIMConfTime = now;
                }
            }
        }
    }
    alarmed = false;
}

#if defined(NO_SIGALARM)

static void
timeToPeriodic(struct timeval *timerp)
{
    gettimeofday(timerp, 0);

    timerp->tv_usec = periodicTimer.tv_usec - timerp->tv_usec;

    if (timerp->tv_usec < 0) {
        timerp->tv_sec--;
        timerp->tv_usec += 1000000;
    }


    if (periodicTimer.tv_sec < timerp->tv_sec)
    {
        timerp->tv_sec = 0;
        timerp->tv_usec = 0;
    } else
        timerp->tv_sec = periodicTimer.tv_sec - timerp->tv_sec;

}

static int
timeForPeriodic(void)
{
    struct timeval tv;

    gettimeofday(&tv, 0);

    if ( (periodicTimer.tv_sec < tv.tv_sec)
            || ((periodicTimer.tv_sec == tv.tv_sec) && (periodicTimer.tv_usec <= tv.tv_usec)))
    {

        return 1;
    }

    return 0;
}
#endif

static void
term_handler(int signum)
{
    static char fname[] = "term_handler";

    if (logclass & (LC_TRACE | LC_HANG))
        ls_syslog(LOG_DEBUG, "%s: Entering this routine...", fname);

    Signal_(signum, SIG_DFL);

    ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5008,
                "%s: Received signal %d, exiting"), /* catgets 5008 */
            fname,
            signum);
    chanClose_(limSock);
    chanClose_(limTcpSock);

    if (elim_pid > 0) {
        kill(elim_pid, SIGTERM);
        millisleep_(2000);
    }

    kill(getpid(), signum);

    _i18n_end (ls_catd);

    exit(0);
}

static void
alrm_handler(int s)
{
}

static void
child_handler (int sig)
{
    static char fname[] = "child_handler";
    int pid;
    int status;

    if (logclass & (LC_TRACE | LC_HANG))
        ls_syslog(LOG_DEBUG1, "%s: Entering this routine...", fname);

    while ((pid = waitpid(-1, &status, WNOHANG)) >0)
    {
        if (pid == elim_pid) {
            ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5008,
                        "%s: elim (pid=%d) died (exit_code=%d,exit_sig=%d)"), /* catgets 5008 */
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
    static char fname[] = "initSock()";
    struct sockaddr_in  limTcpSockId;
    struct servent *sv;
    int Error = 0;
    int size;




    if (limParams[LSF_LIM_PORT].paramValue) {
        if ((lim_port = atoi(limParams[LSF_LIM_PORT].paramValue)) <= 0) {
            ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5009,
                        "%s: LSF_LIM_PORT <%s> must be a positive number"), /* catgets 5009 */
                    fname, limParams[LSF_LIM_PORT].paramValue);
            Error = -1;
        }
    } else if ((lim_debug) || (checkMode)) {
        lim_port  = LIM_PORT;
    } else {
#  if defined(_COMPANY_X_)
        if ((lim_port = get_port_number(LIM_SERVICE, (char *)NULL)) < 0) {
            ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5010,
                        "%s: LIM service not registered. See LSF Guide for help"), fname); /* catgets 5010 */
            Error = -1;
        }
#  else
        if ((sv = getservbyname("lim", "udp")) == NULL) {
            ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5011,
                        "%s: Service lim/udp is unknown. Read LSF Guide for help"), fname); /* catgets 5011 */
            Error = -1;
        } else {
            lim_port = ntohs(sv->s_port);
        }
#  endif
    }




    if (! Error) {
        limSock = chanServSocket_(SOCK_DGRAM, lim_port, -1,  0);
        if (limSock < 0) {
            ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5012,
                        "%s: unable to create datagram service socket using port %d ; another LIM running?: %M "), fname, lim_port); /* catgets 5012 */
            Error = -1;
        }
    }

    lim_port = htons(lim_port);


    if (! Error) {

        limTcpSock = chanServSocket_(SOCK_STREAM, 0, 10, 0);
        if (limTcpSock < 0) {
            ls_syslog(LOG_ERR, I18N_FUNC_FAIL_MM, fname, "chanServSocket_");
            Error = -1;
        }

        size = sizeof(limTcpSockId);
        if (!Error &&
                (getsockname(chanSock_(limTcpSock), (struct sockaddr *)&limTcpSockId, &size)<0)) {

            ls_syslog(LOG_ERR, I18N_FUNC_S_FAIL_M, fname, "getsockname",
                    limTcpSock);
            Error = -1;
        }
        lim_tcp_port = limTcpSockId.sin_port;
    }

    return Error;
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
    static char fname[] = "startPIM";
    char *pargv[16];
    int i;
    static time_t lastTime = 0;


    if (time(NULL) - lastTime < 60*2)
        return;

    lastTime = time(NULL);

    if ((pimPid = fork())) {
        if (pimPid < 0)
            ls_syslog(LOG_ERR, I18N_FUNC_FAIL_M, fname, "fork");
        return;
    }

    alarm(0);

    if (lim_debug > 1) {

        for(i=3; i < sysconf(_SC_OPEN_MAX); i++)
            close(i);
    } else {
        for(i=0; i < sysconf(_SC_OPEN_MAX); i++)
            close(i);
    }

    for (i=1; i < NSIG; i++)
        Signal_(i, SIG_DFL);


    for (i = 1; i < argc; i++)
        pargv[i] = argv[i];
    pargv[i] = NULL;
    pargv[0] = getDaemonPath_("/pim", limParams[LSF_SERVERDIR].paramValue);
    lsfExecv(pargv[0], pargv);
    ls_syslog(LOG_ERR, I18N_FUNC_S_FAIL_M, fname, "execv", pargv[0]);
    exit(-1);
}


int
load_objects(void)
{

    return 0;
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
    for (i=NBUILTINDEX; i<allInfo.numIndx; i++) {
        li[i].delta[0]=9000;
        li[i].delta[1]=9000; li[i].extraload[0]=0; li[i].extraload[1]=0;
        li[i].valuesent=0.0; li[i].exchthreshold=0.0001; li[i].sigdiff=0.0001;
    }
}


static void
printTypeModel(void)
{
    printf(_i18n_msg_get(ls_catd, NL_SETN, 5013,
                "Host Type             : %s\n"),   /* catgets 5013 */
            getHostType());
    printf(_i18n_msg_get(ls_catd, NL_SETN, 5014,
                "Host Architecture     : %s\n"),   /* catgets 5014 */
            getHostModel());
    if (isMasterCandidate) {

        printf(_i18n_msg_get(ls_catd, NL_SETN, 5015,
                    "Matched Type          : %s\n"),   /* catgets 5015 */
                allInfo.hostTypes[myHostPtr->hTypeNo]);
        printf(_i18n_msg_get(ls_catd, NL_SETN, 5016,
                    "Matched Architecture  : %s\n"),   /* catgets 5016 */
                allInfo.hostArchs[myHostPtr->hModelNo]);
        printf(_i18n_msg_get(ls_catd, NL_SETN, 5017,
                    "Matched Model         : %s\n"),   /* catgets 5017 */
                allInfo.hostModels[myHostPtr->hModelNo]);
        printf(_i18n_msg_get(ls_catd, NL_SETN, 5018,
                    "CPU Factor            : %.1f\n"), /* catgets 5018 */
                allInfo.cpuFactor[myHostPtr->hModelNo]);
        if (myHostPtr->hTypeNo==1 || myHostPtr->hModelNo==1)
        {
            printf("When automatic detection of host type or model fails, the type or\n");
            printf("model is set to DEFAULT. LSF will still work on the host. A DEFAULT\n");
            printf("model may be inefficient because of incorrect CPU factor. A DEFAULT\n");
            printf("type may cause binary incompatibility - a job from a DEFAULT host \n");
            printf("type can be dispatched or migrated to another DEFAULT host type.\n\n");
            printf("User can use lim -t to detect the real model or type for a host. \n");
            printf("Change a DEFAULT host model by adding a new model in HostModel in\n");
            printf("lsf.shared.  Change a DEFAULT host type by adding a new type in \n");
            printf("HostType in lsf.shared.\n\n");
        }
    } else {
        printf(_i18n_msg_get(ls_catd, NL_SETN, 5021,
                    "Current host is a slave-only LIM host\n") /* catgets 5021 */);
    }
}



int
slaveOnlyPostConf(int checkMode, int *kernelPerm)
{

    static char fname[]="slaveOnlyPostConf";
    struct sharedResourceInstance *tmp;
    int oldstatus, i, bitPos, newMax;
    struct tclLsInfo *tclLsInfo;
    int numUsrIndx;

    numUsrIndx = 0;

    for (i = NBUILTINDEX; i < allInfo.nRes; i++) {


        if ((allInfo.resTable[i].interval > 0) &&
                (allInfo.resTable[i].valueType == LS_NUMERIC) &&
                (!(allInfo.resTable[i].flags & RESF_SHARED))) {


            if (numUsrIndx+NBUILTINDEX >=li_len-1) {
                li_len *= 2;

                if (!(li=(struct liStruct *)
                            realloc(li, li_len*sizeof(struct liStruct)))) {
                    ls_syslog(LOG_ERR, I18N_FUNC_D_FAIL_M, fname, "malloc",
                            li_len*sizeof(struct liStruct));
                    return -1;
                }

            }

            if ((li[NBUILTINDEX + numUsrIndx].name =
                        putstr_(allInfo.resTable[i].name)) == NULL) {
                ls_syslog(LOG_ERR, I18N_FUNC_D_FAIL_M, fname, "malloc", sizeof(allInfo.resTable[i].name));
                return -1;
            }

            li[NBUILTINDEX + numUsrIndx].increasing =
                (allInfo.resTable[i].orderType == INCR)?1:0;
            numUsrIndx++;
        }
    }

    allInfo.nTypes = 0;
    allInfo.nModels = 0;

    shortInfo.nRes = 0;
    shortInfo.resName = calloc (allInfo.nRes, sizeof(char *));
    shortInfo.stringResBitMaps = calloc(GET_INTNUM(allInfo.nRes),
            sizeof (int));
    shortInfo.numericResBitMaps = calloc(GET_INTNUM(allInfo.nRes),
            sizeof (int));
    if (shortInfo.resName == NULL || shortInfo.stringResBitMaps == NULL
            || shortInfo.numericResBitMaps == NULL) {
        ls_syslog(LOG_ERR, I18N_FUNC_FAIL_M, fname, "malloc");
        return -1;
    }

    for (i = 0; i < GET_INTNUM(allInfo.nRes); i++) {
        shortInfo.stringResBitMaps[i] = 0;
        shortInfo.numericResBitMaps[i] = 0;
    }

    for (i = 0; i < allInfo.nRes; i++) {


        if ((allInfo.resTable[i].flags & RESF_BUILTIN)
                || (allInfo.resTable[i].flags & RESF_DYNAMIC
                    && allInfo.resTable[i].valueType == LS_NUMERIC) ||
                (allInfo.resTable[i].valueType != LS_STRING
                 && allInfo.resTable[i].valueType != LS_BOOLEAN))
            continue;
        shortInfo.resName[shortInfo.nRes] = putstr_(allInfo.resTable[i].name);
        if (shortInfo.resName[shortInfo.nRes] == NULL) {
            ls_syslog(LOG_ERR, I18N_FUNC_FAIL_M, fname, "malloc");
            return -1;
        }
        if (allInfo.resTable[i].valueType == LS_STRING)
            SET_BIT (shortInfo.nRes,  shortInfo.stringResBitMaps);
        shortInfo.nRes++;
    }

    shortInfo.nTypes = 0;
    shortInfo.nModels = 0;

    myClusterPtr->loadIndxNames = calloc(allInfo.numIndx, sizeof(char *));
    if (myClusterPtr->loadIndxNames == NULL) {
        ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "calloc");
        return -1;
    }

    for (i = 0; i < allInfo.numIndx; i++)
        myClusterPtr->loadIndxNames[i] = putstr_(li[i].name);

    for (i = 1; i < 8; i++)
        if (myHostPtr->week[i] != NULL)
            break;

    if (i==8)   {
        for (i = 1; i < 8; i++)
            insertW(&(myHostPtr->week[i]), -1.0, 25.0);
    }

    myHostPtr->status[0] = 0;
    checkHostWd();


    myClusterPtr->status = CLUST_STAT_OK | CLUST_ACTIVE | CLUST_INFO_AVAIL;
    myClusterPtr->managerName = clusAdminNames[0];
    myClusterPtr->managerId = clusAdminIds[0];

    myClusterPtr->nAdmins = nClusAdmins;
    myClusterPtr->adminIds = clusAdminIds;
    myClusterPtr->admins   = clusAdminNames;

    myHostPtr->resBitMaps = (int *)realloc(myHostPtr->resBitMaps, GET_INTNUM(allInfo.nRes) * sizeof (int));
    myHostPtr->DResBitMaps =(int *)realloc(myHostPtr->DResBitMaps,GET_INTNUM(allInfo.nRes) * sizeof (int));
    if (myHostPtr->resBitMaps == NULL || myHostPtr->DResBitMaps == NULL) {
        ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "realloc");
        return -1;
    }


    if ((tclLsInfo = getTclLsInfo()) == NULL) {
        return -1;
    }
    if (initTcl(tclLsInfo) < 0)  {
        return -1;
    }

    initParse(&allInfo);

    oldstatus = myHostPtr->status[0];
    free(myHostPtr->status);
    myHostPtr->status = (int *)malloc((GET_INTNUM(allInfo.numIndx) + 1) *
            sizeof(int));
    memset(myHostPtr->status, 0, (GET_INTNUM(allInfo.numIndx) + 1) *
            sizeof(int));
    myHostPtr->status[0] = oldstatus;

    myHostPtr->loadIndex = (float *)realloc(myHostPtr->loadIndex,
            allInfo.numIndx * sizeof(float));
    myHostPtr->uloadIndex = (float *)realloc(myHostPtr->uloadIndex,
            allInfo.numIndx * sizeof(float));
    if (myHostPtr->loadIndex == NULL || myHostPtr->uloadIndex == NULL) {
        ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "realloc");
        return -1;
    }

    for (i = 0; i < allInfo.numIndx; i++) {
        myHostPtr->loadIndex[i] = INFINIT_LOAD;
        myHostPtr->uloadIndex[i] = INFINIT_LOAD;
    }

    if (checkMode) {
        initReadLoad(LIM_CHECK, kernelPerm);
    } else {
        initReadLoad(LIM_STARTUP, kernelPerm);
    }

    myHostPtr->hModelNo = -1;
    myHostPtr->hTypeNo = -1;
    strcpy(myHostPtr->statInfo.hostType, getHostType());
    strcpy(myHostPtr->statInfo.hostArch, getHostModel());

    for (tmp = sharedResourceHead, bitPos=0; tmp; tmp = tmp->nextPtr, bitPos++);
    newMax = GET_INTNUM(bitPos);
    if (!(myHostPtr->resBitArray)) {
        myHostPtr->resBitArray = (int *)malloc((newMax+1)*sizeof(int));
        if (myHostPtr->resBitArray == NULL) {
            ls_syslog(LOG_ERR, I18N_FUNC_FAIL_M, fname, "malloc");
            return -1;
        }

        for (i = 0; i < newMax+1; i++) {
            myHostPtr->resBitArray[i] = 0;
        }
    }

    if (! checkMode) {
        initConfInfo();
        satIndex();
        loadIndex();
    }

    for(i = 0; i < 2*MAXCLIENTS; i++) {
        clientMap[i] = NULL;
    }


    {
        char *lsfLimLock;
        int     flag = -1;
        time_t  lockTime =-1;

        if ((NULL != (lsfLimLock=getenv("LSF_LIM_LOCK")))
                && ('\0' != lsfLimLock[0])) {

            if ( logclass & LC_TRACE) {
                ls_syslog(LOG_DEBUG2, "%s: LSF_LIM_LOCK=<%s>", fname, lsfLimLock);
            }
            sscanf(lsfLimLock, "%d %ld", &flag, &lockTime);
            if ( flag > 0) {
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

    initMiscLiStruct();

    readLoad(*kernelPerm);

    limConfReady = true;

    return 0;
}
