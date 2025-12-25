/* $Id: lim.rload.c,v 1.8 2007/08/15 22:18:54 tmizan Exp $
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

#define MAXIDLETIME 15552000
extern float *extraload;

float k_hz;
static FILE *lim_popen(char **, char *);
static int lim_pclose(FILE *);

static time_t getXIdle(void);

pid_t elim_pid = -1;

int defaultRunElim = false;

static float overRide[NBUILTINDEX];

int ELIMrestarts = -1;
int ELIMdebug = 0;
int ELIMblocktime = -1;

extern int maxnLbHost;
float initLicFactor(void);
static void setUnkwnValues(void);

/* Used in lim.rload.c and lim.policy.c
 */
int ncpus = 1;

extern char *getExtResourcesVal(char *);
extern int blockSigs_(int, sigset_t *, sigset_t *);
static void unblockSigs_(sigset_t *);
static int callElim(void);
static int startElim(void);
static void termElim(void);
static int isResourceSharedInAllHosts(char *);

void satIndex(void)
{
    int i;

    for (i = 0; i < allInfo.numIndx; i++)
        li[i].satvalue = myHostPtr->busyThreshold[i];
}

void loadIndex(void)
{
    li[R15S].exchthreshold += 0.05 * (myHostPtr->statInfo.maxCpus - 1);
    li[R1M].exchthreshold += 0.04 * (myHostPtr->statInfo.maxCpus - 1);
    li[R15M].exchthreshold += 0.03 * (myHostPtr->statInfo.maxCpus - 1);
}

void smooth(float *val, float instant, float factor)
{
    (*val) = ((*val) * factor) + (instant * (1 - factor));
}

time_t getutime(char *usert)
{
    struct stat ttystatus;
    char buffer[MAXPATHLEN];
    time_t t;
    time_t lastinputtime;

    if (strchr(usert, ':') != NULL)
        return MAXIDLETIME;

    strcpy(buffer, "/dev/");
    strcat(buffer, usert);

    if (stat(buffer, &ttystatus) < 0) {
        ls_syslog(LOG_DEBUG, "getutime: stat(%s) failed: %m", buffer);
        return MAXIDLETIME;
    }
    lastinputtime = ttystatus.st_atime;

    time(&t);
    if (t < lastinputtime)
        return (time_t) 0;
    else
        return (t - lastinputtime);
}

#define IDLE_INTVL 30
#define GUESS_NUM 30
time_t lastActiveTime = 0;

#define ENV_LAST_ACTIVE_TIME "LSF_LAST_ACTIVE_TIME"

void putLastActiveTime(void)
{
    char lsfLastActiveTime[MAXLINELEN];

    sprintf(lsfLastActiveTime, "%ld", lastActiveTime);
    if (putEnv(ENV_LAST_ACTIVE_TIME, lsfLastActiveTime) != 0) {
        ls_syslog(LOG_WARNING, "putLastActiveTime: %s, failed.",
                  lsfLastActiveTime);
    }
}

void getLastActiveTime(void)
{
    char *lsfLastActiveTime = NULL;

    lsfLastActiveTime = getenv(ENV_LAST_ACTIVE_TIME);

    if (lsfLastActiveTime != NULL && lsfLastActiveTime[0] != '\0') {
        lastActiveTime = (time_t) atol(lsfLastActiveTime);

        if (lastActiveTime < 0) {
            time(&lastActiveTime);
        }

        putEnv(ENV_LAST_ACTIVE_TIME, "");
    } else {
        time(&lastActiveTime);
    }
}

