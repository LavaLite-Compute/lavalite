/* $Id: daemon.c,v 1.4 2007/08/15 22:18:49 tmizan Exp $
 * Copyright (C) 2007 Platform Computing Inc
 * Copyright (C) LavaLite Contributors
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


#include <sys/time.h>
#include <sys/resource.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include "lsf/intlib/intlib_internal.h"
#include "lsf/lib/lproto.h"

#define NL_SETN      22

void
daemonize_(void)
{
    int i;
    struct rlimit rlp;
    char  errMsg[MAXLINELEN];

    ls_closelog();
    switch (fork()) {
    case 0:
	break;
    case -1:
	sprintf(errMsg, I18N_FUNC_FAIL_M, "daemonize_", "fork");
	perror(errMsg);
	exit(-1);
    default:
	exit(0);
    }

    setsid();

#ifdef RLIMIT_NOFILE

    getrlimit (RLIMIT_NOFILE, &rlp);
    rlp.rlim_cur = rlp.rlim_max;
    setrlimit (RLIMIT_NOFILE, &rlp);
#endif

#ifdef RLIMIT_CORE

    getrlimit (RLIMIT_CORE, &rlp);
    rlp.rlim_cur = rlp.rlim_max;
    setrlimit (RLIMIT_CORE, &rlp);
#endif



    for (i = 0 ; i < 3 ; i++)
	close(i);

    i = open(LSDEVNULL, O_RDWR);
    if (i != 0)
    {
	dup2(i, 0);
	close(i);
    }
    dup2(0, 1);
    dup2(0, 2);
}


static char daemon_dir[MAXPATHLEN];
void
saveDaemonDir_(char *argv0)
{
    int i;

    daemon_dir[0]='\0';
    if (argv0[0] != '/') {
        getcwd(daemon_dir, sizeof(daemon_dir));
        strcat(daemon_dir,"/");
    }
    strcat(daemon_dir, argv0);
    for(i=strlen(daemon_dir); i >= 0 && daemon_dir[i] != '/'; i--);


    daemon_dir[i++] = '\0';
    if(strncasecmp(&daemon_dir[i], "lim", 3) == 0) {
	daemonId = DAEMON_ID_LIM;
    } else if(strncasecmp(&daemon_dir[i], "res", 3) == 0) {
	daemonId = DAEMON_ID_RES;
    } else if(strncasecmp(&daemon_dir[i], "sbatchd", 7) == 0) {
	daemonId = DAEMON_ID_SBD;
    } else if(strncasecmp(&daemon_dir[i], "mbatchd", 7) == 0) {
	daemonId = DAEMON_ID_MBD;
    }

}

char *
getDaemonPath_(char *name, char *serverdir)
{
    static char daemonpath[MAXPATHLEN];

    strcpy(daemonpath, daemon_dir);
    strcat(daemonpath, name);
    if (access(daemonpath, X_OK) < 0) {
       ls_syslog(LOG_ERR,
		 _i18n_msg_get(ls_catd, NL_SETN, 5500,
			      "%s: Can't access %s: %s. Trying LSF_SERVERDIR."), /* catgets 5500 */
		 "getDaemonPath_", daemonpath, strerror(errno));

       strcpy(daemonpath, serverdir);
       strcat(daemonpath, name);

    }
    return(daemonpath);

}
