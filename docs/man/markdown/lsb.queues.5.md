\

# NAME

**lsb.queues**

## Overview

The lsb.queues file defines the batch queues in an Lava cluster.

This file is optional; if no queues are configured, Lava creates a queue
named default, with all parameters set to default values.

## Contents

> · lsb.queues Structure

# lsb.queues Structure

Each queue definition begins with the line Begin Queue and ends with the
line End Queue. The queue name must be specified; all other parameters
are optional.

# ADMINISTRATORS

## Syntax

**ADMINISTRATORS** **=** *user_name* \| *user_group* \...

## Description

List of queue administrators.

Queue administrators can perform operations on any user\'s job in the
queue, as well as on the queue itself.

## Default

Undefined (you must be a cluster administrator to operate on this
queue).

# CHKPNT

## Syntax

**CHKPNT = ***chkpnt_dir *\[*chkpnt_period*\]

## Description

Enables automatic checkpointing.

The checkpoint directory is the directory where the checkpoint files are
created. Specify an absolute path or a path relative to CWD, do not use
environment variables.

Specify the checkpoint period in minutes.

Job-level checkpoint parameters override queue-level checkpoint
parameters.

## Default

Undefined.

# CORELIMIT

## Syntax

**CORELIMIT = ***integer* .SS Description

The per-process (hard) core file size limit (in KB) for all of the
processes belonging to a job from this queue (see **getrlimit**(**2**)).

## Default

Unlimited

# CPULIMIT

## Syntax

**CPULIMIT = **\[*default_limit*\] *maximum_limit* .PP where
*default_limit* and *maximum_limit* are:

