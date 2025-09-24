/* $Id: utmpreg.c,v 1.2 2007/08/15 22:18:56 tmizan Exp $
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
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/procfs.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>

#include "res.h"
#include "../lib/lproto.h"

static int checkParent(void);
int 
checkParent()
{   
    char *checkCode;

    checkCode = getenv("LSB_UTMP_CHECK_CODE");
    if (!checkCode) {
	return -1;
    } else if (!strcmp(checkCode, UTMP_CHECK_CODE)){
        return 0;
    } else {
	return -1;
    }
}

int main (int argc, char **argv)
{
    char fname[] = "utmpreg";
    char userName[MAXLSFNAMELEN];
    char ptyName[sizeof(PTY_TEMPLATE)];
    char logDir[MAXPATHLEN];
    char logMask[128];
    pid_t procId;
    struct passwd* ptrPass = 0; 
    int result = 0;

    if (argc != 6) {
	errno = EINVAL;
        exit(-1);
    }

    
    if (checkParent()<0) {
        errno = EINVAL;
	exit(-2);
    }
    
    if (getuid()==0) {
        strcpy(userName,argv[1]);
    } else {
        ptrPass = getpwuid(getuid()); 
        if (strcmp(argv[1],ptrPass->pw_name)) {
	    errno = EINVAL;
            exit(-3); 
        }
        strcpy(userName,ptrPass->pw_name);
    }

    
    procId = (pid_t) atol(argv[2]);
    if (procId != getppid()) {
	errno = EINVAL;
        exit(-4);
    }
    strcpy(ptyName,argv[3]);

    strcpy(logDir,argv[4]);
    strcpy(logMask,argv[5]);

    ls_openlog("res", logDir, 0, logMask);
    if (logclass & LC_TRACE) 
        ls_syslog(LOG_DEBUG,
            "%s: Entering UTMP registration program ...",fname);

     

    result = createUtmpEntry(userName, procId, ptyName);
    if (result < 0) {
        ls_syslog(LOG_ERR, "%s: createUtmpEntry failed", fname); 
    }
    if (logclass & LC_TRACE) 
        ls_syslog(LOG_DEBUG, "%s: Leaving UTMP registration program.",fname);
    ls_closelog();
    return result;
}

