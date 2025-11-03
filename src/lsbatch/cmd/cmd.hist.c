/* $Id: cmd.hist.c,v 1.7 2007/08/15 22:18:44 tmizan Exp $
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

#include "lsbatch/cmd/cmd.h"

static int eventMatched = FALSE;

void displayEvent(struct eventRec *, struct histReq *);
static int isRequested(char *, char **);
extern char *myGetOpt (int nargc, char **, char *);
// Bug fix the includes
extern int ls_readconfenv(struct config_param *, char *);
int
sysHist(int argc, char **argv, int opCode)
{
    struct histReq req;
    int all = FALSE, eventFound;
    char **nameList=NULL;
    int numNames = 0;
    char *optName;

    req.opCode = opCode;
    req.names = NULL;
    req.eventFileName = NULL;
    req.eventTime[0] = 0;
    req.eventTime[1] = 0;

    while ((optName = myGetOpt(argc, argv, "t:|f:")) != NULL) {
	switch (optName[0]) {
	case 'f':
            if (strlen(optarg) > MAXFILENAMELEN -1) {
		fprintf(stderr, ("%s: File name too long\n"), optarg);
		return(-1);
            } else
	        req.eventFileName = optarg;
	    break;
        case 't':
            if (getBEtime(optarg, 't', req.eventTime) == -1) {
		ls_perror(optarg);
		return (-1);
            }
            break;
	default:
	    return(-2);
	}
    }

    switch (opCode) {
    case QUEUE_HIST:
    case HOST_HIST:
	if (argc > optind) {
	    numNames = getNames(argc, argv, optind, &nameList, &all, "queueC");
	    if (!all && numNames != 0) {
	        nameList[numNames] = NULL;
		req.names = nameList;
	    }
	}
	break;

    case MBD_HIST:
    case SYS_HIST:
        if (argc > optind)
            return(-2);
	break;

    default:
	fprintf(stderr, ("Unknown operation code\n"));
	return(-2);
    }

    return (searchEventFile(&req, &eventFound));

}

int
searchEventFile(struct histReq *req, int *eventFound)
{
    char eventFileName[MAXFILENAMELEN];
    char *clusterName;
    int lineNum = 0;
    struct stat statBuf;
    struct eventRec *log;
    FILE *log_fp;
    char *envdir;

    struct config_param histParams[] = {
#define LSB_SHAREDIR 0
        {"LSB_SHAREDIR", NULL},
        {NULL, NULL}
    };

    if ((envdir = getenv("LSF_ENVDIR")) == NULL)
	envdir = "/etc";

    if (ls_readconfenv(histParams, envdir) < 0) {
	ls_perror("ls_readconfenv");
	return(-1);
    }

    if (!req->eventFileName) {
        if ( (clusterName = ls_getclustername()) == NULL ) {
	    ls_perror("ls_getclustername()");
            return(-1);
        }
	memset(eventFileName,0,sizeof(eventFileName));
	ls_strcat(eventFileName,sizeof(eventFileName),histParams[LSB_SHAREDIR].paramValue);
	ls_strcat(eventFileName,sizeof(eventFileName),"/");
	ls_strcat(eventFileName,sizeof(eventFileName),clusterName);
	ls_strcat(eventFileName,sizeof(eventFileName),"/logdir/lsb.events");
    } else {
	memset(eventFileName,0,sizeof(eventFileName));
	ls_strcat(eventFileName,sizeof(eventFileName),req->eventFileName);
    }

    FREEUP (histParams[LSB_SHAREDIR].paramValue);

    if (stat(eventFileName, &statBuf) < 0) {
        perror(eventFileName);
        return (-1);
    } else if ((statBuf.st_mode & S_IFREG) != S_IFREG ) {
        fprintf(stderr, "%s: Not a regular file\n", eventFileName);
        return(-1);
    }

    if ((log_fp = fopen(eventFileName, "r")) == NULL) {
	perror(eventFileName);
	return(-1);
    }

    eventMatched = FALSE;
    while (TRUE) {
	if ((log = lsb_geteventrec(log_fp, &lineNum)) != NULL) {
	    displayEvent(log, req);
	    continue;
        }
        if (lsberrno == LSBE_EOF)
	    break;
	fprintf(stderr, ("File %s at line %d: %s\n"), eventFileName, lineNum,
		                 lsb_sysmsg());
    }
    if (!eventMatched)
        fprintf(stderr, ("No matching event found\n"));
    *eventFound = eventMatched;
    fclose(log_fp);
    return 0;

}

void
displayEvent(struct eventRec *log, struct histReq *req)
{
    char prline[MSGSIZE];
    char localTimeStr[60];

    if (req->eventTime[1] != 0
	&& req->eventTime[0] < req->eventTime[1]) {
	if (log->eventTime < req->eventTime[0]
	    || log->eventTime > req->eventTime[1])
            return;
    }

    switch (log->type) {
    case EVENT_LOG_SWITCH:
         if (req->opCode == MBD_HIST || req->opCode == SYS_HIST) {
            eventMatched = TRUE;
	    strcpy ( localTimeStr, ctime2(&log->eventTime));
            sprintf(prline, ("%s: event file is switched; last JobId <%d>\n"),
            localTimeStr, log->eventLog.logSwitchLog.lastJobId);
            prtLine(prline);
        }
        break;
    case EVENT_MBD_START:
	if (req->opCode == MBD_HIST || req->opCode == SYS_HIST) {
	    strcpy ( localTimeStr, ctime2(&log->eventTime));
            eventMatched = TRUE;
	    sprintf(prline, ("%s: mbatchd started on host <%s>; cluster name <%s>, %d server hosts, %d queues.\n"),
		    localTimeStr,
		    log->eventLog.mbdStartLog.master,
		    log->eventLog.mbdStartLog.cluster,
		    log->eventLog.mbdStartLog.numHosts,
		    log->eventLog.mbdStartLog.numQueues);
	    prtLine(prline);
	}
        break;
    case EVENT_MBD_DIE:
	if (req->opCode == MBD_HIST || req->opCode == SYS_HIST) {
            eventMatched = TRUE;
	    strcpy ( localTimeStr, ctime2(&log->eventTime));
	    sprintf(prline, ("%s: mbatchd on host <%s> died: "),
		    localTimeStr,
		    log->eventLog.mbdStartLog.master);
	    prtLine(prline);
            switch (log->eventLog.mbdDieLog.exitCode) {
            case MASTER_RESIGN:
                sprintf(prline, ("master resigned.\n"));
                break;
	    case MASTER_RECONFIG:
                sprintf(prline, ("reconfiguration initiated.\n"));
                break;
	    case MASTER_FATAL:
                sprintf(prline, ("fatal errors.\n"));
                break;
	    case MASTER_MEM:
                sprintf(prline, ("fatal memory errors.\n"));
                break;
	    case MASTER_CONF:
                sprintf(prline, ("bad configuration file.\n"));
                break;
            default:
                sprintf(prline, ("killed by signal <%d>.\n"),
                        log->eventLog.mbdDieLog.exitCode);
                break;
	    }
            prtLine(prline);
	}
        break;
    case EVENT_QUEUE_CTRL:
	if ((req->opCode == SYS_HIST)
	    || (req->opCode == QUEUE_HIST
		&& isRequested(log->eventLog.queueCtrlLog.queue,
			       req->names))) {
	    strcpy ( localTimeStr, ctime2(&log->eventTime));
            eventMatched = TRUE;
	    sprintf(prline, "%s: %s <%s> ",
		    localTimeStr,
		    "Queue",
	            log->eventLog.queueCtrlLog.queue);
            prtLine(prline);
	    switch (log->eventLog.queueCtrlLog.opCode) {
	    case QUEUE_OPEN:
		sprintf(prline, "opened");
		break;
	    case QUEUE_CLOSED:
		sprintf(prline, "closed");
		break;
	    case QUEUE_ACTIVATE:
		sprintf(prline, "activated");
		break;
 	    case QUEUE_INACTIVATE:
		sprintf(prline, "inactivated");
		break;
            default:
                sprintf(prline, "%s <%d>",
			"unknown operation code",
                        log->eventLog.queueCtrlLog.opCode);
                break;
	    }
            prtLine(prline);
            if (log->eventLog.queueCtrlLog.userName
		&& log->eventLog.queueCtrlLog.userName[0])
                sprintf(prline, (" by user or administrator <%s>.\n"), log->eventLog.queueCtrlLog.userName);
             else
                sprintf(prline, ".\n");

	    prtLine(prline);
    	}
	break;
    case EVENT_HOST_CTRL:
	if ((req->opCode == SYS_HIST)
	     || (req->opCode == HOST_HIST
		 && isRequested(log->eventLog.hostCtrlLog.host, req->names))) {
            eventMatched = TRUE;
	    strcpy ( localTimeStr, ctime2(&log->eventTime));
	    sprintf(prline, "%s: %s <%s> ",
		    localTimeStr,
		    "Host",
	            log->eventLog.hostCtrlLog.host);
 	    prtLine(prline);
	    switch (log->eventLog.hostCtrlLog.opCode) {
	    case HOST_OPEN:
		sprintf(prline, ("opened"));
		break;
	    case HOST_CLOSE:
		sprintf(prline, ("closed"));
		break;
	    case HOST_REBOOT:
		sprintf(prline, ("rebooted"));
		break;
	    case HOST_SHUTDOWN:
		sprintf(prline, ("shutdown"));
		break;
            default:
                sprintf(prline, "%s <%d>",
		        "unknown operation code",
                        log->eventLog.hostCtrlLog.opCode);
                break;
	    }
	    prtLine(prline);
            if (log->eventLog.hostCtrlLog.userName
	        && log->eventLog.hostCtrlLog.userName[0])
                sprintf(prline, (" by administrator <%s>.\n"), log->eventLog.hostCtrlLog.userName);
             else
                sprintf(prline, ".\n");
            prtLine(prline);
	}
        break;

    default:
	break;
    }
}

static int
isRequested(char *name, char **nameList)
{
    int  i = 0;

    if (!nameList)
	return(TRUE);

    while (nameList[i]) {
	if (strcmp(name, nameList[i++]) == 0)
	    return(TRUE);
    }

    return(FALSE);
}
