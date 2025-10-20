/* $Id: lib.cwd.c,v 1.4 2007/08/15 22:18:50 tmizan Exp $
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
#include "lsf/lib/lib.common.h"

/* Bug. This function is problematic.
 */
char *
mygetwd_(char *path)
{
    char *pwd;
    struct stat pwdstat;
    struct stat dotstat;
    static char temp_buff[PATH_MAX];

    pwd = getenv("PWD");
    if (pwd == NULL && (pwd = getenv("CWD")) == NULL) {
        if (getcwd(temp_buff, sizeof(temp_buff))) {
            strncpy(path, temp_buff, PATH_MAX);
            path[strlen(path) - 1] = 0;
            return path;
        } else
            return NULL;
    }

    if (stat(pwd, &pwdstat) == 0 && stat(".", &dotstat) == 0) {
        if (pwdstat.st_dev == dotstat.st_dev &&
            pwdstat.st_ino == dotstat.st_ino) {
            strcpy(path, pwd);
            return path;
        }
    }

    if (getcwd(temp_buff, sizeof(temp_buff))) {
        strncpy(path, temp_buff, PATH_MAX);
        path[PATH_MAX - 1] = 0;
        return path;
    } else
        return NULL;
}
