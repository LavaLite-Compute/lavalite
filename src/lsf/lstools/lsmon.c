/* $Id: lsmon.c,v 1.4 2007/08/15 22:18:55 tmizan Exp $
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
#include "lsf/lib/liblavalite.h"







extern void (*Signal_ (int, void (*)(int)))(int);


extern int makeFields(struct hostLoad *, char *loadval[], char **);
extern void *formatHeader(char **, char);
extern char **filterToNames(char *);
                                             
void  printHeader(char **);

#define CONTROL_L  12  
#define MAXLISTSIZE 256

char    resreq[MAXLINELEN] = "";
char    indexfilter[MAXLINELEN] = "";
char    *hostnames[MAXLISTSIZE];
int     numneeded, num = 0, options = 0;
int     updateRate = 10;               
FILE    *lf;			
int     Lflag = 0;		
int     redirect = 0;           
char    logFilename[MAXFILENAMELEN];
extern int num_loadindex;


#define NL_SETN  27 


void
usage(char * cmd)
{
	
    fprintf(stderr, 
            "%s: %s [-h] [-V] [-N|-E] [-n num_hosts] [-R res_req] [-I index_list] [-i interval] [-L logfile] [host_name ... ]\n",
	    I18N_Usage,cmd); 
    exit(0);
}



void
endPgm(char * message, int status)
{
    
    if (!redirect) {
        echo();
        nocbreak();
        move(LINES-1, 0);
        refresh();
        endwin(); 
        putchar('\n');
    } 

    if (message != NULL) {
       ls_perror(message);
    }
    if (Lflag)
	fclose(lf);
    exit(status);
}

void
quit()
{
    endPgm(NULL, 0);
}

void
getLoadInfo(void)
{
    int  i, j, nf;  
    static int  numStatus;  
    struct hostLoad *hosts;
    int  numwanted;
    static char **loadval = NULL;
    static char first=true;
    static char *defaultindex[]={"r15s", "r1m", "r15m", "ut", "pg", "ls",
				  "it", "tmp", "swp", "mem", NULL};
    char **nlp;
    char linebuf[132];

    numwanted = numneeded;
    if (num > 0)
       options = options | EXACT;

    if (indexfilter[0] != '\0')
        nlp = filterToNames(indexfilter);
    else
        nlp = defaultindex;

    hosts = ls_loadinfo(resreq, &numwanted, options, 0, hostnames, num,
                             &nlp);      

    if (!hosts) {
	numwanted=0;
	if (!redirect) {
            mvprintw(3, 0, ls_sysmsg());
            clrtobot();
            move(1,0);
            refresh();
        }
        return;
    }

    printHeader(nlp);
    
    if (!redirect)
        move(3,0);

    if (loadval == NULL)
        if (!(loadval=(char **)malloc(num_loadindex*sizeof(char *)))) {
            lserrno=LSE_MALLOC;
            ls_perror("lsmon");
            exit(-1);
        }
   
    if (!redirect) 
        for (i = 0 ; (i < numwanted) && (i < (LINES - 4)) ; i++) {
	    

 	    mvprintw(i + 3, 0, "%-15.15s ", hosts[i].hostName);

	    
	    if (LS_ISUNAVAIL(hosts[i].status)) 
	        addstr(I18N_unavail);
	    else if (!LS_ISUNAVAIL(hosts[i].status) && LS_ISRESDOWN(hosts[i].status)) {
		      char str[7];

		      sprintf(str, "-%2s    ", I18N_ok);
		      addstr(str);
		  }
	    else if (LS_ISOKNRES(hosts[i].status)) {
		      char str[7];

		      sprintf(str, " %2s    ", I18N_ok);
		      addstr(str);
		  }
	    else if (LS_ISLOCKED(hosts[i].status))
	        {
	 	    addstr((_i18n_msg_get(ls_catd,NL_SETN,2107, "lock"))); /* catgets  2107  */
		    if (LS_ISLOCKEDU(hosts[i].status))
		        addch('U');
		    if (LS_ISLOCKEDW(hosts[i].status))
		        addch('W');
		    if (LS_ISLOCKEDM(hosts[i].status))
		        addch('M');
		    addstr("   ");
	        }
            else if (LS_ISBUSY(hosts[i].status))
		    addstr((_i18n_msg_get(ls_catd,NL_SETN,2108, "busy   "))); /* catgets  2108  */

	    if (!LS_ISUNAVAIL(hosts[i].status)) {
	        nf = makeFields(&hosts[i], loadval, nlp);
                strcpy(linebuf, loadval[0]);
                for(j=1; j < nf; j++) 
                    strcat(linebuf, loadval[j]);

               mvprintw(i + 3, 22, "%s",linebuf);
	    }

	if (Lflag) {
            if (first) {
	       numStatus = 0;
               for(j=0; nlp[j]; j++) {
                   fprintf(lf,"%s ",nlp[j]); 
		   numStatus++;
               }
               fprintf(lf,"\n");
               first=false;
            }

	    fprintf(lf, "%ld %s", time(NULL), hosts[i].hostName);

            for (j = 0; j < 1 + GET_INTNUM (numStatus); j++) {
		if (j  == GET_INTNUM (numStatus))
		    fprintf(lf, " %d ", hosts[i].status[j]);
                else
		    fprintf(lf, " %d", hosts[i].status[j]);
            }
	    for (j=0; nlp[j]; j++)
		fprintf(lf, "%f ", hosts[i].li[j]);
	    fprintf(lf, "\n");
	}
	    clrtoeol();
    } 
   else { 
       for (i=0; i < numwanted; i++) {
	    if (Lflag) {
		if (first) {
		    numStatus = 0;
 	            for(j=0; nlp[j]; j++) {
		        fprintf(lf,"%s ",nlp[j]);
			numStatus++;
                    }
		    fprintf(lf,"\n");
		    first=false;
		 }
		fprintf(lf, "%ld %s", time(NULL), hosts[i].hostName);
		for (j = 0; j < 1 + GET_INTNUM (numStatus); j++) {
		    if (j  == GET_INTNUM (numStatus))
		        fprintf(lf, " %d ", hosts[i].status[j]);
                    else
		        fprintf(lf, " %d", hosts[i].status[j]);
                }
		for (j=0; nlp[j]; j++)
		    fprintf(lf, "%f ", hosts[i].li[j]);
	        fprintf(lf, "\n");
	     }
       }
    } 
  
    if (!redirect) {
        if (i < (LINES - 4))
        {
            move(i+3, 0);
            clrtobot();
        }
        move(1,0);
        refresh();
    }
}


