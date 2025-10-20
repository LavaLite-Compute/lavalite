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

#define NL_SETN  27

static void usage(char *);
extern int errLineNum_;

static void
usage(char *cmd)
{
    fprintf (stderr, "%s: %s [-h] [-V]\n", I18N_Usage, cmd);
    exit(-1);
}

int
main(int argc, char **argv)
{
    static char fname[] = "lsid:main";
    char *Name;
    int achar;

    _i18n_init ( I18N_CAT_MIN );

    if (ls_initdebug(argv[0]) < 0) {
        ls_perror("ls_initdebug");
        exit(-1);
    }
    if (logclass & (LC_TRACE))
        ls_syslog(LOG_DEBUG, "%s: Entering this routine...", fname);

    while ((achar = getopt(argc, argv, "hV")) != EOF) {
        switch(achar) {
        case 'h':
            usage(argv[0]);
            exit(0);
        case 'V':
            fputs(_LAVALITE_VERSION_, stderr);
            exit(0);
        default:
            usage(argv[0]);
            exit(-1);
        }
    }
    puts(_LAVALITE_VERSION_);

    TIMEIT(0, (Name = ls_getclustername()), "ls_getclustername");
    if (Name == NULL) {
        if (lserrno == LSE_CONF_SYNTAX) {
            char lno[20];
            sprintf (lno, _i18n_msg_get(ls_catd,NL_SETN,1701, "Line %d"), errLineNum_); /* catgets 1701 */
            ls_perror(lno);
        } else
        {
            char buf[150];
            sprintf( buf,I18N_FUNC_FAIL_NO_PERIOD,"lsid", "ls_getclustername");
            ls_perror( buf );
        }
        exit(-1);
    }
    printf(_i18n_msg_get(ls_catd,NL_SETN,1702, "My cluster name is %s\n"), Name); /* catgets 1702  */

    TIMEIT(0, (Name = ls_getmastername()), "ls_getmastername");
    if (Name == NULL) {
        char buf[150];
        sprintf ( buf, I18N_FUNC_FAIL_NO_PERIOD,"lsid","ls_getmastername");
        ls_perror( buf );
        exit(-1);
    }
    printf(_i18n_msg_get(ls_catd,NL_SETN,1703, "My master name is %s\n"), Name); /* catgets  1703 */

    _i18n_end ( ls_catd );

    exit(0);
}
