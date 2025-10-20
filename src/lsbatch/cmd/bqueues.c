/* $Id: bqueues.c,v 1.8 2007/08/15 22:18:43 tmizan Exp $
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 *
 */

#include "lsbatch/cmd/cmd.h"

void load2Str();
static void prtQueuesLong (int, struct queueInfoEnt *);
static void prtQueuesShort (int, struct queueInfoEnt *);
static char wflag = FALSE;
extern int terminateWhen_(int *, char *);

#define QUEUE_NAME_LENGTH    15
#define QUEUE_PRIO_LENGTH    4
#define QUEUE_STATUS_LENGTH  14
#define QUEUE_JL_U_LENGTH    4
#define QUEUE_JL_P_LENGTH    4
#define QUEUE_JL_H_LENGTH    4
#define QUEUE_MAX_LENGTH     4
#define QUEUE_NJOBS_LENGTH   5
#define QUEUE_PEND_LENGTH    5
#define QUEUE_RUN_LENGTH     5
#define QUEUE_SUSP_LENGTH    5
#define QUEUE_SSUSP_LENGTH   5
#define QUEUE_USUSP_LENGTH   5
#define QUEUE_NICE_LENGTH    4
#define QUEUE_RSV_LENGTH     4

static char fomt[200];

void
usage (char *cmd)
{
    fprintf(stderr, I18N_Usage);
    fprintf(stderr, ": %s [-h] [-V] [-w | -l] [-m host_name | -m cluster_name]\n", cmd);

    if (lsbMode_ & LSB_MODE_BATCH)
	fprintf(stderr, " [-u user_name]");
    fprintf(stderr, " [queue_name ...]\n");
    exit(-1);
}

int
main (int argc, char **argv)
{
    extern int optind;
    extern char *optarg;
    int numQueues;
    char **queueNames=NULL, **queues = NULL;
    struct queueInfoEnt *queueInfo;
    char lflag = FALSE;
    int cc, defaultQ = FALSE;
    char *host = NULL, *user = NULL;
    int rc;

    numQueues = 0;

    rc = 0;

    if (lsb_init(argv[0]) < 0) {
	lsb_perror("lsb_init");
	exit(-1);
    }

    while ((cc = getopt(argc, argv, "Vhlwm:u:")) != EOF) {
        switch (cc) {
        case 'l':
            lflag = TRUE;
            if (wflag)
                usage(argv[0]);
            break;
        case 'w':
            wflag = TRUE;
            if (lflag)
                usage(argv[0]);
            break;
        case 'm':
            if (host != NULL || *optarg == '\0')
                usage(argv[0]);
            host = optarg;
            break;
        case 'u':
            if (user != NULL || *optarg == '\0')
                usage(argv[0]);
            user = optarg;
            break;
	case 'V':
	    fputs(_LAVALITE_VERSION_, stderr);
	    exit(0);
        case 'h':
        default:
            usage(argv[0]);
        }
    }

    numQueues = getNames (argc, argv, optind, &queueNames,  &defaultQ, "queue");
    if (!defaultQ && numQueues != 0)
        queues = queueNames;
    else
        queues = NULL;

    TIMEIT(0, (queueInfo = lsb_queueinfo(queues,
					 &numQueues,
					 host,
					 user,
					 0)), "lsb_queueinfo");

    if (!queueInfo) {
        if (lsberrno == LSBE_BAD_QUEUE && queues)
	    lsb_perror(queues[numQueues]);
        else {
            switch (lsberrno) {
            case LSBE_BAD_HOST   :
            case LSBE_QUEUE_HOST :
                lsb_perror (host);
                break;
            case LSBE_BAD_USER   :
            case LSBE_QUEUE_USE  :
                lsb_perror (user);
                break;
            default :
                lsb_perror (NULL);
            }
         }
         exit(-1);
    }
    if (lflag )
        prtQueuesLong(numQueues, queueInfo);
    else
        prtQueuesShort(numQueues, queueInfo);
    exit(0);

}

