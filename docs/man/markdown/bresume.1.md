\

# NAME

**bresume** - resumes one or more suspended jobs

# SYNOPSIS

**bresume ** \[**-J ***job_name*\] \[**-m ***host_name *\]** **\[**-q
***queue_name*\] \[**-u ***user_name* \| **-u ***user_group* \| **-u all
**\] \[**0**\]

**bresume ** \[*job_ID* \| **\"***job_ID***\[***index_list***\]\"**\]
\...

**bresume **\[**-h** **\| -V**\]

# DESCRIPTION

Sends the SIGCONT signal to resume one or more of your suspended jobs.
Only root and Lava administrators can operate on jobs submitted by other
users. Using bresume on a job that is not in either the PSUSP or the
USUSP state has no effect.

By default, resumes only one job, your most recently submitted job.

You can also use bkill -s to send the resume signal to a job.

You cannot resume a job that is not suspended.

# OPTIONS

**0**

:   

    Resumes all the jobs that satisfy other options (-m, -q, -u and -J).

```{=html}
<!-- -->
```

**-J** *job_name* 

:   

    Resumes only jobs with the specified name.

```{=html}
<!-- -->
```

**-m** *host_name*

:   

    Resumes only jobs dispatched to the specified host.

```{=html}
<!-- -->
```

**-q** *queue_name* 

:   

    Resumes only jobs in the specified queue.

```{=html}
<!-- -->
```

**-u ***user_name* \| **-u ***user_group* \| **-u all** 

:   

    Resumes only jobs owned by the specified user or group, or all users
    is the reserved user name all is specified.

```{=html}
<!-- -->
```

*job_ID* \...* *\|* ***\"***job_ID***\[***index_list***\]\"** \... 

:   

    Resumes only the specified jobs. Jobs submitted by any user can be
    specified here without using the -u option.

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

\% **bresume -q night 0**\
Resumes all of the user\'s suspended jobs that are in the night queue.
If the user is an Lava administrator, resumes all suspended jobs in the
night queue.

# SEE ALSO

bsub(1), bjobs(1), bqueues(1), bhosts(1), bstop(1), bkill(1),
bparams(5), mbatchd(8), kill(1), signal(2)
