/* $Id: bmig.c,v 1.3 2007/08/15 22:18:43 tmizan Exp $
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

#include <unistd.h>
#include <pwd.h>

#include <signal.h>
#include "cmd.h"

#define NL_SETN 8 	

void 
usage (char *cmd)
{
    fprintf(stderr, I18N_Usage);
    fprintf(stderr, ": %s [-h] [-V] [-f] [-m \"host_name ...\"]\n            [-u user_name |-u all] [-J job_name] [jobId | \"jobId[index_list]\" ...]\n", cmd);
    exit(-1);
} 

int
main (int argc, char **argv)
{
    extern char *optarg;

    int cc, i, badHostIdx;
    char *user = NULL, *jobName = NULL;
    LS_LONG_INT *jobIds;
    int numJobs = 0;
    struct submig mig;
    int rc;

    rc = _i18n_init ( I18N_CAT_MIN );	


    if (lsb_init(argv[0]) < 0) {
	lsb_perror("lsb_init");
	exit(-1);
    }

    mig.options = 0;
    mig.askedHosts = NULL;
    mig.numAskedHosts = 0;
    
    while ((cc = getopt(argc, argv, "Vhfm:u:J:")) != EOF) {
        switch (cc) {
	  case 'm':
            if (mig.numAskedHosts)        
                usage(argv[0]);
            if (getAskedHosts_(optarg, &mig.askedHosts, &mig.numAskedHosts,
			 &i, FALSE) < 0 && lserrno != LSE_BAD_HOST) {
		ls_perror(NULL);
                exit(-1);
            }
	    if (mig.numAskedHosts == 0)
		usage(argv[0]);
	    break;
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
	  case 'f':
	    mig.options |= LSB_CHKPNT_FORCE;
	    break;
	  case 'V':
	    fputs(_LS_VERSION_, stderr);
	    exit(0);
	  case 'h':
	  default:
            usage(argv[0]);
        }
    }

    numJobs = getJobIds (argc, argv, jobName, user, NULL, NULL, &jobIds, 0);

    
    for (i = 0; i < numJobs; i++) {
	mig.jobId = jobIds[i];
	if (lsb_mig(&mig, &badHostIdx) < 0) {
	    fprintf(stderr, "%s <%s>: ", I18N_Job, lsb_jobid2str(jobIds[i])); 
	    if (lsberrno == LSBE_QUEUE_HOST || lsberrno == LSBE_BAD_HOST)
		lsb_perror(mig.askedHosts[badHostIdx]);
	    else
		lsb_perror(NULL);
	} else {
	    printf((_i18n_msg_get(ls_catd,NL_SETN,2053, "Job <%s> is being migrated\n")), lsb_jobid2str(jobIds[i])); /* catgets  2053  */
	}
    }
    
    _i18n_end ( ls_catd );			
    exit(0);
} 
