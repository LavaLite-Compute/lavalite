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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 *
 */
#include "lsf/lib/lib.h"

#define MAXLISTSIZE 256
#define NL_SETN 27

void
usage(const char *cmd)
{
    fprintf(stderr, "usage: %s [-h] [-V] [-L] [-R res_req] [-n needed]"
            "[-w wanted]\n [host_name ... ]\n", cmd);
}


int
main(int argc, char **argv)
{
    static char fname[] = "lsplace/main";
    char *resreq = NULL;
    char *hostnames[MAXLISTSIZE];
    char **desthosts;
    int cc = 0;
    int needed = 1;
    int wanted = 1;
    int i;
    char locality=false;
    extern int  optind, opterr;
    extern char *optarg;
    int achar;
    char badHost = false;

    _i18n_init ( I18N_CAT_MIN );

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
            locality=true;
            break;

        case 'R':
            resreq = optarg;
            break;

        case 'n':
            for (i = 0 ; optarg[i] ; i++)
                if (! isdigit(optarg[i]))
                    usage(argv[0]);
            needed = atoi(optarg);
            break;

        case 'w':
            for (i = 0 ; optarg[i] ; i++)
                if (! isdigit(optarg[i]))
                    usage(argv[0]);
            wanted = atoi(optarg);
            break;

        case 'V':
            fputs(_LAVALITE_VERSION_, stderr);
            exit(0);

        case 'h':
        default:
            usage(argv[0]);
        }
    }

    for ( ; optind < argc ; optind++) {
        if (cc>=MAXLISTSIZE) {
            fprintf(stderr,
                    _i18n_msg_get(ls_catd,NL_SETN,2201, "%s: too many hosts specified (max %d)\n"), /* catgets  2201  */
                    argv[0], MAXLISTSIZE);
            exit(-1);
        }

        if (ls_isclustername(argv[optind]) <= 0 &&
            !isValidHost_(argv[optind])) {
            fprintf(stderr, "%s: %s %s\n",
                    argv[0],
                    I18N(1953, "invalid hostname"), /* catgets 1953 */
                    argv[optind]);
            badHost = true;
            continue;
        }
        hostnames[cc] = argv[optind];
        cc++;
    }
    if (cc == 0 && badHost)
        exit(-1);

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
        char i18nBuf[150];
        sprintf( i18nBuf,I18N_FUNC_FAIL,"lsplace","ls_placereq");
        ls_perror( i18nBuf );
        if (lserrno == LSE_BAD_EXP ||
            lserrno == LSE_UNKWN_RESNAME ||
            lserrno == LSE_UNKWN_RESVALUE)
            exit(-1);
        else
            exit(1);
    }

    if (wanted < needed) {

        char i18nBuf[150];
        sprintf( i18nBuf,I18N_FUNC_FAIL,"lsplace","ls_placereq");
        fputs( i18nBuf, stderr );
        fputs(ls_errmsg[LSE_NO_HOST], stderr);
        putc('\n', stderr);
        exit(1);
    }

    for (cc=0; cc < wanted; cc++)
        printf("%s ", desthosts[cc]);
    printf("\n");

    _i18n_end ( ls_catd );

    exit(0);
}
