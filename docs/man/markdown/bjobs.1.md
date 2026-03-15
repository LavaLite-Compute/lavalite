\

# NAME

**bjobs** - displays information about Lava jobs

# SYNOPSIS

**bjobs** \[**-a**\] \[**-A**\] \[**-w** \| **-l**\] \[**-J**
*job_name*\] \[**-m** *host_name* \| **-m** *host_group*\] \[**-N**
*host_name \| ***-N** *host_model \| ***-N ***CPU_factor*\] \[**-P***
project_name*\] \[**-q** *queue_name*\] \[**-u ***user_name* \| **-u**
*user_group* \| **-u all**\]** ***job_ID \...* .PP **bjobs** \[**-d**\]
\[**-p**\] \[**-r**\] \[**-s**\] \[**-A**\] \[**-w** \| **-l**\]
\[**-J** *job_name*\] \[**-m** *host_name* \| **-m** *host_group*\]
\[**-N** *host_name \| ***-N** *host_model \| ***-N ***CPU_factor*\]
\[**-P*** project_name*\] \[**-q** *queue_name*\] \[**-u ***user_name*
\| **-u** *user_group* \| **-u all**\]** ***job_ID \...* .PP **bjobs
**\[**-h** \| **-V**\]

# DESCRIPTION

Displays information about jobs.

By default, displays information about your own pending, running and
suspended jobs.

To display older historical information, use bhist.

# OPTIONS

**-a**

:   

    Displays information about jobs in all states, including finished
    jobs that finished recently, within an interval specified by
    CLEAN_PERIOD in lsb.params (the default period is 1 hour).

```{=html}
<!-- -->
```

**-A**

:   

    Displays summarized information about job arrays. If you specify job
    arrays with the job array ID, and also specify -A, do not include
    the index list with the job array ID.

> You can use -w to show the full array specification, if necessary.

**-d**

:   

    Displays information about jobs that finished recently, within an
    interval specified by CLEAN_PERIOD in lsb.params (the default period
    is 1 hour).

```{=html}
<!-- -->
```

**-p**

:   

    Displays pending jobs, together with the pending reasons that caused
    each job not to be dispatched during the last dispatch turn. The
    pending reason shows the number of hosts for that reason, or names
    the hosts if -l is also specified.

> Each pending reason is associated with one or more hosts and it states
> the cause why these hosts are not allocated to run the job. In
> situations where the job requests specific hosts (using bsub -m),
> users may see reasons for unrelated hosts also being displayed,
> together with the reasons associated with the requested hosts. The
> life cycle of a pending reason ends after a new dispatch turn starts.
> The reason may not reflect the current load situation because it could
> last as long as the interval specified by MBD_SLEEP_TIME in
> lsb.params.

