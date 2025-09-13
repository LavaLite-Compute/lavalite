/* $Id: busers.c,v 1.2 2007/08/15 22:18:44 tmizan Exp $
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

#include <unistd.h>

#include "cmd.h"

#define NL_SETN 8	 

#define USR_NAME_LENGTH    18
#define USR_JL_P_LENGTH    5
#define USR_NJOBS_LENGTH   6
#define USR_PEND_LENGTH    6
#define USR_RUN_LENGTH     6
#define USR_SSUSP_LENGTH   6
#define USR_USUSP_LENGTH   6
#define USR_RSV_LENGTH     6
#define USR_MAX_LENGTH     6

static void display_users (struct userInfoEnt *, int);
static void sort_users (struct userInfoEnt *, int);

static char fomt[200];

void 
usage (char *cmd)
{
    fprintf(stderr, I18N_Usage);
    fprintf(stderr, ": %s [-h] [-V] [user_name ...] [all]\n", cmd);
    exit(-1);
}

int 
main (int argc, char **argv)
{
    extern int optind;
    int cc, numUsers;
    struct userInfoEnt  *usrInfo;
    char **users=NULL, **userPoint;
    int all = FALSE;
    int rc;

    rc = _i18n_init ( I18N_CAT_MIN );	


    if (lsb_init(argv[0]) < 0) {
	lsb_perror("lsb_init");
	exit(-1);
    }

    while ((cc = getopt(argc, argv, "Vh")) != EOF) {
        switch (cc) {
	case 'V':
	    fputs(_LS_VERSION_, stderr);
	    exit(0);
        case 'h':
        default:
            usage(argv[0]);
        }
    }
    numUsers = getNames (argc, argv, optind, &users, &all, "user");
    if (!all && !numUsers) {	 
        numUsers = 1;
        userPoint = NULL;
    } else 
        userPoint = users;
    if (!(usrInfo = lsb_userinfo (userPoint, &numUsers))) {
        if (lsberrno == LSBE_BAD_USER && userPoint) 
            lsb_perror(users[numUsers]);        
        else
            lsb_perror(NULL);
        exit (-1);
    }
    display_users(usrInfo, numUsers);
    _i18n_end ( ls_catd );			
    exit(0);

} 

static void
display_users (struct userInfoEnt *reply, int numReply)
{
    int i;
    int first = TRUE;
    char maxJobs[MAX_CHARLEN], procJobLimit[MAX_CHARLEN];

    if (first) {
	first = FALSE;

	if (lsbMode_ & LSB_MODE_BATCH) {
            prtWord(USR_NAME_LENGTH, I18N_USER_GROUP, 0);
            prtWord(USR_JL_P_LENGTH, I18N_JL_P, -1);
            prtWord(USR_MAX_LENGTH,  I18N_MAX, -1);
	}
	else
            prtWord(USR_NAME_LENGTH, I18N_USER, 0);

        prtWord(USR_NJOBS_LENGTH, I18N_NJOBS, -1);
        prtWord(USR_PEND_LENGTH,  I18N_PEND,  -1);
        prtWord(USR_RUN_LENGTH,   I18N_RUN,   -1);
        prtWord(USR_SSUSP_LENGTH, I18N_SSUSP, -1);
        prtWord(USR_USUSP_LENGTH, I18N_USUSP, -1);
        prtWord(USR_RSV_LENGTH,   I18N_RSV,   -1);
        printf("\n");
    }
    if (numReply > 1)                      
	sort_users (reply, numReply);      

    for (i = 0; i < numReply; i++) {

	
        if (reply[i].procJobLimit < INFINIT_FLOAT) {
            sprintf(fomt, "%%%d.1f ", USR_JL_P_LENGTH);
            sprintf (procJobLimit, fomt, reply[i].procJobLimit);
	}
        else 
             strcpy(procJobLimit, prtDash(USR_JL_P_LENGTH) );

	
        if (reply[i].maxJobs < INFINIT_INT)
            strcpy(maxJobs, prtValue(USR_MAX_LENGTH, reply[i].maxJobs) );
        else 
             strcpy(maxJobs, prtDash(USR_MAX_LENGTH) );
         
        prtWordL(USR_NAME_LENGTH, reply[i].user);

	if (lsbMode_ & LSB_MODE_BATCH) {
            sprintf(fomt, "%%%ds%%%ds", USR_JL_P_LENGTH,
                                        USR_MAX_LENGTH );
	    printf(fomt,
                   procJobLimit, maxJobs);
	};

	if (strcmp(reply[i].user, "default") == 0) {
            printf("%s%s%s%s%s%s\n",
                   prtDash(USR_NJOBS_LENGTH), prtDash(USR_PEND_LENGTH),
                   prtDash(USR_RUN_LENGTH),   prtDash(USR_SSUSP_LENGTH),
                   prtDash(USR_USUSP_LENGTH), prtDash(USR_RSV_LENGTH)
                   );
	}
	else if (reply[i].numJobs == -INFINIT_INT) {
            printf("%s%s%s%s%s%s\n",
                   prtDash(USR_NJOBS_LENGTH), prtDash(USR_PEND_LENGTH),
                   prtDash(USR_RUN_LENGTH),   prtDash(USR_SSUSP_LENGTH),
                   prtDash(USR_USUSP_LENGTH), prtDash(USR_RSV_LENGTH)
                   );
	}
        else {
        sprintf(fomt, "%%%dd %%%dd %%%dd %%%dd %%%dd %%%dd\n", 
                                                  USR_NJOBS_LENGTH,
                                                  USR_PEND_LENGTH,
                                                  USR_RUN_LENGTH,
                                                  USR_SSUSP_LENGTH,
                                                  USR_USUSP_LENGTH,
                                                  USR_RSV_LENGTH );
	printf(fomt, 
                reply[i].numJobs, reply[i].numPEND, reply[i].numRUN,
                reply[i].numSSUSP, reply[i].numUSUSP, reply[i].numRESERVE);
	};
    }

    

} 

static void
sort_users (struct userInfoEnt *users, int numUsers)
{
    int i, j, k;
    struct userInfoEnt temUser;

    for (k = numUsers / 2; k > 0; k /= 2) {
        for (i = k; i < numUsers; i++) {
            for (j = i-k; j >= 0; j -= k) {
                 if (strcmp(users[j].user, users[j+k].user) < 0)
                     break;
                 memcpy(&temUser, users+j, sizeof (struct userInfoEnt));
                 memcpy(users+j, users+j+k, sizeof (struct userInfoEnt));
                 memcpy(users+j+k, &temUser, sizeof (struct userInfoEnt));
            }
        }
    }
    return;

} 

