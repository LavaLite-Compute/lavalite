/* $Id: lsb.sub.c,v 1.12 2007/08/15 22:18:48 tmizan Exp $
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

#include "lsbatch/lib/lsb.h"
#include "lsf/lib/lib.h"
#include "lsbatch/lib/lsb.spool.h"

#define exit(a) _exit(a)
#define SKIPSPACE(sp)                                                          \
    while (isspace(*(sp)))                                                     \
        (sp)++;

#define EMBED_INTERACT 0x01
#define EMBED_OPTION_ONLY 0x02
#define EMBED_BSUB 0x04
#define EMBED_RESTART 0x10
#define EMBED_QSUB 0x20

#define PRINT_ERRMSG0(errMsg, fmt)                                             \
    {                                                                          \
        if (errMsg == NULL)                                                    \
            fprintf(stderr, fmt);                                              \
        else                                                                   \
            sprintf(*errMsg, fmt);                                             \
    }
#define PRINT_ERRMSG1(errMsg, fmt, msg1)                                       \
    {                                                                          \
        if (errMsg == NULL)                                                    \
            fprintf(stderr, fmt, msg1);                                        \
        else                                                                   \
            sprintf(*errMsg, fmt, msg1);                                       \
    }
#define PRINT_ERRMSG2(errMsg, fmt, msg1, msg2)                                 \
    {                                                                          \
        if (errMsg == NULL)                                                    \
            fprintf(stderr, fmt, msg1, msg2);                                  \
        else                                                                   \
            sprintf(*errMsg, fmt, msg1, msg2);                                 \
    }
#define PRINT_ERRMSG3(errMsg, fmt, msg1, msg2, msg3)                           \
    {                                                                          \
        if (errMsg == NULL)                                                    \
            fprintf(stderr, fmt, msg1, msg2, msg3);                            \
        else                                                                   \
            sprintf(*errMsg, fmt, msg1, msg2, msg3);                           \
    }

int optionFlag = false;
char optionFileName[MAXLSFNAMELEN];
char *loginShell;
static char *additionEsubInfo = NULL;

extern void sub_perror(char *usrMsg);

static void trimSpaces(char *str);
static int parseXF(struct submit *, char *, char **);
void subUsage_(int, char **);
static int checkLimit(int limit, int factor);

int mySubUsage_(void *);
int bExceptionTabInit(void);

static hTab *bExceptionTab;

typedef int (*bException_handler_t)(void *);

typedef struct bException {
    char *name;
    bException_handler_t handler;
} bException_t;

extern int optind;
extern char *optarg;
extern char *my_getopt(int, char **, char *, char **);
extern char *getNextLine_(FILE *fp, int confFormat);
extern uid_t getuid(void);
extern char *lsb_sysmsg(void);
extern int _lsb_conntimeout;

extern int lsbMode_;

static char *useracctmap = NULL;
static struct lenData ed = {0, NULL};

static int64_t send_batch(struct submitReq *, struct lenData *,
                          struct submitReply *, struct lsfAuth *);
static int dependCondSyntax(char *);
static int createJobInfoFile(struct submit *, struct lenData *);
static int64_t subJob(struct submit *jobSubReq, struct submitReq *submitReq,
                      struct submitReply *submitRep, struct lsfAuth *auth);
static int getUserInfo(struct submitReq *, struct submit *);
static char *acctMapGet(int *, char *);

static int xdrSubReqSize(struct submitReq *req);

int getChkDir(char *, char *);
static void postSubMsg(struct submit *, int64_t, struct submitReply *);
static int readOptFile(char *filename, char *childLine);

extern void makeCleanToRunEsub();
extern char *translateString(char *);
extern void modifyJobInformation(struct submit *);
extern void compactXFReq(struct submit *);
extern char *wrapCommandLine(char *);
extern char *unwrapCommandLine(char *);
extern int checkEmptyString(char *);
extern int stringIsToken(char *, char *);
extern int stringIsDigitNumber(char *s);
extern int processXFReq(char *key, char *line, struct submit *jobSubReq);
extern char *extractStringValue(char *line);
static int getAskedHosts_(char *optarg, char ***askedHosts, int *numAskedHosts,
                          int *badIdx, int checkHost);
#define ESUBNAME "esub"

int64_t lsb_submit(struct submit *jobSubReq, struct submitReply *submitRep)
{
    static char fname[] = "lsb_submit";
    struct submitReq submitReq;
    int64_t jobId = -1;
    char cwd[MAXFILENAMELEN];
    struct group *grpEntry;
    int loop;
    char *queue = NULL;

    if (logclass & (LC_TRACE | LC_EXEC))
        ls_syslog(LOG_DEBUG, "%s: Entering this routine...", fname);

    lsberrno = LSBE_BAD_ARG;

    subNewLine_(jobSubReq->resReq);
    subNewLine_(jobSubReq->dependCond);
    subNewLine_(jobSubReq->preExecCmd);
    subNewLine_(jobSubReq->mailUser);
    subNewLine_(jobSubReq->jobName);
    subNewLine_(jobSubReq->queue);
    subNewLine_(jobSubReq->inFile);
    subNewLine_(jobSubReq->outFile);
    subNewLine_(jobSubReq->errFile);
    subNewLine_(jobSubReq->chkpntDir);
    subNewLine_(jobSubReq->projectName);
    for (loop = 0; loop < jobSubReq->numAskedHosts; loop++) {
        subNewLine_(jobSubReq->askedHosts[loop]);
    }

    if (getCommonParams(jobSubReq, &submitReq, submitRep) < 0)
        return -1;

    if (!(jobSubReq->options & SUB_QUEUE)) {
        if ((queue = getenv("LSB_DEFAULTQUEUE")) != NULL && queue[0] != '\0') {
            submitReq.queue = queue;
            submitReq.options |= SUB_QUEUE;
        }
    }

    submitReq.cwd = cwd;

    if ((grpEntry = getgrgid(getgid())) == NULL) {
        if (logclass & (LC_TRACE | LC_EXEC))
            ls_syslog(
                LOG_DEBUG,
                "%s: group id %d, does not have an name in the unix group file",
                fname, (int) getgid());
    } else {
        if (putEnv("LSB_UNIXGROUP", grpEntry->gr_name) < 0) {
            if (logclass & (LC_TRACE | LC_EXEC))
                ls_syslog(LOG_DEBUG,
                          "%s: group <%s>, cannot be set in the environment.",
                          fname, grpEntry->gr_name);
        }
    }

    makeCleanToRunEsub();

    if (getUserInfo(&submitReq, jobSubReq) < 0)
        return -1;

    if (!(jobSubReq->options & SUB_QUEUE)) {
        if (queue != NULL && queue[0] != '\0') {
            jobSubReq->queue = queue;
            jobSubReq->options |= SUB_QUEUE;
        }
    }

    modifyJobInformation(jobSubReq);
    if (getCommonParams(jobSubReq, &submitReq, submitRep) < 0)
        return -1;

    if ((lsbParams[LSB_INTERACTIVE_STDERR].paramValue != NULL) &&
        (strcasecmp(lsbParams[LSB_INTERACTIVE_STDERR].paramValue, "y") == 0)) {
        if (putEnv("LSF_INTERACTIVE_STDERR", "y") < 0) {
            ls_syslog(LOG_ERR, "%s: %s(%s) failed", fname, "putenv");
        }
    }

    struct lsfAuth auth;
    if (authTicketTokens_(&auth, NULL) == -1) {
        return -1;
    }

    jobId = subJob(jobSubReq, &submitReq, submitRep, &auth);

    return jobId;
}

int getCommonParams(struct submit *jobSubReq, struct submitReq *submitReq,
                    struct submitReply *submitRep)
{
    int i, useKb = 0;
    static char fname[] = "getCommonParams";

    if (logclass & (LC_TRACE | LC_EXEC))
        ls_syslog(LOG_DEBUG, "%s: Entering this routine...", fname);

    if (jobSubReq == NULL || submitRep == NULL)
        return -1;

    submitReq->options = jobSubReq->options;
    submitReq->options2 = jobSubReq->options2;

    if (jobSubReq->options & SUB_DEPEND_COND) {
        if (dependCondSyntax(jobSubReq->dependCond) < 0)
            return -1;
        else {
            submitReq->dependCond = jobSubReq->dependCond;
        }
    } else
        submitReq->dependCond = "";

    if (jobSubReq->options & SUB_PRE_EXEC) {
        if (!jobSubReq->preExecCmd)
            return -1;
        if (strlen(jobSubReq->preExecCmd) >= MAXLINELEN - 1)
            return -1;
        else
            submitReq->preExecCmd = jobSubReq->preExecCmd;
    } else
        submitReq->preExecCmd = "";

    if (jobSubReq->options & SUB_QUEUE) {
        if (!jobSubReq->queue) {
            lsberrno = LSBE_BAD_QUEUE;
            return -1;
        }
        submitReq->queue = jobSubReq->queue;
    } else {
        submitReq->queue = "";
    }

    if (jobSubReq->options & SUB_HOST) {
        submitReq->numAskedHosts = jobSubReq->numAskedHosts;
        submitReq->askedHosts = jobSubReq->askedHosts;
    } else
        submitReq->numAskedHosts = 0;

    if (submitReq->numAskedHosts < 0)
        return -1;

    for (i = 0; i < submitReq->numAskedHosts; i++) {
        if ((submitReq->askedHosts[i]) &&
            (strlen(submitReq->askedHosts[i]) + 1) < MAXHOSTNAMELEN)
            continue;
        lsberrno = LSBE_BAD_HOST;
        submitRep->badReqIndx = i;
        return -1;
    }

    if (jobSubReq->options & SUB_HOST_SPEC) {
        if (!jobSubReq->hostSpec)
            return -1;
        if (strlen(jobSubReq->hostSpec) >= MAXHOSTNAMELEN - 1)
            return -1;
        else
            submitReq->hostSpec = jobSubReq->hostSpec;
    } else
        submitReq->hostSpec = "";

    submitReq->beginTime = jobSubReq->beginTime;
    submitReq->termTime = jobSubReq->termTime;

    if (!limitIsOk_(jobSubReq->rLimits)) {
        useKb = 1;
        submitReq->options |= SUB_RLIMIT_UNIT_IS_KB;
    }

    for (i = 0; i < LSF_RLIM_NLIMITS; i++)
        submitReq->rLimits[i] = -1;

    if (jobSubReq->rLimits[LSF_RLIMIT_CPU] >= 0)
        submitReq->rLimits[LSF_RLIMIT_CPU] = jobSubReq->rLimits[LSF_RLIMIT_CPU];
    if (jobSubReq->rLimits[LSF_RLIMIT_RUN] >= 0)
        submitReq->rLimits[LSF_RLIMIT_RUN] = jobSubReq->rLimits[LSF_RLIMIT_RUN];

    if (jobSubReq->rLimits[LSF_RLIMIT_FSIZE] > 0) {
        if (useKb) {
            submitReq->rLimits[LSF_RLIMIT_FSIZE] =
                jobSubReq->rLimits[LSF_RLIMIT_FSIZE];
        } else {
            submitReq->rLimits[LSF_RLIMIT_FSIZE] =
                jobSubReq->rLimits[LSF_RLIMIT_FSIZE] * 1024;
        }
    }
    if (jobSubReq->rLimits[LSF_RLIMIT_DATA] > 0) {
        if (useKb) {
            submitReq->rLimits[LSF_RLIMIT_DATA] =
                jobSubReq->rLimits[LSF_RLIMIT_DATA];
        } else {
            submitReq->rLimits[LSF_RLIMIT_DATA] =
                jobSubReq->rLimits[LSF_RLIMIT_DATA] * 1024;
        }
    }
    if (jobSubReq->rLimits[LSF_RLIMIT_STACK] > 0) {
        if (useKb) {
            submitReq->rLimits[LSF_RLIMIT_STACK] =
                jobSubReq->rLimits[LSF_RLIMIT_STACK];
        } else {
            submitReq->rLimits[LSF_RLIMIT_STACK] =
                jobSubReq->rLimits[LSF_RLIMIT_STACK] * 1024;
        }
    }
    if (jobSubReq->rLimits[LSF_RLIMIT_CORE] >= 0) {
        if (useKb) {
            submitReq->rLimits[LSF_RLIMIT_CORE] =
                jobSubReq->rLimits[LSF_RLIMIT_CORE];
        } else {
            submitReq->rLimits[LSF_RLIMIT_CORE] =
                jobSubReq->rLimits[LSF_RLIMIT_CORE] * 1024;
        }
    }
    if (jobSubReq->rLimits[LSF_RLIMIT_RSS] > 0) {
        if (useKb) {
            submitReq->rLimits[LSF_RLIMIT_RSS] =
                jobSubReq->rLimits[LSF_RLIMIT_RSS];
        } else {
            submitReq->rLimits[LSF_RLIMIT_RSS] =
                jobSubReq->rLimits[LSF_RLIMIT_RSS] * 1024;
        }
    }
    if (jobSubReq->rLimits[LSF_RLIMIT_SWAP] > 0) {
        if (useKb) {
            submitReq->rLimits[LSF_RLIMIT_SWAP] =
                jobSubReq->rLimits[LSF_RLIMIT_SWAP];
        } else {
            submitReq->rLimits[LSF_RLIMIT_SWAP] =
                jobSubReq->rLimits[LSF_RLIMIT_SWAP] * 1024;
        }
    }

    if (jobSubReq->rLimits[LSF_RLIMIT_PROCESS] > 0)
        submitReq->rLimits[LSF_RLIMIT_PROCESS] =
            jobSubReq->rLimits[LSF_RLIMIT_PROCESS];

    if ((jobSubReq->beginTime > 0 && jobSubReq->termTime > 0) &&
        (submitReq->beginTime > submitReq->termTime)) {
        lsberrno = LSBE_START_TIME;
        return -1;
    }

    submitReq->submitTime = time(0);

    if (jobSubReq->options2 & SUB2_JOB_PRIORITY) {
        submitReq->userPriority = jobSubReq->userPriority;
    } else {
        submitReq->userPriority = -1;
    }

    if (logclass & (LC_TRACE | LC_EXEC))
        ls_syslog(LOG_DEBUG, "%s: Okay", fname);

    return 0;
}
extern char **environ;
static int createJobInfoFile(struct submit *jobSubReq, struct lenData *jf)
{
    static char fname[] = "createJobInfoFile";
    char **ep;
    char *sp, num[LL_BUFSIZ_32], *p, *oldp;
    int size = MSGSIZE, length = 0, len, len1, numEnv = 0, noEqual;

    int tsoptlen;

    tsoptlen = 0;

    if (logclass & (LC_TRACE | LC_EXEC))
        ls_syslog(LOG_DEBUG, "%s: Entering this routine...", fname);

    length = sizeof(CMDSTART) + sizeof(TRAPSIGCMD) + sizeof(WAITCLEANCMD) +
             sizeof(EXITCMD) + strlen(jobSubReq->command) + tsoptlen +
             sizeof(LSBNUMENV) + sizeof(ENVSSTART) + sizeof(EDATASTART) +
             sizeof(SHELLLINE) + 1 + LL_BUFSIZ_32 * 2 + ed.len;

    jf->len = 0;
    size = MAX(length, MSGSIZE);
    if ((jf->data = (char *) malloc(size)) == NULL) {
        lsberrno = LSBE_NO_MEM;
        return -1;
    }
    jf->data[0] = '\0';
    strcat(jf->data, SHELLLINE);

    if (useracctmap) {
        strcat(jf->data, "LSB_ACCT_MAP='");
        strcat(jf->data, useracctmap);
        strcat(jf->data, "'; export LSB_ACCT_MAP\n");
        length += 14 + strlen(useracctmap) + 24;
        free(useracctmap);
        useracctmap = NULL;
    }

    strcat(jf->data, "LSF_VERSION='");
    /* Bug decide what to print here
     */
    sprintf(num, "%d", 1);
    strcat(jf->data, num);
    strcat(jf->data, "'; export LSF_VERSION\n");
    length += 13 + strlen(num) + 23;

    for (ep = environ; *ep; ep++) {
        noEqual = false;
        if (logclass & (LC_TRACE | LC_EXEC)) {
            ls_syslog(LOG_DEBUG, "%s: environment variable <%s>", fname, *ep);
        }

        if (!strncmp(*ep, "LSB_JOBID=", 10) ||
            !strncmp(*ep, "LSB_HOSTS=", 10) ||
            !strncmp(*ep, "LSB_QUEUE=", 10) ||
            !strncmp(*ep, "LSB_JOBNAME=", 12) ||
            !strncmp(*ep, "LSB_TRAPSIGS=", 13) ||
            !strncmp(*ep, "LSB_JOBFILENAME=", 16) ||
            !strncmp(*ep, "LSB_RESTART=", 12) ||
            !strncmp(*ep, "LSB_EXIT_PRE_ABORT=", 19) ||
            !strncmp(*ep, "LSB_EXIT_REQUEUE=", 17) ||
            !strncmp(*ep, "LS_JOBPID=", 10) ||
            !strncmp(*ep, "LSB_INTERACTIVE=", 16) ||
            !strncmp(*ep, "LSB_ACCT_MAP=", 13) ||
            !strncmp(*ep, "LSB_JOB_STARTER=", 16) ||
            !strncmp(*ep, "LSB_EVENT_ATTRIB=", 17) ||
            !strncmp(*ep, "LSF_VERSION=", 12) || !strncmp(*ep, "LSB_SUB_", 8) ||
            !strncmp(*ep, "HOME=", 5) || !strncmp(*ep, "PWD=", 4) ||
            !strncmp(*ep, "USER=", 5)) {
            continue;
        }

        if (!(jobSubReq->options & SUB_INTERACTIVE)) {
            if (!strncmp(*ep, "TERMCAP=", 8) || !strncmp(*ep, "TERM=", 5))
                continue;
        }

        sp = putstr_(*ep);
        oldp = sp;
        if (!sp) {
            FREEUP(sp);
            lsberrno = LSBE_NO_MEM;
            return -1;
        }

        for (p = sp; *p != '\0' && *p != '='; p++)
            ;

        if (*p == '\0') {
            noEqual = true;
            if (logclass & (LC_TRACE | LC_EXEC)) {
                ls_syslog(LOG_DEBUG,
                          "%s: environment variable <%s> doesn't have '='",
                          fname, sp);
            }
        } else {
            *p = '\0';
        }

        if (noEqual == true) {
            len1 = 2;
        } else {
            len1 = strlen(p + 1) + 1;
        }
        if ((length += (len = strlen(sp) + len1 + sizeof(TAILCMD) + strlen(sp) +
                              1)) > size) {
            char *newp = (char *) realloc(
                jf->data, (size += (len > MSGSIZE ? len : MSGSIZE)));
            if (newp == NULL) {
                if (noEqual != true)
                    *p = '=';
                lsberrno = LSBE_NO_MEM;
                FREEUP(oldp);
                free(jf->data);
                return -1;
            }
            jf->data = newp;
        }
        strcat(jf->data, sp);
        strcat(jf->data, "='");
        if (noEqual == true) {
            strcat(jf->data, "\0");
        } else {
            strcat(jf->data, p + 1);
        }
        strcat(jf->data, TAILCMD);
        strcat(jf->data, sp);
        strcat(jf->data, "\n");
        if (noEqual != true) {
            *p = '=';
        }
        if (logclass & (LC_TRACE | LC_EXEC)) {
            ls_syslog(LOG_DEBUG, "%s:length=%d, size=%d, jf->len=%d, numEnv=%d",
                      fname, length, size, strlen(jf->data), ++numEnv);
        }
        FREEUP(oldp);
    }

    if ((length +=
         (len = sizeof(TRAPSIGCMD) + sizeof(CMDSTART) +
                strlen(jobSubReq->command) + tsoptlen + sizeof(WAITCLEANCMD) +
                sizeof(EXITCMD) + sizeof(EDATASTART) + ed.len

                + LL_BUFSIZ_32)) > size) {
        char *newp = (char *) realloc(
            jf->data, (size += (len > MSGSIZE ? len : MSGSIZE)));
        if (newp == NULL) {
            lsberrno = LSBE_NO_MEM;
            FREEUP(jf->data);
            return -1;
        }
        jf->data = newp;
    }
    if (logclass & (LC_TRACE | LC_EXEC)) {
        ls_syslog(LOG_DEBUG, "%s:length=%d, size=%d, jf->len=%d, numEnv=%d",
                  fname, length, size, strlen(jf->data), numEnv);
    }

    strcat(jf->data, TRAPSIGCMD);
    strcat(jf->data, CMDSTART);

    strcat(jf->data, jobSubReq->command);

    strcat(jf->data, WAITCLEANCMD);
    strcat(jf->data, EXITCMD);

    appendEData(jf, &ed);

    return 0;
}

