/* $Id: res.pty.c,v 1.6 2007/08/15 22:18:56 tmizan Exp $
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


#include "lsf/res/res.h"
#define NL_SETN     29

int grantpt(int);
int unlockpt(int);
char *ptsname(int);
static int letterInd = 0;
static int digitInd = 0;

void
ptyreset(void)
{
    letterInd = 0;
    digitInd  = 0;
}


int
ptymaster(char *line)
{
    static char fname[] = "ptymaster()";
    int master_fd;
    int ptyno;
    char *slave;


    master_fd = open("/dev/ptmx", O_RDWR);
    if (master_fd < 0) {
        ls_syslog(LOG_ERR, I18N_FUNC_S_FAIL_M, fname, "open", "/dev/ptmx");
        return -1;
    }
    if (grantpt(master_fd) < 0) {
        ls_syslog(LOG_ERR, I18N_FUNC_D_FAIL_M, fname, "grantpt",
                master_fd);
        close(master_fd);
        return -1;
    }

    if (unlockpt(master_fd) < 0) {
        ls_syslog(LOG_ERR, I18N_FUNC_D_FAIL_M, fname, "unlockpt",
                master_fd);
        close(master_fd);
    }
    if (ioctl(master_fd, TIOCGPTN, &ptyno) != 0) {
        ls_syslog(LOG_DEBUG, I18N_FUNC_FAIL_M, fname, "ioctl(TIOCGPTN)");
        slave = ptsname(master_fd);
        if (slave == NULL) {
            ls_syslog(LOG_ERR, I18N_FUNC_FAIL_M, fname, "ptsname");
            close(master_fd);
            return -1;
        }
        strcpy(line,slave);
    } else {
        sprintf(line, "/dev/pts/%d", ptyno);
    }

    return master_fd;
}

int
ptyslave(char *tty_name)
{
    int slave;


    slave = open(tty_name, O_RDWR);

    if (slave < 0) {
        return -1;
    }

    return slave;
}



char *pty_translate(char *pty_name)
{
    static char tmp[11] = "/dev/ttyXX";
    int n;

    n = strlen(pty_name);

    tmp[8] = pty_name[n-2];
    tmp[9] = pty_name[n-1];

    if (debug > 1)
        printf("%s -> %s\n", pty_name, tmp);


    return tmp;
}



int check_valid_tty(char *tty_name)
{
    int i;
    char valid_name[9] = "/dev/tty";

    for (i=0; i<8; i++)
        if (tty_name[i] != valid_name[i])
            return 0;
    return 1;
}
