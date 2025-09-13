/* $Id: mbd.dep.c,v 1.9 2007/08/15 22:18:45 tmizan Exp $
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

#include "daemons.h" 
#include "mbd.h"
#include <malloc.h>
#include <stdlib.h>

#define NL_SETN		10	

static struct jobIdx * createjobIdxRef (struct jobIdx *);
static void            destroyjobIdxRef(struct jobIdx *);


#define IS_QUOTE(x) ( (x) == '"' || (x) == '\'')
#define IS_VALIDCHAR(x) (isalnum (x) || (x) == '*' || (x) == '_' || \
	   (x) == '.' || (x) == ':' || (x) == '%' || (x) == ',' || \
	   (x) == '@' || (x) == '/' || (x) == '[' || (x) == ']' || \
           (x) == '-' )
#define IS_DELIM(x)  ( (x) == ',' || (x) == ' ')
#define MERGE_TOP  1
#define MERGE_LEFT 2 
#define MERGE_ALL  3

char preceTable[]={ 
	2,                          
	1,                          
	3,                          
 };

struct token {
    char *name;
    int   namelen;
    dptType type;
    int   checkCha;
} tokenTable[]={ 
	{"(", 	     1, DPT_LEFT_, FALSE},
	{")", 	     1, DPT_RIGHT_, FALSE},
	{"&&",	     2, DPT_AND, FALSE},
	{"||",	     2, DPT_OR, FALSE},
	{"exit",     4, DPT_EXIT, TRUE},
	{"started",  7, DPT_STARTED, TRUE},
	{"ended",    5, DPT_ENDED, TRUE},
        {"time",     4, DPT_WINDOW, TRUE},
	{"done",     4, DPT_DONE, TRUE},
	{"post_done",9, DPT_POST_DONE, TRUE},
	{"post_err", 8, DPT_POST_ERR, TRUE},
        {"numpend",  7, DPT_NUMPEND, TRUE},
	{"numhold",  7, DPT_NUMHOLD, TRUE},
        {"numrun",   6, DPT_NUMRUN, TRUE},
        {"numexit",  7, DPT_NUMEXIT,TRUE},
        {"numdone",  7, DPT_NUMDONE, TRUE},
        {"numstart", 8, DPT_NUMSTART, TRUE},
        {"numended", 8, DPT_NUMENDED, TRUE},
	{",",	1, DPT_COMMA, FALSE},
	{">=",	2, DPT_GE, FALSE},
	{"<=",	2, DPT_LE, FALSE},
	{"==",	2, DPT_EQ, FALSE},
	{"!=",	2, DPT_NE, FALSE},
	{"<",	1, DPT_LT, FALSE},
	{">",	1, DPT_GT, FALSE},
	{"!",	1, DPT_NOT, FALSE},
	{NULL ,0, DPT_DONE, FALSE}};

struct Stack {
    int top;
    int size;
    struct dptNode **nodes;
}; 


struct exceptNodeData {
   struct jData *jobRec;
   int  type;
};

static struct Stack *operatorStack;
static struct Stack *operandStack;

static char *getToken(char **,  dptType *);
static struct dptNode *newNode(dptType, void *);
static int mergeNode(int );

static struct Stack *initStackDep(void);
static int pushStackDep(struct Stack *, struct dptNode *);
static struct dptNode *popStackDep(struct Stack *);
static void freeStackDep(struct Stack *, int );

static struct jData **matchJobs(char *, char *, int *, int *, struct jobIdx **, LS_LONG_INT *, int *); 
static int createMoreNodes (dptType, int, struct jData **, struct jobIdx *,
                            struct dptNode **);
static int getCounterOfDep(int, int *);
static int evalJobDep(struct dptNode *, struct jData *, struct jData *);
static int opExpr(int, int, int);


struct dptNode * 
parseDepCond (char *dependCond, struct lsfAuth *auth, int *replyCode, char **badName, int *jFlags, int flags) 
{
    dptType tokenType, tempType;
    struct dptNode *rootNode = NULL;
    struct jData **jobRec = NULL;
    char *token, *tempToken;
    struct dptNode *node, *prev;
    struct jobIdx    *jobIdx;
    struct dptNode **nodeList = NULL;
    int numJob = 0;
    LS_LONG_INT notExistArrayJob = 0;
    int    opFlag = 0;

    
    operandStack = initStackDep();
    operatorStack = initStackDep();

#define PUSH_STACK(s, n) \
    {if (pushStackDep(s, n) < 0) {free(n); goto Error;}}

    *replyCode = LSBE_DEPEND_SYNTAX;
    while ((token=getToken(&dependCond, &tokenType)) != NULL) {
        
        jobIdx = NULL;

        switch(tokenType) {
            
            case DPT_AND:
            case DPT_OR:
            case DPT_NOT:
                if ((node=newNode(tokenType, NULL)) == NULL)
                   goto Error;
                if ((prev=popStackDep(operatorStack)) != NULL) {
                   if (preceTable[prev->type] > preceTable[node->type]) {
                      
                      PUSH_STACK(operatorStack, prev);	
                      if (mergeNode(MERGE_TOP) < 0)
                         goto Error;
                      PUSH_STACK(operatorStack, node);
                   } else {
                      
                      PUSH_STACK(operatorStack, prev);  
                      PUSH_STACK(operatorStack, node);  
                   }    
                } else 
                   PUSH_STACK(operatorStack, node)
                break;
            case DPT_LEFT_:
                if ((node=newNode(tokenType, NULL)) == NULL)
                   goto Error;
                PUSH_STACK(operatorStack, node)
                break;
            case DPT_RIGHT_:
	        
                 if (mergeNode(MERGE_LEFT) < 0)
                     goto Error;
                 break;
           case DPT_WINDOW:
           {
               struct timeWindow *timeW=NULL;
               int    windowLen=0;
               char   *tempCond;

               
               tempToken=getToken(&dependCond, &tempType);
               if (!tempToken || tempType != DPT_LEFT_)
                   goto Error;

               
               tempCond = dependCond;
               tempToken=getToken(&tempCond, &tempType);
               while (tempType != DPT_RIGHT_){
                   if (!tempToken || tempType != DPT_NAME)
                       goto Error;

                   windowLen += strlen(tempToken) + 1;
                   tempToken=getToken(&tempCond, &tempType);
               }

               
               if ((timeW = newTimeWindow()) == NULL)
                   goto Error;
               if ((timeW->windows = malloc(windowLen)) == NULL)
                   goto Error;
               *(timeW->windows) = '\0';

               
               tempToken=getToken(&dependCond, &tempType);
               while (tempType != DPT_RIGHT_){
                   if (!tempToken || tempType != DPT_NAME) {
                       freeTimeWindow (timeW);
                       goto Error;
                   }

                   
                   if (*(timeW->windows) != '\0')
                       strcat (timeW->windows, " ");
                   strcat (timeW->windows, tempToken);

                   if (addWindow(tempToken, timeW->week, "WINDOW_CONF") <0) {
                       freeTimeWindow (timeW);
                       goto Error;
                   }
                   tempToken=getToken(&dependCond, &tempType);

               }
               timeW->windEdge = now;        
               updateTimeWindow(timeW);

               if ((node=newNode(tokenType, (void *)timeW)) == NULL) {
                   freeTimeWindow (timeW);
                   goto Error;
               }
               PUSH_STACK(operandStack, node);

               break;
           }
           case DPT_DONE:	
           case DPT_POST_DONE:	
           case DPT_POST_ERR:	
           case DPT_EXIT:
           case DPT_STARTED:
           case DPT_ENDED:

		if (flags & WINDOW_CONF) 
		    goto Error;

               
               tempToken=getToken(&dependCond, &tempType);
               if (!tempToken || tempType != DPT_LEFT_)
                   goto Error;
               
               tempToken=getToken(&dependCond, &tempType);
               if (!tempToken)
                   goto Error;
               
               if ((jobRec=matchJobs(tempToken, 
				     auth->lsfUserName,
				     &numJob, 
				     replyCode, 
				     &jobIdx, 
				     &notExistArrayJob,
				     &opFlag)) == NULL) {
                   if (badName && (*badName) && !notExistArrayJob) {
		       STRNCPY ((*badName), tempToken, MAX_CMD_DESC_LEN);
                   } else  if (badName && (*badName) && notExistArrayJob) {
		       char jobIdStr[MAX_CMD_DESC_LEN];
		       sprintf(jobIdStr, "%d[%d]", LSB_ARRAY_JOBID(notExistArrayJob), LSB_ARRAY_IDX(notExistArrayJob));
		       STRNCPY ((*badName), jobIdStr, MAX_CMD_DESC_LEN);
		   }
                   if (mSchedStage != M_STAGE_REPLAY)
                       goto Error;    
               }

               if (numJob > 1) { 
                   nodeList = (struct dptNode **)
                              my_calloc(numJob, sizeof(struct dptNode *), 
                   "parseDepCond");
		   if (createMoreNodes (tokenType, numJob, jobRec, jobIdx, 
                                        nodeList) < 0)
		       goto Error;
                   node = nodeList[0];
               } else {
                   if (numJob > 0 && jobRec ) {
                       if ((node=newNode(tokenType, (void *)jobRec[0])) == NULL)
                           goto Error;
                       node->dptJobIdx = createjobIdxRef(jobIdx);

		       
		       if (opFlag == ARRAY_DEP_ONE_TO_ONE) {
			   (*node).dptUnion.job.opFlag = opFlag;
		       }
		       
		       opFlag = 0;
                   }
                   else { 
                       node = newNode(tokenType, NULL);
                       node->dptJobIdx = NULL;
                   }
                   PUSH_STACK(operandStack, node);
               }
               FREEUP (jobRec); 

               if ((tokenType == DPT_EXIT)) {
                   struct dptNode *tmpNode;
                   int i;

                   
                   
                   if (!(tempToken=getToken(&dependCond, &tempType)) ||
                        (tempType != DPT_RIGHT_ && tempType != DPT_COMMA) )
                       goto Error;
                   if (tempType == DPT_COMMA) {
                       
                       if (!(tempToken=getToken(&dependCond, &tempType)) ||
                           !((tempType >= DPT_GT && tempType <= DPT_NE) ||
                             tempType == DPT_NAME))
                           goto Error;
                       if (tempType >= DPT_GT && tempType <= DPT_NE) {
                          
                           node->dptUnion.job.opType = tempType;
                           if (!(tempToken=getToken(&dependCond, &tempType)) ||
                               tempType != DPT_NAME) 
                               goto Error;
                       }
                       else
                           node->dptUnion.job.opType = DPT_EQ;
                       
                       if (isint_(tempToken))   
                           node->dptUnion.job.exitCode = atoi(tempToken);
                       else
                           goto Error;
                       
                       tempToken=getToken(&dependCond, &tempType);
                       if (!tempToken || tempType != DPT_RIGHT_)
                           goto Error;
                   }
                   
                   for ( i = 1; i < numJob; i++) {
                      tmpNode = nodeList[i];
                      tmpNode->dptUnion.job.opType = node->dptUnion.job.opType;
                      tmpNode->dptUnion.job.exitCode = 
                                              node->dptUnion.job.exitCode;
                   }
	       } 
	       else {
                   
                   tempToken=getToken(&dependCond, &tempType);
                   if (!tempToken || tempType != DPT_RIGHT_)
                       goto Error;
               }
               FREEUP(nodeList);
               break;

           case DPT_NAME:
	       if (flags & WINDOW_CONF) 
	           goto Error;
               if ((jobRec=matchJobs(token, auth->lsfUserName, 
				     &numJob, replyCode, &jobIdx,  
				     &notExistArrayJob, NULL)) == NULL) {
                   if (badName && (*badName))
                      STRNCPY ((*badName), token, MAX_CMD_DESC_LEN);
                   if (mSchedStage != M_STAGE_REPLAY)
                       goto Error;
               }
	       if (numJob > 1) {
		   if (createMoreNodes (DPT_DONE, numJob, jobRec, jobIdx, NULL) < 0)
		       goto Error;
               } else {
                   if (numJob > 0 && jobRec ) {
                       if ((node=newNode(DPT_DONE, (void *)jobRec[0])) == NULL)
                           goto Error;
                       node->dptJobIdx = createjobIdxRef(jobIdx);
                   }
                   else { 
                       node = newNode(DPT_DONE, NULL);
                       node->dptJobIdx = NULL;
                   }
                   PUSH_STACK(operandStack, node);
               }
	       FREEUP (jobRec);
               break;
           case DPT_NUMPEND:
	   case DPT_NUMHOLD:
           case DPT_NUMRUN:
           case DPT_NUMEXIT:
           case DPT_NUMDONE:
           case DPT_NUMSTART: 
           case DPT_NUMENDED: {
                struct jData *jpbw = NULL;

               
               tempToken=getToken(&dependCond, &tempType);
               if (!tempToken || tempType != DPT_LEFT_)
                   goto Error;
               
               tempToken=getToken(&dependCond, &tempType);
               if (!tempToken)
                   goto Error;
               
               if (!isint_(tempToken) || 
                   (jpbw = getJobData(atoi(tempToken))) == NULL){
                   if (badName && (*badName))
                       STRNCPY ((*badName), tempToken, MAX_CMD_DESC_LEN);

                   *replyCode = LSBE_ARRAY_NULL;
                       
                   if (mSchedStage != M_STAGE_REPLAY)
                       goto Error;
               }
               
               if (jpbw) {
                   if (jpbw->jgrpNode->nodeType != JGRP_NODE_ARRAY) {
                       if (badName && (*badName))
                           STRNCPY ((*badName), tempToken, MAX_CMD_DESC_LEN);
                       *replyCode = LSBE_ARRAY_NULL;
                       if (mSchedStage != M_STAGE_REPLAY)
                           goto Error;
		   }
                   node=newNode(tokenType, (void *)(ARRAY_DATA(jpbw->jgrpNode)));
               }
               else
                   node = newNode(tokenType, NULL);

               if (node == NULL) {
                   goto Error;
               }
               PUSH_STACK(operandStack, node);

               if ((DPT_NUMPEND <= tokenType) && (tokenType <= DPT_NUMENDED)) {
                   
                   if (!(tempToken=getToken(&dependCond, &tempType)) ||
                        (tempType != DPT_RIGHT_ && tempType != DPT_COMMA) )
                       goto Error;
                   if (tempType == DPT_COMMA) {
                        
                       if (!(tempToken=getToken(&dependCond, &tempType)) ||
                           !((tempType >= DPT_GT && tempType <= DPT_NE) ||
                             tempType == DPT_NAME || 
                             strcmp(tempToken, "*") == 0))
                           goto Error;
                       if (tempType >= DPT_GT && tempType <= DPT_NE) {
                          
                           node->dptUnion.jgrp.opType = tempType;
                           if (!(tempToken=getToken(&dependCond, &tempType)) ||
                               tempType != DPT_NAME)
                               goto Error;
                       }
                       else
                           node->dptUnion.jgrp.opType = DPT_EQ;
                       
                       if (isint_(tempToken))
                           node->dptUnion.jgrp.num = atoi(tempToken);
                       else if (strcmp(token, "*") == 0) 
                           node->dptUnion.jgrp.num = INFINIT_INT;
                       else
                           goto Error;
                       
                       tempToken=getToken(&dependCond, &tempType);
                       if (!tempToken || tempType != DPT_RIGHT_)
                           goto Error;
                   }
               }
               else {
                   
                   tempToken=getToken(&dependCond, &tempType);
                   if (!tempToken || tempType != DPT_RIGHT_)
                       goto Error;
               }
               }
               break;
           default:
               goto Error; 
	        
        }
    }
    
    if (mergeNode(MERGE_ALL) < 0)
        goto Error;

    rootNode=popStackDep(operandStack);
    if (!rootNode)
	goto Error;
    
    if ((node=popStackDep(operandStack)) != NULL) {
        freeDepCond(node);
        goto Error;
    }
    if ((node=popStackDep(operatorStack)) != NULL) {
        freeDepCond(node);
        goto Error;
    }
    rootNode->updFlag = TRUE;  
    freeStackDep(operatorStack, FALSE);
    freeStackDep(operandStack, FALSE);
    *replyCode = LSBE_NO_ERROR;
    return (rootNode);

Error:
    
    if (jobRec)
	FREEUP (jobRec);
    freeStackDep(operatorStack, TRUE);
    freeStackDep(operandStack, TRUE);
    FREEUP(nodeList);
    return(NULL);
} 

int
evalDepCond (struct dptNode *node, struct jData *jobRec)
{
    static char fname[]="evalDepCond";
    int value;

    switch (node->type) {
    case DPT_OR:                                
	value = evalDepCond (node->dptLeft, jobRec);
	if (value == DP_REJECT) 
	   return(node->value = DP_REJECT);
	if (value == DP_TRUE) {
	    return (node->value = DP_TRUE);
	} else if (value == DP_FALSE) {
	    value = evalDepCond (node->dptRight, jobRec);
	    if (value == DP_REJECT)
		return(node->value = DP_REJECT);
            if (value == DP_TRUE)
		return (node->value = DP_TRUE);
            else
		return (node->value = DP_FALSE);
        } else
            return(node->value = evalDepCond (node->dptRight, jobRec)); 

    case DPT_AND:                              
	if((value = evalDepCond (node->dptLeft, jobRec)) == DP_INVALID ||
	    value == DP_REJECT) {  
            return (node->value = value);
	} else if (value == DP_FALSE) {
	  value = evalDepCond (node->dptRight, jobRec);
	    if (value == DP_INVALID || value == DP_REJECT)
		return (node->value = value);
            else
		return (node->value = DP_FALSE);
        } else
            return(node->value = evalDepCond (node->dptRight, jobRec));

    case DPT_NOT:
        node->value = evalDepCond(node->dptLeft, jobRec);
	if (node->value == DP_REJECT)
	    return(node->value);
        else if ((node->value == DP_FALSE) || (node->value == DP_INVALID))
            node->value  = DP_TRUE;
        else if (node->value == DP_TRUE)
            node->value  = DP_FALSE;
        return(node->value); 
    
    case DPT_DONE:
    case DPT_POST_DONE:
    case DPT_POST_ERR:
    case DPT_ENDED:
    case DPT_STARTED:
    case DPT_EXIT: { 
        struct jData *jpbw = node->dptJobRec;
	struct listSet *ptr = NULL; 

        int jobIsFinished  = 0;
        int jobIsPostFinished  = 0;

        if ( jpbw == NULL) {
            node->value = DP_INVALID;
            return(node->value);
        }

	if (jpbw->nodeType == JGRP_NODE_ARRAY) {
	    
	    if (node->dptJobIdx 
		&& (ptr = node->dptJobIdx->depJobList)
		&&  ((*node).dptUnion.job.opFlag != ARRAY_DEP_ONE_TO_ONE)) {
		
	        jpbw = (struct jData *)ptr->elem;
		
	    } else if ((*node).dptUnion.job.opFlag == ARRAY_DEP_ONE_TO_ONE)
		{
		    LS_LONG_INT       arrayIdx;

		    
		    arrayIdx = LSB_ARRAY_IDX((*jobRec).jobId);
		    
		    jpbw = getJobData(LSB_JOBID((*jpbw).jobId, arrayIdx));
		    if (jpbw == NULL) {
			
			struct listSetIterator      iter;
			long                        *ptr1;

			if (node->dptJobIdx == NULL) {
			    ls_syslog(LOG_ERR,"\
%s: job %s has one to one dependency but dptJobIdx is NULL",
				      fname, 
				      lsb_jobid2str(jobRec->jobId));
			    node->value = DP_INVALID;
			    return(node->value);
			}

			listSetIteratorAttach(node->dptJobIdx->depJobList,
					      &iter);
			for (ptr1 = listSetIteratorBegin(&iter);
			     ptr1 != NULL;
			     ptr1 = listSetIteratorGetNext(&iter)) {

			    
			    jpbw = (struct jData *)*ptr1;

			    
			    if (arrayIdx ==  LSB_ARRAY_IDX(jpbw->jobId)) {
				break;
			    }
			}
		    } 
		    
		} else {
		    
		    jpbw = jpbw->nextJob;
		}
        } 

        while (jpbw) {
	    if (node->dptJobIdx 
		&& (!inIdxList(jpbw->jobId, node->dptJobIdx->idxList))
		&& ((*node).dptUnion.job.opFlag != ARRAY_DEP_ONE_TO_ONE)) {
		goto Next;
	    }

	   switch(node->type) {
	       case DPT_POST_DONE:
	       case DPT_POST_ERR:
		   jobIsPostFinished = ( IS_POST_FINISH(jpbw->jStatus) ? 1 : 0 );
		   break;
	       case DPT_DONE:
	       case DPT_EXIT:
	       case DPT_ENDED:
	       case DPT_STARTED:
		   jobIsFinished = ( IS_FINISH(jpbw->jStatus) ? 1 : 0 );
		   break;
	       default:
		   ls_syslog(LOG_ERR, 
			    I18N(5502, "At line %d of file %s, should never be here"), /*catgets 5502 */
			     __LINE__, __FILE__);
	   } 

           if  (evalJobDep(node, jpbw, jobRec))
                node->value = DP_TRUE;
           else if ((jpbw->jStatus & JOB_STAT_VOID)   
		    ||  ( IS_FINISH(jpbw->jStatus)    
                          && !(!IS_POST_FINISH(jpbw->jStatus)
                               && (jpbw->jStatus & JOB_STAT_DONE)
                               && ((node->type == DPT_POST_DONE)
                                   || (node->type == DPT_POST_ERR))))){
		
                node->value = DP_INVALID;
		return(node->value);
           } else {
		
                node->value = DP_FALSE;
                return(node->value);
           }