void appendEData(struct lenData *jf, struct lenData *ed)
{
    char *sp, num[LL_BUFSIZ_32];

    strcat(jf->data, EDATASTART);
    sprintf(num, "%d\n", ed->len);
    strcat(jf->data, num);

    sp = jf->data + strlen(jf->data) + 1;
    memcpy(sp, ed->data, ed->len);
    jf->len = strlen(jf->data) + 1 + ed->len;
}

static int64_t send_batch(struct submitReq *submitReqPtr, struct lenData *jf,
                          struct submitReply *submitReply, struct lsfAuth *auth)
{
    mbdReqType mbdReqtype;
    XDR xdrs;
    char *request_buf;
    int reqBufSize;
    char *reply_buf;
    int cc;
    struct packet_header hdr;
    struct submitMbdReply *reply;
    int64_t jobId;

    reqBufSize = xdrSubReqSize(submitReqPtr);
    reqBufSize += xdr_lsfAuthSize(auth);
    if ((request_buf = malloc(reqBufSize)) == NULL) {
        lsberrno = LSBE_NO_MEM;
        return -1;
    }

    mbdReqtype = BATCH_JOB_SUB;
    xdrmem_create(&xdrs, request_buf, reqBufSize, XDR_ENCODE);
    init_pack_hdr(&hdr);
    hdr.operation = mbdReqtype;
    if (!xdr_encodeMsg(&xdrs, (char *) submitReqPtr, &hdr, xdr_submitReq, 0,
                       auth)) {
        xdr_destroy(&xdrs);
        lsberrno = LSBE_XDR;
        free(request_buf);
        return -1;
    }

    if ((cc = call_mbd(request_buf, XDR_GETPOS(&xdrs), &reply_buf, &hdr, jf)) <
        0) {
        xdr_destroy(&xdrs);
        free(request_buf);
        return -1;
    }
    xdr_destroy(&xdrs);
    free(request_buf);

    lsberrno = hdr.operation;
    if (cc == 0) {
        submitReply->badJobId = 0;
        submitReply->badReqIndx = 0;
        submitReply->queue = "";
        submitReply->badJobName = "";
        return -1;
    }

    reply = calloc(1, sizeof(struct submitMbdReply));
    if (reply == NULL) {
        lsberrno = LSBE_NO_MEM;
        free(reply);
        return -1;
    }

    xdrmem_create(&xdrs, reply_buf, cc, XDR_DECODE);
    if (!xdr_submitMbdReply(&xdrs, reply, &hdr)) {
        lsberrno = LSBE_XDR;
        free(reply_buf);
        free(reply);
        xdr_destroy(&xdrs);
        return -1;
    }

    free(reply_buf);

    xdr_destroy(&xdrs);

    submitReply->badJobId = reply->jobId;
    submitReply->badReqIndx = reply->badReqIndx;
    submitReply->queue = reply->queue;
    submitReply->badJobName = reply->badJobName;

    if (lsberrno == LSBE_NO_ERROR) {
        if (reply->jobId == 0)
            lsberrno = LSBE_PROTOCOL;
        jobId = reply->jobId;
        free(reply);
        return jobId;
    }

    free(reply);
    return -1;
}

static int dependCondSyntax(char *dependCond)
{
    // Bug fix this
    (void) dependCond;
    return 0;
}

int getChkDir(char *givenDir, char *chkPath)
{
    char *strPtr;

    if (((strPtr = strrchr(givenDir, '/')) != NULL) &&
        (islongint_(strPtr + 1))) {
        sprintf(chkPath, "%s", givenDir);
        return 0;
    } else {
        DIR *dirp;
        struct dirent *dp;
        char jobIdDir[MAXFILENAMELEN];
        int i;

        i = 0;
        if ((dirp = opendir(givenDir)) == NULL)
            return -1;
        while ((dp = readdir(dirp)) != NULL) {
            if (strcpy(jobIdDir, dp->d_name) == NULL)
                i++;
            if (i > 1) {
                (void) closedir(dirp);
                return -1;
            }
        }
        if (islongint_(jobIdDir)) {
            sprintf(chkPath, "%s/%s", givenDir, jobIdDir);
            (void) closedir(dirp);
            return 0;
        } else {
            (void) closedir(dirp);
            return -1;
        }
    }
}

static int64_t subJob(struct submit *jobSubReq, struct submitReq *submitReq,
                      struct submitReply *submitRep, struct lsfAuth *auth)
{
    char homeDir[MAXFILENAMELEN];
    char resReq[MAXLINELEN];
    char cmd[MAXLINELEN];
    struct lenData jf;
    LSB_SUB_SPOOL_FILE_T subSpoolFiles;
    int64_t jobId = -1;

    subSpoolFiles.inFileSpool[0] = 0;
    subSpoolFiles.commandSpool[0] = 0;

    submitReq->subHomeDir = homeDir;
    submitReq->resReq = resReq;
    submitReq->command = cmd;

    if (getOtherParams(jobSubReq, submitReq, submitRep, auth, &subSpoolFiles) <
        0) {
        goto cleanup;
    }

    if (createJobInfoFile(jobSubReq, &jf) == -1) {
        goto cleanup;
    }

    if (submitReq->options & SUB_INTERACTIVE) {
        if (submitReq->options & SUB_PTY) {
            if (!isatty(0) && !isatty(1))
                submitReq->options &= ~SUB_PTY;
        }
    }

    if (submitReq->options2 & SUB2_BSUB_BLOCK) {
        // Bug need to support this
    }

    jobId = send_batch(submitReq, &jf, submitRep, auth);
    free(jf.data);

    if (jobId > 0) {
        if (submitReq->options & SUB_INTERACTIVE) {
            sigset_t sigMask;
            sigemptyset(&sigMask);
            sigaddset(&sigMask, SIGINT);
            sigprocmask(SIG_BLOCK, &sigMask, NULL);
        }

        if (!getenv("BSUB_QUIET"))
            postSubMsg(jobSubReq, jobId, submitRep);

        if (submitReq->options & SUB_INTERACTIVE) {
            // Bug need to support this
        }
    }

cleanup:

    return jobId;
}

int getOtherParams(struct submit *user_submit_request,
                   struct submitReq *submitReq, struct submitReply *submitRep,
                   struct lsfAuth *auth, LSB_SUB_SPOOL_FILE_T *subSpoolFiles)
{
    char *jobdesp, *sp, jobdespBuf[MAX_CMD_DESC_LEN];
    char lineStrBuf[MAXLINELEN], lastNonSpaceChar;
    int i, lastNonSpaceIdx;
    char *myHostName;
    struct passwd *pw;
    int jobSubReqCmd1Offset;

    // Bug unused data memeber
    (void) submitRep;
    (void) subSpoolFiles;

    if (user_submit_request->options & SUB_JOB_NAME) {
        if (!user_submit_request->jobName) {
            lsberrno = LSBE_BAD_JOB;
            return -1;
        }

        if (strlen(user_submit_request->jobName) >= MAX_CMD_DESC_LEN - 1) {
            lsberrno = LSBE_BAD_JOB;
            return -1;
        } else
            submitReq->jobName = user_submit_request->jobName;
    } else
        submitReq->jobName = "";

    // Bug remove all spool reference and code
    submitReq->inFileSpool = "";
    submitReq->inFile = "";
    if (user_submit_request->options & SUB_IN_FILE) {
        if (!user_submit_request->inFile) {
            lsberrno = LSBE_BAD_ARG;
            return -1;
        }

        if (strlen(user_submit_request->inFile) >= MAXFILENAMELEN - 1) {
            lsberrno = LSBE_SYS_CALL;
            errno = ENAMETOOLONG;
            return -1;
        }
        submitReq->inFile = user_submit_request->inFile;
        submitReq->inFileSpool = "";
    }

    if (user_submit_request->options & SUB_MAIL_USER) {
        if (!user_submit_request->mailUser) {
            lsberrno = LSBE_BAD_ARG;
            return -1;
        }
        if (strlen(user_submit_request->mailUser) >= MAXHOSTNAMELEN - 1) {
            lsberrno = LSBE_SYS_CALL;
            errno = ENAMETOOLONG;
            return -1;
        }
        submitReq->mailUser = user_submit_request->mailUser;
    } else
        submitReq->mailUser = "";

    if (user_submit_request->options & SUB_PROJECT_NAME) {
        if (!user_submit_request->projectName) {
            lsberrno = LSBE_BAD_ARG;
            return -1;
        }
        if (strlen(user_submit_request->projectName) >= LL_BUFSIZ_32 - 1) {
            lsberrno = LSBE_BAD_ARG;
            errno = ENAMETOOLONG;
            return -1;
        }
        submitReq->projectName = user_submit_request->projectName;
    } else
        submitReq->projectName = "";

    if (user_submit_request->options & SUB_OUT_FILE) {
        if (!user_submit_request->outFile) {
            lsberrno = LSBE_BAD_ARG;
            return -1;
        }

        if (strlen(user_submit_request->outFile) >= MAXFILENAMELEN - 1) {
            lsberrno = LSBE_SYS_CALL;
            errno = ENAMETOOLONG;
            return -1;
        }
        submitReq->outFile = user_submit_request->outFile;
    } else
        submitReq->outFile = "";

    if (user_submit_request->options & SUB_ERR_FILE) {
        if (!user_submit_request->errFile) {
            lsberrno = LSBE_BAD_ARG;
            return -1;
        }

        if (strlen(user_submit_request->errFile) >= MAXFILENAMELEN - 1) {
            lsberrno = LSBE_SYS_CALL;
            errno = ENAMETOOLONG;
            return -1;
        }
        submitReq->errFile = user_submit_request->errFile;
    } else
        submitReq->errFile = "";

    if (user_submit_request->options & SUB_CHKPNT_PERIOD) {
        if (!(user_submit_request->options & SUB_CHKPNTABLE))
            return -1;

        if (user_submit_request->chkpntPeriod < 0)
            return -1;
        else
            submitReq->chkpntPeriod = user_submit_request->chkpntPeriod;
    } else
        submitReq->chkpntPeriod = 0;

    if (user_submit_request->options & SUB_CHKPNT_DIR) {
        if (!user_submit_request->chkpntDir) {
            lsberrno = LSBE_BAD_ARG;
            return -1;
        }

        if (strlen(user_submit_request->chkpntDir) >= MAXFILENAMELEN - 1) {
            lsberrno = LSBE_SYS_CALL;
            errno = ENAMETOOLONG;
            return -1;
        }

        submitReq->chkpntDir = user_submit_request->chkpntDir;
        submitReq->options |= SUB_CHKPNTABLE;
    } else
        submitReq->chkpntDir = "";

    if (user_submit_request->numProcessors < 0 ||
        user_submit_request->maxNumProcessors <
            user_submit_request->numProcessors) {
        lsberrno = LSBE_BAD_ARG;
        return -1;
    }

    if (user_submit_request->numProcessors == 0 &&
        user_submit_request->maxNumProcessors == 0) {
        user_submit_request->options2 |= SUB2_USE_DEF_PROCLIMIT;
        submitReq->options2 |= SUB2_USE_DEF_PROCLIMIT;
    }

    if (user_submit_request->numProcessors != DEFAULT_NUMPRO &&
        user_submit_request->maxNumProcessors != DEFAULT_NUMPRO) {
        submitReq->numProcessors = (user_submit_request->numProcessors)
                                       ? user_submit_request->numProcessors
                                       : 1;
        submitReq->maxNumProcessors =
            (user_submit_request->maxNumProcessors)
                ? user_submit_request->maxNumProcessors
                : 1;
    } else {
        submitReq->numProcessors = DEFAULT_NUMPRO;
        submitReq->maxNumProcessors = DEFAULT_NUMPRO;
    }

    if (user_submit_request->options & SUB_LOGIN_SHELL) {
        if (!user_submit_request->loginShell) {
            lsberrno = LSBE_BAD_ARG;
            return -1;
        }
        if (strlen(user_submit_request->loginShell) >= LL_BUFSIZ_32 - 1) {
            lsberrno = LSBE_BAD_ARG;
            errno = ENAMETOOLONG;
            return -1;
        }
        submitReq->loginShell = user_submit_request->loginShell;
    } else
        submitReq->loginShell = "";

    submitReq->schedHostType = "";

    submitReq->restartPid = 0;
    submitReq->jobFile = "";

    if (user_submit_request->command == (char *) NULL) {
        lsberrno = LSBE_BAD_CMD;
        return -1;
    }

    lineStrBuf[0] = '\0';
    jobdesp = lineStrBuf;
    strncat(lineStrBuf, user_submit_request->command, MAX_CMD_DESC_LEN - 1);

    if ((sp = strstr(jobdesp, "SCRIPT_\n")) != NULL) {
        jobSubReqCmd1Offset = sp + strlen("SCRIPT_\n") - jobdesp;
        jobdesp += jobSubReqCmd1Offset;
        if ((sp = strstr(jobdesp, "SCRIPT_\n")) != NULL) {
            while (*sp != '\n')
                --sp;
            *sp = '\0';
        }
    } else {
        jobSubReqCmd1Offset = 0;
    }
    jobdespBuf[0] = '\0';
    strncat(jobdespBuf, jobdesp, MAX_CMD_DESC_LEN - 1);
    jobdesp = jobdespBuf;

    for (lastNonSpaceIdx = 0, lastNonSpaceChar = ';', i = 0; jobdesp[i] != '\0';
         i++) {
        if (jobdesp[i] == '\n') {
            if (lastNonSpaceChar != ';' && lastNonSpaceChar != '&')
                jobdesp[i] = ';';
            else
                jobdesp[i] = ' ';
        }
        if (jobdesp[i] != ' ' && jobdesp[i] != '\t') {
            lastNonSpaceChar = jobdesp[i];
            lastNonSpaceIdx = i;
        }
    }
    if (jobdesp[lastNonSpaceIdx] == ';')
        jobdesp[lastNonSpaceIdx] = '\0';

    for (i = 0; user_submit_request->command[i] != '\0'; i++) {
        if (user_submit_request->command[i] == ';' ||
            user_submit_request->command[i] == ' ' ||
            user_submit_request->command[i] == '&' ||
            user_submit_request->command[i] == '>' ||
            user_submit_request->command[i] == '<' ||
            user_submit_request->command[i] == '|' ||
            user_submit_request->command[i] == '\t' ||
            user_submit_request->command[i] == '\n')
            break;
    }
    if ((i == 0) && (user_submit_request->command[0] != ' ')) {
        lsberrno = LSBE_BAD_CMD;
        return -1;
    }

    strcpy(submitReq->command, jobdesp);

    if (user_submit_request->options2 & SUB2_MODIFY_CMD) {
        for (i = 0; user_submit_request->newCommand[i] != '\0'; i++) {
            if (user_submit_request->newCommand[i] == ';' ||
                user_submit_request->newCommand[i] == ' ' ||
                user_submit_request->newCommand[i] == '&' ||
                user_submit_request->newCommand[i] == '>' ||
                user_submit_request->newCommand[i] == '<' ||
                user_submit_request->newCommand[i] == '|' ||
                user_submit_request->newCommand[i] == '\t' ||
                user_submit_request->newCommand[i] == '\n')
                break;
        }
        if ((i == 0) && (user_submit_request->command[0] != ' ')) {
            lsberrno = LSBE_BAD_CMD;
            return -1;
        }
    }

    if (user_submit_request->options2 & SUB2_JOB_CMD_SPOOL) {
        // Bug remove reference
    } else if (user_submit_request->options2 & SUB2_MODIFY_CMD) {
        strcpy(submitReq->command, user_submit_request->newCommand);
        submitReq->commandSpool = "";
    } else {
        submitReq->commandSpool = "";
    }

    {
        lineStrBuf[0] = '\0';
        strncat(lineStrBuf, user_submit_request->command, MIN(i, MAXLINELEN));
    }

    if ((myHostName = ls_getmyhostname()) == NULL) {
        lsberrno = LSBE_LSLIB;
        return -1;
    }

