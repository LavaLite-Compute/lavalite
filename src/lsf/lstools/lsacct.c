/* $Id: lsacct.c,v 1.3 2007/08/15 22:18:55 tmizan Exp $
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
#include "lsf/lib/lib.h"
#include "lsf/lib/lsi18n.h"

struct interval {
    time_t begin;
    time_t end;
};

#define MAX_PRINTLINE LBUF_SIZ
static char printline[MAX_PRINTLINE + 1];
static const char *dashed_line =
"------------------------------------------------------------------------------"
"--";


#define MAX_LOGIN 31
#define MAX_PATH 1000
#define MAX_USERS 1000
#define MAX_HOSTS 1000
#define MAX_PIDS 1000
static int details;
static char logfile[MAX_PATH + 1];
static char mylogin[MAX_LOGIN + 1];
static char *users[MAX_USERS + 1];
static char *hosts[MAX_HOSTS + 1];
static struct tm creation =
{ 0, 0, 0, 1, 0, 70, 4, 0, 0, 0, 0};

static struct tm destruction =
{ 0, 0, 0, 1, 0, 120, 3, 0, 0, 0, 0};

static struct interval eternity;
static struct interval start;
static struct interval complete;
static int pids[MAX_PIDS + 1];

static int totstatnz;
static int totstatz;
static int tottasks;

static struct interval actual_start;
static struct interval actual_complete;

enum  resource {cpu, pf, swaps, bin, bout, msgs, msgr, vcsw, ivcsw, turn};
enum useage {min, max, tot};
#define NUM_RESOURCES 10
#define NUM_USAGE 3
static double statistics[NUM_RESOURCES][NUM_USAGE];

static int resRecNum[NUM_RESOURCES];

static void setdefaults(void);
static void getoptions(int argc, char *argv[]);
static void getpids(int argc, char *argv[]);
static void printheader(void);
static void initstats(void);
static void processlogfile(void);
static void printsummary(void);
static void usage(const char *cmd);
static void getarglist(char *argstr, char *arglist[], int n);
static void expandhostnames(void);
static void processacctrec(const struct lsfAcctRec *acctrec);
static void printacctrec(const struct lsfAcctRec *acctrec);
static void processresuse(enum resource res, double use);
static void processtime(time_t time, struct interval *inter);
static void printresuse(enum resource res, const char *label);
static int isinteresting(const struct lsfAcctRec *acctrec);
static int innamelist(const char *name, char *namelist[]);
static int innumlist(int num, const int numlist[]);
static int ininterval(time_t time, struct interval inter);
static struct interval mkinterval(time_t begin, time_t end);
static struct interval getinterval(char *timeform);

#define NL_SETN 27

static void
setdefaults(void)
{
    int n;
    char *confpath, *localHost;
    struct config_param acctdir[] = {    {"LSF_RES_ACCTDIR", NULL},
        {NULL, NULL}
    };
    struct stat statBuf;
    char lsfUserName[MAX_LOGIN + 1];

    details = 0;

    if ((confpath = getenv("LSF_ENVDIR")) == NULL)
        confpath = "/etc";

    ls_readconfenv(acctdir, confpath);

    if (acctdir[0].paramValue == NULL
            || stat(acctdir[0].paramValue, &statBuf) == -1)
        acctdir[0].paramValue = "/tmp";
    strcpy(logfile, acctdir[0].paramValue);
    n = strlen(logfile);
    strncat(logfile, "/lsf.acct.", MAX_PATH - n);
    if ((localHost = ls_getmyhostname()) == NULL) {
        ls_perror("ls_getmyhostname");
        return;
    }
    n = strlen(logfile);
    strncat(logfile, localHost, MAX_PATH - n);

    if (getpwnam(lsfUserName) == NULL) {
        ls_perror("getpwnam");
        return;
    }

    strcpy(mylogin, lsfUserName);
    users[0] = mylogin;
    users[1] = NULL;

    hosts[0] = NULL;

    eternity = mkinterval(mktime(&creation), mktime(&destruction));
    start = complete = eternity;
}


static void
getoptions(int argc, char *argv[])
{
    extern char *optarg;

    int cc;

    while ((cc = getopt(argc, argv, "hVlf:u:m:C:S:")) != EOF) {
        switch (cc) {
            case 'C':
                complete = getinterval(optarg);
                break;
            case 'S':
                start = getinterval(optarg);
                break;
            case 'V':
                fputs(_LAVALITE_VERSION_, stderr);
                exit(-1);
            case 'f':
                strcpy(logfile, optarg);
                break;
            case 'h':
                usage(argv[0]);
            case 'l':
                details = 1;
                break;
            case 'm':
                getarglist(optarg, hosts, MAX_HOSTS + 1);
                expandhostnames();
                break;
            case 'u':
                getarglist(optarg, users, MAX_USERS + 1);
                if (strcmp(users[0], "all") == 0)
                    users[0] = NULL;
                break;
            case '?':
            default:
                usage(argv[0]);
        }
    }
}


static void
getpids(int argc, char *argv[])
{
    int i;
    int pid;

    for (i = 0; optind < argc; optind++) {
        pid = atoi(argv[optind]);
        if (pid != 0) {
            if (i < MAX_PIDS)
                pids[i++] = pid;
            else {
                fprintf(stderr,  "lsacct: %s pids\n",
                        I18N(1201, "too many")); /* catgets 1201 */
                exit(-1);
            }
        }
    }

    pids[i] = 0;
}


