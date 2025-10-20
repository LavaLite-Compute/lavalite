/* $Id: idx.y,v 1.1 2007/07/18 23:17:45 cchen Exp $
 ***********************************************************************
 *              Job Array Indcies  YYACC Parser
 *                    -- IndParser --
 ***********************************************************************
 */
%token ICON     1
%token SZZZZ    40

%{
#       include <stdio.h>
#       include <stdlib.h>
#include"inlib_internal.h"
#       include "lsf/lib/lsi18n.h"
#define NL_SETN      22      /* MOD_LSF_INTLIB  22  MsgId 550-599 */

#define YYALLOC(x) yyalloc(&idxAllocHead, (x))
struct mallocList  *idxAllocHead = NULL;
int     idxerrno = IDX_NOERR;

%}

%union  {
        int ival ;
        struct idxList  idxType;
        struct idxList  *idxPtr;
        }


%left ','
%nonassoc '!'
%right '='
%left  OR
%left '+' '-' '/' '*'
%right AND

%type <idxType> index region
%type <idxPtr> indcies arrayInd
%type <ival> icon
%%

arrayIndSpec   : arrayInd
               {
                   *idxList = $1;
                   yparseSucc(&idxAllocHead);
                }
                | arrayInd '%' icon
                {
                   *idxList = $1;
                   *maxJLimit = $3;
                    yparseSucc(&idxAllocHead);
                }
arrayInd	: '[' indcies ']'
		{
                   $$ = $2;
		}
indcies		: index
                {
                   if (($$ = (struct idxList *)
                             YYALLOC(sizeof(struct idxList))) == NULL) {
                       idxerror(_i18n_msg_get(ls_catd, NL_SETN, 550,
					      "No Memory")); /* catgets 550 */
                       idxerrno = IDX_MEM;
                       YYABORT;
                   }
                   $$->start = $1.start;
                   $$->end = $1.end;
                   $$->step = $1.step;
                   $$->next = NULL;
                }
                | index ',' indcies
                {
                   if (($$ = (struct idxList *)
                             YYALLOC(sizeof(struct idxList))) == NULL) {
                       idxerror(_i18n_msg_get(ls_catd, NL_SETN, 550, "No Memory"));
                       idxerrno = IDX_MEM;
                       YYABORT;
                   }
                   $$->start = $1.start;
                   $$->end = $1.end;
                   $$->step = $1.step;
                   $$->next = $3;
                }
index          	: region
                {
		    $$.start = $1.start;
                    $$.end   = $1.end;
                    $$.step  = 1;
                    if ($$.start < 1 || $$.start > $$.end || $$.step <= 0) {
                        idxerror(_i18n_msg_get(ls_catd, NL_SETN, 551,
					       "boundary error")); /* catgets 551 */
                        idxerrno = IDX_BOUND;
                        YYABORT;
                    }
                }
                | region ':' icon
                {
		    $$.start = $1.start;
                    $$.end   = $1.end;
                    $$.step  = $3;
                    if ($$.start < 1 || $$.start > $$.end || $$.step <= 0) {
                        idxerror(_i18n_msg_get(ls_catd, NL_SETN, 551,
					       "boundary error"));
                        idxerrno = IDX_BOUND;
                        YYABORT;
                    }
                }

region          : icon
                {
                    $$.start = $1;
                    $$.end  = $$.start;
                    $$.step = 1;
                };
                | '-' icon
                {
                    $$.start = 1;
                    $$.end  = $2;
                    $$.step = 1;
                };
                | icon '-'
                {
                    $$.start = $1;
                    $$.end  = INFINIT_INT;
                    $$.step = 1;
                };
                | icon '-' icon
                {
                    $$.start = $1;
                    $$.end  = $3;
                    $$.step = 1;
  		}
icon		: ICON
		{
		    $$ = atoi(token);
		}
%%
