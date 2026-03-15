\

# NAME

**bkill** - sends signals to kill, suspend, or resume unfinished jobs

# SYNOPSIS

**bkill **\[**-l**\] \[**-R**\] \[**-J ***job_name*\] \[**-m
***host_name \| ***-m*** host_group*\] \[**-q ***queue_name*\] \[**-r**
\| **-s (***signal_value \| signal_name***)**\] \[**-u ***user_name \|
***-u*** user_group*** \| -u all**\] \[*job_ID *\... \| **0** \|
**\"***job_ID***\[***index***\]\"** \...\]

**bkill **\[**-h** \| **-V**\]

# DESCRIPTION

By default, sends a set of signals to kill the specified jobs. On LINUX,
SIGINT and SIGTERM are sent to give the job a chance to clean up before
termination, then SIGKILL is sent to kill the job. The time interval
between sending each signal is defined by the JOB_TERMINATE_INTERVAL
parameter in lsb.params(5).

Users can only operate on their own jobs. Only root and Lava
administrators can operate on jobs submitted by other users.

If a signal request fails to reach the job execution host, Lava tries
the operation later when the host becomes reachable. Lava retries the
most recent signal request.

If a job is running in a queue with CHUNK_JOB_SIZE set, bkill has the
following results depending on job state:

## PEND

Job is removed from chunk (NJOBS -1, PEND -1)

## RUN

All jobs in the chunk are suspended (NRUN -1, NSUSP +1)

## USUSP

Job finishes, next job in the chunk starts if one exists (NJOBS -1, PEND
-1, SUSP -1, RUN +1)

## WAIT

Job finishes (NJOBS-1, PEND -1)

Using bkill on a repetitive job kills the current run, if the job has
been started, and requeues the job. See bcadd(1) and bsub(1) for
information on setting up a job to run repetitively.

If the job cannot be killed, use bkill -r to remove the job from the
Lava system without waiting for the job to terminate, and free the
resources of the job.

A job ID or -m, -u, -q, or -J must be specified with bkill.

# OPTIONS

**-l**

:   

    Displays the signal names supported by bkill. This is a subset of
    signals supported by /bin/kill and is platform-dependent.

```{=html}
<!-- -->
```

**-J** *job_name*

:   

    Operates only on jobs with the specified *job_name*. The -J option
    is ignored if a job ID other than 0 is specified in the *job_ID*
    option.

```{=html}
<!-- -->
```

**-m** *host_name *\| **-m*** host_group*

:   

    Operates only on jobs dispatched to the specified host or host
    group.

> If *job_ID* is not specified, only the most recently submitted
> qualifying job is operated on. The -m option is ignored if a job ID
> other than 0 is specified in the *job_ID* option. See bhosts(1) and
> bmgroup(1) for more information about hosts and host groups.

**-q** *queue_name*

:   

    Operates only on jobs in the specified queue.

> If *job_ID* is not specified, only the most recently submitted
> qualifying job is operated on.

> The -q option is ignored if a job ID other than 0 is specified in the
> *job_ID* option.

> See bqueues(1) for more information about queues.

**-r**

:   

    Removes a job from the Lava system without waiting for the job to
    terminate in the operating system.

> Sends the same series of signals as bkill without -r, except that the
> job is removed from the system immediately, the job is marked as EXIT,
> and the job resources that Lava monitors are released as soon as Lava
> receives the first signal.

> Also operates on jobs for which a bkill command has been issued but
> which cannot be reached to be acted on by SBD (jobs in ZOMBI state).
> If SBD recovers before the jobs are completely removed, Lava ignores
> the zombi jobs killed with bkill -r.

> Use **bkill -r** only on jobs that cannot be killed in the operating
> system, or on jobs that cannot be otherwise removed using **bkill**.

> The -r option cannot be used with the -s option.

**-s (***signal_value* \| *signal_name***)**

:   

    Sends the specified signal to specified jobs. You can specify either
    a name, stripped of the SIG prefix (such as KILL), or a number (such
    as 9).

> Eligible signal names are listed by **bkill -l**.

> The **-s** option cannot be used with the **-r** option.

> Use bkill -s to suspend and resume jobs by using the appropriate
> signal instead of using bstop or bresume. Sending the SIGCONT signal
> is the same as using bresume. Sending the SIGSTOP signal to sequential
> jobs or the SIGTSTP to parallel jobs is the same as using bstop.

> You cannot suspend a job that is already suspended, or resume a job
> that is not suspended. Using SIGSTOP or SIGTSTP on a job that is in
> the USUSP state has no effect and using SIGCONT on a job that is not
> in either the PSUSP or the USUSP state has no effect. See bjobs(1) for
> more information about job states.

**-u** *user_name *\|* ***-u*** user_group *\| **-u all**

:   

    Operates only on jobs submitted by the specified user or user group
    (see bugroup(1)), or by all users if the reserved user name all is
    specified.

> If *job_ID* is not specified, only the most recently submitted
> qualifying job is operated on. The -u option is ignored if a job ID
> other than 0 is specified in the *job_ID* option.

*job_ID *\... \| **0** \| **\"***job_ID***\[***index***\]\" **\...

:   

    Operates only on jobs that are specified by *job_ID* or**
    \"***job_ID***\[***index***\]\"**, where
    **\"***job_ID***\[***index***\]\"** specifies selected job array
    elements (see bjobs(1)). For job arrays, quotation marks must
    enclose the job ID and index, and index must be enclosed in square
    brackets.

> Jobs submitted by any user can be specified here without using the -u
> option. If you use the reserved job ID 0, all the jobs that satisfy
> other options (that is, -m, -q, -u and -J) are operated on; all other
> job IDs are ignored.

> The options -u, -q, -m and -J have no effect if a job ID other than 0
> is specified. Job IDs are returned at job submission time (see
> bsub(1)) and may be obtained with the bjobs command (see bjobs(1)).

**-h**

:   

    Prints command usage to stderr and exits.

```{=html}
<!-- -->
```

**-V**

:   

    Prints Lava release version to stderr and exits.

# EXAMPLES

\%** bkill -s 17 -q night** .PP Sends signal 17 to the last job that was
submitted by the invoker to queue night.

\%** bkill -q short -u all 0** .PP Kills all the jobs that are in the
queue short.

\%** bkill -r 1045** .PP Forces the removal of unkillable job 1045.

# SEE ALSO

bsub(1), bjobs(1), bqueues(1), bhosts(1), bresume(1), bstop(1),
bparams(5), mbatchd(8), kill(1), signal(2)