static void
printheader(void)
{
    int i;
    char localTimeStrStart[50];
    char localTimeStrEnd[50];

    printf(_i18n_msg_get(ls_catd,NL_SETN,1202, "Accounting information in %s about commands\n"), /* catgets  1202  */
            logfile);

    printf(" - %s", I18N(1203, "executed by user(s)")); /* catgets  1203  */
    if (users[0] == NULL)
        printf(" %s", I18N(1212, "all"));
    else
        for (i = 0; users[i]; i++)
            printf(" %s", users[i]);
    printf("\n");

    printf(" - %s", I18N(1204, "executed on host(s)")); /* catgets  1204  */
    if (hosts[0] == NULL)
        printf(" %s", I18N(1212, "all"));
    else
        for (i = 0; hosts[i]; i++)
            printf(" %s", hosts[i]);
    printf("\n");

    if (start.begin == eternity.begin && start.end == eternity.end)
        printf("  - %s", I18N(1205, "started any time")); /* catgets  1205  */
    else {
        strcpy ( localTimeStrStart, _i18n_ctime ( ls_catd, 1, &start.begin ));
        strcpy ( localTimeStrEnd,   _i18n_ctime ( ls_catd, 1, &start.end ));
        sprintf(printline,
                I18N(1206, "  - started between %s and %s"), /* catgets  1206 */
                localTimeStrStart, localTimeStrEnd );

        printf("%s", printline);
    }
    printf("\n");

    if (complete.begin == eternity.begin && complete.end == eternity.end)
        printf("  - %s", I18N(1208, "completed any time")); /* catgets 1208 */
    else {
        strcpy ( localTimeStrStart, _i18n_ctime ( ls_catd, 1, &complete.begin ));
        strcpy ( localTimeStrEnd,   _i18n_ctime ( ls_catd, 1, &complete.end ));
        sprintf(printline,
                I18N(1209, "  - completed between %s and %s"), /* catgets 1209 */
                localTimeStrStart, localTimeStrEnd);

        printf("%s", printline);
    }
    printf("\n");

    printf(_i18n_msg_get(ls_catd,NL_SETN,1211, "  - with pid(s)")); /* catgets  1211  */
    if (pids[0] == 0)
        printf(" %s", I18N(1212, "all")); /* catgets  1212  */
    else
        for (i = 0; pids[i]; i++)
            printf(" %d", pids[i]);
    printf("\n");

    printf("%s\n\n", dashed_line);
}