\[*hours*:\]*minutes*\[/*host_name* \| /*host_model*\]

## Description

Maximum normalized CPU time and optionally, the default normalized CPU
time allowed for all processes of a job running in this queue. The name
of a host or host model specifies the CPU time normalization host to
use.

If a job dynamically spawns processes, the CPU time used by these
processes is accumulated over the life of the job.

Processes that exist for fewer than 30 seconds may be ignored.

By default, if a default CPU limit is specified, jobs submitted to the
queue without a job-level CPU limit are killed when the default CPU
limit is reached.

If you specify only one limit, it is the maximum, or hard, CPU limit. If
you specify two limits, the first one is the default, or soft, CPU
limit, and the second one is the maximum CPU limit. The number of
minutes may be greater than 59. Therefore, three and a half hours can be
specified either as 3:30 or 210.

You can define whether the CPU limit is a per-process limit enforced by
the OS or a per-job limit enforced by Lava with LSB_JOB_CPULIMIT in
lsf.conf.

## Default

Unlimited

# DATALIMIT

## Syntax

**DATALIMIT =** \[*default_limit*\] *maximum_limit* .SS Description

The per-process data segment size limit (in KB) for all of the processes
belonging to a job from this queue (see **getrlimit**(**2**)).

By default, if a default data limit is specified, jobs submitted to the
queue without a job-level data limit are killed when the default data
limit is reached.

If you specify only one limit, it is the maximum, or hard, data limit.
If you specify two limits, the first one is the default, or soft, data
limit, and the second one is the maximum data limit

## Default

Unlimited

# DEFAULT_HOST_SPEC

## Syntax

**DEFAULT_HOST_SPEC =** *host_name \| host_model* .SS Description

The default CPU time normalization host for the queue.

The CPU factor of the specified host or host model will be used to
normalize the CPU time limit of all jobs in the queue, unless the CPU
time normalization host is specified at the job level.

## Default

Undefined.

# DESCRIPTION

## Syntax

**DESCRIPTION =** *text* .SS Description

Description of the job queue that will be displayed by **bqueues -l.**
.PP This description should clearly describe the service features of
this queue, to help users select the proper queue for each job.

The text can include any characters, including white space. The text can
be extended to multiple lines by ending the preceding line with a
backslash (. The maximum length for the text is 512 characters.

# DISPATCH_WINDOW

## Syntax

**DISPATCH_WINDOW =** *time_window *\...

## Description

The time windows in which jobs from this queue are dispatched. Once
dispatched, jobs are no longer affected by the dispatch window.

## Default

Undefined (always open).

# EXCLUSIVE

## Syntax

**EXCLUSIVE = Y** \| **N** .SS Description

If Y, specifies an exclusive queue.

Jobs submitted to an exclusive queue with **bsub -x** will only be
dispatched to a host that has no other Lava jobs running.

# FILELIMIT

## Syntax

**FILELIMIT =** *integer* .SS Description

The per-process (hard) file size limit (in KB) for all of the processes
belonging to a job from this queue (see **getrlimit**(**2**)).

## Default

Unlimited

# HJOB_LIMIT

## Syntax

**HJOB_LIMIT** **=** *integer* .SS Description

Per-host job slot limit.

Maximum number of job slots that this queue can use on any host. This
limit is configured per host, regardless of the number of processors it
may have.

This may be useful if the queue dispatches jobs that require a node-
locked license. If there is only one node-locked license per host then
the system should not dispatch more than one job to the host even if it
is a multiprocessor host.

## Example

The following will run a maximum of one job on each of hostA, hostB, and
hostC:

Begin Queue\
\
HJOB_LIMIT = 1\
HOSTS=hostA hostB hostC\
\
End Queue

## Default

Unlimited

# HOSTS

## Syntax

**HOSTS =** \[**\~**\]*host_name*\[**+***pref_level*\] \|
\[**\~**\]*host_group*\[**+***pref_level*\] \|
**others**\[**+***pref_level*\] \| **all** \| **none** \...

## Description

A space-separated list of hosts, host groups, and host partitions on
which jobs from this queue can be run. All the members of the host list
should either belong to a single host partition or not belong to any
host partition. Otherwise, job scheduling may be affected.

Any item can be followed by a plus sign (+) and a positive number to
indicate the preference for dispatching a job to that host, host group,
or host partition. A higher number indicates a higher preference. If a
host preference is not given, it is assumed to be 0. Hosts at the same
level of preference are ordered by load.

Use the keyword others to indicate all hosts not explicitly listed.

Use the not operator (\~) to exclude hosts or host groups from the
queue. This is useful if you have a large cluster but only want to
exclude a few hosts from the queue definition.

Use the keyword all to indicate all hosts not explicitly excluded.

## Compatibility

Host preferences specified by **bsub -m** override the queue
specification.

## Example 1

HOSTS = hostA+1 hostB hostC+1 GroupX+3

This example defines three levels of preferences: run jobs on hosts in
GroupX as much as possible, otherwise run on either hostA or hostC if
possible, otherwise run on hostB. Jobs should not run on hostB unless
all other hosts are too busy to accept more jobs.

## Example 2

HOSTS = hostD+1 others

Run jobs on hostD as much as possible, otherwise run jobs on the
least-loaded host available.

## Example 3

HOSTS = Group1 \~hostA hostB hostC

Run jobs on hostB, hostC, and all hosts in Group1 except for hostA.

## Example 4

HOSTS = all \~group2 \~hostA

Run jobs on all hosts in the cluster, except for hostA and the hosts in
group2.

## Default

all (the queue can use all hosts in the cluster, and every host has
equal preference).

# IGNORE_DEADLINE

## Syntax

**IGNORE_DEADLINE = Y** .SS Description

If Y, disables deadline constraint scheduling (starts all jobs
regardless of deadline contraints).

# INTERACTIVE

## Syntax

**INTERACTIVE = NO** \| **ONLY** .SS Description

Causes the queue to reject interactive batch jobs (NO) or accept nothing
but interactive batch jobs (ONLY).

Interactive batch jobs are submitted via **bsub -I**.

## Default

Undefined (the queue accepts both interactive and non-interactive jobs).

# JOB_ACCEPT_INTERVAL

## Syntax

**JOB_ACCEPT_INTERVAL =** *integer* .SS Description

The number of dispatch turns to wait after dispatching a job to a host,
before dispatching a second job to the same host. By default, a dispatch
turn lasts 60 seconds (MBD_SLEEP_TIME in lsb.params).

If 0 (zero), a host may accept more than one job in each dispatch turn.
By default, there is no limit to the total number of jobs that can run
on a host, so if this parameter is set to 0, a very large number of jobs
might be dispatched to a host all at once. You may notice performance
problems if this occurs.

JOB_ACCEPT_INTERVAL set at the queue level (lsb.queues) overrides
JOB_ACCEPT_INTERVAL set at the cluster level (lsb.params).

## Default

Undefined (the queue uses JOB_ACCEPT_INTERVAL defined in lsb.params,
which has a default value of 1).

# JOB_CONTROLS

## Syntax

**JOB_CONTROLS = SUSPEND**\[*signal* \| *command* \| **CHKPNT**\]
**RESUME**\[*signal* \| *command*\] **TERMINATE**\[*signal* \| *command*
\| **CHKPNT**\]

> · CHKPNT is a special action, which causes the system to checkpoint
> the job. If the SUSPEND action is CHKPNT, the job is checkpointed and
> then stopped by sending the SIGSTOP signal to the job automatically.
>
> · *signal* is a UNIX signal name (such as SIGSTOP or SIGTSTP).
>
> · *command* specifies a /bin/sh command line to be invoked. Do not
> specify a signal followed by an action that triggers the same signal
> (for example, do not specify JOB_CONTROLS=TERMINATE\[bkill\] or
> JOB_CONTROLS=TERMINATE\[brequeue\]). This will cause a deadlock
> between the signal and the action.

## Description

Changes the behaviour of the SUSPEND, RESUME, and TERMINATE actions in
Lava.

For SUSPEND and RESUME, if the action is a command, the following points
should be considered:

> · The contents of the configuration line for the action are run with
> /bin/sh -c so you can use shell features in the command.
>
> · The standard input, output, and error of the command are redirected
> to the NULL device.
>
> · The command is run as the user of the job.
>
> · All environment variables set for the job are also set for the
> command action. The following additional environment variables are
> set:
>
> > · LSB_JOBPGIDS \-- a list of current process group IDs of the job
> >
> > · LSB_JOBPIDS \--a list of current process IDs of the job

> For the SUSPEND action command, the following environment variable is
> also set:

> · LSB_SUSP_REASONS \-- an integer representing a bitmap of suspending
> reasons as defined in lsbatch.h
>
> > The suspending reason can allow the command to take different
> > actions based on the reason for suspending the job.

## Default

On LINUX, by default, SUSPEND sends SIGTSTP for parallel or interactive
jobs and SIGSTOP for other jobs. RESUME sends SIGCONT. TERMINATE sends
SIGINT, SIGTERM and SIGKILL in that order.

# JOB_STARTER

## Syntax

**JOB_STARTER =** *starter* \[*starter*\] \[**\"%USRCMD\"**\]
\[*starter*\]

## Description

Creates a specific environment for submitted jobs prior to execution.

*starter* is any executable that can be used to start the job (i.e., can
accept the job as an input argument). Optionally, additional strings can
be specified.

By default, the user commands run after the job starter. A special
string, %USRCMD, can be used to represent the position of the user\'s
job in the job starter command line. The %USRCMD string may be enclosed
with quotes or followed by additional commands.

## Example

JOB_STARTER = csh -c \"%USRCMD;sleep 10\"

In this case, if a user submits a job

\% bsub myjob arguments

the command that actually runs is:

\% csh -c \"myjob arguments;sleep 10\"

## Default

Undefined (no job starter).

# load_index

## Syntax

*load_index* **=** *loadSched*\[**/***loadStop*\]

