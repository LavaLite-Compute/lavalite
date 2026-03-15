\

# NAME

**lsmon** - displays load information for Lava hosts and periodically
updates the display

# SYNOPSIS

**lsmon** \[**-N*** *\| **-E**\] \[**-n** *num_hosts*\] \[**-R**
*res_req*\] \[**-I** *index_list*\] \[**-i** *interval*\] \[**-L**
*file_name*\] \[*host_name* \...\]

**lsmon** \[**-h** \| **-V**\]

# DESCRIPTION

lsmon is a full-screen Lava monitoring utility that displays and updates
load information for hosts in a cluster.

By default, displays load information for all hosts in the cluster, up
to the number of lines that will fit on-screen.

By default, displays raw load indices.

By default, load information is sorted according to CPU and paging load.

By default, load information is updated every 10 seconds.

# OPTIONS

**-N**

:   

    Displays normalized CPU run queue length load indices (see
    lsfintro(1)).

```{=html}
<!-- -->
```

**-E**

:   

    Displays effective CPU run queue length load indices (see
    lsfintro(1)). Options -N and -E are mutually exclusive.

```{=html}
<!-- -->
```

**-n** *num_hosts*

:   

    Displays only load information for the requested number of hosts.
    Information for up to *num_hosts* hosts that best satisfy resource
    requirements is displayed.

```{=html}
<!-- -->
```

**-R*** res_req* 

:   

    Displays only load information for hosts that satisfy the specified
    resource requirements. See lsinfo(1) for a list of built-in resource
    names.

> Load information for the hosts is sorted according to load on the
> specified resources.

> If *res_req* contains special resource names, only load information
> for hosts that provide these resources is displayed (see lshosts(1) to
> find out what resources are available on each host).

> If one or more host names are specified, only load information for the
> hosts that satisfy the resource requirements is displayed.

**-I** *index_list*

:   

    Displays only load information for the specified load indices. Load
    index names must be separated by a colon (for example, r1m:pg:ut).

> If the index list *index_list* is too long to fit in the screen of the
> user who invoked the command, the output is truncated. For example, if
> the invoker\'s screen is 80 characters wide, then up to 10 load
> indices are displayed.

**-i** *interval*

:   

    Sets how often load information is updated on-screen, in seconds.

```{=html}
<!-- -->
```

**-L** *file_name*

:   

    Saves load information in the specified file while it is displayed
    on- screen.

> If you do not want load information to be displayed on your screen at
> the same time, use **lsmon -L** *file_name* **\< /dev/null**. The
> format of the file is described in lim.acct(5).

*host_name* \...

:   

    Displays only load information for the specified hosts.

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

# USAGE

You can use the following commands while lsmon is running:

\[**\^L** \|** i** \| **n** \| **N** \| **E** \| **R** \| **q**\]

**\^L**

:   

    Refreshes the screen.

```{=html}
<!-- -->
```

**i**

:   

    Prompts you to input a new update interval.

```{=html}
<!-- -->
```

**n**

:   

    Prompts you to input a new number of hosts to display.

```{=html}
<!-- -->
```

**N**

:   

    Toggles between displaying raw CPU run queue length load indices and
    normalized CPU run queue length load indices.

```{=html}
<!-- -->
```

**E**

:   

    Toggles between displaying raw CPU run queue length load indices and
    effective CPU run queue length load indices.

```{=html}
<!-- -->
```

**R**

:   

    Prompts you to input new resource requirements.

```{=html}
<!-- -->
```

**q**

:   

    Quits lsmon.

# OUTPUT

The following fields are displayed by default.

> HOST_NAME

> > Name of specified hosts for which load information is displayed, or
> > if resource requirements were specified, name of hosts that
> > satisfied the specified resource requirement and for which load
> > information is displayed.

> status

> > Status of the host. A minus sign (-) may precede the status,
> > indicating that the Remote Execution Server (RES) on the host is not
> > running.
>
> > Possible statuses are:
>
> > ok
>
> > > The host is in normal load sharing state and can accept remote
> > > jobs.
>
> > busy
>
> > > The host is overloaded because some load indices exceed configured
> > > thresholds. Load index values that caused the host to be busy are
> > > preceded by an asterisk (\*). Built-in load indices include r15s,
> > > r1m, r15m, ut, pg, io, ls, it, swp, mem and tmp (see below).
> > > External load indices are configured in the file
> > > lsf.cluster.*cluster_name* (see lsf.cluster(5)).
>
> > lockW
>
> > > The host is locked by its run window. Run windows for a host are
> > > specified in the configuration file (see lsf.conf(5)) and can be
> > > displayed by lshosts. A locked host will not accept load shared
> > > jobs from other hosts.
>
> > lockU
>
> > > The host is locked by the Lava administrator or root.
>
> > unavail
>
> > > The host is down or the Load Information Manager (LIM) on the host
> > > is not running.

> r15s

> > The 15-second exponentially averaged CPU run queue length.

> r1m

> > The 1-minute exponentially averaged CPU run queue length.

> r15m

> > The 15-minute exponentially averaged CPU run queue length.

> ut

> > The CPU utilization exponentially averaged over the last minute,
> > between 0 and 1.

> pg

> > The memory paging rate exponentially averaged over the last minute,
> > in pages per second.

> ls

> > The number of current login users.

> it

> > On UNIX, the idle time of the host (keyboard not touched on all
> > logged in sessions), in minutes.

> tmp

> > The amount of free space in /tmp, in megabytes.

> swp

> > The amount of currently available swap space, in megabytes.

> mem

> > The amount of currently available memory, in megabytes.

# SEE ALSO

lsfintro(1), lshosts(1), lsinfo(1), lsload(1), lslockhost(8),
lim.acct(5), ls_load(3)

# DIAGNOSTICS

Specifying an invalid resource requirement string while lsmon is running
(via the R option) causes lsmon to exit with an appropriate error
message.

lsmon exits if it does not receive a reply from LIM within the update
interval.