Next:
	   
           if (node->dptJobRec->nodeType == JGRP_NODE_ARRAY
	       && (*node).dptUnion.job.opFlag != ARRAY_DEP_ONE_TO_ONE) {
	       
	       if (ptr != NULL) {
		   ptr = ptr->next;
		   if (ptr != NULL) {
		       jpbw = (struct jData *)ptr->elem;
		   } else {
		       jpbw = NULL;
		   }
	       } else {
		   
                   jpbw = jpbw->nextJob;
	       }
           } else {
               jpbw = NULL;
	   }
        }
    }
    break;
    case DPT_WINDOW:

        updateTimeWindow(node->dptWindow);
        if (node->dptWindow->status == WINDOW_OPEN)
            node->value = DP_TRUE;
        else
            node->value = DP_FALSE;
        break;
    case DPT_NUMPEND:
    case DPT_NUMHOLD:
    case DPT_NUMRUN:
    case DPT_NUMEXIT:
    case DPT_NUMDONE:
    case DPT_NUMSTART: 
    case DPT_NUMENDED:  {
        int num = 0;

        if (!node->dptJgrp) {
            node->value = DP_INVALID;
            return(node->value);
        }
        if (node->dptJgrp->status == JGRP_VOID) {
            node->value = DP_INVALID;
            DESTROY_REF(node->dptJgrp, destroyJgArrayBaseRef);
            node->dptJgrp = NULL;
            return(node->value);
        }

        if (node->dptUnion.jgrp.num == INFINIT_INT) {
            num = node->dptJgrp->counts[JGRP_COUNT_NJOBS];
            num = ( num <= 0 ) ? INFINIT_INT:num; 
        }
        else
           num = node->dptUnion.jgrp.num;
        if (opExpr(node->dptUnion.jgrp.opType,
                   getCounterOfDep(node->type, node->dptJgrp->counts),
                   num)) 
            node->value = DP_TRUE;
        else
            node->value = DP_FALSE;
        break;
    }
    default:
        break;
    }
    return(node->value);

} 

