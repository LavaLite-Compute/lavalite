/* $Id: cmd.rescontrol.c,v 1.3 2007/08/15 22:18:54 tmizan Exp $
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include "../lsf.h"
#include "../../config.h"
#include "../lib/lsi18n.h"
#include "../lib/mls.h"


#define MAX_CONN 20
#define NUM_TRY 2 

#define NL_SETN 25      

static void ctrlAllRes (int opCode, int data);
static void resGroupControl(char **hosts, int numHosts, int opCode, 
			    int data, int ask);
static void resControl (char *host, int opCode, int data, int ask);

static int exitrc, fFlag;
static char opStr[MAXLINELEN];

extern char *optarg;

extern char isint_(char *word);
extern int  getConfirm (char *);
extern char *myGetOpt (int nargc, char **nargv, char *ostr);
extern void millisleep_(int);


int
resCtrl(int argc, char **argv, int opCode)
{
    extern int  optind;
    int  num, data = 0;
    char *optName, *localHost;

    fFlag = 0;          
    while ((optName = myGetOpt(argc, argv, "f|c:")) != NULL) {
	switch (optName[0]) {
	case 'c':
            if (opCode != RES_CMD_LOGON) 
                return -2;
	    data = atoi(optarg);
            if (!isint_(optarg) || data < 0) {
		fprintf(stderr, I18N(300, "CPU time <%s> should be a non-negative integer\n"), optarg);  /* catgets 300 */
	        return(-2);
	    }
            break;
	case 'f':
	    if (opCode != RES_CMD_REBOOT && opCode != RES_CMD_SHUTDOWN)
		return(-2);
            fFlag = 1;
            break;
        default:
	    return(-2);
	}
    }
    exitrc = 0;
    if (optind >= argc) {           
        if ((localHost = ls_getmyhostname()) == NULL) {
            ls_perror("ls_getmyhostname");
            return -1;
        }
	resGroupControl(&localHost, 1, opCode, data, 0);
        return(exitrc);   
    }
   
    if (optind == argc-1 && strcmp(argv[optind], "all") == 0) {
        
        ctrlAllRes (opCode, data);
        return(exitrc);
    }
    for (; optind < argc; optind += MAX_CONN) {
        num = (argc - optind <  MAX_CONN) ? argc - optind : MAX_CONN;
        resGroupControl(&(argv[optind]), num, opCode, data, 0);
    }
    
    return(exitrc);   

} 


static void 
ctrlAllRes (int opCode, int data)
{
    int i, j, num, ask = 0;
    int numhosts = 0;
    char *hosts[MAX_CONN];
    struct hostInfo *hostinfo;
    char msg[100];

    hostinfo = ls_gethostinfo("-:server", &numhosts, NULL, 0, LOCAL_ONLY);
    if (hostinfo == NULL) {
	ls_perror("ls_gethostinfo");
	exitrc = -1;
	return;
    }

    if (!fFlag && (opCode == RES_CMD_SHUTDOWN || opCode == RES_CMD_REBOOT)) {
	if (opCode == RES_CMD_REBOOT)
	    sprintf(msg, I18N(301, "Do you really want to restart RES on all hosts? [y/n] ")); /* catgets 301 */
        else
            sprintf(msg, I18N(302, "Do you really want to shut down RES on all hosts? [y/n] ")); /* catgets 302 */
        ask = (!getConfirm(msg));
    }
    for (i=0; i<numhosts; i += MAX_CONN) { 
        num = (numhosts - i <  MAX_CONN) ? numhosts - i : MAX_CONN;
        for (j=0; j<num; j++)
            hosts[j] = hostinfo[i+j].hostName;
        resGroupControl(hosts, num, opCode, data, ask);
    }

} 