void
printHeader(char **nlp)
{
    char *hname;
    char *headLine;
    
    if (!redirect)
        mvaddstr(0, 0, _i18n_msg_get(ls_catd,NL_SETN,2109, "Hostname: ")); /* catgets  2109  */
    if ((hname = ls_getmyhostname()) == NULL) {
	endPgm("ls_getmyhostname", -1);
    }

    
    headLine = formatHeader(nlp, false);

    if (!redirect) {
        addstr(hname);
       mvprintw(0, COLS - 25, 
	   _i18n_msg_get(ls_catd,NL_SETN,2110, "Refresh rate: %3d secs"), /* catgets  2110  */
	   updateRate); 
		mvaddstr(2, 0, headLine);
    }

}


static void
chInterval(void)
{
   
   int        interval = 0;
   char       str[255];

   


   mvaddstr(1, 0, _i18n_msg_get(ls_catd,NL_SETN,2111, "Enter a new refresh rate (secs):  ")); /* catgets  2111  */
   clrtoeol();
   refresh();

   echo();
   getstr(str);
   noecho();

   interval = atoi(str);

   if (interval <= 0) {
      mvaddstr(1, 0, _i18n_msg_get(ls_catd,NL_SETN,2112, "Refresh rate must be greater than zero.  Interval unchanged.")); /* catgets  2112  */
   } else {
      updateRate = interval;
   }

   clrtoeol();
   refresh();
}


int
chNumNeeded(void)
{
   
   int        numhosts;
   char       str[255];

   
   move(1,0);
   clrtoeol();
   mvaddstr(1, 0, _i18n_msg_get(ls_catd,NL_SETN,2113, "Enter number of hosts to be displayed:  ")); /* catgets  2113  */
   refresh();
   echo();
   getstr(str);
   noecho();
   numhosts = atoi(str);

   if (numhosts <= 0) {
      move(1,0);
      clrtoeol();
      mvaddstr(1, 0, _i18n_msg_get(ls_catd,NL_SETN,2114, "Number of hosts must be greater than zero. Number unchanged.")); /* catgets  2114  */
      numhosts = numneeded; 
   } else {
      move(1,0);
      clrtoeol();
   }

   move(1,0);
   refresh();

   return numhosts;
}