void
freeDepCond(struct dptNode *dptNode)
{
#define FREEDEP(n) {if(n) freeDepCond(n);}

    if (!dptNode)
        return;

    switch (dptNode->type) {
        case DPT_AND:
        case DPT_OR:
            
            FREEDEP(dptNode->dptLeft);
            FREEDEP(dptNode->dptRight);
            free(dptNode);
            break;
        case DPT_NOT:
            FREEDEP(dptNode->dptLeft);
            free(dptNode);
            break;
        case DPT_LEFT_: 
        case DPT_RIGHT_:
            free(dptNode);
            break;
        case DPT_DONE:
        case DPT_EXIT:
        case DPT_STARTED:
        case DPT_ENDED:
        case DPT_POST_DONE:
        case DPT_POST_ERR:
            DESTROY_REF(dptNode->dptJobRec, destroyjDataRef);
            DESTROY_REF(dptNode->dptJobIdx, destroyjobIdxRef);
   	    dptNode->dptUnion.job.opFlag = 0;
            free(dptNode);
            break;
        case DPT_WINDOW:
            freeTimeWindow(dptNode->dptWindow);
            free(dptNode);
            break;
        case DPT_NUMPEND: 
        case DPT_NUMHOLD:
        case DPT_NUMRUN: 
        case DPT_NUMEXIT: 
        case DPT_NUMDONE: 
        case DPT_NUMSTART:
        case DPT_NUMENDED:
            DESTROY_REF(dptNode->dptJgrp, destroyJgArrayBaseRef);
            dptNode->dptJgrp = NULL;
            free(dptNode);
            break;
        default:
            break;
    }
} 

