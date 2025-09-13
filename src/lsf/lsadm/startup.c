/* $Id: startup.c,v 1.8 2007/08/15 22:18:54 tmizan Exp $
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

#include <netdb.h>

#include "../lsf.h"
#include <sys/wait.h>
#include "../lsadm/lsadmin.h"
#include "../lib/lproto.h"
#include "../lib/mls.h"
#include "../intlib/intlibout.h"
#include "../lib/lsi18n.h"
#include "../lib/mls.h"

#   define RSHCMD "rsh"

#define NL_SETN 25      

extern int  optind;
extern int  getConfirm(char *);

extern struct hostent * my_gethostbyname(char *);
static char * getOfficialHostName (char *);

static void startupAllHosts(int, int);
static void startupLocalHost(int);
static void startupRemoteHost(char *, int, int);
static int execDaemon(int, char **);
static int getLSFenv(void);
static struct clusterConf *findMyCluster(char *, struct sharedConf *);
static char *daemonPath(char *);
static int setStartupUid(void);

static int startupUid;
static struct clusterConf *myClusterConf = NULL;

static struct config_param  myParamList[] =
{
#define LSF_CONFDIR     0
        {"LSF_CONFDIR", NULL},
#define LSF_SERVERDIR     1
        {"LSF_SERVERDIR", NULL},
#define LSF_BINDIR     2
        {"LSF_BINDIR", NULL},
#define LSF_LIM_DEBUG     3
        {"LSF_LIM_DEBUG", NULL},
#define LSF_RES_DEBUG     4
        {"LSF_RES_DEBUG", NULL},
#define LSB_DEBUG     5
        {"LSB_DEBUG", NULL},
#define LSF_LINK_PATH     6
        {"LSF_LINK_PATH", NULL},
#define LSF_CONF_LAST  7 
        {NULL, NULL}
};

#define BADMIN_HSTARTUP 11   

static int
getLSFenv(void)
{
    int i;
    char lsfSharedFile[MAXLINELEN];
    char *envDir;
    struct sharedConf *mySharedConf;

    for (i = 0; i < LSF_CONF_LAST; i++) {
	FREEUP(myParamList[i].paramValue);
    }
	
    if ((envDir = getenv("LSF_ENVDIR")) == NULL)
	envDir = LSETCDIR;
	
    if (logclass & (LC_TRACE))
	ls_syslog(LOG_DEBUG, "LSF_ENVDIR is %s", envDir);
	
    if (initenv_(myParamList, envDir) < 0){
	ls_perror(envDir);
	return (-1);
    }

    if (myParamList[LSF_CONFDIR].paramValue == NULL) {
        fprintf(stderr, "%s %s %s/lsf.conf\n", "LSF_CONFDIR",
		I18N(400, "not defined in") /* catgets 400 */,
	        envDir);
	return (-1);
    }

    if (myParamList[LSF_SERVERDIR].paramValue == NULL
         && myParamList[LSF_LINK_PATH].paramValue != NULL) {
        fprintf(stderr, "%s %s %s/lsf.conf or environment\n", "LSF_SERVERDIR", 
		I18N(400, "not defined in"), 
	        envDir);
	return (-1);
    }

    if (myParamList[LSF_BINDIR].paramValue == NULL
            && myParamList[LSF_LINK_PATH].paramValue != NULL) {
        fprintf(stderr, "%s %s %s/lsf.conf  or environment\n", "LSF_BINDIR", 
		I18N(400, "not defined in"),
	        envDir);
	return (-1);
    }

    if (logclass & (LC_TRACE)) {
        ls_syslog(LOG_DEBUG,
		  "LSF_CONFDIR=<%s>, LSF_BINDIR=<%s>, LSF_SERVERDIR=<%s>",
		  myParamList[LSF_CONFDIR].paramValue,
		  myParamList[LSF_BINDIR].paramValue,
		  myParamList[LSF_SERVERDIR].paramValue);		  
    }

    initMasterList_();

    memset(lsfSharedFile,0,sizeof(lsfSharedFile));
    ls_strcat(lsfSharedFile,sizeof(lsfSharedFile),myParamList[LSF_CONFDIR].paramValue);
    ls_strcat(lsfSharedFile,sizeof(lsfSharedFile),"/lsf.shared");

    
    if ( access(lsfSharedFile, R_OK)) {
        if (!getIsMasterCandidate_()) { 
           return 0;
        } else {
           ls_perror("Can't access lsf.shared.");
           return(-1);
        }
    }

    mySharedConf = ls_readshared(lsfSharedFile);
    if (mySharedConf == NULL) {
        if (!getIsMasterCandidate_())  
           return 0;
        ls_perror("ls_readshared");
        return (-1);
    }
    
    if (logclass & (LC_TRACE)) { 
        ls_syslog(LOG_DEBUG, "My lsf.shared file is: %s", lsfSharedFile);
        ls_syslog(LOG_DEBUG, "Clusters name is: %s\n", mySharedConf->clusterName);
    }

    if ((myClusterConf = findMyCluster(mySharedConf->clusterName, mySharedConf))) {
        return(0);
    } else {
	    
	    
        if (lserrno && lserrno != LSE_NO_FILE) {
            return (-1);
        }
    }
    
    fprintf(stderr, "%s\n", I18N(401, "Host does not belong to LSF cluster.")); /* catgets 401 */
    return (-1);
} 

