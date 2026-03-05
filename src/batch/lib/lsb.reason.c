/* $Id: lsb.reason.c,v 1.7 2007/08/15 22:18:47 tmizan Exp $
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 USA
 *
 */

#include "lsbatch/lib/lsb.h"

static char msgbuf[MSGSIZE];
char msgline[MAXLINELEN];
struct msgMap {
    int number;
    char *message;
};

static void userIndexReasons(char *, int, int, struct loadIndexLog *);
static char *getMsg(struct msgMap *msgMap, int *msgId, int number);
static void getMsgByRes(int, int, char **, struct loadIndexLog *);

static char *getMsg(struct msgMap *msgMap, int *msg_ID, int number)
{
    int i;

    for (i = 0; msgMap[i].message != NULL; i++)
        if (msgMap[i].number == number)
            return (msgMap[i].message);

    return ("");
}

struct susp_msg_map {
    int code;
    const char *msg;
};

char *lsb_suspreason(int reasons, int subreasons, struct loadIndexLog *ld)
{
    static char fname[] = "lsb_suspreason";
    msgbuf[0] = '\0';

    if (logclass & (LC_TRACE | LC_SCHED | LC_EXEC))
        ls_syslog(LOG_DEBUG1, "%s: reasons=%x, subreasons=%d", fname, reasons,
                  subreasons);

    if (reasons & SUSP_USER_STOP)
        sprintf(msgbuf, " The job was suspended by user;\n");
    else if (reasons & SUSP_ADMIN_STOP)
        sprintf(msgbuf, " The job was suspended by LSF admin or root;\n");
    else if (reasons & SUSP_QUEUE_WINDOW)
        sprintf(msgbuf, " The run windows of the queue are closed;\n");
    else if (reasons & SUSP_HOST_LOCK)
        sprintf(msgbuf,
                " The execution host is locked by LSF administrator now;\n");
    else if (reasons & SUSP_HOST_LOCK_MASTER) {
        sprintf(msgbuf, " The execution host is locked by master LIM now;\n");
    } else if (reasons & SUSP_USER_RESUME)
        sprintf(msgbuf,
                " Waiting for re-scheduling after being resumed by user;\n");
    else if (reasons & SUSP_QUE_STOP_COND)
        sprintf(msgbuf, " STOP_COND is true with current host load;\n");
    else if (reasons & SUSP_QUE_RESUME_COND)
        sprintf(msgbuf, " RESUME_COND is false with current host load;\n");
    else if (reasons & SUSP_RES_RESERVE)
        sprintf(
            msgbuf,
            " Job's requirements for resource reservation not satisfied;\n");
    else if (reasons & SUSP_PG_IT)
        sprintf(msgbuf, " Job was suspended due to paging rate (pg) and the "
                        "host is not idle yet\n");

    else if (reasons & SUSP_LOAD_UNAVAIL)
        sprintf(msgbuf,
                " Load information on execution host(s) is unavailable\n");
    else if (reasons & SUSP_LOAD_REASON) {
        strcpy(msgbuf, " Host load exceeded threshold: ");
        if (subreasons == R15S)
            sprintf(msgbuf, "%s 15-second CPU run queue length (r15s)\n",
                    msgbuf);
        else if (subreasons == R1M)
            sprintf(msgbuf, "%s 1-minute CPU run queue length (r1m)\n", msgbuf);
        else if (subreasons == R15M)
            sprintf(msgbuf, "%s 15-minute CPU run queue length (r15m)\n",
                    msgbuf);
        else if (subreasons == UT)
            sprintf(msgbuf, "%s 1-minute CPU utilization (ut)\n", msgbuf);
        else if (subreasons == IO)
            sprintf(msgbuf, "%s Disk IO rate (io)\n", msgbuf);
        else if (subreasons == PG)
            sprintf(msgbuf, "%s Paging rate (pg)\n", msgbuf);
        else if (subreasons == IT)
            sprintf(msgbuf, "%s Idle time (it)\n", msgbuf);
        else if (subreasons == MEM)
            sprintf(msgbuf, "%s Available memory (mem)\n", msgbuf);
        else if (subreasons == SWP)
            sprintf(msgbuf, "%s Available swap space (swp)\n", msgbuf);
        else if (subreasons == TMP)
            sprintf(msgbuf, "%s Available /tmp space (tmp)\n", msgbuf);
        else if (subreasons == LS)
            sprintf(msgbuf, "%s Number of login sessions (ls)\n", msgbuf);
        else {
            userIndexReasons(msgline, 0, subreasons, ld);
            strcat(msgbuf, "  ");
            strcat(msgbuf, msgline);
            strcat(msgbuf, "\n");
        }
    } else if (reasons & SUSP_RES_LIMIT) {
        if (subreasons & SUB_REASON_RUNLIMIT)
            sprintf(msgbuf, " RUNLIMIT was reached;\n");
        else if (subreasons & SUB_REASON_DEADLINE)
            sprintf(msgbuf, " DEADLINE was reached;\n");
        else if (subreasons & SUB_REASON_PROCESSLIMIT)
            sprintf(msgbuf, " PROCESSLIMIT was reached;\n");
        else if (subreasons & SUB_REASON_CPULIMIT)
            sprintf(msgbuf, " CPULIMIT was reached;\n");
        else if (subreasons & SUB_REASON_MEMLIMIT)
            sprintf(msgbuf, " MEMLIMIT was reached;\n");
    } else
        sprintf(msgbuf, " Unknown suspending reason code: %d\n", reasons);

    return msgbuf;
}
char *lsb_pendreason(int numReasons, int *rsTb, struct jobInfoHead *jInfoH,
                     struct loadIndexLog *ld)
{
    static char fname[] = "lsb_pendreason";
    int i, j, num;
    int hostId, reason, hostIdJ, reasonJ;
    static int *reasonTb, memSize = 0;
    static char *hostList = NULL, *retMsg = NULL;
    char *sp;

