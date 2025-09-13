/* $Id: cmd.move.c,v 1.2 2007/08/15 22:18:44 tmizan Exp $
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

#include "cmd.h"

#define NL_SETN 8 	





void 
usage (char *cmd)
{
    fprintf(stderr, I18N_Usage);
    fprintf(stderr, ": %s [-h] [-V] jobId | \"jobId[index]\" [position]\n", cmd);
    exit(-1);
}

void
bmove (int argc, char **argv, int opCode)
{
    int position, reqPos;
    LS_LONG_INT jobId = 0;
    int achar;
    extern int optind, opterr;

    if (lsb_init(argv[0]) < 0) {
	lsb_perror("lsb_init");
	exit(-1);
    }

    

    opterr = 0;
    while((achar = getopt(argc, argv, "hV")) != EOF) {
	switch(achar) {
            case 'V':
		fputs(_LS_VERSION_, stderr);
		exit(0);
	    case 'h':
            default:
		usage(argv[0]);
        }
    }
    if (argc == optind) {
        fprintf(stderr, "%s.\n", 
	    (_i18n_msg_get(ls_catd,NL_SETN,852, "Job ID must be specified"))); /* catgets  852  */
        usage(argv[0]);
    }
    if (optind < argc-2) {
	fprintf(stderr, "%s.\n", 
	    (_i18n_msg_get(ls_catd,NL_SETN,853, "Command syntax error: too many arguments"))); /* catgets  853  */
	usage(argv[0]);
    }

    if (getOneJobId (argv[optind], &jobId, 0)) {
	usage(argv[0]);
    }

    position = 1;			
    if (optind == argc - 2) { 
	if (!isint_(argv[++optind]) || atoi(argv[optind]) <= 0) {
            fprintf(stderr, "%s: %s.\n", argv[optind],
		I18N(854, "Position value must be a positive integer")); /* catgets854*/
	    usage(argv[0]);
        }
	position = atoi(argv[optind]);
    }

    reqPos = position;
    if (lsb_movejob(jobId, &position, opCode) <0) {
	lsb_perror(lsb_jobid2str (jobId));
	exit(-1);
    }

    if (position != reqPos)
	fprintf(stderr, (_i18n_msg_get(ls_catd,NL_SETN,855, "Warning: position value <%d> is beyond movable range.\n")), /* catgets  855  */
	       reqPos);
    if (opCode == TO_TOP)
	fprintf(stderr, (_i18n_msg_get(ls_catd,NL_SETN,856, "Job <%s> has been moved to position %d from top.\n")),  /* catgets  856  */
	    lsb_jobid2str (jobId), position);
    else
	fprintf(stderr, (_i18n_msg_get(ls_catd,NL_SETN,857, "Job <%s> has been moved to position %d from bottom.\n")), /* catgets  857  */
	    lsb_jobid2str (jobId), position);

    exit(0);
} 