static char *
daemonPath(char *daemon)
{
    static char path[MAXFILENAMELEN], *srvdir;
   
    srvdir = myParamList[LSF_SERVERDIR].paramValue;

    memset(path,0,sizeof(path));
    ls_strcat(path,sizeof(path),srvdir);
    ls_strcat(path,sizeof(path),"/");
    ls_strcat(path,sizeof(path),daemon);
    if (logclass & (LC_TRACE))
	ls_syslog(LOG_DEBUG, "daemonPath %s", path);

    return (path);

} 


static int
setStartupUid(void)
{
    startupUid = getuid();
    
    if (startupUid == 0 || myParamList[LSF_LIM_DEBUG].paramValue ||
	myParamList[LSF_RES_DEBUG].paramValue ||
	myParamList[LSB_DEBUG].paramValue) {
	return 0;
    }

    return (-1);
}


static struct clusterConf *
findMyCluster(char *CName, struct sharedConf *mySharedConf)
{
    int j, k;
    char *lhost;
    char lsfClusterFile[MAXLINELEN];
    struct clusterConf *cl = NULL;
    
    if ((lhost = ls_getmyhostname()) == NULL) {
        ls_perror("ls_getmyhostname");
	return (NULL);
    }
    
    
       memset(lsfClusterFile,0,sizeof(lsfClusterFile));
       ls_strcat(lsfClusterFile,sizeof(lsfClusterFile),myParamList[LSF_CONFDIR].paramValue);
       ls_strcat(lsfClusterFile,sizeof(lsfClusterFile),"/lsf.cluster.");
       ls_strcat(lsfClusterFile,sizeof(lsfClusterFile),CName);

    cl = ls_readcluster(lsfClusterFile, mySharedConf->lsinfo);
    if (cl == NULL) {
	if (logclass & LC_TRACE)
	    ls_syslog(LOG_DEBUG, "ls_readcluster <%s> failed: %M",
		      lsfClusterFile);

	
	if (lserrno == LSE_NO_ERR) {
	    lserrno = LSE_BAD_ENV;
	}

	return NULL;
    }
    
    
    for (k = 0; k < cl->numHosts; k++) {
        if (logclass & (LC_TRACE)) 
            ls_syslog(LOG_DEBUG,
		" Host[%d]: %s", k, cl->hosts[k].hostName);
        if (strcmp(lhost, cl->hosts[k].hostName) == 0) {
            if (logclass & (LC_TRACE)) {
                ls_syslog(LOG_DEBUG,
			  "Local host %s belongs to cluster %s, nAdmins %d",
			  lhost, CName, cl->clinfo->nAdmins);
	    }
	    for (j = 0; j < cl->clinfo->nAdmins; j++) {
		if (logclass & (LC_TRACE)) 
		    ls_syslog(LOG_DEBUG, "Admin[%d]: %s", j,
			      cl->clinfo->admins[j]);
	    }
	    return (cl);
        }
    }

    return NULL;
} 



