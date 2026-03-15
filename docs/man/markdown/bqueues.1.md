\

# NAME

**bqueues** - displays information about queues

# SYNOPSIS

**bqueues **\[**-w** \| **-l**\] \[**-m** *host_name *\|* ***-m***
host_group \| ***-m all**\] \[**-u ***user_name *\|* ***-u*** user_group
\| ***-u all**\] \[*queue_name* \...\]

**bqueues **\[**-h** \| **-V**\]

# DESCRIPTION

Displays information about queues.

By default, returns the following information about all queues: queue
name, queue priority, queue status, job slot statistics, and job state
statistics.

Batch queue names and characteristics are set up by the Lava
administrator (see lsb.queues(5) and mbatchd(8)).

# OPTIONS

**-w** 

:   

    Displays queue information in a wide format. Fields are displayed
    without truncation.

```{=html}
<!-- -->
```

**-l **

:   

    Displays queue information in a long multi-line format. The -l
    option displays the following additional information: queue
    description, queue characteristics and statistics, scheduling
    parameters, resource limits, scheduling policies, users, hosts, user
    shares, windows, associated commands, and job controls.

```{=html}
<!-- -->
```

**-m** *host_name *\|* ***-m*** host_group *\| **-m*** ***all** 

:   

    Displays the queues that can run jobs on the specified host or host
    group. If the keyword all is specified, displays the queues that can
    run jobs on all hosts . For a list of host groups see bmgroup(1).

```{=html}
<!-- -->
```

**-u** *user_name *\|* ***-u*** user_group*** **\|** -u all** 

