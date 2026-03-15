\

# NAME

**lsb.events** - Lava batch event files

# DESCRIPTION

The Lava batch event log is used to display Lava batch event history and
for mbatchd failure recovery.

Whenever a host, job, or queue changes status, a record is appended to
the event log file **lsb.events**. The file is located in
LSB_SHAREDIR/*\<clustername\>*/logdir*,* where **LSB_SHAREDIR** must be
defined in lsf.conf(5) and \<*clustername*\> is the name of the Lava
cluster, as returned by **lsid**(1). See mbatchd(8) for the description
of LSB_SHAREDIR.

The **lsb.events** file can be read with the lsb_geteventrec() LSBLIB
call. See lsb_geteventrec(3) on this call.

The event log file is an ASCII file with one record per line. For the
**lsb.events** file, the first line has the format \"# \<history seek
position\>\", which indicates the file position of the first history
event after log switch. For the lsb.events**.***\#*** file**, the first
line has the format \"# \<timestamp of most recent event\>\", which
gives the timestamp of the recent event in the file.

# Fields

The fields of a record are separated by blanks. The first string of an
event record indicates its type. The following 17 types of events are
recorded:

# JOB_NEW

A new job has been submitted. The fields in order of occurrence are:

Version number (%s)

:   

    The version number

```{=html}
<!-- -->
```

Event time (%d)

:   

    The time of the event

```{=html}
<!-- -->
```

jobId (%d)

:   

    Job ID

```{=html}
<!-- -->
```

userId (%d)

:   

    UNIX user ID of the submitter

```{=html}
<!-- -->
```

options (%d)

:   

    Bit flags for job processing

```{=html}
<!-- -->
```

numProcessors (%d)

:   

    Number of processors requested for execution

```{=html}
<!-- -->
```

submitTime (%d)

:   

    Job submission time

```{=html}
<!-- -->
```

beginTime (%d)

:   

    Start time - the job should be started on or after this time

```{=html}
<!-- -->
```

termTime (%d)

:   

    Termination deadline - the job should be terminated by this time
    (%d)

```{=html}
<!-- -->
```

sigValue (%d)

:   

    Signal value

```{=html}
<!-- -->
```

chkpntPeriod (%d)

:   

    Checkpointing period

```{=html}
<!-- -->
```

restartPid (%d)

:   

    Restart process ID

```{=html}
<!-- -->
```

userName (%s)

:   

    User name

```{=html}
<!-- -->
```

rLimits

:   

    Soft CPU time limit (%d), see getrlimit(2)

```{=html}
<!-- -->
```

rLimits

:   

    Soft file size limit (%d), see getrlimit(2)

```{=html}
<!-- -->
```

rLimits

:   

    Soft data segment size limit (%d), see getrlimit(2)

```{=html}
<!-- -->
```

rLimits

:   

    Soft stack segment size limit (%d), see getrlimit(2)

```{=html}
<!-- -->
```

rLimits

:   

    Soft core file size limit (%d), see getrlimit(2)

```{=html}
<!-- -->
```

rLimits

:   

    Soft memory size limit (%d), see getrlimit(2)

```{=html}
<!-- -->
```

rLimits

:   

    Reserved (%d)

```{=html}
<!-- -->
```

rLimits

:   

    Reserved (%d)

```{=html}
<!-- -->
```

rLimits

:   

    Reserved (%d)

```{=html}
<!-- -->
```

rLimits

:   

    Soft run time limit (%d), see getrlimit(2)

```{=html}
<!-- -->
```

rLimits

:   

    Reserved (%d)

```{=html}
<!-- -->
```

hostSpec (%s)

:   

    Model or host name for normalizing CPUTIME and RUNTIME

```{=html}
<!-- -->
```

hostFactor (%f)

:   

    CPU factor of the above host

```{=html}
<!-- -->
```

umask (%d)

:   

    File creation mask for this job

```{=html}
<!-- -->
```

queue (%s)

:   

    Name of job queue to which the job was submitted

```{=html}
<!-- -->
```

resReq (%s)

