/* $Id: lsb.err.c,v 1.10 2007/08/15 22:18:47 tmizan Exp $
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

__thread lsb_error_t lsberrno = 0;

const char *lsb_errmsg[LSBE_NUM_ERR + 1] = {
    [LSBE_NO_ERROR]            = "No error",
    [LSBE_NO_JOB]              = "No matching job found",
    [LSBE_NOT_STARTED]         = "Job has not started yet",
    [LSBE_JOB_STARTED]         = "Job has already started",
    [LSBE_JOB_FINISH]          = "Job has already finished",
    [LSBE_STOP_JOB]            = "Error 5",
    [LSBE_DEPEND_SYNTAX]       = "Dependency condition syntax error",
    [LSBE_EXCLUSIVE]           = "Queue does not accept EXCLUSIVE jobs",
    [LSBE_ROOT]                = "Root job submission is disabled",
    [LSBE_MIGRATION]           = "Job is already being migrated",
    [LSBE_J_UNCHKPNTABLE]      = "Job is not checkpointable",
    [LSBE_NO_OUTPUT]           = "No output so far",
    [LSBE_NO_JOBID]            = "No job Id can be used now",
    [LSBE_ONLY_INTERACTIVE]    = "Queue only accepts interactive jobs",
    [LSBE_NO_INTERACTIVE]      = "Queue does not accept interactive jobs",

    [LSBE_NO_USER]             = "No user is defined in the lsb.users file",
    [LSBE_BAD_USER]            = "Unknown user",
    [LSBE_PERMISSION]          = "User permission denied",
    [LSBE_BAD_QUEUE]           = "No such queue",
    [LSBE_QUEUE_NAME]          = "Queue name must be specified",
    [LSBE_QUEUE_CLOSED]        = "Queue has been closed",
    [LSBE_QUEUE_WINDOW]        = "Not activated because queue windows are closed",
    [LSBE_QUEUE_USE]           = "User cannot use the queue",
    [LSBE_BAD_HOST]            = "Bad host name, host group name or cluster name",
    [LSBE_PROC_NUM]            = "Too many processors requested",
    [LSBE_RESERVE1]            = "Reserved for future use",
    [LSBE_RESERVE2]            = "Reserved for future use",
    [LSBE_NO_GROUP]            = "No user/host group defined in the system",
    [LSBE_BAD_GROUP]           = "No such user/host group",
    [LSBE_QUEUE_HOST]          = "Host or host group is not used by the queue",
    [LSBE_UJOB_LIMIT]          = "Queue does not have enough per-user job slots",
    [LSBE_NO_HOST]             = "Current host is more suitable at this time",

    [LSBE_BAD_CHKLOG]          = "Checkpoint log is not found or is corrupted",
    [LSBE_PJOB_LIMIT]          = "Queue does not have enough per-processor job slots",
    [LSBE_NOLSF_HOST]          = "Request from non-LSF host rejected",

    [LSBE_BAD_ARG]             = "Bad argument",
    [LSBE_BAD_TIME]            = "Bad time specification",
    [LSBE_START_TIME]          = "Start time is later than termination time",
    [LSBE_BAD_LIMIT]           = "Bad CPU limit specification",
    [LSBE_OVER_LIMIT]          = "Cannot exceed queue's hard limit(s)",
    [LSBE_BAD_CMD]             = "Empty job",
    [LSBE_BAD_SIGNAL]          = "Signal not supported",
    [LSBE_BAD_JOB]             = "Bad job name",
    [LSBE_QJOB_LIMIT]          = "The destination queue has reached its job limit",

    [LSBE_UNKNOWN_EVENT]       = "Unknown event",
    [LSBE_EVENT_FORMAT]        = "Bad event format",
    [LSBE_EOF]                 = "End of file",

    [LSBE_MBATCHD]             = "Master batch daemon internal error",
    [LSBE_SBATCHD]             = "Slave batch daemon internal error",
    [LSBE_LSBLIB]              = "Batch library internal error",
    [LSBE_LSLIB]               = "Failed in an LSF library call",
    [LSBE_SYS_CALL]            = "System call failed",
    [LSBE_NO_MEM]              = "Cannot allocate memory",
    [LSBE_SERVICE]             = "Batch service not registered",
    [LSBE_NO_ENV]              = "LSB_SHAREDIR not defined",
    [LSBE_CHKPNT_CALL]         = "Checkpoint system call failed",
    [LSBE_NO_FORK]             = "Batch daemon cannot fork",

    [LSBE_PROTOCOL]            = "Batch protocol error",
    [LSBE_XDR]                 = "XDR encode/decode error",
    [LSBE_PORT]                = "Fail to bind to an appropriate port number",
    [LSBE_TIME_OUT]            = "Contacting batch daemon: Communication timeout",
    [LSBE_CONN_TIMEOUT]        = "Timeout on connect call to server",
    [LSBE_CONN_REFUSED]        = "Connection refused by server",
    [LSBE_CONN_EXIST]          = "Server connection already exists",
    [LSBE_CONN_NONEXIST]       = "Server is not connected",
    [LSBE_SBD_UNREACH]         = "Unable to contact execution host",
    [LSBE_OP_RETRY]            = "Operation is in progress",
    [LSBE_USER_JLIMIT]  = "User or one of user's groups does not have enough job slots",

    [LSBE_JOB_MODIFY] = "Job parameters cannot be changed now; non-repetitive job is running",
    [LSBE_JOB_MODIFY_ONCE]     = "Modified parameters have not been used",

    [LSBE_J_UNREPETITIVE]      = "Job cannot be run more than once",
    [LSBE_BAD_CLUSTER]         = "Unknown cluster name or cluster master",

    [LSBE_JOB_MODIFY_USED]     = "Modified parameters are being used",

    [LSBE_HJOB_LIMIT]          = "Queue does not have enough per-host job slots",

    [LSBE_NO_JOBMSG]           = "Mbatchd could not find the message that SBD mentions about",

    [LSBE_BAD_RESREQ]          = "Bad resource requirement syntax",

    [LSBE_NO_ENOUGH_HOST]      = "Not enough host(s) currently eligible",

    [LSBE_CONF_FATAL]          = "Error 77",
    [LSBE_CONF_WARNING]        = "Error 78",

    [LSBE_NO_RESOURCE]         = "No resource defined",
    [LSBE_BAD_RESOURCE]        = "Bad resource name",
    [LSBE_INTERACTIVE_RERUN]   = "Interactive job cannot be rerunnable",
    [LSBE_PTY_INFILE]          = "Input file not allowed with pseudo-terminal",
    [LSBE_BAD_SUBMISSION_HOST] =
        "Cannot find restarted or newly submitted job's submission host and host type",
    [LSBE_LOCK_JOB]            = "Error 109",
    [LSBE_UGROUP_MEMBER]       = "User not in the specified user group",
    [LSBE_OVER_RUSAGE]         = "Cannot exceed queue's resource reservation",
    [LSBE_BAD_HOST_SPEC]       = "Bad host specification",
    [LSBE_BAD_UGROUP]          = "Bad user group name",
    [LSBE_ESUB_ABORT]          = "Request aborted by esub",
    [LSBE_EXCEPT_ACTION]       = "Bad or invalid action specification",
    [LSBE_JOB_DEP]             = "Has dependent jobs",
    [LSBE_JGRP_NULL]           = "Job group does not exist",
    [LSBE_JGRP_BAD]            = "Bad/empty job group name",
    [LSBE_JOB_ARRAY]           = "Cannot operate on job array",
    [LSBE_JOB_SUSP]            = "Operation not supported for a suspended job",
    [LSBE_JOB_FORW]            = "Operation not supported for a forwarded job",
    [LSBE_BAD_IDX]             = "Job array index error",
    [LSBE_BIG_IDX]             = "Job array index too large",
    [LSBE_ARRAY_NULL]          = "Job array does not exist",
    [LSBE_JOB_EXIST]           = "Job exists",
    [LSBE_JOB_ELEMENT]         = "Cannot operate on element job",
    [LSBE_BAD_JOBID]           = "Bad jobId",
    [LSBE_MOD_JOB_NAME]        = "Change job name is not allowed for job array",

    [LSBE_PREMATURE]           = "Child process died",

    [LSBE_BAD_PROJECT_GROUP]   = "Invoker is not in specified project group",

    [LSBE_NO_HOST_GROUP]       = "No host group defined in the system",
    [LSBE_NO_USER_GROUP]       = "No user group defined in the system",
    [LSBE_INDEX_FORMAT]        = "Unknown jobid index file format",

    [LSBE_BAD_USER_PRIORITY]   = "Bad user priority",
    [LSBE_NO_JOB_PRIORITY]     = "Job priority control undefined",
    [LSBE_JOB_REQUEUED]        = "Job has already been requeued",

    [LSBE_MULTI_FIRST_HOST]    = "Multiple first execution hosts specified",
    [LSBE_HG_FIRST_HOST]       = "Host group specified as first execution host",
    [LSBE_HP_FIRST_HOST]       = "Host partition specified as first execution host",
    [LSBE_OTHERS_FIRST_HOST]   = "\"Others\" specified as first execution host",

    [LSBE_PROC_LESS]           = "Too few processors requested",
    [LSBE_MOD_MIX_OPTS] =
        "Only the following parameters can be used to modify a running job: -c, -M, -W, -o, -e, -r",
    [LSBE_MOD_CPULIMIT]        =
        "You must set LSB_JOB_CPULIMIT in lsf.conf to modify the CPU limit of a running job",
    [LSBE_MOD_MEMLIMIT]        =
        "You must set LSB_JOB_MEMLIMIT in lsf.conf to modify the memory limit of a running job",
    [LSBE_MOD_ERRFILE]         =
        "No error file specified before job dispatch. Error file does not exist, so error file name cannot be changed",
    [LSBE_LOCKED_MASTER]       = "The host is locked by master LIM",
    [LSBE_DEP_ARRAY_SIZE]      = "Dependent arrays do not have the same size",
};


char *lsb_sysmsg(void)
{
    if (lsberrno < 0 || lsberrno > LSBE_NUM_ERR)
        return (char *)"Unknown batch system error";

    if (lsb_errmsg[lsberrno] == NULL)
        return (char *)"Unknown batch system error";

    return (char *)lsb_errmsg[lsberrno];
}

void lsb_perror(char *usrMsg)
{
    if (usrMsg) {
        fputs(usrMsg, stderr);
        fputs(": ", stderr);
    }
    fputs(lsb_sysmsg(), stderr);
    putc('\n', stderr);
}

void sub_perror(char *usrMsg)
{
    if (usrMsg) {
        fputs(usrMsg, stderr);
        fputs(": ", stderr);
    }
    fputs(lsb_sysmsg(), stderr);
}
