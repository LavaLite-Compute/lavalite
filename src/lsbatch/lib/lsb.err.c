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

int lsberrno = 0;
char *lsb_errmsg[] = {
    /*00*/ "No error",
    /*01*/ "No matching job found",
    /*02*/ "Job has not started yet",
    /*03*/ "Job has already started",
    /*04*/ "Job has already finished",
    /*05*/ "Error 5", /* no message needed */
    /*06*/ "Dependency condition syntax error",
    /*07*/ "Queue does not accept EXCLUSIVE jobs",
    /*08*/ "Root job submission is disabled",
    /*09*/ "Job is already being migrated",
    /*10*/ "Job is not checkpointable",
    /*11*/ "No output so far",
    /*12*/ "No job Id can be used now",
    /*13*/ "Queue only accepts interactive jobs",
    /*14*/ "Queue does not accept interactive jobs",

    /*15*/ "No user is defined in the lsb.users file",
    /*16*/ "Unknown user",
    /*17*/ "User permission denied",
    /*18*/ "No such queue",
    /*19*/ "Queue name must be specified",
    /*20*/ "Queue has been closed",
    /*21*/ "Not activated because queue windows are closed",
    /*22*/ "User cannot use the queue",
    /*23*/ "Bad host name, host group name or cluster name",
    /*24*/ "Too many processors requested",
    /*25*/ "Reserved for future use",
    /*26*/ "Reserved for future use",
    /*27*/ "No user/host group defined in the system",
    /*28*/ "No such user/host group",
    /*29*/ "Host or host group is not used by the queue",
    /*30*/ "Queue does not have enough per-user job slots",
    /*31*/ "Current host is more suitable at this time",
    /*32*/ "Checkpoint log is not found or is corrupted",
    /*33*/ "Queue does not have enough per-processor job slots",
    /*34*/ "Request from non-LSF host rejected",

    /*35*/ "Bad argument",
    /*36*/ "Bad time specification",
    /*37*/ "Start time is later than termination time",
    /*38*/ "Bad CPU limit specification",
    /*39*/ "Cannot exceed queue's hard limit(s)",
    /*40*/ "Empty job",
    /*41*/ "Signal not supported",
    /*42*/ "Bad job name",
    /*43*/ "The destination queue has reached its job limit",

    /*44*/ "Unknown event",
    /*45*/ "Bad event format",
    /*46*/ "End of file",

    /*47*/ "Master batch daemon internal error",
    /*48*/ "Slave batch daemon internal error",
    /*49*/ "Batch library internal error",
    /*50*/ "Failed in an LSF library call",
    /*51*/ "System call failed",
    /*52*/ "Cannot allocate memory",
    /*53*/ "Batch service not registered",
    /*54*/ "LSB_SHAREDIR not defined",
    /*55*/ "Checkpoint system call failed",
    /*56*/ "Batch daemon cannot fork",

    /*57*/ "Batch protocol error",
    /*58*/ "XDR encode/decode error",
    /*59*/ "Fail to bind to an appropriate port number",
    /*60*/ "Contacting batch daemon: Communication timeout",
    /*61*/ "Timeout on connect call to server",
    /*62*/ "Connection refused by server",
    /*63*/ "Server connection already exists",
    /*64*/ "Server is not connected",
    /*65*/ "Unable to contact execution host",
    /*66*/ "Operation is in progress",
    /*67*/ "User or one of user's groups does not have enough job slots",

    /*68*/
    "Job parameters cannot be changed now; non-repetitive job is running",
    /*69*/ "Modified parameters have not been used",
    /*70*/ "Job cannot be run more than once",
    /*71*/ "Unknown cluster name or cluster master",
    /*72*/ "Modified parameters are being used",

    /*73*/ "Queue does not have enough per-host job slots",
    /*74*/ "Mbatchd could not find the message that SBD mentions about",
    /*75*/ "Bad resource requirement syntax",
    /*76*/ "Not enough host(s) currently eligible",
    /*77*/ "Error 77", /* internal only; no need to print message */
    /*78*/ "Error 78", /* internal only; no need to print message */
    /*79*/ "No resource defined",
    /*80*/ "Bad resource name",
    /*81*/ "Interactive job cannot be rerunnable",
    /*82 */ "Input file not allowed with pseudo-terminal",
    /*83*/
    "Cannot find restarted or newly submitted job's submission host and host "
    "type",
    /*84*/ "Error 109", /* internal only; no need to print message */
    /*85*/ "User not in the specified user group",
    /*86*/ "Cannot exceed queue's resource reservation",
    /*87*/ "Bad host specification",
    /*88*/ "Bad user group name",
    /*89*/ "Request aborted by esub",
    /*90*/ "Bad or invalid action specification",
    /*91*/ "Has dependent jobs",
    /*92*/ "Job group does not exist",
    /*93*/ "Bad/empty job group name",
    /*94*/ "Cannot operate on job array",
    /*95*/ "Operation not supported for a suspended job",
    /*96*/ "Operation not supported for a forwarded job",
    /*97*/ "Job array index error",
    /*98*/ "Job array index too large",
    /*99*/ "Job array does not exist",
    /*100*/ "Job exists",
    /*101*/ "Cannot operate on element job",
    /*102*/ "Bad jobId",
    /*103*/ "Change job name is not allowed for job array",
    /*104*/ "Child process died",
    /*105*/ "Invoker is not in specified project group",
    /*106*/ "No host group defined in the system",
    /*107*/ "No user group defined in the system",
    /*108*/ "Unknown jobid index file format",

    /*109*/ "Source file for spooling does not exist",
    /*110*/ "Number of failed spool hosts reached max",
    /*111*/ "Spool copy failed for this host",
    /*112*/ "Fork for spooling failed",
    /*113*/ "Status of spool child is not available",
    /*114*/ "Spool child terminated with failure",
    /*115*/ "Unable to find a host for spooling",
    /*116*/ "Cannot get $JOB_SPOOL_DIR for this host",
    /*117*/ "Cannot delete spool file for this host",

    /*118*/ "Bad user priority",
    /*119*/ "Job priority control undefined",
    /*120*/ "Job has already been requeued",

    /*121*/ "Multiple first execution hosts specified",
    /*122*/ "Host group specified as first execution host",
    /*123*/ "Host partition specified as first execution host",
    /*124*/ "\"Others\" specified as first execution host",

    /*125*/ "Too few processors requested",
    /*126*/
    "Only the following parameters can be used to modify a running job: -c, "
    "-M, -W, -o, -e, -r",
    /*127*/
    "You must set LSB_JOB_CPULIMIT in lsf.conf to modify the CPU limit of a "
    "running job",
    /*128*/
    "You must set LSB_JOB_MEMLIMIT in lsf.conf to modify the memory limit of a "
    "running job",
    /*129*/
    "No error file specified before job dispatch. Error file does not exist, "
    "so error file name cannot be changed",
    /*130*/ "The host is locked by master LIM",
    /*131*/ "Dependent arrays do not have the same size",

    /* when you add a new message here, remember two things: first do not
     * forget to add "," after the error message; second, add its catgets
     * id in the above array lsb_errmsg_ID[].
     */
    NULL};

