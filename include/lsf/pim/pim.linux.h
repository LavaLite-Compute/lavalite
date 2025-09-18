/* $Id: pim.linux.h,v 1.4 2007/08/15 22:18:55 tmizan Exp $
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
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>
#include <syslog.h>
#include <sys/sysmacros.h>
#include <sys/vfs.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <dirent.h>
#include "../lim/lim.h"

#include "../../lsf/lib/lsi18n.h"
#define NL_SETN		28	




static char buffer[1024];

extern int bytes;			
extern int nproc;			
extern int pagesize;			
extern int nr_of_processes;		
extern struct lsPidInfo *pbase;		
extern struct lsPidInfo *old_pbase;	

extern int scanIt;

static struct dirent *process;
static DIR *dir_proc_fd;
static struct lsPidInfo info_rec;

#define MAX_NR_OF_PROCESSES		50 
#define PROCESSES_INCREMENT		10 

extern int getpagesize(void);
extern char *strdup __P ((__const char *__s));
static int parse_stat(char* S, struct lsPidInfo* P);
static int get_lsPidInfo( int PID, struct lsPidInfo *info_rec );
extern int isNfsDaemon(char *, char *);


void
open_kern(void)
{
    static char fname[] = "pim/open_kern";

    nproc = MAX_NR_OF_PROCESSES;

    

    bytes = nproc * sizeof(struct lsPidInfo);
    pbase = (struct lsPidInfo *)malloc(bytes);
    memset( (char*)pbase, 0, bytes );
    old_pbase = (struct lsPidInfo *)malloc(bytes);
    memset( (char*)old_pbase, 0, bytes );

    if (pbase == (struct lsPidInfo *)NULL ||
	    old_pbase == (struct lsPidInfo *)NULL ) {
	ls_syslog(LOG_ERR, I18N_FUNC_FAIL_M, fname, "malloc");
        ls_syslog(LOG_ERR, I18N_Exiting);
        exit(-1);
    }

    
    pagesize = getpagesize();

    
    info_rec.command = (char*)malloc( 1024 );

    scanIt = TRUE;

    return;
} 

void
scan_procs(void)
{
    static char fname[] = "pim/get_process_id";
    int j;

    

    for ( j=0; j < nr_of_processes; j++ ) {
        if ( pbase[j].command != (char*)0 ) {
            free( pbase[j].command );
            pbase[j].command = (char*)0;
        }
    }

    

    dir_proc_fd = opendir("/proc");

    if ( dir_proc_fd == (DIR*)0 ) {
	ls_syslog(LOG_ERR, I18N_FUNC_S_FAIL_M, fname, "open", "/proc");
        ls_syslog(LOG_ERR, I18N_Exiting);
	exit(-1);
    }

    nr_of_processes = 0;
    while (( process = readdir( dir_proc_fd ))) {
	
	if (isdigit( process->d_name[0] ) ) {
	    

	    
	    if ( nr_of_processes == nproc - 1 ) {
		nproc += PROCESSES_INCREMENT;
		bytes = nproc * sizeof( struct lsPidInfo );
		pbase = (struct lsPidInfo *)realloc( pbase, bytes );
		memset( (char*)&pbase[nr_of_processes], 0, 
			PROCESSES_INCREMENT * sizeof( struct lsPidInfo ) );

		

		old_pbase = (struct lsPidInfo *)realloc( old_pbase, bytes );
		memset( (char*)&old_pbase[nr_of_processes], 0,
			PROCESSES_INCREMENT * sizeof( struct lsPidInfo ) );

		if (pbase == (struct lsPidInfo *)NULL ||
		    old_pbase == (struct lsPidInfo *)NULL ) {
		    ls_syslog(LOG_ERR, I18N_FUNC_FAIL_M, fname, "malloc");
        	    ls_syslog(LOG_ERR, I18N_Exiting);
		    exit(-1);
		}
	    }
	    if (get_lsPidInfo(atoi(process->d_name), &info_rec) != 0) {
		
		continue;
	    }

	    
	    if (isNfsDaemon(buffer, "/nfsiod"))
		continue;

	     
	    if (info_rec.pgid == 1) 
		continue;

	    
            pbase[nr_of_processes].pid = info_rec.pid;
            pbase[nr_of_processes].ppid = info_rec.ppid;
            pbase[nr_of_processes].pgid = info_rec.pgid;

	    pbase[nr_of_processes].utime = info_rec.utime/100;
	    pbase[nr_of_processes].stime = info_rec.stime/100;
	    pbase[nr_of_processes].cutime = info_rec.cutime/100;
	    pbase[nr_of_processes].cstime = info_rec.cstime/100;
          
            pbase[nr_of_processes].proc_size = info_rec.proc_size;
            pbase[nr_of_processes].resident_size = info_rec.resident_size*(getpagesize()/1024);
            pbase[nr_of_processes].stack_size = info_rec.stack_size;
            if ( pbase[nr_of_processes].stack_size < 0 )
                pbase[nr_of_processes].stack_size = 0;
	    pbase[nr_of_processes].status = info_rec.status;

#if defined(_COMMAND_LINE_)
	    

            pbase[nr_of_processes].command = strdup( info_rec.command );
#endif 

	    nr_of_processes++;
	}
    }
    closedir( dir_proc_fd );

    return;

} 



static int
get_lsPidInfo( int PID, struct lsPidInfo *info_rec )
{
    static char fname[] = "pim/get_lsPidInfo";
    int fd;
    char filename[30];


     

    sprintf( filename, "/proc/%d/stat", PID );

    fd = open( filename, O_RDONLY, 0);
    if ( fd == -1 ) {
	ls_syslog( LOG_DEBUG, "%s: cannot open [%s] %m, continuing", fname, filename );
	return(1);
    }
    if ( read(fd, buffer, sizeof( buffer) - 1 ) <= 0 ) {
	ls_syslog( LOG_DEBUG, "%s: cannot read [%s] %m, continuing", fname, filename );
	close( fd );
	return(1);
    }
    close( fd );

    if (parse_stat((char*)&buffer, info_rec) < 0) {
	if (logclass & LC_PIM) 
	    ls_syslog(LOG_DEBUG, "%s: failed for process <%d>, continuing", fname, PID);
	return(1);
    }

#if defined(_COMMAND_LINE_)

    sprintf( filename, "/proc/%d/cmdline", PID );

    fd = open( filename, O_RDONLY, 0);
    if ( fd == -1 ) {
	ls_syslog( LOG_DEBUG, "%s: cannot open [%s] %m, continuing", fname, filename );
	return(1);
    }
    len = read(fd, info_rec->command, sizeof(buffer)-1 );
    if ( len <= 0 ) {
	close( fd );
	return(1);
    }
    info_rec->command[len] = 0;
    close( fd );
    nulls2spc( info_rec->command, len );
#endif 

    return(0);

} 


static int
parse_stat(char* S, struct lsPidInfo* P)
{
    unsigned int rss_rlim, start_code, end_code, start_stack, end_stack;
    unsigned char status;
    unsigned long vsize;

    sscanf(S, "%d %s %c %d %d %*d %*d %*d %*u %*u %*u %*u %*u %d %d %d "
	      "%d %*d %*d %*u %*u %*d %lu %u %u %u %u %u %u",
           &P->pid, P->command, &status, &P->ppid, &P->pgid,
           &P->utime, &P->stime, &P->cutime, &P->cstime, 
           &vsize, &P->resident_size, &rss_rlim, &start_code, 
	   &end_code, &start_stack, &end_stack);

    
    if (P->pid == 0) {
	if (logclass & LC_PIM)
            ls_syslog(LOG_DEBUG, "parse_stat(): invalid process 0 found: %s", S);
	return (-1);
    }

    P->stack_size = start_stack - end_stack;
    P->proc_size = vsize/1024; 

    switch ( status ) {
	case 'R' : 
	    P->status = LS_PSTAT_RUNNING;
	    break;
	case 'S' : 
	    P->status = LS_PSTAT_SLEEP;
	    break;
	case 'D' : 
	    P->status = LS_PSTAT_SLEEP;
	    break;
	case 'T' : 
	    P->status = LS_PSTAT_STOPPED;
	    break;
	case 'Z' : 
	    P->status = LS_PSTAT_ZOMBI;
	    break;
	case 'W' : 
	    P->status = LS_PSTAT_SWAPPED;
	    break;
	default :
	    P->status = LS_PSTAT_RUNNING;
	    break;
    }
    return (0);

} 

#if defined(_COMMAND_LINE_)
static void
nulls2spc(char* str, int len)
{
    int i;
    for (i=0; i < len; i++)
	if (str[i]==0)
	    str[i]=' ';
} 
#endif 