    submitReq->resReq = "";
    if (user_submit_request->resReq != NULL &&
        user_submit_request->options & SUB_RES_REQ) {
        if (strlen(user_submit_request->resReq) > MAXLINELEN - 1) {
            lsberrno = LSBE_BAD_RESREQ;
            return -1;
        }
        strcpy(submitReq->resReq, user_submit_request->resReq);
    }

    submitReq->fromHost = myHostName;

    umask(submitReq->umask = umask(0077));

    if ((user_submit_request->options & SUB_OTHER_FILES) &&
        user_submit_request->nxf > 0) {
        submitReq->nxf = user_submit_request->nxf;
        submitReq->xf = user_submit_request->xf;
    } else
        submitReq->nxf = 0;

    if ((pw = getpwnam2(auth->lsfUserName)) == NULL) {
        lsberrno = LSBE_SYS_CALL;
        return -1;
    }

    if (strlen(pw->pw_dir) >= MAXFILENAMELEN - 1) {
        lsberrno = LSBE_SYS_CALL;
        errno = ENAMETOOLONG;
        return -1;
    }

    for (i = 0; pw->pw_dir[i] != '\0' && submitReq->cwd[i] == pw->pw_dir[i];
         i++)
        ;

    if ((i != 0) && (pw->pw_dir[i] == '\0')) {
        if (submitReq->cwd[i] == '\0')
            submitReq->cwd[0] = '\0';
        else if (submitReq->cwd[i] == '/')
            strcpy(submitReq->cwd, submitReq->cwd + i + 1);
    }

    strcpy(submitReq->subHomeDir, pw->pw_dir);

    submitReq->sigValue = (user_submit_request->options & SUB_WINDOW_SIG)
                              ? sig_encode(user_submit_request->sigValue)
                              : 0;
    if (submitReq->sigValue > 31 || submitReq->sigValue < 0) {
        lsberrno = LSBE_BAD_SIGNAL;
        return -1;
    }

    return 0;
}

static char *acctMapGet(int *fail, char *lsfUserName)
{
    char hostfn[MAXFILENAMELEN];
    FILE *fp;
    char *line, *map, clusorhost[LL_BUFSIZ_32];
    char user[LL_BUFSIZ_32], dir[40];
    int maplen, len, num;
    struct passwd *pw;
    struct stat statbuf;

    if ((pw = getpwnam2(lsfUserName)) == NULL)
        return NULL;

    strcpy(hostfn, pw->pw_dir);
    strcat(hostfn, "/.lsfhosts");

    if ((fp = fopen(hostfn, "r")) == NULL)
        return NULL;

    if ((fstat(fileno(fp), &statbuf) < 0) ||
        (statbuf.st_uid != 0 && statbuf.st_uid != pw->pw_uid) ||
        (statbuf.st_mode & 066)) {
        fclose(fp);
        return NULL;
    }

    maplen = 256;
    map = malloc(maplen);
    if (!map) {
        lsberrno = LSBE_NO_MEM;
        *fail = 1;
        return NULL;
    }
    map[0] = '\0';
    len = 0;
    while ((line = getNextLine_(fp, true)) != NULL) {
        num = sscanf(line, "%s %s %s", clusorhost, user, dir);
        if (num < 2 || (num == 3 &&
                        (strcmp(dir, "recv") == 0 && strcmp(dir, "send") != 0)))
            continue;

        if (strcmp(user, "+") == 0) {
            continue;
        }

        len += strlen(clusorhost) + 1 + strlen(user) + 1;
        if (len > maplen) {
            char *newmap;

            maplen += 256;
            if ((newmap = realloc(map, maplen)) == NULL) {
                lsberrno = LSBE_NO_MEM;
                free(map);
                *fail = 1;
                return NULL;
            }
            map = newmap;
        }
        strcat(map, clusorhost);
        strcat(map, " ");
        strcat(map, user);
        strcat(map, " ");
    }
    return map;
}

static int getUserInfo(struct submitReq *submitReq, struct submit *jobSubReq)
{
    int childIoFd[2];
    char lsfUserName[MAXLINELEN];
    uid_t uid;
    pid_t pid;
    int cwdLen, len, fail;
    char *usermap;

    struct {
        int error;
        int eno;
        int lserrno;
        int lsberrno;
    } err;

    uid = getuid();

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, childIoFd) < 0) {
        lsberrno = LSBE_SYS_CALL;
        return -1;
    }

    if ((pid = fork()) < 0) {
        lsberrno = LSBE_SYS_CALL;
        return -1;
    } else if (pid > 0) {
        int status;
        close(childIoFd[1]);

        err.eno = -1;
        if (read(childIoFd[0], (char *) &err, sizeof(err)) != sizeof(err)) {
            lsberrno = LSBE_SYS_CALL;
            err.error = true;
            goto waitforchild;
        }

        if (err.error) {
            errno = err.eno;
            lserrno = err.lserrno;
            lsberrno = err.lsberrno;
            goto waitforchild;
        }
        err.eno = -1;

        if (read(childIoFd[0], (char *) &cwdLen, sizeof(cwdLen)) !=
            sizeof(cwdLen)) {
            err.error = true;
            lsberrno = LSBE_SYS_CALL;
            goto waitforchild;
        }
        if (read(childIoFd[0], (char *) submitReq->cwd, cwdLen) != cwdLen) {
            err.error = true;
            lsberrno = LSBE_SYS_CALL;
            goto waitforchild;
        }

        err.eno = -1;
        if (read(childIoFd[0], (char *) &err, sizeof(err)) != sizeof(err)) {
            lsberrno = LSBE_SYS_CALL;
            err.error = true;
            goto waitforchild;
        }

        if (err.error) {
            errno = err.eno;
            lserrno = err.lserrno;
            lsberrno = err.lsberrno;
            goto waitforchild;
        }
        err.eno = -1;

        if (read(childIoFd[0], (char *) &ed.len, sizeof(ed.len)) !=
            sizeof(ed.len)) {
            err.error = true;
            lsberrno = LSBE_SYS_CALL;
            goto waitforchild;
        }

        FREEUP(ed.data);
        if (ed.len > 0) {
            if ((ed.data = (char *) malloc(ed.len)) == NULL) {
                err.error = true;
                lsberrno = LSBE_NO_MEM;
                goto waitforchild;
            }
            if (b_read_fix(childIoFd[0], ed.data, ed.len) != ed.len) {
                FREEUP(ed.data);
                err.error = true;
                lsberrno = LSBE_SYS_CALL;
                goto waitforchild;
            }
        } else
            ed.data = NULL;

        err.eno = -1;
        if (read(childIoFd[0], (char *) &err, sizeof(err)) != sizeof(err)) {
            lsberrno = LSBE_SYS_CALL;
            err.error = true;
            goto waitforchild;
        }

        if (err.error) {
            errno = err.eno;
            lserrno = err.lserrno;
            lsberrno = err.lsberrno;
            goto waitforchild;
        }
        err.eno = -1;

        if (read(childIoFd[0], (char *) &len, sizeof(len)) != sizeof(len)) {
            err.error = true;
            lsberrno = LSBE_SYS_CALL;
            goto waitforchild;
        }
        if (len) {
            FREEUP(useracctmap);
            if ((useracctmap = malloc(len)) == NULL) {
                lsberrno = LSBE_NO_MEM;
                err.error = true;
                goto waitforchild;
            }
            if (read(childIoFd[0], useracctmap, len) != len) {
                err.error = true;
                lsberrno = LSBE_SYS_CALL;
                goto waitforchild;
            }
        }

    waitforchild:
        close(childIoFd[0]);

        if (err.eno < 0)
            err.eno = errno;
        if (waitpid(pid, &status, 0) < 0) {
            lsberrno = LSBE_SYS_CALL;
            return -1;
        }

        if (err.error) {
            errno = err.eno;
            return -1;
        }

        return 0;
    }

    close(childIoFd[0]);

    if (setuid(uid) < 0) {
        lsberrno = LSBE_BAD_USER;
        goto errorParent;
    }

    if (mygetwd_(submitReq->cwd) == NULL) {
        lsberrno = LSBE_SYS_CALL;
        goto errorParent;
    }
    cwdLen = strlen(submitReq->cwd) + 1;

    err.error = false;
    if (write(childIoFd[1], (char *) &err, sizeof(err)) != sizeof(err)) {
        close(childIoFd[1]);
        exit(-1);
    }

    if (write(childIoFd[1], (char *) &cwdLen, sizeof(cwdLen)) !=
        sizeof(cwdLen)) {
        close(childIoFd[1]);
        exit(-1);
    }
    if (write(childIoFd[1], (char *) submitReq->cwd, cwdLen) != cwdLen) {
        close(childIoFd[1]);
        exit(-1);
    }

    if (runBatchEsub(&ed, jobSubReq) < 0) {
        goto errorParent;
    } else {
        err.error = false;
        if (write(childIoFd[1], (char *) &err, sizeof(err)) != sizeof(err)) {
            close(childIoFd[1]);
            exit(-1);
        }
    }

    if (write(childIoFd[1], (char *) &ed.len, sizeof(ed.len)) !=
        sizeof(ed.len)) {
        close(childIoFd[1]);
        exit(-1);
    }

    if (ed.len > 0) {
        if (write(childIoFd[1], (char *) ed.data, ed.len) != ed.len) {
            close(childIoFd[1]);
            exit(-1);
        }
    }

    fail = 0;

    if ((usermap = acctMapGet(&fail, lsfUserName)) == NULL)
        len = 0;
    else
        len = strlen(usermap) + 1;

    if (fail)
        goto errorParent;
    else {
        err.error = false;
        if (write(childIoFd[1], (char *) &err, sizeof(err)) != sizeof(err)) {
            close(childIoFd[1]);
            exit(-1);
        }
    }

    if (write(childIoFd[1], (char *) &len, sizeof(len)) != sizeof(len)) {
        close(childIoFd[1]);
        exit(-1);
    }
    if (write(childIoFd[1], (char *) usermap, len) != len) {
        close(childIoFd[1]);
        exit(-1);
    }
    exit(0);

errorParent:

    err.error = true;
    err.eno = errno;
    err.lserrno = lserrno;
    err.lsberrno = lsberrno;

    if (write(childIoFd[1], (char *) &err, sizeof(err)) != sizeof(err))
        exit(-1);

    exit(0);
}

static int xdrSubReqSize(struct submitReq *req)
{
    int i, sz;

    sz = 1024 + ALIGNWORD_(sizeof(struct submitReq));

    sz += ALIGNWORD_(strlen(req->queue) + 1) + 4 +
          ALIGNWORD_(strlen(req->resReq)) + 4 +
          ALIGNWORD_(strlen(req->fromHost) + 1) + 4 +
          ALIGNWORD_(strlen(req->dependCond) + 1) + 4 +
          ALIGNWORD_(strlen(req->jobName) + 1) + 4 +
          ALIGNWORD_(strlen(req->command) + 1) + 4 +
          ALIGNWORD_(strlen(req->jobFile) + 1) + 4 +
          ALIGNWORD_(strlen(req->inFile) + 1) + 4 +
          ALIGNWORD_(strlen(req->outFile) + 1) + 4 +
          ALIGNWORD_(strlen(req->errFile) + 1) + 4 +
          ALIGNWORD_(strlen(req->inFileSpool) + 1) + 4 +
          ALIGNWORD_(strlen(req->commandSpool) + 1) + 4 +
          ALIGNWORD_(strlen(req->preExecCmd) + 1) + 4 +
          ALIGNWORD_(strlen(req->hostSpec) + 1) + 4 +
          ALIGNWORD_(strlen(req->chkpntDir) + 1) + 4 +
          ALIGNWORD_(strlen(req->subHomeDir) + 1) + 4 +
          ALIGNWORD_(strlen(req->cwd) + 1) + 4 +
          ALIGNWORD_(strlen(req->mailUser) + 1) + 4 +
          ALIGNWORD_(strlen(req->projectName) + 1) + 4;

    for (i = 0; i < req->numAskedHosts; i++)
        sz += ALIGNWORD_(strlen(req->askedHosts[i]) + 1 + 4);

    for (i = 0; i < req->nxf; i++)
        sz += ALIGNWORD_(sizeof(struct xFile) + 4 * 4);

    return sz;
}

static void postSubMsg(struct submit *req, int64_t jobId,
                       struct submitReply *reply)
{
    if ((req->options & SUB_QUEUE) || (req->options & SUB_RESTART)) {
        if (getenv("BSUB_STDERR"))
            fprintf(stderr, ("Job <%s> is submitted to queue <%s>.\n"),
                    lsb_jobid2str(jobId), reply->queue);
        else
            fprintf(stdout, ("Job <%s> is submitted to queue <%s>.\n"),
                    lsb_jobid2str(jobId), reply->queue);
    } else {
        if (getenv("BSUB_STDERR"))
            fprintf(stderr, ("Job <%s> is submitted to default queue <%s>.\n"),
                    lsb_jobid2str(jobId), reply->queue);
        else
            fprintf(stdout, ("Job <%s> is submitted to default queue <%s>.\n"),
                    lsb_jobid2str(jobId), reply->queue);
    }

    fflush(stdout);

    prtBETime_(req);

    if (req->options2 & SUB2_BSUB_BLOCK)
        fprintf(stderr, ("<<Waiting for dispatch ...>>\n"));

    if (req->options & SUB_INTERACTIVE)
        fprintf(stderr, ("<<Waiting for dispatch ...>>\n"));
}

void prtBETime_(struct submit *req)
{
    const char *sp;

    if (req->beginTime > 0) {
        sp = ctime2(&req->beginTime);
        fprintf(stderr, ("Job will be scheduled after %s\n"), sp);
    }
    if (req->termTime > 0) {
        sp = ctime2(&req->termTime);
        fprintf(stderr, ("Job will be terminated by %s\n"), sp);
    }
}

int gettimefor(char *toptarg, time_t *tTime)
{
    struct tm *tmPtr;
    char *cp;
    int tindex, ttime[5];
    int currhour, currmin, currday;

    TIMEIT(1, *tTime = time(0), "time");
    TIMEIT(1, tmPtr = localtime(tTime), "localtime");
    tmPtr->tm_sec = 0;
    currhour = tmPtr->tm_hour;
    currmin = tmPtr->tm_min;
    currday = tmPtr->tm_mday;

    for (tindex = 0; toptarg; tindex++) {
        ttime[tindex] = 0;
        cp = strrchr(toptarg, ':');
        if (cp != NULL) {
            if (!isint_(cp + 1))
                return -1;
            ttime[tindex] = atoi(cp + 1);
            *cp = '\000';
        } else {
            if (!isint_(toptarg))
                return -1;
            ttime[tindex] = atoi(toptarg);
            tindex++;
            break;
        }
    }
    if (tindex < 2 || tindex > 4) {
        return -1;
    }
    if (ttime[0] < 0 || ttime[0] > 59) {
        return -1;
    }
    tmPtr->tm_min = ttime[0];

    if (ttime[1] < 0 || ttime[1] > 23) {
        return -1;
    }
    tmPtr->tm_hour = ttime[1];

    tindex -= 2;
    if (tindex > 0) {
        if (ttime[2] < 1 || ttime[2] > 31) {
            return -1;
        }
        if (((ttime[2] < tmPtr->tm_mday) ||
             ((ttime[2] == tmPtr->tm_mday) && (ttime[1] < currhour)) ||
             ((ttime[2] == tmPtr->tm_mday) && (ttime[1] == currhour) &&
              (ttime[0] < currmin))) &&
            tindex == 1)
            tmPtr->tm_mon++;
        tmPtr->tm_mday = ttime[2];

        tindex--;
        switch (tindex) {
        case 1:
            if (ttime[3] < 0 || ttime[3] > 12) {
                return -1;
            }
            if ((((ttime[3] - 1) < tmPtr->tm_mon) ||
                 (((ttime[3] - 1) == tmPtr->tm_mon) && (ttime[2] < currday)) ||
                 (((ttime[3] - 1) == tmPtr->tm_mon) && (ttime[2] == currday) &&
                  (ttime[1] < currhour)) ||
                 (((ttime[3] - 1) == tmPtr->tm_mon) && (ttime[2] == currday) &&
                  (ttime[1] == currhour) && (ttime[0] < currmin))))
                tmPtr->tm_year++;
            tmPtr->tm_mon = ttime[3] - 1;
            break;
        default:
            break;
        }
    }

    tmPtr->tm_isdst = -1;

    *tTime = *tTime < mktime(tmPtr) ? mktime(tmPtr) : mktime(tmPtr) + 86400;

    return 0;
}