Specify io, it, ls, mem, pg, r15s, r1m, r15m, swp, tmp, ut, or a non-
shared custom external load index. Specify multiple lines to configure
thresholds for multiple load indices.

Specify io, it, ls, mem, pg, r15s, r1m, r15m, swp, tmp, ut, or a non-
shared custom external load index as a column. Specify multiple columns
to configure thresholds for multiple load indices.

## Description

Scheduling and suspending thresholds for the specified dynamic load
index.

The loadSched condition must be satisfied before a job is dispatched to
the host. If a RESUME_COND is not specified, the loadSched condition
must also be satisfied before a suspended job can be resumed.

If the loadStop condition is satisfied, a job on the host will be
suspended.

The loadSched and loadStop thresholds permit the specification of
conditions using simple AND/OR logic. Any load index that does not have
a configured threshold has no effect on job scheduling.

Lava will not suspend a job if the job is the only batch job running on
the host and the machine is interactively idle (it\>0).

The r15s, r1m, and r15m CPU run queue length conditions are compared to
the effective queue length as reported by **lsload -E**, which is
normalized for multiprocessor hosts. Thresholds for these parameters
should be set at appropriate levels for single processor hosts.

## Example

MEM=100/10\
SWAP=200/30

These two lines translate into a loadSched condition of

mem\>=100 && swap\>=200

and a loadStop condition of

mem \< 10 \|\| swap \< 30