    int pendMsg_ID[] = {
        550, 551, 552, 553, 554, 555, 556, 557, 558, 559, 560, 561, 562,
        563, 564, 566, 567, 568, 571, 583, 655, 656, 586, 587, 588, 589,
        590, 591, 592, 596, 597, 598, 599, 600, 662, 601, 602, 603, 604,
        605, 606, 607, 608, 609, 610, 611, 612, 614, 615, 616, 617, 618,
        619, 667, 620, 621, 622, 623, 624, 625, 626, 627, 628, 629, 630,
        631, 633, 634, 635, 636, 638, 639, 640, 641, 642, 664, 646, 647,
        648, 649, 650, 651, 652, 653, 654, 644, 645, 665, 666};

    struct msgMap pendMsg[] = {
        {PEND_JOB_NEW, "New job is waiting for scheduling"},
        {PEND_JOB_START_TIME, "The job has a specified start time"},
        {PEND_JOB_DEPEND, "Job dependency condition not satisfied"},
        {PEND_JOB_DEP_INVALID,
         "Dependency condition invalid or never satisfied"},
        {PEND_JOB_MIG, "Migrating job is waiting for rescheduling"},
        {PEND_JOB_PRE_EXEC,
         "The job's pre-exec command exited with non-zero status"},
        {PEND_JOB_NO_FILE, "Unable to access job file"},
        {PEND_JOB_ENV, "Unable to set job's environment variables"},
        {PEND_JOB_PATHS, "Unable to determine job's home/working directories"},
        {PEND_JOB_OPEN_FILES, "Unable to open job's I/O buffers"},
        {PEND_JOB_EXEC_INIT, "Job execution initialization failed"},
        {PEND_JOB_RESTART_FILE,
         "Unable to copy restarting job's checkpoint files"},
        {PEND_JOB_DELAY_SCHED,
         "The schedule of the job is postponed for a while"},
        {PEND_JOB_SWITCH, "Waiting for re-scheduling after switching queue"},
        {PEND_JOB_DEP_REJECT,
         "Event is rejected by eeventd due to syntax error"},
        {PEND_JOB_NO_PASSWD, "Failed to get user password"},
        {PEND_JOB_MODIFY,
         "Waiting for re-scheduling after parameters have been changed"},
        {PEND_JOB_REQUEUED, "Requeue the job for the next run"},
        {PEND_SYS_UNABLE, "System is unable to schedule the job"},
        {PEND_JOB_ARRAY_JLIMIT,
         "The job array has reached its running element limit"},
        {PEND_CHKPNT_DIR, "Checkpoint directory is invalid"},