float idletime(int *logins)
{
    static char fname[] = "idletime()";
    time_t itime;
    static time_t idleSeconds;
    static int idcount;
    static int last_logins;
    time_t currentTime;
    int numusers;
    int ufd;
    struct utmp user;
    char **users;
    char excused_ls = false;
    int listsize = GUESS_NUM;
    char *thisHostname;
    int i;
    bool_t firstLoop;

    if ((thisHostname = ls_getmyhostname()) == NULL) {
        ls_syslog(LOG_ERR, "%s: %s failed: %m", fname, "ls_getmyhostname");
        thisHostname = "localhost";
    }

    time(&currentTime);
    idleSeconds = currentTime - lastActiveTime;
    if (idleSeconds < 0) {
        idleSeconds = 0;
    }

    if (idcount >= (IDLE_INTVL / exchIntvl))
        idcount = 0;

    idcount++;
    if (idcount != 1) {
        *logins = last_logins;
        return (idleSeconds / 60.0);
    }

    if ((ufd = open(UTMP_FILE, O_RDONLY)) < 0) {
        ls_syslog(LOG_WARNING, "%s: %s(%s) failed: %m", fname, "open",
                  UTMP_FILE);
        *logins = last_logins;
        return (MAXIDLETIME / 60.0);
    }

    numusers = 0;
    users = (char **) calloc(listsize, sizeof(char *));
    if (users == NULL) {
        ls_syslog(LOG_ERR, "%s: %s failed: %m", fname, "calloc");
        excused_ls = true;
    }

    firstLoop = true;

    while (read(ufd, (char *) &user, sizeof user) == sizeof user) {
        if (user.ut_name[0] == 0 || (user.ut_type != USER_PROCESS))
            continue;
        else {
            char *ut_name;
            if (!(ut_name =
                      malloc((sizeof(user.ut_name) + 1) * sizeof(char)))) {
                ls_syslog(LOG_ERR, "%s: %s failed: %m", fname, "malloc");
                lim_Exit(fname);
            }
            memset(ut_name, '\0', (sizeof(user.ut_name) + 1));
            strncpy(ut_name, user.ut_name, sizeof(user.ut_name));
            ut_name[sizeof(user.ut_name)] = '\0';

            if (!excused_ls) {
                for (i = 0; i < numusers; i++) {
                    if (strcmp(ut_name, users[i]) == 0)
                        break;
                }
                if (i >= numusers) {
                    users[numusers] = putstr_(ut_name);
                    if (users[numusers] == NULL) {
                        ls_syslog(LOG_ERR, "%s: %s(%s) failed: %m", fname,
                                  "putstr_", ut_name);
                        excused_ls = true;
                        for (i = 0; i < numusers; i++)
                            FREEUP(users[i]);
                        FREEUP(users);
                    } else {
                        numusers++;
                        if (numusers >= listsize) {
                            char **sp;
                            listsize = 2 * listsize;
                            sp = (char **) realloc(users,
                                                   listsize * sizeof(char *));
                            if (sp == NULL) {
                                ls_syslog(LOG_ERR, "%s: %s failed: %m", fname,
                                          "realloc");
                                for (i = 0; i < numusers; i++)
                                    FREEUP(users[i]);
                                FREEUP(users);
                                excused_ls = true;
                            } else {
                                users = sp;
                            }
                        }
                    }
                }
            }

            user.ut_line[sizeof(user.ut_line)] = '\0';

            if (idleSeconds > 0) {
                itime = getutime(user.ut_line);
                if (firstLoop == true) {
                    idleSeconds = itime;
                    firstLoop = false;
                } else {
                    if (itime < idleSeconds)
                        idleSeconds = itime;
                }
            }
            FREEUP(ut_name);
        }
    }
    close(ufd);

    if (idleSeconds > 0 && (itime = getXIdle()) < idleSeconds)
        idleSeconds = itime;

    if (excused_ls)
        *logins = last_logins;
    else {
        *logins = numusers;
        last_logins = numusers;
        for (i = 0; i < numusers; i++)
            FREEUP(users[i]);
        FREEUP(users);
    }

    time(&currentTime);
    if ((currentTime - idleSeconds) > lastActiveTime) {
        lastActiveTime = currentTime - idleSeconds;
    } else {
        idleSeconds = currentTime - lastActiveTime;
    }
    return (idleSeconds / 60.0);
}