static void
prtQueuesLong(int numQueues, struct queueInfoEnt *queueInfo)
{
    struct queueInfoEnt*  qp;
    char                  statusStr[64];
    char                  userJobLimit[MAX_CHARLEN];
    char                  procJobLimit[MAX_CHARLEN];
    char                  hostJobLimit[MAX_CHARLEN];
    char                  maxJobs[MAX_CHARLEN];
    int                   i;
    int                   numDefaults = 0;
    struct lsInfo*        lsInfo;
    int                   printFlag = 0;
    int                   printFlag1 = 0;
    int                   printFlag2 = 0;
    int			  procLimits[3];

    if ((lsInfo = ls_info()) == NULL) {
	ls_perror("ls_info");
	exit(-1);
    }

    printf("\n");

    for ( i=0; i<numQueues; i++ )  {
	 qp = &(queueInfo[i]);
	 if (qp->qAttrib & Q_ATTRIB_DEFAULT)
	     numDefaults++;
    }

    for ( i=0; i<numQueues; i++ ) {

        qp = &(queueInfo[i]);

        if (qp->qStatus & QUEUE_STAT_OPEN) {
            sprintf(statusStr, "%s:", I18N_Open);
	}
	else {
            sprintf(statusStr, "%s:", I18N_Closed);
	}
        if (qp->qStatus & QUEUE_STAT_ACTIVE) {
            if (qp->qStatus & QUEUE_STAT_RUN) {
	        strcat(statusStr, I18N_Active);
	    } else {
	        strcat(statusStr, I18N_Inact__Win);
	    }
	} else {
	    strcat(statusStr, I18N_Inact__Adm);
	}

	if (qp->maxJobs < INFINIT_INT)
            strcpy(maxJobs, prtValue(QUEUE_MAX_LENGTH, qp->maxJobs) );
        else
            strcpy(maxJobs, prtDash(QUEUE_MAX_LENGTH) );

	if (qp->userJobLimit < INFINIT_INT)
            strcpy(userJobLimit,
                   prtValue(QUEUE_JL_U_LENGTH, qp->userJobLimit) );
        else
            strcpy(userJobLimit, prtDash(QUEUE_JL_U_LENGTH) );

        if (qp->procJobLimit < INFINIT_FLOAT) {
            sprintf(fomt, "%%%d.1f ", QUEUE_JL_P_LENGTH);
            sprintf (procJobLimit, fomt, qp->procJobLimit);
	}
        else
            strcpy(procJobLimit, prtDash(QUEUE_JL_P_LENGTH) );

        if (qp->hostJobLimit < INFINIT_INT)
            strcpy(hostJobLimit,
                   prtValue(QUEUE_JL_H_LENGTH, qp->hostJobLimit) );
        else
            strcpy(hostJobLimit, prtDash(QUEUE_JL_H_LENGTH) );

        if (i > 0)
            printf("-------------------------------------------------------------------------------\n\n");
        printf("%s: %s\n", "QUEUE", qp->queue);

        printf("  -- %s", qp->description);
	if (qp->qAttrib & Q_ATTRIB_DEFAULT) {
            if (numDefaults == 1)
	       printf("  %s.\n\n", "This is the default queue");
            else
	       printf("  %s.\n\n", "This is one of the default queues");
        } else
	    printf("\n\n");

        printf(("PARAMETERS/STATISTICS\n"));

        prtWord(QUEUE_PRIO_LENGTH, I18N_PRIO, 0);

	if ( lsbMode_ & LSB_MODE_BATCH )
            prtWord(QUEUE_NICE_LENGTH, I18N_NICE, 1);

        prtWord(QUEUE_STATUS_LENGTH, I18N_STATUS, 0);

	if ( lsbMode_ & LSB_MODE_BATCH ) {
            prtWord(QUEUE_MAX_LENGTH,  I18N_MAX, -1);
            prtWord(QUEUE_JL_U_LENGTH, I18N_JL_U, -1);
            prtWord(QUEUE_JL_P_LENGTH, I18N_JL_P, -1);
            prtWord(QUEUE_JL_H_LENGTH, I18N_JL_H, -1);
	};

        prtWord(QUEUE_NJOBS_LENGTH, I18N_NJOBS, -1);
        prtWord(QUEUE_PEND_LENGTH,  I18N_PEND,  -1);
        prtWord(QUEUE_RUN_LENGTH,   I18N_RUN,   -1);
        prtWord(QUEUE_SSUSP_LENGTH, I18N_SSUSP, -1);
        prtWord(QUEUE_USUSP_LENGTH, I18N_USUSP, -1);
        prtWord(QUEUE_RSV_LENGTH,   I18N_RSV,   -1);
        printf("\n");

        prtWordL(QUEUE_PRIO_LENGTH,
                 prtValue(QUEUE_PRIO_LENGTH-1, qp->priority));

	if ( lsbMode_ & LSB_MODE_BATCH )
            prtWordL(QUEUE_NICE_LENGTH,
                     prtValue(QUEUE_NICE_LENGTH-1, qp->nice));

        prtWord(QUEUE_STATUS_LENGTH, statusStr, 0);

	if ( lsbMode_ & LSB_MODE_BATCH ) {
            sprintf(fomt, "%%%ds%%%ds%%%ds%%%ds", QUEUE_MAX_LENGTH,
                                                  QUEUE_JL_U_LENGTH,
                                                  QUEUE_JL_P_LENGTH,
                                                  QUEUE_JL_H_LENGTH );
	    printf(fomt,
                   maxJobs, userJobLimit, procJobLimit, hostJobLimit);
	};

        sprintf(fomt, "%%%dd %%%dd %%%dd %%%dd %%%dd %%%dd\n",
                                                  QUEUE_NJOBS_LENGTH,
                                                  QUEUE_PEND_LENGTH,
                                                  QUEUE_RUN_LENGTH,
                                                  QUEUE_SSUSP_LENGTH,
                                                  QUEUE_USUSP_LENGTH,
                                                  QUEUE_RSV_LENGTH );
	printf(fomt,
                qp->numJobs, qp->numPEND, qp->numRUN,
                qp->numSSUSP, qp->numUSUSP, qp->numRESERVE);

	if ( qp->mig < INFINIT_INT )
       	    printf(("Migration threshold is %d minutes\n"), qp->mig);

        if ( qp->schedDelay < INFINIT_INT )
	    printf(("Schedule delay for a new job is %d seconds\n"),
		    qp->schedDelay);

        if ( qp->acceptIntvl < INFINIT_INT )
	    printf(("Interval for a host to accept two jobs is %d seconds\n"),
		    qp->acceptIntvl);

        if (((qp->defLimits[LSF_RLIMIT_CPU] != INFINIT_INT) &&
             (qp->defLimits[LSF_RLIMIT_CPU] > 0 )) ||
            ((qp->defLimits[LSF_RLIMIT_RUN] != INFINIT_INT) &&
             (qp->defLimits[LSF_RLIMIT_RUN] > 0 )) ||
            ((qp->defLimits[LSF_RLIMIT_DATA] != INFINIT_INT) &&
             (qp->defLimits[LSF_RLIMIT_DATA] > 0 )) ||
            ((qp->defLimits[LSF_RLIMIT_RSS] != INFINIT_INT) &&
             (qp->defLimits[LSF_RLIMIT_RSS] > 0 )) ||
            ((qp->defLimits[LSF_RLIMIT_PROCESS] != INFINIT_INT) &&
             (qp->defLimits[LSF_RLIMIT_PROCESS] > 0 ))) {

	     printf("\n");
	     printf("DEFAULT LIMITS:"  );
	     prtResourceLimit (qp->defLimits, qp->hostSpec, 1.0, 0);
	     printf("\n");
	     printf("MAXIMUM LIMITS:"  );
	}

	procLimits[0] = qp->minProcLimit;
	procLimits[1] = qp->defProcLimit;
	procLimits[2] = qp->procLimit;
        prtResourceLimit (qp->rLimits, qp->hostSpec, 1.0, procLimits);

	if ( lsbMode_ & LSB_MODE_BATCH ) {
	    printf(("\nSCHEDULING PARAMETERS\n"));

 	    if (printThresholds (qp->loadSched,  qp->loadStop, NULL, NULL,
				 MIN(lsInfo->numIndx, qp->nIdx), lsInfo) < 0)
		exit (-1);
	}

        if ((qp->qAttrib & Q_ATTRIB_EXCLUSIVE)
            || (qp->qAttrib & Q_ATTRIB_BACKFILL)
            || (qp->qAttrib & Q_ATTRIB_IGNORE_DEADLINE)
            || (qp->qAttrib & Q_ATTRIB_ONLY_INTERACTIVE)
	    || (qp->qAttrib & Q_ATTRIB_NO_INTERACTIVE)) {
            printf("\n%s:", "SCHEDULING POLICIES");
            if (qp->qAttrib & Q_ATTRIB_BACKFILL)
                printf("  %s", ("BACKFILL"));
            if (qp->qAttrib & Q_ATTRIB_IGNORE_DEADLINE)
                printf("  %s", ("IGNORE_DEADLINE"));
            if (qp->qAttrib & Q_ATTRIB_EXCLUSIVE)
                printf("  %s", ("EXCLUSIVE"));
            if (qp->qAttrib & Q_ATTRIB_NO_INTERACTIVE)
                printf("  %s", ("NO_INTERACTIVE"));
            if (qp->qAttrib & Q_ATTRIB_ONLY_INTERACTIVE)
                printf("  %s", ("ONLY_INTERACTIVE"));
	    printf("\n");
        }

        if (strcmp (qp->defaultHostSpec, " ") !=  0)
            printf("\n%s: %s\n",
		("DEFAULT HOST SPECIFICATION"), qp->defaultHostSpec);

        if (qp->windows && strcmp (qp->windows, " " ) !=0)
	    printf("\n%s: %s\n",
		("RUN_WINDOW"), qp->windows);
        if (strcmp (qp->windowsD, " ")  !=  0)
	    printf("\n%s: %s\n",
		("DISPATCH_WINDOW"), qp->windowsD);

	if (lsbMode_ & LSB_MODE_BATCH) {
	    if ( strcmp(qp->userList, " ") == 0) {
		printf("\n%s:  %s\n", I18N_USERS,
		       "all users");
	    } else {
		if (strcmp(qp->userList, " ") != 0 && qp->userList[0] != 0)
		    printf("\n%s: %s\n", I18N_USERS, qp->userList);
	    }
	}

	if (strcmp(qp->hostList, " ") == 0) {
	    if (lsbMode_ & LSB_MODE_BATCH)
		printf("%s\n",
			("HOSTS:  all hosts used by the LSF Batch system"));
	    else
		printf("%s\n",
			("HOSTS: all hosts used by the LSF JobScheduler system"));
        } else {
	    if (strcmp(qp->hostList, " ") != 0 && qp->hostList[0])
	        printf("%s:  %s\n", I18N_HOSTS, qp->hostList);
        }
        if (strcmp (qp->admins, " ") != 0)
            printf("%s:  %s\n",
		("ADMINISTRATORS"), qp->admins);
        if (strcmp (qp->preCmd, " ") != 0)
            printf("%s:  %s\n",
		("PRE_EXEC"), qp->preCmd);
        if (strcmp (qp->postCmd, " ") != 0)
            printf("%s:  %s\n",
		("POST_EXEC"), qp->postCmd);
        if (strcmp (qp->requeueEValues, " ") != 0)
            printf("%s:  %s\n",
		("REQUEUE_EXIT_VALUES"), qp->requeueEValues);
        if (strcmp (qp->resReq, " ") != 0)
            printf("%s:  %s\n",
		("RES_REQ"), qp->resReq);
        if (qp->slotHoldTime > 0)
	    printf(("Maximum slot reservation time: %d seconds\n"), qp->slotHoldTime);
        if (strcmp (qp->resumeCond, " ") != 0)
	    printf("%s:  %s\n",
		("RESUME_COND"), qp->resumeCond);
        if (strcmp (qp->stopCond, " ") != 0)
	    printf("%s:  %s\n",
		("STOP_COND"), qp->stopCond);
        if (strcmp (qp->jobStarter, " ") != 0)
	    printf("%s:  %s\n",
		("JOB_STARTER"), qp->jobStarter);
	if ( qp->qAttrib & Q_ATTRIB_RERUNNABLE )
	   printf(("RERUNNABLE :  yes\n"));

	if ( qp->qAttrib & Q_ATTRIB_CHKPNT ) {
	   printf(("CHKPNTDIR : %s\n"), qp->chkpntDir);
	   printf(("CHKPNTPERIOD : %d\n"), qp->chkpntPeriod);
	}

        printf("\n");
        printFlag = 0;
        if  ((qp->suspendActCmd != NULL)
            && (qp->suspendActCmd[0] != ' '))
                printFlag = 1;

        printFlag1 = 0;
        if  ((qp->resumeActCmd != NULL)
            && (qp->resumeActCmd[0] != ' '))
            printFlag1 = 1;

        printFlag2 = 0;
        if  ((qp->terminateActCmd != NULL)
            && (qp->terminateActCmd[0] != ' '))
            printFlag2 = 1;

        if (printFlag || printFlag1 || printFlag2)
            printf("%s:\n",
		("JOB_CONTROLS"));

        if (printFlag) {
            printf("    %-9.9s", ("SUSPEND:"));
            if (strcmp (qp->suspendActCmd, " ") != 0)
                printf("    [%s]\n", qp->suspendActCmd);
        }

        if (printFlag1) {
            printf("    %-9.9s", ("RESUME:"));
            if (strcmp (qp->resumeActCmd, " ") != 0)
                printf("    [%s]\n", qp->resumeActCmd);
        }

        if (printFlag2) {
            printf("    %-9.9s", ("TERMINATE:"));
            if (strcmp (qp->terminateActCmd, " ") != 0)
                printf("    [%s]\n", qp->terminateActCmd);
        }

        if (printFlag || printFlag1 || printFlag2)
            printf("\n");

        printFlag = terminateWhen_(qp->sigMap, "USER");
        printFlag1 = terminateWhen_(qp->sigMap, "WINDOW");
        printFlag2 = terminateWhen_(qp->sigMap, "LOAD");

        if (printFlag | printFlag1 | printFlag2) {
            printf(("TERMINATE_WHEN = "));
            if (printFlag) printf(("USER "));
            if (printFlag1) printf(("WINDOW "));
            if (printFlag2) printf(("LOAD"));
	    printf("\n");
        };

    };

    printf("\n");

}
static void
prtQueuesShort(int numQueues, struct queueInfoEnt *queueInfo)
{
    struct queueInfoEnt *qp;
    char statusStr[64];
    char first = FALSE;
    int i;
    char userJobLimit[MAX_CHARLEN],
         procJobLimit[MAX_CHARLEN],
         hostJobLimit[MAX_CHARLEN];
    char maxJobs[MAX_CHARLEN];

    if( !first ) {
            prtWord(QUEUE_NAME_LENGTH, I18N_QUEUE__NAME, 0);
            prtWord(QUEUE_PRIO_LENGTH, I18N_PRIO, 1);
            prtWord(QUEUE_STATUS_LENGTH, I18N_STATUS, 0);

	    if ( lsbMode_ & LSB_MODE_BATCH ) {
                prtWord(QUEUE_MAX_LENGTH,  I18N_MAX,  -1);
                prtWord(QUEUE_JL_U_LENGTH, I18N_JL_U, -1);
                prtWord(QUEUE_JL_P_LENGTH, I18N_JL_P, -1);
                prtWord(QUEUE_JL_H_LENGTH, I18N_JL_H, -1);
	    };

        prtWord(QUEUE_NJOBS_LENGTH, I18N_NJOBS, -1);
        prtWord(QUEUE_PEND_LENGTH,  I18N_PEND,  -1);
        prtWord(QUEUE_RUN_LENGTH,   I18N_RUN,   -1);
        prtWord(QUEUE_SUSP_LENGTH,  I18N_SUSP,  -1);
        printf("\n");
        first = TRUE;
    }

    for ( i=0; i<numQueues; i++) {

        qp = &(queueInfo[i]);

        if (qp->qStatus & QUEUE_STAT_OPEN) {
            sprintf(statusStr, "%s:", I18N_Open);
	}
	else {
            sprintf(statusStr, "%s:", I18N_Closed);
	}
        if (qp->qStatus & QUEUE_STAT_ACTIVE) {
            if (qp->qStatus & QUEUE_STAT_RUN) {
	        strcat(statusStr, I18N_Active);
	    } else {
	        strcat(statusStr, I18N_Inact);
	    }
	} else {
	    strcat(statusStr, I18N_Inact);
	}

	if (qp->maxJobs < INFINIT_INT)
            strcpy(maxJobs, prtValue(QUEUE_MAX_LENGTH, qp->maxJobs) );
        else
            strcpy(maxJobs, prtDash(QUEUE_MAX_LENGTH) );

	if (qp->userJobLimit < INFINIT_INT)
            strcpy(userJobLimit,
                   prtValue(QUEUE_JL_U_LENGTH, qp->userJobLimit) );
        else
            strcpy(userJobLimit, prtDash(QUEUE_JL_U_LENGTH) );

        if (qp->procJobLimit < INFINIT_FLOAT) {
            sprintf(fomt, "%%%d.0f ", QUEUE_JL_P_LENGTH);
            sprintf (procJobLimit, fomt, qp->procJobLimit);
	}
        else
            strcpy(procJobLimit, prtDash(QUEUE_JL_P_LENGTH) );

        if (qp->hostJobLimit < INFINIT_INT)
            strcpy(hostJobLimit,
                   prtValue(QUEUE_JL_H_LENGTH, qp->hostJobLimit) );
        else
            strcpy(hostJobLimit, prtDash(QUEUE_JL_H_LENGTH) );

        if ( wflag ) {
            prtWordL(QUEUE_NAME_LENGTH, qp->queue);
            prtWordL(QUEUE_PRIO_LENGTH,
                   prtValue(3, qp->priority));
            prtWordL(QUEUE_STATUS_LENGTH, statusStr);
	} else {
            prtWord(QUEUE_NAME_LENGTH, qp->queue, 0);
            prtWord(QUEUE_PRIO_LENGTH,
                   prtValue(3, qp->priority), 1);
            prtWord(QUEUE_STATUS_LENGTH, statusStr, 1);
	};

	if ( lsbMode_ & LSB_MODE_BATCH ) {
            sprintf(fomt, "%%%ds%%%ds%%%ds%%%ds", QUEUE_MAX_LENGTH,
                                                  QUEUE_JL_U_LENGTH,
                                                  QUEUE_JL_P_LENGTH,
                                                  QUEUE_JL_H_LENGTH );
	    printf(fomt,
                   maxJobs, userJobLimit, procJobLimit, hostJobLimit);
	};

        sprintf(fomt, "%%%dd %%%dd %%%dd %%%dd\n", QUEUE_NJOBS_LENGTH,
                                                   QUEUE_PEND_LENGTH,
                                                   QUEUE_RUN_LENGTH,
                                                   QUEUE_SUSP_LENGTH );
	printf(fomt,
                qp->numJobs, qp->numPEND, qp->numRUN,
                (qp->numSSUSP + qp->numUSUSP) );

    };

}