static
void startupAllHosts(int opCode, int confirm)
{
    int nh;
    char msg[MAXLINELEN];

    
    if (confirm) {
        if (opCode == LSADM_LIMSTARTUP)
            sprintf(msg,  I18N(404, "Do you really want to start up LIM on all hosts ? [y/n]")); /* catgets 404 */
        else if (opCode == LSADM_RESSTARTUP)
            sprintf(msg, I18N(405,  "Do you really want to start up RES on all hosts ? [y/n]"));  /* catgets 405 */
        else if (opCode == BADMIN_HSTARTUP)
            sprintf(msg, I18N(406,   "Do you really want to start up slave batch daemon on all hosts ? [y/n] ")); /* catgets 406 */
        else {
	    fprintf(stderr, "startupAllHosts: %s %d\n", 
		    I18N(407, "Unknown operation code"), opCode); /* catgets 407 */
	    return;
	}
        confirm = (!getConfirm(msg));
    }

     
    for (nh=0; nh < myClusterConf->numHosts; nh++) { 
	
        if ((myClusterConf->hosts[nh].isServer) &&
            (strncmp(myClusterConf->hosts[nh].hostType, "NT", 2) != 0)) {
	    startupRemoteHost(myClusterConf->hosts[nh].hostName, opCode,
			      confirm);
	}
        if (logclass & LC_TRACE) {
            ls_syslog(LOG_DEBUG1, "Number of Hosts is: %d\n",
                myClusterConf->numHosts);
            ls_syslog(LOG_DEBUG1, "Hostname: %s\n",
                myClusterConf->hosts[nh].hostName);
            ls_syslog(LOG_DEBUG1, "isServer?:  %d\n",
                myClusterConf->hosts[nh].isServer);
        }
    }

} 

static void 
startupLocalHost (int opCode)
{
    char operation[MAXLINELEN/2];
    char *host;
    char *myargv[10];
    
    if ((host = ls_getmyhostname()) == NULL)
	host = "localhost";

    if (opCode == LSADM_LIMSTARTUP)
	sprintf(operation, I18N(408, "Starting up LIM on"));  /* catgets 408 */
    else if (opCode == LSADM_RESSTARTUP)
	sprintf(operation, I18N(409, "Starting up RES on"));  /* catgets 409 */
    else if (opCode == BADMIN_HSTARTUP)
	sprintf(operation, I18N(410, "Starting up slave batch daemon on")); /* catgets 410 */   
    else {
	fprintf(stderr, "%s %d\n",  I18N(407, "Unknown operation code"), 
		opCode);
	return;
    }

    fprintf(stderr, "%s <%s> ...... ", operation, host);
    fflush(stderr);

    myargv[1] = NULL;
    myargv[2] = NULL;
    myargv[3] = NULL;
    myargv[4] = NULL;

    switch (opCode) {
    case LSADM_LIMSTARTUP :
	myargv[0] = daemonPath("lim");
	break;
    case LSADM_RESSTARTUP :
        myargv[0] = daemonPath("res");
	break;
    case BADMIN_HSTARTUP :
        myargv[0] = daemonPath("sbatchd");
	break;
    default :
	fprintf(stderr, "startUpHost: %s %d\n", I18N(407, "Unknown operation code"), opCode);
	return;
    }

    if (execDaemon(startupUid, myargv) == 0)
        fprintf (stderr, "%s\n", I18N_done);
    fflush(stderr);

} 