:   

    Resource requirements

```{=html}
<!-- -->
```

fromHost (%s)

:   

    Submission host name

```{=html}
<!-- -->
```

cwd (%s)

:   

    Current working directory

```{=html}
<!-- -->
```

chkpntDir (%s)

:   

    Checkpoint directory

```{=html}
<!-- -->
```

inFile (%s)

:   

    Input file name

```{=html}
<!-- -->
```

outFile (%s)

:   

    Output file name

```{=html}
<!-- -->
```

errFile (%s)

:   

    Error output file name

```{=html}
<!-- -->
```

subHomeDir (%s)

:   

    Submitter\'s home directory

```{=html}
<!-- -->
```

jobFile (%s)

:   

    Job file name

```{=html}
<!-- -->
```

numAskedHosts (%d)

:   

    Number of candidate host names

```{=html}
<!-- -->
```

askedHosts (%s)

:   

    List of names of candidate hosts for job dispatching

```{=html}
<!-- -->
```

dependCond (%s)

:   

    Job dependency condition

```{=html}
<!-- -->
```

preExecCmd (%s)

:   

    Job pre-execution command

```{=html}
<!-- -->
```

jobName (%s)

:   

    Job name

```{=html}
<!-- -->
```

command (%s)

:   

    Job command

```{=html}
<!-- -->
```

nxf (%d)

:   

    Number of files to transfer (%d)

```{=html}
<!-- -->
```

xf (%s)

:   

    List of file transfer specifications

```{=html}
<!-- -->
```

mailUser (%s)

:   

    Mail user name

```{=html}
<!-- -->
```

projectName (%s)

:   

    Project name

```{=html}
<!-- -->
```

niosPort (%d)

:   

    Callback port if batch interactive job

```{=html}
<!-- -->
```

maxNumProcessors (%d)

:   

    Maximum number of processors

```{=html}
<!-- -->
```

schedHostType (%s)

:   

    Execution host type

```{=html}
<!-- -->
```

loginShell (%s)

:   

    Login shell

```{=html}
<!-- -->
```

userGroup (%s)

:   

    User group

```{=html}
<!-- -->
```

options2 (%d)

:   

    Bit flags for job processing

```{=html}
<!-- -->
```

idx (%d)

:   

    Job array index

```{=html}
<!-- -->
```

inFileSpool (%s)

:   

    Spool input file

```{=html}
<!-- -->
```

commandSpool (%s)

:   

    Spool command file

```{=html}
<!-- -->
```

jobSpoolDir (%s)

:   

    Job spool directory

```{=html}
<!-- -->
```

userPriority (%d)

:   

    User priority

# JOB_START

> A job has been dispatched. The fields in order of occurrence are:

Version number (%s)

:   

    The version number

```{=html}
<!-- -->
```

Event time (%d)

:   

    The time of the event

```{=html}
<!-- -->
```

jobId (%d)

:   

    Job ID

```{=html}
<!-- -->
```

jStatus (%d)

:   

    Job status, (**4**, indicating the **RUN** status of the job)

```{=html}
<!-- -->
```

jobPid (%d)

:   

    Job process ID

```{=html}
<!-- -->
```

jobPGid (%d)

:   

    Job process group ID

```{=html}
<!-- -->
```

hostFactor (%f)

:   

    CPU factor of the first execution host

```{=html}
<!-- -->
```

numExHosts (%d)

:   

    Number of processors used for execution

```{=html}
<!-- -->
```

execHosts (%s)

:   

    List of execution host names

```{=html}
<!-- -->
```

queuePreCmd (%s)

:   

    Pre-execution command

```{=html}
<!-- -->
```

queuePostCmd (%s)

:   

    Post-execution command

```{=html}
<!-- -->
```

jFlags (%d)

:   

    Job processing flags

```{=html}
<!-- -->
```

userGroup (%s)

:   

    User group name

```{=html}
<!-- -->
```

idx (%d)

:   

    Job array index

# JOB_START_ACCEPT

A job has started on the execution host(s). The fields in order of
occurrence are:

Version number (%s)