time_t getXIdle()
{
    static char fname[] = "getXIdle()";
    time_t lastTime = 0;
    time_t t;

    struct stat st;

    if (stat("/dev/kbd", &st) == 0) {
        if (lastTime < st.st_atime)
            lastTime = st.st_atime;
    } else {
        if (errno != ENOENT)

            ls_syslog(LOG_ERR, "%s: %s(%s) failed: %m", fname, "stat",
                      "/dev/kbd");
    }

    if (stat("/dev/mouse", &st) == 0) {
        if (lastTime < st.st_atime)
            lastTime = st.st_atime;
    } else {
        if (errno != ENOENT)

            ls_syslog(LOG_ERR, "%s: %s(%s) failed: %m", fname, "stat",
                      "/dev/mouse");
    }

    time(&t);
    if (t < lastTime)
        return (time_t) 0;
    else
        return (t - lastTime);
}

void readLoad(void)
{
    int i;

    /* 1. Refresh local host load indexes from /proc */
    lim_proc_read_load();

    /* 2. For now, we don't derive "busy" from thresholds.
     *    Just clear the LIM_BUSY summary bit.
     */
    myHostPtr->status[0] &= ~LIM_BUSY;

    /* 3. User lock handling stays for now */
    if (LOCK_BY_USER(limLock.on)) {
        if (time(0) > limLock.time) {
            limLock.on &= ~LIM_LOCK_STAT_USER;
            limLock.time = 0;
            mustSendLoad = true;
            myHostPtr->status[0] &= ~LIM_LOCKEDU;
        } else {
            myHostPtr->status[0] |= LIM_LOCKEDU;
        }
    }

    myHostPtr->loadMask = 0;

    /* 4. Send current load to the master */
    TIMEIT(0, sendLoad(), "sendLoad()");

    /* 5. Export raw values to uloadIndex for clients */
    for (i = 0; i < allInfo.numIndx; i++) {
        myHostPtr->uloadIndex[i] = myHostPtr->loadIndex[i];
    }
}

static FILE *lim_popen(char **argv, char *mode)
{
    int p[2], pid, i;

    if (mode[0] != 'r')
        return NULL;

    if (pipe(p) < 0)
        return NULL;

    if ((pid = fork()) == 0) {
        char *resEnv;
        resEnv = getElimRes();
        if (resEnv != NULL) {
            if (logclass & LC_TRACE)
                ls_syslog(LOG_DEBUG, "lim_popen: LS_ELIM_RESOURCES <%s>",
                          resEnv);
            putEnv("LS_ELIM_RESOURCES", resEnv);
        }
        close(p[0]);
        dup2(p[1], 1);

        alarm(0);

        for (i = 2; i < sysconf(_SC_OPEN_MAX); i++)
            close(i);
        for (i = 1; i < NSIG; i++)
            signal_set(i, SIG_DFL);

        execvp(argv[0], argv);
        ls_syslog(LOG_ERR, "%s: execvp() failed: %m", argv[0]);
        exit(127);
    }
    if (pid == -1) {
        close(p[0]);
        close(p[1]);
        return NULL;
    }

    elim_pid = pid;
    close(p[1]);

    return fdopen(p[0], mode);
}

static int lim_pclose(FILE *ptr)
{
    sigset_t omask, newmask;
    pid_t child;

    child = elim_pid;
    elim_pid = -1;
    if (ptr)
        fclose(ptr);
    if (child == -1)
        return -1;

    kill(child, SIGTERM);

    sigemptyset(&newmask);
    sigaddset(&newmask, SIGINT);
    sigaddset(&newmask, SIGQUIT);
    sigaddset(&newmask, SIGHUP);
    sigprocmask(SIG_BLOCK, &newmask, &omask);

    sigprocmask(SIG_SETMASK, &omask, NULL);

    return 0;
}