> When the job slot limit is reached for a job array (bsub -J
> \"jobArray\[indexList\]%job_slot_limit\") the following message is
> displayed:

> The job array has reached its job slot limit.

**-r**

:   

    Displays running jobs.

```{=html}
<!-- -->
```

**-s**

:   

    Displays suspended jobs, together with the suspending reason that
    caused each job to become suspended.

> The suspending reason may not remain the same while the job stays
> suspended. For example, a job may have been suspended due to the
> paging rate, but after the paging rate dropped another load index
> could prevent the job from being resumed. The suspending reason will
> be updated according to the load index. The reasons could be as old as
> the time interval specified by SBD_SLEEP_TIME in lsb.params. So the
> reasons shown may not reflect the current load situation.

**-w**

:   

    Wide format. Displays job information without truncating fields.

```{=html}
<!-- -->
```

**-l**

:   

    Long format. Displays detailed information for each job in a
    multi-line format.

> The -l option displays the following additional information: project
> name, job command, current working directory on the submission host,
> pending and suspending reasons, job status, resource usage, resource
> limits information.

> Use bjobs -A -l to display detailed information for job arrays
> including job array job limit (%*job_limit*) if set.

*job_ID*

:   

    Displays information about the specified jobs or job arrays.

> If you use -A, specify job array IDs without the index list.

**-J** *job_name*

:   

    Displays information about the specified jobs or job arrays.

```{=html}
<!-- -->
```

**-m** *host_name* \| **-m** *host_group* 

:   

    Only displays jobs dispatched to the specified hosts.

> To determine the available hosts and host groups, use bhosts and
> bmgroup.

**-N** *host_name *\| **-N ***host_model *\| **-N ***CPU_factor* 

:   

    Displays the normalized CPU time consumed by the job. Normalizes
    using the CPU factor specified, or the CPU factor of the host or
    host model specified.

```{=html}
<!-- -->
```

**-P ***project_name *

:   

    Only displays jobs that belong to the specified project.

```{=html}
<!-- -->
```

**-q** *queue_name *

:   

    Only displays jobs in the specified queue.

> The command bqueues returns a list of queues configured in the system,
> and information about the configurations of these queues.

**-u ***user_name* \| **-u** *user_group* \| **-u all** 

:   

    Only displays jobs that have been submitted by the specified users.
    The keyword all specifies all users.

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

Pending jobs are displayed in the order in which they will be considered
for dispatch. Jobs in higher priority queues are displayed before those
in lower priority queues. Pending jobs in the same priority queues are
displayed in the order in which they were submitted but this order can
be changed by using the commands btop or bbot. If more than one job is
dispatched to a host, the jobs on that host are listed in the order in
which they will be considered for scheduling on this host by their queue
priorities and dispatch times. Finished jobs are displayed in the order
in which they were completed.

## Default Display

A listing of jobs is displayed with the following fields:

> JOBID

> > The job ID that Lava assigned to the job.

> USER

> > The user who submitted the job.

> STAT

> > The current status of the job (see JOB STATUS below).

> QUEUE

> > The name of the job queue to which the job belongs. If the queue to
> > which the job belongs has been removed from the configuration, the
> > queue name will be displayed as lost_and_found. Use bhist to get the
> > original queue name. The job in the lost_and_found queue will remain
> > pending until it is switched with the bswitch command into another
> > queue.

> FROM_HOST

> > The name of the host from which the job was submitted.

> EXEC_HOST

> > The name of one or more hosts on which the job is executing (this
> > field is empty if the job has not been dispatched). If the host on
> > which the job is running has been removed from the configuration,
> > the host name will be displayed as lost_and_found. Use bhist to get
> > the original host name.

> JOB_NAME

> > The job name assigned by the user, or the *command* string assigned
> > by default (see bsub (1)). If the job name is too long to fit in
> > this field, then only the latter part of the job name is displayed.

> SUBMIT_TIME

> > The submission time of the job.

## -l output

If the -l option is specified, the resulting long format listing
includes the following additional fields:

> Project

> > The project the job was submitted from.

> Command

> > The job command.

> CWD

> > The current working directory on the submission host.

> PENDING REASONS

> > The reason the job is in the PEND or PSUSP state. The names of the
> > hosts associated with each reason will be displayed when both -p and
> > -l options are specified.

> SUSPENDING REASONS

> > The reason the job is in the USUSP or SSUSP state.
>
> > loadSched
>
> > > The load scheduling thresholds for the job.
>
> > loadStop
>
> > > The load suspending thresholds for the job.

> JOB STATUS

> > Possible values for the status of a job include:
>
> > PEND
>
> > > The job is pending, that is, it has not yet been started.
>
> > PSUSP
>
> > > The job has been suspended, either by its owner or the Lava
> > > administrator, while pending.
>
> > RUN
>
> > > the job is currently running.
>
> > USUSP
>
> > > The job has been suspended, either by its owner or the Lava
> > > administrator, while running.
>
> > SSUSP
>
> > > The job has been suspended by Lava. The job has been suspended by
> > > Lava due to either of the following two causes:
> >
> > > 1\) The load conditions on the execution host or hosts have
> > > exceeded a threshold according to the loadStop vector defined for
> > > the host or queue.
> >
> > > 2\) the run window of the job\'s queue is closed. See bqueues(1),
> > > bhosts(1), and lsb.queues(5).
>
> > DONE
>
> > > The job has terminated with status of 0.
>
> > EXIT
>
> > > The job has terminated with a non-zero status - it may have been
> > > aborted due to an error in its execution, or killed by its owner
> > > or the Lava administrator.
>
> > UNKWN
>
> > > MBD has lost contact with the SBD on the host on which the job
> > > runs.
>
> > ZOMBI
>
> > > A job will become ZOMBI if:
> >
> > > \- A non-rerunnable job is killed by bkill while the SBD on the
> > > execution host is unreachable and the job is shown as UNKWN.
> >
> > > \- The host on which a rerunnable job is running is unavailable
> > > and the job has been requeued by Lava with a new job ID, as if the
> > > job were submitted as a new job.
> >
> > > After the execution host becomes available, Lava will try to kill
> > > the ZOMBI job. Upon successful termination of the ZOMBI job, the
> > > job\'s status will be changed to EXIT.