int setOption_(int argc, char **argv, char *template, struct submit *req,
               int mask, int mask2, char **errMsg)
{
    int eflag = 0, oflag = 0;
    int badIdx, v1, v2;
    char *sp, *cp;
    char savearg[MAXLINELEN];
    char *optName;
    char *(*getoptfunc)();
    int flagI = 0;
    int flagK = 0;
    struct args {
        int argc;
        char **argv;
    } myArgs;
    char *pExclStr;

    myArgs.argc = req->options;
    myArgs.argv = errMsg;

#define checkSubDelOption(option, opt)                                         \
    {                                                                          \
        if (req->options & SUB_MODIFY && strcmp(optName, opt) == 0) {          \
            if (req->options & option) {                                       \
                fprintf(                                                       \
                    stderr,                                                    \
                    ("You cannot modify and set default at the same time"));   \
                return -1;                                                     \
            }                                                                  \
            req->delOptions |= option;                                         \
            break;                                                             \
        }                                                                      \
    }

#define checkSubDelOption2(option2, opt)                                       \
    {                                                                          \
        if (req->options & SUB_MODIFY && strcmp(optName, opt) == 0) {          \
            if (req->options2 & option2) {                                     \
                fprintf(                                                       \
                    stderr,                                                    \
                    ("You cannot modify and set default at the same time"));   \
                return -1;                                                     \
            }                                                                  \
            req->delOptions2 |= option2;                                       \
            break;                                                             \
        }                                                                      \
    }

#define checkRLDelOption(rLimit, opt)                                          \
    if (req->options & SUB_MODIFY && strcmp(optName, opt) == 0) {              \
        if (req->rLimits[rLimit] != DEFAULT_RLIMIT &&                          \
            req->rLimits[rLimit] != DELETE_NUMBER) {                           \
            fprintf(stderr,                                                    \
                    ("You cannot modify and set default at the same  time"));  \
            return -1;                                                         \
        }                                                                      \
        req->rLimits[rLimit] = DELETE_NUMBER;                                  \
        break;                                                                 \
    }

    getoptfunc = my_getopt;

    while ((optName = getoptfunc(argc, argv, template, errMsg)) != NULL) {
        switch (optName[0]) {
        case 'E':
            req->options2 |= SUB2_MODIFY_PEND_JOB;
            checkSubDelOption(SUB_PRE_EXEC, "En");
            if (mask & SUB_PRE_EXEC) {
                req->options |= SUB_PRE_EXEC;
                req->preExecCmd = optarg;
            }
            break;

        case 'w':
            req->options2 |= SUB2_MODIFY_PEND_JOB;
            checkSubDelOption(SUB_DEPEND_COND, "wn");
            if (mask & SUB_DEPEND_COND) {
                req->options |= SUB_DEPEND_COND;
                req->dependCond = optarg;
            }
            break;

        case 'L':
            req->options2 |= SUB2_MODIFY_PEND_JOB;
            checkSubDelOption(SUB_LOGIN_SHELL, "Ln");
            if (mask & SUB_LOGIN_SHELL) {
                req->options |= SUB_LOGIN_SHELL;
                req->loginShell = optarg;
            }
            break;

        case 'B':
            req->options2 |= SUB2_MODIFY_PEND_JOB;
            checkSubDelOption(SUB_NOTIFY_BEGIN, "Bn");
            req->options |= SUB_NOTIFY_BEGIN;
            break;

        case 'f':
            req->options2 |= SUB2_MODIFY_PEND_JOB;
            checkSubDelOption(SUB_OTHER_FILES, "fn");
            if (req->options & SUB_RESTART) {
                req->options |= SUB_RESTART_FORCE;
                break;
            }

            if (mask & SUB_OTHER_FILES) {
                if (parseXF(req, optarg, errMsg) < 0)
                    return -1;
                req->options |= SUB_OTHER_FILES;
            }
            break;

        case 'k':
            req->options2 |= SUB2_MODIFY_PEND_JOB;
            checkSubDelOption((SUB_CHKPNT_PERIOD | SUB_CHKPNT_DIR), "kn");
            if (!(mask & SUB_CHKPNT_DIR))

                break;

            cp = optarg;
            while (*(cp++) == ' ')
                ;

            if ((sp = strchr(cp, ' ')) != NULL) {
                int bPeriodExist = 0;
                int bMethodExist = 0;
                char *pCurWord = NULL;

                *sp = '\0';

                while (*(++sp) == ' ')
                    ;

                while ((sp != NULL) && (*sp != '\0')) {
                    pCurWord = sp;

                    if (isdigit((int) (*pCurWord))) {
                        if (bPeriodExist) {
                            PRINT_ERRMSG1(errMsg,
                                          "%s: Bad checkpoint period value",
                                          pCurWord);
                            return -1;
                        }

                        sp = strchr(pCurWord, ' ');
                        if (sp != NULL) {
                            *sp = '\0';
                        }

                        if (!isint_(pCurWord) ||
                            (req->chkpntPeriod = atoi(pCurWord) * 60) <= 0) {
                            PRINT_ERRMSG1(errMsg,
                                          "%s: Bad checkpoint period value",
                                          pCurWord);
                            return -1;
                        }
                        bPeriodExist = 1;
                        req->options |= SUB_CHKPNT_PERIOD;

                    } else if (strstr(pCurWord, "method=") == pCurWord) {
                        if (bMethodExist) {
                            PRINT_ERRMSG1(
                                errMsg,
                                "%s: Syntax error. Correct syntax is "
                                "method=name of your checkpoint method",
                                pCurWord);
                            return -1;
                        }

                        sp = strchr(pCurWord, ' ');
                        if (sp != NULL) {
                            *sp = '\0';
                        }

                        pCurWord = strchr(pCurWord, '=');
                        if (*(++pCurWord) != '\0') {
                            putEnv("LSB_ECHKPNT_METHOD", pCurWord);
                        } else {
                            PRINT_ERRMSG1(
                                errMsg,
                                "%s: Syntax error. Correct syntax is "
                                "method=name of your checkpoint method",
                                pCurWord);
                            return -1;
                        }
                        bMethodExist = 1;
                    } else {
                        PRINT_ERRMSG1(errMsg,
                                      "%s: Syntax error. Correct syntax is "
                                      "bsub -k \"chkpnt_dir [period] "
                                      "[method=checkpoint method]\"",
                                      pCurWord);
                        return -1;
                    }

                    if (sp != NULL) {
                        while (*(++sp) == ' ')
                            ;
                    }
                }
            }
            req->chkpntDir = optarg;
            req->options |= SUB_CHKPNT_DIR;
            break;

        case 'R':

            checkSubDelOption(SUB_RES_REQ, "Rn");
            if (mask & SUB_RES_REQ) {
                if (req->resReq != NULL) {
                    PRINT_ERRMSG0(errMsg, "Invalid syntax; the -R option was "
                                          "used more than once.\n");
                    return -1;
                }
                req->resReq = optarg;
                req->options |= SUB_RES_REQ;
                while (*(req->resReq) == ' ')
                    req->resReq++;
            }
            break;

        case 'x':
            req->options2 |= SUB2_MODIFY_PEND_JOB;
            checkSubDelOption(SUB_EXCLUSIVE, "xn");
            req->options |= SUB_EXCLUSIVE;
            break;

        case 'I':
            if (flagI || flagK) {
                myArgs.argc = req->options;
                lsb_throw("LSB_BAD_BSUBARGS", &myArgs);

                return -1;
            }

            if (!strcmp(optName, "I")) {
                req->options |= SUB_INTERACTIVE;
                flagI++;
            } else if (!strcmp(optName, "Ip")) {
                req->options |= SUB_INTERACTIVE | SUB_PTY;
                flagI++;
            } else if (!strcmp(optName, "Is")) {
                req->options |= SUB_INTERACTIVE | SUB_PTY | SUB_PTY_SHELL;
                flagI++;
            } else {
                myArgs.argc = req->options;
                lsb_throw("LSB_BAD_BSUBARGS", &myArgs);

                return -1;
            }
            break;

        case 'H':
            checkSubDelOption(SUB2_HOLD, "Hn");
            req->options2 |= SUB2_HOLD;
            break;
        case 'K':
            if (flagI || flagK) {
                myArgs.argc = req->options;
                lsb_throw("LSB_BAD_BSUBARGS", &myArgs);

                return -1;
            }
            flagK++;
            req->options2 |= SUB2_BSUB_BLOCK;
            break;
        case 'r':
            req->options2 |= SUB2_MODIFY_RUN_JOB;
            checkSubDelOption(SUB_RERUNNABLE, "rn");

            req->options |= SUB_RERUNNABLE;
            break;

        case 'N':
            req->options2 |= SUB2_MODIFY_PEND_JOB;
            checkSubDelOption(SUB_NOTIFY_END, "Nn");
            req->options |= SUB_NOTIFY_END;
            break;

        case 'h':
            myArgs.argc = req->options;
            lsb_throw("LSB_BAD_BSUBARGS", &myArgs);

            return -1;

        case 'm':
            req->options2 |= SUB2_MODIFY_PEND_JOB;
            checkSubDelOption(SUB_HOST, "mn");
            if (!(mask & SUB_HOST))
                break;

            req->options |= SUB_HOST;
            if (getAskedHosts_(optarg, &req->askedHosts, &req->numAskedHosts,
                               &badIdx, false) < 0 &&
                lserrno != LSE_BAD_HOST) {
                lsberrno = LSBE_LSLIB;
                return -1;
            }
            if (req->numAskedHosts == 0) {
                myArgs.argc = req->options;
                lsb_throw("LSB_BAD_BSUBARGS", &myArgs);

                return -1;
            }
            break;

        case 'J':
            req->options2 |= SUB2_MODIFY_PEND_JOB;
            checkSubDelOption(SUB_JOB_NAME, "Jn");
            if (mask & SUB_JOB_NAME) {
                req->jobName = optarg;
                req->options |= SUB_JOB_NAME;
                req->options2 |= SUB2_MODIFY_PEND_JOB;
            }
            break;

        case 'i':

            pExclStr = "isn|is|in|i";
            req->options2 |= SUB2_MODIFY_PEND_JOB;
            if (strncmp(optName, "is", 2) == 0) {
                if ((req->options & SUB_IN_FILE) ||
                    (req->delOptions & SUB_IN_FILE)) {
                    PRINT_ERRMSG1(errMsg, "%s options are exclusive", pExclStr);
                    return -1;
                }
                checkSubDelOption2(SUB2_IN_FILE_SPOOL, "isn");
                if (!(mask2 & SUB2_IN_FILE_SPOOL))
                    break;

                if (strlen(optarg) > MAXFILENAMELEN - 1) {
                    PRINT_ERRMSG1(errMsg, "%s: File name too long", optarg);
                    return -1;
                }
                req->inFile = optarg;
                req->options2 |= SUB2_IN_FILE_SPOOL;
            } else {
                if ((req->options2 & SUB2_IN_FILE_SPOOL) ||
                    (req->delOptions2 & SUB2_IN_FILE_SPOOL)) {
                    PRINT_ERRMSG1(errMsg, "%s options are exclusive", pExclStr);
                    return -1;
                }

                checkSubDelOption(SUB_IN_FILE, "in");
                if (!(mask & SUB_IN_FILE))
                    break;

                if (strlen(optarg) > MAXFILENAMELEN - 1) {
                    PRINT_ERRMSG1(errMsg, "%s: File name too long", optarg);
                    return -1;
                }
                req->inFile = optarg;
                req->options |= SUB_IN_FILE;
            }
            break;
        case 'o':
            req->options2 |= SUB2_MODIFY_RUN_JOB;
            checkSubDelOption(SUB_OUT_FILE, "on");

            if (!(mask & SUB_OUT_FILE))
                break;

            if (strlen(optarg) > MAXFILENAMELEN - 1) {
                PRINT_ERRMSG1(errMsg, "%s: File name too long", optarg);
                return -1;
            }
            req->outFile = optarg;
            req->options |= SUB_OUT_FILE;
            oflag = 1;
            if (eflag) {
                if (strcmp(req->outFile, req->errFile) == 0) {
                    req->options &= ~SUB_ERR_FILE;
                    req->errFile = "";
                }
            }
            break;

        case 'u':

            req->options2 |= SUB2_MODIFY_PEND_JOB;
            checkSubDelOption(SUB_MAIL_USER, "un");
            if ((mask & SUB_MAIL_USER)) {
                if (strlen(optarg) > MAXHOSTNAMELEN - 1) {
                    PRINT_ERRMSG1(errMsg, "%s: Mail destination name too long",
                                  optarg);
                    return -1;
                }

                req->mailUser = optarg;
                req->options |= SUB_MAIL_USER;
            }
            break;

        case 'e':
            req->options2 |= SUB2_MODIFY_RUN_JOB;
            checkSubDelOption(SUB_ERR_FILE, "en");

            if (!(mask & SUB_ERR_FILE))
                break;

            if (strlen(optarg) > MAXFILENAMELEN - 1) {
                PRINT_ERRMSG1(errMsg, "%s: File name too long", optarg);
                return -1;
            }
            req->errFile = optarg;
            req->options |= SUB_ERR_FILE;
            eflag = 1;
            if (oflag) {
                if (strcmp(req->outFile, req->errFile) == 0) {
                    req->options &= ~SUB_ERR_FILE;
                    req->errFile = "";
                }
            }
            break;

        case 'n':
            req->options2 |= SUB2_MODIFY_PEND_JOB;
            if (req->options & SUB_MODIFY && strcmp(optName, "nn") == 0) {
                if (req->numProcessors != 0) {
                    PRINT_ERRMSG0(
                        errMsg,
                        "You cannot modify and set default at the same time");
                    return -1;
                }
                req->numProcessors = DEL_NUMPRO;
                req->maxNumProcessors = DEL_NUMPRO;
                break;
            }
            if (req->numProcessors != 0)

                break;

            if (getValPair(&optarg, &v1, &v2) < 0) {
                PRINT_ERRMSG0(errMsg, "Bad argument for option -n");
                return -1;
            }
            if (v1 <= 0 || v2 <= 0) {
                PRINT_ERRMSG0(
                    errMsg,
                    "The number of processors must be a positive integer");
                return -1;
            }
            if (v1 != INFINIT_INT) {
                req->numProcessors = v1;
                req->maxNumProcessors = v1;
            }
            if (v2 != INFINIT_INT) {
                req->maxNumProcessors = v2;
                if (v1 == INFINIT_INT)
                    req->numProcessors = 1;
            }
            if (req->numProcessors > req->maxNumProcessors) {
                PRINT_ERRMSG0(errMsg, "Bad argument for option -n");
                return -1;
            }
            break;

        case 'q':
            req->options2 |= SUB2_MODIFY_PEND_JOB;
            checkSubDelOption(SUB_QUEUE, "qn");
            if (mask & SUB_QUEUE) {
                req->options |= SUB_QUEUE;
                req->queue = optarg;
            }
            break;

        case 'b':
            req->options2 |= SUB2_MODIFY_PEND_JOB;
            if (req->options & SUB_MODIFY && strcmp(optName, "bn") == 0) {
                if (req->beginTime != 0 && req->beginTime != DELETE_NUMBER) {
                    PRINT_ERRMSG0(
                        errMsg,
                        "You cannot modify and set default at the same time");
                    return -1;
                }
                req->beginTime = DELETE_NUMBER;
                break;
            }
            if (req->beginTime != 0)

                break;

            strcpy(savearg, optarg);
            if (gettimefor(optarg, &req->beginTime) < 0) {
                lsberrno = LSBE_BAD_TIME;
                if (errMsg != NULL)
                    sprintf(*errMsg, "%s:%s", savearg, lsb_sysmsg());
                else
                    sub_perror(savearg);
                return -1;
            }
            break;

        case 't':
            req->options2 |= SUB2_MODIFY_PEND_JOB;
            if (req->options & SUB_MODIFY && strcmp(optName, "tn") == 0) {
                if (req->termTime != 0 && req->termTime != DELETE_NUMBER) {
                    PRINT_ERRMSG0(
                        errMsg,
                        "You cannot modify and set default at the same time");
                    return -1;
                }
                req->termTime = DELETE_NUMBER;
                break;
            }

            if (req->termTime != 0)

                break;

            strcpy(savearg, optarg);
            if (gettimefor(optarg, &req->termTime) < 0) {
                lsberrno = LSBE_BAD_TIME;
                if (errMsg != NULL)
                    sprintf(*errMsg, "%s:%s", savearg, lsb_sysmsg());
                else
                    sub_perror(savearg);
                return -1;
            }
            break;

        case 's':
            req->options2 |= SUB2_MODIFY_PEND_JOB;
            if (strncmp(optName, "sp", 2) == 0) {
                checkSubDelOption2(SUB2_JOB_PRIORITY, "spn");
                if (!(mask2 & SUB2_JOB_PRIORITY))
                    break;

                if (!isint_(optarg) ||
                    (req->userPriority = atoi(optarg)) <= 0) {
                    PRINT_ERRMSG1(errMsg, "%s: Illegal job priority", optarg);
                    return -1;
                }
                req->options2 |= SUB2_JOB_PRIORITY;
            } else {
                printf("-s option is set\n");
                checkSubDelOption(SUB_WINDOW_SIG, "sn");

                if (!(mask & SUB_WINDOW_SIG))
                    break;
                if ((req->sigValue = getSigVal(optarg)) < 0) {
                    PRINT_ERRMSG1(errMsg, "%s: Illegal signal value", optarg);
                    return -1;
                }
                req->options |= SUB_WINDOW_SIG;
            }
            break;

        case 'c':
            req->options2 |= SUB2_MODIFY_RUN_JOB;
            checkRLDelOption(LSF_RLIMIT_CPU, "cn");
            if (req->rLimits[LSF_RLIMIT_CPU] != DEFAULT_RLIMIT)

                break;

            strcpy(savearg, optarg);

            if ((sp = strchr(optarg, '/')) != NULL && strlen(sp + 1) > 0) {
                req->options |= SUB_HOST_SPEC;
                if (req->hostSpec && strcmp(req->hostSpec, sp + 1) != 0) {
                    PRINT_ERRMSG2(
                        errMsg,
                        ("More than one host_spec is specified: <%s> and <%s>"),
                        req->hostSpec, sp + 1);
                    return -1;
                }
                req->hostSpec = sp + 1;
            }
            if (sp)
                *sp = '\0';

            if ((cp = strchr(optarg, ':')) != NULL)
                *cp = '\0';
            if ((!isint_(optarg)) || (atoi(optarg) < 0)) {
                PRINT_ERRMSG1(errMsg, "%s: Bad CPULIMIT specification",
                              savearg);
                return -1;
            } else
                req->rLimits[LSF_RLIMIT_CPU] = atoi(optarg);
            if (cp != NULL) {
                optarg = cp + 1;
                if ((!isint_(optarg)) || (atoi(optarg) < 0)) {
                    PRINT_ERRMSG1(errMsg, "%s: Bad CPULIMIT specification",
                                  savearg);
                    return -1;
                } else {
                    req->rLimits[LSF_RLIMIT_CPU] *= 60;
                    req->rLimits[LSF_RLIMIT_CPU] += atoi(optarg);
                }
            }
            if (req->rLimits[LSF_RLIMIT_CPU] < 0) {
                PRINT_ERRMSG1(errMsg,
                              "%s: CPULIMIT value should be a positive integer",
                              savearg);
                return -1;
            }

            if (!checkLimit(req->rLimits[LSF_RLIMIT_CPU], 60)) {
                PRINT_ERRMSG1(errMsg, "%s: CPULIMIT value is too big", optarg);
                return -1;
            }

            req->rLimits[LSF_RLIMIT_CPU] *= 60;
            break;

        case 'P':
            req->options2 |= SUB2_MODIFY_PEND_JOB;
            checkSubDelOption(SUB_PROJECT_NAME, "Pn");
            if ((mask & SUB_PROJECT_NAME)) {
                if (strlen(optarg) > LL_BUFSIZ_32 - 1) {
                    PRINT_ERRMSG1(errMsg, "%s:  project name too long", optarg);
                    return -1;
                }

                req->projectName = optarg;
                req->options |= SUB_PROJECT_NAME;
            }
            break;

        case 'W':
            req->options2 |= SUB2_MODIFY_RUN_JOB;
            checkRLDelOption(LSF_RLIMIT_RUN, "Wn");

            if (req->rLimits[LSF_RLIMIT_RUN] != DEFAULT_RLIMIT)

                break;

            strcpy(savearg, optarg);
            if ((sp = strchr(optarg, '/')) != NULL) {
                req->options |= SUB_HOST_SPEC;
                if (req->hostSpec && strcmp(req->hostSpec, sp + 1) != 0) {
                    PRINT_ERRMSG2(
                        errMsg,
                        ("More than one host_spec is specified: <%s> and <%s>"),
                        req->hostSpec, sp + 1);
                    return -1;
                }
                req->hostSpec = sp + 1;
                *sp = '\0';
            }
            if ((cp = strchr(optarg, ':')) != NULL)
                *cp = '\0';
            if ((!isint_(optarg)) || (atoi(optarg) < 0)) {
                PRINT_ERRMSG1(errMsg, "%s: Bad RUNLIMIT specification",
                              savearg);
                return -1;
            } else
                req->rLimits[LSF_RLIMIT_RUN] = atoi(optarg);
            if (cp != NULL) {
                optarg = cp + 1;
                if ((!isint_(optarg)) || (atoi(optarg) < 0)) {
                    PRINT_ERRMSG1(errMsg, "%s: Bad RUNLIMIT specification",
                                  savearg);
                    return -1;
                } else {
                    req->rLimits[LSF_RLIMIT_RUN] *= 60;
                    req->rLimits[LSF_RLIMIT_RUN] += atoi(optarg);
                }
            }
            if (req->rLimits[LSF_RLIMIT_RUN] < 0) {
                PRINT_ERRMSG1(errMsg,
                              "%s: RUNLIMIT value should be a positive integer",
                              savearg);
                return -1;
            }
            if (!checkLimit(req->rLimits[LSF_RLIMIT_RUN], 60)) {
                PRINT_ERRMSG1(errMsg, "%s: RUNLIMIT value is too big", optarg);
                return -1;
            }

            req->rLimits[LSF_RLIMIT_RUN] *= 60;
            break;

        case 'F':
            req->options2 |= SUB2_MODIFY_PEND_JOB;
            checkRLDelOption(LSF_RLIMIT_FSIZE, "Fn");
            if (req->rLimits[LSF_RLIMIT_FSIZE] != DEFAULT_RLIMIT)

                break;

            if (isint_(optarg) &&
                ((req->rLimits[LSF_RLIMIT_FSIZE] = atoi(optarg)) > 0)) {
                break;
            }
            PRINT_ERRMSG1(errMsg,
                          "%s: FILELIMIT value should be a positive integer",
                          optarg);
            return -1;

        case 'D':
            req->options2 |= SUB2_MODIFY_PEND_JOB;
            checkRLDelOption(LSF_RLIMIT_DATA, "Dn");
            if (req->rLimits[LSF_RLIMIT_DATA] != DEFAULT_RLIMIT)

                break;

            if (isint_(optarg) &&
                ((req->rLimits[LSF_RLIMIT_DATA] = atoi(optarg)) > 0)) {
                break;
            }
            PRINT_ERRMSG1(errMsg,
                          "%s: DATALIMIT value should be a positive integer",
                          optarg);
            return -1;

        case 'S':
            req->options2 |= SUB2_MODIFY_PEND_JOB;
            checkRLDelOption(LSF_RLIMIT_STACK, "Sn");

            if (req->rLimits[LSF_RLIMIT_STACK] != DEFAULT_RLIMIT)

                break;

            if (isint_(optarg) &&
                ((req->rLimits[LSF_RLIMIT_STACK] = atoi(optarg)) > 0)) {
                break;
            }
            PRINT_ERRMSG1(errMsg,
                          "%s: STACKLIMIT value should be a positive integer",
                          optarg);
            return -1;

        case 'C':
            req->options2 |= SUB2_MODIFY_PEND_JOB;
            checkRLDelOption(LSF_RLIMIT_CORE, "Cn");
            if (req->rLimits[LSF_RLIMIT_CORE] != DEFAULT_RLIMIT)

                break;

            if (isint_(optarg) &&
                ((req->rLimits[LSF_RLIMIT_CORE] = atoi(optarg)) >= 0)) {
                break;
            }
            PRINT_ERRMSG1(errMsg,
                          "%s: CORELIMIT value should be a positive integer",
                          optarg);
            return -1;

        case 'M':
            req->options2 |= SUB2_MODIFY_RUN_JOB;
            checkRLDelOption(LSF_RLIMIT_RSS, "Mn");
            if (req->rLimits[LSF_RLIMIT_RSS] != DEFAULT_RLIMIT)

                break;

            if (isint_(optarg) &&
                ((req->rLimits[LSF_RLIMIT_RSS] = atoi(optarg)) > 0)) {
                break;
            }
            PRINT_ERRMSG1(errMsg,
                          "%s: MEMLIMT value should be a positive integer",
                          optarg);
            return -1;

        case 'p':
            checkRLDelOption(LSF_RLIMIT_PROCESS, "pn");
            if (req->rLimits[LSF_RLIMIT_PROCESS] != DEFAULT_RLIMIT)

                break;

            if (isint_(optarg) &&
                ((req->rLimits[LSF_RLIMIT_PROCESS] = atoi(optarg)) > 0)) {
                if (!checkLimit(req->rLimits[LSF_RLIMIT_PROCESS], 1)) {
                    PRINT_ERRMSG1(errMsg, "%s: PROCESSLIMIT value is too big\n",
                                  optarg);
                    return -1;
                }
                break;
            }
            PRINT_ERRMSG1(
                errMsg, "%s: PROCESSLIMIT value should be a positive integer\n",
                optarg);
            return -1;

        case 'v':
            checkRLDelOption(LSF_RLIMIT_SWAP, "vn");
            if (req->rLimits[LSF_RLIMIT_SWAP] != DEFAULT_RLIMIT)

                break;

            if (isint_(optarg) &&
                ((req->rLimits[LSF_RLIMIT_SWAP] = atoi(optarg)) > 0)) {
                break;
            }
            PRINT_ERRMSG1(errMsg,
                          "%s: SWAPLIMIT value should be a positive integer\n",
                          optarg);
            return -1;

        case 'O':
            if (req->options == 0) {
                optionFlag = true;
                strcpy(optionFileName, optarg);
            } else
                req->options |= SUB_MODIFY_ONCE;
            break;

        case 'Z':
            pExclStr = "Zsn|Zs|Z";
            req->options2 |= SUB2_MODIFY_PEND_JOB;
            if (strncmp(optName, "Zs", 2) == 0) {
                if (req->options2 & SUB2_MODIFY_CMD) {
                    PRINT_ERRMSG1(errMsg, "%s options are exclusive", pExclStr);
                    return -1;
                }

                checkSubDelOption2(SUB2_JOB_CMD_SPOOL, "Zsn");
                if (!(mask2 & SUB2_JOB_CMD_SPOOL))
                    break;

                if (req->options & SUB_MODIFY) {
                    if (strlen(optarg) >= MAXLINELEN) {
                        PRINT_ERRMSG1(errMsg, "%s: File name too long", optarg);
                        return -1;
                    }

                    req->newCommand = optarg;
                    req->options2 |= SUB2_MODIFY_CMD;
                }

                req->options2 |= SUB2_JOB_CMD_SPOOL;
            } else {
                if ((req->options2 & SUB2_JOB_CMD_SPOOL) ||
                    (req->delOptions2 & SUB2_JOB_CMD_SPOOL)) {
                    PRINT_ERRMSG1(errMsg, "%s options are exclusive", pExclStr);
                    return -1;
                }

                if (strlen(optarg) >= MAXLINELEN) {
                    PRINT_ERRMSG1(errMsg, "%s: File name too long", optarg);
                    return -1;
                }

                req->newCommand = optarg;
                req->options2 |= SUB2_MODIFY_CMD;
            }
            break;
        case 'a':
            additionEsubInfo = putstr_(optarg);
            break;
        case 'V':
            fprintf(stderr, "%s\n", LAVALITE_VERSION_STR);
            exit(0);

        default:
            myArgs.argc = req->options;
            lsb_throw("LSB_BAD_BSUBARGS", &myArgs);

            return -1;
        }
    }

    if ((req->options & SUB_INTERACTIVE) && (req->options & SUB_JOB_NAME) &&
        strchr(req->jobName, '[')) {
        PRINT_ERRMSG0(errMsg, "Interactive job not supported for job arrays");
        return -1;
    }

    if ((req->options2 & SUB2_BSUB_BLOCK) && (req->options & SUB_JOB_NAME) &&
        strchr(req->jobName, '[')) {
        PRINT_ERRMSG0(errMsg, "Job array doesn't support -K option");
        return -1;
    }
    return 0;
}