static int saveIndx(char *name, float value)
{
    static char fname[] = "saveIndx()";
    static char **names;
    int indx, i;

    if (!names) {
        if (!(names =
                  (char **) malloc((allInfo.numIndx + 1) * sizeof(char *)))) {
            ls_syslog(LOG_ERR, "%s: %s failed: %m", fname, "malloc");
            lim_Exit(fname);
        }
        memset(names, 0, (allInfo.numIndx + 1) * sizeof(char *));
    }
    indx = getResEntry(name);

    if (indx < 0) {
        for (i = NBUILTINDEX; names[i] && i < allInfo.numIndx; i++) {
            if (strcmp(name, names[i]) == 0)
                return 0;
        }

        ls_syslog(LOG_ERR, "%s: Unknown index name %s from ELIM", fname, name);
        if (names[i]) {
            FREEUP(names[i]);
        }
        names[i] = putstr_(name);
        return 0;
    }

    if (allInfo.resTable[indx].valueType != LS_NUMERIC ||
        indx >= allInfo.numIndx) {
        return 0;
    }

    if (indx < NBUILTINDEX) {
        if (!names[indx]) {
            names[indx] = allInfo.resTable[indx].name;
            ls_syslog(LOG_WARNING, "%s: ELIM over-riding value of index %s",
                      fname, name);
        }
        overRide[indx] = value;
    } else
        myHostPtr->loadIndex[indx] = value;

    return 0;
}

static int getSharedResBitPos(char *resName)
{
    struct sharedResourceInstance *tmpSharedRes;
    int bitPos;

    if (resName == NULL)
        return -1;

    for (tmpSharedRes = sharedResourceHead, bitPos = 0; tmpSharedRes;
         tmpSharedRes = tmpSharedRes->nextPtr, bitPos++) {
        if (!strcmp(resName, tmpSharedRes->resName)) {
            return bitPos;
        }
    }
    return -1;
}

static void getExtResourcesLoad(void)
{
    int i, isSet, bitPos;
    char *resName, *resValue;
    float fValue;

    for (i = 0; i < allInfo.nRes; i++) {
        if (allInfo.resTable[i].flags & RESF_DYNAMIC &&
            allInfo.resTable[i].flags & RESF_EXTERNAL) {
            resName = allInfo.resTable[i].name;

            if (!defaultRunElim) {
                if ((bitPos = getSharedResBitPos(resName)) == -1)
                    continue;
                TEST_BIT(bitPos, myHostPtr->resBitArray, isSet)
                if (!isSet)
                    continue;
            }
            if ((resValue = getExtResourcesVal(resName)) == NULL)
                continue;

            if (saveSBValue(resName, resValue) == 0)
                continue;
            fValue = atof(resValue);

            saveIndx(resName, fValue);
        }
    }
}

int isResourceSharedByHost(struct hostNode *host, char *resName)
{
    int i;
    for (i = 0; i < host->numInstances; i++) {
        if (strcmp(host->instances[i]->resName, resName) == 0) {
            return true;
        }
    }
    return false;
}

#define timersub(a, b, result)                                                 \
    do {                                                                       \
        (result)->tv_sec = (a)->tv_sec - (b)->tv_sec;                          \
        (result)->tv_usec = (a)->tv_usec - (b)->tv_usec;                       \
        if ((result)->tv_usec < 0) {                                           \
            --(result)->tv_sec;                                                \
            (result)->tv_usec += 1000000;                                      \
        }                                                                      \
    } while (0)

#define ELIMNAME "elim"
#define MAXEXTRESLEN 4096