## Default

Undefined.

# MEMLIMIT

## Syntax

**MEMLIMIT =** \[*default_limit*\] *maximum_limit* .SS Description

The per-process (hard) process resident set size limit (in KB) for all
of the processes belonging to a job from this queue (see
**getrlimit**(**2**)).

Sets the maximum amount of physical memory (resident set size, RSS) that
may be allocated to a process.

By default, if a default memory limit is specified, jobs submitted to
the queue without a job-level memory limit are killed when the default
memory limit is reached.

If you specify only one limit, it is the maximum, or hard, memory limit.
If you specify two limits, the first one is the default, or soft, memory
limit, and the second one is the maximum memory limit.

Lava has two methods of enforcing memory usage:

> · OS Memory Limit Enforcement
>
> · Lava Memory Limit Enforcement

## OS Memory Limit Enforcement

OS memory limit enforcement is the default MEMLIMIT behavior and does
not require further configuration. OS enforcement usually allows the
process to eventually run to completion. Lava passes MEMLIMIT to the OS
which uses it as a guide for the system scheduler and memory allocator.
The system may allocate more memory to a process if there is a surplus.
When memory is low, the system takes memory from and lowers the
scheduling priority (re-nice) of a process that has exceeded its
declared MEMLIMIT. Only available on systems that support **RUSAGE_RSS**
for **setrlimit()**.

## Lava Memory Limit Enforcement

To enable Lava memory limit enforcement, set LSB_MEMLIMIT_ENFORCE in
lsf.conf to y. Lava memory limit enforcement explicitly sends a signal
to kill a running process once it has allocated memory past MEMLIMIT.

You can also enable Lava memory limit enforcement by setting
LSB_JOB_MEMLIMIT in lsf.conf to y. The difference between
LSB_JOB_MEMLIMIT set to y and LSB_MEMLIMIT_ENFORCE set to y is that with
LSB_JOB_MEMLIMIT, only the per-job memory limit enforced by Lava is
enabled. The per-process memory limit enforced by the OS is disabled.
With LSB_MEMLIMIT_ENFORCE set to y, both the per-job memory limit
enforced by Lava and the per-process memory limit enforced by the OS are
enabled.

Available for all systems on which Lava collects total memory usage.

## Example

The following configuration defines a queue with a memory limit of 5000
KB:

Begin Queue\
QUEUE_NAME = default\
DESCRIPTION = Queue with memory limit of 5000 kbytes\
MEMLIMIT = 5000\
End Queue

## Default

Unlimited

# MIG

## Syntax

**MIG =** *minutes* .SS Description

Enables automatic job migration and specifies the migration threshold,
in minutes.

If a checkpointable or rerunnable job dispatched to the host is
suspended (SSUSP state) for longer than the specified number of minutes,
the job is migrated (unless another job on the same host is being
migrated). A value of 0 (zero) specifies that a suspended job should be
migrated immediately.

If a migration threshold is defined at both host and queue levels, the
lower threshold is used.

## Default

Undefined (no automatic job migration).

# NEW_JOB_SCHED_DELAY

## Syntax

**NEW_JOB_SCHED_DELAY =** *seconds* .SS Description

The maximum or minimum length of time that a new job waits before being
dispatched; the behavior depends on whether the delay period specified
is longer or shorter than a regular dispatch interval (MBD_SLEEP_TIME in
lsb.params, 60 seconds by default).

> · If less than the dispatch interval, specifies the maximum number of
> seconds to wait, after a new job is submitted, before starting a new
> dispatch turn and scheduling the job. Usually, this causes Lava to
> schedule dispatch turns more frequently. You might notice performance
> problems (affecting the entire cluster) if this value is set too low
> in a busy queue.
>
> · If 0 (zero), starts a new dispatch turn as soon as a job is
> submitted to this queue (affecting the entire cluster).
>
> · If greater than the dispatch interval, specifies the minimum number
> of seconds to wait, after a new job is submitted, before scheduling
> the job. Has no effect of the timing of the dispatch turns, but new
> jobs in this queue are always delayed by one or more dispatch turns.

## Default

10 seconds.

# NICE

## Syntax

**NICE =** *integer* .SS Description

Adjusts the LINUX scheduling priority at which jobs from this queue
execute.

