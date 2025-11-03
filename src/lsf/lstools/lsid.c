/* $Id: lsid.c,v 1.5 2007/08/15 22:18:55 tmizan Exp $
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
#include "lsf/lib/lib.h"

static void usage(char *);
extern int errLineNum_;

static void
usage(char *cmd)
{
    fprintf(stderr, "Usage: %s [-h] [-V]\n", cmd);
    exit(-1);
}

int
main(int argc, char **argv)
{
    char *Name;
    int achar;

    if (ls_initdebug(argv[0]) < 0) {
        ls_perror("ls_initdebug");
        exit(-1);
    }

    while ((achar = getopt(argc, argv, "hV")) != EOF) {
        switch(achar) {
        case 'h':
            usage(argv[0]);
            exit(0);
        case 'V':
            fprintf(stderr, "%s\n", LAVALITE_VERSION_STR);
            return 0;
        default:
            usage(argv[0]);
            exit(-1);
        }
    }
    puts(LAVALITE_VERSION_STR);

    TIMEIT(0, (Name = ls_getclustername()), "ls_getclustername");
    if (Name == NULL) {
        if (lserrno == LSE_CONF_SYNTAX) {
            char lno[20];
            sprintf (lno, "Line %d", errLineNum_);
            ls_perror(lno);
        } else
        {
            char buf[150];
            
    sprintf(buf, "%s: %s failed", "lsid", "ls_getclustername")
;
            ls_perror( buf );
        }
        exit(-1);
    }
    printf("My cluster name is %s\n", Name);

    TIMEIT(0, (Name = ls_getmastername()), "ls_getmastername");
    if (Name == NULL) {
        char buf[150];
        
    sprintf(buf, "%s: %s failed", "lsid", "ls_getmastername")
;
        ls_perror( buf );
        exit(-1);
    }
    printf("My master name is %s\n", Name);
    exit(0);
}