        {PEND_QUE_INACT, "The queue is inactivated by the administrator"},
        {PEND_QUE_WINDOW, "The queue is inactivated by its time windows"},
        {PEND_QUE_JOB_LIMIT, "The queue has reached its job slot limit"},
        {PEND_QUE_PJOB_LIMIT,
         "The queue has not enough job slots for the parallel job"},
        {PEND_QUE_USR_JLIMIT,
         "User has reached the per-user job slot limit of the queue"},
        {PEND_QUE_USR_PJLIMIT,
         "Not enough per-user job slots of the queue for the parallel job"},
        {PEND_QUE_PRE_FAIL,
         "The queue's pre-exec command exited with non-zero status"},
        {PEND_SYS_NOT_READY,
         "System is not ready for scheduling after reconfiguration"},
        {PEND_SBD_JOB_REQUEUE, "Requeued job is waiting for rescheduling"},
        {PEND_JOB_SPREAD_TASK,
         "Not enough hosts to meet the job's spanning requirement"},
        {PEND_QUE_SPREAD_TASK,
         "Not enough hosts to meet the queue's spanning requirement"},
        {PEND_QUE_WINDOW_WILL_CLOSE,
         "Job will not finish before queue's run window is closed"},
        {PEND_QUE_PROCLIMIT,
         "Job no longer satisfies queue PROCLIMIT configuration"},
        {PEND_USER_JOB_LIMIT, "The user has reached his/her job slot limit"},
        {PEND_UGRP_JOB_LIMIT,
         "One of the user's groups has reached its job slot limit"},
        {PEND_USER_PJOB_LIMIT,
         "The user has not enough job slots for the parallel job"},
        {PEND_UGRP_PJOB_LIMIT,
         "One of user's groups has not enough job slots for the parallel job"},
        {PEND_USER_RESUME, "Waiting for scheduling after resumed by user"},
        {PEND_USER_STOP, "The job was suspended by the user while pending"},
        {PEND_ADMIN_STOP,
         "The job was suspended by LSF admin or root while pending"},
        {PEND_NO_MAPPING, "Unable to determine user account for execution"},
        {PEND_RMT_PERMISSION,
         "The user has no permission to run the job on remote host/cluster"},

        {PEND_HOST_RES_REQ, "Job's resource requirements not satisfied"},
        {PEND_HOST_NONEXCLUSIVE,
         "Job's requirement for exclusive execution not satisfied"},
        {PEND_HOST_JOB_SSUSP,
         "Higher or equal priority jobs suspended by host load"},
        {PEND_SBD_GETPID, "Unable to get the PID of the restarting job"},
        {PEND_SBD_LOCK,
         "Unable to lock host for exclusively executing the job"},
        {PEND_SBD_ZOMBIE, "Cleaning up zombie job"},
        {PEND_SBD_ROOT, "Can't run jobs submitted by root"},
        {PEND_HOST_WIN_WILL_CLOSE,
         "Job will not finish on the host before queue's run window is closed"},
        {PEND_HOST_MISS_DEADLINE,
         "Job will not finish on the host before job's termination deadline"},
        {PEND_FIRST_HOST_INELIGIBLE, "The specified first exection host is not "
                                     "eligible for this job at this time"},
        {PEND_HOST_DISABLED, "Closed by LSF administrator"},
        {PEND_HOST_LOCKED, "Host is locked by LSF administrator"},
        {PEND_HOST_LESS_SLOTS, "Not enough job slot(s)"},
        {PEND_HOST_WINDOW, "Dispatch windows closed"},
        {PEND_HOST_JOB_LIMIT, "Job slot limit reached"},
        {PEND_QUE_PROC_JLIMIT, "Queue's per-CPU job slot limit reached"},
        {PEND_QUE_HOST_JLIMIT, "Queue's per-host job slot limit reached"},
        {PEND_USER_PROC_JLIMIT, "User's per-CPU job slot limit reached"},
        {PEND_UGRP_PROC_JLIMIT, "User group's per-CPU job slot limit reached"},
        {PEND_HOST_USR_JLIMIT, "Host's per-user job slot limit reached"},
        {PEND_HOST_QUE_MEMB, "Not usable to the queue"},
        {PEND_HOST_USR_SPEC, "Not specified in job submission"},
        {PEND_HOST_NO_USER, "There is no such user account"},
        {PEND_HOST_ACCPT_ONE, "Just started a job recently"},
        {PEND_LOAD_UNAVAIL, "Load information unavailable"},
        {PEND_HOST_NO_LIM, "LIM is unreachable now"},
        {PEND_HOST_QUE_RESREQ, "Queue's resource requirements not satisfied"},
        {PEND_HOST_SCHED_TYPE, "Not the same type as the submission host"},
        {PEND_JOB_NO_SPAN,
         "Not enough processors to meet the job's spanning requirement"},
        {PEND_QUE_NO_SPAN,
         "Not enough processors to meet the queue's spanning requirement"},
        {PEND_HOST_EXCLUSIVE, "Running an exclusive job"},
        {PEND_HOST_LOCKED_MASTER, "Host is locked by master LIM"},
        {PEND_SBD_UNREACH, "Unable to reach slave batch server"},
        {PEND_SBD_JOB_QUOTA, "Number of jobs exceeds quota"},
        {PEND_JOB_START_FAIL, "Failed in talking to server to start the job"},
        {PEND_JOB_START_UNKNWN,
         "Failed in receiving the reply from server when starting the job"},
        {PEND_SBD_NO_MEM, "Unable to allocate memory to run job"},
        {PEND_SBD_NO_PROCESS, "Unable to fork process to run job"},
        {PEND_SBD_SOCKETPAIR, "Unable to communicate with job process"},
        {PEND_SBD_JOB_ACCEPT, "Slave batch server failed to accept job"},
        {PEND_HOST_LOAD, "Load threshold reached"},

