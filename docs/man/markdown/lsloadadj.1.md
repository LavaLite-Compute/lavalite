\

# NAME

**lsloadadj** - adjusts load indices on hosts

# SYNOPSIS

**lsloadadj** \[**-R ***res_req*\] \[*host_name***\[:***num_task***\]**
\...\]

**lsloadadj** \[**-h** \| **-V**\]

# DESCRIPTION

Adjusts load indices on hosts. This is useful if a task placement
decision is made outside LIM by another application.

By default, assumes tasks are CPU-intensive and memory-intensive. This
means the CPU and memory load indices are adjusted to a higher number
than other load indices.

By default, adjusts load indices on the local host, the host from which
the command was submitted.

By default, starts 1 task.

Upon receiving a load adjustment request, LIM temporarily increases the
load on hosts according to resource requirements. This helps LIM avoid
sending too many jobs to the same host in quick succession. The inflated
load decays over time before the real load produced by the dispatched
task is reflected in LIM\'s load information.

lsloadadj adjusts all indices with the exception of ls (login sessions),
it (idle time), r15m (15-minute run queue length) and external load
indices.

# OPTIONS

**-R*** res_req*

:   

    Specify resource requirements for tasks. Only the resource usage
    section of the resource requirement string is considered (see
    lsfintro(1)). This is used by LIM to determine by how much
    individual load indices are to be adjusted.

> For example, if a task is swap-space-intensive, load adjustment on the
> swp load index is higher; other indices are increased only slightly.

*host_name***\[:***num_task***\]** \... 

:   

    Specify a list of hosts for which load is to be adjusted. *num_task*
    indicates the number of tasks to be started on the host.

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

    Prints LSF release version to stderr and exits.

# EXAMPLES

\
% **lsloadadj -R \"rusage\[swp=20:mem=10\]\"**

Adjusts the load indices swp and mem on the host from which the command
was submitted.

# SEE ALSO

lsinfo(1), lsplace(1), lsload(1), ls_loadadj(3)

# DIAGNOSTICS

Returns -1 if a bad parameter is specified; otherwise returns 0.
