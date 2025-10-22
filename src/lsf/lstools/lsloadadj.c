/* $Id: lsloadadj.c,v 1.3 2007/08/15 22:18:55 tmizan Exp $
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

#define MAXLISTSIZE 256

static void usage(char *);

extern int  optind, opterr;
extern char *optarg;

int
main(int argc, char **argv)
{
    static char fname[] = "lsloadadj/main";
    char *resreq = NULL;
    struct placeInfo  placeadvice[MAXLISTSIZE];
    char *p, *hname;
    int cc = 0;
    int achar;

    if (ls_initdebug(argv[0]) < 0) {
        ls_perror("ls_initdebug");
        exit(-1);
    }
    if (logclass & (LC_TRACE))
        ls_syslog(LOG_DEBUG, "%s: Entering this routine...", fname);

    opterr = 0;
    while ((achar = getopt(argc, argv, "VhR:")) != EOF)
    {
        switch (achar)
        {
        case 'R':
            resreq = optarg;
            break;
        case 'V':
            fprintf(stderr, "%s\n", LAVALITE_VERSION_STR);
            return 0;
        case 'h':
        default:
            usage(argv[0]);
        }
    }

    for ( ; optind < argc ; optind++)
    {
        if (cc >= MAXLISTSIZE)
        {
            fprintf(stderr, "%s: too many hostnames (maximum %d\n",
                    fname, MAXLISTSIZE);
            usage(argv[0]);
        }

        p = strchr(argv[optind],':');
        if ( (p != NULL) && (*(p+1) != '\0') )  {
            *p++ = '\0';
            placeadvice[cc].numtask = atoi(p);
            if (errno == ERANGE) {
                fprintf(stderr, "%s: invalid format for number of components\n",
                        fname);
                usage(argv[0]);
            }
        } else {
            placeadvice[cc].numtask = 1;
        }

        if (!isValidHost_(argv[optind]))
        {
            fprintf(stderr, "%s: %s %s\n",
                    fname,  "invalid hostname",
                    argv[optind]);
            usage(argv[0]);
        }
        strcpy(placeadvice[cc++].hostName, argv[optind]);
    }

    if (cc == 0) {

        if ((hname = ls_getmyhostname()) == NULL) {
            ls_perror("ls_getmyhostname");
            exit(-1);
        }
        strcpy(placeadvice[0].hostName, hname);
        placeadvice[0].numtask = 1;
        cc = 1;
    }

    if (ls_loadadj(resreq, placeadvice, cc) < 0) {
        ls_perror("lsloadadj");
        exit(-1);
    } else
    exit(0);
}

static void usage(char *cmd)
{
    printf("%s: %s [-h] [-V] [-R res_req] [host_name[:num_task] host_name[:num_task] ...]\n",I18N_Usage, cmd);
    exit(-1);
}