void getusr(void)
{
    static char fname[] = "getusr";
    static FILE *fp;
    static time_t lastStart;
    static char first = true;
    int i, nfds;
    int size;
    struct sharedResourceInstance *tmpSharedRes;
    struct timeval t, expire;
    struct timeval time0 = {0, 0};
    int bw;

    if (first) {
        for (i = 0; i < NBUILTINDEX; i++)
            overRide[i] = INFINITY;
        first = false;
    }
    if (!callElim()) {
        return;
    }

    getExtResourcesLoad();

    if (!startElim()) {
        return;
    }

    if ((elim_pid < 0) && (time(0) - lastStart > 90)) {
        if (ELIMrestarts < 0 || ELIMrestarts > 0) {
            if (ELIMrestarts > 0) {
                ELIMrestarts--;
            }

            if (!myClusterPtr->eLimArgv) {
                char *path =
                    malloc(strlen(genParams[LSF_SERVERDIR].paramValue) +
                           strlen(ELIMNAME) + 8);
                if (!path) {
                    ls_syslog(LOG_ERR, "%s: %s failed: %m", fname, "malloc");
                    setUnkwnValues();
                    return;
                }
                strcpy(path, genParams[LSF_SERVERDIR].paramValue);
                strcat(path, "/");
                strcat(path, ELIMNAME);

                if (logclass & LC_EXEC) {
                    ls_syslog(LOG_DEBUG, "%s : the elim's name is <%s>\n",
                              fname, path);
                }

                myClusterPtr->eLimArgv =
                    parseCommandArgs(path, myClusterPtr->eLimArgs);
                if (!myClusterPtr->eLimArgv) {
                    ls_syslog(LOG_ERR, "%s: %s failed: %m", fname, "malloc");
                    setUnkwnValues();
                    return;
                }
            }

            if (fp) {
                fclose(fp);
                fp = NULL;
            }

            lastStart = time(0);

            size = 0;
            for (tmpSharedRes = sharedResourceHead; tmpSharedRes;
                 tmpSharedRes = tmpSharedRes->nextPtr) {
                size += strlen(tmpSharedRes->resName) + sizeof(char);
            }
            for (i = NBUILTINDEX; i < allInfo.nRes; i++) {
                if (allInfo.resTable[i].flags & RESF_EXTERNAL)
                    continue;
                if ((allInfo.resTable[i].flags & RESF_DYNAMIC) &&
                    !(allInfo.resTable[i].flags & RESF_BUILTIN)) {
                    size += strlen(allInfo.resTable[i].name) + sizeof(char);
                }
            }
            char *resStr = malloc((size + 1) * sizeof(char));
            if (!resStr) {
                ls_syslog(LOG_ERR, "%s: %s failed: %m", fname, "malloc");
                setUnkwnValues();
                return;
            }
            resStr[0] = '\0';

            for (i = NBUILTINDEX; i < allInfo.nRes; i++) {
                if (allInfo.resTable[i].flags & RESF_EXTERNAL)
                    continue;
                if ((allInfo.resTable[i].flags & RESF_DYNAMIC) &&
                    !(allInfo.resTable[i].flags & RESF_BUILTIN)) {
                    if ((allInfo.resTable[i].flags & RESF_SHARED) &&
                        (!masterMe) &&
                        (isResourceSharedInAllHosts(
                            allInfo.resTable[i].name))) {
                        continue;
                    }

                    if ((allInfo.resTable[i].flags & RESF_SHARED) &&
                        (!isResourceSharedByHost(myHostPtr,
                                                 allInfo.resTable[i].name)))
                        continue;

                    if (resStr[0] == '\0')
                        sprintf(resStr, "%s", allInfo.resTable[i].name);
                    else {
                        sprintf(resStr, "%s %s", resStr,
                                allInfo.resTable[i].name);
                    }
                }
            }
            putEnv("LSF_RESOURCES", resStr);

            if ((fp = lim_popen(myClusterPtr->eLimArgv, "r")) == NULL) {
                ls_syslog(LOG_ERR, "%s: %s(%s) failed: %m", fname, "lim_popen",
                          myClusterPtr->eLimArgv[0]);
                setUnkwnValues();

                return;
            }
            ls_syslog(LOG_INFO, ("%s: Started ELIM %s pid %d"), fname,
                      myClusterPtr->eLimArgv[0], (int) elim_pid);
            mustSendLoad = true;
        }
    }

    if (elim_pid < 0) {
        setUnkwnValues();
        if (fp) {
            fclose(fp);
            fp = NULL;
        }

        return;
    }
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 5;

    // Initialize poll data structure
    struct pollfd pfd = {.fd = fileno(fp), .events = POLLOUT};
    int tm = 60;

    if ((nfds = poll(&pfd, 1, tm * 1000)) < 0) {
        ls_syslog(LOG_ERR, "%s: %s failed: %m", fname, "rd_select_");
        lim_pclose(fp);
        fp = NULL;

        return;
    }

    if (nfds == 1) {
        int numIndx, cc;
        char name[MAXLSFNAMELEN], valueString[MAXEXTRESLEN];
        float value;
        sigset_t oldMask;
        sigset_t newMask;

        static char *fromELIM = NULL;
        static int sizeOfFromELIM = MAXLINELEN;
        char *elimPos;
        int spaceLeft, spaceRequired;

        if (!fromELIM) {
            fromELIM = malloc(sizeOfFromELIM);
            if (!fromELIM) {
                ls_syslog(LOG_ERR, "%s: %s failed: %m", fname, "malloc");

                ls_syslog(
                    LOG_ERR,
                    "%s:Received from ELIM: <out of memory to record contents>",
                    fname);

                setUnkwnValues();
                lim_pclose(fp);
                fp = NULL;
                return;
            }
        }

        elimPos = fromELIM;
        *elimPos = '\0';

        blockSigs_(0, &newMask, &oldMask);

        if (logclass & LC_TRACE) {
            ls_syslog(LOG_DEBUG, "\
                    %s: Signal mask has been changed, all are signals blocked now",
                      fname);
        }

        if (ELIMblocktime >= 0) {
            io_nonblock_(fileno(fp));
        }

        cc = fscanf(fp, "%d", &numIndx);
        if (cc != 1) {
            ls_syslog(LOG_ERR, "%s: Protocol error numIndx not read (cc=%d: %m",
                      fname, cc);
            lim_pclose(fp);
            fp = NULL;
            unblockSigs_(&oldMask);

            return;
        }

        bw = sprintf(elimPos, "%d ", numIndx);
        elimPos += bw;

        if (numIndx < 0) {
            ls_syslog(LOG_ERR, "%s: Protocol error numIndx=%d", fname, numIndx);
            setUnkwnValues();
            lim_pclose(fp);
            fp = NULL;
            unblockSigs_(&oldMask);
            return;
        }

        if (ELIMblocktime >= 0) {
            gettimeofday(&t, NULL);
            expire.tv_sec = t.tv_sec + ELIMblocktime;
            expire.tv_usec = t.tv_usec;
        }

        i = numIndx * 2;
        while (i) {
            if (i % 2) {
                cc = fscanf(fp, "%4096s", valueString);
                valueString[MAXEXTRESLEN - 1] = '\0';
            } else {
                cc = fscanf(fp, "%40s", name);
                name[MAXLSFNAMELEN - 1] = '\0';
            }

            if (cc == -1) {
                int scanerrno = errno;
                if (scanerrno == EAGAIN) {
                    gettimeofday(&t, NULL);
                    timersub(&expire, &t, &timeout);
                    if (timercmp(&timeout, &time0, <)) {
                        timerclear(&timeout);
                    }
                    // scc = rd_select_(fileno(fp), &timeout);
                }
                int scc = 0;
                if (scanerrno != EAGAIN || scc <= 0) {
                    ls_syslog(
                        LOG_ERR,
                        "%s: Protocol error, expected %d more tokens cc=%d: %m",
                        fname, i, cc);

                    ls_syslog(LOG_ERR, "Received from ELIM: %s.", fromELIM);

                    setUnkwnValues();
                    lim_pclose(fp);
                    fp = NULL;
                    unblockSigs_(&oldMask);
                    return;
                }

                continue;
            }

            spaceLeft = sizeOfFromELIM - (elimPos - fromELIM) - 1;
            spaceRequired = strlen((i % 2) ? valueString : name) + 1;

            if (spaceLeft < spaceRequired) {
                char *oldFromElim = fromELIM;
                int oldSizeOfFromELIM = sizeOfFromELIM;

                sizeOfFromELIM += (spaceRequired - spaceLeft);

                fromELIM = realloc(fromELIM, sizeOfFromELIM);
                if (!fromELIM) {
                    ls_syslog(LOG_ERR, "%s: %s failed: %m", fname, "malloc");

                    ls_syslog(LOG_ERR,
                              "%s:Received from ELIM: <out of memory to record "
                              "contents>",
                              fname);

                    sizeOfFromELIM = oldSizeOfFromELIM;
                    fromELIM = oldFromElim;

                    setUnkwnValues();
                    lim_pclose(fp);
                    fp = NULL;
                    unblockSigs_(&oldMask);
                    return;
                }
                elimPos = fromELIM + strlen(fromELIM);
            }

            bw = sprintf(elimPos, "%s ", (i % 2) ? valueString : name);
            elimPos += bw;

            if (i % 2) {
                if (saveSBValue(name, valueString) == 0) {
                    i--;
                    continue;
                }

                value = atof(valueString);
                saveIndx(name, value);
            }
            i--;
        }

        unblockSigs_(&oldMask);

        if (ELIMdebug) {
            ls_syslog(LOG_WARNING, "ELIM: %s.", fromELIM);
        }
    }
}