void
resetDepCond(struct dptNode *dptNode)
{
    dptNode->value= DP_FALSE;
    if(dptNode->type == DPT_AND || dptNode->type == DPT_OR) {
	resetDepCond(dptNode->dptLeft);
	resetDepCond(dptNode->dptRight);
    } else if (dptNode->type == DPT_NOT) {
        resetDepCond(dptNode->dptLeft);
    }     
} 

static
char *getToken(char **sp, dptType *type)
{
   static char token[4*MAXLINELEN];
   int i, j = 0;
   
   if (!*sp)
      return NULL;
   while(isspace(**sp))
      (*sp)++;
   if (**sp == '\0')
      return NULL;

   for(i=0; tokenTable[i].name; i++) {
      if (strncmp(*sp, tokenTable[i].name, tokenTable[i].namelen) == 0) {
          strcpy(token, tokenTable[i].name);
          *sp += tokenTable[i].namelen;
          if ((isalnum (**sp) || **sp == '*') && 
			    tokenTable[i].checkCha == TRUE) {
	      j = tokenTable[i].namelen;
	      break;
        } 
          *type = tokenTable[i].type;
          return(token);       
      }
   }

   
   if (IS_QUOTE(**sp)) { 
     char quote = **sp;
     (*sp)++;
     token[j++]=quote;
     while( (j < 4*MAXLINELEN -1) && (**sp != '\0')) {
            token[j++]=**sp;
            if (token[j-1] == quote) {
		(*sp)++;
                break;
	    }
         (*sp)++;
     }
   } else {	
      while(IS_VALIDCHAR(**sp) && !IS_DELIM(**sp) && (j < 4*MAXLINELEN -1)) {
          token[j]=**sp;
          (*sp)++;
          j++;
      }
   }
   *type=DPT_NAME;
   token[j]='\0';
   return(token);
        
} 

