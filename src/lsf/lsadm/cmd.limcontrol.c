/* $Id: cmd.limcontrol.c,v 1.5 2007/08/15 22:18:54 tmizan Exp $
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

#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include "../../config.h"
#include "../lsf.h"
#include "../intlib/intlibout.h"
#include "../lib/lsi18n.h" 
#define NL_SETN 25      

extern int  optind;
extern char *optarg;

extern void millisleep_(int);
extern char isint_(char *word);
extern int  getConfirm(char *);
extern int checkConf(int verbose, int who);

extern char *myGetOpt (int nargc, char **nargv, char *ostr);

static void doHosts (int, char **, int);
static void doAllHosts (int);
static void operateHost (char *, int, int);

static int  exitrc;
static int fFlag;

int
limCtrl(int argc, char **argv, int opCode)
{
    char *optName, *localHost;
    int vFlag = 0;      
    int config = 0, checkReply;

    fFlag = 0;          
    if (strcmp(argv[optind-1], "reconfig") == 0) {
	config = 1;
    }
    while ((optName = myGetOpt(argc, argv, "f|v|")) != NULL) {
        switch (optName[0]) {
        case 'v':
	    if (opCode == LIM_CMD_SHUTDOWN)
		return(-2);
            vFlag = 1;
            break;
        case 'f':
            fFlag = 1;
            break;
        default:
            return(-2);
        }
    }
    exitrc = 0;
    if (config && optind != argc)  
        return -2;                     

    initMasterList_();
    if (config || opCode == LIM_CMD_REBOOT) {
	if ( getIsMasterCandidate_()) { 
	    checkReply = checkConf(vFlag, 1);
	    if ((checkReply == EXIT_FATAL_ERROR || checkReply == EXIT_WARNING_ERROR) && !vFlag && !fFlag) 
	        if (getConfirm(I18N(201, "Do you want to see the detailed messages? [y/n] ")))
		    checkReply = checkConf(1, 1);

	} else { 
	    if ( !vFlag && !fFlag) {
                if (!getConfirm(I18N(202, "This host is not a master candidate. We can not do a LIM configuration check. \n Do you still want to continue ? [y/n] "))) {
		    return 0;
	        }
	    }
	}
	switch (checkReply)  {
	case EXIT_FATAL_ERROR:                      
            return -1;
	case EXIT_WARNING_ERROR:                      
            if (fFlag)
                break;                 
            if (!getConfirm(I18N(250, "Do you want to reconfigure? [y/n] "))) /* catgets 250 */ {
                fprintf(stderr, I18N(251, "Reconfiguration aborted.\n")); /* catgets 251 */
                return(-1);
            }
            break;
        default:                      
	    break;
        }
    }
    if (config) {
        doAllHosts(opCode);
        
        return(exitrc); 
    }

    if (optind == argc) {              
	if ((localHost = ls_getmyhostname()) == NULL) { 
            ls_perror("ls_getmyhostname");
            return -1;
	}
        operateHost(localHost, opCode, 0);
    }
    else
    {
	if (!getIsMasterCandidate_())
	{ 
	    if (!(optind == argc-1 && !strcmp(argv[optind], ls_getmyhostname()))) { 
		fprintf(stderr, "%s\n",
                I18N(265, "Should not operate remote lim from slave only or client host")); /* catgets 265 */
		return (-1);
	    }
	}

	doHosts(argc, argv, opCode);
    }

    return(exitrc);         

} 

static
void doHosts (int argc, char **argv, int opCode)
{
    if (optind == argc-1 && strcmp(argv[optind], "all") == 0) {
        
	doAllHosts(opCode);
	return;
    }
    for (; optind < argc; optind++) 
	operateHost(argv[optind], opCode, 0);
    
} 