void unblockSigs_(sigset_t *mask)
{
    static char fname[] = "unblockSigs_()";

    sigprocmask(SIG_SETMASK, mask, NULL);

    if (logclass & LC_TRACE) {
        ls_syslog(LOG_DEBUG, "\
                %s: The original signal mask has been restored",
                  fname);
    }
}

static void setUnkwnValues(void)
{
    int i;

    for (i = 0; i < allInfo.numUsrIndx; i++)
        myHostPtr->loadIndex[NBUILTINDEX + i] = INFINITY;
    for (i = 0; i < NBUILTINDEX; i++)
        overRide[i] = INFINITY;

    for (i = 0; i < myHostPtr->numInstances; i++) {
        if (myHostPtr->instances[i]->updateTime == 0 ||
            myHostPtr->instances[i]->updHost == NULL)

            continue;
        if (myHostPtr->instances[i]->updHost == myHostPtr) {
            strcpy(myHostPtr->instances[i]->value, "-");
            myHostPtr->instances[i]->updHost = NULL;
            myHostPtr->instances[i]->updateTime = 0;
        }
    }
}

int saveSBValue(char *name, char *value)
{
    static char fname[] = "saveSBValue()";
    int i, indx, j, myHostNo = -1, updHostNo = -1;
    char *temp = NULL;
    time_t currentTime = 0;

    if ((indx = getResEntry(name)) < 0)
        return -1;

    if (!(allInfo.resTable[indx].flags & RESF_DYNAMIC))
        return -1;

    if (allInfo.resTable[indx].valueType == LS_NUMERIC) {
        if (!isanumber_(value)) {
            return -1;
        }
    }

    if (myHostPtr->numInstances <= 0)
        return -1;

    for (i = 0; i < myHostPtr->numInstances; i++) {
        if (strcmp(myHostPtr->instances[i]->resName, name))
            continue;
        if (currentTime == 0)
            currentTime = time(0);
        if (masterMe) {
            for (j = 0; j < myHostPtr->instances[i]->nHosts; j++) {
                if (myHostPtr->instances[i]->updHost &&
                    (myHostPtr->instances[i]->updHost ==
                     myHostPtr->instances[i]->hosts[j]))
                    updHostNo = j;
                if (myHostPtr->instances[i]->hosts[j] == myHostPtr)
                    myHostNo = j;
                if (myHostNo >= 0 && (updHostNo >= 0 ||
                                      myHostPtr->instances[i]->updHost == NULL))
                    break;
            }
            if (updHostNo >= 0 &&
                (myHostNo < 0 || ((updHostNo < myHostNo) &&
                                  strcmp(myHostPtr->instances[i]->value, "-"))))
                return 0;
        }

        if ((temp = (char *) malloc(strlen(value) + 1)) == NULL) {
            ls_syslog(LOG_ERR, "%s: %s failed: %m", fname, "malloc");
            FREEUP(temp);
            return 0;
        }
        strcpy(temp, value);
        FREEUP(myHostPtr->instances[i]->value);
        myHostPtr->instances[i]->value = temp;
        myHostPtr->instances[i]->updateTime = currentTime;
        myHostPtr->instances[i]->updHost = myHostPtr;
        if (logclass & LC_LOADINDX)
            ls_syslog(LOG_DEBUG3,
                      "saveSBValue: i = %d, resName=%s, value=%s, newValue=%s, "
                      "updHost=%s",
                      i, myHostPtr->instances[i]->resName,
                      myHostPtr->instances[i]->value, temp,
                      myHostPtr->instances[i]->updHost->hostName);
        return 0;
    }
    return -1;
}