void
chResReq(void)
{
   
   char       newresreq[MAXLINELEN];

   
   move(1,0);
   clrtoeol();
   mvaddstr(1, 0, _i18n_msg_get(ls_catd,NL_SETN,2115, "Enter resource requirements string:  ")); /* catgets  2115  */
   refresh();

   echo();
   getstr(newresreq);
   noecho();

   
   if (expSyntax_(newresreq) < 0) {
      
      mvaddstr(1, 0, 
           _i18n_msg_get(ls_catd,NL_SETN,2116, "Invalid resource requirements.  Resource requirements unchanged.")); /* catgets  2116  */
      clrtoeol();
   } else {
      
      move(1,0);
      strcpy(resreq, newresreq);
   } 
   clrtoeol();
   refresh();
}

int
main(int argc, char **argv)
{

    char   *cmd;             
    struct stat statBuf; 
    fd_set mask, allMask;    
    int    nReady;           
    int StdIn = 0;           
    int	optc;

    
    
    int Vflag = 0, Nflag = 0, Eflag = 0, Rflag = 0, iflag = 0, nflag = 0, Iflag = 0;
    extern int optind, opterr;
    extern char *optarg;
    int rc;

    Signal_(SIGINT, (SIGFUNCTYPE) quit);

    
    numneeded = 0;
    cmd = *argv;                               

    opterr = 0;
    rc = _i18n_init ( I18N_CAT_MIN );	


    while ((optc = getopt(argc, argv, "hVNEn:R:I:i:L:")) != EOF)
    {
	switch(optc)
	{
	case 'R':
	    if (Rflag)
		usage(cmd);
	    else {
		Rflag = 1;
		strcpy(resreq , optarg);
		break;
	    }
        case 'I':
            if (Iflag)
		usage(cmd);
	    else {
		Iflag = 1;
		strcpy(indexfilter, optarg);
		break;
	    }

	case 'n':		
	    if (nflag)
		usage(cmd);
	    else {
		nflag = 1;
		numneeded = atoi(optarg);
		if (numneeded <= 0) {
		    usage(cmd);
		}
		break;
	    }

	case 'i':
	    if (iflag)
		usage(cmd);
	    else {
		iflag = 1;
		updateRate = atoi(optarg);
		if (updateRate <= 0) {
		    usage(cmd);
		}
		break;
	    }
	    
        case 'L':		
	    if (Lflag)		
		usage(cmd);
	    else {
		Lflag = 1;
		strcpy(logFilename, optarg);
		break;
	    }
	    
	case 'N':
	    if (Nflag || Eflag)
		usage(cmd);
	    else {
		Nflag = 1;
		options = NORMALIZE;
		break;
	    }

	case 'E':
	    if (Nflag || Eflag)
		usage(cmd);
	    else {
		Eflag = 1;
		options = EFFECTIVE;
		break;
	    }

	case 'V':
	    if (Vflag)
		usage(cmd);
	    else {
		Vflag = 1;
		fputs(_LAVALITE_VERSION_, stderr);
		exit(0);
	    }

	case 'h':
	default:
            usage(cmd);
	}

    }


    while (optind < argc)		
    {
       hostnames[num] = argv[optind++];
       if (ls_isclustername(hostnames[num]) <=0 &&
          !isValidHost_(hostnames[num])) {  
	  fputs(_i18n_msg_get(ls_catd,NL_SETN,2141, "Unknown host: "), /* catgets  2141  */
	      stderr); 
	  fputs(hostnames[num], stderr);
	  putc('\n', stderr);
	  usage(cmd);
	  }
       num++;
       if (num>=MAXLISTSIZE) {
	  fprintf(stderr, _i18n_msg_get(ls_catd,NL_SETN,2142, "too many hosts specified\n")); /* catgets  2142  */
	  exit(-1);
       }
    }
  
    if (Lflag) {
        lf = fopen(logFilename, "a");
	if (lf == NULL) {
	    fprintf(stderr, I18N_FUNC_FAIL_S, logFilename, "fopen", strerror(errno));
	    fprintf(stderr, "\n" );
    	    exit(-1);
	}
    }

    if (fstat(0, &statBuf) == 0) 
	redirect = ((ftell(stdin) == 0) && Lflag) ? 1 : 0;

    if (!redirect) {
        
    (void)initscr();
        cbreak(); 
        noecho();

        
        if ((COLS < 80)) {
            (void)endwin();
            (void)fprintf(stderr,
	              _i18n_msg_get(ls_catd,NL_SETN,2144, "Sorry, screen must be at least %d COLUMNS wide (currently COLS = %d).\n"), /* catgets  2144  */
		      80, COLS); 
            exit(1);
        }

        if (LINES < 5) {
            (void)endwin();
            (void)fprintf(stderr,
		      _i18n_msg_get(ls_catd,NL_SETN,2145, "Sorry, screen must have at least %d LINES (currently LINES = %d).\n"), /* catgets  2145  */
		      5, LINES); 
            exit(1);
        } else if ((LINES < num + 4)) {
            
            (void)endwin();
            (void)fprintf(stderr,
                      _i18n_msg_get(ls_catd,NL_SETN,2146, "Sorry, screen must have at least %d LINES to display all of the requested hosts.\n"),  /* catgets  2146  */
		      num+4);
            (void)fprintf(stderr,
		      _i18n_msg_get(ls_catd,NL_SETN,2147, "The current window has %d lines.\n"), /* catgets  2147  */
		      LINES); 
            exit(1);
        }
        
        FD_ZERO(&allMask);
        FD_SET(StdIn, &allMask);
    } 


        
    for (;;) { 
	struct timeval timeout;

        getLoadInfo();

        if (!redirect)
        {
       	 	
  	    	timeout.tv_usec = 0;
	    	timeout.tv_sec = updateRate;

        	mask = allMask;
        	nReady = select(StdIn+1, &mask, 0, 0, &timeout);

	    	
	    	move(1, 0);
	    	clrtoeol();

                if ((nReady < 0) && (errno != EINTR)) {
		    char i18nBuf[100];
		    sprintf(i18nBuf, I18N_ERROR,"lsmon","select");
                    endPgm(i18nBuf, 1);
                } else if (nReady > 0) {
                    if (FD_ISSET(StdIn, &mask)) {
                     
                     switch (getch()) {
                     case CONTROL_L:
                         
                         erase();
                         refresh();
                         break;

                      case 'q':
                          
                          endPgm(NULL, 0);
                          break;
              
                      case 'i':
                          
                          chInterval();
                          break;
              
                      case 'n':
                          
                          numneeded = chNumNeeded();
                          break;
              
                      case 'N':
                          
                          move(1,0);
                          clrtoeol();
                          if (options == NORMALIZE) {
                              options = 0;
                              mvaddstr(1, 0,
                                _i18n_msg_get(ls_catd,NL_SETN,2149, "Output raw CPU run queue load indices")); /* catgets  2149  */
                          } else {
                              options = NORMALIZE;
                              mvaddstr(1, 0, 
			       	_i18n_msg_get(ls_catd,NL_SETN,2150, "Output normalized CPU run queue load indices")); /* catgets  2150  */
                          }
                          move(1,0);
                          refresh();
                          break;
              
                      case 'E':
                          
                          move(1,0);
                          clrtoeol();
                          if (options == EFFECTIVE) {
                              options = 0;
                              mvaddstr(1, 0,
                                  _i18n_msg_get(ls_catd,NL_SETN,2151, "Output raw CPU run queue load indices")); /* catgets  2151  */
                          } else {
                              options = EFFECTIVE;
                              mvaddstr(1, 0, 
			        _i18n_msg_get(ls_catd,NL_SETN,2152, "Output effective CPU run queue load indices")); /* catgets  2152  */
                          }
                          move(1,0);
                          refresh();
                          break;

                      case 'R':
                          
                          chResReq();
                          break;

                      default:
                          
                          move(1,0);
                          clrtoeol();
                          mvaddstr(1, 0, 
			    _i18n_msg_get(ls_catd,NL_SETN,2153, "Options:(i)nterval,(n)umber,(N)ormalize,(E)ffective,(R)esources,(q)uit,(^L)refresh")); /* catgets  2153  */
                          move(1,0);
                          refresh();
                          break;
                      }
                    }   
                }   

        }   
        else 
            sleep(updateRate);
    }             

    return 0;
}