The default value of 0 (zero) maintains the default scheduling priority
for UNIX interactive jobs. This value adjusts the run-time priorities
for batch jobs on a queue-by-queue basis, to control their effect on
other batch or interactive jobs. See the **nice**(**1**) manual page for
more details.

## Default

0 (zero)

# PJOB_LIMIT

## Syntax

**PJOB_LIMIT =** *integer* .SS Description

Per-processor job slot limit for the queue.

Maximum number of job slots that this queue can use on any processor.
This limit is configured per processor, so that multiprocessor hosts
automatically run more jobs.

## Default

Unlimited

# POST_EXEC

## Syntax

**POST_EXEC = ***command* .SS Description

A command run on the execution host after the job.

## LINUX

The entire contents of the configuration line of the pre- and post-
execution commands are run under /bin/sh -c, so shell features can be
used in the command.

The pre- and post-execution commands are run in /tmp.

Standard input and standard output and error are set to:

/dev/null

The output from the pre- and post-execution commands can be explicitly
redirected to a file for debugging purposes.

The PATH environment variable is set to:

\"/bin /usr/bin /sbin/usr/sbin\"

## Default

No post-execution commands

# PRE_EXEC

## Syntax

**PRE_EXEC = ***command* .SS Description

A command run on the execution host before the job.

To specify a pre-execution command at the job level, use **bsub -E**. If
both queue and job level pre-execution commands are specified, the job
level pre-execution is run after the queue level pre-execution command.

For LINUX:

> · The entire contents of the configuration line of the pre- and post-
> execution commands are run under /bin/sh -c, so shell features can be
> used in the command.
>
> · The pre- and post-execution commands are run in /tmp.
>
> · Standard input and standard output and error are set to: /dev/null
>
> · The output from the pre- and post-execution commands can be
> explicitly redirected to a file for debugging purposes.
>
> · The PATH environment variable is set to: /bin /usr/bin
> /sbin/usr/sbin
>
> · If the pre-execution command exits with a non-zero exit code, it is
> considered to have failed, and the job is requeued to the head of the
> queue. This feature can be used to implement customized scheduling by
> having the pre-execution command fail if conditions for dispatching
> the job are not met.
>
> · Other environment variables set for the job are also set for the
> pre- and post-execution commands.

## Default

No pre-execution commands

# PROCESSLIMIT

## Syntax

**PROCESSLIMIT =** \[*default_limit*\] *maximum_limit* .SS Description

Limits the number of concurrent processes that can be part of a job.

By default, if a default process limit is specified, jobs submitted to
the queue without a job-level process limit are killed when the default
process limit is reached.

If you specify only one limit, it is the maximum, or hard, process
limit. If you specify two limits, the first one is the default, or soft,
process limit, and the second one is the maximum process limit.

## Default

Unlimited

# PROCLIMIT

## Syntax

**PROCLIMIT =** \[*minimum_limit* \[*default_limit*\]\] *maximum_limit*
.SS Description

Maximum number of slots that can be allocated to a job. For parallel
jobs, the maximum number of processors that can be allocated to t he
job.

Optionally specifies the minimum and default number of job slots.

Jobs that specify fewer slots than the minimum PROCLIMIT or more slots
than the maximum PROCLIMIT cannot use this queue and are rejected.

All limits must be positive numbers greater than or equal to 1 that
satisfy the following relationship:

1 \<= *minimum* \<= *default* \<= *maximum* .PP You can specify up to
three limits in the PROCLIMIT parameter:

If you specify one limit, it is the maximum processor limit. The minimum
and default limits are set to 1.

If you specify two limits, the first is the minimum processor limit, and
the second one is the maximum. The default is set equal to the minimum.
The minimum must be less than or equal to the maximum.

If you specify three limits, the first is the minimum processor limit,
the second is the default processor limit, and the third is the
maximum.The minimum must be less than the default and the maximum.

## Default

Unlimited, the default number of slots is 1.

# QJOB_LIMIT

## Syntax

**QJOB_LIMIT** **=** *integer* .SS Description

Job slot limit for the queue. Total number of job slots that this queue
can use.

## Default

Unlimited

# QUEUE_NAME

## Syntax

**QUEUE_NAME =** *string* .SS Description

Required. Name of the queue.

Specify any ASCII string up to 40 characters long. You can use letters,
digits, underscores (\_) or dashes (-). You cannot use blank spaces. You
cannot specify the reserved name default.

## Default

You must specify this parameter to define a queue. The default queue
automatically created by Lava is named default.

