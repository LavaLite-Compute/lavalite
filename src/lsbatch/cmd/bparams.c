/* $Id: bparams.c,v 1.10 2007/08/15 22:18:43 tmizan Exp $
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

static void printLong (struct parameterInfo *);
static void printShort (struct parameterInfo *);
void
usage (char *cmd)
{
    fprintf(stderr, "Usage");
    fprintf(stderr, ": %s [-h] [-V] [-l]\n", cmd);
    exit(-1);
}

int
main (int argc, char **argv)
{
    int cc;
    struct parameterInfo  *paramInfo;
    int longFormat;

    if (lsb_init(argv[0]) < 0) {
	lsb_perror("lsb_init");
	exit(-1);
    }

    longFormat = FALSE;

    while ((cc = getopt(argc, argv, "Vhl")) != EOF) {
        switch (cc) {
        case 'l':
            longFormat = TRUE;
            break;
	case 'V':
	    fprintf(stderr, "%s\n", LAVALITE_VERSION_STR);
	    exit(0);
        case 'h':
        default:
            usage(argv[0]);
        }
    }
    if (!(paramInfo = lsb_parameterinfo (NULL, NULL, 0))) {
	lsb_perror(NULL);
        exit (-1);
    }
    if (longFormat)
        printLong (paramInfo);
    else
	printShort (paramInfo);
    exit(0);

}

static void
printShort (struct parameterInfo *reply)
{
    printf ("%s:  %s\n",
	("Default Queues"), reply->defaultQueues);
    if (reply->defaultHostSpec[0] != '\0')
	printf ("%s:  %s\n",
	    "Default Host Specification",
	    reply->defaultHostSpec);
    printf (("Job Dispatch Interval:  %d seconds\n"), reply->mbatchdInterval);
    printf (("Job Checking Interval:  %d seconds\n"), reply->sbatchdInterval);
    printf (("Job Accepting Interval:  %d seconds\n"),
                         reply->jobAcceptInterval * reply->mbatchdInterval);
}

static void
printLong (struct parameterInfo *reply)
{

    printf ("\n%s:\n",
	("System default queues for automatic queue selection"));
    printf (" %16.16s = %s\n\n",  "DEFAULT_QUEUE", reply->defaultQueues);

    if (reply->defaultHostSpec[0] != '\0') {
	printf ("%s:\n",
	    ("System default host or host model for adjusting CPU time limit"));
	printf (" %20.20s = %s\n\n",  "DEFAULT_HOST_SPEC",
		reply->defaultHostSpec);
    }

    printf ("%s:\n",
	("The interval for dispatching jobs by master batch daemon"));
    printf ("    MBD_SLEEP_TIME = %d (%s)\n\n", reply->mbatchdInterval,
            "seconds");

    printf ("%s:\n",
	("The interval for checking jobs by slave batch daemon"));
    printf ("    SBD_SLEEP_TIME = %d (%s)\n\n", reply->sbatchdInterval,
	    "seconds");

    printf ("%s:\n",
	("The interval for a host to accept two batch jobs"));
    printf ("    JOB_ACCEPT_INTERVAL = %d (* MBD_SLEEP_TIME)\n\n",
            reply->jobAcceptInterval);

    if (lsbMode_ & LSB_MODE_BATCH) {
	printf ("%s:\n",
	    ("The idle time of a host for resuming pg suspended jobs"));
	printf ("    PG_SUSP_IT = %d (%s)\n\n", reply->pgSuspendIt,
                "seconds");
    }

    printf ("%s:\n",
	("The amount of time during which finished jobs are kept in core"));
    printf ("    CLEAN_PERIOD = %d (%s)\n\n", reply->cleanPeriod,
            "seconds");

    printf ("%s:\n",
	("The maximum number of finished jobs that can be stored in current events file"));
    printf ("    MAX_JOB_NUM = %d\n\n", reply->maxNumJobs);

    printf ("%s:\n",
	("The maximum number of retries for reaching a slave batch daemon"));
    printf ("    MAX_SBD_FAIL = %d\n\n", reply->maxSbdRetries);

    if (lsbMode_ & LSB_MODE_BATCH) {
	char *temp = NULL;

	printf ("%s.\n",
	    ("The default project assigned to jobs"));
	printf ("    %15s = %s\n\n", "DEFAULT_PROJECT", reply->defaultProject);

	printf("%s.\n",
	    ("The interval to terminate a job"));
	printf ("    JOB_TERMINATE_INTERVAL = %d \n\n",
		reply->jobTerminateInterval);

	printf("%s.\n",
	    ("The maximum number of jobs in a job array"));
	printf ("    MAX_JOB_ARRAY_SIZE = %d\n\n", reply->maxJobArraySize);

	if (reply->disableUAcctMap == TRUE)
	    temp = putstr_("disabled");
	else
	    temp = putstr_("permitted");

	printf("%s %s.\n\n",
	    "User level account mapping for remote jobs is",
	    temp);

        FREEUP(temp);

    }

    if (strlen(reply->pjobSpoolDir) > 0) {
	printf ("\n%s:\n",
	       ("The batch jobs' temporary output directory"));
		printf ("    JOB_SPOOL_DIR = %s\n\n", reply->pjobSpoolDir);
    }

    if ( reply->maxUserPriority > 0 ) {
        printf("%s \n", "Maximal job priority defined for all users:");
        printf("    MAX_USER_PRIORITY = %d\n", reply->maxUserPriority);
	printf("%s: %d\n\n",
  	     "The default job priority is",
	     reply->maxUserPriority/2);
    }

    if ( reply->jobPriorityValue > 0) {
	printf("%s.\n", "Job priority is increased by the system dynamically based on waiting time");
	printf("    JOB_PRIORITY_OVER_TIME = %d/%d (%s)\n\n",
               reply->jobPriorityValue, reply->jobPriorityTime, "minutes");
    }

    if (reply->sharedResourceUpdFactor > 0){
        printf("%s:\n", "Static shared resource update interval for the cluster") ;
        printf("    SHARED_RESOURCE_UPDATE_FACTOR = %d \n\n",reply->sharedResourceUpdFactor);
    }

    if (reply->jobDepLastSub == 1) {
        printf("%s:\n", "Used with job dependency scheduling") ;
        printf("    JOB_DEP_LAST_SUB = %d \n\n", reply->jobDepLastSub);
    }

	printf("%s:\n", "The Maximum JobId defined in the system");
    printf("    MAX_JOBID = %d\n\n",  reply->maxJobId);

    if (reply->maxAcctArchiveNum>0 ) {
        printf("%s:\n", "Max number of Acct files");
        printf(" %24s = %d\n\n", "MAX_ACCT_ARCHIVE_FILE", reply->maxAcctArchiveNum);
    }

    if (reply->acctArchiveInDays>0 ) {
        printf("%s:\n", "Mbatchd Archive Interval");
        printf(" %19s = %d %s \n\n", "ACCT_ARCHIVE_AGE", reply->acctArchiveInDays, "days");
    }

    if (reply->acctArchiveInSize>0 ) {
        printf("%s:\n", "Mbatchd Archive threshold");
        printf(" %20s = %d %s\n\n", "ACCT_ARCHIVE_SIZE", reply->acctArchiveInSize, "kB");
    }

}