static
void doAllHosts (int opCode)
{
    int numhosts = 0, i;
    struct hostInfo *hostinfo;
    int ask = FALSE, try = FALSE;
    char msg[100]; 
    
    hostinfo = ls_gethostinfo("-:server", &numhosts, NULL, 0, LOCAL_ONLY);
    if (hostinfo == NULL) {
	ls_perror("ls_gethostinfo");
	fprintf(stderr, I18N(252, "Operation aborted\n")); /* catgets 252 */
        exitrc = -1;
	return;
    }

    if (!fFlag) {
	if (opCode == LIM_CMD_REBOOT)
            sprintf(msg, I18N(253, "Do you really want to restart LIMs on all hosts? [y/n] ")); /* catgets 253 */
        else 
	    sprintf(msg, I18N(254, "Do you really want to shut down LIMs on all hosts? [y/n] ")); /* catgets 254 */
	ask = (!getConfirm(msg));
    }
    for (i=0; i<numhosts; i++) 
        if (hostinfo[i].maxCpus > 0)
	    operateHost (hostinfo[i].hostName, opCode, ask);
        else 
	    try = 1;
    if (try) {
        fprintf(stderr, "\n%s :\n\n", I18N(255, "Trying unavailable hosts")); /* catgets 255 */
        for (i=0; i<numhosts; i++) 
            if (hostinfo[i].maxCpus <= 0)
	        operateHost (hostinfo[i].hostName, opCode, ask);
    }

} 

static void 
operateHost (char *host, int opCode, int confirm)
{
    char msg1[MAXLINELEN];
    char msg[MAXLINELEN];

    if (opCode == LIM_CMD_REBOOT)
	sprintf(msg1, I18N(256, "Restart LIM on <%s>"), host); /* catgets 256 */
    else
	sprintf(msg1, I18N(257, "Shut down LIM on <%s>"), host); /* catgets 257 */

    if (confirm) {
	sprintf(msg, "%s ? [y/n] ", msg1);
        if (!getConfirm(msg))
	    return;
    }
    fprintf(stderr, "%s ...... ", msg1);
    fflush(stderr);
    if (ls_limcontrol(host, opCode) == -1) {
	ls_perror ("ls_limcontrol");
	exitrc = -1;
    } else {
	char *delay = getenv("LSF_RESTART_DELAY");
	int  delay_time;
	if (delay == 0)
	    delay_time = 500;
	else
	    delay_time = atoi(delay) * 1000;

	millisleep_(delay_time); 
	fprintf (stderr, "%s\n", I18N_done);
    }
    fflush(stderr);

} 

int
limLock(int argc, char **argv)
{
    u_long duration = 0;
    extern int optind;
    extern char *optarg;
    char *optName;

    while ((optName = myGetOpt(argc, argv, "l:")) != NULL) {
        switch(optName[0]) {
            case 'l':
                duration = atoi(optarg);
                if (!isint_(optarg) || atoi(optarg) <= 0) {
	            fprintf(stderr, I18N(258, "The host locking duration <%s> should be a positive integer\n"), optarg); /* catgets 258 */
                    return -2;
		}
		break;
            default:
                return -2;
        }
    }
   
    if (argc > optind) 
        return -2;

    if (ls_lockhost(duration) < 0) {
	ls_perror("failed");
        return(-1);
    }

    if (duration)
        printf(I18N(259, "Host is locked for %lu seconds\n") /* catgets 259 */,
	       (unsigned long)duration);
    else
        printf(I18N(260, "Host is locked\n")); /* catgets 260 */

    fflush(stdout);
    return(0);
} 

int
limUnlock(int argc, char **argv)
{
    extern int optind;

    if (argc > optind) {
	fprintf(stderr, I18N(261, "Syntax error: too many arguments.\n")); /* catgets 261 */
	return(-2);
    }

    if (ls_unlockhost() < 0) {
        ls_perror("ls_unlockhost");
        return(-1);
    }

    printf(I18N(262, "Host is unlocked\n")); /* catgets 262 */
    fflush(stdout);

    return(0);
} 