static struct dptNode *
newNode(dptType nodeType, void *data)
{
    struct dptNode *node;

    
    node = (struct dptNode *)my_calloc(1, 
				       sizeof(struct dptNode), 
				       "newNode");
    node->type = nodeType;
    node->value = DP_FALSE;
    switch (nodeType) {
    case DPT_OR:
    case DPT_AND:
    case DPT_LEFT_:
        node->dptLeft = NULL;
        node->dptRight = NULL;
        break;
    case DPT_NOT:
        node->dptLeft = NULL;
        break;
    case DPT_DONE:
    case DPT_POST_DONE: 
    case DPT_POST_ERR:
        node->dptJobRec = createjDataRef((struct jData *)data);
        break;
    case DPT_EXIT:
        node->dptJobRec = createjDataRef((struct jData *)data);
        
        node->dptUnion.job.opType = DPT_TRUE; 
        node->dptUnion.job.exitCode = 0;
        break;
    case DPT_ENDED:
        node->dptJobRec = createjDataRef((struct jData *)data);
        break;
    case DPT_STARTED:
        node->dptJobRec = createjDataRef((struct jData *)data);
        break;
    case DPT_WINDOW:
        node->dptWindow = (struct timeWindow *)data;
        break;
    case DPT_NUMPEND:
    case DPT_NUMHOLD:
    case DPT_NUMRUN:
    case DPT_NUMEXIT:
    case DPT_NUMDONE:
    case DPT_NUMSTART:
    case DPT_NUMENDED:
        node->dptJgrp = createJgArrayBaseRef((struct jgArrayBase *)data);
        
        node->dptUnion.jgrp.opType = DPT_NE;
        node->dptUnion.jgrp.num = 0;
        break;
    default:
        break;
    }
    return(node);
        
} 

