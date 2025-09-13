/* $Id: cmd.jobid.c,v 1.4 2007/08/15 22:18:44 tmizan Exp $
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
#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include "../../lsf/intlib/jidx.h"
#include "../../lsf/lib/lib.h"
#include "../../lsf/intlib/intlibout.h"

#define NL_SETN 8 	


extern int optind;
extern char *optarg;

extern char *yybuff; 
static void freeIdxList(struct idxList *);
static struct idxList * parseJobArrayIndex(char *);
static int getJobIdIndexList (char *, int *, struct idxList **, int);

int
getJobIds (int argc, char **argv, char *jobName, char *user, char *queue,
           char *host, LS_LONG_INT **jobIds0, int extOption)
{
    int numJobIds = 0;                                 
    int options = LAST_JOB;                     
    struct jobInfoHead *jobInfoHead;

    if (extOption) {
        options = extOption;
    }

    numJobIds = getSpecJobIds (argc, argv, jobIds0, &options);

    
    if (extOption & ZOMBIE_JOB) {
	options |= ZOMBIE_JOB;
    }

    if (numJobIds != 0)
        return (numJobIds);                    

    if (strstr(argv[0], "bmig")) {
     
	options &= ~CUR_JOB;
	options |= (RUN_JOB | SUSP_JOB);
    }

    if (strstr(argv[0], "brequeue")){
       options = 0;
       options = extOption;
    }

    options |= JOBID_ONLY;

    
    if (options & DONE_JOB)
    {
            options &= ~JOBID_ONLY;
    }

    TIMEIT(0, (jobInfoHead = lsb_openjobinfo_a ((LS_LONG_INT)0, jobName, user, queue, 
                                        host, options)), "lsb_openjobinfo");
    if (jobInfoHead == NULL) {
	jobInfoErr (0, jobName, user, queue, host, options);
	exit(-1);
    }

    TIMEIT(0, lsb_closejobinfo(), "lsb_closejobinfo");

    *jobIds0 = jobInfoHead->jobIds;
    return (jobInfoHead->numJobs);

} 

int
getSpecJobIds (int argc, char **argv, LS_LONG_INT **jobIds0, int *options)
{
    int numJobIds = 0;                
    static LS_LONG_INT *jobIds = NULL;
    int i;
    int j;
    LS_LONG_INT *temp;
    int jobId;
    LS_LONG_INT lsbJobId;
    int sizeOfJobIdArray = MAX_JOB_IDS;
    struct idxList *idxListP = NULL, *idx;
    static char fName[] = "getSpecJobIds";

    if (jobIds)
        free (jobIds);               

    if ( argc < optind + 1 ) {      
        *jobIds0 = NULL;
        return (0);
    }

    if ((jobIds = (LS_LONG_INT *) calloc (MAX_JOB_IDS, sizeof (LS_LONG_INT))) == NULL) {
        perror("calloc"); 
        exit(-1);
    }

    for ( ; argc>optind; optind++) {

	
        if (getJobIdIndexList (argv[optind], &jobId, &idxListP,0)) {
            exit(-1);
	}

	
	if (idxListP == NULL) {

            if (numJobIds >= sizeOfJobIdArray) {
	        sizeOfJobIdArray += MAX_JOB_IDS;
	        if ((jobIds = realloc(jobIds, sizeOfJobIdArray*sizeof(LS_LONG_INT))) == NULL) {
               	    fprintf(stderr, I18N_FUNC_FAIL, fName, "malloc"); 
               	    exit(-1);
	        }
            }

	    
            if (jobId == 0 ) {
                if (options)
		    *options = CUR_JOB;
                numJobIds = 0;            
                break;
            }
            for (i = 0; i < numJobIds; i++)  
                if (jobId == jobIds[i])
                    break;
            if (i == numJobIds) {
                jobIds[numJobIds] = jobId;
	        numJobIds++;
	    }
	    continue;
	}

	
	for (idx = idxListP; idx; idx = idx->next) {
	    for(j = idx->start; j <= idx->end; j+= idx->step) {
    		lsbJobId = LSB_JOBID(jobId, j);
                if (numJobIds >= sizeOfJobIdArray) {
	            sizeOfJobIdArray += MAX_JOB_IDS;
	            if ((temp = (LS_LONG_INT *) realloc(jobIds, 
			sizeOfJobIdArray * sizeof(LS_LONG_INT))) == NULL) {
               	        fprintf(stderr, I18N_FUNC_FAIL, fName, "malloc");
               	        exit(-1);
	            }
		    jobIds = temp;
                }
                for (i = 0; i < numJobIds; i++)  
                    if (lsbJobId == jobIds[i])
                        break;
                if (i == numJobIds) {
                    jobIds[numJobIds] = lsbJobId;
	            numJobIds++;
	        }
	    }
	}
        freeIdxList(idxListP);
    }
    *jobIds0 = jobIds;
    return (numJobIds);

} 

int
getSpecIdxs (char *jobName, int **idxs0)
{
    int numIdxs = 0;
    int *idxs, *temp;
    struct idxList *idxList = NULL, *idx = NULL;
    int sizeOfJobIdArray = MAX_JOB_IDS;
    int i, j;
    static char fName[] = "getSpecIdxs";

    
    if ((idxList = parseJobArrayIndex(jobName)) == NULL) {
        *idxs0= NULL;
        return(0);
    }
    if ((idxs = (int *) calloc (MAX_JOB_IDS, sizeof (int))) == NULL) {
        perror("calloc"); 
        exit(-1);
    }
    
    for (idx = idxList; idx; idx = idx->next) {
        for(j = idx->start; j <= idx->end; j+= idx->step) {
            
            if (numIdxs >= sizeOfJobIdArray) {
                sizeOfJobIdArray += MAX_JOB_IDS;
                if ((temp = (int *) realloc(idxs,
                    sizeOfJobIdArray * sizeof(int))) == NULL) {
                    fprintf(stderr, I18N_FUNC_FAIL, fName, "malloc"); 
                    exit(-1);
                }
                idxs = temp;
            }
            for (i = 0; i < numIdxs; i++)  
                if (j == idxs[i])
                    break;

            
            if (i == numIdxs)
                idxs[numIdxs++] = j;
        }
    }
    *idxs0 = idxs;
    return (numIdxs);
} 

int
getOneJobId (char *string, LS_LONG_INT *outJobId, int options)
{
    int     jobId = 0;
    struct idxList *idxListP = NULL;

    
    if (getJobIdIndexList (string, &jobId, &idxListP, 0)) {
        return(-1);
    }

    if (jobId == 0) {
	fprintf(stderr, "%s.\n", 
		(_i18n_msg_get(ls_catd,NL_SETN,1006, "Job Id = 0 is out of valid range"))); /* catgets  1006  */
	freeIdxList(idxListP);
        return(-1);
    }

    
    if (idxListP == NULL) {
	*outJobId = jobId;
	return(0);
    }

    
    if ((idxListP->next != NULL) |
	(idxListP->start != idxListP->end) |
	(idxListP->step != 1)) {
	fprintf(stderr, "%s: %s.\n", string, 
		(_i18n_msg_get(ls_catd,NL_SETN,1007, "Illegal job array index. One element only"))); /* catgets  1007  */
	freeIdxList(idxListP);
	return(-1);
    }

    
    *outJobId = LSB_JOBID(jobId, idxListP->start);
    freeIdxList(idxListP);
    return (0);

} 


