/*$Id: cal.l,v 1.3 2007/07/30 04:09:35 cchen Exp $ */

%{
/*
 *********************************************************************
 *                       Lex Analyser
 *                        -- yylex --
 *********************************************************************
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <limits.h>
#include <ctype.h>
#include <strings.h>
#include <string.h>
#include <linux/limits.h>
unsigned char *yysptr, yysbuf[];
#include <syslog.h>
#include <malloc.h>

#ifndef  MAXLINELEN
#include "../lsf.h"
#endif
#include "intlib_internal.h"
// #include "calendar.h"
#include "lsf/intlib/tokdefs.h"
#include "lsf/intlib/yparse.h"
#include "lsf/lib/lsi18n.h"

#define MAXTOKENLEN 302
#define NL_SETN      22     /* MOD_LSFINTLIB 22   msgId  350-399 */

typedef struct stream Stream;
        struct stream { char *buf, *bp; Stream * prev; };

static Stream _in, * in=& _in;
/* FILE * yyerfp={stdin} ; */

char *source=0 ;
char *token = NULL;
char *yybuff;
char yyerr[MAXLINELEN];
extern int calerrno;

static int screen(void);
static void s_lookup(int);
static int getbufc(void);
int yywhere(char *);
void set_lower(char *);
struct rwtable *END(struct rwtable *);
int yymark(void);

%}
letter 		[a-zA-Z]
digit  		[0-9]
letter_or_digit	[a-zA-Z_0-9]
white_space	[ \t\n]
blank		[ \t]
other		.

%%

"#" 					yymark() ;
"/*".*"*/"				yymark() ;
"\|\|"	return (OR);
"&&"	return (AND);
"<="	return (LE);
">="	return (GE);
"=="	return (EQ);
".."	return (DOTS);

{letter}{letter_or_digit}*	return screen();

{digit}+			{s_lookup((ICON));
				 return (ICON) ;
                                }
{digit}+"."{digit}*		{s_lookup((RCON));
				 return (RCON) ;
                                }
"'".*"'"			{s_lookup((ESTRING));
				 return (ESTRING) ;
                                }
"\"".*"\""			{s_lookup((ESTRING));
				 return (ESTRING) ;
                                }
{white_space}+                  ;
{other}				return ((yytext[0]));

%%

static struct rwtable {
	char   rw_name[40] ;
	int    rw_yylex ;
        } rwtable[] = {
         {"apr", APR},
         {"april", APR},
         {"aug", AUG},
         {"august", AUG},
         {"dates", DATES},
         {"day", DAYS},
         {"dec", DEC},
         {"december", DEC},
         {"feb", FEB},
         {"february", FEB},
         {"fri", FRI},
         {"friday", FRI},
         {"fy",  FY},
         {"h",   HH},
         {"jan", JAN},
         {"january", JAN},
         {"jul", JUL},
         {"july", JUL},
         {"jun", JUN},
         {"june", JUN},
         {"m",    MM},
         {"mar", MAR},
         {"march", MAR},
         {"may", MAY},
         {"mon", MON},
         {"monday", MON},
         {"month", MONTH},
         {"nov", NOV},
         {"november", NOV},
         {"oct", OCT},
         {"october", OCT},
         {"quarter", QUARTER},
         {"range",   RANGE},
         {"sat", SAT},
         {"saturday", SAT},
         {"sep", SEP},
         {"september", SEP},
         {"sun", SUN},
         {"sunday", SUN},
         {"thu", THU},
         {"thursday", THU},
         {"tue",      TUE},
         {"tuesday",  TUE},
         {"wed",      WED},
         {"wednesday",WED},
         {"week", WEEK},
         {"yy",  YY},
	 {"zzzz",  	(SZZZZ)}
};



struct rwtable *END(low)
struct rwtable *low;
{ int c ;
  struct rwtable *i;

  for (i=low; ; i++){
     if ((c=strcmp(i->rw_name, "zzzz"))==0)
     return(i);
  }

}

void set_lower(str)
char *str;
{ int i ;

  for (i=0; i<strlen(str); i++){
   if ('A' <=str[i] && str[i]<='Z')
      str[i]=str[i]+'a'-'A' ;
  }
}