:   

    The version number

```{=html}
<!-- -->
```

Event time (%d)

:   

    The time of the event

```{=html}
<!-- -->
```

jobId (%d)

:   

    Job ID

```{=html}
<!-- -->
```

jobPid (%d)

:   

    Job process ID

```{=html}
<!-- -->
```

jobPGid (%d)

:   

    Job process group ID

```{=html}
<!-- -->
```

idx (%d)

:   

    Job array index

# JOB_STATUS

The status of a job changed after dispatch. The fields in order of
occurrence are:

Version number (%s)

:   

    The version number

```{=html}
<!-- -->
```

Event time (%d)

:   

    The time of the event

```{=html}
<!-- -->
```

jobId (%d)

:   

    Job ID

```{=html}
<!-- -->
```

jStatus (%d)

:   

    New status, see **\<**lsbatch/lsbatch.h\>

```{=html}
<!-- -->
```

reason (%d)

:   

    Pending or suspended reason code, see \<lsbatch/lsbatch.h\>

```{=html}
<!-- -->
```

subreasons (%d)

:   

    Pending or suspended subreason code, see \<lsbatch/lsbatch.h\>

```{=html}
<!-- -->
```

cpuTime (%f)

:   

    CPU time consumed so far

```{=html}
<!-- -->
```

endTime (%d)

:   

    Job completion time

```{=html}
<!-- -->
```

ru (%d)

:   

    Resource usage flag

```{=html}
<!-- -->
```

lsfRusage (%s)

:   

    Resource usage statistics, see **\<**lsf/lsf.h**\>**

```{=html}
<!-- -->
```

exitStatus (%d)

:   

    Exit status of the job, see \<lsbatch/lsbatch.h\>

```{=html}
<!-- -->
```

idx (%d)

:   

    Job array index (%d)

# JOB_SWITCH

A job switched from one queue to another. The fields in order of
occurrence are:

Version number (%s)

:   

    The version number

```{=html}
<!-- -->
```

Event time (%d)

:   

    The time of the event

```{=html}
<!-- -->
```

userId (%d)

:   

    UNIX user ID of the user invoking the command

```{=html}
<!-- -->
```

jobId (%d)

:   

    Job ID

```{=html}
<!-- -->
```

queue (%s)

:   

    Target queue name

```{=html}
<!-- -->
```

idx (%d)

:   

    Job array index

```{=html}
<!-- -->
```

userName (%s)

:   

    Name of the job submitter

# JOB_MOVE

A job moved toward the top or bottom of its queue. The fields in order
of occurrence are:

Version number (%s)

:   

    The version number

```{=html}
<!-- -->
```

Event time (%d)

:   

    The time of the event

```{=html}
<!-- -->
```

userId (%d)

:   

    UNIX user ID of the user invoking the command

```{=html}
<!-- -->
```

jobId (%d)

:   

    Job ID

```{=html}
<!-- -->
```

position (%d)

:   

    Position number

```{=html}
<!-- -->
```

base (%d)

:   

    Operation code, (TO_TOP or TO_BOTTOM), see \<lsbatch/lsbatch.h\>

```{=html}
<!-- -->
```

idx (%d)

:   

    Job array index

```{=html}
<!-- -->
```

userName (%s)

:   

    Name of the job submitter

# QUEUE_CTRL

A job queue has been altered. The fields in order of occurrence are:

Version number (%s)

:   

    The version number

```{=html}
<!-- -->
```

Event time (%d)

:   

    The time of the event

```{=html}
<!-- -->
```

opCode (%d)

:   

    Operation code), see \<lsbatch/lsbatch.h\>

```{=html}
<!-- -->
```

queue (%s)

:   

    Queue name

```{=html}
<!-- -->
```

userId (%d)

:   

    UNIX user ID of the user invoking the command

> userName (%s)

> Name of the user

# HOST_CTRL

A batch server host changed status. The fields in order of occurrence
are:

Version number (%s)

:   

    The version number

```{=html}
<!-- -->
```

Event time (%d)

:   

    The time of the event

```{=html}
<!-- -->
```

