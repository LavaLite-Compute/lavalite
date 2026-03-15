\

# NAME

**brequeue** - Kills and requeues a job

# SYNOPSIS

**brequeue \[-J** *job_name* \| **-J
\"***job_name***\[***index_list***\]\"\]** \[ **-u** *user_name* \|
**-u** **all** \]\[ *job_ID* \| *
***\"***job_ID***\[***index_list***\]\"\]\[-d\]\[-e\]\[-r\]\[-a\]\[-H\]**
.PP **brequeue** \[**-h** \| **-V**\]

# DESCRIPTION

You can only use brequeue on a job you own, unless you are root or an
Lava administrator.

Kills a running (RUN), user-suspended (USUSP), or system-suspended
(SSUSP) job and returns it to its original queue, with the same job ID.
A job that is killed and requeued retains its submit time but is
dispatched according to its requeue time. When the job is requeued, it
is assigned the PEND status or PSUSP if the -H option is used. Once
dispatched, the job starts over from the beginning.

Use brequeue to requeue job arrays or elements of them.

By default, kills and requeues your most recently submitted job when no
job_ID is specified.

# OPTIONS

**-J** *job_name* \| **-J \"***job_name***\[***index_list***\]\"**

:   

    Operates on the specified job.

> Since job names are not unique, multiple job arrays may have the same
> name with a different or same set of indices.

**-u** *user_name *\|* ***-u all **

:   

    Operates on the specified user\'s jobs or all jobs.

> Only root and an Lava administrator can requeue jobs submitted by
> other users.

*job_ID *\|**\"***job_ID***\[***index_list***\]\"**

:   

    Operates on the specified job or job array elements.

> The value of 0 for *job_ID* is ignored.

**-h** 

:   

    Prints command usage to stderr and exits.

```{=html}
<!-- -->
```

**-V **

:   

    Prints Lava release version to stdout and exits.

```{=html}
<!-- -->
```

**-d **

:   

    Requeues jobs that have finished running with DONE job status.

```{=html}
<!-- -->
```

**-e **

:   

    Requeues jobs that have terminated abnormally with EXIT job status.

```{=html}
<!-- -->
```

**-r **

:   

    Requeues jobs that are running.

```{=html}
<!-- -->
```

**-a **

:   

    Requeues all jobs including running jobs, suspending jobs, and jobs
    with EXIT or DONE status.

```{=html}
<!-- -->
```

**-H **

:   

    Requeues jobs to PSUSP job status.

# LIMITATIONS

brequeue cannot be used on interactive batch jobs; brequeue only kills
interactive batch jobs, it does not restart them.