        {PEND_HOST_QUE_RUSAGE,
         "Queue's requirements for resource reservation not satisfied"},
        {PEND_HOST_JOB_RUSAGE,
         "Job's requirements for resource reservation not satisfied"},
        {PEND_BAD_HOST, "Bad host name, host group name or cluster name"},

        {PEND_QUEUE_HOST, "Host or host group is not used by the queue"},

        {0, NULL}};

    if (logclass & (LC_TRACE | LC_SCHED | LC_EXEC))
        ls_syslog(LOG_DEBUG1, "%s: numReasons=%d", fname, numReasons);

    if (!numReasons || !rsTb) {
        lsberrno = LSBE_BAD_ARG;
        return ("");
    }
    if (memSize < numReasons) {
        FREEUP(reasonTb);
        reasonTb = (int *) calloc(numReasons, sizeof(int));
        if (!reasonTb) {
            memSize = 0;
            lsberrno = LSBE_NO_MEM;
            return ("");
        }
        memSize = numReasons;
    }
    for (i = 0; i < numReasons; i++)
        reasonTb[i] = rsTb[i];

    FREEUP(hostList);
    FREEUP(retMsg);
    if (jInfoH && jInfoH->numHosts != 0 && jInfoH->hostNames != NULL) {
        hostList = malloc(jInfoH->numHosts * MAXHOSTNAMELEN);
        retMsg = malloc(jInfoH->numHosts * MAXHOSTNAMELEN + MSGSIZE);
        if (hostList == NULL || retMsg == NULL) {
            lsberrno = LSBE_NO_MEM;
            return ("");
        }
    } else {
        retMsg = malloc(MSGSIZE);
        if (retMsg == NULL) {
            lsberrno = LSBE_NO_MEM;
            return ("");
        }
    }