opCode (%d)

:   

    Operation code, see \<lsbatch/lsbatch.h\>

```{=html}
<!-- -->
```

host (%s)

:   

    Host name

```{=html}
<!-- -->
```

userId (%d)

:   

    UNIX user ID of the user invoking the command

```{=html}
<!-- -->
```

userName (%s)

:   

    Name of the user

# MBD_START

The mbatchd has started. The fields in order of occurrence are:

Version number (%s)

:   

    The version number

```{=html}
<!-- -->
```

Event time (%d)

:   

    The time of the event

```{=html}
<!-- -->
```

master (%s)

:   

    Master host name

```{=html}
<!-- -->
```

cluster (%s)

:   

    cluster name

```{=html}
<!-- -->
```

numHosts (%d)

:   

    Number of hosts in the cluster

```{=html}
<!-- -->
```

numQueues (%d)

:   

    Number of queues in the cluster

# MBD_DIE

The mbatchd died. The fields in order of occurrence are:

Version number (%s)

:   

    The version number

```{=html}
<!-- -->
```

Event time (%d)

:   

    The time of the event

```{=html}
<!-- -->
```

master (%s)

:   

    Master host name

```{=html}
<!-- -->
```

numRemoveJobs (%d)

:   

    Number of finished jobs that have been removed from the system and\
    logged in the current event file

```{=html}
<!-- -->
```

exitCode (%d)

:   

    Exit code from mbatchd

# UNFULFILL

Actions that were not taken because the mbatchd was unable to contact
the sbatchd on the job\'s execution host. The fields in order of
occurrence are:

Version number (%s)

:   

    The version number

```{=html}
<!-- -->
```

Event time (%d)

:   

    The time of the event

```{=html}
<!-- -->
```

jobId (%d)

:   

    Job ID

```{=html}
<!-- -->
```

notSwitched (%d)

:   

    Not switched: the mbatchd has switched the job to a new queue, but
    the sbatchd has not been informed of the switch

```{=html}
<!-- -->
```

sig (%d)

:   

    Signal: this signal has not been sent to the job

```{=html}
<!-- -->
```

sig1 (%d)

:   

    Checkpoint signal: the job has not been sent this signal to
    checkpoint itself

```{=html}
<!-- -->
```

sig1Flags (%d)

:   

    Checkpoint flags, see \<lsbatch/lsbatch.h\>

```{=html}
<!-- -->
```

chkPeriod (%d) 

:   

    Job\'s new checkpoint period

```{=html}
<!-- -->
```

notModified (%s)

:   

    If set to true, then parameters for the job cannot be modified.

```{=html}
<!-- -->
```

idx (%d)

:   

    Job array index

# LOAD_INDEX

mbatchd restarted with these load index names (see **lsf.cluster**(5)).
The fields in order of occurrence are:

Version number (%s)

:   

    The version number

```{=html}
<!-- -->
```

Event time (%d)

:   

    The time of the event

```{=html}
<!-- -->
```

nIdx (%d)

:   

    Number of index names

```{=html}
<!-- -->
```

name (%s)

:   

    List of index names

# JOB_SIGACT

An action on a job has been taken. The fields in order of occurrence
are:

Version number (%s)

:   

    The version number

```{=html}
<!-- -->
```

Event time (%d)

:   

    The time of the event

```{=html}
<!-- -->
```

jobId (%d)

:   

    Job ID

```{=html}
<!-- -->
```

period (%d)

:   

    Action period

```{=html}
<!-- -->
```

pid (%d)

:   

    Process ID of the child sbatchd that initiated the action

```{=html}
<!-- -->
```

jstatus (%d)

:   

    Job status

```{=html}
<!-- -->
```

reasons (%d)

:   

    Job pending reasons

```{=html}
<!-- -->
```

flags (%d)

:   

    Action flags, see \<lsbatch/lsbatch.h\>

```{=html}
<!-- -->
```

actStatus (%d)

:   

    Action status:

> **1**: Action started

> **2**: One action preempted other actions

> **3**: Action succeeded