static int screen(void)
 { struct rwtable *low=rwtable ,
	     	  *high=END(low),
		  *mid ;
   int c;
   char str[YYLMAX];

  strcpy(str,(char *)yytext);
  set_lower((char *)yytext);
   while ((int) (low<=high)){
	mid=low+(high-low)/2 ;
	if ((c=strcmp(mid->rw_name, (char *)yytext))==0) {
	    return (mid->rw_yylex) ;
	}
	else if (c<0)
	    low=mid+1;
	else
	    high=mid-1;
   }
   strcpy((char *)yytext,str);
   s_lookup(NAME);
   return (NAME);
 }

static void s_lookup(yylex)
int yylex;
{
    if (!token){
        token = (char *)malloc(MAXTOKENLEN);
        token[0] = '\0';
    }
    strcpy(token, (char *)yytext) ;
}


int yymark(void)
{
  if (source)
	free(source);
  source=(char *) calloc(yyleng, sizeof(char));
  if (source)
	sscanf((char *)yytext, "# %d %s", &yylineno, source);
  return (1);
}

void calerror(s)
 	register char *s ;
{ extern int calnerrs ;
        char sptr[YYLMAX];

	sptr[0]='\0';
	yywhere(sptr) ;
        sprintf(yyerr, "[error %d] %s %s", calnerrs+1,sptr, s);
        calerrno = CALEN_SYNTAX;
	yysptr=yysbuf;
        yylineno =1;
        yparseFail(&calAllocHead);
}

void timerror(s)
        register char *s ;
{ extern int timnerrs ;
        char sptr[YYLMAX];

        sptr[0]='\0';
        yywhere(sptr) ;
        sprintf(yyerr, "[error %d] %s %s", timnerrs+1,sptr, s);
        calerrno = CALEN_SYNTAX;
        yysptr=yysbuf;
        yylineno =1;
        yparseFail(&timAllocHead);
}

void idxerror(s)
        register char *s ;
{ extern int idxnerrs ;
        char sptr[YYLMAX];

        sptr[0]='\0';
        yywhere(sptr) ;
        sprintf(yyerr, "[error %d] %s %s", idxnerrs+1,sptr, s);
        yysptr=yysbuf;
        yylineno =1;
        yparseFail(&idxAllocHead);
}



int yywhere(sptr)
char *sptr;
{ char colon=0;

  if (source && *source && strcmp(source, "\"\""))
  { char * cp=source ;
    int len =strlen(source);

    if (*cp=='*')
      ++cp, len-=2;
    if (strncmp(cp, "./", 2 )==0)
      cp+=2, len-=2;
    sprintf(sptr, "file %.*s", len, cp);
    colon=1;
  }
  if (yylineno >0)
  {	if (colon)
		sprintf(sptr,"%s, ",sptr) ;
	sprintf(sptr, "%s line %d", sptr, yylineno -
 				  (*yytext =='\n' || ! *yytext));
	colon=1;
  }
  if (*yytext)
  {	register int i;

	for (i=0; i<20; ++i)
		if (!yytext[i] || yytext[i]=='\n')
			break;
	if (i)
   	{
         	if (colon)
			sprintf(sptr,"%s ", sptr);
		sprintf(sptr,"%s near \"%.*s%.8s\"", sptr, i, yytext, yybuff);
		colon=1;
	}
  }
  if (colon)
	sprintf(sptr,"%s: ",sptr);

  return(yylineno);
}

int
yywrap(void)
{	register Stream * sp;
	if (sp=in->prev)
	{	if (in->buf)
		    free(in->buf);
		if (in) {
		    free(in);
		    in=NULL;
		}
		in=sp;
		return 0;
	}
	return 1;
}

static int getbufc(void)
{
  if (*yybuff =='\0')
    return(EOF);
  else
    return(*yybuff++);
}

/*
 *-----------------------------------------------------------------------
 * yyalloc --
 * ARGUMENT:
 *     struct mallocList  *head[IN]: the head of a  malloc list;
 *     int                 size[IN]: the size of allocated space;
 *-----------------------------------------------------------------------
 */

void *
yyalloc(struct mallocList  **head, int size)
{
    struct mallocList  *entry;
    void               *space;

    space = (void *)calloc(1, size);
    if (space) {
        entry = (struct mallocList  *)malloc(sizeof(struct mallocList));
        if (!entry) {
            free(space);
            return(NULL);
        }
        entry->space = space;
        entry->next = *head;
        *head = entry;
    };
    return(space);
} /* yyalloc */


