\

# NAME

**lsplace** - displays hosts available to execute tasks

# SYNOPSIS

**lsplace** \[**-L**\] \[**-n ***minimum* \| **-n 0**\] \[**-R
***res_req*\] \[**-w ***maximum* \| **-w 0**\] \[*host_name \...*\]

**lsplace** \[**-h** \| **-V**\]

# DESCRIPTION

Displays hosts available for the execution of tasks, and temporarily
increases the load on these hosts (to avoid sending too many jobs to the
same host in quick succession). The inflated load will decay slowly over
time before the real load produced by the dispatched task is reflected
in the LIM\'s load information. Host names may be duplicated for
multiprocessor hosts, to indicate that multiple tasks can be placed on a
single host.

By default, displays only one host name.

By default, uses Lava default resource requirements.

# OPTIONS

**-L** 

:   

    Attempts to place tasks on as few hosts as possible. This is useful
    for distributed parallel applications in order to minimize
    communication costs between tasks.

```{=html}
<!-- -->
```

**-n ***minimum* \| **-n 0** 

:   

    Displays at least the specified number of hosts. Specify 0 to
    display as many hosts as possible.

> Prints Not enough host(s) currently eligible and exits with status 1
> if the required number of hosts holding the required resources cannot
> be found.

**-R*** res_req* 

:   

    Displays only hosts with the specified resource requirements.

```{=html}
<!-- -->
```

**-w*** maximum* \| **-w 0** 

:   

    Displays no more than the specified number of hosts. Specify 0 to
    display as many hosts as possible.

```{=html}
<!-- -->
```

*host_name* \...

:   

    Displays only hosts that are among the specified hosts.

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

# EXAMPLES

lsplace is mostly used in backquotes to pick out a host name which is
then passed to other commands.

The -w and -n options can be combined to specify the upper and lower
bounds in processors to be returned, respectively. For example, the
command lsplace -n 3 -w 5 returns at least 3 and not more than 5 host
names.

# SEE ALSO

lsinfo(1), ls_placereq(3), lsload(1)

# DIAGNOSTICS

lsplace returns 1 if insufficient hosts are available. The exit status
is -10 if a problem is detected in Lava, -1 for other errors, otherwise
0.