static int
getJobIdIndexList (char *string, int *outJobId, struct idxList **idxListP,
int options)
{
    int     jobId = 0;
    char    *startP;
    static  char jobIdStr[16];
    int     jobIdLen;
    char    inJobIdStr[MAXLINELEN];

    *idxListP = NULL;

     
    strcpy (inJobIdStr, string);
    if ((startP = strchr(string, '[')) == NULL) {
        if (!isint_(string)) {
	    if ( isdigitstr_(string) && islongint_(string) ) {
		LS_LONG_INT interJobId = 0;
		if ((interJobId = atoi64_(string)) > 0) {
		    strcpy(inJobIdStr, lsb_jobid2str(interJobId));
		}
	    }
	}
    }
    
    
    if ((startP = strchr(inJobIdStr, '[')) == NULL) {

        
        if (!isint_(inJobIdStr)) {
	    fprintf(stderr, (_i18n_msg_get(ls_catd,NL_SETN,1008, "%s: Illegal job ID.\n")), string); /* catgets  1008  */
	    return(-1);
	}

	
	if ((jobId = atoi(inJobIdStr)) < 0) {
	    fprintf(stderr, (_i18n_msg_get(ls_catd,NL_SETN,1008, "%s: Illegal job ID.\n")), string);
	    return(-1);
	}
	*outJobId = jobId;
	return(0);
    }

    
    if ((jobIdLen = (int) (startP - inJobIdStr)) >= 16) {
	fprintf(stderr, (_i18n_msg_get(ls_catd,NL_SETN,1010, "Job Id (%s) is too long.\n")), string); /* catgets  1010  */
        return(-1);
    }
    STRNCPY(jobIdStr, inJobIdStr, jobIdLen+1);

    
    if (!isint_(jobIdStr)) {
	fprintf(stderr, (_i18n_msg_get(ls_catd,NL_SETN,1008, "%s: Illegal job ID.\n")), string); 
        return(-1);
    }
    jobId = atoi(jobIdStr);
    if ((jobId <= 0) || (jobId > LSB_MAX_ARRAY_JOBID))  {
	fprintf(stderr, (_i18n_msg_get(ls_catd,NL_SETN,1012, "%s: Job ID out of valid range.\n")), string); /* catgets  1012  */
        return(-1);
    }
    *outJobId = jobId;

    
    if ((*idxListP = parseJobArrayIndex(inJobIdStr)) == NULL) {
        return(-1);
    }
    return (0);

} 

