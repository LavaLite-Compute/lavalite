/* $Id: lsadmin.h,v 1.5 2007/08/15 22:18:54 tmizan Exp $
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>


#include "../../config.h"
#include "../lsf.h"
#include "../lib/lproto.h"

extern int limCtrl(int argc, char **argv, int opCode);
extern int limLock(int argc, char **argv);
extern int limUnlock(int argc, char **argv);
extern int resCtrl(int argc, char **argv, int opCode);
extern int checkConf(int verbose, int who);
extern int oneLimDebug(struct debugReq *p, char *host);
extern int oneResDebug(struct debugReq *p, char *host);

extern void parseAndDo (char *cmdBuf, int (*func)());
extern int adminCmdIndex(char *cmd, char *cmdList[]);
extern void cmdsUsage (char *cmd, char *cmdList[], char *cmdInfo[]);
extern void oneCmdUsage (int i, char *cmdList[], char *cmdSyntax[]);
extern void cmdHelp (int argc, char **argv, char *cmdList[], 
		     char *cmdInfo[], char *cmdSyntax[]);
extern char *myGetOpt (int nargc, char **nargv, char *ostr);
extern int getConfirm (char *msg);

extern int optind;

#define NL_SETN   25   

#define LSADM_RECONFIG    0
#define LSADM_CKCONFIG    1
#define LSADM_LIMREBOOT   2
#define LSADM_LIMSTARTUP  3
#define LSADM_LIMSHUTDOWN 4
#define LSADM_LIMLOCK     5
#define LSADM_LIMUNLOCK   6
#define LSADM_RESREBOOT   7
#define LSADM_RESSTARTUP  8
#define LSADM_RESSHUTDOWN 9
#define LSADM_RESLOGON   10
#define LSADM_RESLOGOFF  11 
#define LSADM_LIMDEBUG   12 
#define LSADM_LIMTIME    13 
#define LSADM_RESDEBUG   14 
#define LSADM_RESTIME    15 
#define LSADM_HELP       16
#define LSADM_QES        17
#define LSADM_QUIT       18
