/* $Id: jidx.c,v 1.6 2007/08/15 22:18:49 tmizan Exp $
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
# define ICON 1
# define SZZZZ 40

# line 11 "idx.y"
#       include <stdio.h>
#       include <stdlib.h>
#include "lsf/intlib/libllcore.h"
#       include "lsf/lib/lsi18n.h"

#define YYALLOC(x) yyalloc(&idxAllocHead, (x))
struct mallocList  *idxAllocHead = NULL;
int     idxerrno = IDX_NOERR;

# line 24 "idx.y"
typedef union   {
        int ival ;
        struct idxList  idxType;
        struct idxList  *idxPtr;
        } YYSTYPE;
# define yyparse idxparse
# define yyerror idxerror
# define yylval idxlval
# define yychar idxchar
# define yydebug idxdebug
# define yymaxdepth idxmaxdepth
# define yynerrs idxnerrs
# define OR 259
# define AND 260
#define yyclearin yychar = -1
#define yyerrok yyerrflag = 0
extern int yychar;
#ifndef YYMAXDEPTH
#define YYMAXDEPTH 150
#endif

#ifndef __YYSCLASS
# define __YYSCLASS static
#endif
YYSTYPE yylval;
__YYSCLASS YYSTYPE yyval;
typedef int yytabelem;
# define YYERRCODE 256

# line 138 "idx.y"

__YYSCLASS yytabelem yyexca[] ={
-1, 1,
	0, -1,
	-2, 0,
	};
# define YYNPROD 13
# define YYLAST 48
__YYSCLASS yytabelem yyact[]={

    12,     3,    14,    10,    15,    13,     4,     8,    10,     1,
     5,     2,    11,     7,     6,     0,     0,    16,     0,     0,
     0,     0,    18,    19,    17,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     9 };
__YYSCLASS yytabelem yypact[]={

   -90, -3000,   -31,     2,     7,   -93,   -39,   -56,   -41,     7,
 -3000, -3000, -3000,     2,     7,     7, -3000, -3000, -3000, -3000 };
__YYSCLASS yytabelem yypgo[]={

     0,    14,    13,    10,    11,     7,     9 };
__YYSCLASS yytabelem yyr1[]={

     0,     6,     6,     4,     3,     3,     1,     1,     2,     2,
     2,     2,     5 };
__YYSCLASS yytabelem yyr2[]={

     0,     3,     7,     7,     3,     7,     3,     7,     3,     5,
     5,     7,     3 };
__YYSCLASS yytabelem yychk[]={

 -3000,    -6,    -4,    91,    37,    -3,    -1,    -2,    -5,    45,
     1,    -5,    93,    44,    58,    45,    -5,    -3,    -5,    -5 };
__YYSCLASS yytabelem yydef[]={

     0,    -2,     1,     0,     0,     0,     4,     6,     8,     0,
    12,     2,     3,     0,     0,    10,     9,     5,     7,    11 };
typedef struct { char *t_name; int t_val; } yytoktype;
#ifndef YYDEBUG
#	define YYDEBUG	0
#endif

#if YYDEBUG

__YYSCLASS yytoktype yytoks[] =
{
	"ICON",	1,
	"SZZZZ",	40,
	",",	44,
	"!",	33,
	"=",	61,
	"OR",	259,
	"+",	43,
	"-",	45,
	"/",	47,
	"*",	42,
	"AND",	260,
	"-unknown-",	-1
};

__YYSCLASS char * yyreds[] =
{
	"-no such reduction-",
	"arrayIndSpec : arrayInd",
	"arrayIndSpec : arrayInd '%' icon",
	"arrayInd : '[' indcies ']'",
	"indcies : index",
	"indcies : index ',' indcies",
	"index : region",
	"index : region ':' icon",
	"region : icon",
	"region : '-' icon",
	"region : icon '-'",
	"region : icon '-' icon",
	"icon : ICON",
};
#endif
#define YYFLAG  (-3000)

#if defined(NLS) && !defined(0)
#include <msgbuf.h>
#endif

#ifndef nl_msg
#define nl_msg(i,s) (s)
#endif

#define YYERROR		goto yyerrlab

#ifndef __RUNTIME_YYMAXDEPTH
#define YYACCEPT	return(0)
#define YYABORT		return(1)
#else
#define YYACCEPT	{free_stacks(); return 0;}
#define YYABORT		{free_stacks(); return 1;}
#endif

#define YYBACKUP( newtoken, newvalue )\
{\
	if ( yychar >= 0 || ( yyr2[ yytmp ] >> 1 ) != 1 )\
	{\
		yyerror( (nl_msg(30001,"syntax error - cannot backup")) );\
		goto yyerrlab;\
	}\
	yychar = newtoken;\
	yystate = *yyps;\
	yylval = newvalue;\
	goto yynewstate;\
}
#define YYRECOVERING()	(!!yyerrflag)
#ifndef YYDEBUG
#	define YYDEBUG	1
#endif

int yydebug;

# ifndef __RUNTIME_YYMAXDEPTH
__YYSCLASS YYSTYPE yyv[ YYMAXDEPTH ];
__YYSCLASS int yys[ YYMAXDEPTH ];
# else
__YYSCLASS YYSTYPE *yyv;
__YYSCLASS int *yys;

#if defined(__STDC__)
#include <stdlib.h>
#else
	extern char *malloc();
	extern char *realloc();
	extern void free();
#endif

static int allocate_stacks();
static void free_stacks();
# ifndef YYINCREMENT
# define YYINCREMENT (YYMAXDEPTH/2) + 10
# endif
# endif
long  yymaxdepth = YYMAXDEPTH;

__YYSCLASS YYSTYPE *yypv;
__YYSCLASS int *yyps;

__YYSCLASS int yystate;
__YYSCLASS int yytmp;

int yynerrs;
__YYSCLASS int yyerrflag;
int yychar;

int
idxparse(idxList, maxJLimit)
struct idxList **idxList;
int *maxJLimit;
{
	register YYSTYPE *yypvt;

# ifdef __RUNTIME_YYMAXDEPTH
	if (allocate_stacks()) YYABORT;
# endif
	yypv = &yyv[-1];
	yyps = &yys[-1];
	yystate = 0;
	yytmp = 0;
	yynerrs = 0;
	yyerrflag = 0;
	yychar = -1;

	goto yystack;
	{
		register YYSTYPE *yy_pv;
		register int *yy_ps;
		register int yy_state;
		register int  yy_n;

		yy_pv = yypv;
		yy_ps = yyps;
		yy_state = yystate;
		goto yy_newstate;

	yystack:
		yy_pv = yypv;
		yy_ps = yyps;
		yy_state = yystate;

	yy_stack:

#if YYDEBUG

		if ( yydebug )
		{
			register int yy_i;

			printf( "State %d, token ", yy_state );
			if ( yychar == 0 )
				printf( "end-of-file\n" );
			else if ( yychar < 0 )
				printf( "-none-\n" );
			else
			{
				for ( yy_i = 0; yytoks[yy_i].t_val >= 0;
					yy_i++ )
				{
					if ( yytoks[yy_i].t_val == yychar )
						break;
				}
				printf( "%s\n", yytoks[yy_i].t_name );
			}
		}
#endif
		if ( ++yy_ps >= &yys[ yymaxdepth ] )
		{
# ifndef __RUNTIME_YYMAXDEPTH
			yyerror( (nl_msg(30002,"yacc stack overflow")) );
			YYABORT;
# else

			YYSTYPE * yyv_old = yyv;
			int * yys_old = yys;
			yymaxdepth += YYINCREMENT;
			yys = (int *) realloc(yys, yymaxdepth * sizeof(int));
			yyv = (YYSTYPE *) realloc(yyv, yymaxdepth * sizeof(YYSTYPE));
			if (yys==0 || yyv==0) {
			    yyerror( (nl_msg(30002,"yacc stack overflow")) );
			    YYABORT;
			    }

			yy_ps = (yy_ps - yys_old) + yys;
			yyps = (yyps - yys_old) + yys;
			yy_pv = (yy_pv - yyv_old) + yyv;
			yypv = (yypv - yyv_old) + yyv;
# endif

		}
		*yy_ps = yy_state;
		*++yy_pv = yyval;

	yy_newstate:
		if ( ( yy_n = yypact[ yy_state ] ) <= YYFLAG )
			goto yydefault;
#if YYDEBUG

		yytmp = yychar < 0;
#endif
		if ( ( yychar < 0 ) && ( ( yychar = yylex() ) < 0 ) )
			yychar = 0;
#if YYDEBUG
		if ( yydebug && yytmp )
		{
			register int yy_i;

			printf( "Received token " );
			if ( yychar == 0 )
				printf( "end-of-file\n" );
			else if ( yychar < 0 )
				printf( "-none-\n" );
			else
			{
				for ( yy_i = 0; yytoks[yy_i].t_val >= 0;
					yy_i++ )
				{
					if ( yytoks[yy_i].t_val == yychar )
						break;
				}
				printf( "%s\n", yytoks[yy_i].t_name );
			}
		}
#endif
		if ( ( ( yy_n += yychar ) < 0 ) || ( yy_n >= YYLAST ) )
			goto yydefault;
		if ( yychk[ yy_n = yyact[ yy_n ] ] == yychar )
		{
			yychar = -1;
			yyval = yylval;
			yy_state = yy_n;
			if ( yyerrflag > 0 )
				yyerrflag--;
			goto yy_stack;
		}

	yydefault:
		if ( ( yy_n = yydef[ yy_state ] ) == -2 )
		{
#if YYDEBUG
			yytmp = yychar < 0;
#endif
			if ( ( yychar < 0 ) && ( ( yychar = yylex() ) < 0 ) )
				yychar = 0;
#if YYDEBUG
			if ( yydebug && yytmp )
			{
				register int yy_i;

				printf( "Received token " );
				if ( yychar == 0 )
					printf( "end-of-file\n" );
				else if ( yychar < 0 )
					printf( "-none-\n" );
				else
				{
					for ( yy_i = 0;
						yytoks[yy_i].t_val >= 0;
						yy_i++ )
					{
						if ( yytoks[yy_i].t_val
							== yychar )
						{
							break;
						}
					}
					printf( "%s\n", yytoks[yy_i].t_name );
				}
			}
#endif

			{
				register int *yyxi = yyexca;

				while ( ( *yyxi != -1 ) ||
					( yyxi[1] != yy_state ) )
				{
					yyxi += 2;
				}
				while ( ( *(yyxi += 2) >= 0 ) &&
					( *yyxi != yychar ) )
					;
				if ( ( yy_n = yyxi[1] ) < 0 )
					YYACCEPT;
			}
		}

		if ( yy_n == 0 )
		{

			switch ( yyerrflag )
			{
			case 0:
				yyerror( (nl_msg(30003,"syntax error")) );
				yynerrs++;
				goto skip_init;

				yy_pv = yypv;
				yy_ps = yyps;
				yy_state = yystate;
				yynerrs++;
			skip_init:
			case 1:
			case 2:

				yyerrflag = 3;

				while ( yy_ps >= yys )
				{
					yy_n = yypact[ *yy_ps ] + YYERRCODE;
					if ( yy_n >= 0 && yy_n < YYLAST &&
						yychk[yyact[yy_n]] == YYERRCODE)					{

						yy_state = yyact[ yy_n ];
						goto yy_stack;
					}

#if YYDEBUG
#	define _POP_ "Error recovery pops state %d, uncovers state %d\n"
					if ( yydebug )
						printf( _POP_, *yy_ps,
							yy_ps[-1] );
#	undef _POP_
#endif
					yy_ps--;
					yy_pv--;
				}

				YYABORT;
			case 3:
#if YYDEBUG

				if ( yydebug )
				{
					register int yy_i;

					printf( "Error recovery discards " );
					if ( yychar == 0 )
						printf( "token end-of-file\n" );
					else if ( yychar < 0 )
						printf( "token -none-\n" );
					else
					{
						for ( yy_i = 0;
							yytoks[yy_i].t_val >= 0;
							yy_i++ )
						{
							if ( yytoks[yy_i].t_val
								== yychar )
							{
								break;
							}
						}
						printf( "token %s\n",
							yytoks[yy_i].t_name );
					}
				}
#endif
				if ( yychar == 0 )
					YYABORT;
				yychar = -1;
				goto yy_newstate;
			}
		}

#if YYDEBUG

		if ( yydebug )
			printf( "Reduce by (%d) \"%s\"\n",
				yy_n, yyreds[ yy_n ] );
#endif
		yytmp = yy_n;
		yypvt = yy_pv;

		{

			register int yy_len = yyr2[ yy_n ];

			if ( !( yy_len & 01 ) )
			{
				yy_len >>= 1;
				yyval = ( yy_pv -= yy_len )[1];
				yy_state = yypgo[ yy_n = yyr1[ yy_n ] ] +
					*( yy_ps -= yy_len ) + 1;
				if ( yy_state >= YYLAST ||
					yychk[ yy_state =
					yyact[ yy_state ] ] != -yy_n )
				{
					yy_state = yyact[ yypgo[ yy_n ] ];
				}
				goto yy_stack;
			}
			yy_len >>= 1;
			yyval = ( yy_pv -= yy_len )[1];
			yy_state = yypgo[ yy_n = yyr1[ yy_n ] ] +
				*( yy_ps -= yy_len ) + 1;
			if ( yy_state >= YYLAST ||
				yychk[ yy_state = yyact[ yy_state ] ] != -yy_n )
			{
				yy_state = yyact[ yypgo[ yy_n ] ];
			}
		}

		yystate = yy_state;
		yyps = yy_ps;
		yypv = yy_pv;
	}

	switch( yytmp )
	{

case 1:
# line 44 "idx.y"
{
                   *idxList = yypvt[-0].idxPtr;
                   yparseSucc(&idxAllocHead);
                } break;
case 2:
# line 49 "idx.y"
{
                   *idxList = yypvt[-2].idxPtr;
                   *maxJLimit = yypvt[-0].ival;
                    yparseSucc(&idxAllocHead);
                } break;
case 3:
# line 55 "idx.y"
{
                   yyval.idxPtr = yypvt[-1].idxPtr;
		} break;
case 4:
# line 59 "idx.y"
{
                   if ((yyval.idxPtr = (struct idxList *)
                             YYALLOC(sizeof(struct idxList))) == NULL) {
                       idxerror("No Memory");
                       idxerrno = IDX_MEM;
                       YYABORT;
                   }
                   yyval.idxPtr->start = yypvt[-0].idxType.start;
                   yyval.idxPtr->end = yypvt[-0].idxType.end;
                   yyval.idxPtr->step = yypvt[-0].idxType.step;
                   yyval.idxPtr->next = NULL;
                } break;
case 5:
# line 73 "idx.y"
{
                   if ((yyval.idxPtr = (struct idxList *)
                             YYALLOC(sizeof(struct idxList))) == NULL) {
                       idxerror("No Memory");
                       idxerrno = IDX_MEM;
                       YYABORT;
                   }
                   yyval.idxPtr->start = yypvt[-2].idxType.start;
                   yyval.idxPtr->end = yypvt[-2].idxType.end;
                   yyval.idxPtr->step = yypvt[-2].idxType.step;
                   yyval.idxPtr->next = yypvt[-0].idxPtr;
                } break;
case 6:
# line 86 "idx.y"
{
		    yyval.idxType.start = yypvt[-0].idxType.start;
                    yyval.idxType.end   = yypvt[-0].idxType.end;
                    yyval.idxType.step  = 1;
                    if (yyval.idxType.start < 1 || yyval.idxType.start > yyval.idxType.end || yyval.idxType.step <= 0) {
                        idxerror("boundary error");
                        idxerrno = IDX_BOUND;
                        YYABORT;
                    }
                } break;
case 7:
# line 98 "idx.y"
{
		    yyval.idxType.start = yypvt[-2].idxType.start;
                    yyval.idxType.end   = yypvt[-2].idxType.end;
                    yyval.idxType.step  = yypvt[-0].ival;
                    if (yyval.idxType.start < 1 || yyval.idxType.start > yyval.idxType.end || yyval.idxType.step <= 0) {
                        idxerror("boundary error");
                        idxerrno = IDX_BOUND;
                        YYABORT;
                    }
                } break;
case 8:
# line 111 "idx.y"
{
                    yyval.idxType.start = yypvt[-0].ival;
                    yyval.idxType.end  = yyval.idxType.start;
                    yyval.idxType.step = 1;
                } break;
case 9:
# line 117 "idx.y"
{
                    yyval.idxType.start = 1;
                    yyval.idxType.end  = yypvt[-0].ival;
                    yyval.idxType.step = 1;
                } break;
case 10:
# line 123 "idx.y"
{
                    yyval.idxType.start = yypvt[-1].ival;
                    yyval.idxType.end  = INFINIT_INT;
                    yyval.idxType.step = 1;
                } break;
case 11:
# line 129 "idx.y"
{
                    yyval.idxType.start = yypvt[-2].ival;
                    yyval.idxType.end  = yypvt[-0].ival;
                    yyval.idxType.step = 1;
  		} break;
case 12:
# line 135 "idx.y"
{
		    yyval.ival = atoi(token);
		} break;
	}
	goto yystack;
}

# ifdef __RUNTIME_YYMAXDEPTH

static int allocate_stacks() {

	yys = (int *) malloc(yymaxdepth * sizeof(int));
	yyv = (YYSTYPE *) malloc(yymaxdepth * sizeof(YYSTYPE));

	if (yys==0 || yyv==0) {
	   yyerror( (nl_msg(30004,"unable to allocate space for yacc stacks")) );
	   return 1;
	   }
	else return 0;

}

static void free_stacks() {
	if (yys!=0) free((char *) yys);
	if (yyv!=0) free((char *) yyv);
}

# endif
