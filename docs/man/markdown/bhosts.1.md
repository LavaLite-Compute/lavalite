\

# NAME

**bhosts** - displays hosts and their static and dynamic resources

# SYNOPSIS

**bhosts **\[**-w \| -l**\] \[**-R \"***res_req***\"**\] \[*host_name \|
host_group*\] \...

**bhosts -s** \[*shared_resource_name* \...\]

**bhosts **\[**-h** \| **-V**\]

# DESCRIPTION

Displays information about hosts.

By default, returns the following information about all hosts: host
name, host status, job slot limits, and job state statistics.

The -s option displays information about the numeric shared resources
and their associated hosts.

# OPTIONS

**-w**

:   

    Displays host information in wide format. Fields are displayed
    without truncation.

```{=html}
<!-- -->
```

**-l**

:   

    Displays host information in a (long) multi-line format. In addition
    to the default fields, displays information about the CPU factor,
    the dispatch windows, the current load, and the load thresholds.

```{=html}
<!-- -->
```

**-R \"***res_req***\"**

:   

    Only displays information about hosts that satisfy the resource
    requirement expression. For more information about resource
    requirements, see lsfintro(1). The size of the resource requirement
    string is limited to 512 bytes.

> Lava supports ordering of resource requirements on all load indices,
> including external load indices, either static or dynamic.

*host_name* \... \| *host_group* \...

:   

    Only displays information about the specified hosts or host groups.
    For host groups, the names of the hosts belonging to the group are
    displayed instead of the name of the host group. Do not use quotes
    when specifying multiple hosts or host groups.

```{=html}
<!-- -->
```

**-s** \[*shared_resource_name *\...\]

:   

    Displays information about the specified shared resources. The
    resources must have numeric values. Returns the following
    information: the resource names, the total and reserved amounts, and
    the resource locations. If no shared resources are specified,
    displays information about all numeric shared resources.

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

## Host-Based Default

Displays the following fields:

> HOST_NAME

> > The name of the host. If a host has batch jobs running and the host
> > is removed from the configuration, the host name will be displayed
> > as lost_and_found.

> STATUS

> > The current status of the host. Batch jobs can only be dispatched to
> > hosts with an ok status. The possible values for host status are as
> > follows:
>
> > ok
>
> > > The host is available to accept batch jobs.
>
> > unavail
>
> > > The host is down, or LIM and SBD on the host are unreachable.
>
> > unreach
>
> > > LIM on the host is running but SBD is unreachable.
>
> > closed
>
> > > The host is not allowed to accept any remote batch jobs. There are
> > > several reasons for the host to be closed (see Host-Based -l
> > > Options).

> JL/U

> > The maximum number of job slots that the host can process on a per
> > user basis.
>
> > These job slots are used by running jobs, as well as by suspended or
> > pending jobs that have slots reserved for them.

> MAX

> > The maximum number of job slots that the host can process. These job
> > slots are used by running and suspended jobs on the host, and by
> > pending jobs that have jobs slots reserved for them on the host.

> NJOBS

> > The number of job slots used by started jobs on the host (including
> > running, suspended).

> RUN

> > The number of job slots used by running jobs on the host.

> SSUSP

> > The number of job slots used by system suspended jobs on the host.

> USUSP

> > The number of job slots used by user suspended jobs on the host.
> > Jobs can be suspended by the user or by the Lava administrator.

> RSV

> > The number of job slots used by pending jobs that have jobs slots
> > reserved on the host.

## Host-Based **-**l Option

In addition to the above fields, the -l option also displays the
following:

> STATUS

> > closed
>
> > > The long format shown by the -l option gives the possible reasons
> > > for a host to be closed:
> >
> > > closed_Adm
> >
> > > > The host is closed by the Lava administrator or root (see
> > > > badmin(8)). No job can be dispatched to the host, but jobs that
> > > > are executing on the host will not be affected.
> >
> > > closed_Lock
> >
> > > > The host is locked by the Lava administrator or root (see
> > > > lsadmin(8)). All batch jobs on the host are suspended by Lava.
> >
> > > closed_Wind
> >
> > > > The host is closed by its dispatch windows, which are defined in
> > > > the configuration file lsb.hosts(5). All batch jobs on the host
> > > > are suspended by the Lava system.
> >
> > > closed_Full
> >
> > > > The configured maximum number of batch job slots on the host has
> > > > been reached (see MAX field below).
> >
> > > closed_Excl
> >
> > > > The host is currently running an exclusive job.
> >
> > > closed_Busy
> >
> > > > The host is overloaded, because some load indices go beyond the
> > > > configured thresholds (see lsb.hosts(5)). The displayed
> > > > thresholds that cause the host to be busy are preceded by an
> > > > asterisk (\*).
> >
> > > closed_LIM
> >
> > > > LIM on the host is unreachable, but SBD is ok.

> CPUF

> > Displays the CPU normalization factor of the host (see lshosts(1)).

> DISPATCH_WINDOWS

> > Displays the dispatch windows for each host. The dispatch windows
> > are the time windows during the week when batch jobs can be run on
> > each host. Jobs already started are not affected by the dispatch
> > windows. The default for the dispatch window is no restriction or
> > always open (that is, twenty-four hours a day and seven days a
> > week). For the dispatch window specification, see the description
> > for the DISPATCH_WINDOWS keyword under the -l option in bqueues(1).

> CURRENT LOAD

> > Displays the total and reserved host load.
>
> > Reserved
>
> > > You specify reserved resources by using bsub -R (see lsfintro(1)).
> > > These resources are reserved by jobs running on the host.
>
> > Total
>
> > > The total load has different meanings depending on whether the
> > > load index is increasing or decreasing.
> >
> > > For increasing load indices, such as run queue lengths, CPU
> > > utilization, paging activity, logins, and disk I/O, the total load
> > > is the consumed plus the reserved amount. The total load is
> > > calculated as the sum of the current load and the reserved load.
> > > The current load is the load seen by lsload(1).
> >
> > > For decreasing load indices, such as available memory, idle time,
> > > available swap space, and available space in tmp, the total load
> > > is the available amount. The total load is the difference between
> > > the current load and the reserved load. This difference is the
> > > available resource as seen by lsload(1).

> LOAD THRESHOLD

> > Displays the scheduling threshold loadSched and the suspending
> > threshold loadStop. Also displays the migration threshold if defined
> > and the checkpoint support if the host supports checkpointing.
>
> > The format for the thresholds is the same as for batch job queues
> > (see bqueues(1)) and lsb.queues(5)). For an explanation of the
> > thresholds** **and load indices, see the description for the \"QUEUE
> > SCHEDULING PARAMETERS\" keyword under the -l option in bqueues(1).

## Resource-Based **-**s Option

The **-**s option displays the following: the amounts used for
scheduling, the amounts reserved, and the associated hosts for the
shared resources. Only shared resources with numeric values are
displayed. See lim(8), and lsf.cluster(5) on how to configure shared
resources.

The following fields are displayed:

> RESOURCE

> > The name of the resource.

> TOTAL

> > The value of the shared resource used for scheduling. This is the
> > sum of the current and the reserved load for the shared resource.

> RESERVED

> > The amount reserved by jobs. You specify the reserved resource using
> > bsub -R (see lsfintro(1)).

> LOCATION

> > The hosts that are associated with the shared resource.

# SEE ALSO

lsb.hosts(5), bqueues(1), lsfintro(1), lshosts(1), badmin(8), lsadmin(8)