static int parseLine_(char *line, int *embedArgc, char ***embedArgv,
                      char **errMsg)
{
#define INCREASE 40
    int i;
    static char **argBuf = NULL, *key;
    static char fname[] = "parseLine_";
    static int argNum = 0;
    static int first = true, bufSize = INCREASE;
    char *sp, *sQuote, *dQuote, quoteMark;
    char **tmp;

    if (first == true) {
        if ((argBuf = (char **) malloc(INCREASE * sizeof(char *))) == NULL) {
            PRINT_ERRMSG2(errMsg, "%s: %s failed: %m", fname, "malloc");
            return -1;
        }
        first = false;
    } else {
        for (i = 0; i < argNum; i++)
            FREEUP(argBuf[i + 1]);
        argNum = 0;
    }
    *embedArgc = 1;
    argBuf[0] = "bsub";
    argNum = 1;
    key = "BSUB";
    SKIPSPACE(line);

    if (*line == '\0' || *line != '#')
        return -1;

    ++line;
    SKIPSPACE(line);
    if (strncmp(line, key, strlen(key)) == 0) {
        line += strlen(key);
        SKIPSPACE(line);
        if (*line != '-') {
            return -1;
        }
        while (true) {
            quoteMark = '"';
            if ((sQuote = strchr(line, '\'')) != NULL)
                if ((dQuote = strchr(line, '"')) == NULL || sQuote < dQuote)

                    quoteMark = '\'';

            if ((sp = getNextValueQ_(&line, quoteMark, quoteMark)) == NULL)
                goto FINISH;

            if (*sp == '#')
                goto FINISH;

            if ((*embedArgc) + 2 > bufSize) {
                if ((tmp = (char **) realloc(argBuf, (bufSize + INCREASE) *
                                                         sizeof(char *))) ==
                    NULL) {
                    PRINT_ERRMSG2(errMsg, "%s: %s failed: %m", fname,
                                  "realloc");
                    argNum = *embedArgc - 1;
                    *embedArgv = argBuf;
                    return -1;
                }
                argBuf = tmp;
                bufSize += INCREASE;
            }
            argBuf[*embedArgc] = putstr_(sp);
            (*embedArgc)++;
            argBuf[*embedArgc] = NULL;
        }
    } else
        return -1;

FINISH:
    argNum = *embedArgc - 1;
    argBuf[*embedArgc] = NULL;
    *embedArgv = argBuf;
    return 0;
}

struct submit *parseOptFile_(char *filename, struct submit *req, char **errMsg)
{
    static char fname[] = "parseOptFile_";
    char *lineBuf;
    int length = 0;
    int lineLen;
    int optArgc;
    char **optArgv;
    char template[] = "E:T:w:f:k:R:m:J:L:u:i:o:e:n:q:b:t:sp:s:c:W:F:D:S:C:M:P:"
                      "p:v:Ip|Is|I|r|H|x|N|B|h|V|G:X:";
    pid_t pid;
    uid_t uid;
    int childIoFd[2];
    int status;

    if (access(filename, F_OK) != 0) {
        fprintf(stderr, "%s: access(%s) failed: %m\n", __func__, filename);
        return NULL;
    }

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, childIoFd) < 0) {
        PRINT_ERRMSG2(errMsg, "%s: %s failed: %m", fname, "socketpair");
        lsberrno = LSBE_SYS_CALL;
        return NULL;
    }
    optArgc = 0;

    pid = fork();
    if (pid < 0) {
        lsberrno = LSBE_SYS_CALL;
        PRINT_ERRMSG2(errMsg, "%s: %s failed: %m", fname, "fork");
        return NULL;
    } else if (pid == 0) {
        char childLine[MAXLINELEN * 4];
        int exitVal = -1;

        close(childIoFd[0]);
        uid = getuid();
        if (setuid(uid) < 0) {
            lsberrno = LSBE_BAD_USER;
            ls_syslog(LOG_ERR, "%s: %s failed: %m", fname, "setuid");
            goto childExit;
        }
        if (logclass & (LC_TRACE | LC_EXEC))
            ls_syslog(LOG_DEBUG1, "%s: Child tries to open a option file <%s>",
                      fname, filename);
        lineLen = 0;
        childLine[0] = '\0';

        if (readOptFile(filename, childLine) < 0)
            exit(exitVal);

        lineLen = strlen(childLine);

        if (write(childIoFd[1], (char *) &lineLen, sizeof(lineLen)) !=
            sizeof(lineLen)) {
            goto childExit;
        }
        if (write(childIoFd[1], (char *) childLine, lineLen) != lineLen) {
            goto childExit;
        }
        exitVal = 0;
    childExit:
        if (logclass & (LC_TRACE | LC_EXEC)) {
            if (exitVal == 0)
                ls_syslog(LOG_DEBUG,
                          "%s: Child succeeded in sending messages to parent",
                          fname);
            else
                ls_syslog(LOG_DEBUG,
                          "%s: Child failed in sending messages to parent",
                          fname);
        }
        close(childIoFd[1]);
        exit(exitVal);
    }

    if (waitpid(pid, &status, 0) < 0) {
        lsberrno = LSBE_SYS_CALL;
        goto parentErr;
    }

    close(childIoFd[1]);
    if (WEXITSTATUS(status)) {
        ls_syslog(LOG_DEBUG, "%s: child failed!", fname);
        goto parentErr;
    }

    if (read(childIoFd[0], (char *) &length, sizeof(length)) != sizeof(length))
        goto parentErr;

    if ((lineBuf = (char *) malloc(length + 1)) == NULL) {
        if (logclass & (LC_TRACE | LC_EXEC))
            ls_syslog(LOG_DEBUG, "%s: parent malloc faild!", fname);
        goto parentErr;
    }
    if (read(childIoFd[0], (char *) lineBuf, length) != length) {
        goto parentErr;
    }

    if (length) {
        char *p;
        p = lineBuf + length;
        *p = '\0';
        if (parseLine_(lineBuf, &optArgc, &optArgv, errMsg) == -1)
            goto parentErr;
    }
    if (optArgc > 1) {
        optind = 1;
        if (setOption_(optArgc, optArgv, template, req, ~req->options,
                       ~req->options2, errMsg) == -1)
            goto parentErr;
    }
    close(childIoFd[0]);
    FREEUP(lineBuf);
    return req;

parentErr:
    if (logclass & (LC_TRACE | LC_EXEC))
        ls_syslog(LOG_DEBUG, "%s: parent malloc faild!", fname);
    FREEUP(lineBuf);
    close(childIoFd[0]);
    return NULL;
}

void subUsage_(int option, char **errMsg)
{
#define ESUB_INFO_USAGE "\t\t[-a additional_esub_information]\n"

    if (errMsg == NULL) {
        if (option & SUB_RESTART) {
            fprintf(stderr, "Usage");
            fprintf(stderr, ": brestart [-h] [-V] [-x] [-f] [-N] [-B] [-q "
                            "\"queue_name...\"] \n");
            fprintf(stderr, "\t\t[-m \"host_name[+[pref_level]] | "
                            "host_group[+[pref_level]]...]\"\n");

            fprintf(stderr, "\t\t[-w \'dependency_expression\'] [-b "
                            "begin_time] [-t term_time]\n");
            fprintf(stderr, "\t\t[-c [hour:]minute[/host_name|/host_model]]\n");
            fprintf(stderr, "\t\t[-F file_limit] [-D data_limit]\n");
            fprintf(stderr, "\t\t[-C core_limit] [-M mem_limit] \n");
            fprintf(stderr, "\t\t[-W run_limit[/host_name|/host_model]] \n");
            fprintf(stderr, "\t\t[-S stack_limit] [-E \"pre_exec_command "
                            "[argument ...]\"]\n");

            fprintf(stderr, "\t\tcheckpoint_dir[job_ID | \"job_ID[index]\"]\n");
            fprintf(stderr, ESUB_INFO_USAGE);

        } else if (option & SUB_MODIFY) {
            fprintf(stderr, "Usage");
            fprintf(stderr, ": bmod [-h] [-V] [-x | -xn]");

            fprintf(stderr, "\n");

            if (lsbMode_ & LSB_MODE_BATCH) {
                fprintf(stderr, "\t\t[-r | -rn] [-N | -Nn] [-B | -Bn]\n");
                fprintf(stderr, "\t\t[-c cpu_limit[/host_spec] | -cn] [-F "
                                "file_limit | -Fn]\n");
                fprintf(stderr, "\t\t[-M mem_limit | -Mn] [-D data_limit | "
                                "-Dn] [-S stack_limit | -Sn]\n");
                fprintf(stderr, "\t\t[-C core_limit | -Cn] [-W "
                                "run_limit[/host_spec] | -Wn ]\n");
                fprintf(stderr, "\t\t[-k chkpnt_dir [chkpnt_period] | -kn] [-P "
                                "project_name | -Pn]\n");
                fprintf(stderr, "\t\t[-L login_shell | -Ln] \n");
            }

            fprintf(stderr, "\t\t[-w depend_cond | -wn] [-R res_req| -Rn] [-J "
                            "job_name | -Jn]\n");
            fprintf(stderr, "\t\t[-q queue_name ... | -qn] \n");
            fprintf(stderr, "\t\t[-m host_name[+[pref_level]] | "
                            "host_group[+[pref_level]]...| -mn]\"\n");
            fprintf(stderr, "\t\t[-n min_processors[,max_processors] | -nn]\n");
            fprintf(stderr, "\t\t[-b begin_time | -bn] [-t term_time | -tn]\n");
            fprintf(stderr, "\t\t[-i in_file | -is in_file | -in | -isn]\n");
            fprintf(stderr, "\t\t[-o out_file | -on] [-e err_file | -en]\n");
            fprintf(stderr, "\t\t[-u mail_user | -un] [[-f \"lfile op "
                            "[rfile]\"] ... | -fn] \n");
            fprintf(stderr,
                    "\t\t[-E \"pre_exec_command [argument ...]\" | -En]\n");
            fprintf(stderr, "\t\t[-sp job_priority | -spn]\n");
            fprintf(stderr,
                    "\t\t[-Z \"new_command\" | -Zs \"new_command\" | -Zsn] \n");
            fprintf(stderr, "\t\t[ jobId | \"jobId[index_list]\" ] \n");
            fprintf(stderr, ESUB_INFO_USAGE);
        } else {
            fprintf(stderr, "Usage");
            fprintf(stderr, ": bsub [-h] [-V] [-x] [-H]");

            if (lsbMode_ & LSB_MODE_BATCH) {
                fprintf(stderr, " [-r] [-N] [-B] [-I | -K | -Ip | -Is]\n");
                fprintf(stderr, "\t\t[-L login_shell] [-c "
                                "cpu_limit[/host_spec]] [-F file_limit]\n");
                fprintf(stderr, "\t\t[-W run_limit[/host_spec]] [-k chkpnt_dir "
                                "[chkpnt_period] [method=chkpnt_dir]]\n");
                fprintf(stderr, "\t\t[-P project_name] ");
            }
            fprintf(stderr, "\n");

            fprintf(stderr, "\t\t[-q queue_name ...]  [-R res_req]\n");
            fprintf(stderr, "\t\t[-m \"host_name[+[pref_level]] | "
                            "host_group[+[pref_level]]...]\"\n");
            fprintf(stderr,
                    "\t\t[-n min_processors[,max_processors]] [-J job_name]\n");
            fprintf(stderr,
                    "\t\t[-b begin_time] [-t term_time] [-u mail_user]\n");
            fprintf(
                stderr,
                "\t\t[-i in_file | -is in_file] [-o out_file] [-e err_file]\n");
            fprintf(stderr,
                    "\t\t[-M mem_limit]  [-D data_limit]  [-S stack_limit]\n");

            fprintf(stderr,
                    "\t\t[[-f \"lfile op [rfile]\"] ...] [-w depend_cond]\n");

            fprintf(stderr,
                    "\t\t[-E \"pre_exec_command [argument ...]\"] [-Zs]\n");
            fprintf(stderr, "\t\t[-sp job_priority]\n");
            fprintf(stderr, "\t\t[command [argument ...]]\n");
            fprintf(stderr, ESUB_INFO_USAGE);
        }

        exit(-1);
    }
}

