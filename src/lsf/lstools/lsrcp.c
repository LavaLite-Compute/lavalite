/* $Id: lsrcp.c,v 1.3 2007/08/15 22:18:55 tmizan Exp $
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





extern void usage(char *cmd);
extern char *optarg;
extern int optind, opterr;

extern int mystat_(char *, struct stat *, struct hostent *);
extern int myopen_(char *, int, int, struct hostent *);

extern char **environ;

#define NL_SETN 27 

#define LSRCP_MSGSIZE   1048576 

void displayXfer( lsRcpXfer *lsXfer );
void doXferUsage(void);
int createXfer( lsRcpXfer *lsXfer );
int destroyXfer( lsRcpXfer *lsXfer );
int doXferOptions( lsRcpXfer *lsXfer, int argc, char *argv[] );

int 
main( int argc, char *argv[] )
{

    lsRcpXfer lsXfer;    
    int iCount;
    char* buf;
    int rc;

    rc = _i18n_init ( I18N_CAT_MIN );	

    
    Signal_(SIGUSR1, SIG_IGN);


    
    if (ls_initdebug(argv[0]) < 0) {
        ls_perror("ls_initdebug");
        exit(-1);
    }

    
    if (ls_initrex(1,0) == -1) {
        ls_perror("lsrcp: ls_initrex");
        return -1;
    }

    
    ls_rfcontrol(RF_CMD_RXFLAGS, REXF_CLNTDIR);

    
    if (setuid(getuid()) < 0) {
        perror("lsrcp: setuid");
        goto handle_error;
    }

    if (createXfer(&lsXfer)) {
        perror("lsrcp");
        goto handle_error;
    }    

    doXferOptions(&lsXfer, argc, argv);

     

    buf = (char*)malloc(LSRCP_MSGSIZE);
    if(!buf) {
        
        ls_donerex();
        ls_syslog(LOG_ERR, I18N_FUNC_FAIL_S,"lsrcp","main", 
	    _i18n_msg_get(ls_catd,NL_SETN,2301,"try rcp...")); /* catgets 2301 */
        if (doXferRcp(&lsXfer, 0) < 0)
            return -1;
        return 0;
    }
    for (iCount=0;iCount < lsXfer.iNumFiles; iCount++) {
        if (copyFile(&lsXfer, buf, 0)) {
                
                ls_donerex();
        	ls_syslog(LOG_ERR, I18N_FUNC_FAIL_S,"lsrcp","main",
		    _i18n_msg_get(ls_catd,NL_SETN,2301,"try rcp..."));
                if (doXferRcp(&lsXfer, 0) < 0)
		    return -1;
                return 0;
        } 
	if (logclass & (LC_FILE))
            ls_syslog(LOG_DEBUG, "main(), copy file succeeded.");
    } 
    free(buf);

    ls_donerex();

    if (destroyXfer(&lsXfer)) {
        perror("lsrcp");
        return -1;
    }


    _i18n_end ( ls_catd );			

    return 0;

handle_error:
    ls_donerex();
    return -1;

} 


void 
doXferUsage() 
{
    fprintf(stderr, "%s: lsrcp [-h] [-a] [-V] f1 f2\n", I18N_Usage );  
} 

void 
displayXfer( lsRcpXfer *lsXfer )
{

    if (lsXfer->szSourceArg)
        fprintf(stderr, "%s: %s\n",
	    _i18n_msg_get(ls_catd,NL_SETN,2308, "Source arg"),
	    lsXfer->szSourceArg); 

    if (lsXfer->szDestArg)
        fprintf(stderr, "%s: %s\n",
	    _i18n_msg_get(ls_catd,NL_SETN,2309, "Dest arg"), 
	    lsXfer->szDestArg); 

    if (lsXfer->szHostUser) 
        fprintf(stderr, "%s: %s\n",
	    _i18n_msg_get(ls_catd,NL_SETN,2310, "Host User"), 
	    lsXfer->szHostUser);

    if (lsXfer->szDestUser)
        fprintf(stderr, "%s: %s\n",
	    _i18n_msg_get(ls_catd,NL_SETN,2311, "Dest User"),
	    lsXfer->szDestUser);

    if (lsXfer->szHost)
        fprintf(stderr, "%s: %s\n",
	    _i18n_msg_get(ls_catd,NL_SETN,2310, "Host"), lsXfer->szHost); 

    if (lsXfer->szDest)
        fprintf(stderr, "%s: %s\n",
	    _i18n_msg_get(ls_catd,NL_SETN,2313, "Dest"), lsXfer->szDest); 

    fprintf(stderr, "%s: %d\n", 
	_i18n_msg_get(ls_catd,NL_SETN,2314, "Num, Files"), lsXfer->iNumFiles); 
    
    if (lsXfer->ppszHostFnames[0])
        fprintf(stderr, "%s: %s\n",
	    _i18n_msg_get(ls_catd,NL_SETN,2315, "Source Filename"),
	    lsXfer->ppszHostFnames[0]); 

    if (lsXfer->ppszDestFnames[0])
        fprintf(stderr, "%s: %s\n",
	    _i18n_msg_get(ls_catd,NL_SETN,2316, "Dest Filename"), 
	    lsXfer->ppszDestFnames[0]); 


    fprintf(stderr, "%s: %d\n", 
	_i18n_msg_get(ls_catd,NL_SETN,2317, "Options"),
	lsXfer->iOptions); 

} 

int 
doXferOptions( lsRcpXfer *lsXfer, int argc, char *argv[] )
{
    int c;

    while((c= getopt(argc, argv,"ahV")) != -1) {
        switch(c) {
            case 'a':
                
                lsXfer->iOptions |= O_APPEND;
                break;

            case 'V':
                
                fputs(_LAVALITE_VERSION_,stderr);
                exit(-1);

            case '?':
                doXferUsage();
                exit(-1);

            case 'h':
                doXferUsage();
                exit(-1);

            case ':':
                doXferUsage();
                exit(-1);

        } 
    } 

    if (argc >= 3 && argv[argc-2]) {
    lsXfer->szSourceArg = putstr_(argv[argc-2]);
        parseXferArg(argv[argc-2],&(lsXfer->szHostUser), 
                     &(lsXfer->szHost),
                     &(lsXfer->ppszHostFnames[0]));
    } else {
        doXferUsage();
        exit(-1);
    }

    if (argc >= 3 && argv[argc-1]) {
    lsXfer->szDestArg = putstr_(argv[argc-1]);
        parseXferArg(argv[argc-1],&(lsXfer->szDestUser),
                     &(lsXfer->szDest),
                     &(lsXfer->ppszDestFnames[0]));
    } else {
        doXferUsage();
        exit(-1);
    }
    return 0;

} 

