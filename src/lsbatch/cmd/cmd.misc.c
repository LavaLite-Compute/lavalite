/* $Id: cmd.misc.c,v 1.3 2007/08/15 22:18:44 tmizan Exp $
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
#include "../lib/lsb.h"

#include <netdb.h>
#include <ctype.h>

#define NL_SETN 8 	


extern int optind;

#define WIDTH 80
#define BLANKLEN 22 
static int cursor = 0;

void
prtLine(char *line)
{
    int i, length, flag;
    char timeblank[BLANKLEN];

    if(line[0] == '\n')
        cursor = 0;
    for(i=0;i<BLANKLEN-1;i++)
        timeblank[i] = ' ';
    timeblank[BLANKLEN-1] = '\0' ;

    flag = 1;
    length = 0;
    while(flag) {
        if(cursor == WIDTH) {
            printf("\n%s", timeblank);
            cursor = BLANKLEN;
            if(line[length] == ' ')
                length++;
        }
        putchar(line[length]);
        if(line[length] == '\n')
            cursor = 0;
        length++;
        cursor++;
        if(line[length] == '\0')
            flag = 0;
    }
    fflush(stdout);
} 

int
repeatedName (char *s,  char **ss, int n)
{
    int i;
    if (n == 0)
        return (FALSE);
    for(i = 0; i < n; i++) {
        if (ss[i] == NULL)
            return (FALSE);
        if (strcmp(s, ss[i]) == 0)
            return (TRUE);
    }
    return (FALSE);
}

int
getNames (int argc, char **argv,  int optind, char ***nameList, 
                                int *allOrDef, char *nameType)
{
    int numNames = 0;
    int a_size=0;
    static char **list = NULL;
    char **temp;

    FREEUP (list);
    a_size=16;
    if ((list=(char **)malloc(a_size*sizeof(char *)))==NULL)
        return 0;
    
    *allOrDef = FALSE;
    if (argc >= optind+1) {
        for (numNames = 0; argc > optind; optind++) {
            
            if (strcmp (nameType, "host") == 0
                     &&  strcmp (argv[optind],  "myhostname") == 0) {
                *allOrDef =  TRUE;
                numNames = 1;
                break;
            }
            
            if ((strcmp (nameType, "hostC") == 0 
                              || strcmp (nameType, "queueC") == 0
                              ||  strcmp (nameType, "user") == 0) 
                 && strcmp(argv[optind], "all") == 0) {
                *allOrDef = TRUE;
                numNames = 0;
                break;
            }
            if (repeatedName(argv[optind],  list, numNames))
                continue;
            if ((a_size!=0) && (numNames>=a_size)) {
                a_size <<= 1;  
                if ((temp=(char **)realloc(list, 
			a_size*sizeof(char *)))==NULL) {
		    *nameList = list;
                    return (numNames);
                }
		list = temp;
	    }
            list[numNames] = argv[optind];
            numNames ++;
        }
    }
    
    if (numNames == 1 && strcmp (nameType, "queue") == 0
		    && strcmp ("default", list[0]) == 0)
	*allOrDef = TRUE;
    *nameList = list;
    return (numNames);
} 

void 
prtWord(int len, const char *word, int cent)
{
    char fomt[200];

       if ( cent == 0 )    
           sprintf(fomt, "%%-%d.%ds ", len, len);
       else if (cent < 0)  
           sprintf(fomt, "%%%d.%ds ", len, len);
       else {                     
 int lenW = (int)strlen(word);
            if ( lenW >= len )    
                sprintf(fomt, "%%-%d.%ds ", len, len);
            else {
 int d1 = lenW + ((len - lenW) >> 1), d2;

            d2 = len - d1;
            sprintf(fomt, "%%%ds%%%ds ", d1, d2);
	    };
       };
       
       printf(fomt, word, " ");
}

void 
prtWordL(int len, const char *word)
{
    char fomt[200];

       sprintf(fomt, "%%-%ds ",len);
       printf(fomt, word);
}

char *
prtValue(int len, int val)
{
    static char out[100];
    char fomt[200];

       sprintf(fomt, "%%%dd ", len);
       sprintf(out, fomt, val);

       return (out);
}

char *
prtDash(int len)
{
    static char out[100]; 
    char fmrt[100];

    sprintf(fmrt, "%%%ds ", len);
    sprintf (out, fmrt, "-");

    return (out);
}
