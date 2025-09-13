/* $Id: mls.h,v 1.5 2007/08/15 22:18:51 tmizan Exp $
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
#ifndef LSF_MLS_H
#define LSF_MLS_H

typedef enum {
    MLS_FATAL,		
    MLS_INVALID,	
    MLS_CLEARANCE,	
    MLS_RHOST,		
    MLS_DOMINATE	
} mlsErrCode;

extern int mlsSbdMode;			

#define lsfSetUid(uid)              lsfSetXUid(0, uid, uid, -1, setuid)

#define lsfSetEUid(uid)             lsfSetXUid(0, -1, uid, -1, seteuid)

#define lsfSetREUid(ruid, euid)	    lsfSetXUid(0, ruid, euid, -1, setreuid)

#define lsfExecv(path, argv)	    lsfExecX(path, argv, execv)
#define lsfExecvp(file, argv)	    lsfExecX(file, argv, execvp)

extern int lsfSetXUid(int, int, int, int, int(*)());
extern void lsfExecLog(const char *);
extern int lsfExecX(char *, char **argv, int(*)());

#endif 