static void
freeIdxList(struct idxList *idxList)
{
    struct idxList *ptr;

    while(idxList) {
         ptr = idxList->next;
         FREEUP(idxList);
         idxList = ptr;
    }
    return;
} 

 
static struct idxList *
parseJobArrayIndex(char *jobName)
{
    struct idxList *idxList = NULL, *idx;
    char   *index;
    int    maxJLimit;
    static char fName[] = "parseJobArrayIndex";

    index = strchr(jobName, '[');
    if (!index)
        return(NULL);
    yybuff = index;
    if (idxparse(&idxList, &maxJLimit)) {
        freeIdxList(idxList);
        if (idxerrno == IDX_MEM) 
	    fprintf(stderr, I18N_FUNC_FAIL, fName, "malloc"); 
        else
	    fprintf(stderr, (_i18n_msg_get(ls_catd,NL_SETN,1014, "%s: Bad job array index list.\n")), jobName); /* catgets  1014  */
        return(NULL);
    }    

    
    for (idx = idxList; idx; idx = idx->next) {
	if (idx->end == INFINIT_INT)
	    idx->end = LSB_MAX_ARRAY_IDX;
	 
        if (idx->start > idx->end)  {
	    fprintf(stderr, "%d-%d: %s.\n",
		    idx->start, idx->end, 
		    I18N(1015, "Job Array index invalid range"));/* catgets 1015 */
            freeIdxList(idxList);
            return(NULL);
        }
        if ((idx->start <= 0) || (idx->start > LSB_MAX_ARRAY_IDX))  {
	    fprintf(stderr, "%d: %s.\n", 
		   idx->start,
		   I18N(1016, "Job Array index out of valid range")); /* catgets  1016  */
            freeIdxList(idxList);
            return(NULL);
        }
        if ((idx->end <= 0) || (idx->end > LSB_MAX_ARRAY_IDX))  {
	    fprintf(stderr, "%d: %s.\n", 
		    idx->end,
		    I18N(1016, "Job Array index out of valid range"));
            freeIdxList(idxList);
            return(NULL);
        }
        if ((idx->step <= 0) || (idx->step > LSB_MAX_ARRAY_IDX))  {
	    fprintf(stderr, "%d: %s.\n", 
		    idx->step,
		    I18N(1018, "Job Array index step out of valid range"));  /* catgets  1018  */
            freeIdxList(idxList);
            return(NULL);
        }

    }
    return(idxList);
} 


int
getJobIdList (char *jobIdStr, LS_LONG_INT **jobIdList)
{
    int numJobIds = 0;                
    int jobId;
    LS_LONG_INT lsbJobId;
    LS_LONG_INT *temp, *jobIds;
    struct idxList *idxListP = NULL, *idx;
    int sizeOfJobIdArray = MAX_JOB_IDS;
    int i, j;

    if (getJobIdIndexList (jobIdStr, &jobId, &idxListP, 0)) {
        return(-1);
    }

    if (jobId <= 0)
        return(-1);

    if ((jobIds = (LS_LONG_INT *) calloc (MAX_JOB_IDS, sizeof (LS_LONG_INT))) == NULL) {
        return(-1);
    }
    
    if (idxListP == NULL) {
        jobIds[0] = jobId;
        numJobIds = 1;
        *jobIdList = jobIds;
        return(numJobIds);
    }
    
    for (idx = idxListP; idx; idx = idx->next) {
        for (j = idx->start; j <= idx->end; j+= idx->step) {
            lsbJobId = LSB_JOBID(jobId, j);
            if (numJobIds >= sizeOfJobIdArray) {
                sizeOfJobIdArray += MAX_JOB_IDS;
                if ((temp = (LS_LONG_INT *) realloc(jobIds,
                    sizeOfJobIdArray * sizeof(LS_LONG_INT))) == NULL) {
                    return(-1);
                }
                jobIds = temp;
            }
            for (i = 0; i < numJobIds; i++)  
                if (lsbJobId == jobIds[i])
                    break;
            if (i == numJobIds) {
                jobIds[numJobIds++] = lsbJobId;
            }
        }
    }
    freeIdxList(idxListP);

    *jobIdList = jobIds;
    return (numJobIds);

} 

