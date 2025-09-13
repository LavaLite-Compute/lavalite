/* $Id: brun.c,v 1.3 2007/08/15 22:18:44 tmizan Exp $
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
#include <string.h>
#include <ctype.h>

#define NL_SETN 8	 

extern int   optind;
extern char *optarg;

static void
usage(char* name)
{
    fprintf(stderr, I18N_Usage);
    fprintf(stderr, ": %s [ -h ] [ -V ] [ -f ] [ -b ] -m host_name ... jobId", name);
    if (lsbMode_ & LSB_MODE_BATCH)
	fprintf(stderr,  " | \"jobId[index]\""); 
    fprintf(stderr, "\n");
}

static int
countHosts(char* buf)
{
    char*  p;
    char*  u;
    int    n = 0;

    if (buf == NULL) {
	usage("brun");
	exit(-1);
    }

    p = buf;

    u = strtok(p, " \n\t");
    ++n ;

    while(( u = strtok(NULL, " \n\t")))
	++n;

    return(n);
}


int
main(int argc, char** argv)
{
    char*                 hosts   = NULL;
    struct runJobRequest  runJobRequest;
    int                   cc;
    int                   c;
    bool_t                fFlag = FALSE;
    bool_t		  bFlag = FALSE;
    int rc;

    rc = _i18n_init ( I18N_CAT_MIN );	


    if (lsb_init(argv[0]) < 0) {
	lsb_perror("lsb_init");
	exit (-1);
    }

    while((c = getopt(argc, argv, "m:fbhV")) != EOF) {
	switch(c) {
	case 'm':
	    hosts = putstr_(optarg);
	    if (hosts == NULL) {
		perror("putstr_");
		exit(-1);
	    }
	    break;
        case 'f':
	    fFlag = TRUE;
	    break;
	case 'b':
	    bFlag = TRUE;
	    break;
	case 'V':
	    fputs(_LS_VERSION_, stderr);
	    return (0);
	case 'h':
	    usage(argv[0]);
	    exit(-1);
	}
    }

    if (argc <= optind) {
	usage(argv[0]);
	exit(-1);
    }

    memset((struct runJobRequest* )&runJobRequest, 0,
	   sizeof(struct runJobRequest));

    
    if (getOneJobId (argv[argc - 1], &(runJobRequest.jobId), 0)) {
	usage(argv[0]);
	exit(-1);
    }
    runJobRequest.numHosts = countHosts(hosts);
    
    if (runJobRequest.numHosts > 1) {
	int     i;

	runJobRequest.hostname = (char **)calloc(runJobRequest.numHosts,
						 sizeof(char *));
	if (runJobRequest.hostname == NULL) {
	    perror("calloc");
	    exit(-1);
	}

	for (i = 0; i < runJobRequest.numHosts; i++) {
	    while (isspace(*hosts)) hosts++;
	    runJobRequest.hostname[i] = hosts;
	    hosts += strlen(hosts) + 1;
	}
    } else
	runJobRequest.hostname = &hosts;
	
    runJobRequest.options = (fFlag == TRUE) ? 
	RUNJOB_OPT_NOSTOP : RUNJOB_OPT_NORMAL;

    if (bFlag) {
	runJobRequest.options |= RUNJOB_OPT_FROM_BEGIN;
    } 

        
    cc = lsb_runjob(&runJobRequest);
    if (cc < 0) {
	lsb_perror((_i18n_msg_get(ls_catd,NL_SETN,2755, "Failed to run the job"))); /* catgets  2755  */
	exit(-1);
    }

    printf((_i18n_msg_get(ls_catd,NL_SETN,2756, "Job <%s> is being forced to run.\n")), /* catgets  2756  */
	   lsb_jobid2str(runJobRequest.jobId));

    _i18n_end ( ls_catd );			
    return (0);
}