:   

    Displays the queues that can accept jobs from the specified user or
    user group (For a list of user groups see bugroup(1).) If the
    keyword \`all\' is specified, displays the queues that can accept
    jobs from all users.

```{=html}
<!-- -->
```

*queue_name *\... 

:   

    Displays information about the specified queues.

```{=html}
<!-- -->
```

**-h** 

:   

    Prints command usage to stderr and exits.

```{=html}
<!-- -->
```

**-V **

:   

    Prints Lava release version to stdout and exits.

# OUTPUT

## Default Output

Displays the following fields:

> QUEUE_NAME

> > The name of the queue. Queues are named to correspond to the type of
> > jobs usually submitted to them, or to the type of services they
> > provide.
>
> > lost_and_found
>
> > > If the Lava administrator removes queues from the system, Lava
> > > creates a queue called lost_and_found and places the jobs from the
> > > removed queues into the lost_and_found queue. Jobs in the
> > > lost_and_found queue will not be started unless they are switched
> > > to other queues (see bswitch).

> PARAMETER/STATISTICS

> > PRIO
>
> > > The priority of the queue. If job priority is not configured,
> > > determines the queue search order at job dispatch, suspension and
> > > resumption time. Queues with higher priority values are searched
> > > first for job dispatch and resumption (this is contrary to UNIX
> > > process priority ordering), and queues with higher priority values
> > > are searched last for job suspension.
>
> > STATUS
>
> > > The current status of the queue. The possible values are:
> >
> > > Open
> >
> > > > The queue is able to accept jobs.
> >
> > > Closed
> >
> > > > The queue is not able to accept jobs.
> >
> > > Active
> >
> > > > Jobs in the queue may be started.
> >
> > > Inactive
> >
> > > > Jobs in the queue cannot be started for the time being.
> >
> > > At any moment, each queue is either Open or Closed, and is either
> > > Active or Inactive. The queue can be opened, closed, inactivated
> > > and re-activated by the Lava administrator using badmin (see
> > > badmin(8)). The queue becomes inactive when either its dispatch
> > > window is closed or its run window is closed (see DISPATCH_WINDOWS
> > > in the \"Output for the -l Option\" section). In this case, the
> > > queue cannot be activated using badmin. The queue is re-activated
> > > by Lava when one of its dispatch windows and one of its run
> > > windows are open again. The initial state of a queue at Lava boot
> > > time is set to open, and either active or inactive depending on
> > > its windows.

> MAX

> > The maximum number of job slots that can be used by the jobs from
> > the queue. These job slots are used by dispatched jobs which have
> > not yet finished, and by pending jobs which have slots reserved for
> > them. A sequential job will use one job slot when it is dispatched
> > to a host, while a parallel job will use as many job slots as is
> > required by bsub -n when it is dispatched. See bsub(1) for details.
> > If \`-\' is displayed, there is no limit.

> JL/U

> > The maximum number of job slots you can use for your jobs in the
> > queue. These job slots are used by your dispatched jobs which have
> > not yet finished, and by pending jobs which have slots reserved for
> > them. If \`-\' is displayed, there is no limit.

> JL/P

> > The maximum number of job slots a processor can process from the
> > queue. This includes job slots of dispatched jobs that have not yet
> > finished, and job slots reserved for some pending jobs. The job slot
> > limit per processor (JL/P) controls the number of jobs sent to each
> > host. This limit is configured per processor so that multiprocessor
> > hosts are automatically allowed to run more jobs. If \`-\' is
> > displayed, there is no limit.

> JL/H

> > The maximum number of job slots a host can process from the queue.
> > This includes the job slots of dispatched jobs that have not yet
> > finished, and those reserved for some pending jobs. The job slot
> > limit per host (JL/H) controls the number of jobs sent to each host,
> > regardless of whether a host is a uniprocessor host or a
> > multiprocessor host. If \`-\' is displayed, there is no limit.

> NJOBS

> > The total number of job slots held currently by jobs in the queue.
> > This includes pending, running, suspended and reserved job slots. A
> > parallel job that is running on *n* processors is counted as *n* job
> > slots, since it takes *n* job slots in the queue. See bjobs(1) for
> > an explanation of batch job states.

> PEND

> > The number of pending job slots in the queue.

> RUN

> > The number of running job slots in the queue.

> SUSP

> > The number of suspended job slots in the queue.

## Output for **-**l Option

In addition to the above fields, the **-**l option displays the
following:

> Description

> > A description of the typical use of the queue.

> PARAMETERS/STATISTICS

> > NICE
>
> > > The nice value at which jobs in the queue will be run. This is the
> > > UNIX nice value for reducing the process priority (see nice(1)).
>
> > STATUS
>
> > > Inactive
> >
> > > > The long format for the **-**l option gives the possible reasons
> > > > for a queue to be inactive:
> >
> > > Inact_Win
> >
> > > > The queue is out of its dispatch window or its run window.
> >
> > > Inact_Adm
> >
> > > > The queue has been inactivated by the Lava administrator.
> >
> > > SSUSP
> >
> > > > The number of job slots in the queue allocated to jobs that are
> > > > suspended by Lava.
> >
> > > USUSP
> >
> > > > The number of job slots in the queue allocated to jobs that are
> > > > suspended by the job submitter or by the Lava administrator.
> >
> > > RSV
> >
> > > > The numbers of job slots in the queue that are reserved by Lava
> > > > for pending jobs.
> >
> > > Migration threshold
> >
> > > > The length of time in seconds that a job dispatched from the
> > > > queue will remain suspended by the system before Lava attempts
> > > > to migrate the job to another host. See the MIG parameter in
> > > > lsb.queues and lsb.hosts.
> >
> > > Schedule delay for a new job
> >
> > > > The delay time in seconds for scheduling a session after a new
> > > > job is submitted. If the schedule delay time is zero, a new
> > > > scheduling session is started as soon as the job is submitted to
> > > > the queue. See the NEW_JOB_SCHEDULE_DELAY parameter in
> > > > lsb.queues.
> >
> > > Interval for a host to accept two jobs
> >
> > > > The length of time in seconds to wait after dispatching a job to
> > > > a host before dispatching a second job to the same host. If the
> > > > job accept interval is zero, a host may accept more than one job
> > > > in each dispatching interval. See the JOB_ACCEPT_INTERVAL
> > > > parameter in lsb.queues and lsb.params.
> >
> > > RESOURCE LIMITS
> >
> > > > The hard resource limits that are imposed on the jobs in the
> > > > queue (see getrlimit(2) and lsb.queues(5)). These limits are
> > > > imposed on a per-job and a per-process basis.
> > >
> > > > The possible per-job limits are:
> > >
> > > > CPULIMIT
> > >
> > > > PROCLIMIT
> > >
> > > > MEMLIMIT
> > >
> > > > SWAPLIMIT
> > >
> > > > The possible UNIX per-process resource limits are:
> > >
> > > > RUNLIMIT
> > >
> > > > FILELIMIT
> > >
> > > > DATALIMIT
> > >
> > > > STACKLIMIT
> > >
> > > > CORELIMIT
> > >
> > > > If a job submitted to the queue has any of these limits
> > > > specified (see bsub(1)), then the lower of the corresponding job
> > > > limits and queue limits are used for the job.
> > >
> > > > If no resource limit is specified, the resource is assumed to be
> > > > unlimited.
> >
> > > SCHEDULING PARAMETERS
> >
> > > > The scheduling and suspending thresholds for the queue.
> > >
> > > > The scheduling threshold loadSched and the suspending threshold
> > > > loadStop are used to control batch job dispatch, suspension, and
> > > > resumption. The queue thresholds are used in combination with
> > > > the thresholds defined for hosts (see bhosts(1) and
> > > > lsb.hosts(5)). If both queue level and host level thresholds are
> > > > configured, the most restrictive thresholds are applied.
> > >
> > > > The loadSched and loadStop thresholds have the following fields:
> > >
> > > > r15s
> > >
> > > > > The 15-second exponentially averaged effective CPU run queue
> > > > > length.
> > >
> > > > r1m
> > >
> > > > > The 1-minute exponentially averaged effective CPU run queue
> > > > > length.
> > >
> > > > r15m
> > >
> > > > > The 15-minute exponentially averaged effective CPU run queue
> > > > > length.
> > >
> > > > ut
> > >
> > > > > The CPU utilization exponentially averaged over the last
> > > > > minute, expressed as a percentage between 0 and 1.
> > >
> > > > pg
> > >
> > > > > The memory paging rate exponentially averaged over the last
> > > > > minute, in pages per second.
> > >
> > > > io
> > >
> > > > > The disk I/O rate exponentially averaged over the last minute,
> > > > > in kilobytes per second.
> > >
> > > > ls
> > >
> > > > > The number of current login users.
> > >
> > > > it
> > >
> > > > > On UNIX, the idle time of the host (keyboard not touched on
> > > > > all logged in sessions), in minutes.
> > >
> > > > tmp
> > >
> > > > > The amount of free space in /tmp, in megabytes.
> > >
> > > > swp
> > >
> > > > > The amount of currently available swap space, in megabytes.
> > >
> > > > mem
> > >
> > > > > The amount of currently available memory, in megabytes.
> > >
> > > > In addition to these internal indices, external indices are also
> > > > displayed if they are defined in lsb.queues (see lsb.queues(5)).
> > >
> > > > The loadSched threshold values specify the job dispatching
> > > > thresholds for the corresponding load indices. If \`-\' is
> > > > displayed as the value, it means the threshold is not
> > > > applicable. Jobs in the queue may be dispatched to a host if the
> > > > values of all the load indices of the host are within (below or
> > > > above, depending on the meaning of the load index) the
> > > > corresponding thresholds of the queue and the host. The same
> > > > conditions are used to resume jobs dispatched from the queue
> > > > that have been suspended on this host.
> > >
> > > > Similarly, the loadStop threshold values specify the thresholds
> > > > for job suspension. If any of the load index values on a host go
> > > > beyond the corresponding threshold of the queue, jobs in the
> > > > queue will be suspended.
>
> > SCHEDULING POLICIES
>
> > > Scheduling policies of the queue. Optionally, one or more of the
> > > following policies may be configured:
> >
> > > IGNORE_DEADLINE
> >
> > > > If IGNORE_DEADLINE is set to Y, starts all jobs regardless of
> > > > the run limit.
> >
> > > EXCLUSIVE
> >
> > > > Jobs dispatched from an exclusive queue can run exclusively on a
> > > > host if the user so specifies at job submission time (see
> > > > bsub(1)). Exclusive execution means that the job is sent to a
> > > > host with no other batch job running there, and no further job,
> > > > batch or interactive, will be dispatched to that host while the
> > > > job is running. The default is not to allow exclusive jobs.
> >
> > > NO_INTERACTIVE
> >
> > > > This queue does not accept batch interactive jobs. (see the -I,
> > > > -Is, and -Ip options of bsub(1)). The default is to accept both
> > > > interactive and non-interactive jobs.
> >
> > > ONLY_INTERACTIVE
> >
> > > > This queue only accepts batch interactive jobs. Jobs must be
> > > > submitted using the -I, -Is, and -Ip options of bsub(1). The
> > > > default is to accept both interactive and non-interactive jobs.

> DEFAULT HOST SPECIFICATION

> > The default host or host model that will be used to normalize the
> > CPU time limit of all jobs.
>
> > If you want to view a list of the CPU factors defined for the hosts
> > in your cluster, use lsinfo(1). The CPU factors are configured in
> > lsf.shared(5).
>
> > The appropriate CPU scaling factor of the host or host model is used
> > to adjust the actual CPU time limit at the execution host (see
> > CPULIMIT in lsb.queues(5)). The DEFAULT_HOST_SPEC parameter in
> > lsb.queues overrides the system DEFAULT_HOST_SPEC parameter in
> > lsb.params (see lsb.params(5)). If a user explicitly gives a host
> > specification when submitting a job using bsub -c
> > *cpu_limit*\[/*host_name* \| /*host_model*\], the user specification
> > overrides the values defined in both lsb.params and lsb.queues.

> RUN_WINDOWS

> > One or more run windows in a week during which jobs in the queue may
> > run.
>
> > When the end of a run window is reached, any running jobs from the
> > queue are suspended until the beginning of the next run window when
> > they are resumed. The default is no restriction, or always open.

> DISPATCH_WINDOWS

> > The dispatch windows for the queue. The dispatch windows are the
> > time windows in a week during which jobs in the queue may be
> > dispatched.
>
> > When a queue is out of its dispatch window or windows, no job in the
> > queue will be dispatched. Jobs already dispatched are not affected
> > by the dispatch windows. The default is no restriction, or always
> > open (that is, twenty-four hours a day, seven days a week). Note
> > that such windows are only applicable to batch jobs. Interactive
> > jobs scheduled by LIM are controlled by another set of dispatch
> > windows (see lshosts(1)). Similar dispatch windows may be configured
> > for individual hosts (see bhosts(1)).
>
> > A window is displayed in the format *begin_time*-*end_time*. Time is
> > specified in the format \[*day*:\]*hour*\[:*minute*\], where all
> > fields are numbers in their respective legal ranges: 0(Sunday)-6 for
> > *day*, 0-23 for *hour*, and 0-59 for *minute*. The default value for
> > *minute* is 0 (on the hour). The default value for *day* is every
> > day of the week. The *begin_time* and *end_time* of a window are
> > separated by \`-\', with no blank characters (SPACE and TAB) in
> > between. Both *begin_time* and *end_time* must be present for a
> > window. Windows are separated by blank characters.

> USERS

> > A list of users and user groups allowed to submit jobs to the queue.
> > User group names have a slash (/) added at the end of the group
> > name. See bugroup(1).
>
> > Lava cluster administrators can submit jobs to the queue by default

> HOSTS

> > A list of hosts and host groups where jobs in the queue can be
> > dispatched. Host group names have a slash (/) added at the end of
> > the group name. See bmgroup(1).

> ADMINISTRATORS

> > A list of queue administrators. The users whose names are listed are
> > allowed to operate on the jobs in the queue and on the queue itself.
> > See lsb.queues(5) for more information.

> PRE_EXEC

> > The queue\'s pre-execution command. The pre-execution command is
> > executed before each job in the queue is run on the execution host
> > (or on the first host selected for a parallel batch job). See
> > lsb.queues(5) for more information.

> POST_EXEC

> > The queue\'s post-execution command. The post-execution command is
> > run when a job terminates. See lsb.queues(5) for more information.

> REQUEUE_EXIT_VALUES

> > Jobs that exit with these values are automatically requeued. See
> > lsb.queues(5) for more information.

> RES_REQ

> > Resource requirements of the queue. Only the hosts that satisfy
> > these resource requirements can be used by the queue.

> Maximum slot reservation time

> > The maximum time in seconds a slot is reserved for a pending job in
> > the queue. See the SLOT_RESERVE=MAX_RESERVE_TIME\[n\] parameter in
> > lsb.queues.

> RESUME_COND

> > Resume threshold conditions for a suspended job in the queue. See
> > lsb.queues(5) for more information.

> STOP_COND

> > Stop threshold conditions for a running job in the queue. See
> > lsb.queues(5) for more information.

> JOB_STARTER

> > Job starter command for a running job in the queue. See
> > lsb.queues(5) for more information.

> RERUNNABLE

> > If the RERUNNABLE field displays yes, jobs in the queue are
> > rerunnable. That is, jobs in the queue are automatically restarted
> > or rerun if the execution host becomes unavailable. However, a job
> > in the queue will not be restarted if the you have removed the
> > rerunnable option from the job. See lsb.queues(5) for more
> > information.

> CHECKPOINT

> > If the CHKPNTDIR field is displayed, jobs in the queue are
> > checkpointable. Jobs will use the default checkpoint directory and
> > period unless you specify other values. Note that a job in the queue
> > will not be checkpointed if you have removed the checkpoint option
> > from the job. See lsb.queues(5) for more information.
>
> > CHKPNTDIR
>
> > > Specifies the checkpoint directory using an absolute or relative
> > > path name.
>
> > CHKPNTPERIOD
>
> > > Specifies the checkpoint period in seconds.
> >
> > > Although the output of bqueues reports the checkpoint period in
> > > seconds, the checkpoint period is defined in minutes (the
> > > checkpoint period is defined through the bsub -k \"*checkpoint_dir
> > > *\[*checkpoint_period*\]\" option, or in lsb.queues).

> JOB CONTROLS

> > The configured actions for job control. See JOB_CONTROLS parameter
> > in lsb.queues.
>
> > The configured actions are displayed in the format \[*action_type*,
> > *command*\] where *action_type* is either SUSPEND, RESUME, or
> > TERMINATE.

# SEE ALSO

lsfbatch(1), bugroup(1), nice(1), getrlimit(2), lsb.queues(5), bsub(1),
bjobs(1), bhosts(1), badmin(8), mbatchd(8)