static void
initstats(void)
{
    int i;

    actual_start = mkinterval(mktime(&destruction), mktime(&creation));
    actual_complete = actual_start;

    tottasks = totstatz = totstatnz = 0;

    for (i = 0; i < NUM_RESOURCES; i++) {
        statistics[i][min] = DBL_MAX;
        statistics[i][max] = 0.0;
        statistics[i][tot] = 0.0;
        resRecNum[i] = 0;
    }
}


static void
processlogfile(void)
{
    struct stat statBuf;
    FILE *lfp;
    struct lsfAcctRec *acctrec;
    int linenum = 0;

    if (stat(logfile, &statBuf) < 0) {
        perror(logfile);
        exit(-1);
    }

    if ((statBuf.st_mode & S_IFMT) != S_IFREG ) {
        fprintf(stderr, "%s: %s\n",
                logfile,
                _i18n_msg_get(ls_catd,NL_SETN,1213, "Not a regular file")); /* catgets  1213  */
        exit(-1);
    }

    if ((lfp = fopen(logfile, "r")) == NULL) {
        perror("fopen");
        exit(-1);
    }

    for (;;) {
        if ((acctrec = ls_getacctrec(lfp, &linenum)) == NULL) {
            if (lserrno == LSE_EOF)
                break;
            fprintf(stderr, (_i18n_msg_get(ls_catd,NL_SETN,1215, "Warning : %s at line <%d>: %s. Ignored.\n\n")), /* catgets  1215  */
                    logfile, linenum, ls_sysmsg());
            continue;
        }

        if (isinteresting(acctrec)) {
            processacctrec(acctrec);

            if (details)
                printacctrec(acctrec);
        }
    }
}


static void
processacctrec(const struct lsfAcctRec *acctrec)
{
    tottasks++;

    if (acctrec->exitStatus == 0)
        totstatz++;
    else
        totstatnz++;

    processtime(acctrec->dispTime, &actual_start);
    processtime(acctrec->termTime, &actual_complete);

    processresuse(cpu, acctrec->lsfRu.ru_utime + acctrec->lsfRu.ru_stime);
    processresuse(pf, acctrec->lsfRu.ru_majflt);
    processresuse(swaps, acctrec->lsfRu.ru_nswap);
    processresuse(bin, acctrec->lsfRu.ru_inblock);
    processresuse(bout, acctrec->lsfRu.ru_oublock);
    processresuse(msgs, acctrec->lsfRu.ru_msgsnd);
    processresuse(msgr, acctrec->lsfRu.ru_msgrcv);
    processresuse(vcsw, acctrec->lsfRu.ru_nvcsw);
    processresuse(ivcsw, acctrec->lsfRu.ru_nivcsw);
    processresuse(turn, difftime(acctrec->termTime, acctrec->dispTime));
}


