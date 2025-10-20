/* $Id: bmgroup.c,v 1.3 2007/08/15 22:18:43 tmizan Exp $
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

#include "lsbatch/cmd/cmd.h"

int wflag = 0;

static void prtGroups (struct groupInfoEnt *, int, int);

void
usage (char *cmd)
{
    fprintf(stderr, I18N_Usage);
    if (strstr(cmd, "bugroup") != NULL)
	fprintf(stderr, ": %s [-h] [-V] [-r] [-w] [-l] [group_name ...]\n", cmd);
    else
        fprintf(stderr, ": %s [-h] [-V] [-r] [-w] [group_name ...]\n", cmd);
    exit(-1);
}

int
main (int argc, char **argv)
{
    extern int             optind;
    int                    cc;
    int                    numGroups = 0;
    int			   enumGrp = 0;
    char**                 groups=NULL;
    char**                 groupPoint;
    struct groupInfoEnt*   grpInfo = NULL;
    int                    options = GRP_ALL;
    int                    all;

    if (lsb_init(argv[0]) < 0) {
	lsb_perror("lsb_init");
	exit(-1);
    }

    while ((cc = getopt(argc, argv, "Vhrlw")) != EOF) {
        switch (cc) {
        case 'r':
            options |= GRP_RECURSIVE;
            break;
	case 'V':
	    fputs(_LAVALITE_VERSION_, stderr);
	    exit(0);
	case 'l':
	    if (strstr(argv[0], "bmgroup") != NULL) {

		usage(argv[0]);
	    }
	    options |= GRP_SHARES;
	    break;
	case 'w':
	    wflag = 1;
	    break;

        case 'h':
        default:
            usage(argv[0]);
        }
    }
    numGroups = getNames (argc, argv, optind, &groups, &all, "group");
    enumGrp = numGroups;

    if (numGroups) {
        options &= ~GRP_ALL;
        groupPoint = groups;
    } else
        groupPoint = NULL;

    if (strstr(argv[0], "bugroup") != NULL) {
        options |= USER_GRP;
        grpInfo = lsb_usergrpinfo(groupPoint, &enumGrp, options);
    } else if (strstr(argv[0], "bmgroup") != NULL) {
	options |= HOST_GRP;
        grpInfo = lsb_hostgrpinfo(groupPoint, &enumGrp, options);
    }
    if (grpInfo == NULL) {
        if (lsberrno == LSBE_NO_USER_GROUP || lsberrno == LSBE_NO_HOST_GROUP ) {
            if (options & HOST_GRP)
                lsb_perror(("host group"));
            else
                lsb_perror(("user group"));
	    FREEUP (groups);
            exit(-1);
        }
        if (lsberrno == LSBE_BAD_GROUP && groups)

            lsb_perror (groups[enumGrp]);
        else
	    lsb_perror(NULL);
        FREEUP (groups);
	exit(-1);
    }

    if (numGroups != enumGrp && numGroups != 0 && lsberrno == LSBE_BAD_GROUP) {
	if (groups)

            lsb_perror (groups[enumGrp]);
        else
            lsb_perror(NULL);
        FREEUP (groups);
        exit(-1);
    }

    FREEUP (groups);
    prtGroups (grpInfo, enumGrp, options);
    exit (0);

}

static void
prtGroups (struct groupInfoEnt *grpInfo, int numReply, int options)
{
    int     i;
    int     j;
    int     strLen;
    char*   sp;
    char*   cp ;
    char*   save_sp;
    char*   save_sp1;
    char    word[256];
    char    gname[256];
    char    first = TRUE;

    if (!(options & GRP_SHARES)) {

	for (i = 0; i < numReply; i++) {
	    sp = grpInfo[i].memberList;
	    strcpy(gname, grpInfo[i].group);
	    if (!wflag) {
	        sprintf(word,"%-12.12s", gname);
	    } else {
	        sprintf(word,"%-12s", gname);
	    }

	    strLen = strlen(gname);
	    for (j = 0; j < strLen ; j++)
		if (gname[j] == ' ') {
		    gname[j] = '\0';
		    break;
		}
	    if (first) {
    		char   *buf=NULL;

		buf = putstr_("GROUP_NAME");
		if(options & USER_GRP)
		    printf("%-12.12s  %s\n", buf, I18N_USERS);
		else
		    printf("%-12.12s  %s\n", buf, I18N_HOSTS);
		first = FALSE;
		FREEUP(buf);
	    }
	    printf("%s ", word);

	    if (strcmp (sp, "all") == 0) {
		if(options & USER_GRP) {
		    printf("%s\n", "all users");
		    continue;
		}
		else
		    printf("%s\n",
			"all hosts used by the batch system");
		continue;
	    }

	    while ((cp = getNextWord_(&sp)) != NULL) {
		save_sp = sp;
		strcpy(word, cp);
		printf("%s ", word);
		save_sp1 = sp;
		while ((cp = getNextWord_(&sp)) != NULL) {
		    if (strcmp(word, cp) == 0) {
			save_sp1 = strchr(save_sp1, *cp);
			for (j = 0; j < strlen(cp); j++)
			    *(save_sp1 + j) = ' ';
		    }
		    save_sp1 = sp;
		}
		sp = save_sp;
	    }
	    putchar('\n');
	}
    } else {
#define LEFTCOLSPACE	12
	char buf[LEFTCOLSPACE+2];

	for (i = 0; i < numReply; i++) {

	    sp = grpInfo[i].memberList;
	    strcpy(gname, grpInfo[i].group);
	    if (!wflag) {
	        sprintf(word,"%-12.12s", gname);
	    } else {
	        sprintf(word,"%-12s", gname);
	    }

	    strLen = strlen(gname);
	    for (j = 0; j < strLen ; j++) {
		if (gname[j] == ' ') {
		    gname[j] = '\0';
		    break;
		}
	    }
	    strncpy(buf, "GROUP_NAME",
		LEFTCOLSPACE-1);
	    buf[LEFTCOLSPACE-1]='\0';
	    strcat(buf, ":");
	    printf("%-12.12s %s\n", buf, word);

	    if (strcmp (sp, "all") == 0) {
	        strncpy(buf, I18N_USERS, LEFTCOLSPACE-1);
	        buf[LEFTCOLSPACE-1]='\0';
	        strcat(buf, ":");
	        printf("%-12.12s ", buf);
		printf("%s\n", "all users");
		goto PrintShares;
	    }

	    if (options & USER_GRP) {
	        strncpy(buf, I18N_USERS, LEFTCOLSPACE-1);
	    } else {
	        strncpy(buf, I18N_HOSTS, LEFTCOLSPACE-1);
	    }
	    buf[LEFTCOLSPACE-1]='\0';
	    strcat(buf, ":");
	    printf("%-12.12s ", buf);

	    while ((cp = getNextWord_(&sp)) != NULL) {
		sp = grpInfo[i].memberList;
		save_sp = sp;
		strcpy(word, cp);
		printf("%s ", word);
		save_sp1 = sp;
		while ((cp = getNextWord_(&sp)) != NULL) {
		    if (strcmp(word, cp) == 0) {
			save_sp1 = strchr(save_sp1, *cp);
			for (j = 0; j < strlen(cp); j++)
			    *(save_sp1 + j) = ' ';
		    }
		    save_sp1 = sp;
		}
		sp = save_sp;
	    }
	    putchar('\n');

	PrintShares:

	    if ((i + 1) < numReply)
		putchar('\n');

	}
    }

    return;
}