static int parseXF(struct submit *req, char *arg, char **errMsg)
{
    static int maxNxf = 0;
    static struct xFile *xp = NULL;
    static char fname[] = "parseXF";
    struct xFile *tmp = NULL;
    int options = 0;
    char op[MAXLINELEN], lf[MAXFILENAMELEN], rf[MAXFILENAMELEN];
    char *p;
    char saveArg[MAXLINELEN];

#define NUMXF 10

    if (maxNxf == 0) {
        if ((xp = (struct xFile *) malloc(NUMXF * sizeof(struct xFile))) ==
            NULL) {
            if (errMsg != NULL) {
                sprintf(*errMsg, "%s: %s(%s) failed", fname, "malloc",
                        lsb_sysmsg());
            } else
                sub_perror("Unable to allocate memory for -f option");
            return -1;
        }
        maxNxf = NUMXF;
    }

    req->xf = xp;

    strcpy(saveArg, arg);

    if ((p = strstr(saveArg, "<<"))) {
        strcpy(op, "<<");
        options = XF_OP_EXEC2SUB | XF_OP_EXEC2SUB_APPEND;
    } else if ((p = strstr(saveArg, "><"))) {
        strcpy(op, "><");
        options = XF_OP_EXEC2SUB | XF_OP_SUB2EXEC;
    } else if ((p = strstr(saveArg, "<>"))) {
        strcpy(op, "<>");
        options = XF_OP_EXEC2SUB | XF_OP_SUB2EXEC;
    } else if ((p = strstr(saveArg, ">>"))) {
        strcpy(op, ">>");
        PRINT_ERRMSG2(
            errMsg,
            ("Invalid file operation \"%s\" specification in -f \"%s\""), op,
            saveArg);
        return -1;
    } else if ((p = strstr(saveArg, "<"))) {
        strcpy(op, "<");
        options = XF_OP_EXEC2SUB;
    } else if ((p = strstr(saveArg, ">"))) {
        strcpy(op, ">");
        options = XF_OP_SUB2EXEC;
    } else {
        PRINT_ERRMSG2(
            errMsg,
            ("Invalid file operation \"%s\" specification in -f \"%s\""), op,
            saveArg);
        return -1;
    }

    memset(lf, 0, MAXFILENAMELEN);
    memset(rf, 0, MAXFILENAMELEN);
    memcpy(lf, saveArg, p - saveArg);
    memcpy(rf, p + strlen(op), strlen(saveArg) - strlen(lf) - strlen(op));

    if (strstr(lf, ">") || strstr(rf, ">") || strstr(rf, "<")) {
        PRINT_ERRMSG2(
            errMsg,
            ("Invalid file operation \"%s\" specification in -f \"%s\""), op,
            saveArg);
        return -1;
    }

    if (strlen(lf) != 0) {
        if ((lf[strlen(lf) - 1]) != ' ') {
            PRINT_ERRMSG2(
                errMsg,
                ("Invalid file operation \"%s\" specification in -f \"%s\""),
                op, saveArg);
            return -1;
        }

        trimSpaces(lf);

        if (strlen(lf) == 0) {
            PRINT_ERRMSG1(errMsg,
                          "Invalid local file specification in -f \"%s\"",
                          saveArg);
            return -1;
        }
    } else {
        PRINT_ERRMSG1(errMsg, "Invalid local file specification in -f \"%s\"",
                      saveArg);
        return -1;
    }

    if (strlen(rf) != 0) {
        if ((rf[0]) != ' ') {
            PRINT_ERRMSG2(
                errMsg,
                ("Invalid file operation \"%s\" specification in -f \"%s\""),
                op, saveArg);
            return -1;
        }

        trimSpaces(rf);

        if (strlen(rf) == 0) {
            strcpy(rf, lf);
        }
    } else {
        strcpy(rf, lf);
    }

    if (req->nxf + 1 > maxNxf) {
        tmp = xp;
        if ((xp = (struct xFile *) myrealloc(
                 req->xf, (maxNxf + NUMXF) * sizeof(struct xFile))) == NULL) {
            if (errMsg != NULL) {
                sprintf(*errMsg, "%s: %s(%s) failed", fname, "myrealloc",
                        lsb_sysmsg());
            } else
                sub_perror(("Unable to allocate memory for -f option"));
            xp = tmp;
            return -1;
        }
        maxNxf += NUMXF;
        req->xf = xp;
    }

    strcpy(req->xf[req->nxf].subFn, lf);
    strcpy(req->xf[req->nxf].execFn, rf);
    req->xf[req->nxf].options = options;

    req->nxf++;
    return 0;
}

static int checkLimit(int limit, int factor)
{
    if ((float) limit * (float) factor >= (float) INFINIT_INT)
        return false;
    else
        return true;
}

int runBatchEsub(struct lenData *ed, struct submit *jobSubReq)
{
    static char fname[] = "runBatchEsub";

    char *subRLimitName[LSF_RLIM_NLIMITS] = {
        "LSB_SUB_RLIMIT_CPU",    "LSB_SUB_RLIMIT_FSIZE",
        "LSB_SUB_RLIMIT_DATA",   "LSB_SUB_RLIMIT_STACK",
        "LSB_SUB_RLIMIT_CORE",   "LSB_SUB_RLIMIT_RSS",
        "LSB_SUB_RLIMIT_NOFILE", "LSB_SUB_RLIMIT_OPEN_MAX",
        "LSB_SUB_RLIMIT_SWAP",   "LSB_SUB_RLIMIT_RUN",
        "LSB_SUB_RLIMIT_PROCESS"};
    int cc, i;
    char parmFile[MAXFILENAMELEN], esub[MAXFILENAMELEN];
    FILE *parmfp;
    struct stat sbuf;
#define LSB_SUB_COMMANDNAME 0
    struct config_param myParams[] = {{"LSB_SUB_COMMANDNAME", NULL},
                                      {NULL, NULL}};

#define QUOTE_STR(_str1, _str)                                                 \
    {                                                                          \
        int i, j, cnt = 0;                                                     \
        char ch, next, *tmp_str = NULL;                                        \
        for (i = 0; _str1[i]; i++) {                                           \
            if (_str1[i] == '"')                                               \
                cnt++;                                                         \
        }                                                                      \
        tmp_str =                                                              \
            (char *) malloc((strlen(_str1) + 1 + cnt + 2) * sizeof(char));     \
        if (tmp_str != NULL) {                                                 \
            tmp_str[0] = '"';                                                  \
            _str = tmp_str + 1;                                                \
            strcpy(_str, _str1);                                               \
            for (i = 0; _str[i];) {                                            \
                if (_str[i] == '"') {                                          \
                    ch = _str[i];                                              \
                    _str[i] = '\\';                                            \
                    next = ch;                                                 \
                    for (j = i + 1; _str[j]; j++) {                            \
                        ch = _str[j];                                          \
                        _str[j] = next;                                        \
                        next = ch;                                             \
                    }                                                          \
                    _str[j] = next;                                            \
                    _str[j + 1] = '\0';                                        \
                    i += 2;                                                    \
                }                                                              \
                i++;                                                           \
            }                                                                  \
            for (i = 0; _str[i]; i++) {                                        \
            }                                                                  \
            _str[i] = '"';                                                     \
            _str[i + 1] = '\0';                                                \
        }                                                                      \
        _str = tmp_str;                                                        \
    }

#define SET_PARM_STR(flag, name, sub, field)                                   \
    {                                                                          \
        char *quote_field;                                                     \
        if (((sub)->options & flag) && (sub)->field) {                         \
            QUOTE_STR((sub)->field, quote_field);                              \
            if (quote_field != NULL) {                                         \
                fprintf(parmfp, "%s=%s\n", name, quote_field);                 \
                free(quote_field);                                             \
            }                                                                  \
        } else if ((sub)->delOptions & flag) {                                 \
            fprintf(parmfp, "%s=SUB_RESET\n", name);                           \
        }                                                                      \
    }

#define SET_PARM_BOOL(flag, name, sub)                                         \
    {                                                                          \
        if ((sub)->options & flag) {                                           \
            fprintf(parmfp, "%s=Y\n", name);                                   \
        } else if ((sub)->delOptions & flag) {                                 \
            fprintf(parmfp, "%s=SUB_RESET\n", name);                           \
        }                                                                      \
    }

#define SET_PARM_INT(flag, name, sub, field)                                   \
    {                                                                          \
        if ((sub)->options & flag) {                                           \
            fprintf(parmfp, "%s=%d\n", name, (int) (sub)->field);              \
        } else if ((sub)->delOptions & flag) {                                 \
            fprintf(parmfp, "%s=SUB_RESET\n", name);                           \
        }                                                                      \
    }

#define SET_PARM_STR_2(flag, name, sub, field)                                 \
    {                                                                          \
        char *quote_field;                                                     \
        if (((sub)->options2 & flag) && (sub)->field) {                        \
            QUOTE_STR((sub)->field, quote_field);                              \
            if (quote_field != NULL) {                                         \
                fprintf(parmfp, "%s=%s\n", name, quote_field);                 \
                free(quote_field);                                             \
            }                                                                  \
        } else if ((sub)->delOptions2 & flag) {                                \
            fprintf(parmfp, "%s=SUB_RESET\n", name);                           \
        }                                                                      \
    }

#define SET_PARM_BOOL_2(flag, name, sub)                                       \
    {                                                                          \
        if ((sub)->options2 & flag) {                                          \
            fprintf(parmfp, "%s=Y\n", name);                                   \
        } else if ((sub)->delOptions2 & flag) {                                \
            fprintf(parmfp, "%s=SUB_RESET\n", name);                           \
        }                                                                      \
    }

#define SET_PARM_INT_2(flag, name, sub, field)                                 \
    {                                                                          \
        if ((sub)->options2 & flag) {                                          \
            fprintf(parmfp, "%s=%d\n", name, (int) (sub)->field);              \
        } else if ((sub)->delOptions2 & flag) {                                \
            fprintf(parmfp, "%s=SUB_RESET\n", name);                           \
        }                                                                      \
    }

#define SET_PARM_NUMBER(name, field, delnum, defnum)                           \
    {                                                                          \
        if (field == delnum) {                                                 \
            fprintf(parmfp, "%s=SUB_RESET\n", name);                           \
        } else if (field != defnum) {                                          \
            fprintf(parmfp, "%s=%d\n", name, (int) field);                     \
        }                                                                      \
    }

    sprintf(esub, "%s/%s", lsbParams[LSB_SERVERDIR].paramValue, ESUBNAME);
    if (stat(esub, &sbuf) < 0)
        return 0;

    sprintf(parmFile, "%s/.lsbsubparm.%d", LSTMPDIR, (int) getpid());

    if ((parmfp = fopen(parmFile, "w")) == NULL) {
        lsberrno = LSBE_SYS_CALL;
        return -1;
    }

    chmod(parmFile, 0666);

    SET_PARM_STR(SUB_JOB_NAME, "LSB_SUB_JOB_NAME", jobSubReq, jobName);
    SET_PARM_STR(SUB_QUEUE, "LSB_SUB_QUEUE", jobSubReq, queue);
    SET_PARM_STR(SUB_IN_FILE, "LSB_SUB_IN_FILE", jobSubReq, inFile);
    SET_PARM_STR(SUB_OUT_FILE, "LSB_SUB_OUT_FILE", jobSubReq, outFile);
    SET_PARM_STR(SUB_ERR_FILE, "LSB_SUB_ERR_FILE", jobSubReq, errFile);
    SET_PARM_BOOL(SUB_EXCLUSIVE, "LSB_SUB_EXCLUSIVE", jobSubReq);
    SET_PARM_BOOL(SUB_NOTIFY_END, "LSB_SUB_NOTIFY_END", jobSubReq);
    SET_PARM_BOOL(SUB_NOTIFY_BEGIN, "LSB_SUB_NOTIFY_BEGIN", jobSubReq);
    SET_PARM_INT(SUB_CHKPNT_PERIOD, "LSB_SUB_CHKPNT_PERIOD", jobSubReq,
                 chkpntPeriod);
    SET_PARM_STR(SUB_CHKPNT_DIR, "LSB_SUB_CHKPNT_DIR", jobSubReq, chkpntDir);
    SET_PARM_BOOL(SUB_RESTART_FORCE, "LSB_SUB_RESTART_FORCE", jobSubReq);
    SET_PARM_BOOL(SUB_RESTART, "LSB_SUB_RESTART", jobSubReq);
    SET_PARM_BOOL(SUB_RERUNNABLE, "LSB_SUB_RERUNNABLE", jobSubReq);
    SET_PARM_BOOL(SUB_WINDOW_SIG, "LSB_SUB_WINDOW_SIG", jobSubReq);
    SET_PARM_STR(SUB_HOST_SPEC, "LSB_SUB_HOST_SPEC", jobSubReq, hostSpec);
    SET_PARM_STR(SUB_DEPEND_COND, "LSB_SUB_DEPEND_COND", jobSubReq, dependCond);
    SET_PARM_STR(SUB_RES_REQ, "LSB_SUB_RES_REQ", jobSubReq, resReq);
    SET_PARM_STR(SUB_PRE_EXEC, "LSB_SUB_PRE_EXEC", jobSubReq, preExecCmd);
    SET_PARM_STR(SUB_LOGIN_SHELL, "LSB_SUB_LOGIN_SHELL", jobSubReq, loginShell);
    SET_PARM_STR(SUB_MAIL_USER, "LSB_SUB_MAIL_USER", jobSubReq, mailUser);
    SET_PARM_BOOL(SUB_MODIFY, "LSB_SUB_MODIFY", jobSubReq);
    SET_PARM_BOOL(SUB_MODIFY_ONCE, "LSB_SUB_MODIFY_ONCE", jobSubReq);
    SET_PARM_STR(SUB_PROJECT_NAME, "LSB_SUB_PROJECT_NAME", jobSubReq,
                 projectName);
    SET_PARM_BOOL(SUB_INTERACTIVE, "LSB_SUB_INTERACTIVE", jobSubReq);
    SET_PARM_BOOL(SUB_PTY, "LSB_SUB_PTY", jobSubReq);
    SET_PARM_BOOL(SUB_PTY_SHELL, "LSB_SUB_PTY_SHELL", jobSubReq);

    SET_PARM_BOOL_2(SUB2_HOLD, "LSB_SUB_HOLD", jobSubReq);
    SET_PARM_INT_2(SUB2_JOB_PRIORITY, "LSB_SUB2_JOB_PRIORITY", jobSubReq,
                   userPriority);
    SET_PARM_STR_2(SUB2_IN_FILE_SPOOL, "LSB_SUB2_IN_FILE_SPOOL", jobSubReq,
                   inFile);
    SET_PARM_BOOL_2(SUB2_JOB_CMD_SPOOL, "LSB_SUB2_JOB_CMD_SPOOL", jobSubReq);

    ls_readconfenv(myParams, NULL);

    if (myParams[LSB_SUB_COMMANDNAME].paramValue) {
        int cmdSize, cmdNameSize, tmpCnt;

        int start = 0;

        cmdSize = strlen(jobSubReq->command);
        cmdNameSize = 0;

        if (strstr(jobSubReq->command, SCRIPT_WORD) != NULL) {
            int i = 0, found;
            char *p, *q;

            while (jobSubReq->command[i] != '\n')
                i++;

            start = i + 1;
            cmdNameSize = i + 1;

            do {
                found = 0;

                while ((cmdNameSize < cmdSize) &&
                       (isspace(jobSubReq->command[cmdNameSize]) ||
                        (jobSubReq->command[cmdNameSize] == '\n'))) {
                    start++;
                    cmdNameSize++;
                    found = 1;
                }

                while ((cmdNameSize < cmdSize) &&
                       (jobSubReq->command[start] == '#')) {
                    while ((cmdNameSize < cmdSize) &&
                           (jobSubReq->command[start] != '\n')) {
                        start++;
                        cmdNameSize++;
                    }
                    found = 1;
                }
            } while (found);

            p = strstr(&jobSubReq->command[start], SCRIPT_WORD_END);
            q = &jobSubReq->command[start];
            while (q != p) {
                q++;

                if (*q == '\n')
                    break;
            }

            if (q == p)
                start = cmdNameSize = cmdSize;
        }

        while (cmdNameSize < cmdSize) {
            if (isspace(jobSubReq->command[cmdNameSize]))
                break;
            cmdNameSize++;
        }

        fprintf(parmfp, "%s=\"", "LSB_SUB_COMMANDNAME");
        for (tmpCnt = start; tmpCnt < cmdNameSize; tmpCnt++) {
            if (jobSubReq->command[tmpCnt] == '"' ||
                jobSubReq->command[tmpCnt] == '\\')
                fprintf(parmfp, "\\");

            fprintf(parmfp, "%c", jobSubReq->command[tmpCnt]);
        }
        fprintf(parmfp, "\"\n");

        do {
            char *x = unwrapCommandLine(jobSubReq->command);
            fprintf(parmfp, "LSB_SUB_COMMAND_LINE=\"%s\"\n", x);
        } while (0);
    }

    if (jobSubReq->options & SUB_HOST) {
        char askedHosts[MAXLINELEN];

        askedHosts[0] = '\0';
        for (i = 0; i < jobSubReq->numAskedHosts; i++) {
            strcat(askedHosts, jobSubReq->askedHosts[i]);
            strcat(askedHosts, " ");
        }

        if (askedHosts[0] != '\0') {
            fprintf(parmfp, "LSB_SUB_HOSTS=\"%s\"\n", askedHosts);
        }
    } else if (jobSubReq->delOptions & SUB_HOST) {
        fprintf(parmfp, "LSB_SUB_HOSTS=SUB_RESET\n");
    }

    for (i = 0; i < LSF_RLIM_NLIMITS; i++) {
        SET_PARM_NUMBER(subRLimitName[i], jobSubReq->rLimits[i], DELETE_NUMBER,
                        DEFAULT_RLIMIT);
    }

    SET_PARM_NUMBER(
        "LSB_SUB_NUM_PROCESSORS",
        (jobSubReq->numProcessors ? jobSubReq->numProcessors : DEFAULT_NUMPRO),
        DEL_NUMPRO, DEFAULT_NUMPRO);
    SET_PARM_NUMBER("LSB_SUB_MAX_NUM_PROCESSORS",
                    (jobSubReq->maxNumProcessors ? jobSubReq->maxNumProcessors
                                                 : DEFAULT_NUMPRO),
                    DEL_NUMPRO, DEFAULT_NUMPRO);
    SET_PARM_NUMBER("LSB_SUB_BEGIN_TIME", jobSubReq->beginTime, DELETE_NUMBER,
                    0);
    SET_PARM_NUMBER("LSB_SUB_TERM_TIME", jobSubReq->termTime, DELETE_NUMBER, 0);

    if (jobSubReq->delOptions & SUB_OTHER_FILES) {
        fprintf(parmfp, "LSB_SUB_OTHER_FILES=SUB_RESET\n");
    } else if (jobSubReq->options & SUB_OTHER_FILES) {
        char str[MAXLINELEN];

        fprintf(parmfp, "LSB_SUB_OTHER_FILES=%d\n", jobSubReq->nxf);

        for (i = 0; i < jobSubReq->nxf; i++) {
            sprintf(str, "%s ", jobSubReq->xf[i].subFn);

            if (jobSubReq->xf[i].options & XF_OP_SUB2EXEC)
                strcat(str, ">");
            else if (jobSubReq->xf[i].options & XF_OP_SUB2EXEC_APPEND)
                strcat(str, ">>");
            if (jobSubReq->xf[i].options & XF_OP_EXEC2SUB_APPEND)
                strcat(str, "<<");
            else if (jobSubReq->xf[i].options & XF_OP_EXEC2SUB)
                strcat(str, "<");

            sprintf(str, "%s %s", str, jobSubReq->xf[i].execFn);
            fprintf(parmfp, "LSB_SUB_OTHER_FILES_%d=\"%s\"\n", i, str);
        }
    }

    if (additionEsubInfo != NULL) {
        fprintf(parmfp, "LSB_SUB_ADDITIONAL=\"%s\"\n", additionEsubInfo);
    }

    fclose(parmfp);

    putEnv("LSB_SUB_ABORT_VALUE", "97");
    putEnv("LSB_SUB_PARM_FILE", parmFile);

    if ((cc = runEsub_(ed, NULL)) < 0) {
        if (logclass & LC_TRACE)
            ls_syslog(LOG_DEBUG, "%s: runEsub_() failed %d: %M", fname, cc);
        if (cc == -2) {
            char *deltaFileName = NULL;
            struct stat stbuf;

            lsberrno = LSBE_ESUB_ABORT;
            unlink(parmFile);

            if ((deltaFileName = getenv("LSB_SUB_MODIFY_FILE")) != NULL) {
                if (stat(deltaFileName, &stbuf) != ENOENT)
                    unlink(deltaFileName);
            }

            deltaFileName = NULL;
            if ((deltaFileName = getenv("LSB_SUB_MODIFY_ENVFILE")) != NULL) {
                if (stat(deltaFileName, &stbuf) != ENOENT)
                    unlink(deltaFileName);
            }
            return -1;
        }
    }

    unlink(parmFile);

    return 0;
}

