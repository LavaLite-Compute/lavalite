\

# NAME

**bstop** - suspends unfinished jobs

# SYNOPSIS

**bstop** \[**-a**\] \[**-d**\] \[**-R**\] \[**0**\] \[**-J**
*job_name*\] \[**-m** *host_name* *\|* **-m*** host_group*\] \[**-q**
*queue_name*\] \[**-u** *user_name* \| **-u** *user_group* \| **-u
all**\] \[*job_ID *\| **\"***job_ID***\[***index***\]\"**\] *\...* .PP
**bstop **\[**-h** **\| -V**\]

# DESCRIPTION

Suspends unfinished jobs.

Sends the SIGSTOP signal to sequential jobs and the SIGTSTP signal to
parallel jobs to suspend them.

By default, suspends only one job, your most recently submitted job.

Only root and Lava administrators can operate on jobs submitted by other
users.

Using bstop on a job that is in the USUSP state has no effect.

You can also use bkill -s to send the suspend signal to a job, or send a
resume signal to a job. You can use bresume to resume suspended jobs.
You cannot suspend a job that is already suspended.

If a signal request fails to reach the job execution host, Lava will
retry the operation later when the host becomes reachable. Lava retries
the most recent signal request.

# OPTIONS

**-a** 

:   

    Suspends all jobs.

```{=html}
<!-- -->
```

**-d** 

:   

    Suspends only finished jobs (with a DONE or EXIT status).

```{=html}
<!-- -->
```

**0**

:   

    Suspends all the jobs that satisfy other options (-m, -q, -u and
    -J).

```{=html}
<!-- -->
```

**-J** *job_name* 

:   

    Suspends only jobs with the specified name.

```{=html}
<!-- -->
```

**-m** *host_name* \| **-m** *host_group*

:   

    Suspends only jobs dispatched to the specified host or host group.

```{=html}
<!-- -->
```

**-q** *queue_name*

:   

    Suspends only jobs in the specified queue.

```{=html}
<!-- -->
```

**-u ***user_name* \| **-u ***user_group* \| **-u all**

:   

    Suspends only jobs owned by the specified user or user group, or all
    users if the keyword all is specified.

```{=html}
<!-- -->
```

*job_ID* \...* *\| **\"***job_ID***\[***index***\]\"** \... 

:   

    Suspends only the specified jobs. Jobs submitted by any user can be
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

\% **bstop 314**\
Suspends job number 314.

\% **bstop -m apple**\
Suspends the invoker\'s last job that was dispatched to host apple.

\% **bstop -u smith 0**\
Suspends all the jobs submitted by user smith.

\% **bstop -u all**\
Suspends the last submitted job in the Lava system.

\% **bstop -u all 0**\
Suspends all the batch jobs in the Lava system.

# SEE ALSO

bsub(1), bjobs(1), bqueues(1), bhosts(1), bresume(1), bkill(1),
bparams(5), mbatchd(8), kill(1), signal(2)
