\

# NAME

**lshosts** - displays hosts and their static resource information

# SYNOPSIS

**lshosts** \[**-w \| -l**\] \[**-R \"***res_req***\"**\]
\[*host_name*\] \...

**lshosts** **-s **\[*shared_resource_name* \...\]

**lshosts** \[**-h** \| **-V**\]

# DESCRIPTION

Displays static resource information about hosts.

By default, returns the following information: host name, host type,
host model, CPU factor, number of CPUs, total memory, total swap space,
whether or not the host is a server host, and static resources. Displays
information about all hosts in thecluster . See lsf.cluster(5).

The -s option displays information about the static shared resources and
their associated hosts.

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

    Displays host information in a long multi-line format. In addition
    to the default fields, displays information about the maximum /tmp
    space, the number of local disks, the execution priority for remote
    jobs, load thresholds, and run windows.

```{=html}
<!-- -->
```

**-R** **\"***res_req***\"** 

:   

    Only displays information about the hosts that satisfy the resource
    requirement expression. For more information about resource
    requirements, see lsfintro(1). The size of the resource requirement
    string is limited to 512 bytes.

> Lava supports ordering of resource requirements on all load indices,
> including external load indices, either static or dynamic.

*host_name*\...\...

:   

    Only displays information about the specified hosts. Do not use
    quotes when specifying multiple hosts.

```{=html}
<!-- -->
```

**-s*** *\[*shared_resource_name *\...\]

:   

    Displays information about the specified resources. The resources
    must be static shared resources. Returns the following information:
    the resource names, the values of the resources, and the resource
    locations. If no shared resource is specified, then displays
    information about all shared resources.

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

    Prints the Lava release version to stderr and exits.

# OUTPUT

## Host-Based Default

> Displays the following fields:

> HOST_NAME

> > The name of the host. The host name is truncated if too long.

> type

> > The host type. The host type is truncated if too long.

> model

> > The host model. The host model is truncated if too long.

> cpuf

> > The CPU factor. The CPU factor is used to scale the CPU load value
> > so that differences in CPU speeds are considered. The faster the
> > CPU, the larger the CPU factor.
>
> > The CPU factor of a host with an unknown host type is 1.0.

> ncpus

> > The number of CPUs.

> maxmem

> > The total memory.

> maxswp

> > The total swap space.

> server

> > \"Yes\" if the host is a server host.

> RESOURCES

> > The available Boolean resources denoted by resource names, and the
> > values of external numeric and string static resources. See
> > lsf.cluster(5), and lsf.shared(5) on how to configure external
> > static resources.

## Host Based **-**l Option

In addition to the above fields, the -l option also displays the
following:

> ndisks

> > The number of local disks.

> maxtmp

> > The maximum /tmp space in megabytes configured on a host.

> rexpri

> > The remote execution priority.

> RUN_WINDOWS

> > The time periods during which the host is open for sharing loads
> > from other hosts. (See lsf.cluster(5).)

> LOAD_THRESHOLDS

> > The thresholds for scheduling interactive jobs. If a load threshold
> > is exceeded, the host status is changed to \"busy.\" See lsload(1).

## Resource-Based **-**s Option

Displays the static shared resources. Each line gives the value and the
associated hosts for the static shared resource. See lsf.shared(5), and
lsf.cluster(5) on how to configure static shared resources.

The following fields are displayed:

> RESOURCE

> > The name of the resource.

> VALUE

> > The value of the static shared resource.

> LOCATION

> > The hosts that are associated with the static shared resource.

# SEE ALSO

lsfintro(1), ls_info(3), ls_policy(3), ls_gethostinfo(3),
lsf.cluster(5), lsf.shared(5)