> RESOURCE USAGE

> > The values for the current usage of a job include:
>
> > CPU time
>
> > > Cumulative total CPU time in seconds of all processes in a job.
>
> > MEM
>
> > > Total resident memory usage of all processes in a job, in MB.
>
> > SWAP
>
> > > Total virtual memory usage of all processes in a job, in MB.
>
> > PGID
>
> > > Currently active process group ID in a job.
>
> > PIDs
>
> > > Currently active processes in a job.

> RESOURCE LIMITS

> > The hard resource limits that are imposed on the jobs in the queue
> > (see getrlimit(2) and lsb.queues(5)). These limits are imposed on a
> > per-job and a per-process basis.
>
> > The possible per-job limits are:
>
> > CPULIMIT
>
> > PROCLIMIT
>
> > MEMLIMIT
>
> > SWAPLIMIT
>
> > PROCESSLIMIT
>
> > The possible UNIX per-process resource limits are:
>
> > RUNLIMIT
>
> > FILELIMIT
>
> > DATALIMIT
>
> > STACKLIMIT
>
> > CORELIMIT
>
> > If a job submitted to the queue has any of these limits specified
> > (see bsub(1)), then the lower of the corresponding job limits and
> > queue limits are used for the job.
>
> > If no resource limit is specified, the resource is assumed to be
> > unlimited.

## Job Array Summary Information

If you use -A, displays summary information about job arrays. The
following fields are displayed:

> JOBID

> > Job ID of the job array.

> ARRAY_SPEC

> > Array specification in the format of *name*\[*index*\]. The array
> > specification may be truncated, use -w option together with -A to
> > show the full array specification.

> OWNER

> > Owner of the job array.

> NJOBS

> > Number of jobs in the job array.

> PEND

> > Number of pending jobs of the job array.

> RUN

> > Number of running jobs of the job array.

> DONE

> > Number of successfully completed jobs of the job array.

> EXIT

> > Number of unsuccessfully completed jobs of the job array.

> SSUSP

> > Number of Lava system suspended jobs of the job array.

> USUSP

> > Number of user suspended jobs of the job array.

> PSUSP

> > Number of held jobs of the job array.

# EXAMPLES

\% **bjobs -pl** .PP Displays detailed information about all pending
jobs of the invoker.

\% **bjobs -ps** .PP Display only pending and suspended jobs.

\% **bjobs -u all -a** .PP Displays all jobs of all users.

\% **bjobs -d -q short -m apple -u john** .PP Displays all the recently
finished jobs submitted by john to the queue short, and executed on the
host apple.

\% **bjobs 101 102 203 509** .PP Display jobs with job_ID 101, 102, 203,
and 509.

# SEE ALSO

bsub(1), bkill(1), bhosts(1), bmgroup(1), bqueues(1) bhist(1),
bresume(1), bstop(1), lsb.params(5), mbatchd(8)