static void
printacctrec(const struct lsfAcctRec *acctrec)
{
    char *buf1, *buf2, *buf3, *buf4, *buf5;

    sprintf(printline, "%s", _i18n_ctime( ls_catd, 1, &acctrec->dispTime)) ;

    sprintf(printline + strlen(printline),
            _i18n_msg_get(ls_catd,NL_SETN,1216, ": %s@%s executed pid %d on %s."), /* catgets  1216  */
            acctrec->username, acctrec->fromHost, acctrec->pid, acctrec->execHost);
    printf("%s\n", printline);

    printf("  %s: %s\n",
            _i18n_msg_get(ls_catd,NL_SETN,1217, "Command"), /* catgets 1217 */
            acctrec->cmdln);

    printf("  %s: %s\n",
            _i18n_msg_get(ls_catd,NL_SETN,1218,"CWD"),  /* catgets 1218 */
            acctrec->cwd);

    sprintf(printline, "%s", _i18n_ctime(ls_catd, 1 , &acctrec->termTime));
    sprintf(printline + strlen(printline),
            _i18n_msg_get(ls_catd,NL_SETN,1219, ": Completed, exit status = %d."), /* catgets  1219  */
            acctrec->exitStatus);
    printf("%s\n\n", printline);

    printf("%s:\n\n",
            _i18n_msg_get(ls_catd,NL_SETN,1220, "Accounting information")); /* catgets  1220  */

    buf1 = putstr_(_i18n_msg_get(ls_catd,NL_SETN,1221, "CPU time")); /* catgets  1221  */
    buf2 = putstr_(_i18n_msg_get(ls_catd,NL_SETN,1222, "Page faults")); /* catgets  1222  */
    buf3 = putstr_(_i18n_msg_get(ls_catd,NL_SETN,1223, "Swaps"));  /* catgets  1223  */
    buf4 = putstr_(_i18n_msg_get(ls_catd,NL_SETN,1224, "Blocks in")); /* catgets  1224  */
    buf5 = putstr_(_i18n_msg_get(ls_catd,NL_SETN,1225, "Blocks out")); /* catgets  1225  */

    printf("%16.16s%16.16s%16.16s%16.16s%16.16s\n", buf1, buf2, buf3, buf4, buf5);

    FREEUP(buf1);
    FREEUP(buf2);
    FREEUP(buf3);
    FREEUP(buf4);
    FREEUP(buf5);

    if (acctrec->lsfRu.ru_stime >= 0)
        printf("%16.1f", acctrec->lsfRu.ru_utime + acctrec->lsfRu.ru_stime);
    else
        printf("%16s", "-");

    if (acctrec->lsfRu.ru_majflt >= 0)
        printf("%16.0f", acctrec->lsfRu.ru_majflt);
    else
        printf("%16s", "-");

    if (acctrec->lsfRu.ru_nswap >= 0)
        printf("%16.0f", acctrec->lsfRu.ru_nswap);
    else
        printf("%16s", "-");

    if (acctrec->lsfRu.ru_inblock >= 0)
        printf("%16.0f", acctrec->lsfRu.ru_inblock);
    else
        printf("%16s", "-");

    if (acctrec->lsfRu.ru_oublock >= 0)
        printf("%16.0f\n\n", acctrec->lsfRu.ru_oublock);
    else
        printf("%16s\n\n", "-");

    buf1 = putstr_(_i18n_msg_get(ls_catd,NL_SETN,1226, "Messages sent")); /* catgets  1226  */
    buf2 = putstr_(_i18n_msg_get(ls_catd,NL_SETN,1227, "Messages rcvd")); /* catgets  1227  */
    buf3 = putstr_(_i18n_msg_get(ls_catd,NL_SETN,1228, "Vol cont sw")); /* catgets  1228  */
    buf4 = putstr_(_i18n_msg_get(ls_catd,NL_SETN,1229, "Invol cont sw"));  /* catgets  1229  */
    buf5 = putstr_(_i18n_msg_get(ls_catd,NL_SETN,1230, "Turnaround"));  /* catgets  1230  */


    printf("%16.16s%16.16s%16.16s%16.16s%16.16s\n", buf1, buf2, buf3, buf4, buf5);

    FREEUP(buf1);
    FREEUP(buf2);
    FREEUP(buf3);
    FREEUP(buf4);
    FREEUP(buf5);

    if (acctrec->lsfRu.ru_msgsnd >= 0)
        printf("%16.0f", acctrec->lsfRu.ru_msgsnd);
    else
        printf("%16s", "-");

    if (acctrec->lsfRu.ru_msgrcv >= 0)
        printf("%16.0f", acctrec->lsfRu.ru_msgrcv);
    else
        printf("%16s", "-");

    if (acctrec->lsfRu.ru_nvcsw >= 0)
        printf("%16.0f", acctrec->lsfRu.ru_nvcsw);
    else
        printf("%16s", "-");

    if (acctrec->lsfRu.ru_nivcsw >= 0)
        printf("%16.0f", acctrec->lsfRu.ru_nivcsw);
    else
        printf("%16s", "-");

    printf("%16.1f\n", (double)difftime(acctrec->termTime, acctrec->dispTime));

    printf("%s\n\n", dashed_line);
}