void initConfInfo(void)
{
   long n = sysconf(_SC_NPROCESSORS_ONLN);

    if (n < 1) {
        LS_INFO("sysconf(_SC_NPROCESSORS_ONLN) returned %ld, using 1", n);
        ncpus = 1;
    } else {
        ncpus = (int)n;
    }

    LS_INFO("Running on %d CPUS", ncpus);

    myHostPtr->statInfo.portno = lim_tcp_port;
    myHostPtr->statInfo.hostNo = myHostPtr->hostNo;
    myHostPtr->infoValid = true;
}

char *getElimRes(void)
{
    int i, numEnv = 0, resNo;
    char *resNameString = NULL;

    if ((resNameString = (char *) malloc((allInfo.nRes) * MAXLSFNAMELEN)) ==
        NULL) {
        ls_syslog(LOG_ERR, "%s: %s(%d) failed: %m", "getElimRes", "malloc",
                  allInfo.nRes * MAXLSFNAMELEN);
        lim_Exit("getElimRes");
    }

    resNameString[0] = '\0';
    for (i = 0; i < allInfo.numIndx; i++) {
        if (allInfo.resTable[i].flags & RESF_EXTERNAL)
            continue;
        if (numEnv != 0)
            strcat(resNameString, " ");
        strcat(resNameString, allInfo.resTable[i].name);
        numEnv++;
    }
    for (i = 0; i < myHostPtr->numInstances; i++) {
        resNo = resNameDefined(myHostPtr->instances[i]->resName);
        if (allInfo.resTable[resNo].flags & RESF_EXTERNAL)
            continue;
        if (allInfo.resTable[resNo].interval > 0) {
            if (numEnv != 0)
                strcat(resNameString, " ");
            strcat(resNameString, myHostPtr->instances[i]->resName);
            numEnv++;
        }
    }
    if (numEnv == 0)
        return NULL;
    else
        return resNameString;
}

