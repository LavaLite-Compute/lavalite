/* $Id: cmd.prt.c,v 1.4 2007/08/15 22:18:44 tmizan Exp $
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

#include <pwd.h>
#include "cmd.h"
#include "../lib/lsb.h"

#include <netdb.h>
#include <ctype.h>

#define NL_SETN 8 	 
#define MINFIELDWIDE  7 	


typedef char loadCharType[MAXLSFNAMELEN];
static void fill_load(float *, loadCharType *, int *, int);
static void fillDefal (char *, char *, int, float);
static void prtLimit (int, char *, float);


int
getDispEnd(struct resItem* resTable, int start, int last)
{
    int endItem = start;
    int dispWidth = strlen(" loadSched");
    int feildWidth;
    if (resTable != NULL) {
        while (endItem < last) {
            feildWidth = strlen(resTable[endItem].name);
            feildWidth = (feildWidth > MINFIELDWIDE)? feildWidth : MINFIELDWIDE;
            dispWidth += (feildWidth + 1);  
            if (dispWidth > 80)
                break;
            else
                endItem++;
        }
    }
    return endItem;
} 

char* 
getPrtFmt(char* name)
{
    static char frmtStr[128];
    int fwidth;
    if (name != NULL && (fwidth = strlen(name)) != 0) {
        fwidth = (fwidth > MINFIELDWIDE)? fwidth : MINFIELDWIDE;
        sprintf(frmtStr, "%%%ds ", fwidth);
    } else
        strcpy(frmtStr, "%s ");

    return frmtStr;
} 

int
printThresholds (float *loadSched, float *loadStop, int *busySched,
		 int *busyStop, int nIdx, struct lsInfo *lsInfo)
{
    static loadCharType *loadSchedVal = NULL, *loadStopVal = NULL;
    static int nLoad = 0;
    int i;
    static char fName[] = "printThresholds";

    if (nIdx > nLoad) {
	if (loadSchedVal) {
	    FREEUP(loadSchedVal);
	    loadSchedVal = NULL;
	}
	
	if (loadStopVal) {
	    FREEUP(loadStopVal);
	    loadStopVal = NULL;
	}
	
	nLoad = 0;

	if ((loadSchedVal = (loadCharType *) calloc(nIdx, sizeof(loadCharType)))
	     == NULL) {
	    fprintf(stderr, I18N_FUNC_FAIL, fName, "calloc"); 
	    return (-1);
	}

	if ((loadStopVal = (loadCharType *) calloc(nIdx,
						   sizeof(loadCharType)))
	    == NULL) {
	    fprintf(stderr, I18N_FUNC_FAIL, fName, "calloc");
	    return (-1);
	}
	
	nLoad = nIdx;
    }

    printf("           r15s   r1m  r15m   ut      pg    io   ls    it    tmp    swp    mem\n");
    fill_load(loadSched, loadSchedVal, busySched, nIdx);
    printf(" loadSched");
    for (i = 0; i <= MEM; i++)
	 printf("%s", loadSchedVal[i]);

    if (loadStop != NULL) {
        printf("\n loadStop ");
	fill_load(loadStop, loadStopVal, busyStop, nIdx);
	for (i = 0; i <= MEM; i++)
	    printf("%s", loadStopVal[i]);
    }
    printf("\n");

    if (nIdx > MEM + 1) {
        int start = MEM + 1;
        int end = 0;
        char* frmt = NULL;
        
        while ((end = getDispEnd(lsInfo->resTable, start, nIdx)) > start && start < nIdx) {
            printf("\n          ");
            for (i = start; i < end; i++) {
                frmt = getPrtFmt(lsInfo->resTable[i].name);
                printf(frmt, lsInfo->resTable[i].name);
            }

            printf("\n loadSched");
            for (i = start; i < end; i++) {
                frmt = getPrtFmt(lsInfo->resTable[i].name);
                printf(frmt, loadSchedVal[i]);
            }
            printf("\n loadStop ");
            for (i = start; i < end; i++) {
                frmt = getPrtFmt(lsInfo->resTable[i].name);
                printf(frmt, loadStopVal[i]);
            }
            printf("\n");
            if (end > start)
                start = end;
            else
                break;
		}
    }
    return (0);
} 

static void
fill_load(float *load, loadCharType *loadval, int *busyBit, int nIdx)
{
    int j;
    int i=1;

    for (j=0;j<UT;j++) {         
        if (busyBit && (busyBit[0] & i))
            sprintf(loadval[j], "*%4.1f ", load[j]);
        else
            fillDefal (loadval[j], "%5.1f ", j, load[j]);
        i = i << 1;
     }
    if (busyBit && (busyBit[0] & HOST_BUSY_UT))
        sprintf(loadval[UT], "*%2.1f ", load[UT]);
    else
        fillDefal (loadval[UT], " %3.1f ", UT, load[UT]);
    if (busyBit && (busyBit[0] & HOST_BUSY_PG))
        sprintf(loadval[PG], "*%6.1f ", load[PG]);
    else
        fillDefal (loadval[PG], "%7.1f ", PG, load[PG]);
    if (busyBit && (busyBit[0] & HOST_BUSY_IO))
        sprintf(loadval[IO], "*%4.0f ", load[IO]);
    else
        fillDefal (loadval[IO], "%5.0f ", IO, load[IO]);
    if (busyBit && (busyBit[0] & HOST_BUSY_LS))
        sprintf(loadval[LS], "* %2.0f ", load[LS]);
    else
        fillDefal (loadval[LS], " %3.0f ", LS, load[LS]);
    if (busyBit && (busyBit[0] & HOST_BUSY_IT))
        sprintf(loadval[IT], "*%4.0f ", load[IT]);
    else
        fillDefal (loadval[IT], "%5.0f ", IT, load[IT]);
    if (busyBit && (busyBit[0] & HOST_BUSY_SWP))
        sprintf(loadval[SWP], "*%4.0fM ", load[SWP]);
    else
        fillDefal (loadval[SWP], "%5.0fM ", SWP, load[SWP]);
    if (busyBit && (busyBit[0] & HOST_BUSY_MEM))
        sprintf(loadval[MEM], "*%4.0fM ", load[MEM]);
    else
        fillDefal (loadval[MEM], "%5.0fM ", MEM, load[MEM]);
    if (busyBit && (busyBit[0] & HOST_BUSY_TMP))
        sprintf(loadval[TMP], "*%4.0fM ", load[TMP]);
    else
        fillDefal (loadval[TMP], "%5.0fM ", TMP, load[TMP]);

    for (j = MEM + 1; j < nIdx; j++) {
        if (busyBit && LSB_ISBUSYON (busyBit, j))
            sprintf(loadval[j], "*%6.1f", load[j]);
        else
            fillDefal(loadval[j], "%6.1f", j, load[j]);
    }

    
    for (j=0;j<nIdx;j++) {
        char *sp;
        if (loadval[j][0] != '*')
            continue;
        sp = loadval[j];
        for (sp=sp+1; *sp==' '; sp++) {
            *(sp-1) = ' ';
            *sp = '*';
        }
    }
} 


static void
fillDefal (char *loadval, char *string, int num, float load)
{
    if ((load < INFINIT_LOAD) && (load > -INFINIT_LOAD))
        sprintf(loadval, string, load);
    else {
        switch (num) {
          case UT:
          case LS:
            sprintf(loadval, "   - ");
            break;
          case R15S:
          case R1M:
          case R15M:
            sprintf(loadval, "   -  ");
            break;
          case IT:
          case IO:
            sprintf(loadval, "    - ");
            break;
          case SWP:
          case MEM:
          case TMP:
            sprintf(loadval, "    -  ");
            break;
          case PG:
            sprintf(loadval, "      - ");
            break;
          default:
	    sprintf(loadval, "     - ");
            break;
        }
    }
} 

void
prtResourceLimit (int *rLimits, char *hostSpec, float hostFactor, int *procLimits)
{
    int limit, i;

    
    limit = FALSE;
    if (rLimits[LSF_RLIMIT_CPU] >= 0) {
        if (!limit)
            printf("\n");
        printf(" %-24s", 
	    (_i18n_msg_get(ls_catd,NL_SETN,1402, "CPULIMIT"))); /* catgets  1402  */
        limit = TRUE;
    }
    if (rLimits[LSF_RLIMIT_RUN] >= 0) {
        if (!limit)
            printf("\n");
        printf(" %-24s", 
	    (_i18n_msg_get(ls_catd,NL_SETN,1403, "RUNLIMIT"))); /* catgets  1403  */
        limit = TRUE;
    }
    if (procLimits != NULL) {
	if (procLimits[2] > 0 && procLimits[2] != INFINIT_INT) { 
	    
            if (!limit)
                printf("\n");
            printf(" %-24s", 
	        (_i18n_msg_get(ls_catd,NL_SETN,1404, "PROCLIMIT"))); /* catgets  1404  */
            limit = TRUE;
	}
    }
    if (limit)
        printf ("\n");                       

    if (rLimits[LSF_RLIMIT_CPU] >= 0) 
         prtLimit (rLimits[LSF_RLIMIT_CPU], hostSpec, hostFactor);
    if (rLimits[LSF_RLIMIT_RUN] >= 0) {
	
        prtLimit (rLimits[LSF_RLIMIT_RUN], hostSpec, hostFactor);
    }
    if (procLimits != NULL) {
	if (procLimits[0] == 1 && procLimits[1] == 1 && procLimits[2] != INFINIT_INT) { 
	    
	    printf (" %-d", procLimits[2]);
	} else {
	    for (i=0; i<3; i++) {
	        if ((procLimits[i] >0) && (procLimits[i] != INFINIT_INT)) 
	            printf (" %-d", procLimits[i]);
	    }
	}
    }
    if (limit)
        printf ("\n");                       

    
    limit = FALSE;
    if (rLimits[LSF_RLIMIT_FSIZE] > 0) {
        if (!limit)
            printf("\n");
        printf(" %s", 
	    (_i18n_msg_get(ls_catd,NL_SETN,1405, "FILELIMIT"))); /* catgets  1405  */
        limit = TRUE;
    }
    if (rLimits[LSF_RLIMIT_DATA] > 0) {
        if (!limit)
            printf("\n");
        printf(" %s", 
	    (_i18n_msg_get(ls_catd,NL_SETN,1406, "DATALIMIT"))); /* catgets  1406  */
        limit = TRUE;
    }
    if (rLimits[LSF_RLIMIT_STACK] > 0) {
        if (!limit)
            printf("\n");
        printf(" %s", 
	    (_i18n_msg_get(ls_catd,NL_SETN,1407, "STACKLIMIT"))); /* catgets  1407  */
        limit = TRUE;
    }
    if (rLimits[LSF_RLIMIT_CORE] >= 0) {
        if (!limit)
            printf("\n");
        printf(" %s", 
	    (_i18n_msg_get(ls_catd,NL_SETN,1408, "CORELIMIT"))); /* catgets  1408  */
        limit = TRUE;
    }
    if (rLimits[LSF_RLIMIT_RSS] > 0) {
        if (!limit)
            printf("\n");
        printf(" %s", 
	    (_i18n_msg_get(ls_catd,NL_SETN,1409, "MEMLIMIT"))); /* catgets  1409  */
        limit = TRUE;
    }

    if (rLimits[LSF_RLIMIT_SWAP] > 0) {
        if (!limit)
            printf("\n");
        printf(" %s", 
	    (_i18n_msg_get(ls_catd,NL_SETN,1410, "SWAPLIMIT"))); /* catgets  1410  */
        limit = TRUE;
    }

    if (rLimits[LSF_RLIMIT_PROCESS] > 0) {
        if (!limit)
            printf("\n");
        printf(" %s", 
	    (_i18n_msg_get(ls_catd,NL_SETN,1411, "PROCESSLIMIT"))); /* catgets  1411  */
        limit = TRUE;
    }


    if (limit)
        printf ("\n");                        

    if (rLimits[LSF_RLIMIT_FSIZE] > 0)
        printf(" %6d K ", rLimits[LSF_RLIMIT_FSIZE]);
    if (rLimits[LSF_RLIMIT_DATA] > 0)
        printf(" %6d K ", rLimits[LSF_RLIMIT_DATA]);
    if (rLimits[LSF_RLIMIT_STACK] > 0)
        printf(" %6d K  ", rLimits[LSF_RLIMIT_STACK]);
    if (rLimits[LSF_RLIMIT_CORE] >= 0)
        printf(" %6d K ", rLimits[LSF_RLIMIT_CORE]);
    if (rLimits[LSF_RLIMIT_RSS] > 0)
        printf(" %6d K", rLimits[LSF_RLIMIT_RSS]);
    if (rLimits[LSF_RLIMIT_SWAP] > 0)
        printf(" %6d K ", rLimits[LSF_RLIMIT_SWAP]);
    if (rLimits[LSF_RLIMIT_PROCESS] > 0)
        printf(" %6d      ", rLimits[LSF_RLIMIT_PROCESS]);
    if (limit)
        printf ("\n");                         

} 

static void
prtLimit (int rLimit, char *hostSpec, float hostFactor)
{
    float norCpuLimit;
    char str[50];
    
    memset(str,' ',50);
    if (hostFactor != 0.0) {
        norCpuLimit =  rLimit/(hostFactor * 60.0);
	if (hostSpec != NULL)
        {
            sprintf(str," %-.1f min of %s", norCpuLimit, hostSpec);
	    str[strlen(str)]=' ';
            str[25]='\0';
        }
        else
	{
	    sprintf(str," %-.1f min", norCpuLimit);
	    str[strlen(str)]=' ';
            str[25]='\0';
	}
	printf("%s",str);
    }
} 

