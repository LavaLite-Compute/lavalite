/* $Id: pim.main.c,v 1.6 2007/08/15 22:18:55 tmizan Exp $
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

#include "lsf/pim/pim.h"

#define MAX_MIN_SLEEPTIME (3)

int bytes;
int nproc;
int pagesize;
struct lsPidInfo *pbase;
struct lsPidInfo *old_pbase;
int nr_of_processes = 0;
int old_nr_of_processes = 0;

static struct lsPidInfo *deadPid;
static int nDeadPids = 0;
static int pimPort;

extern void open_kern( void );
extern void scan_procs( void );
extern int getHdrReserved(struct LSFHeader *);

#define NPIDS_SIZE 32
#define NPGIDS_SIZE 300
#define DEFAULT_UPDATE_PERIOD 15*60
#define USED_TIME 5*60

struct config_param pimParams[] =
{
    {"LSF_LIM_DEBUG", NULL},
    {"LSF_LOGDIR", NULL},
    {"LSF_DEBUG_PIM",NULL},
    {"LSF_LOG_MASK",NULL},
    {"LSF_TIME_PIM",NULL},
    {"LSF_PIM_SLEEPTIME", NULL},
    {"LSF_PIM_INFODIR", NULL},
    {"LSF_PIM_NPROC", NULL},
    {"LSF_PIM_TRACE", NULL},
    {"LSF_PIM_NICEOFF", NULL},
    {"LSF_PIM_SGI_NOSHMEM", NULL},
    {"LSF_PIM_SLEEPTIME_UPDATE", NULL},
    {"LSF_PIM_CPUTIMECHECK", NULL},
    {NULL, NULL}
};

#define LSF_LIM_DEBUG       0
#define LSF_LOGDIR          1
#define LSF_DEBUG_PIM       2
#define LSF_LOG_MASK        3
#define LSF_TIME_PIM        4
#define LSF_PIM_SLEEPTIME   5
#define LSF_PIM_INFODIR     6
#define LSF_PIM_NPROC       7
#define LSF_PIM_TRACE       8
#define LSF_PIM_NICEOFF     9
#define LSF_PIM_SGI_NOSHMEM 10
#define LSF_PIM_SLEEPTIME_UPDATE 11
#define LSF_PIM_CPUTIMECHECK 12

static struct pidLink {
    int child;
    int sibling;
    int sortedIdx;
    int sorted;
} *pidLink;

static int sortedIdx = 0;

struct pgidRec {
    int pgid;
    int npids;
    int *pid;
    int deadUTime, deadSTime;
};

struct lspgidEnt {
    int pgid;
    int active;
};

static struct lspgidEnt *pgidList;
static int nPGidList = 0;

int pidInPGList(int inPid);
void newPGid(int inPGid);
int initPGidList(void);
int cleanPGidList(void);

char *env_dir = NULL;
int pim_debug = 0;
int scanIt = FALSE;
char infoFN[MAXFILENAMELEN];
int sgiPimNoShmem = FALSE;
static int sleepTime = PIM_SLEEP_TIME;
static bool_t sleepBeforeUpdateProcs = TRUE;
int pimCpuTimeCheck = FALSE;

static void usage (char *cmd);
static void logProcessInfo(void);
static void addDeadProcesses(void);
static int pidInGroup(struct lsPidInfo *p, struct pgidRec *pg);
static struct pgidRec *newPGidList(int *npgids);
static void newDeadPid(struct lsPidInfo *pinfo);
static void replayDeadPids(void);
static void doServ(void);
static void updateProcs(const time_t);
static void cpuTimeCheck(const time_t);

static void sortPids(void);
static void getChildren(int ppidIdx);
static void sortIt(int pIdx);

extern char *argvmsg_(int argc, char **argv);

static void
usage (char *cmd)
{
    fprintf(stderr, I18N_Usage);
    fprintf(stderr, ": %s [-V] [-h] [-debug_level] [-d env_dir]\n",
            cmd);
    exit(-1);
}

int
main(int argc, char **argv)
{
    static char fname[] = "main()";
    char *sp;
    char *traceVal = NULL;
    int i;
    char *myHost = "localhost";

    0;

    for (i=1; i<argc; i++) {

        if (strcmp(argv[i], "-d") == 0 && argv[i+1] != NULL) {
            env_dir = argv[i+1];
            i++;
            continue;
        }

        if (strcmp(argv[i], "-1") == 0) {
            pim_debug = 1;
            continue;
        }

        if (strcmp(argv[i], "-2") == 0) {
            pim_debug = 2;
            continue;
        }

        if (strcmp(argv[i], "-V") == 0) {
            fputs(_LAVALITE_VERSION_, stderr);
            exit(0);
        }

        usage(argv[0]);
    }

    if (env_dir == NULL) {
        if ((env_dir = getenv("LSF_ENVDIR")) == NULL) {
            env_dir = "/etc";
        }
    }

    if (initenv_(pimParams, env_dir) < 0) {

        sp = getenv("LSF_LOGDIR");
        if (sp != NULL)
            pimParams[LSF_LOGDIR].paramValue = sp;
        ls_openlog("pim", pimParams[LSF_LOGDIR].paramValue, (pim_debug == 2),
                pimParams[LSF_LOG_MASK].paramValue);
        ls_syslog(LOG_ERR, I18N_FUNC_S_FAIL_M, fname, "initenv_", env_dir);
        ls_syslog(LOG_ERR, I18N_Exiting);
        exit(-1);
    }

    if (!pim_debug && pimParams[LSF_LIM_DEBUG].paramValue) {
        pim_debug = atoi(pimParams[LSF_LIM_DEBUG].paramValue);
        if (pim_debug <= 0)
            pim_debug = 1;
    }

    if (pimParams[LSF_PIM_TRACE].paramValue) {
        traceVal = pimParams[LSF_PIM_TRACE].paramValue;
    } else if (pimParams[LSF_DEBUG_PIM].paramValue) {
        traceVal = pimParams[LSF_DEBUG_PIM].paramValue;
    }
    getLogClass_(traceVal, pimParams[LSF_TIME_PIM].paramValue);

    if (pim_debug > 1)
        ls_openlog("pim", pimParams[LSF_LOGDIR].paramValue, TRUE, "LOG_DEBUG");
    else
        ls_openlog("pim", pimParams[LSF_LOGDIR].paramValue, FALSE,
                pimParams[LSF_LOG_MASK].paramValue);

    if (logclass & (LC_TRACE | LC_HANG))
        ls_syslog(LOG_DEBUG, "pim/main: logclass=%x", logclass);

    ls_syslog(LOG_NOTICE, argvmsg_(argc, argv));

    if (pimParams[LSF_PIM_SLEEPTIME].paramValue) {
        if ((sleepTime = atoi(pimParams[LSF_PIM_SLEEPTIME].paramValue)) < 0) {
            ls_syslog(LOG_ERR, "LSF_PIM_SLEEPTIME value <%s> must be a positive integer, defaulting to %d",
                    pimParams[LSF_PIM_SLEEPTIME].paramValue, PIM_SLEEP_TIME);
            sleepTime = PIM_SLEEP_TIME;
        }
    }

    if ((myHost = ls_getmyhostname()) == NULL) {
        ls_syslog(LOG_ERR, I18N_FUNC_FAIL_MM, fname, "ls_getmyhostname");
        ls_syslog(LOG_ERR, "Using local host");
    }

    if (pimParams[LSF_PIM_INFODIR].paramValue) {
        sprintf(infoFN, "%s/pim.info.%s",
                pimParams[LSF_PIM_INFODIR].paramValue, myHost);
    } else {
        if (pim_debug) {

            if (pimParams[LSF_LOGDIR].paramValue)
                sprintf(infoFN, "%s/pim.info.%s",
                        pimParams[LSF_LOGDIR].paramValue, myHost);
            else
                sprintf(infoFN, "/tmp/pim.info.%s.%d",
                        myHost, (int)getuid());
        } else {
            sprintf(infoFN, "/tmp/pim.info.%s", myHost);
        }
    }

    if (pimParams[LSF_PIM_NPROC].paramValue != NULL) {
        errno = 0;
        nproc = strtol(pimParams[LSF_PIM_NPROC].paramValue, NULL, 10);
        if (nproc == 0 || errno != 0) {
            ls_syslog(LOG_ERR, "pim/main: invalid value for LSF_PIM_NPROC defined: %s", pimParams[LSF_PIM_NPROC].paramValue);
            exit(-1);
        }
    } else {
        nproc = -1;
    }

    if (pimParams[LSF_PIM_SGI_NOSHMEM].paramValue) {
        sgiPimNoShmem = TRUE;
    }

    if (pimParams[LSF_PIM_SLEEPTIME_UPDATE].paramValue != NULL
            && strcasecmp(pimParams[LSF_PIM_SLEEPTIME_UPDATE].paramValue, "y") == 0) {
        sleepBeforeUpdateProcs = FALSE;
    }

    if (pimParams[LSF_PIM_CPUTIMECHECK].paramValue != NULL
            && strcasecmp(pimParams[LSF_PIM_CPUTIMECHECK].paramValue, "y") == 0) {
        pimCpuTimeCheck = TRUE;
    }

    doServ();

    return(0);

}

static void
doServ(void)
{
    static char fname[] = "doServ";
    int ppid = getppid();
    struct sockaddr_in sin;
    int asock;
    time_t lastTime, nextTime, now, lastUsedTime;
    time_t lastUpdateTime = 0;
    time_t lastUpdateStartTime = 0;
    time_t currentUpdateStartTime = 0;

    if ((asock = TcpCreate_(TRUE, 0)) < 0) {
        ls_syslog(LOG_ERR, I18N_FUNC_FAIL_M, fname, "TcpCreate_");
        ls_syslog(LOG_ERR, I18N_Exiting);
        exit(-1);
    }

    socklen_t len = sizeof(sin);
    if (getsockname (asock, (struct sockaddr *) &sin, &len) < 0) {
        ls_syslog(LOG_ERR, I18N_FUNC_FAIL_M, fname, "getsockname");
        ls_syslog(LOG_ERR, I18N_Exiting);
        exit(-1);
    }

    pimPort = ntohs(sin.sin_port);

    replayDeadPids();

    open_kern();

    currentUpdateStartTime = time(0);
    TIMEIT(0, updateProcs(0), "updateProcs");
    lastUpdateStartTime = currentUpdateStartTime;

    now = time(0);
    lastTime = now;
    nextTime = lastTime + sleepTime;
    lastUpdateTime = now;
    lastUsedTime = 0;

    for (;;) {
        struct timeval timeOut;
        fd_set rmask;
        int nready;

        if (nextTime - lastTime < 0)
            timeOut.tv_sec = 0;
        else
            timeOut.tv_sec = nextTime - lastTime;

        timeOut.tv_usec = 0;

        FD_ZERO(&rmask);
        FD_SET(asock, &rmask);

        nready = select(asock + 1, &rmask, NULL, NULL, &timeOut);

        if (logclass & LC_PIM)
            ls_syslog(LOG_DEBUG, "%s: select nready=%d", fname, nready);

        if (nready < 0) {
            ls_syslog(LOG_DEBUG, "%s: select(): %m", fname);
            lastTime = time(0);
            continue;
        }

        if (ppid <= 1) {

            ppid = getppid();
        }

        if ((ppid <= 1) || (kill(ppid, 0) == -1)) {
            ls_syslog(LOG_INFO, ("%s: Parent gone, PIM exiting ..."), fname);
            exit(0);
        }

        now = time(0);

        if (nready != 0) {
            int sock;

            if ((sock = b_accept_(asock, (struct sockaddr *) &sin, &len))
                    < 0) {
                ls_syslog(LOG_ERR, I18N_FUNC_FAIL_M, fname, "accept");
            } else {
                int option;
                struct LSFHeader hdrBuf, hdr;
                int cc;

                io_nonblock_(sock);

                if ((cc = lsRecvMsg_(sock, (char *) &hdrBuf, sizeof(hdrBuf),
                                &hdr, NULL, NULL, nb_read_fix)) < 0) {
                    ls_syslog(LOG_DEBUG,
                            "%s: lsRecvMsg_() failed, cc=%d: %M", fname, cc);
                } else {

                    int sleptTime = now - lastUpdateTime;

                    option = hdr.opCode;
                    newPGid(getHdrReserved(&hdr));

                    if (logclass & LC_PIM)
                        ls_syslog(LOG_DEBUG, "%s: got opCode = %d", fname,
                                option);

                    if ((option & PIM_API_UPDATE_NOW) ||
                            sleptTime >= sleepTime) {
                        if ((option & PIM_API_UPDATE_NOW)
                                && sleepBeforeUpdateProcs == TRUE) {

                            int moreTime;
                            int breakTime = sleepTime/10;

                            if(breakTime == 0) {
                                breakTime = 1;
                            } else if(breakTime > MAX_MIN_SLEEPTIME) {

                                breakTime = MAX_MIN_SLEEPTIME;
                            }

                            moreTime = breakTime - sleptTime;

                            if(moreTime > 0) {
                                if (logclass & LC_PIM)
                                    ls_syslog(LOG_DEBUG,
                                            "%s: I need to sleep more = %d",
                                            fname, moreTime);

                                sleep(moreTime);
                            }
                        }

                        if (logclass & LC_PIM)
                            ls_syslog(LOG_DEBUG,
                                    "%s: Got connection, updateProcs",
                                    fname);
                        currentUpdateStartTime = time(0);
                        TIMEIT(0, updateProcs(lastUpdateStartTime), "updateProcs");
                        lastUpdateStartTime = currentUpdateStartTime;

                        now = time(0);
                        lastUpdateTime = now;
                        nextTime = now + sleepTime;
                    }

                    if ((cc = writeEncodeHdr_(sock, &hdr, nb_write_fix)) < 0)
                        ls_syslog(LOG_DEBUG,
                                "%s: writeEncodeHdr_ failed, cc=%d: %M",
                                fname, cc);
                }

                close(sock);
            }

            if (logclass & LC_PIM)
                ls_syslog(LOG_DEBUG, "%s: Got connection", fname);

            lastTime = now;
            lastUsedTime = now;
            continue;
        }

        if (logclass & LC_PIM)
            ls_syslog(LOG_DEBUG, "%s: Timeout ...", fname);

        if (now - lastUsedTime < USED_TIME ||
                now - lastUpdateTime >= DEFAULT_UPDATE_PERIOD) {
            if (logclass & LC_PIM)
                ls_syslog(LOG_DEBUG, "%s: Timeout updating", fname);

            currentUpdateStartTime = time(0);
            TIMEIT(0, updateProcs(lastUpdateStartTime), "updateProcs");
            lastUpdateStartTime = currentUpdateStartTime;
            now = time(0);
            lastUpdateTime = now;
        }

        lastTime = now;
        nextTime = now + sleepTime;
    }

}

static void
updateProcs(const time_t lastUpdate)
{
    int temp_nr;
    struct lsPidInfo *temp_pbase;

    temp_nr = old_nr_of_processes;
    old_nr_of_processes = nr_of_processes;
    nr_of_processes = temp_nr;

    temp_pbase = old_pbase;
    old_pbase = pbase;
    pbase = temp_pbase;

    if (scanIt) {

        scan_procs();
        if (pimCpuTimeCheck) {
            TIMEIT(0, cpuTimeCheck(lastUpdate), "cpuTimeCheck");
        }
        addDeadProcesses();
        logProcessInfo();
    } else {
        unlink(infoFN);
    }
}

static void
logProcessInfo(void)
{
    int i, pIdx;
    FILE *fp;
    char workFN[MAXFILENAMELEN];
    static char fname[] = "logProcessInfo";
    struct stat st;

    umask(022);

    sprintf(workFN, "%s.%d", infoFN, (int)getpid());

    pidLink = (struct pidLink *) malloc(nr_of_processes *
            sizeof(struct pidLink));

    if (pidLink == NULL) {
        ls_syslog(LOG_ERR, I18N_FUNC_FAIL_M, fname, "malloc");
        exit(-1);
    }
    memset((char*)pidLink, 0, nr_of_processes * sizeof(struct pidLink));

    for (i = 0; i < nr_of_processes; i++) {
        pidLink[i].child = -1;
        pidLink[i].sortedIdx = -1;
        pidLink[i].sibling = -2;
    }

    sortPids();

    if (lstat(workFN, &st) < 0) {
        if (errno == ENOENT) {
            if ((fp = fopen(workFN, "w")) == NULL) {
                ls_syslog(LOG_ERR, I18N_FUNC_S_FAIL_M, fname, "fopen", workFN);
                return;
            }
        } else {
            ls_syslog(LOG_ERR, I18N_FUNC_S_FAIL_M, fname, "stat", workFN);
            return;
        }
    } else if (S_ISREG(st.st_mode) && st.st_nlink == 1) {

        if ((fp = fopen(workFN, "w")) == NULL) {
            ls_syslog(LOG_ERR, I18N_FUNC_S_FAIL_M, fname, "fopen", workFN);
            return;
        }
    } else {

        ls_syslog(LOG_ERR, ("%s: pim info file <%s> is not a regular file, file untouched"), fname, workFN);
        ls_syslog(LOG_ERR, I18N_Exiting);
        exit(-1);
    }

    fprintf(fp, "%d\n", pimPort);

    for (pIdx = 0; pIdx < sortedIdx; pIdx++) {
        i = pidLink[pIdx].sortedIdx;

        fprintf(fp, "%d %d %d %d %d %d %d %d %d %d %d %d\n",
                pbase[i].pid, pbase[i].ppid, pbase[i].pgid, pbase[i].jobid,
                pbase[i].utime, pbase[i].stime, pbase[i].cutime,
                pbase[i].cstime, pbase[i].proc_size,
                pbase[i].resident_size, pbase[i].stack_size,
                (int) pbase[i].status);
    }

    for (i = 0; i < nDeadPids; i++) {
        fprintf(fp, "%d %d %d %d %d %d %d %d %d %d %d %d\n",
                deadPid[i].pid, deadPid[i].ppid, deadPid[i].pgid, deadPid[i].jobid,
                deadPid[i].utime, deadPid[i].stime, deadPid[i].cutime,
                deadPid[i].cstime, deadPid[i].proc_size,
                deadPid[i].resident_size, deadPid[i].stack_size,
                (int) deadPid[i].status);
    }

    fclose(fp);

    if (unlink(infoFN) < 0 && errno != ENOENT) {
        ls_syslog(LOG_DEBUG, "%s: unlink(%s) failed: %m", fname, infoFN);
    }
    if (rename(workFN, infoFN) < 0) {
        ls_syslog(LOG_ERR, I18N_FUNC_FAIL_M, fname, "rename");
    }

    free(pidLink);
}

static void
addDeadProcesses(void)
{
    static char fname[] = "addDeadProcesses";
    struct pgidRec *pg;
    int npgids;
    int i, j;

    pg = newPGidList(&npgids);

    for (i = 0; i < old_nr_of_processes; i++) {
        for (j = 0; j < npgids; j++) {
            if (pidInGroup(old_pbase + i, pg + j))
                break;
        }
    }

    for (i = 0; i < nDeadPids; i++) {
        for (j = 0; j < npgids; j++) {
            if (pidInGroup(deadPid + i, pg + j))
                break;
        }
    }

    FREEUP(deadPid);
    deadPid = (struct lsPidInfo *) malloc(npgids * sizeof(struct lsPidInfo));
    if (deadPid == NULL) {
        ls_syslog(LOG_ERR, I18N_FUNC_FAIL_M, fname, "malloc");
        ls_syslog(LOG_ERR, I18N_Exiting);
        exit(-1);
    }

    nDeadPids = 0;
    for (i = 0; i < npgids; i++) {

        if (pg[i].deadSTime || pg[i].deadUTime) {
            memset((char *) &deadPid[nDeadPids], 0, sizeof(struct lsPidInfo));
            deadPid[nDeadPids].pid = -1;
            deadPid[nDeadPids].pgid = pg[i].pgid;
            deadPid[nDeadPids].utime = pg[i].deadUTime;
            deadPid[nDeadPids].stime = pg[i].deadSTime;
            nDeadPids++;
        }
        FREEUP(pg[i].pid);
    }

    FREEUP(pg);

}

static struct pgidRec *
newPGidList(int *npgids)
{
    static char fname[] = "newPGidList";
    struct pgidRec *pg;
    int i, j;

    *npgids = 0;

    pg = (struct pgidRec *) malloc(nr_of_processes * sizeof(struct pgidRec));
    if (pg == NULL) {
        ls_syslog(LOG_ERR, I18N_FUNC_FAIL_M, fname, "malloc");
        ls_syslog(LOG_ERR, I18N_Exiting);
        exit(-1);
    }

    for (i = 0; i < nr_of_processes; i++) {
        for (j = 0; j < *npgids; j++) {
            if (pbase[i].pgid == pg[j].pgid)
                break;
        }

        if (j == *npgids) {

            if ((pg[*npgids].pid = (int *) malloc(NPIDS_SIZE * sizeof(int)))
                    == NULL) {
                ls_syslog(LOG_ERR, I18N_FUNC_FAIL_M, fname, "malloc");
                ls_syslog(LOG_ERR, I18N_Exiting);
                exit(-1);
            }
            pg[*npgids].npids = 1;
            pg[*npgids].pid[0] = pbase[i].pid;
            pg[*npgids].deadSTime = 0;
            pg[*npgids].deadUTime = 0;
            pg[*npgids].pgid = pbase[i].pgid;
            (*npgids)++;
        } else {

            if (pg[j].npids % NPIDS_SIZE == 0) {
                if ((pg[j].pid = (int *) realloc((char *) pg[j].pid,
                                (pg[j].npids + NPIDS_SIZE) *
                                sizeof(int))) == NULL) {
                    ls_syslog(LOG_ERR, I18N_FUNC_FAIL_M, fname, "realloc");
                    ls_syslog(LOG_ERR, I18N_Exiting);
                    exit(-1);
                }
            }
            pg[j].pid[pg[j].npids] = pbase[i].pid;
            pg[j].npids++;
        }
    }

    return (pg);
}

static int
pidInGroup(struct lsPidInfo *p, struct pgidRec *pg)
{
    int k;

    if (p->pgid != pg->pgid)
        return FALSE;

    for (k = 0; k < pg->npids; k++) {

        if (p->pid == pg->pid[k])
            return (TRUE);
    }

    pg->deadUTime += p->utime;
    pg->deadSTime += p->stime;

    return (TRUE);
}

static void
replayDeadPids(void)
{
    FILE *fp;
    struct lsPidInfo pinfo;
    int port;
    static char fname[] = "replayDeadPids";
    struct stat st;

    if (stat(infoFN, &st) == -1) {
        ls_syslog(LOG_DEBUG, "%s: stat(%s) failed: %m, not doing replay",
                fname, infoFN);
        return;
    }

    if ((fp = fopen(infoFN, "r")) == NULL) {
        ls_syslog(LOG_DEBUG, "%s: fopen(%s) failed: %m, not doing replay",
                fname, infoFN);
        return;
    }

    fscanf(fp, "%d", &port);

    while (fscanf(fp, "%d %d %d %d %d %d %d %d %d %d %d %d",
                &pinfo.pid, &pinfo.ppid, &pinfo.pgid, &pinfo.jobid,
                &pinfo.utime, &pinfo.stime, &pinfo.cutime,
                &pinfo.cstime, &pinfo.proc_size,
                &pinfo.resident_size, &pinfo.stack_size,
                (int *) &pinfo.status) != EOF) {
        if (pinfo.pid == -1)
            newDeadPid(&pinfo);
    }

    fclose(fp);
}

static void
newDeadPid(struct lsPidInfo *pinfo)
{
    static char fname[] = "newDeadPid";

    if (deadPid == NULL) {
        deadPid = (struct lsPidInfo *) malloc(NPIDS_SIZE *
                sizeof(struct lsPidInfo));
        if (deadPid == NULL) {
            ls_syslog(LOG_ERR, I18N_FUNC_FAIL_M, fname, "malloc");
            ls_syslog(LOG_ERR, I18N_Exiting);
            exit(-1);
        }
    } else {
        if (nDeadPids % NPIDS_SIZE == 0) {
            deadPid = (struct lsPidInfo *) realloc(deadPid,
                    (nDeadPids + NPIDS_SIZE)*sizeof(struct lsPidInfo));
            if (deadPid == NULL) {
                ls_syslog(LOG_ERR, I18N_FUNC_FAIL_M, fname, "realloc");
                ls_syslog(LOG_ERR, I18N_Exiting);
                exit(-1);
            }
        }
    }

    memcpy((char *) &deadPid[nDeadPids], (char *) pinfo,
            sizeof(struct lsPidInfo));

    nDeadPids++;
}

void
newPGid(int inPGid)
{
    static char fname[] = "newPGid";

    if (inPGid <= 0) return;

    if (pgidList == NULL) {
        pgidList = (struct lspgidEnt *) malloc(NPGIDS_SIZE * sizeof(struct lspgidEnt));
        if (pgidList == NULL) {
            ls_syslog(LOG_ERR, I18N_FUNC_FAIL_M, fname, "malloc");
            ls_syslog(LOG_ERR, I18N_Exiting);
            exit(-1);
        }
    } else {
        if (nPGidList % NPGIDS_SIZE == 0) {
            pgidList = (struct lspgidEnt *) realloc(pgidList,
                    (nPGidList + NPGIDS_SIZE)*sizeof(struct lspgidEnt));
            if (pgidList == NULL) {
                ls_syslog(LOG_ERR, I18N_FUNC_FAIL_M, fname, "realloc");
                ls_syslog(LOG_ERR, I18N_Exiting);
                exit(-1);
            }
        }
    }

    pgidList[nPGidList].pgid = inPGid;
    pgidList[nPGidList].active = TRUE;

    nPGidList++;
}

int
pidInPGList(int inPGid)
{
    int k;

    if (inPGid <= 0)
        return (FALSE);

    for (k = 0; k < nPGidList; k++) {

        if (inPGid == pgidList[k].pgid) {
            pgidList[k].active = TRUE;
            return (TRUE);
        }
    }

    return (FALSE);
}

int
initPGidList(void)
{
    int i;

    for (i=0;i<nPGidList;i++)
        pgidList[i].active = FALSE;
    return(TRUE);
}

int
cleanPGidList(void)
{
    struct lspgidEnt *pgidList_p;
    int nPGidList_p;
    int i;

    if (nPGidList == 0) {
        return(TRUE);
    }

    i=0;
    while ((i<nPGidList)&&(pgidList[i].active == TRUE))
        i ++;

    if (i>=nPGidList) {
        return(TRUE);
    }

    pgidList_p = pgidList;
    nPGidList_p= nPGidList;

    pgidList = NULL;
    nPGidList= 0;

    for (i=0;i<nPGidList_p;i++) {
        if (pgidList_p[i].active)
            newPGid(pgidList_p[i].pgid);
    }

    FREEUP(pgidList_p);

    return(TRUE);
}

static void
sortPids(void)
{
    static char fname[] = "sortPids";
    int i, initPidIdx = 0;

    for (i = 0; i < nr_of_processes; i++) {
        if (pbase[i].pid < pbase[initPidIdx].pid)
            initPidIdx = i;

        getChildren(i);
    }

    if (logclass & LC_PIM)
        ls_syslog(LOG_DEBUG2, "%s: sortIt initPidIdx=%d", fname, initPidIdx);

    sortedIdx = 0;
    sortIt(initPidIdx);

}

static void
getChildren(int ppidIdx)
{
    static char fname[] = "getChildren";
    int lastSibling = ppidIdx;
    int i;

    if (logclass & LC_PIM)
        ls_syslog(LOG_DEBUG2, "%s: ppidIdx=%d pbase.pid=%d pbase.ppid=%d",
                fname, ppidIdx, pbase[ppidIdx].pid, pbase[ppidIdx].ppid);

    for (i = 0; i < nr_of_processes; i++) {
        if (i == ppidIdx)
            continue;

        if (pbase[i].ppid == pbase[ppidIdx].pid) {
            if (logclass & LC_PIM)
                ls_syslog(LOG_DEBUG2,
                        "\ti=%d pid=%d maybe a child, sibling=%d",
                        i, pbase[i].pid, pidLink[i].sibling);

            if (pidLink[i].sibling == -2) {

                if (logclass & LC_PIM)
                    ls_syslog(LOG_DEBUG2, "\tadding as a child");

                if (lastSibling == ppidIdx)
                    pidLink[ppidIdx].child = i;
                else
                    pidLink[lastSibling].sibling = i;
                lastSibling = i;
                pidLink[i].sibling = -1;
            }
        }
    }
}

static void
sortIt(int pIdx)
{
    int i;

    if (logclass & LC_PIM)
        ls_syslog(LOG_DEBUG2,
                "sortIt: sortedIdx %d pIdx = %d pid %d sorted %d",
                sortedIdx, pIdx, pbase[pIdx].pid, pidLink[pIdx].sorted);

    if (pidLink[pIdx].sorted)
        return;

    pidLink[sortedIdx].sortedIdx = pIdx;
    pidLink[pIdx].sorted = TRUE;
    sortedIdx++;

    for (i = pidLink[pIdx].child; i != -1; i = pidLink[i].sibling)
        sortIt(i);
}

int
isNfsDaemon( char *command, char *nfsDaemon )
{
    char *cp, *word;

    if (strstr(command, nfsDaemon) != NULL) {

        cp = command;
        word = getNextWord_(&cp);
        cp = strrchr(word, '/');
        if ( cp == NULL )
            cp = word;
        if (strcmp(cp, nfsDaemon) == 0) {
            return(TRUE);
        }
    }
    return(FALSE);
}

static void
freePidHT(void*p) {}

static void
cpuTimeCheck(const time_t lastUpdate)
{
    static char fname[] = "cpuTimeCheck()";
    int i, j;
    time_t clockTime;
    struct hTab pidHT;
    char   pidStr[20];
    int    new;

    if (lastUpdate == 0)
        return;

    clockTime = time(0) - lastUpdate;

    h_initTab_(&pidHT, 64);

    for (j = 0; j < old_nr_of_processes; j++) {
        hEnt *ent;
        sprintf(pidStr, "%d", old_pbase[j].pid);
        ent = h_addEnt_(&pidHT, pidStr, &new);
        ent->hData = (int*)j;
    }

    for (i = 0; i < nr_of_processes; i++) {
        hEnt *ent;
        sprintf(pidStr, "%d", pbase[i].pid);
        if ((ent = h_getEnt_(&pidHT, pidStr))) {
            int j = (int)ent->hData;

            if (((pbase[i].utime + pbase[i].stime)
                        - (old_pbase[j].utime + old_pbase[j].stime)) > 2*clockTime) {
                if (logclass & LC_PIM)
                    ls_syslog(LOG_DEBUG, "%s: pid <%d> utime <%d> stime <%d>, old_utime <%d> old_stime <%d>, clockTime <%d>",
                            fname, pbase[i].pid, pbase[i].utime, pbase[i].stime,
                            old_pbase[j].utime, old_pbase[j].stime, clockTime);
                pbase[i].utime = old_pbase[j].utime + clockTime/2;
                pbase[i].stime = old_pbase[j].stime + clockTime/2;
            }
        } else {
            if ((pbase[i].utime + pbase[i].stime) > 2*clockTime) {
                if (logclass & LC_PIM)
                    ls_syslog(LOG_DEBUG, "%s: new pid <%d> utime <%d> stime <%d>, clockTime <%d>",
                            fname, pbase[i].pid, pbase[i].utime, pbase[i].stime, clockTime);
                pbase[i].utime = clockTime/2;
                pbase[i].stime = clockTime/2;
            }
        }
    }

    h_freeTab_(&pidHT, freePidHT);
}
