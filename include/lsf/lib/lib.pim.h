/* $Id: lib.pim.h,v 1.4 2007/08/15 22:18:51 tmizan Exp $
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

#ifndef _LIB_PIM_H_
#define _LIB_PIM_H_

#include <time.h>

enum lsPStatType {
    LS_PSTAT_RUNNING,
    LS_PSTAT_INTERRUPTIBLE,
    LS_PSTAT_UNINTERRUPTIBLE,
    LS_PSTAT_ZOMBI,
    LS_PSTAT_STOPPED,
    LS_PSTAT_SWAPPED,
    LS_PSTAT_SLEEP,
    LS_PSTAT_EXITING
};


struct lsPidInfo {
    int pid;            
    int ppid;           
    int pgid;           
    int jobid;		
    int utime;          
                        
    int stime;          
    int cutime;         
    int cstime;         
    int proc_size;      
    int resident_size;  
    int stack_size;     
    enum lsPStatType status;	

    char *command;	
};

#define PIM_API_TREAT_JID_AS_PGID 0x1 
#define PIM_API_UPDATE_NOW        0x2 

#define PIM_SLEEP_TIME 30

extern struct jRusage *getJInfo_(int, int*, int, int);

#endif 
