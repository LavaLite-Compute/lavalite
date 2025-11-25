/* $Id: cmd.sig.c,v 1.5 2007/08/15 22:18:44 tmizan Exp $
 * Copyright (C) 2007 Platform Computing Inc
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

#include "lsbatch/cmd/cmd.h"

static int signalJobs(int64_t *, int);
static void prtSignaled(int, int64_t);
static int sigValue;
static int64_t sigJobId;
static char *sigUser, *sigQueue, *sigHost, *sigJobName;
static time_t chkPeriod = LSB_CHKPERIOD_NOCHNG;
static int chkOptions;
static int runCount = 0;

static int do_options(int argc, char **argv, int64_t **jobIds,
                      int signalValue);
static void usage(char *cmd, int sig);

static void usage(char *cmd, int sig)
{
    fprintf(stderr, "Usage");
    fprintf(stderr, ": %s [-h] [-V] ", cmd);

    if (sig == SIGKILL)
        fprintf(stderr,
                "[-l] [-m host_name] [-q queue_name]\n               [-u "
                "user_name | -u all] [0] [-r | -s sigval] [-J job_name] ");
    else if (sig == SIGCHK)
        fprintf(stderr,
                "[-m host_name] [-q queue_name] [-u user_name |-u all]\n       "
                "        [-p period] [-f] [-k] [-J job_name]\n               ");
    else if (sig == SIGDEL)
        fprintf(stderr,
                " [-m host_name] [-q queue_name] [-u user_name | -u all]\n     "
                "       [-n running_times] [-d] [-a] [-J job_name] ");
    else if (sig == SIGSTOP)
        fprintf(stderr, " [-m host_name] [-q queue_name]\n               [-u "
                        "user_name | -u all] [0] [-d] [-a] [-J job_name] ");
    else
        fprintf(stderr, " [-m host_name] [-q queue_name]\n               [-u "
                        "user_name | -u all] [0] [-J job_name] ");

    if (lsbMode_ & LSB_MODE_BATCH)
        fprintf(stderr,
                "\n               [jobId | \"jobId[index_list]\"]...\n");
    else
        fprintf(stderr, "\n               [jobId ...]\n");

    exit(-1);
}

static int do_options(int argc, char **argv, int64_t **jobIds,
                      int signalValue)
{
    extern char *optarg;
    extern int optind;

    int cc;

    int kflg = 0;
    int sflg = 0;
    int sigflg = 0;
    int forceflg = 0;
    int newOptions = 0;

    sigJobId = 0;
    sigUser = NULL;
    sigQueue = NULL;
    sigHost = NULL;
    sigJobName = NULL;
    chkOptions = 0;

    sigValue = signalValue;

    while ((cc = getopt(argc, argv, "VhkSlq:u:m:p:rfs:J:n:ad")) != EOF) {
        switch (cc) {
        case 'u':
            if (sigUser)
                usage(argv[0], signalValue);
            if (strcmp(optarg, "all") == 0)
                sigUser = ALL_USERS;
            else
                sigUser = optarg;
            break;
        case 'q':
            sigQueue = optarg;
            break;
        case 'm':
            sigHost = optarg;
            break;
        case 'l':

            if (signalValue != SIGKILL) {
                fprintf(stderr, "%s: %s %s\n", argv[0], "Illegal option --",
                        "l");
                usage(argv[0], signalValue);
            }
            puts(getSigSymbolList());
            exit(-1);
        case 's':
            if (signalValue != SIGKILL) {
                fprintf(stderr, "%s: %s %s\n", argv[0], "Illegal option --",
                        "s");
                usage(argv[0], signalValue);
            }
            if (forceflg) {
                usage(argv[0], signalValue);
                exit(-1);
            } else {
                if ((sigValue = getSigVal(optarg)) < 0) {
                    fprintf(stderr, ("%s: Illegal signal value\n"), optarg);
                    exit(-1);
                }
                sigflg = TRUE;
            }
            break;
        case 'a':
            if (signalValue != SIGDEL && signalValue != SIGSTOP) {
                fprintf(stderr, "%s: %s %s\n", argv[0], "Illegal option --",
                        "a");
                usage(argv[0], signalValue);
            }
            newOptions |= ALL_JOB;
            break;
        case 'd':
            if (signalValue != SIGDEL && signalValue != SIGSTOP) {
                fprintf(stderr, "%s: %s %s\n", argv[0], "Illegal option --",
                        "d");
                usage(argv[0], signalValue);
            }
            newOptions |= DONE_JOB;
            break;
        case 'n':
            if (signalValue != SIGDEL) {
                fprintf(stderr, "%s: %s %s\n", argv[0], "Illegal option --",
                        "n");
                usage(argv[0], signalValue);
            }
            if (isint_(optarg) && ((runCount = atoi(optarg)) >= 0))
                break;
            fprintf(stderr, ("%s: Illegal times value\n"), optarg);
            exit(-1);

        case 'J':
            if (supportJobNamePattern(optarg) != 0) {
                fprintf(stderr, ("%s: Job or Job Group name is not valid.\n"),
                        optarg);
                exit(-1);
            }

            sigJobName = optarg;
            break;

        case 'f':
            if (signalValue == SIGCHK) {
                chkOptions |= LSB_CHKPNT_FORCE;
            } else {
                usage(argv[0], signalValue);
                exit(-1);
            }
            break;

        case 'r':
            if ((signalValue == SIGKILL) && (!sigflg)) {
                sigValue = SIGFORCE;
                newOptions |= ZOMBIE_JOB;
                forceflg = TRUE;
            } else {
                usage(argv[0], signalValue);
                exit(-1);
            }
            break;

        case 'p':
            if (signalValue != SIGCHK) {
                usage(argv[0], signalValue);
                exit(-1);
            }

            if (isint_(optarg) && ((chkPeriod = atoi(optarg) * 60) >= 0))
                break;

            fprintf(stderr, ("%s: Illegal checkpoint period value\n"), optarg);
            exit(-1);

        case 'k':
            if (sflg) {
                usage(argv[0], signalValue);
                exit(-1);
            } else {
                kflg++;
                if (signalValue != SIGCHK) {
                    usage(argv[0], signalValue);
                    exit(-1);
                }
                chkOptions |= LSB_CHKPNT_KILL;
            }
            break;
        case 'S':
            if (kflg) {
                usage(argv[0], signalValue);
                exit(-1);
            } else {
                sflg++;
                if (signalValue != SIGCHK) {
                    usage(argv[0], signalValue);
                    exit(-1);
                }
                chkOptions |= LSB_CHKPNT_STOP;
            }
            break;

        case 'V':
            fprintf(stderr, "%s\n", LAVALITE_VERSION_STR);
            exit(0);
        case 'h':
        default:
            usage(argv[0], signalValue);
        }
    }

    if ((argc == optind) && (sigUser == NULL) && (sigQueue == NULL) &&
        (sigHost == NULL) && (sigJobName == NULL)) {
        fprintf(
            stderr, "%s.\n",
            ("Job ID or one of '-m', '-u' '-q' and '-J' must be specified"));
        usage(argv[0], signalValue);
    }

    return (getJobIds(argc, argv, sigJobName, sigUser, sigQueue, sigHost,
                      jobIds, newOptions));
}

int bsignal(int argc, char **argv)
{
    int numJobs;
    int64_t *jobIds;
    int signalValue;

    if (lsb_init(argv[0]) < 0) {
        lsb_perror("lsb_init");
        exit(-1);
    }

    if (strstr(argv[0], "bkill"))
        signalValue = SIGKILL;
    else if (strstr(argv[0], "bstop"))
        signalValue = SIGSTOP;
    else if (strstr(argv[0], "bresume"))
        signalValue = SIGCONT;
    else if (strstr(argv[0], "bchkpnt"))
        signalValue = SIGCHK;
    else if (strstr(argv[0], "bdel"))
        signalValue = SIGDEL;
    else {
        fprintf(stderr, ("%s: Illegal operation\n"), argv[0]);
        exit(-1);
    }
    numJobs = do_options(argc, argv, &jobIds, signalValue);

    if ((signalValue == SIGKILL) && (sigValue != SIGFORCE) &&
        (sigValue >= LSF_NSIG || sigValue < 1)) {
        fprintf(stderr, ("%d: Illegal signal value\n"), sigValue);
        exit(-1);
    }

    return (signalJobs(jobIds, numJobs));
}

static int signalJobs(int64_t *jobIds, int numJobs)
{
    int failsignal = FALSE, signaled = FALSE;
    int i, cc;
    char msg[80];

    for (i = 0; i < numJobs; i++) {
        if (sigValue == SIGCHK)
            cc = lsb_chkpntjob(jobIds[i], chkPeriod, chkOptions);
        else if (sigValue == SIGDEL)
            cc = lsb_deletejob(jobIds[i], runCount, 0);
        else if (sigValue == SIGFORCE)
            cc = lsb_forcekilljob(jobIds[i]);
        else
            cc = lsb_signaljob(jobIds[i], sigValue);

        if (cc < 0) {
            if (sigValue == SIGCHK && lsberrno == LSBE_NOT_STARTED &&
                chkPeriod != LSB_CHKPERIOD_NOCHNG) {
                if (chkPeriod)
                    printf(("Job <%s>: Checkpoint period is now %d min.\n"),
                           lsb_jobid2str(jobIds[i]), (int) (chkPeriod / 60));
                else
                    printf(("Job <%s>: Periodic checkpointing is disabled\n"),
                           lsb_jobid2str(jobIds[i]));
                signaled = TRUE;
            } else {
                failsignal = TRUE;
                sprintf(msg, "%s <%s>", "Job", lsb_jobid2str(jobIds[i]));
                lsb_perror(msg);
            }
        } else {
            signaled = TRUE;
            prtSignaled(sigValue, jobIds[i]);
        }
    }

    return (signaled ? !failsignal : FALSE);
}

static void prtSignaled(int signalValue, int64_t jobId)
{
    char *op;

    switch (signalValue) {
    case SIGCHK:
        op = ("checkpointed");
        break;
    case SIGSTOP:
        op = ("stopped");
        break;
    case SIGCONT:
        op = ("resumed");
        break;
    case SIGKILL:
    case SIGFORCE:
        op = ("terminated");
        break;
    case SIGDEL:
        op = ("deleted");
        break;
    default:
        op = ("signaled");
        break;
    }

    if (signalValue == SIGDEL && runCount != 0)
        printf(("Job <%s> will be deleted after running next %d times\n"),
               lsb_jobid2str(jobId), runCount);
    else
        printf(("Job <%s> is being %s\n"), lsb_jobid2str(jobId), op);
}