static int
mergeNode(int level)
{
   struct dptNode *op, *operand1, *operand2;

   switch (level) {
      case MERGE_TOP:
         op=popStackDep(operatorStack);
         if (!op)
            return(-1);
         operand1=popStackDep(operandStack);
         if (!operand1) {
            pushStackDep(operatorStack,op);
            return(-1);
         }
         if ((op->type != DPT_NOT) &&  
             ((operand2=popStackDep(operandStack)) == NULL)) {
             pushStackDep(operatorStack, op);
             pushStackDep(operandStack,operand1);
             return(-1);
         } 
         op->dptLeft = operand1;
         if (op->type != DPT_NOT)
             op->dptRight = operand2;	
         pushStackDep(operandStack, op);
         return(0);
      
      case MERGE_LEFT:
         
         while (1) {
             op = popStackDep(operatorStack);
             if (!op)
                 return(-1);
             if (op->type == DPT_LEFT_) {
                 free(op);
                 return(0);
             } 
             pushStackDep(operatorStack, op);
             if (mergeNode(MERGE_TOP) < 0)
                return(-1);
         }
       case MERGE_ALL:
          while(mergeNode(MERGE_TOP) >= 0);
          return(0);
     default:
        break;
   }
          
   return(-1);

}  

static struct Stack
*initStackDep(void)
{
   struct Stack *stack;
   
   stack= (struct Stack *)my_malloc(sizeof(struct Stack ), "initStackDep");
 
   stack->top=-1;
   stack->nodes = (struct dptNode **) my_malloc(5*sizeof(struct dptNode *), "initStackDep");
   stack->size = 5;
   return(stack);
} 

static int 
pushStackDep(struct Stack *stack, struct dptNode *node)
{
    static char fname[]="pushStackDep";
    char *sp;
    if (stack->size == stack->top + 1) {
       sp = realloc(stack->nodes, stack->size*2*sizeof(struct dptNode *));
       if (!sp) {
           ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "realloc");
           return(-1); 
       }
       stack->size *= 2;
       stack->nodes = (struct dptNode **)sp;
   } 
   stack->nodes[++stack->top] = node;
   return (0);
} 


static struct dptNode *
popStackDep(struct Stack *stack)
{
   if (stack->top < 0)
      return(NULL);
   return(stack->nodes[stack->top--]);
} 
 

static void
freeStackDep(struct Stack *stack, int freeNodes)
{
   int i;

   if (freeNodes && stack->top >= 0)
       for(i=0; i <= stack->top; i++)
           freeDepCond(stack->nodes[i]);
   free(stack->nodes);
   free(stack);
} 

static struct jData **
matchJobs(char *jobp, char *lsfUserName, int *numFoundJob, int *replyCode,
         struct jobIdx **jobIdx, LS_LONG_INT *element, int *flag) 
{
    struct jData *jpbw, **foundJobRec, **tempJobRec;
    int jobId, numJob, numRec = 20;
    char *jobName, *sp;
    struct jgTreeNode *parent, *nPtr;
    struct idxList *idxList = NULL;
    int error;
    bool_t   wildCardJobName = FALSE;
    time_t submitTime = 0;
    int  maxJLimit = 0;
    struct uData *uPtr; 

    sp = strstr(jobp, "[*]");
    if (sp != NULL) {
	sp[0] = 0;
	if (flag != NULL) {
	    *flag  = ARRAY_DEP_ONE_TO_ONE;
	}
    }

    

    idxList = parseJobArrayIndex(jobp, &error, &maxJLimit);

    if (error != LSBE_NO_ERROR) 
       return(NULL);

    if (idxList) {
        (*jobIdx) = (struct jobIdx *)my_malloc(sizeof (struct jobIdx),
                     "matchJobs");
        (*jobIdx)->idxList = idxList;
	(*jobIdx)->depJobList = NULL;
        (*jobIdx)->numRef = 0;
    }

    *element = 0;
    numJob = 0;
    foundJobRec = (struct jData **)my_calloc (numRec,
					      sizeof(struct jData *), 
					      "matchJobs");    
    
    if ((sp = strchr(jobp, '[')))  
         sp[0] = '\0';

    
    jobId=0;
    jobName=NULL;
    if (IS_QUOTE(jobp[0])) { 
        char quote=jobp[0];
        
        jobp++;
        if((sp=strchr(jobp,quote)) != NULL)
           sp[0]='\0';
        jobName=jobp;
    } else {	
        if (isint_(jobp))
           jobId = atoi(jobp);
        else
           jobName=jobp;
    }

    if (jobName && strlen(jobName) > 0 && jobName[strlen(jobName) - 1] == '*') {
	wildCardJobName = TRUE;
    }
    
