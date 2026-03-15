\

# NAME

**bswitch** - switches unfinished jobs from one queue to another

# SYNOPSIS

**bswitch **\[**-J ***job_name*\] \[**-m ***host_name \| ***-m***
host_group*\]** ** \[**-q ***queue_name*\] \[**-u ***user_name* \| **-u
***user_group* \| **-u all**\] *destination_queue* \[**0**\]

**bswitch ***destination_queue* \[*job_ID* \|
**\"***job_ID***\[***index_list***\]\"**\] \...

**bswitch **\[**-h** \| **-V**\]

# DESCRIPTION

Switches one or more of your unfinished jobs to the specified queue.
Lava administrators and root can switch jobs submitted by other users.

By default, switches one job, the most recently submitted job, or the
most recently submitted job that also satisfies other specified options
(-m, -q, -u and -J). Specify -0 (zero) to switch multiple jobs.

The switch operation can be done only if a specified job is acceptable
to the new queue as if it were submitted to it, and, in case the job has
been dispatched to a host, if the host can be used by the new queue. If
the switch operation is unsuccessful, the job stays where it is.

If a switched job has not been dispatched, then its behavior will be as
if it were submitted to the new queue in the first place.

If a switched job has been dispatched, then it will be controlled by the
loadSched and loadStop vectors, PRIORITY, RUN_WINDOW and other
configuration parameters of the new queue, but its nice value and
resource limits will remain the same except the RUNLIMIT which is reset
to the value of the new queue.

This command is useful to change a job\'s attributes inherited from the
queue.

# OPTIONS

**0**

:   

    (Zero). Switches multiple jobs. Switches all the jobs that satisfy
    other specified options (-m, -q, -u and -J).

```{=html}
<!-- -->
```

**-J** *job_name*

:   

    Only switches jobs that have the specified job name.

```{=html}
<!-- -->
```

**-m** *host_name* \| **-m** *host_group*

:   

    Only switches jobs dispatched to the specified host or host group.

```{=html}
<!-- -->
```

**-q** *queue_name* 

:   

    Only switches jobs in the specified queue.

```{=html}
<!-- -->
```

**-u ***user_name* \| **-u ***user_group* \| **-u all **

:   

    Only switches jobs submitted by the specified user or group, or all
    users if you specify the keyword all.

```{=html}
<!-- -->
```

*destination_queue* 

:   

    Required. Specify the queue to which the job is to be moved.

```{=html}
<!-- -->
```

*job_ID* \... \|**\"***job_ID***\[***index_list***\]\"** \...

:   

    Switches only the specified jobs.

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

# SEE ALSO

bqueues(1), bhosts(1), bugroup(1), bsub(1), bjobs(1)

# LIMITATIONS