static void
printsummary(void)
{
    char localTimeStrBegin[60];
    char localTimeStrEnd[60];
    char *buf1, *buf2, *buf3, *buf4, *buf5;

    if (tottasks == 0)
        return;

    strcpy( localTimeStrBegin, _i18n_ctime ( ls_catd, 1 , &actual_start.begin ));
    strcpy( localTimeStrEnd, _i18n_ctime ( ls_catd, 1 , &actual_start.end ));

    printf(_i18n_msg_get(ls_catd,NL_SETN,1231, "Summary of %d task(s).  (Exit status zero: %d; exit status non-zero: %d).\n"), /* catgets  1231  */
            tottasks, totstatz, totstatnz);

    sprintf(printline,
            _i18n_msg_get(ls_catd,NL_SETN,1232, "Started between %s and %s"), /* catgets  1232  */
            localTimeStrBegin, localTimeStrEnd );

    printf("%s", printline);


    strcpy( localTimeStrBegin, _i18n_ctime ( ls_catd, 1 , &actual_complete.begin ));
    strcpy( localTimeStrEnd, _i18n_ctime ( ls_catd, 1 , &actual_complete.end ));

    sprintf(printline,
            _i18n_msg_get(ls_catd,NL_SETN,1234, "Completed between %s and %s"),  /* catgets 1234 */
            localTimeStrBegin, localTimeStrEnd );


    printf("%s\n", printline);

    buf1 = putstr_(_i18n_msg_get(ls_catd,NL_SETN,1236, "Resource"));    /* catgets  1236  */
    buf2 = putstr_(_i18n_msg_get(ls_catd,NL_SETN,1237, "Total"));   /* catgets  1237  */
    buf3 = putstr_(_i18n_msg_get(ls_catd,NL_SETN,1238, "Average"));     /* catgets  1238  */
    buf4 = putstr_(_i18n_msg_get(ls_catd,NL_SETN,1239, "Maximum"));     /* catgets  1239  */
    buf5 = putstr_(_i18n_msg_get(ls_catd,NL_SETN,1240, "Minimum"));     /* catgets  1240  */

    printf("  %-30s%12s%12s%12s%12s\n", buf1, buf2, buf3, buf4, buf5);

    FREEUP(buf1);
    FREEUP(buf2);
    FREEUP(buf3);
    FREEUP(buf4);
    FREEUP(buf5);

    printresuse(cpu, _i18n_msg_get(ls_catd,NL_SETN,1241, "CPU time (seconds):")); /* catgets  1241  */
    printresuse(pf, _i18n_msg_get(ls_catd,NL_SETN,1242, "Page faults:")); /* catgets  1242  */
    printresuse(swaps, _i18n_msg_get(ls_catd,NL_SETN,1243, "Swaps:")); /* catgets  1243  */
    printresuse(bin, _i18n_msg_get(ls_catd,NL_SETN,1244, "Block input:")); /* catgets  1244  */
    printresuse(bout, _i18n_msg_get(ls_catd,NL_SETN,1245, "Block output:")); /* catgets  1245  */
    printresuse(msgs, _i18n_msg_get(ls_catd,NL_SETN,1246, "Messages sent:")); /* catgets  1246  */
    printresuse(msgr, _i18n_msg_get(ls_catd,NL_SETN,1247, "Messages received:")); /* catgets  1247  */
    printresuse(vcsw, _i18n_msg_get(ls_catd,NL_SETN,1248, "Voluntary context switches:")); /* catgets  1248  */
    printresuse(ivcsw, _i18n_msg_get(ls_catd,NL_SETN,1249, "Involuntary context switches:")); /* catgets  1249  */
    printresuse(turn, _i18n_msg_get(ls_catd,NL_SETN,1250, "Turnaround time (seconds):")); /* catgets  1250  */
}


