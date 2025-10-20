/* $Id: lsb.log.h,v 1.4 2007/08/15 22:18:47 tmizan Exp $
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

#include <string.h>
#include <stdlib.h>

#include "lsb.h"

#define MAXFILENAMELEN          256  
#define MAXVERSIONLEN            12  
#define MAXEVENTNAMELEN          12  


struct newJobLog {
    LS_LONG_INT jobId;
    int    userId;
    int    flags;
    int    nproc;
    time_t submitTime;
    time_t startTime;
    time_t termTime;
    int    sigval;
    int    chkperiod;
    int    restartpid;
    LS_LONG_INT rjobId;
    int    limits[MAX_NRLIMITS];
    int    mailUserId;
    int    umask;
    char   queue[MAXQUEUENAMELEN];
    char   resReq[MAXLINELEN];
    char   fromHost[MAXHOSTNAMELEN];
    char   cwd[MAXPATHLEN];
    char   chkdir[MAXFILENAMELEN];
    char   inFile[MAXFILENAMELEN];
    char   outFile[MAXFILENAMELEN];
    char   errFile[MAXFILENAMELEN];
    char   jobFile[MAXFILENAMELEN];
    int    numUsHosts;
    char   **usHosts;
    char   jobName[MAXJOBDESPLEN];
    char   command[MAXJOBDESPLEN];
};

struct startJobLog {
    LS_LONG_INT jobId;
    int    status;
    int    jobPid;
    int    jobPGid;
    int    numExHosts;
    char   **execHosts;
};

struct newStatusLog {
    int    jobId;
    int    status;
    int    reasons;
    float  cpuTime;
    time_t doneTime;
    time_t delayTime;
};

struct qControlLog {
    int    opCode;
    char   queue[MAXQUEUENAMELEN];
};

struct switchJobLog {
    int    userId;
    LS_LONG_INT jobId;
    char   queue[MAXQUEUENAMELEN];
};

struct moveJobLog {
    int    userId;
    LS_LONG_INT jobId;
    int    pos;
    int    top;
};

struct paramsLog {
    int    nextId;
    int    job_count;
};

struct chkpntLog {
    LS_LONG_INT jobId;
    int    chkperiod;
};

struct finishJobLog {
    LS_LONG_INT jobId;
    int    userId;
    int    flags;
    int    nproc;
    int    status;
    time_t submitTime;
    time_t startTime;
    time_t termTime;
    time_t dispatchTime;
    time_t doneTime;
    char   queue[MAXQUEUENAMELEN];
    char   resReq[MAXLINELEN];
    char   fromHost[MAXHOSTNAMELEN];
    char   cwd[MAXPATHLEN];
    char   inFile[MAXFILENAMELEN];
    char   outFile[MAXFILENAMELEN];
    char   errFile[MAXFILENAMELEN];
    char   jobFile[MAXFILENAMELEN];
    int    numUsHosts;
    char   **usHosts;
    float  cpuTime;
    char   jobName[MAXJOBDESPLEN];
    char   command[MAXJOBDESPLEN];
};

union  eventLog {
    struct newJobLog newJobLog;
    struct startJobLog startJobLog;
    struct newStatusLog newStatusLog;
    struct qControlLog qControlLog;
    struct switchJobLog switchJobLog;
    struct moveJobLog moveJobLog;
    struct paramsLog paramsLog;
    struct chkpntLog chkpntLog;
    struct finishJobLog finishJobLog;
};

struct eventRec {
    char   version[MAXVERSIONLEN];
    int    type;
    time_t eventTime;
    union  eventLog eventLog;
};

extern int putEventRec(FILE *, struct eventRec *);
extern struct eventRec *getEventRec(char *);
extern char *getNextValue0(char **line, char, char);