    retMsg[0] = '\0';
    for (i = 0; i < numReasons; i++) {
        if (!reasonTb[i])
            continue;
        GET_LOW(reason, reasonTb[i]);
        if (!reason)
            continue;
        GET_HIGH(hostId, reasonTb[i]);
        if (logclass & (LC_TRACE | LC_SCHED | LC_EXEC))
            ls_syslog(LOG_DEBUG2, "%s: hostId=%d, reason=%d reasonTb[%d]=%d",
                      fname, hostId, reason, i, reasonTb[i]);
        if (!hostId) {
            sprintf(msgline, " %s;\n", getMsg(pendMsg, pendMsg_ID, reason));
            strcat(retMsg, msgline);
            continue;
        }
        if (jInfoH && jInfoH->numHosts != 0 && jInfoH->hostNames != NULL)
            strcpy(hostList, jInfoH->hostNames[hostId - 1]);
        else
            num = 1;

        for (j = i + 1; j < numReasons; j++) {
            if (reasonTb[j] == 0)
                continue;
            GET_LOW(reasonJ, reasonTb[j]);
            if (logclass & (LC_TRACE | LC_SCHED | LC_EXEC))
                ls_syslog(LOG_DEBUG2, "%s: reasonJ=%d reasonTb[j]=%d", fname,
                          reasonJ, reasonTb[j]);
            if (reasonJ != reason)
                continue;
            GET_HIGH(hostIdJ, reasonTb[j]);
            if (logclass & (LC_TRACE | LC_SCHED | LC_EXEC))
                ls_syslog(LOG_DEBUG2, "%s: j=%d, hostIdJ=%d", fname, j,
                          hostIdJ);
            reasonTb[j] = 0;
            if (jInfoH && jInfoH->numHosts != 0 && jInfoH->hostNames != NULL) {
                sprintf(hostList, "%s, %s", hostList,
                        jInfoH->hostNames[hostIdJ - 1]);
            } else
                num++;
        }
        if (reason >= PEND_HOST_LOAD && reason < PEND_HOST_QUE_RUSAGE) {
            getMsgByRes(reason - PEND_HOST_LOAD, PEND_HOST_LOAD, &sp, ld);

        } else if (reason >= PEND_HOST_QUE_RUSAGE &&
                   reason < PEND_HOST_JOB_RUSAGE) {
            getMsgByRes(reason - PEND_HOST_QUE_RUSAGE, PEND_HOST_QUE_RUSAGE,
                        &sp, ld);

        } else if (reason >= PEND_HOST_JOB_RUSAGE) {
            getMsgByRes(reason - PEND_HOST_JOB_RUSAGE, PEND_HOST_JOB_RUSAGE,
                        &sp, ld);

        } else {
            sp = getMsg(pendMsg, pendMsg_ID, reason);
        }

        if (jInfoH && jInfoH->numHosts != 0 && jInfoH->hostNames != NULL)
            sprintf(retMsg, "%s %s: %s;\n", retMsg, sp, hostList);
        else if (num == 1)
            sprintf(retMsg, "%s %s: 1 host;\n", retMsg, sp);
        else
            sprintf(retMsg, "%s %s: %d hosts;\n", retMsg, sp, num);
    }

    return retMsg;
}

static void userIndexReasons(char *msgline, int resource, int reason,
                             struct loadIndexLog *ld)
{
    if (ld == NULL || reason <= MEM || resource >= ld->nIdx) {
        sprintf(msgline, "External load index is beyond threshold");
        return;
    }

    if (reason == PEND_HOST_LOAD) {
        sprintf(msgline, "External load index (%s) is beyond threshold",
                ld->name[resource]);
    } else if (reason == PEND_HOST_QUE_RUSAGE) {
        sprintf(msgline,
                "Queue requirements for reserving resource (%s) not satisfied",
                ld->name[resource]);
    } else if (reason == PEND_HOST_JOB_RUSAGE) {
        sprintf(msgline,
                "Job's requirements for reserving resource (%s) not satisfied",
                ld->name[resource]);
    }
}