static void 
startupRemoteHost(char *host, int opCode, int ask)
{
    char *myargv[10];
    char *envDir;
    char msg[2*MAXLINELEN];
    int cc, symbolic = FALSE;

    if (opCode == LSADM_LIMSTARTUP)
        sprintf(msg, I18N(411, "Start up LIM on")); /* catgets 411 */
    else if (opCode == LSADM_RESSTARTUP)
        sprintf(msg, I18N(412, "Start up RES on")); /* catgets 412 */
    else if (opCode == BADMIN_HSTARTUP)
        sprintf(msg, I18N(413,"Start up slave batch daemon on")); /* catgets 413 */
    else {
	fprintf(stderr, "startupRemoteHost: %s %d\n", I18N(407, "Unknown operation code"), opCode);
	return;
    }

    if (ask) {
        sprintf(msg, "%s <%s> ? [y/n] ", msg, host);
        if (!getConfirm(msg))
            return;
    }
    
     
    if ((myParamList[LSF_LINK_PATH].paramValue != NULL
                 && strcmp(myParamList[LSF_LINK_PATH].paramValue, "n" ) != 0) 
                  || ((getenv ("LSF_SERVERDIR") == NULL) 
              &&  (getenv("LSF_BINDIR") == NULL))) {
         

        symbolic = TRUE;
    }
    cc = 0;
    myargv[cc++] = RSHCMD;
    myargv[cc++] = getOfficialHostName (host);
    myargv[cc++] = "-n";

     
    if ((envDir = getenv("LSF_ENVDIR"))) {
	
	myargv[cc++] = "/bin/sh ";
	myargv[cc++] = "-c ";

        if (strlen(envDir) > MAXLINELEN) {
           fprintf(stderr,"LSF_ENVDIR is longer than <%d> chars <%s> \n",
                         MAXLINELEN, envDir);
           exit(-1);
        }

	memset(msg,0,sizeof(msg));
	ls_strcat(msg,sizeof(msg),"'LSF_ENVDIR=");
	ls_strcat(msg,sizeof(msg),envDir);

        switch (opCode) {
        case LSADM_LIMSTARTUP :
           if (symbolic == TRUE) {
	        ls_strcat(msg,sizeof(msg),"; export LSF_ENVDIR; . $LSF_ENVDIR/lsf.conf; $LSF_BINDIR/lsadmin limstartup'");
            } else {
	        ls_strcat(msg,sizeof(msg),"; export LSF_ENVDIR; . $LSF_ENVDIR/lsf.conf; . $LSF_CONFDIR/profile.lsf; $LSF_BINDIR/lsadmin limstartup'");
            }
            break;
        case LSADM_RESSTARTUP :
            if (symbolic == TRUE) {
	        ls_strcat(msg,sizeof(msg),"; export LSF_ENVDIR; . $LSF_ENVDIR/lsf.conf; $LSF_BINDIR/lsadmin resstartup'");
            } else {
	        ls_strcat(msg,sizeof(msg),"; export LSF_ENVDIR; . $LSF_ENVDIR/lsf.conf; . $LSF_CONFDIR/profile.lsf; $LSF_BINDIR/lsadmin resstartup'");
            }
            break;
        case BADMIN_HSTARTUP :
            if (symbolic == TRUE) {
	        ls_strcat(msg,sizeof(msg),"; export LSF_ENVDIR; . $LSF_ENVDIR/lsf.conf; $LSF_BINDIR/badmin hstartup'");
            } else {
	        ls_strcat(msg,sizeof(msg),"; export LSF_ENVDIR; . $LSF_ENVDIR/lsf.conf; . $LSF_CONFDIR/profile.lsf; $LSF_BINDIR/badmin hstartup'");
            }
            break;
        default :
            fprintf(stderr, "startUpHostR: %s %d", 
		    I18N(407, "Unknown operation  code"), opCode);
            exit(-1);
        }

        myargv[cc++] = msg;
    } 
    else {
	myargv[cc++] = "/bin/sh ";
	myargv[cc++] = "-c ";

        switch (opCode) {
        case LSADM_LIMSTARTUP :
            if (symbolic == TRUE) {
	        myargv[cc++] = "'. /etc/lsf.conf; $LSF_BINDIR/lsadmin limstartup'";
            } else {
                 myargv[cc++] = "'. /etc/lsf.conf; . $LSF_CONFDIR/profile.lsf;  lsadmin limstartup'";
            }
            break;
        case LSADM_RESSTARTUP :
            if (symbolic == TRUE) {
	        myargv[cc++] = "'. /etc/lsf.conf; $LSF_BINDIR/lsadmin resstartup'";
            } else {
                 myargv[cc++] = "'. /etc/lsf.conf; . $LSF_CONFDIR/profile.lsf; lsadmin resstartup'";
            }
            break;
        case BADMIN_HSTARTUP :
            if (symbolic == TRUE) {
	        myargv[cc++] = "'. /etc/lsf.conf; $LSF_BINDIR/badmin hstartup'";
            } else {
                 myargv[cc++] = "'. /etc/lsf.conf; . $LSF_CONFDIR/profile.lsf; badmin hstartup'";
            }
            break;
        default :
            fprintf(stderr, "startUpHostR: %s %d\n", 
		    I18N(407, "Unknown operation  code"), opCode);
	    return;
        }

    }

    myargv[cc] = NULL;
    
    execDaemon(getuid(), myargv);
    fflush(stderr);

} 