static void
printresuse(enum resource res, const char *label)
{
    printf("  %-30s", label);
    if (resRecNum[res] > 0) {
        printf("%12.*f", res == cpu || res == turn, statistics[res][tot]);
        printf("%12.*f", res == cpu || res == turn, statistics[res][tot]/resRecNum[res]);
        printf("%12.*f", res == cpu || res == turn, statistics[res][max]);
        printf("%12.*f", res == cpu || res == turn, statistics[res][min]);
    } else {
        printf("%12s%12s%12s%12s", "-", "-", "-", "-");
    }

    printf("\n");
}


static void
usage(const char *cmd)
{
    fprintf(stderr, "%s:  %s  [-h] [-V] [-l] [-f logfile] [-u userlist | -u all] [-m machinelist] ", I18N_Usage, cmd );
    fprintf(stderr,"[-C 'time0,time1'] [-S 'time0,time1'] [pid ...]\n");
    exit(-1);
}


static struct interval
mkinterval(time_t begin, time_t end)
{
    struct interval i;

    i.begin = begin;
    i.end = end;

    return i;
}


static struct interval
getinterval(char *timeform)
{
    time_t twotimes[2];
    struct interval i;

    if (getBEtime(timeform, 't', twotimes) == -1) {
        ls_perror(timeform);
        exit(-1);
    }

    i.begin = twotimes[0];
    i.end = twotimes[1];

    return i;
}


static void
getarglist(char *argstr, char *arglist[], int n)
{
    int i;

    arglist[0] = strtok(argstr, " ");

    for (i = 1; i < n; i++) {
        arglist[i] = strtok(NULL, " ");
        if (arglist[i] == NULL)
            break;
    }

    if (i == n) {
        fprintf(stderr, "getarglist: %s\n",
                I18N(1254, "too many arguments") /* catgets 1254 */);
        exit(-1);
    }
}


static void
expandhostnames(void)
{
    int i;
    const char *officialName;

    for (i = 0; hosts[i]; i++) {
        if ((officialName = getHostOfficialByName_(hosts[i])) == NULL)
            continue;

        hosts[i] = (char*)malloc(strlen(officialName) + 1);
        if (hosts[i])
            strcpy(hosts[i], officialName);
        else {
            fprintf(stderr, I18N_NO_MEMORY);
            exit(-1);
        }
    }
}


static void
processresuse(enum resource res, double use)
{
    if (use >= 0) {
        resRecNum[res]++;
        statistics[res][tot] += use;

        if (statistics[res][max] < use)
            statistics[res][max] = use;

        if (statistics[res][min] > use)
            statistics[res][min] = use;
    }
}


static void
processtime(time_t time, struct interval *inter)
{
    if (inter->begin > time)
        inter->begin = time;

    if (inter->end < time)
        inter->end = time;
}


static int
isinteresting(const struct lsfAcctRec *acctrec)
{
    return  innamelist(acctrec->username, users) &&
        innamelist(acctrec->execHost, hosts) &&
        ininterval(acctrec->dispTime, start) &&
        ininterval(acctrec->termTime, complete) &&
        innumlist(acctrec->pid, pids);
}


static int
innamelist(const char * name, char *namelist[])
{
    int i;

    if (namelist[0] == NULL)
        return 1;

    for (i = 0; namelist[i]; i++)
        if (strcmp(name, namelist[i]) == 0)
            return 1;

    return 0;
}


static int
innumlist(int num, const int numlist[])
{
    int i;

    if (numlist[0] == 0)
        return 1;

    for (i = 0; numlist[i]; i++)
        if (num == numlist[i])
            return 1;

    return 0;
}


static int
ininterval(time_t time, struct interval inter)
{
    return inter.begin <= time && time <= inter.end;
}


int
main(int argc, char *argv[])
{
    _i18n_init (I18N_CAT_MIN);

    setdefaults();
    getoptions(argc, argv);
    getpids(argc, argv);
    printheader();
    initstats();
    processlogfile();
    printsummary();

    _i18n_end (ls_catd);
    return 0;
}