static int callElim(void)
{
    static int runit = false;
    static int lastTimeMasterMe = false;

    if (masterMe && !lastTimeMasterMe) {
        lastTimeMasterMe = true;
        if (runit) {
            termElim();
            if (myHostPtr->callElim || defaultRunElim) {
                return true;
            } else {
                runit = false;
                return false;
            }
        }
    }

    if (!masterMe && lastTimeMasterMe) {
        lastTimeMasterMe = false;
        if (runit) {
            termElim();
            if (myHostPtr->callElim || defaultRunElim) {
                return true;
            } else {
                runit = false;
                return false;
            }
        }
    }

    if (masterMe)
        lastTimeMasterMe = true;
    else
        lastTimeMasterMe = false;

    if (runit) {
        if (!myHostPtr->callElim && !defaultRunElim) {
            termElim();
            runit = false;
            return false;
        }
    }

    if (defaultRunElim) {
        runit = true;
        return true;
    }

    if (myHostPtr->callElim) {
        runit = true;
        return true;
    } else {
        runit = false;
        return false;
    }
}

static int startElim(void)
{
    static int notFirst = false, startElim = false;
    int i;

    if (!notFirst) {
        for (i = 0; i < allInfo.nRes; i++) {
            if (allInfo.resTable[i].flags & RESF_EXTERNAL)
                continue;
            if ((allInfo.resTable[i].flags & RESF_DYNAMIC) &&
                !(allInfo.resTable[i].flags & RESF_BUILTIN)) {
                startElim = true;
                break;
            }
        }
        notFirst = true;
    }

    return startElim;
}

static void termElim(void)
{
    if (elim_pid == -1)
        return;

    kill(elim_pid, SIGTERM);
    elim_pid = -1;
}

static int isResourceSharedInAllHosts(char *resName)
{
    struct sharedResourceInstance *tmpSharedRes;

    for (tmpSharedRes = sharedResourceHead; tmpSharedRes;
         tmpSharedRes = tmpSharedRes->nextPtr) {
        if (strcmp(tmpSharedRes->resName, resName)) {
            continue;
        }
        if (tmpSharedRes->nHosts == myClusterPtr->numHosts) {
            return 1;
        }
    }

    return 0;
}