static int readOptFile(char *filename, char *childLine)
{
    char *p, *sp, *sline, *start;
    FILE *fp;

    if ((fp = fopen(filename, "r")) == NULL) {
        ls_syslog(LOG_ERR, "%s: %s(%s) failed: %m", "readOptFile", "fopen",
                  filename);
        return -1;
    }

    while ((sline = getNextLine_(fp, false)) != NULL) {
        sp = sline;
        SKIPSPACE(sp);

        if (*sp != '#')
            continue;
        if ((p = strstr(sp, "\\n")) != NULL)
            *p = '\0';
        ++sp;
        SKIPSPACE(sp);

        if (strncmp(sp, "BSUB", 4) == 0) {
            sp += 4;
            start = sp;
            SKIPSPACE(sp);
            if (*sp != '-')
                continue;
            else {
                if ((sp = strstr(sp, "#")) != NULL)

                    *sp = '\0';
                sp = sline;
                if (childLine[0] == '\0')
                    strcpy(childLine, sline);
                else
                    strncat(childLine, start, strlen(start));
                continue;
            }
        } else
            continue;
    }

    fclose(fp);
    return 0;
}

int bExceptionTabInit(void)
{
    bExceptionTab = (hTab *) malloc(sizeof(hTab));
    if (bExceptionTab) {
        h_initTab_(bExceptionTab, 8);
        return 0;
    } else
        return -1;
}

static bException_t *bExceptionCreate(void)
{
    bException_t *exception;
    exception = (bException_t *) malloc(sizeof(bException_t));

    if (exception) {
        exception->name = NULL;
        exception->handler = NULL;
    }

    return exception;
}

int mySubUsage_(void *args)
{
    struct args {
        int argc;
        char **argv;
    } *myArgs;
    myArgs = (struct args *) args;
    subUsage_(myArgs->argc, myArgs->argv);
    return 0;
}

void lsb_throw(const char *exceptionName, void *extra)
{
    bException_t *exception;
    hEnt *hEnt;

    if (!exceptionName || *exceptionName == '\0')
        return;

    if (!bExceptionTab)
        return;

    hEnt = h_getEnt_(bExceptionTab, (char *) exceptionName);

    if (!hEnt)
        return;

    exception = (bException_t *) hEnt->hData;

    if (!exception || !exception->handler)
        return;
    else
        (*exception->handler)(extra);
}

int lsb_catch(const char *exceptionName, int (*exceptionHandler)(void *))
{
    static char fname[] = "lsb_catch()";
    hEnt *hEnt;
    void *hData;
    bException_t *exception;

    if (!exceptionName || *exceptionName == '\0') {
        lsberrno = LSBE_BAD_ARG;
        return -1;
    }

    if (!bExceptionTab) {
        lsberrno = LSBE_LSLIB;
        return -1;
    }

    hEnt = h_getEnt_(bExceptionTab, (char *) exceptionName);
    if (!hEnt) {
        hData = (int *) bExceptionCreate();
        if (!hData) {
            fprintf(stderr, "%s: failed to create a exception for key %s\n",
                    fname, exceptionName);
            lsberrno = LSBE_LSBLIB;
            return -1;
        }
        hEnt = h_addEnt_(bExceptionTab, (char *) exceptionName, NULL);
        hEnt->hData = (int *) hData;
    }

    exception = (bException_t *) hEnt->hData;
    if (!exception->name)
        exception->name = strdup((char *) exceptionName);
    exception->handler = exceptionHandler;

    return 0;
}

#define msg msg

#define MSG_BAD_ENVAR2s                                                        \
    "%s: Bad parameter variable name(%s) "                                     \
    "read from $LSB_SUB_MODIFY_FILE, the "                                     \
    "setting will be ignored."

#define MSG_BAD_INTVAL3s                                                       \
    "%s: Bad value(%s) read from "                                             \
    "$LSB_SUB_MODIFY_FILE for parameter "                                      \
    "%s, the setting will be ignored. "                                        \
    "The value of this parameter can "                                         \
    "only be SUB_RESET or an integer."

#define MSG_ALLOC_MEMF_IN_XFs                                                  \
    "%s: Memory allocate failed for "                                          \
    "file transfer request."

#define MSG_BAD_XF_OP2s                                                        \
    "%s: unknown file transfer operator"                                       \
    " %s, this transfer request will be"                                       \
    " ignored."

#define MSG_BAD_BOOLVAL3s                                                      \
    "%s: Bad value(%s) read from "                                             \
    "$LSB_SUB_MODIFY_FILE for parameter"                                       \
    " %s, the setting will be ignored. "                                       \
    "The value of this parameter can "                                         \
    "only be SUB_RESET or 'Y'."

#define MSG_VAL_RESET_INT2s                                                    \
    "%s: The value of %s can only be "                                         \
    "SUB_RESET or an integer."

#define MSG_BAD_ENVAL3s                                                        \
    "%s: Bad value(%s) read from "                                             \
    "$LSB_SUB_MODIFY_FILE for parameter"                                       \
    " %s, the setting will be ignored."

#define MSG_WARN_NULLVAL2s                                                     \
    "%s: The value of parameter %s is "                                        \
    "empty, the setting will be ignored."

void makeCleanToRunEsub()
{
    char parmDeltaFile[MAXPATHLEN];
    char envDeltaFile[MAXPATHLEN];
    struct stat stbuf;

    sprintf(parmDeltaFile, "%s/.lsbsubdeltaparm.%d.%d", LSTMPDIR,
            (int) getpid(), (int) getuid());
    sprintf(envDeltaFile, "%s/.lsbsubdeltaenv.%d.%d", LSTMPDIR, (int) getpid(),
            (int) getuid());

    if (stat(parmDeltaFile, &stbuf) != ENOENT) {
        unlink(parmDeltaFile);
    }

    if (stat(envDeltaFile, &stbuf) != ENOENT) {
        unlink(envDeltaFile);
    }

    putEnv("LSB_SUB_MODIFY_FILE", parmDeltaFile);
    putEnv("LSB_SUB_MODIFY_ENVFILE", envDeltaFile);
}