> **4**: Action Failed

signalSymbol (%s)

:   

    Action name, accompanied by actFlags

```{=html}
<!-- -->
```

idx (%d)

:   

    Job array index

# MIG

A job has been migrated. The fields in order of occurrence are:

Version number (%s)

:   

    The version number

```{=html}
<!-- -->
```

Event time (%d)

:   

    The time of the event

```{=html}
<!-- -->
```

jobId (%d)

:   

    Job ID

```{=html}
<!-- -->
```

numAskedHosts (%d)

:   

    Number of candidate hosts for migration

```{=html}
<!-- -->
```

askedHosts (%s)

:   

    List of names of candidate hosts

```{=html}
<!-- -->
```

userId (%d)

:   

    UNIX user ID of the user invoking the command

```{=html}
<!-- -->
```

idx (%d)

:   

    Job array index

```{=html}
<!-- -->
```

userName (%s)

:   

    Name of the job submitter

# JOB_MODIFY

This is created when the mbatchd modifies a previously submitted job via
bmod(1). The fields logged are the same as those for **JOB_NEW.**

# JOB_SIGNAL

This is created when a job is signaled via bkill(1) or deleted via
bdel(1). The fields are in the order they appended :

Version number (%s)

:   

    The version number

```{=html}
<!-- -->
```

Event time (%d)

:   

    The time of the event

```{=html}
<!-- -->
```

jobId (%d)

:   

    Job ID

```{=html}
<!-- -->
```

userId (%d)

:   

    UNIX user ID of the user invoking the command

```{=html}
<!-- -->
```

runCount (%d)

:   

    Number of runs

```{=html}
<!-- -->
```

signalSymbol (%s)

:   

    Signal name

```{=html}
<!-- -->
```

idx (%d)

:   

    Job array index

```{=html}
<!-- -->
```

userName (%s)

:   

    Name of the job submitter

# JOB_EXECUTE

This is created when a job is actually running on an execution host. The
fields in order of occurrence are:

Version number (%s)

:   

    The version number

```{=html}
<!-- -->
```

Event time (%d)

:   

    The time of the event

```{=html}
<!-- -->
```

jobId (%d)

:   

    Job ID

```{=html}
<!-- -->
```

execUid (%d)

:   

    Mapped UNIX user ID on execution host

```{=html}
<!-- -->
```

jobPGid (%d)

:   

    Job process group ID

```{=html}
<!-- -->
```

execCwd (%s)

:   

    Current working directory job used on execution host

```{=html}
<!-- -->
```

execHome (%s)

:   

    Home directory job used on execution host

```{=html}
<!-- -->
```

execUsername (%s)

:   

    Mapped user name on execution host

```{=html}
<!-- -->
```

jobPid (%d)

:   

    Job\'s process ID

```{=html}
<!-- -->
```

idx (%d)

:   

    Job array index

# JOB_REQUEUE

This is created when a job ended and requeued by mbatchd. The fields in
order of occurrence are:

Version number (%s)

:   

    The version number

```{=html}
<!-- -->
```

Event time (%d)

:   

    The time of the event

```{=html}
<!-- -->
```

jobId (%d)

:   

    Job ID

```{=html}
<!-- -->
```

idx (%d)

:   

    Job array index

# JOB_CLEAN

This is created when a job is removed from the mbatchd memory. The
fields in order of occurrence are:

Version number (%s)

:   

    The version number

```{=html}
<!-- -->
```

Event time (%d)

:   

    The time of the event

```{=html}
<!-- -->
```

jobId (%d)

:   

    Job ID

```{=html}
<!-- -->
```

idx (%d)

:   

    Job array index

# SEE ALSO

## Related Topics:

lsid(1), getrlimit(2), lsb_geteventrec(3), lsb.acct(5), lsb.queues(5),
lsb.hosts(5), lsb.users(5), lsb.params(5), lsf.conf(5), lsf.cluster(5),
badmin(8) and mbatchd(8)

## Files:

LSB_SHAREDIR/\<*clustername*\>/logdir/lsb.events\[.?\]