char *lsb_sysmsg(void)
{
    static char buf[512];

    if (lsberrno >= LSBE_NUM_ERR) {
        sprintf(buf, "Unknown batch system error number %d", lsberrno);
        return buf;
    }

    if (lsberrno == LSBE_SYS_CALL) {
        if (strerror(errno) != NULL && errno > 0)
            sprintf(buf, "%s: %s", lsb_errmsg[lsberrno], strerror(errno));
        else {
            char *temp;
            temp = putstr_(lsb_errmsg[lsberrno]);
            sprintf(buf, "%s:%s %d", temp, "unknown system error", errno);
            free(temp);
        }
    } else if (lsberrno == LSBE_LSLIB) {
        sprintf(buf, "%s: %s", lsb_errmsg[lsberrno], ls_sysmsg());
    } else {
        return (lsb_errmsg[lsberrno]);
    }
    return buf;
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

char *lsb_sperror(char *usrMsg)
{
    char errmsg[256];
    char *rtstr;

    errmsg[0] = '\0';

    if (usrMsg)
        sprintf(errmsg, "%s: ", usrMsg);

    strcat(errmsg, lsb_sysmsg());

    if ((rtstr = (char *) malloc(sizeof(char) * (strlen(errmsg) + 1))) ==
        NULL) {
        lserrno = LSE_MALLOC;
        return NULL;
    }

    strcpy(rtstr, errmsg);
    return rtstr;
}

void sub_perror(char *usrMsg)
{
    if (usrMsg) {
        fputs(usrMsg, stderr);
        fputs(": ", stderr);
    }
    fputs(lsb_sysmsg(), stderr);
}
