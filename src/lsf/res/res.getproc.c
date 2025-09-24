/* $Id: res.getproc.c,v 1.3 2007/08/15 22:18:55 tmizan Exp $
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
#include "lsf/res/res.h"

extern int FCLOSEUP(FILE **);

int
getPPSGids_(int pid, int *ppid, int *sid, int *pgid)
{
    FILE *fp;
    char procPath[128], c;
    int i, pp, sd, pg;

    if (pid < 0) {
        lserrno = LSE_BAD_ARGS;
        return -1;
    }
    strcpy(procPath, "/proc/");
    sprintf(&procPath[6], "%d", (int) pid);
    if (access(procPath, R_OK|F_OK)) {
        lserrno = LSE_FILE_SYS;
        return -1;
    }
    strcat(procPath, "/stat");
    if ((fp = fopen(procPath, "r")) == NULL) {
        lserrno = LSE_NO_FILE;
        return -1;
    }
    fscanf(fp, "%d %s %c %d %d %d", &i, procPath, &c, &pp,&pg, &sd);
    FCLOSEUP(&fp);
    if (pid != i) {
        lserrno = LSE_MISC_SYS;
        return -1;
    }
    if (ppid)
        *ppid = (int) pp;
    if (sid)
        *sid = (int) sd;
    if (pgid)
        *pgid = (int) pg;
    return 0;
}
