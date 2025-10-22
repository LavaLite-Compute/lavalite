/* $Id: bswitch.c,v 1.3 2007/08/15 22:18:44 tmizan Exp $
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

static int printErrMsg (LS_LONG_INT jobId, char *queue);

void
usage (char *cmd)
{
    fprintf(stderr, "Usage");
    fprintf(stderr, ": %s [-h] [-V] [-q queue_name] [-u user_name |-u all] [-m host_name]\n               [-J job_name] dest_queue [jobId |  \"jobId[idxList]\" ...]\n", cmd);
    exit(-1);
}

int
main (int argc, char **argv)
{
    extern char *optarg;
    extern int optind;
    char *queue = NULL, *user = NULL, *host = NULL, *jobName = NULL;
    char *destQueue = NULL;
    int numJobs;
    LS_LONG_INT *jobIds;
    int  i, cc, exitrc = 0;

    if (lsb_init(argv[0]) < 0) {
	lsb_perror("lsb_init");
	exit(-1);
    }

    while ((cc = getopt(argc, argv, "VhJ:q:u:m:")) != EOF) {
        switch (cc) {
        case 'u':
            if (user)
                usage(argv[0]);
            if (strcmp(optarg, "all") == 0)
                user = ALL_USERS;
            else
                user = optarg;
            break;
        case 'J':
            jobName = optarg;
            break;
        case 'q':
            queue = optarg;
            break;
        case 'm':
            host = optarg;
            break;
        case 'V':
            fprintf(stderr, "%s\n", LAVALITE_VERSION_STR);
            exit(0);
        case 'h':
        default:
            usage(argv[0]);
       }
    }

    if (argc >= optind + 1) {
        destQueue = argv[optind];
        optind++;
    }
    if (destQueue == NULL) {
        printf(("The destination queue name must be specified.\n"));
        usage(argv[0]);
    }
    numJobs = getJobIds (argc, argv, jobName, user, queue, host, &jobIds, 0);

    for (i = 0; i < numJobs; i++) {
	if (lsb_switchjob (jobIds[i], destQueue) < 0) {
            exitrc = -1;
	    printErrMsg (jobIds[i], destQueue);
        }
        else
	    printf(("Job <%s> is switched to queue <%s>\n"),
		lsb_jobid2str(jobIds[i]), destQueue);
    }
    exit(exitrc);

}

static int
printErrMsg (LS_LONG_INT jobId, char *queue)
{
    char Job[80];
    sprintf (Job, "%s <%s>", "Job", lsb_jobid2str(jobId));

    switch (lsberrno) {
    case LSBE_BAD_USER:
    case LSBE_PROTOCOL:
    case LSBE_MBATCHD:
        lsb_perror ("lsb_switchjob");
        return (-1);

   case LSBE_PERMISSION:
        lsb_perror (Job);
        return (-1);

    case LSBE_BAD_QUEUE:
    case LSBE_QUEUE_CLOSED:
    case LSBE_QUEUE_USE:
    case LSBE_QUEUE_HOST:
        lsb_perror (queue);
        return (-1);
    default:
	lsb_perror (Job);
        return (-1);
    }

}