/*
 *-----------------------------------------------------------------------
 * yyfree --
 * ARGUMENT:
 *     struct mallocList  *head[IN]: the head of a  malloc list;
 *     void               *space[IN]: space to be free;
 *-----------------------------------------------------------------------
 */

void
yyfree(struct mallocList  **head, void *space)
{
    struct mallocList  *entry;

    entry = *head;
    while (entry) {
        if (entry->space == space)
            break;
        entry = entry->next;
    }

    if (entry)
        entry->space = NULL;
    free(space);
} /* yyfree */

void
yparseSucc(struct mallocList  **head)
{
    struct mallocList  *entry, *freeEntry;

    entry = *head;
    while (entry) {
        freeEntry = entry;
        entry = entry->next;
        free(freeEntry);
    }
    *head = NULL;
    return;
} /* yparseSucc */

void
yparseFail(struct mallocList  **head)
{
    struct mallocList  *entry, *freeEntry;

    if (!head)
        return;
    entry = *head;
    while (entry) {
        freeEntry = entry;
        entry = entry->next;
        if (freeEntry->space)
            free(freeEntry->space);
        free(freeEntry);
    }
    *head = NULL;
    return;
} /* yparseFail */

/*
 *----------------------------------------------------------------------
 * checkNameSpec --
 * ARGUMENT:
 *     char *name[IN]: a name specification
 *     char **errMsg[OUT]: a error message which is assigned by a static
 *          message buffer and not allow to be FREED in outside.
 * DESCRIPTION:
 *    If name is invalid, then given error message according to lexCode
 * RETURN
 *    1   -- VALID name spec;
 *    0   -- else;
 *-----------------------------------------------------------------------
 */
int
checkNameSpec(char *name, char **errMsg )
{
    static char bufMess[1024];
    int lexCode, retVal = 1;

    bufMess[0] = '\0';
    yybuff = name;
    lexCode = yylex();

    if ( lexCode != NAME) {
        if (lexCode == UNDEF)
            sprintf(bufMess,
		    _i18n_msg_get(ls_catd, NL_SETN, 350,
					   "Name is invalid: \"%s\""), /* catgets 350 */
		    name);
        else if (lexCode == ICON || lexCode == RCON)
            sprintf(bufMess,
		    _i18n_msg_get(ls_catd, NL_SETN, 351,
					   "Name cannot start with digits: \"%s\""), /* catgets 351 */
		    name);
        else if (lexCode == CCON)
            sprintf(bufMess,
		    _i18n_msg_get(ls_catd, NL_SETN, 352,
					   "Name cannot be const string: \"%s\""), /* catgets 352 */
		    name);
        else if (MON <= lexCode && lexCode <= SUN)
            sprintf(bufMess,
		    _i18n_msg_get(ls_catd, NL_SETN, 353,
					   "Name cannot be name of week: \"%s\""), /* catgets 353 */
		    name);
        else if (JAN <= lexCode && lexCode <= DEC)
            sprintf(bufMess,
		    _i18n_msg_get(ls_catd, NL_SETN, 354,
					   "Name cannot be name of month: \"%s\""), /* catgets 354 */
		    name);
        else if (YY <= lexCode && lexCode <= SZZZZ)
            sprintf(bufMess,
		    _i18n_msg_get(ls_catd, NL_SETN, 355,
					   "Name cannot be reserved word: \"%s\""), /* catgets 355 */
		    name);
        else
            sprintf(bufMess,
		    _i18n_msg_get(ls_catd, NL_SETN, 350,
					   "Name is invalid: \"%s\""), name);
        retVal = 0;
        goto Exit;
    }
    else {
        lexCode = yylex();
        if ((char)lexCode == '@') {
            lexCode = yylex();
            if (lexCode != NAME) {
                sprintf(bufMess,
			_i18n_msg_get(ls_catd, NL_SETN, 357,
					       "User name is wrong: \"%s\""), /* catgets 357 */
			token);
                retVal = 0;
                goto Exit;
            }
            lexCode = yylex();
        }
        if (lexCode > 0 ) {
           sprintf(bufMess,
		   _i18n_msg_get(ls_catd, NL_SETN, 350,
					  "Name is invalid: \"%s\""), name);
           retVal = 0;
           goto Exit;
        }
        else {
           retVal = 1;
           goto Exit;
        }
    }
Exit:
    *errMsg = bufMess;
    yysptr=yysbuf;
    yylineno =1;
    return(retVal);
} /* nameErrMess */
