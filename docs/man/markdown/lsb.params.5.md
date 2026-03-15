\

# NAME

**lsb.params**

## Overview

The lsb.params file defines general parameters used by Lava Batch. This
file contains only one section, named Parameters. MBD uses lsb.params
for initialization. The file is optional. If not present, the
Lava-defined defaults are assumed.

Some of the parameters that can be defined in lsb.params control timing
within the Lava Batch system. The default settings provide good
throughput for long-running batch jobs while adding a minimum of
processing overhead in the batch daemons.

## Contents

> · Parameters Section

# Parameters Section

This section and all the keywords in this section are optional. If
keywords are not present, Lava Batch assumes default values for the
corresponding keywords. The valid keywords for this section are:

# ACCT_ARCHIVE_AGE

## Syntax

*days* .SS Description

Enables automatic archiving of Lava accounting log files, and specifies
the archive interval. Lava archives the current log file if the length
of time from its creation date exceeds the specified number of days.

## See Also

ACCT_ARCHIVE_SIZE also enables automatic archiving.

MAX_ACCT_ARCHIVE_FILE enables automatic deletion of the archives.

## Default

Undefined (no limit to the age of lsb.acct).

# ACCT_ARCHIVE_SIZE

## Syntax

*kilobytes* .SS Description

Enables automatic archiving of Lava accounting log files, and specifies
the archive threshold. Lava archives the current log file if its size
exceeds the specified number of kilobytes.

## See Also

ACCT_ARCHIVE_AGE also enables automatic archiving.

MAX_ACCT_ARCHIVE_FILE enables automatic deletion of the archives.

## Default

Undefined (no limit to the size of lsb.acct).

# CLEAN_PERIOD

## Syntax

**CLEAN_PERIOD** **=** *seconds* .SS Description

For non-repetitive jobs, the amount of time that job records for jobs
that have finished or have been killed are kept in MBD core memory after
they have finished.

Users can still see all jobs after they have finished using the
**bjobs** command. For jobs that finished more than CLEAN_PERIOD seconds
ago, use the **bhist** command.

## Default

3600 (1 hour).

# DEFAULT_HOST_SPEC

## Syntax

**DEFAULT_HOST_SPEC =** *host_name \| host_model* .SS Description

The default CPU time normalization host for the cluster.

The CPU factor of the specified host or host model will be used to
normalize the CPU time limit of all jobs in the cluster, unless the CPU
time normalization host is specified at the queue or job level.

## Default

Undefined

# DEFAULT_PROJECT

## Syntax

**DEFAULT_PROJECT** **=** *project_name* .SS Description

The name of the default project. Specify any string.

When you submit a job without specifying any project name, and the
environment variable LSB_DEFAULTPROJECT is not set, Lava automatically
assigns the job to this project.

## Default

default

# DEFAULT_QUEUE

## Syntax

**DEFAULT_QUEUE** **=** *queue_name *\...

## Description

Space-separated list of candidate default queues (candidates must
already be defined in lsb.queues).

When you submit a job to Lava without explicitly specifying a queue, and
the environment variable LSB_DEFAULTQUEUE is not set, Lava puts the job
in the first queue in this list that satisfies the job\'s specifications
subject to other restrictions, such as requested hosts, queue status,
etc.

## Default

Undefined. When a user submits a job to Lava without explicitly
specifying a queue, and there are no candidate default queues defined
(by this parameter or by the user\'s environment variable
LSB_DEFAULTQUEUE), Lava automatically creates a new queue named default,
using the default configuration, and submits the job to that queue.

# DISABLE_UACCT_MAP

## Syntax

**DISABLE_UACCT_MAP = y \| Y** .SS Description

Specify y or Y to disable user-level account mapping.

## Default

Undefined

# JOB_ACCEPT_INTERVAL

## Syntax

**JOB_ACCEPT_INTERVAL =** *integer* .SS Description

The number of dispatch turns to wait after dispatching a job to a host,
before dispatching a second job to the same host. By default, a dispatch
turn lasts 60 seconds (MBD_SLEEP_TIME in lsb.params).

If 0 (zero), a host may accept more than one job in each job dispatching
interval. By default, there is no limit to the total number of jobs that
can run on a host, so if this parameter is set to 0, a very large number
of jobs might be dispatched to a host all at once. You may notice
performance problems if this occurs.