**REQUEUE_EXIT_VALUES** **=** \[*exit_code *\...\]
\[**EXCLUDE(***exit_code \...***)**\]

## Description

Enables automatic job requeue and sets the LSB_EXIT_REQUEUE environment
variable.

Separate multiple exit codes with spaces. Define an exit code as
EXCLUDE(*exit_code*) to enable exclusive job requeue. Exclusive job
requeue does not work for parallel jobs.

Jobs are requeued to the head of the queue from which they were
dispatched. The output from the failed run is not saved, and the user is
not notified by Lava.

A job terminated by a signal is not requeued.

If MBD is restarted, it will not remember the previous hosts from which
the job exited with an exclusive requeue exit code. In this situation,
it is possible for a job to be dispatched to hosts on which the job has
previously exited with an exclusive exit code.

Automatic job requeue and exclusive job requeue are described in the
*Lava Administrator\'s Guide*.

## Example

REQUEUE_EXIT_VALUES=30 EXCLUDE(20)

means that jobs with exit code 30 are requeued, jobs with exit code 20
are requeued exclusively, and jobs with any other exit code are not
requeued.

## Default

Undefined (jobs in this queue are not requeued)

# RERUNNABLE

## Syntax

**RERUNNABLE = yes** \| **no ** .SS Description

If yes, enables automatic job rerun (restart).

## Default

no

# RES_REQ

## Syntax

**RES_REQ =** *res_req* .SS Description

Resource requirements used to determine eligible hosts. Specify a
resource requirement string as usual. The resource requirement string
lets you specify conditions in a more flexible manner than using the
load thresholds.

The select section defined at the queue level must be satisfied at in
addition to any job-level requirements or load thresholds.

The rusage section defined at the queue level overrides the rusage
section defined at the job level, and jobs are rejected if they specify
resource reservation requirements that exceed the requirements specified
at the queue level.

The order section defined at the queue level is ignored if any resource
requirements are specified at the job level (if the job-level resource
requirements do not include the order section, the default order,
r15s:pg, is used instead of the queue-level resource requirement).

The span section defined at the queue level is ignored if the span
section is also defined at the job level.

If RES_REQ is defined at the queue level and there are no load
thresholds defined, the pending reasons for each individual load index
will not be displayed by **bjobs**.

## Default

select\[type==local\] order\[r15s:pg\]. If this parameter is defined and
a host model or Boolean resource is specified, the default type will be
any.

# RESUME_COND

## Syntax

**RESUME_COND = ***res_req* .PP Use the select section of the resource
requirement string to specify load thresholds. All other sections are
ignored.

## Description

Lava automatically resumes a suspended (SSUSP) job in this queue if the
load on the host satisfies the specified conditions.

If RESUME_COND is not defined, then the loadSched thresholds are used to
control resuming of jobs. The loadSched thresholds are ignored, when
resuming jobs, if RESUME_COND is defined.

# RUN_WINDOW

## Syntax

**RUN_WINDOW =** *time_window *\...

## Description

Time periods during which jobs in the queue are allowed to run.

When the window closes, Lava suspends jobs running in the queue and
stops dispatching jobs from the queue. When the window reopens, Lava
resumes the suspended jobs and begins dispatching additional jobs.

## Default

Undefined (queue is always active)

# RUNLIMIT

## Syntax

**RUNLIMIT = **\[*default_limit*\] *maximum_limit* .PP where
*default_limit* and *maximum_limit* are:

