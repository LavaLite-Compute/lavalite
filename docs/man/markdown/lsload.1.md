\

# NAME

**lsload** - displays load information for hosts

# SYNOPSIS

**lsload** \[**-l**\] \[**-N** \| **-E**\] \[**-I
***load_index*\[**:***load_index*\] *\...*\] \[**-n ***num_hosts*\]
\[**-R ***res_req*\] *host_name \... \...* .PP **lsload** **-s
**\[*resource_name \...*\]

**lsload** \[**-h** \| **-V**\]

# DESCRIPTION

Displays load information for hosts. Load information can be displayed
on a per-host basis, or on a per-resource basis.

By default, displays load information for all hosts in the local
cluster, per host.

By default, displays raw load indices.

By default, load information for resources is displayed according to CPU
and paging load.

# OPTIONS

**-l**

:   

    Long format. Displays load information without truncation along with
    additional fields for I/O and external load indices.

> This option overrides the index names specified with the -I option.

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

**-I** *load_index*\[**:***load_index*\] \...

:   

    Displays only load information for the specified load indices. Load
    index names must be separated by a colon (for example, r1m:pg:ut).

```{=html}
<!-- -->
```

**-n*** num_hosts*

:   

    Displays only load information for the requested number of hosts.
    Information for up to *num_hosts* hosts that best satisfy the
    resource requirements is displayed.

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

> If one or more host names are specified, only load information about
> the hosts that satisfy the resource requirements is displayed.

*host_name* \... 

:   

    Displays only load information for the specified hosts.

```{=html}
<!-- -->
```

-s \[*resource_name* \...\]

:   

    Displays information for all dynamic shared resources configured in
    the cluster.

> If resources are specified, only displays information for the
> specified resources. *resource_name* must be a dynamic shared resource
> name.

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

## HOST-BASED OUTPUT (Default output)

Numeric dynamic non-shared resources are displayed. The selection and
order sections of *res_req* control for which hosts information is
displayed and how they are ordered (see lsfintro(1)).

The displayed default load information includes the following fields:

> HOST_NAME

> > Standard host name used by Lava, typically an Internet domain name
> > with two components.

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
> >
> > > r15s
> >
> > > > The 15-second exponentially averaged CPU run queue length.
> >
> > > r1m
> >
> > > > The 1-minute exponentially averaged CPU run queue length.
> >
> > > r15m
> >
> > > > The 15-minute exponentially averaged CPU run queue length.
> >
> > > ut
> >
> > > > The CPU utilization exponentially averaged over the last minute,
> > > > between 0 and 1.
> >
> > > pg
> >
> > > > The memory paging rate exponentially averaged over the last
> > > > minute, in pages per second.
> >
> > > io
> >
> > > > The disk I/O rate exponentially averaged over the last minute,
> > > > in KB per second (this is only available when the -l option is
> > > > specified).
> >
> > > ls
> >
> > > > The number of current login users.
> >
> > > it
> >
> > > > On UNIX, the idle time of the host (keyboard not touched on all
> > > > logged in sessions), in minutes.
> >
> > > swp
> >
> > > > The amount of swap space available, in megabytes.
> >
> > > mem
> >
> > > > The amount of available memory, in megabytes.
> >
> > > tmp
> >
> > > > The amount of free space in /tmp, in megabytes.
> >
> > > external_index
> >
> > > > Any site-configured global external load indices (see lim(8)).
> > > > Available only when the -l option or the -I option with the
> > > > index name is used, and only if defined in the
> > > > lsf.cluster.*cluster_name* (see lsf.cluster(5)) configuration
> > > > file. Note that *external_index* should not contain shared
> > > > resources.
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

## RESOURCE-BASED OUTPUT (lsload -s )

Displays information about dynamic shared resources. Each line gives the
value and the associated hosts for an instance of the resource. See
lim(8), and lsf.cluster(5) for information on configuring dynamic shared
resources.

The displayed information consists of the following fields:

> RESOURCE

> > Name of the resource.

> VALUE

> > Value for an instance of the resource.

> LOCATION

> > Hosts associated with the instance of the resource.

# EXAMPLES

\% **lsload -R \"select\[r1m\<=0.5 && swp\>=20 && type==ALPHA\]\"** .PP
OR, in restricted format:

\%** lsload -R r1m=0.5:swp=20:type=ALPHA** .PP Displays the load of
ALPHA hosts with at least 20 megabytes of swap space, and a 1-minute run
queue length less than 0.5.

\% **lsload -R \"select\[(1-swp/maxswp)\<0.75\] order\[pg\]\"** .PP
Displays the load of the hosts whose swap space utilization is less than
75%. The resulting hosts are ordered by paging rate.

\%** lsload -I r1m:ut:io:pg** .PP Displays the 1-minute CPU raw run
queue length, the CPU utilization, the disk I/O and paging rates for all
hosts in the cluster.

\%** lsload -E** .PP Displays the load of all hosts, ordered by
r15s:pg**,** with the CPU run queue lengths being the effective run
queue lengths (see lsfintro(1)).

\%** lsload -s verilog_license** .PP Displays the value and location of
all the verilog_license dynamic shared resource instances.

# SEE ALSO

lsfintro(1), lim(8), lsf.cluster(5), lsplace(1), lshosts(1), lsinfo(1),
lslockhost(8), ls_load(3)

# DIAGNOSTICS

Exit status is -10 if an Lava problem is detected or a bad resource name
is specified.

Exit status is -1 if a bad parameter is specified, otherwise lsload
returns 0.