static void getMsgByRes(int resource, int reason, char **sp,
                        struct loadIndexLog *ld)
{
    switch (resource) {
    case R15S:

        if (reason == PEND_HOST_LOAD) {
            sprintf(msgline, "\
                            The 15s effective CPU queue length (r15s) is beyond threshold");
        } else if (reason == PEND_HOST_QUE_RUSAGE) {
            sprintf(msgline, "Queue requirements for reserving resource (r15s) "
                             "not satisfied ");
        } else if (reason == PEND_HOST_JOB_RUSAGE) {
            sprintf(
                msgline,
                "Job requirements for reserving resource (r15s) not satisfied");
        }
        break;

    case R1M:

        if (reason == PEND_HOST_LOAD) {
            sprintf(msgline, "The 1 min effective CPU queue length (r1m) is "
                             "beyond threshold");
        } else if (reason == PEND_HOST_QUE_RUSAGE) {
            sprintf(msgline, "Queue requirements for reserving resource (r1m) "
                             "not satisfied ");
        } else if (reason == PEND_HOST_JOB_RUSAGE) {
            sprintf(
                msgline,
                "Job requirements for reserving resource (r1m) not satisfied");
        }
        break;

    case R15M:

        if (reason == PEND_HOST_LOAD) {
            sprintf(msgline, "The 15 min effective CPU queue length (r15m) is "
                             "beyond threshold");
        } else if (reason == PEND_HOST_QUE_RUSAGE) {
            sprintf(msgline, "Queue requirements for reserving resource (r15m) "
                             "not satisfied ");
        } else if (reason == PEND_HOST_JOB_RUSAGE) {
            sprintf(
                msgline,
                "Job requirements for reserving resource (r15m) not satisfied");
        }
        break;

    case UT:

        if (reason == PEND_HOST_LOAD) {
            sprintf(msgline, "The CPU utilization (ut) is beyond threshold");
        } else if (reason == PEND_HOST_QUE_RUSAGE) {
            sprintf(msgline, "Queue requirements for reserving resource (ut) "
                             "not satisfied ");
        } else if (reason == PEND_HOST_JOB_RUSAGE) {
            sprintf(
                msgline,
                "Job requirements for reserving resource (ut) not satisfied");
        }
        break;

    case PG:

        if (reason == PEND_HOST_LOAD) {
            sprintf(msgline, "The paging rate (pg) is beyond threshold");
        } else if (reason == PEND_HOST_QUE_RUSAGE) {
            sprintf(
                msgline,
                "Queue requirements for reserving resource (pg) not satisfied");
        } else if (reason == PEND_HOST_JOB_RUSAGE) {
            sprintf(
                msgline,
                "Job requirements for reserving resource (pg) not satisfied");
        }
        break;

    case IO:

        if (reason == PEND_HOST_LOAD) {
            sprintf(msgline, "The disk IO rate (io) is beyond threshold");
        } else if (reason == PEND_HOST_QUE_RUSAGE) {
            sprintf(
                msgline,
                "Queue requirements for reserving resource (io) not satisfied");
        } else if (reason == PEND_HOST_JOB_RUSAGE) {
            sprintf(
                msgline,
                "Job requirements for reserving resource (io) not satisfied");
        }
        break;

    case LS:

        if (reason == PEND_HOST_LOAD) {
            sprintf(msgline, "There are too many login users (ls)");
        } else if (reason == PEND_HOST_QUE_RUSAGE) {
            sprintf(
                msgline,
                "Queue requirements for reserving resource (ls) not satisfied");
        } else if (reason == PEND_HOST_JOB_RUSAGE) {
            sprintf(
                msgline,
                "Job requirements for reserving resource (ls) not satisfied");
        }
        break;

    case IT:

        if (reason == PEND_HOST_LOAD) {
            sprintf(msgline, "The idle time (it) is not long enough");
        } else if (reason == PEND_HOST_QUE_RUSAGE) {
            sprintf(
                msgline,
                "Queue requirements for reserving resource (it) not satisfied");
        } else if (reason == PEND_HOST_JOB_RUSAGE) {
            sprintf(
                msgline,
                "Job requirements for reserving resource (it) not satisfied");
        }
        break;

    case TMP:

        if (reason == PEND_HOST_LOAD) {
            sprintf(msgline, "The available /tmp space (tmp) is low");
        } else if (reason == PEND_HOST_QUE_RUSAGE) {
            sprintf(msgline, "Queue requirements for reserving resource (tmp) "
                             "not satisfied");
        } else if (reason == PEND_HOST_JOB_RUSAGE) {
            sprintf(
                msgline,
                "Job requirements for reserving resource (tmp) not satisfied");
        }
        break;

    case SWP:

        if (reason == PEND_HOST_LOAD) {
            sprintf(msgline, "The available swap space (swp) is low");
        } else if (reason == PEND_HOST_QUE_RUSAGE) {
            sprintf(msgline, "Queue requirements for reserving resource (swp) "
                             "not satisfied");
        } else if (reason == PEND_HOST_JOB_RUSAGE) {
            sprintf(
                msgline,
                "Job requirements for reserving resource (swp) not satisfied");
        }
        break;

    case MEM:
        if (reason == PEND_HOST_LOAD) {
            sprintf(msgline, "The available memory (mem) is low");
        } else if (reason == PEND_HOST_QUE_RUSAGE) {
            sprintf(msgline, "Queue requirements for reserving resource (mem) "
                             "not satisfied");
        } else if (reason == PEND_HOST_JOB_RUSAGE) {
            sprintf(
                msgline,
                "Job requirements for reserving resource (mem) not satisfied");
        }
        break;

    default:
        userIndexReasons(msgline, resource, reason, ld);
        break;
    }

    *sp = msgline;
}