    if (jobId) {
        foundJobRec[0] = getJobData(jobId);
        if (foundJobRec[0]) {
            int notFound = FALSE;

	    
	    if (flag != NULL
		&& *flag == ARRAY_DEP_ONE_TO_ONE) {
		
		if (foundJobRec[0]->nodeType == JGRP_NODE_ARRAY) {
		    numJob = 1;
		    goto checkIdx;
		} else {
		    
		    numJob = 0;
		    goto Ret;
		}
	    }
		
	    if (idxList == NULL 
		&& foundJobRec[0]->nodeType == JGRP_NODE_ARRAY) {
    		idxList = parseJobArrayIndex(
			foundJobRec[0]->shared->jobBill.jobName, 
			&error, &maxJLimit);

		if (idxList) {
    		    (*jobIdx) = (struct jobIdx *)my_malloc(
			sizeof (struct jobIdx), "matchJobs");
    		    (*jobIdx)->idxList = idxList;
	   	    (*jobIdx)->depJobList = NULL;
        	    (*jobIdx)->numRef = 0;
    		}
	   }

           if (idxList == NULL) { 
                
                numJob = 1; 
                goto Ret;
            }

	    

            while (idxList != NULL) {
	        

                int i;
                 
                for (i = idxList->start; i <= idxList->end; i += idxList->step){
		    if ((jpbw = getJobData(LSB_JOBID(jobId, i)))) {
		       
			(*jobIdx)->depJobList = 
	                       listSetInsert((long)createjDataRef(jpbw), 
	                       (*jobIdx)->depJobList);
                        continue;
	            } else { 
                        notFound = TRUE;
                        *element = LSB_JOBID(jobId, i);
               	        break;
                    }
                 }
                 if (notFound) { 
                     break;
                 } else {
                     idxList = idxList->next;
                 }
             }
             if (notFound) 
                 numJob = 0;  
             else 
	         numJob = 1;  
	}
	goto Ret;
     } 
 
    
    if (jobName!=NULL && jobName[0] != '\0') {
	
            	    
	    parent = groupRoot;
    } else { 
        goto Ret;
    }

    if (parent == NULL)
        goto Ret;

    
    uPtr = getUserData(lsfUserName);
    
    
    for (nPtr = parent->child; nPtr; nPtr = nPtr->right) {
        if (nPtr->nodeType == JGRP_NODE_ARRAY)
            jpbw = ARRAY_DATA(nPtr)->jobArray;
        else
            jpbw = JOB_DATA(nPtr);
	    if (jpbw->uPtr == uPtr) {
                
                if (jobDepLastSub && !wildCardJobName) {
                    if (submitTime < jpbw->shared->jobBill.submitTime) {
                        submitTime = jpbw->shared->jobBill.submitTime;
                        foundJobRec[0] =jpbw;
                        numJob = 1;
                    }
                } 
                else
		    foundJobRec [numJob++] = jpbw;
		if (numJob >= numRec) {
		    tempJobRec = (struct jData **) realloc (foundJobRec, 
			       2 * numRec * sizeof (struct jData *));
                    if (tempJobRec == NULL) {
		        *replyCode = LSBE_NO_MEM;
			FREEUP (foundJobRec);
                        *numFoundJob = 0;
			return NULL;
                    }
		    numRec *= 2;
		    foundJobRec = tempJobRec;
                }
            }
    } 

 checkIdx:
    
    
    if (flag != NULL
	&& *flag == ARRAY_DEP_ONE_TO_ONE
	&& foundJobRec[0] != NULL) {
	struct idxList      *depArrayIdx;
	struct idxList      *submitIdxList;
	char                *jname;
	int                 err;
	int                 limit;

	jname = foundJobRec[0]->shared->jobBill.jobName;
	depArrayIdx  = parseJobArrayIndex(jname, &err, &limit);

	
	submitIdxList = getIdxListContext();

	
	if ( (depArrayIdx == NULL
	      || submitIdxList == NULL)
	     ||
	     ( (*depArrayIdx).start   != (*submitIdxList).start
	       || (*depArrayIdx).end  != (*submitIdxList).end
	       || (*depArrayIdx).step != (*submitIdxList).step)) {
	    
	    numJob = 0;
	    *replyCode = LSBE_DEP_ARRAY_SIZE;
	} else {
	    int               i;
	    LS_LONG_INT       jobid;

	    
	    jobid = foundJobRec[0]->jobId;

	    (*jobIdx) = 
		(struct jobIdx *)my_calloc(1, 
					   sizeof (struct jobIdx),
					   "matchJobs");
	    for (i  = depArrayIdx->start; 
		 i <= depArrayIdx->end; 
		 i += depArrayIdx->step){
		struct jData      *jPtr;
	    
				
		if ((jPtr = getJobData(LSB_JOBID(jobid, i)))) {
		    (*jobIdx)->depJobList = 
			listSetInsert((long)createjDataRef(jPtr), 
				      (*jobIdx)->depJobList);
		}
	    }
	} 

		
	freeIdxList(depArrayIdx);	    
    }   
    
Ret:
    if (numJob == 0) {
	FREEUP (foundJobRec);
	 
	if (idxList && *jobIdx) { 
	    DESTROY_REF(*jobIdx, destroyjobIdxRef);
	}
	*numFoundJob = 0;

	
	if (*replyCode == LSBE_DEPEND_SYNTAX) {
	    *replyCode = LSBE_NO_JOB;
	}

	return NULL;
    }
    *numFoundJob = numJob;
    return (foundJobRec);
} 

static int 
createMoreNodes (dptType tokenType, int numJob, struct jData **jobRec,
                 struct jobIdx *jobIdx, struct dptNode **nodesList)
{
    int i, first = TRUE;
    struct dptNode *node;

    for (i = 0; i < numJob; i++) {
	if ((node=newNode(tokenType, (void *)jobRec[i])) == NULL)
	    return -1;
        if (jobIdx) {
            node->dptJobIdx = createjobIdxRef(jobIdx);
        }
        else
            node->dptJobIdx = NULL;

        PUSH_STACK(operandStack, node);
        if (nodesList) 
             nodesList[i] = node;
	if (first == TRUE) {
	    first = FALSE;
	    continue;
        }
	if ((node=newNode(DPT_AND, NULL)) == NULL)
	    return -1;
        PUSH_STACK(operatorStack, node);
	if (mergeNode(MERGE_TOP) < 0)
	    return(-1);

   }    
   return(0);

Error:
   return -1;
} 