\[*hours*:\]*minutes*\[/*host_name* \| /*host_model*\]

## Description

The maximum run limit and optionally the default run limit. The name of
a host or host model specifies the run time normalization host to use.

By default, jobs that are in the RUN state for longer than the specified
maximum run limit are killed by Lava. You can optionally provide your
own termination job action to override this default.

Jobs submitted with a job-level run limit (**bsub -W**) that is less
than the maximum run limit are killed when their job-level run limit is
reached. Jobs submitted with a run limit greater than the maximum run
limit are rejected by the queue.

If a default run limit is specified, jobs submitted to the queue without
a job-level run limit are killed when the default run limit is reached.

If you specify only one limit, it is the maximum, or hard, run limit. If
you specify two limits, the first one is the default, or soft, run
limit, and the second one is the maximum run limit. The number of
minutes may be greater than 59. Therefore, three and a half hours can be
specified either as 3:30, or 210.

## Default

Unlimited

# SLOT_RESERVE

## Syntax

**SLOT_RESERVE = MAX_RESERVE_TIME\[***integer***\]** .SS Description

Enables processor reservation and specifies the number of dispatch turns
over which a parallel job can reserve job slots.

After this time, if a job has not accumulated enough job slots to start,
it releases all its reserved job slots. This means a job cannot reserve
job slots for more than (*integer *\* MBD_SLEEP_TIME) seconds.

MBD_SLEEP_TIME is defined in lsb.params; the default value is 60
seconds.

## Example

SLOT_RESERVE = MAX_RESERVE_TIME\[5\]

This example specifies that parallel jobs have up to 5 dispatch turns to
reserve sufficient job slots (equal to 5 minutes, by default).

## Default

Undefined (no processor reservation)

# STACKLIMIT

## Syntax

**STACKLIMIT =** *integer* .SS Description

The per-process (hard) stack segment size limit (in KB) for all of the
processes belonging to a job from this queue (see **getrlimit**(**2**)).

## Default

Unlimited

# STOP_COND

## Syntax

**STOP_COND =** *res_req* .PP Use the select section of the resource
requirement string to specify load thresholds. All other sections are
ignored.

## Description

Lava automatically suspends a running job in this queue if the load on
the host satisfies the specified conditions.

> · Lava will not suspend the only job running on the host if the
> machine is interactively idle (it \> 0).
>
> · Lava will not suspend a forced job (**brun -f**).
>
> · Lava will not suspend a job because of paging rate if the machine is
> interactively idle.

If STOP_COND is specified in the queue and there are no load thresholds,
the suspending reasons for each individual load index will not be
displayed by **bjobs**.

## Example

STOP_COND= select\[((!cs && it \< 5) \|\| (cs && mem \< 15 && swap \<
50))\]

In this example, assume \"cs\" is a Boolean resource indicating that the
host is a computer server. The stop condition for jobs running on
computer servers is based on the availability of swap memory. The stop
condition for jobs running on other kinds of hosts is based on the idle
time.

# SWAPLIMIT

## Syntax

**SWAPLIMIT =** *integer* .SS Description

The amount of total virtual memory limit (in KB) for a job from this
queue.

This limit applies to the whole job, no matter how many processes the
job may contain.

The action taken when a job exceeds its SWAPLIMIT or PROCESSLIMIT is to
send SIGQUIT, SIGINT, SIGTERM, and SIGKILL in sequence. For CPULIMIT,
SIGXCPU is sent before SIGINT, SIGTERM, and SIGKILL.

## Default

Unlimited

# TERMINATE_WHEN

## Description

Configures the queue to invoke the TERMINATE action instead of the
SUSPEND action in the specified circumstance.

## Syntax

**TERMINATE_WHEN = WINDOW** \| **LOAD** .RS

· WINDOW \-- kills jobs if the run window closes.

· LOAD \-- kills jobs when the load exceeds the suspending thresholds.

## Example

Set TERMINATE_WHEN to WINDOW to define a night queue that will kill jobs
if the run window closes:

Begin Queue\
NAME = night\
RUN_WINDOW = 20:00-08:00\
TERMINATE_WHEN = WINDOW\
JOB_CONTROLS = TERMINATE\[kill -KILL \$LS_JOBPGIDS; mail - s \"job
\$LSB_JOBID killed by queue run window\" \$USER \< /dev/null\]\
End Queue

# UJOB_LIMIT

## Syntax

**UJOB_LIMIT** **=** *integer* .SS Description

Per-user job slot limit for the queue. Maximum number of job slots that
each user can use in this queue.

## Default

Unlimited

# USERS

## Syntax

**USERS =** **all** \| *user_name* \| *user_group* \...

## Description

A list of users or user groups that can submit jobs to this queue

Use the reserved word all to specify all Lava users.

Lava cluster administrators can submit jobs to this queue or switch any
user\'s jobs into this queue, even if they are not listed.

## Default

all

# SEE ALSO

lsf.cluster(5), lsf.conf(5), lsb.params(5), lsb.hosts(5), lsb.users(5),
busers(1), bugroup(1), bchkpnt(1), nice(1), getgrnam(3), getrlimit(2),
bmgroup(1), bqueues(1), bhosts(1), bsub(1), lsid(1), mbatchd(8),
badmin(8)