static void
resGroupControl(char **hosts, int numHosts, int opCode, int data, int ask)
{
    pid_t pid;
    int i; 
    LS_WAIT_T status;
    int num_port;
    struct rlimit rlp;

    
    if ((pid = fork()) < 0) {
        perror("fork");
        exitrc = -1;
	return ;   
    }

    if (pid == 0) {                
	num_port = numHosts * NUM_TRY;

#ifdef RLIMIT_NOFILE
	 

	getrlimit(RLIMIT_NOFILE,&rlp);
	rlp.rlim_cur = rlp.rlim_max;
	if (setrlimit(RLIMIT_NOFILE,&rlp) < 0) {
	    ls_syslog(LOG_DEBUG,
 	       "resGroupControl: failed to setrlimit RLIMIT_NOFILE to <%lx> %m",
		(long int) rlp.rlim_cur);
        }
#endif
        if (ls_initrex(num_port, 0) < num_port) {
            ls_perror("ls_initrex");
            exit(-1);   
        }
        
        
	switch (opCode) {
	case RES_CMD_REBOOT : 
	    sprintf(opStr, I18N(303, "Restart RES on")); /* catgets 303 */
	    break;
	case RES_CMD_SHUTDOWN : 
	    sprintf(opStr, I18N(304, "Shut down RES on")); /* catgets 304 */
	    break;
	case RES_CMD_LOGON : 
	    sprintf(opStr, I18N(305, "Turn on RES log on")); /* catgets 305 */
	    break;
	case RES_CMD_LOGOFF : 
	    sprintf(opStr, I18N(306, "Turn off RES log on")); /* catgets 306 */
	    break;
        default : 
	    exit(-1);
        }
        if (!ask) 
            
            for (i=0; i<numHosts; i++) 
                if (ls_connect(hosts[i]) < 0) {
                    fprintf(stderr, "%s <%s> ...... %s: %s\n", opStr, 
			    hosts[i], I18N(307, "failed"), /* catgets 307 */
			    ls_sysmsg());
                    exitrc = -1;
                    
                    hosts[i][0] = '\0';
                }
 
        for (i=0; i<numHosts; i++) {
            if (hosts[i][0] != '\0')
	        resControl (hosts[i], opCode, data, ask);
        }
	exit(exitrc);        
    }

    
    if (waitpid(pid, &status, 0) < 0) {
        perror("waitpid");
        exitrc = -1;
        return;
    }

    
    if (WIFEXITED(status) == 0) {
        fprintf(stderr, "%s.\n", 
		I18N(308, "Child process killed by signal")); /* catgets308 */
        exitrc = -1;
        return;
    } else {  
        if (WEXITSTATUS(status) == 0xff) {  
            exitrc = -1;
            return;
        }
    }

} 

static void 
resControl (char *host, int opCode, int data, int ask)
{
    char msg[512];
    int i;

    if (ask) {
        sprintf(msg, "%s <%s> ? [y/n] ", opStr, host);
        if (!getConfirm(msg))
            return;
    }
    fprintf(stderr, "%s <%s> ...... ", opStr, host);
    fflush(stderr);
    

    for (i = 0; i < NUM_TRY; i++) {
	if (ls_rescontrol(host, opCode, data) < 0) {
	    if (lserrno == LSE_NLSF_HOST) {
		if (i == NUM_TRY - 1) {
		    break;
		} else {
		    fprintf(stderr, "%s ...\n", 
			    I18N(309, "RES is not ready, still trying\n")); /* catgets 309 */
		    millisleep_(10000);
		    continue;
		}
	    } 
	    ls_perror ("ls_rescontrol");
	    exitrc = -1;
	} else {
	    fprintf(stderr, "%s\n", I18N_done);
	    if (opCode == RES_CMD_LOGON && data > 0)
		fprintf(stderr, I18N(311, "Logging tasks used more than %d msec of cpu time.\n"), /* catgets 311 */ data);
	}
	break;
    } 

    if (i == NUM_TRY - 1) 
	ls_perror ("ls_rescontrol");

    fflush(stderr);
    return;

} 

