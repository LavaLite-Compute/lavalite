\

# NAME

**lsacct** - displays accounting statistics on finished RES tasks in the
Lava system

# SYNOPSIS

**lsacct **\[**-l**\] \[**-C ***time0***,***time1*\] \[**-S
***time0***,***time1*\] \[**-f ***logfile_name*\] \[**-m ***host_name*\]
\[**-u ***user_name* \...** \| -u all**\] \[*pid *\...\]** ** .PP
**lsacct **\[**-h** \| **-V**\]

# DESCRIPTION

Displays statistics on finished tasks run through RES. When a remote
task completes, RES logs task statistics in the task log file.

By default, displays accounting statistics for only tasks owned by the
user who invoked the lsacct command.

By default, displays accounting statistics for tasks executed on all
hosts in the Lava system.

By default, displays statistics for tasks logged in the task log file
currently used by RES: LSF_RES_ACCTDIR/lsf.acct.*host_name* or
/tmp/lsf.acct.*host_name* (see lsf.acct(5)).

If -l is not specified, the default is to display the fields in SUMMARY
only (see OUTPUT).

The RES on each host writes its own accounting log file.

All times are reported in seconds. All sizes are reported in kilobytes.

# OPTIONS

**-l **

:   

    Per-task statistics. Displays statistics about each task. See OUTPUT
    for a description of information that is displayed.

```{=html}
<!-- -->
```

**-C** *time0***,***time1 *

:   

    Displays accounting statistics for only tasks that completed or
    exited during the specified time interval.

> The time format is the same as in bhist(1).

**-S** *time0***,***time1*

:   

    Displays accounting statistics for only tasks that began executing
    during the specified time interval.

> The time format is the same as in bhist(1).

**-f** *logfile_name* 

:   

    Searches the specified task log file for accounting statistics.
    Specify either an absolute or a relative path.

```{=html}
<!-- -->
```

**-m** *host_name* \... 

:   

    Displays accounting statistics for only tasks executed on the
    specified hosts.

> If a list of hosts is specified, host names must be separated by
> spaces and enclosed in quotation marks (\") or (\').

**-u** *user_name *\... \| **-u all **

:   

    Displays accounting statistics for only tasks owned by the specified
    users, or by all users if the keyword all is specified.

> If a list of users is specified, user names must be separated by
> spaces and enclosed in quotation marks (\") or (\'). You can specify
> both user names and user IDs in the list of users.

*pid *\... 

:   

    Displays accounting statistics for only tasks with the specified
    *pid*. This option overrides all other options except for -l, -f**,
    **-h, -V.

```{=html}
<!-- -->
```

**-h**

:   

    Prints command usage to stderr and exits.

```{=html}
<!-- -->
```

**-V** 

:   

    Prints Lava release version to stderr and exits.

# OUTPUT

## SUMMARY (default format)

Overall statistics for tasks.

The total, average, maximum and minimum resource usage statistics apply
to all specified tasks.

The following fields are displayed:

> Total number of tasks

> > Total number of tasks including tasks completed successfully and
> > total number of exited tasks.

> Time range of started tasks

> > Start time of the first and last task selected.

> Time range of ended tasks

> > Completion or exit time of the first and last task selected.

> Resource usage of tasks selected

> > See getrusage (2).

> CPU time

> > Total CPU time consumed by the task.

> Page faults

> > Number of page faults.

> Swaps

> > Number of times the process was swapped out.

> Blocks in

> > Number of input blocks.

> Blocks out

> > Number of output blocks.

> Messages sent

> > Number of System VIPC messages sent.

> Messages rcvd

> > Number of IPC messages received.

> Voluntary cont sw

> > Number of voluntary context switches.

> Involuntary con sw

> > Number of involuntary context switches.

> Turnaround

> > Elapsed time from task execution to task completion.

## Per Task Statistics ( -l)

In addition to the fields displayed by default in SUMMARY, displays the
following fields for each task:

> Starting time

> > Time the task started.

> User and host name

> > User who submitted the task, host from which the task was submitted,
> > in the format *user_name*@*host*.

> PID

> > UNIX process ID of the task.

> Execution host

> > Host on which the command was run.

> Command line

> > Complete command line that was executed.

> CWD

> > Current working directory of the task.

> Completion time

> > Time at which the task completed.

> Exit status

> > UNIX exit status of the task.

# FILES

Reads lsf.acct.*host_name* .SH SEE ALSO

lsf.acct(5), res(8), bhist(1)
