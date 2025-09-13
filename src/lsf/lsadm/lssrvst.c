/* $Id: lssrvst.c,v 1.3 2007/08/15 22:18:54 tmizan Exp $
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
#include <errno.h>
#include <sys/types.h>
#include <pwd.h>

#include "../lsf.h"


char *myargv[3];
char *clustername[256], *Admin[256];
char lsf_cluster_file[256], lsf_shared_file[256], lsfdaemons[256];
char *pathname;     
struct config_param  myparamList[] =
{
#define LSF_CONFDIR     0
        {"LSF_CONFDIR", NULL},
#define LSF_SERVERDIR     1
        {"LSF_SERVERDIR", NULL},
        {NULL, NULL}
};

struct clusterConf *myclusterConf;
struct clusterInfo *clinfo;
struct sharedConf *mysharedConf;


static void usage (cmd)
char *cmd;
{
    fprintf(stderr,"Usage: %s start/stop\n", cmd); 
    exit(-1);
}

void
GetLSFenv()
{
    char *envdir;
    int i;
    
    if ((envdir = getenv("LSF_ENVDIR")) != NULL)
        pathname = envdir;
    else if (pathname == NULL)
        pathname = "/etc";
    if (logclass & (LC_TRACE))
	    ls_syslog(LOG_DEBUG, "pathname is %s\n", pathname);

    if (ls_readconfenv(myparamList, pathname) < 0){
       ls_perror("ls_readconfenv");
       exit (-1);
    }

    if (myparamList[LSF_CONFDIR].paramValue == NULL) {
       fprintf(stderr, "LSF_CONFDIR is not defined in %s/lsf.conf\n", pathname);
       ls_perror("LSF_CONFDIR not defined");
       exit (-1);
    }
    else if (logclass & (LC_TRACE))
            ls_syslog(LOG_DEBUG, "LSF_CONFDIR = %s\n", myparamList[LSF_CONFDIR].paramValue);

    if (myparamList[LSF_SERVERDIR].paramValue == NULL)
      fprintf(stderr,"LSF_SERVERFDIR is not defined in %s/lsf.conf\n", pathname);
    else if (logclass & (LC_TRACE))
            ls_syslog(LOG_DEBUG, "LSF_SERVERDIR = %s\n", myparamList[LSF_SERVERDIR].paramValue);

    sprintf(lsfdaemons, "%s/lsf_daemons", myparamList[LSF_SERVERDIR].paramValue);
    sprintf(lsf_shared_file, "%s/lsf.shared",
    myparamList[LSF_CONFDIR].paramValue);
    if (logclass & (LC_TRACE)) 
       ls_syslog(LOG_DEBUG, "My lsf.shared file is: %s\n", lsf_shared_file);

    
    mysharedConf = ls_readshared(lsf_shared_file);
    if (mysharedConf == NULL) {
      ls_perror("ls_readshared");
      exit (-1);
    }

    if (logclass & (LC_TRACE)) {
      ls_syslog(LOG_DEBUG, "Clusters names is: %s\n", mysharedConf->clusterName);
    }


    if ((findMyCluster(mysharedConf->clusterName)) == 1)
        return;
    fprintf(stderr, "Could not find the cluster to which I belong to\n");
}

int
findMyCluster(char *CName)
{
    int j, k;
    char *hname;

    
    if ((hname = ls_getmyhostname()) == NULL)
      ls_perror("ls_getmyhostname");
        if (logclass & (LC_TRACE))
           ls_syslog(LOG_DEBUG, " Myhostname is: %s\n", hname);


    
    sprintf(lsf_cluster_file, "%s/lsf.cluster.%s",
    myparamList[LSF_CONFDIR].paramValue,
    CName);

 
    myclusterConf = ls_readcluster(lsf_cluster_file, mysharedConf->lsinfo);
    if (myclusterConf == NULL) {
      ls_perror("ls_readcluster");
      exit (-1);
    }

    
    
    for (k=0; k<myclusterConf->numHosts; k++) {
       if (logclass & (LC_TRACE)) 
            ls_syslog(LOG_DEBUG, " Host[%d]: %s\n", k, myclusterConf->hosts[k]);
       if (strcmp (hname, myclusterConf->hosts[k]) == 0) {
         if (logclass & (LC_TRACE)) {
            ls_syslog(LOG_DEBUG, "I, Host %s, belong to cluster %s\n", hname, CName);
            ls_syslog(LOG_DEBUG, "Number of Admins: %d\n", myclusterConf->clinfo->nAdmins ); 
          }
         for (j=0; j<myclusterConf->clinfo->nAdmins; j++) {
            if (logclass & (LC_TRACE)) 
               ls_syslog(LOG_DEBUG, "Admin[%d]: %s\n", j, myclusterConf->clinfo->admins[j]);
            Admin[j]=myclusterConf->clinfo->admins[j];
         }
       return 1;
       }
    }

    return 0;
}



int check_user()

{
        int user_id;
        int i;
        struct passwd *pw;

        user_id = getuid();
        pw=getpwuid(user_id);

        for(i=1; i<myclusterConf->clinfo->nAdmins; i++) {
           if ((strcmp(pw->pw_name, Admin[i]) == 0)) {
	   if (logclass & LC_TRACE)
	      ls_syslog(LOG_DEBUG1, "** %s is allowed to run this prog\n", pw->pw_name);
           return 0;
           }
        }
        fprintf(stderr, "** %s is not allowed to run this prog. Only one of the LSF admins can run it..\n", pw->pw_name);
        return -1;
}



void be_root()
{
    if (setuid(0)) {
        perror("setuid(0)");
        exit(1);
    }
}


void
main(argc, argv)

int argc;
char **argv;

{
       int checkuser;	

       if (argc < 2)
	  usage(argv[0]);

       if (ls_initdebug(argv[0]) < 0) {
           ls_perror("ls_initdebug");
           exit(-1);
       }

            
        GetLSFenv();

	
        checkuser = check_user();	
        if (checkuser == -1)  
           exit (-1);
        
	
        be_root();      

	if (strcmp(argv[1], "stop") == 0) { 
	    myargv[0] = lsfdaemons;
            myargv[1] = "stop";
            myargv[2] = NULL;
            if (logclass & LC_TRACE)
               ls_syslog(LOG_DEBUG1, "Stopping LSF daemons ...  \n");
	    execvp(myargv[0], myargv);
	    perror("execvp");
        } else if (strcmp(argv[1], "start") == 0) {
	       myargv[0] = lsfdaemons;
               myargv[1] = "start";
               myargv[2] = NULL;
               if (logclass & LC_TRACE)
                  ls_syslog(LOG_DEBUG1, "Starting the LSF daemons \n");
	       execvp(myargv[0], myargv);
	       perror("execvp");
           } else 
	         usage(argv[0]);
}