JOB_ACCEPT_INTERVAL set at the queue level (lsb.queues) overrides
JOB_ACCEPT_INTERVAL set at the cluster level (lsb.params).

## Default

1

# JOB_DEP_LAST_SUB

## Description

Used only with job dependency scheduling.

If set to 1, whenever dependency conditions use a job name that belongs
to multiple jobs, Lava evaluates only the most recently submitted job.

Otherwise, all the jobs with the specified name must satisfy the
dependency condition.

## Default

Undefined

# JOB_PRIORITY_OVER_TIME

## Syntax

**JOB_PRIORITY_OVER_TIME=***increment***/***interval*

## Description

JOB_PRIORITY_OVER_TIME enables automatic job priority escalation when
MAX_USER_PRIORITY is also defined.

## Valid Values

*increment* .IP Specifies the value used to increase job priority every
*interval* minutes. Valid values are positive integers.

*interval* .IP Specifies the frequency, in minutes, to *increment* job
priority. Valid values are positive integers.

## Default

Undefined

## Example

JOB_PRIORITY_OVER_TIME=3/20

> Specifies that every 20 minute *interval* *increment* to job priority
> of pending jobs by 3.

## See Also

MAX_USER_PRIORITY

# JOB_SPOOL_DIR

## Syntax

**JOB_SPOOL_DIR =** *dir* .SS Description

Specifies the directory for buffering batch standard output and standard
error for a job

When JOB_SPOOL_DIR is defined, the standard output and standard error
for the job is buffered in the specified directory.

Except for **bsub -is** and **bsub -Zs**, if JOB_SPOOL_DIR is not
accessible or does not exist, output is spooled to the default job
output directory .lsbatch.

For **bsub -is** and **bsub -Zs**, JOB_SPOOL_DIR must be readable and
writable by the job submission user, and it must be shared by the master
host, the submission host, and the execution host. If the specified
directory is not accessible or does not exist, **bsub -is** and **bsub
-Zs** cannot write to the default directory and the job will fail.

As Lava runs jobs, it creates temporary directories and files under
JOB_SPOOL_DIR. By default, Lava removes these directories and files
after the job is finished. See **bsub**(**1**) for information about job
submission options that specify the disposition of these files.

On UNIX, specify an absolute path. For example:

JOB_SPOOL_DIR=/home/share/lsf_spool

JOB_SPOOL_DIR can be any valid path up to a maximum length of 256
characters. This maximum path length includes the temporary directories
and files that Lava Batch creates as jobs run. The path you specify for
JOB_SPOOL_DIR should be as short as possible to avoid exceeding this
limit.

## Default

Undefined

Batch job output (standard output and standard error) is sent to the

> · On UNIX: \$HOME/.lsbatch

> If %HOME% is specified in the user environment, uses that directory
> instead of %windir% for spooled output.

# JOB_TERMINATE_INTERVAL

## Syntax

**JOB_TERMINATE_INTERVAL =** *seconds* .SS Description

UNIX only.

Specifies the time interval in seconds between sending SIGINT, SIGTERM,
and SIGKILL when terminating a job. When a job is terminated, the job is
sent SIGINT, SIGTERM, and SIGKILL in sequence with a sleep time of
JOB_TERMINATE_INTERVAL between sending the signals. This allows the job
to clean up if necessary.

## Default

10

# MAX_ACCT_ARCHIVE_FILE

## Syntax

MAX_ACCT_ARCHIVE_FILE=*integer* .SS Description

Enables automatic deletion of archived Lava accounting log files and
specifies the archive limit.

## Compatibility

ACCT_ARCHIVE_SIZE or ACCT_ARCHIVE_AGE should also be defined.

## Example

MAX_ACCT_ARCHIVE_FILE=10

Lava maintains the current lsb.acct and up to 10 archives. Every time
the old lsb.acct.9 becomes lsb.acct.10, the old lsb.acct.10 gets
deleted.

## Default

Undefined (no deletion of lsb.acct.*n* files).

# MAX_JOB_ARRAY_SIZE

## Syntax

**MAX_JOB_ARRAY_SIZE =** *integer* .SS Description

Specifies the maximum index value of a job array that can be created by
a user for a single job submission. The maximum number of jobs in a job
array cannot exceed this value, and will be less if some index values
are not used (start, end, and step values can all be used to limit the
indices used in a job array).

A large job array allows a user to submit a large number of jobs to the
system with a single job submission.

Specify an integer value from 1 to 65534.

## Default

1000

# MAX_JOBID

## Syntax

**MAX_JOBID=***integer* .SS Description

The job ID limit. The job ID limit is the highest job ID that Lava will
ever assign, and also the maximum number of jobs in the system.

Specify any integer from 999999 to 9999999 (for practical purposes, any
seven-digit integer).

## Example

MAX_JOBID=1234567

## Default

999999

# MAX_JOB_NUM

## Syntax

**MAX_JOB_NUM** **=** *integer* .SS Description

The maximum number of finished jobs whose events are to be stored in the
lsb.events log file.

Once the limit is reached, MBD starts a new event log file. The old
event log file is saved as lsb.events.*n*, with subsequent sequence
number suffixes incremented by 1 each time a new log file is started.
Event logging continues in the new lsb.events file.

## Default

1000

# MAX_SBD_FAIL

## Syntax

**MAX_SBD_FAIL = ***integer* .SS Description

The maximum number of retries for reaching a non-responding slave batch
daemon, SBD.

The interval between retries is defined by MBD_SLEEP_TIME. If MBD fails
to reach a host and has retried MAX_SBD_FAIL times, the host is
considered unavailable. When a host becomes unavailable, MBD assumes
that all jobs running on that host have exited and that all rerunnable
jobs (jobs submitted with the **bsub** **-r** option) are scheduled to
be rerun on another host.

## Default

3

# MAX_SBD_CONNS

## Syntax

**MAX_SBD_CONNS = ***integer* .SS Description

The maximum number of files mbatchd can have open and connected to
sbatchd

# MAX_SCHED_STAY

## Syntax

**MAX_SCHED_STAY = ***integer* .SS Description

The time in seconds the mbatchd has for scheduling pass.

## Default

3

# MAX_USER_PRIORITY

## Syntax

**MAX_USER_PRIORITY=***integer* .SS Description

Enables user-assigned job priority and specifies the maximum job
priority a user can assign to a job.

Lava administrators can assign a job priority higher than the specified
value.

## Compatibility

User-assigned job priority changes the behavior of **btop** and
**bbot**.

## Example

MAX_USER_PRIORITY=100

Specifies that 100 is the maximum job priority that can be specified by
a user.

## Default

Undefined

## See Also

bsub, bmod, btop, bbot, JOB_PRIORITY_OVER_TIME

# MBD_SLEEP_TIME

## Syntax

**MBD_SLEEP_TIME =** *seconds* .SS Description

The job dispatching interval; how often Lava tries to dispatch pending
jobs.

## Default

60

# PG_SUSP_IT

## Syntax

**PG_SUSP_IT =** *seconds* .SS Description

The time interval that a host should be interactively idle (it \> 0)
before jobs suspended because of a threshold on the pg load index can be
resumed.

This parameter is used to prevent the case in which a batch job is
suspended and resumed too often as it raises the paging rate while
running and lowers it while suspended. If you are not concerned with the
interference with interactive jobs caused by paging, the value of this
parameter may be set to 0.

## Default

180 (seconds)

# SBD_SLEEP_TIME

## Syntax

**SBD_SLEEP_TIME =** *seconds* .SS Description

The interval at which Lava checks the load conditions of each host, to
decide whether jobs on the host must be suspended or resumed.

## Default

30

# SHARED_RESOURCE_UPDATE_FACTOR

## Syntax

**SHARED_RESOURCE_UPDATE_FACTOR = ***integer* .SS Description

Determines the static shared resource update interval for the cluster.

Specify approximately how many times to update static shared resources
during one MBD sleep time period. The formula is:

*interval* = MBD_SLEEP_TIME / SHARED_RESOURCE_UPDATE_FACTOR

where the result of the calculation is truncated to an integer. The
static shared resource update interval is in seconds.

## Default

Undefined (all resources are updated only once, at the start of each
dispatch turn).