void modifyJobInformation(struct submit *jobSubReq)
{
    static char fname[] = "modifyJobInforation";
    char parmDeltaFile[MAXPATHLEN];
    char envDeltaFile[MAXPATHLEN];
    int validKey, v;
    char *sValue;

#define INTPARM 1
#define STRPARM 2
#define NUMPARM 3
#define BOOLPARM 4
#define INT2PARM 5
#define STR2PARM 6
#define BOOL2PARM 7
#define RLIMPARM 8
#define STRSPARM 9

#define FIELD_OFFSET(type, field) (long) (&(((struct type *) 0)->field))
#define FIELD_PTR_PTR(base, offset) (((char *) base) + offset)

    FILE *fp;
    int lineNum;
    char *line = NULL, *key;
    static struct {
        char *parmName;
        int parmType;
        long fieldOffset;
        long subOption;
    } jobSubReqParams[] = {
        {"LSB_SUB_JOB_NAME", STRPARM, FIELD_OFFSET(submit, jobName),
         SUB_JOB_NAME},
        {"LSB_SUB_QUEUE", STRPARM, FIELD_OFFSET(submit, queue), SUB_QUEUE},
        {"LSB_SUB_IN_FILE", STRPARM, FIELD_OFFSET(submit, inFile), SUB_IN_FILE},
        {"LSB_SUB_OUT_FILE", STRPARM, FIELD_OFFSET(submit, outFile),
         SUB_OUT_FILE},
        {"LSB_SUB_ERR_FILE", STRPARM, FIELD_OFFSET(submit, errFile),
         SUB_ERR_FILE},
        {"LSB_SUB_EXCLUSIVE", BOOLPARM, -1, SUB_EXCLUSIVE},
        {"LSB_SUB_NOTIFY_END", BOOLPARM, -1, SUB_NOTIFY_END},
        {"LSB_SUB_OUT_NOTIFY_BEGIN", BOOLPARM, -1, SUB_NOTIFY_BEGIN},
        {"LSB_SUB_CHKPNT_PERIOD", INTPARM, FIELD_OFFSET(submit, chkpntPeriod),
         SUB_CHKPNT_PERIOD},
        {"LSB_SUB_CHKPNT_DIR", STRPARM, FIELD_OFFSET(submit, chkpntDir),
         SUB_CHKPNT_DIR},
        {"LSB_SUB_RESTART_FORCE", BOOLPARM, -1, SUB_RESTART_FORCE},
        {"LSB_SUB_RESTART", BOOLPARM, -1, SUB_RESTART},
        {"LSB_SUB_RERUNNABLE", BOOLPARM, -1, SUB_RERUNNABLE},
        {"LSB_SUB_WINDOW_SIG", BOOLPARM, -1, SUB_WINDOW_SIG},
        {"LSB_SUB_HOST_SPEC", STRPARM, FIELD_OFFSET(submit, hostSpec),
         SUB_HOST_SPEC},
        {"LSB_SUB_DEPEND_COND", STRPARM, FIELD_OFFSET(submit, dependCond),
         SUB_DEPEND_COND},
        {"LSB_SUB_RES_REQ", STRPARM, FIELD_OFFSET(submit, resReq), SUB_RES_REQ},
        {"LSB_SUB_PRE_EXEC", STRPARM, FIELD_OFFSET(submit, preExecCmd),
         SUB_PRE_EXEC},
        {"LSB_SUB_LOGIN_SHELL", STRPARM, FIELD_OFFSET(submit, loginShell),
         SUB_LOGIN_SHELL},
        {"LSB_SUB_MAIL_USER", STRPARM, FIELD_OFFSET(submit, mailUser),
         SUB_MAIL_USER},
        {"LSB_SUB_MODIFY", BOOLPARM, -1, SUB_MODIFY},
        {"LSB_SUB_MODIFY_ONCE", BOOLPARM, -1, SUB_MODIFY_ONCE},
        {"LSB_SUB_PROJECT_NAME", STRPARM, FIELD_OFFSET(submit, projectName),
         SUB_PROJECT_NAME},
        {"LSB_SUB_INTERACTIVE", BOOLPARM, -1, SUB_INTERACTIVE},
        {"LSB_SUB_PTY", BOOLPARM, -1, SUB_PTY},
        {"LSB_SUB_PTY_SHELL", BOOLPARM, -1, SUB_PTY_SHELL},
        {"LSB_SUB_HOSTS", STRSPARM, FIELD_OFFSET(submit, askedHosts), 0},
        {"LSB_SUB_HOLD", BOOL2PARM, -1, SUB2_HOLD},
        {"LSB_SUB2_JOB_PRIORITY", INT2PARM, FIELD_OFFSET(submit, userPriority),
         SUB2_JOB_PRIORITY},
        {"LSB_SUB_RLIMIT_CPU", RLIMPARM, 0, -1},
        {"LSB_SUB_RLIMIT_FSIZE", RLIMPARM, 1, -1},
        {"LSB_SUB_RLIMIT_DATA", RLIMPARM, 2, -1},
        {"LSB_SUB_RLIMIT_STACK", RLIMPARM, 3, -1},
        {"LSB_SUB_RLIMIT_CORE", RLIMPARM, 4, -1},
        {"LSB_SUB_RLIMIT_RSS", RLIMPARM, 5, -1},
        {"LSB_SUB_RLIMIT_NOFILE", RLIMPARM, 6, -1},
        {"LSB_SUB_RLIMIT_OPEN_MAX", RLIMPARM, 7, -1},
        {"LSB_SUB_RLIMIT_SWAP", RLIMPARM, 8, -1},
        {"LSB_SUB_RLIMIT_RUN", RLIMPARM, 9, -1},
        {"LSB_SUB_RLIMIT_PROCESS", RLIMPARM, 10, -1},
        {"LSB_SUB_NUM_PROCESSORS", NUMPARM, FIELD_OFFSET(submit, numProcessors),
         DEFAULT_NUMPRO},
        {"LSB_SUB_MAX_NUM_PROCESSORS", NUMPARM,
         FIELD_OFFSET(submit, maxNumProcessors), DEFAULT_NUMPRO},
        {"LSB_SUB_BEGIN_TIME", NUMPARM, FIELD_OFFSET(submit, beginTime), 0},
        {"LSB_SUB_TERM_TIME", NUMPARM, FIELD_OFFSET(submit, termTime), 0},
        {"LSB_SUB_COMMAND_LINE", STRPARM, FIELD_OFFSET(submit, command), 0},
        {NULL, 0, 0, 0}};

    sprintf(parmDeltaFile, "%s/.lsbsubdeltaparm.%d.%d", LSTMPDIR,
            (int) getpid(), (int) getuid());
    sprintf(envDeltaFile, "%s/.lsbsubdeltaenv.%d.%d", LSTMPDIR, (int) getpid(),
            (int) getuid());

    if (access(parmDeltaFile, R_OK) == F_OK) {
        fp = fopen(parmDeltaFile, "r");
        lineNum = 0;

        while ((line = getNextLineC_(fp, &lineNum, true)) != NULL) {
            int i, j;

            key = getNextWordSet(&line, " \t=!@#$%^&*()");

            while (*line != '=')
                line++;
            line++;
            while (isspace((int) *line))
                line++;

            validKey = 0;

            if (strncmp(key, "LSB_SUB_OTHER_FILES",
                        strlen("LSB_SUB_OTHER_FILES")) == 0) {
                processXFReq(key, line, jobSubReq);
                continue;
            }

            for (i = 0; jobSubReqParams[i].parmName; i++) {
                if (strcmp(key, jobSubReqParams[i].parmName) == 0) {
                    validKey = 1;

                    switch (jobSubReqParams[i].parmType) {
                    case STRPARM:
                        if (checkEmptyString(line)) {
                            ls_syslog(LOG_WARNING, MSG_WARN_NULLVAL2s, fname,
                                      key);
                            break;
                        }

                        if ((strcmp(key, "LSB_SUB_COMMAND_LINE") == 0) &&
                            (jobSubReq->options & SUB_RESTART)) {
                            break;
                        }

                        if (stringIsToken(line, "SUB_RESET")) {
                            jobSubReq->options &= ~jobSubReqParams[i].subOption;
                            jobSubReq->delOptions |=
                                jobSubReqParams[i].subOption;
                        } else {
                            sValue = extractStringValue(line);
                            if (sValue == NULL) {
                                ls_syslog(LOG_WARNING, MSG_BAD_ENVAL3s, fname,
                                          line, key);
                                break;
                            }

                            if (strcmp(key, "LSB_SUB_COMMAND_LINE") != 0) {
                                *(char **) (FIELD_PTR_PTR(
                                    jobSubReq,
                                    jobSubReqParams[i].fieldOffset)) =
                                    putstr_(sValue);
                            } else {
                                *(char **) (FIELD_PTR_PTR(
                                    jobSubReq,
                                    jobSubReqParams[i].fieldOffset)) =
                                    wrapCommandLine(sValue);
                                if (jobSubReq->options & SUB_MODIFY) {
                                    jobSubReq->newCommand = jobSubReq->command;
                                    jobSubReq->options2 |= SUB2_MODIFY_CMD;
                                    jobSubReq->delOptions2 &= ~SUB2_MODIFY_CMD;
                                }
                            }

                            jobSubReq->options |= jobSubReqParams[i].subOption;
                            jobSubReq->delOptions &=
                                ~jobSubReqParams[i].subOption;
                        }
                        break;
                    case INTPARM:
                        if (stringIsToken(line, "SUB_RESET")) {
                            jobSubReq->options &= ~jobSubReqParams[i].subOption;
                            jobSubReq->delOptions |=
                                jobSubReqParams[i].subOption;
                        } else {
                            if (!stringIsDigitNumber(line)) {
                                ls_syslog(LOG_WARNING, MSG_BAD_INTVAL3s, fname,
                                          line, key);
                                break;
                            }

                            v = atoi(line);
                            *(int *) (FIELD_PTR_PTR(
                                jobSubReq, jobSubReqParams[i].fieldOffset)) = v;
                            jobSubReq->options |= jobSubReqParams[i].subOption;
                            jobSubReq->delOptions &=
                                ~jobSubReqParams[i].subOption;
                        }
                        break;
                    case BOOLPARM:
                        if (stringIsToken(line, "Y")) {
                            jobSubReq->options |= jobSubReqParams[i].subOption;
                            jobSubReq->delOptions &=
                                ~jobSubReqParams[i].subOption;
                        } else if (stringIsToken(line, "SUB_RESET")) {
                            jobSubReq->options &= ~jobSubReqParams[i].subOption;
                            jobSubReq->delOptions |=
                                jobSubReqParams[i].subOption;
                        } else {
                            ls_syslog(LOG_WARNING, MSG_BAD_BOOLVAL3s, fname,
                                      line, key);
                        }
                        break;
                    case STR2PARM:
                        if (checkEmptyString(line)) {
                            ls_syslog(LOG_WARNING, MSG_WARN_NULLVAL2s, fname,
                                      key);
                            break;
                        }

                        if (stringIsToken(line, "SUB_RESET")) {
                            jobSubReq->options2 &=
                                ~jobSubReqParams[i].subOption;
                            jobSubReq->delOptions2 |=
                                jobSubReqParams[i].subOption;
                        } else {
                            sValue = extractStringValue(line);
                            if (sValue == NULL) {
                                ls_syslog(LOG_WARNING, MSG_BAD_ENVAL3s, fname,
                                          line, key);
                                break;
                            }

                            *(char **) (FIELD_PTR_PTR(
                                jobSubReq, jobSubReqParams[i].fieldOffset)) =
                                putstr_(sValue);

                            jobSubReq->options2 |= jobSubReqParams[i].subOption;
                            jobSubReq->delOptions2 &=
                                ~jobSubReqParams[i].subOption;
                        }
                        break;
                    case INT2PARM:
                        if (stringIsToken(line, "SUB_RESET")) {
                            jobSubReq->options2 &=
                                ~jobSubReqParams[i].subOption;
                            jobSubReq->delOptions2 |=
                                jobSubReqParams[i].subOption;
                        } else {
                            if (!stringIsDigitNumber(line)) {
                                ls_syslog(LOG_WARNING, MSG_BAD_INTVAL3s, fname,
                                          line, key);
                                break;
                            }

                            v = atoi(line);
                            *(int *) (FIELD_PTR_PTR(
                                jobSubReq, jobSubReqParams[i].fieldOffset)) = v;
                            jobSubReq->options2 |= jobSubReqParams[i].subOption;
                            jobSubReq->delOptions2 &=
                                ~jobSubReqParams[i].subOption;
                        }
                        break;
                    case BOOL2PARM:
                        if (stringIsToken(line, "Y")) {
                            jobSubReq->options2 |= jobSubReqParams[i].subOption;
                            jobSubReq->delOptions2 &=
                                ~jobSubReqParams[i].subOption;
                        } else if (stringIsToken(line, "SUB_RESET")) {
                            jobSubReq->options2 &=
                                ~jobSubReqParams[i].subOption;
                            jobSubReq->delOptions2 |=
                                jobSubReqParams[i].subOption;
                        } else {
                            ls_syslog(LOG_WARNING, MSG_BAD_BOOLVAL3s, fname,
                                      line, key);
                        }
                        break;
                    case NUMPARM:
                        if (stringIsToken(line, "SUB_RESET")) {
                            *(int *) (FIELD_PTR_PTR(
                                jobSubReq, jobSubReqParams[i].fieldOffset)) =
                                jobSubReqParams[i].subOption;
                        } else {
                            if (!stringIsDigitNumber(line)) {
                                ls_syslog(LOG_WARNING, MSG_BAD_INTVAL3s, fname,
                                          line, key);
                                break;
                            }

                            v = atoi(line);
                            *(int *) (FIELD_PTR_PTR(
                                jobSubReq, jobSubReqParams[i].fieldOffset)) = v;
                        }

                        if (jobSubReq->maxNumProcessors <
                            jobSubReq->numProcessors) {
                            jobSubReq->maxNumProcessors =
                                jobSubReq->numProcessors;
                        }

                        break;
                    case RLIMPARM:
                        j = jobSubReqParams[i].fieldOffset;
                        if (stringIsToken(line, "SUB_RESET")) {
                            jobSubReq->rLimits[j] = DELETE_NUMBER;
                        } else {
                            if (!stringIsDigitNumber(line)) {
                                ls_syslog(LOG_WARNING, MSG_BAD_INTVAL3s, fname,
                                          line, key);
                                break;
                            }

                            v = atoi(line);
                            jobSubReq->rLimits[j] = v;
                        }
                        break;
                    case STRSPARM:
                        if (checkEmptyString(line)) {
                            ls_syslog(LOG_WARNING, MSG_WARN_NULLVAL2s, fname,
                                      key);
                            break;
                        }

                        sValue = extractStringValue(line);
                        if (sValue == NULL) {
                            ls_syslog(LOG_WARNING, MSG_BAD_ENVAL3s, fname, line,
                                      key);
                            break;
                        }

                        if (strcmp(key, "LSB_SUB_HOSTS") == 0) {
                            int badIdx;

                            if (getAskedHosts_(sValue, &jobSubReq->askedHosts,
                                               &jobSubReq->numAskedHosts,
                                               &badIdx, false) < 0) {
                                jobSubReq->options &= ~SUB_HOST;
                                ls_syslog(LOG_WARNING, ls_sysmsg());
                            } else {
                                jobSubReq->options |= SUB_HOST;
                            }
                        }
                        break;
                    default:
                        ls_syslog(LOG_WARNING, MSG_BAD_ENVAR2s, fname, key);
                        break;
                    }
                    break;
                }
            }

            if (!validKey) {
                ls_syslog(LOG_WARNING, MSG_BAD_ENVAR2s, fname, key);
            }
        }
        fclose(fp);

        unlink(parmDeltaFile);
    }

    compactXFReq(jobSubReq);

    if (access(envDeltaFile, R_OK) == F_OK) {
        fp = fopen(envDeltaFile, "r");
        lineNum = 0;

        while ((line = getNextLineC_(fp, &lineNum, true)) != NULL) {
            key = getNextWordSet(&line, " \t =!@#$%^&*()");
            while (*line != '=')
                line++;

            line++;

            putEnv(key, getNextValueQ_(&line, '"', '"'));
        }
        fclose(fp);
        unlink(envDeltaFile);
    }
}

char *unwrapCommandLine(char *commandLine)
{
    char *sp = NULL;
    static char *jobDespBuf = NULL;
    static char *lineStrBuf = NULL;
    char *jobdesp = NULL;
    char *p1, *p2;
    int hasNonSpaceC = 0;

    FREEUP(lineStrBuf);
    FREEUP(jobDespBuf);
    lineStrBuf = putstr_(commandLine);
    if (!lineStrBuf) {
        lsberrno = LSBE_NO_MEM;
        return NULL;
    }
    jobdesp = lineStrBuf;
    sp = (char *) strstr(jobdesp, "SCRIPT_\n");
    if (sp == NULL) {
        jobDespBuf = putstr_(jobdesp);
        return &jobDespBuf[0];
    }

    jobdesp = sp + strlen("SCRIPT_\n");
    sp = NULL;
    sp = strstr(jobdesp, "SCRIPT_\n");
    if (sp == NULL) {
        jobDespBuf = putstr_(jobdesp);
        return &jobDespBuf[0];
    }
    while (*sp != '\n')
        sp--;
    sp++;
    *sp = '\0';

    jobDespBuf = putstr_(jobdesp);
    p1 = NULL;
    p2 = jobDespBuf;
    hasNonSpaceC = 0;
    while (*p2) {
        if (*p2 == '\n') {
            *p2 = ' ';
            if (p1 != NULL) {
                if (hasNonSpaceC) {
                    *p1 = ';';
                } else
                    *p1 = ' ';
            }
            p1 = p2;
            hasNonSpaceC = 0;
        } else if (!isspace((int) *p2))
            hasNonSpaceC = 1;
        p2++;
    }
    return &jobDespBuf[0];
}

char *wrapCommandLine(char *command)
{
    static char *szTmpShellCommands = "\n_USER_SCRIPT_\n) "
                                      "> $LSB_CHKFILENAME.shell\n"
                                      "chmod u+x $LSB_CHKFILENAME.shell\n"
                                      "$LSB_JOBFILENAME.shell\n"
                                      "saveExit=$?\n"
                                      "/bin/rm -f $LSB_JOBFILENAME.shell\n"
                                      "(exit $saveExit)\n";

    static char cmdString[MAXLINELEN * 4];

    if (strchr(command, (int) '\n') == NULL) {
        strcpy(cmdString, command);
        return cmdString;
    }

    sprintf(cmdString, "(cat <<_USER_\\SCRIPT_\n%s\n%s", command,
            szTmpShellCommands);
    return cmdString;
}

void compactXFReq(struct submit *jobSubReq)
{
    int i, j;

    i = 0;
    j = 0;
    while (i < jobSubReq->nxf) {
        if (jobSubReq->xf[i].options != 0) {
            if (i != j) {
                memcpy(&(jobSubReq->xf[j]), &(jobSubReq->xf[i]),
                       sizeof(struct xFile));
            }
            j++;
        }
        i++;
    }
    jobSubReq->nxf = j;
}

int checkEmptyString(char *s)
{
    char *p = s;

    while (*p) {
        if (!isspace((int) *p))
            return 0;
        p++;
    }

    return 1;
}

int stringIsToken(char *s, char *tok)
{
    char *s1 = s;

    while (isspace((int) *s1) && (*s1))
        s1++;

    if (strncmp(s1, tok, strlen(tok)) == 0) {
        char *p = s1 + strlen(tok);
        return checkEmptyString(p);
    }

    return 0;
}

int stringIsDigitNumber(char *s)
{
    char *s1 = s;

    while (isspace((int) *s1) && (*s1))
        s1++;

    if (*s1 == 0x0)
        return 0;

    if (*s1 == '0') {
        return checkEmptyString(s1 + 1);
    }

    while (isdigit((int) *s1))
        s1++;

    return checkEmptyString(s1);
}

char *extractStringValue(char *line)
{
    char *p;
    static char sValue[MAXLINELEN];
    int i;

    p = line;
    while (isspace((int) *p) && (*p != 0x0))
        p++;

    if (*p != '\"') {
        return NULL;
    }

    p++;
    i = 0;
    while ((*p != 0x0) && (*p != '\"')) {
        sValue[i] = *p;
        p++;
        i++;
    }

    if (*p == 0)
        return NULL;

    p++;
    if (checkEmptyString(p)) {
        sValue[i] = 0x0;
        return sValue;
    }

    return NULL;
}

int processXFReq(char *key, char *line, struct submit *jobSubReq)
{
    static char fname[] = "processXFRequest";
    int v;
    char *sValue;

    if (strcmp(key, "LSB_SUB_OTHER_FILES") == 0) {
        if (stringIsToken(line, "SUB_RESET")) {
            free(jobSubReq->xf);
            jobSubReq->xf = NULL;
            jobSubReq->options &= ~SUB_OTHER_FILES;
            jobSubReq->delOptions |= SUB_OTHER_FILES;
            jobSubReq->nxf = 0;
        } else {
            struct xFile *p;

            if (!stringIsDigitNumber(line)) {
                ls_syslog(LOG_WARNING, MSG_BAD_INTVAL3s, fname, line, key);
                return -1;
            }

            v = atoi(line);
            p = (struct xFile *) malloc(v * sizeof(struct xFile));

            if (p == NULL) {
                ls_syslog(LOG_ERR, MSG_ALLOC_MEMF_IN_XFs, fname);
                return -1;
            } else {
                free(jobSubReq->xf);
                jobSubReq->nxf = v;
                jobSubReq->options |= SUB_OTHER_FILES;
                jobSubReq->delOptions &= SUB_OTHER_FILES;
                jobSubReq->xf = p;
                memset(jobSubReq->xf, 0x0, v * sizeof(struct xFile));
            }
        }
    } else {
        char *sp = key + strlen("LSB_SUB_OTHER_FILES_");
        char xfSeq[32];

        xfSeq[0] = 0x0;
        strncat(xfSeq, sp, sizeof(xfSeq) - 2);

        if (!stringIsDigitNumber(xfSeq)) {
            ls_syslog(LOG_WARNING, MSG_BAD_ENVAR2s, fname, key);
            return -1;
        }

        v = atoi(xfSeq);
        if (v < jobSubReq->nxf) {
            char op[20];
            char *txt, *srcf;

            jobSubReq->xf[v].options = 0;

            sValue = extractStringValue(line);
            if (sValue == NULL) {
                ls_syslog(LOG_WARNING, MSG_BAD_ENVAL3s, fname, line, sValue);
                return -1;
            }

            txt = sValue;
            while (isspace((int) *txt) && (*txt != 0x0))
                txt++;

            srcf = getNextWordSet(&txt, " \t<>");

            if (srcf == NULL) {
                ls_syslog(LOG_WARNING, MSG_BAD_ENVAL3s, fname, line, sValue);
                return -1;
            }

            strcpy(jobSubReq->xf[v].subFn, srcf);
            while (isspace((int) *txt) && (*txt != 0x0))
                txt++;

            op[0] = op[1] = op[2] = '\000';
            op[0] = *txt++;
            if (!isspace((int) *txt))
                op[1] = *txt++;

            while (isspace((int) *txt) && (*txt != 0x0))
                txt++;
            strcpy(jobSubReq->xf[v].execFn, txt);

            if (strcmp(op, "<") == 0) {
                jobSubReq->xf[v].options |= XF_OP_EXEC2SUB;
            } else if (strcmp(op, "<<") == 0) {
                jobSubReq->xf[v].options |= XF_OP_EXEC2SUB_APPEND;
                jobSubReq->xf[v].options |= XF_OP_EXEC2SUB;
            } else if ((strcmp(op, "<>") == 0) || (strcmp(op, "><") == 0)) {
                jobSubReq->xf[v].options |= XF_OP_EXEC2SUB;
                jobSubReq->xf[v].options |= XF_OP_SUB2EXEC;
            } else if (strcmp(op, ">") == 0) {
                jobSubReq->xf[v].options |= XF_OP_SUB2EXEC;
            } else if (strcmp(op, ">>") == 0) {
                jobSubReq->xf[v].options |= XF_OP_SUB2EXEC_APPEND;
                jobSubReq->xf[v].options |= XF_OP_SUB2EXEC;
            } else {
                ls_syslog(LOG_WARNING, MSG_BAD_XF_OP2s, fname, op);
            }
        } else {
            ls_syslog(LOG_WARNING, MSG_BAD_ENVAL3s, fname, line, key);
        }
    }

    return 0;
}
static void trimSpaces(char *str)
{
    char *ptr;

    if (!str || str[0] == '\0') {
        return;
    }

    while (isspace((int) *str)) {
        for (ptr = str; *ptr; ptr++) {
            *ptr = ptr[1];
        }
    }

    ptr = str;
    while (*ptr) {
        ptr++;
    }

    ptr--;
    while (ptr >= str && isspace((int) *ptr)) {
        *ptr = '\0';
        ptr--;
    }
}

// Bug implement it
static int getAskedHosts_(char *optarg, char ***askedHosts, int *numAskedHosts,
                          int *badIdx, int options)
{
    (void) optarg;
    (void) askedHosts;
    (void) numAskedHosts;
    (void) badIdx;
    (void) options;

    fprintf(stderr, "-m option not implemented yet\n");
    return 1;
}
