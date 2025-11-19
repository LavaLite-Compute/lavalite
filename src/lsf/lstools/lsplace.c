/* $Id: lsplace.c,v 1.3 2007/08/15 22:18:55 tmizan Exp $
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 USA
 *
 */
#include "lsf/lib/lib.h"

void usage(const char *cmd)
{
    fprintf(stderr,
            "usage: %s [-h] [-V] [-L] [-R res_req] [-n needed]"
            "[-w wanted]\n [host_name ... ]\n",
            cmd);
}

int main(int argc, char **argv)
{
    static char fname[] = "lsplace/main";
    char *resreq = NULL;
    char *hostnames[LL_BUFSIZ_256];
    char **desthosts;
    int needed = 1;
    int wanted = 1;
    int i;
    char locality = false;
    extern int optind, opterr;
    extern char *optarg;
    int achar;

    if (ls_initdebug(argv[0]) < 0) {
        ls_perror("ls_initdebug");
        exit(-1);
    }
    if (logclass & (LC_TRACE))
        ls_syslog(LOG_DEBUG, "%s: Entering this routine...", fname);

    opterr = 0;
    while ((achar = getopt(argc, argv, "VR:Lhn:w:")) != EOF) {
        switch (achar) {
        case 'L':
            locality = true;
            break;

        case 'R':
            resreq = optarg;
            break;

        case 'n':
            for (i = 0; optarg[i]; i++)
                if (!isdigit(optarg[i]))
                    usage(argv[0]);
            needed = atoi(optarg);
            break;

        case 'w':
            for (i = 0; optarg[i]; i++)
                if (!isdigit(optarg[i]))
                    usage(argv[0]);
            wanted = atoi(optarg);
            break;

        case 'V':
            fprintf(stderr, "%s\n", LAVALITE_VERSION_STR);
            return 0;
        case 'h':
        default:
            usage(argv[0]);
        }
    }

    int cc = 0;
    for (; optind < argc; optind++) {
        if (!is_valid_host(argv[optind])) {
            fprintf(stderr, "%s: %s %s\n", argv[0], "invalid hostname",
                    argv[optind]);
            return -1;
        }
        hostnames[cc] = argv[optind];
        cc++;
    }

    if (needed == 0 || wanted == 0)
        wanted = 0;
    else if (needed > wanted)
        wanted = needed;

    if (wanted == needed)
        i = EXACT;
    else
        i = 0;

    i = i | DFT_FROMTYPE;

    if (locality)
        i = i | LOCALITY;

    if (cc == 0)
        desthosts = ls_placereq(resreq, &wanted, i, NULL);
    else
        desthosts = ls_placeofhosts(resreq, &wanted, i, 0, hostnames, cc);
    if (!desthosts) {
        ls_perror("ls_placereq() failed");
        return -1;
    }

    if (wanted < needed)
        printf("lsplace: got less hosts %d then requested\n", wanted);

    for (cc = 0; cc < wanted; cc++)
        printf("%s ", desthosts[cc]);
    printf("\n");

    return 0;
}