static int
opExpr(int opType, int operand1, int operand2) 
{
    switch (opType) {
        case DPT_LT:
            if (operand1 < operand2)
                return(TRUE);
            else
                return(FALSE);
        case DPT_LE:
            if (operand1 <= operand2)
                return(TRUE);
            else
                return(FALSE);
        case DPT_GT:
            if (operand1 > operand2)
                return(TRUE);
            else
                return(FALSE);
        case DPT_GE:
            if (operand1 >= operand2)
                return(TRUE);
            else
                return(FALSE);
        case DPT_EQ:
            if (operand1 == operand2)
                return(TRUE);
            else
                return(FALSE);
        case DPT_NE:
            if (operand1 != operand2)
                return(TRUE);
            else
                return(FALSE);
        case DPT_TRUE:
	    return(TRUE);
    }
    return(FALSE);
} 

static int
getCounterOfDep(int type, int *counts)
{
    static char fname[] = "getCounterOfDep()";
    switch (type) {
        case DPT_NUMPEND:
            return(counts[JGRP_COUNT_PEND]);
        case DPT_NUMRUN:
            return(counts[JGRP_COUNT_NRUN]);
        case DPT_NUMHOLD:
        	return(counts[JGRP_COUNT_NPSUSP]);
        case DPT_NUMEXIT:
            return(counts[JGRP_COUNT_NEXIT]);
        case DPT_NUMDONE:
            return(counts[JGRP_COUNT_NDONE]);
        case DPT_NUMSTART:
            return(counts[JGRP_COUNT_NJOBS] - counts[JGRP_COUNT_PEND] -
                   counts[JGRP_COUNT_NPSUSP]);
        case DPT_NUMENDED:
            return(counts[JGRP_COUNT_NEXIT] + counts[JGRP_COUNT_NDONE]);
        default:
            
            ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5501,
		"%s: dep Type <%d> out of bound"),  /* catgets 5501 */
		fname,
                type);
            return(0);
    }
} 


static struct jobIdx *
createjobIdxRef (struct jobIdx *jobIdx)
{
    if (jobIdx) {
        jobIdx->numRef++;
    }
    return(jobIdx);
} 

static void
destroyjobIdxRef(struct jobIdx *jobIdx)
{
    struct jData *jp = NULL;
    struct listSet *ptr;

    if (jobIdx) {
        jobIdx->numRef--;
        if (jobIdx->numRef <= 0) {
           
	   for (ptr = jobIdx->depJobList; ptr; ptr = ptr->next) {
               jp = (struct jData *)ptr->elem;
	       if (jp) {
                   destroyjDataRef(jp);
               }
           }
           listSetFree(jobIdx->depJobList);
           freeIdxList(jobIdx->idxList);
           FREEUP(jobIdx);
        }
    }
    return;
} 


static int
evalJobDep(struct dptNode *node, struct jData *jpbw, struct jData *depJob)
{
    
    if (
            ( node->type != DPT_DONE      )
         && ( node->type != DPT_POST_DONE )
         && ( node->type != DPT_EXIT      )
         && ( node->type != DPT_POST_ERR )
         && ( node->type != DPT_ENDED     )
         && ( node->type != DPT_STARTED   )
       )  {
        return(FALSE);
    }

    if (jpbw == NULL)
        return(FALSE);

    switch (node->type) {
        case DPT_DONE:
            if ((depJob == NULL || depJob->endTime < jpbw->endTime) &&
                ((jpbw->jStatus & JOB_STAT_DONE) || (IS_PEND(jpbw->jStatus) 
                && jpbw->jFlags & JFLAG_LASTRUN_SUCC)))
                return(TRUE);
             else
                return(FALSE);
	    
        case DPT_POST_DONE:
            if ( (depJob == NULL) || (depJob->endTime < jpbw->endTime) ) {
		if ( IS_POST_DONE(jpbw->jStatus)
			   && (jpbw->jStatus & JOB_STAT_DONE) ) {
                    
		    return(TRUE);
		}
	    }
            return(FALSE);
	    
        case DPT_POST_ERR:
            if ( (depJob == NULL) || (depJob->endTime < jpbw->endTime) ) {
		if ( IS_POST_ERR(jpbw->jStatus)
			   && (jpbw->jStatus & JOB_STAT_DONE) ) {
                    
		    return(TRUE);
		}
	    }
            return(FALSE);
	    
        case DPT_EXIT: {
            int exitCode;
            exitCode = jpbw->exitStatus >> 8;
            if ((depJob == NULL || depJob->endTime < jpbw->endTime) && 
                opExpr(node->dptUnion.job.opType, exitCode,
                       node->dptUnion.job.exitCode) &&
                (IS_FINISH(jpbw->jStatus) || (IS_PEND(jpbw->jStatus)
                && !(jpbw->jFlags & JFLAG_LASTRUN_SUCC)))
                && !(jpbw->jStatus & JOB_STAT_DONE))
                return(TRUE);
            else
                return(FALSE);
        }
        case DPT_ENDED:
            if ((depJob == NULL || depJob->endTime < jpbw->endTime) &&
                ((IS_FINISH(jpbw->jStatus) || IS_PEND(jpbw->jStatus))))     
                return(TRUE);
            else
                return(FALSE);
        case DPT_STARTED:
            if (jpbw->startTime 
                && (depJob == NULL || depJob->endTime < jpbw->startTime) 
                && !(jpbw->jStatus & JOB_STAT_PRE_EXEC))
                return(TRUE);
            else
                return(FALSE);
        default:
                return(FALSE);
    }
} 


