/* $Id: brequeue.c,v 1.2 2007/08/15 22:18:44 tmizan Exp $
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

#include "cmd.h"

#define NL_SETN 8	

void 
usage (char *cmd)
{
    fprintf(stderr, I18N_Usage);
    fprintf(stderr, ": %s [-deraH] [-h] [-V] [-u user_name | -u all]\n", cmd);
    fprintf(stderr, "                [-J name_spec] [jobId | \"jobId[idxList]\" ...]\n ");
    exit(-1);
}

int 
main (int argc, char **argv)
{
    extern char *optarg;
    extern int optind;
    char *queue = NULL, *user = NULL, *host = NULL, *jobName = NULL;
    char Job[80];
    int numJobs;
    LS_LONG_INT *jobIds;
    struct jobrequeue reqJob;
    int  i, cc, exitrc = 0;
    int rc;
    int options;
    int options1 = FALSE;

    rc = _i18n_init ( I18N_CAT_MIN );	

    if (lsb_init(argv[0]) < 0) {
	lsb_perror("lsb_init");
	exit(-1);
    }

    memset(&reqJob,0,sizeof(struct jobrequeue));

    reqJob.status = JOB_STAT_PEND;
    reqJob.options |= REQUEUE_RUN;
    options |= CUR_JOB;

    while ((cc = getopt(argc, argv, "deraHVhu:J:")) != EOF) {
        switch (cc) {
        case 'd':
           if (!options1) {
               reqJob.options = 0 ;
                reqJob.options |= REQUEUE_DONE;
                if (!(options & DONE_JOB)) {
                   options = 0 ;
                    options |= DONE_JOB;
               }
               options1 = TRUE;
           } else {
                reqJob.options |= REQUEUE_DONE;
                if (!(options & DONE_JOB)) {
                    options |= DONE_JOB;
               }
           }
           break;
        case 'e':
           if (!options1) {
               reqJob.options = 0 ;
                reqJob.options |= REQUEUE_EXIT;
                if (!(options & DONE_JOB)) {
                   options = 0 ;
                    options |= DONE_JOB;
               }
               options1 = TRUE;
           } else {
                reqJob.options |= REQUEUE_EXIT;
                if (!(options & DONE_JOB)) {
                    options |= DONE_JOB;
               }
           }
           break;
        case 'r':
           if (!options1) {
               reqJob.options = 0 ;
               reqJob.options |= REQUEUE_RUN;
               if (!(options & CUR_JOB)) {
                  options = 0 ;
                  options |= CUR_JOB;
               }
               options1 = TRUE;
           } else {
                reqJob.options |= REQUEUE_RUN;
                if (!(options & CUR_JOB)) {
                    options |= CUR_JOB;
               }
           }
           break;
        case 'a':
           
           reqJob.options = 0 ;
           options = 0 ;
            reqJob.options |= (REQUEUE_DONE | REQUEUE_EXIT | REQUEUE_RUN);
            options |= (CUR_JOB|DONE_JOB);
           break;
        case 'H':
           
           reqJob.status = 0;
           reqJob.status = JOB_STAT_PSUSP;
           break;
        case 'J': jobName = optarg;
            break;
        case 'V':
            fputs(_LS_VERSION_, stderr);
            exit(0);
	case 'u':
            if (user)
                usage(argv[0]);
            if (strcmp(optarg, "all") == 0)
                user = ALL_USERS;
            else
                user = optarg;
            break;
        case 'h':
        default:
            usage(argv[0]);
       }
    }

    numJobs = getJobIds (argc,argv,jobName,user,queue,host,&jobIds,options);
    
    for (i = 0; i < numJobs; i++) {
       reqJob.jobId = jobIds[i] ;
       if (lsb_requeuejob(&reqJob) < 0 ) {
	    exitrc = -1;
	    sprintf (Job,"%s <%s> ", I18N_Job, lsb_jobid2str(jobIds[i]));	    
            lsb_perror(Job);
	}
	else {
	    printf((_i18n_msg_get(ls_catd,NL_SETN,3151, "Job <%s> is being requeued \n")), /* catgets 3151 */
		   lsb_jobid2str(jobIds[i]));
	}
    }
    _i18n_end ( ls_catd ); 			
    exit(exitrc);

} 