static int 
execDaemon(int uid, char **myargv)
{
    int childpid;
    LS_WAIT_T status;

    if ((childpid = fork()) < 0) {
        perror("fork");
	return (-1);
    } else if (childpid > 0) {   
        while ( wait(&status) != childpid) 
           ;
        if (!WEXITSTATUS(status))
	   return 0;
	else
	   return -1;
    }

    

    if (lsfSetUid(uid) < 0) {
	perror("setuid");
	exit(-1);
    }
	
    lsfExecvp(myargv[0], myargv);
    perror(myargv[0]);
    exit(-2);
} 


int
startup(int argc, char **argv, int opCode)
{
    char *optName;
    int confirm;

    if (ls_initdebug(argv[0]) < 0) {
	ls_perror("ls_initdebug");
	return (-1);
    }

    if (getLSFenv() < 0)
	return (-1);
    

    if (setStartupUid() < 0) {
	fprintf(stderr, "%s\n", 
		I18N(414, "Not authorized to start up as root")); /* catgets 414 */
	return (-1);
    }

    confirm = TRUE;
    while ((optName = myGetOpt(argc, argv, "f|")) != NULL) {
        switch (optName[0]) {
        case 'f':
            confirm = FALSE;
            break;
        default:
            
            return (-1);
        }
    }

    if (!getIsMasterCandidate_() 
    && opCode == LSADM_LIMSTARTUP
    && !(optind == argc || (optind == argc-1 && !strcmp(argv[optind], ls_getmyhostname())))) 
    {
	
	fprintf(stderr, "%s\n", 
		I18N(415, "Should not start remote lim from slave only or clienthost")); /* catgets 415 */
	return (-1);
    }

    if (optind == argc) {
	
        startupLocalHost(opCode);
    } else if (optind == argc - 1 && strcmp(argv[optind], "all") == 0) {
        
	startupAllHosts(opCode, confirm);
    } else {
	
	for (; optind < argc; optind++) 
	    startupRemoteHost(argv[optind], opCode, confirm);
    }

    return 0;

}

static char *
getOfficialHostName (char * host)
{
    struct hostent* pmyhostent = NULL;
    struct hostent* phostent = NULL;
    static char officialHost[MAXHOSTNAMELEN];

    if (host == NULL)
	return (NULL);

    strncpy (officialHost, host, MAXHOSTNAMELEN);
    
    if ((pmyhostent = my_gethostbyname (host)) != NULL)
    {
	if ((phostent = gethostbyaddr (*pmyhostent->h_addr_list, 
	                               pmyhostent->h_length, 
				       pmyhostent->h_addrtype)) != NULL)
        if (phostent->h_name != NULL)
            strncpy (officialHost, phostent->h_name, MAXHOSTNAMELEN);
    }
    return (officialHost);
}
