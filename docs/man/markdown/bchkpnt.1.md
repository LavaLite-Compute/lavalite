\

# NAME

**bchkpnt** - checkpoints one or more checkpointable jobs

# SYNOPSIS

**bchkpnt** \[**-f**\] \[**-k**\] \[**-p** *minutes *\| **-p 0**\]
\[*job_ID *\| **\"***job_ID***\[***index_list***\]\"**\] \...

**bchkpnt** \[**-f**\] \[**-k**\] \[**-p** *minutes *\| **-p 0**\]
\[**-J** *job_name*\] \[**-m** *host_name* \| **-m** *host_group*\]
\[**-q** *queue_name*\] \[**-u** **\"***user_name***\"** \| **-u all**\]
\[**0**\]

**bchkpnt** \[**-h** \| **-V**\]

# DESCRIPTION

Checkpoints your running (RUN) or suspended (SSUSP, USUSP, and PSUSP)
checkpointable jobs. Lava administrators and root can checkpoint jobs
submitted by other users.

By default, checkpoints one job, the most recently submitted job, or the
most recently submitted job that also satisfies other specified options
(-m, -q, -u and -J). Specify -0 (zero) to checkpoint multiple jobs.
Specify a job ID to checkpoint one specific job.

By default, jobs continue to execute after they have been checkpointed.

To submit a checkpointable job, use bsub -k or submit the job to a
checkpoint queue (CHKPNT in lsb.queues(5)). Use brestart(1) to start
checkpointed jobs.

Lava invokes the echkpnt(8) executable found in LSF_SERVERDIR to perform
the checkpoint.

# OPTIONS

**0**

:   

    (Zero). Checkpoints multiple jobs. Checkpoints all the jobs that
    satisfy other specified options (-m, -q, -u and -J).

```{=html}
<!-- -->
```

**-f**

:   

    Forces a job to be checkpointed even if non-checkpointable
    conditions exist (these conditions are OS-specific).

```{=html}
<!-- -->
```

**-k**

:   

    Kills a job after it has been successfully checkpointed.

```{=html}
<!-- -->
```

**-p** *minutes* \| **-p 0**

:   

    Enables periodic checkpointing and specifies the checkpoint period,
    or modifies the checkpoint period of a checkpointed job. Specify
    **-p 0** (zero) to disable periodic checkpointing.

> Checkpointing is a resource-intensive operation. To allow your job to
> make progress while still providing fault tolerance, specify a
> checkpoint period of 30 minutes or longer.

**-J** *job_name*

:   

    Only checkpoints jobs that have the specified job name.

```{=html}
<!-- -->
```

**-m** *host_name* \| **-m** *host_group*

:   

    Only checkpoints jobs dispatched to the specified hosts.

```{=html}
<!-- -->
```

**-q** *queue_name* 

:   

    Only checkpoints jobs dispatched from the specified queue.

```{=html}
<!-- -->
```

**-u** **\"***user_name***\"*** *\| **-u all**

:   

    Only checkpoints jobs submitted by the specified users. The keyword
    all specifies all users. Ignored if a job ID other than 0 (zero) is
    specified.

```{=html}
<!-- -->
```

*job_ID* \| **\"***job_ID***\[***index_list***\]\"**

:   

    Checkpoints only the specified jobs.

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

\% **bchkpnt 1234**

Checkpoints the job with job ID 1234.

\% **bchkpnt -p 120 1234**

Enables periodic checkpointing or changes the checkpoint period to 120
minutes (2 hours) for a job with job ID 1234.

\% **bchkpnt -m hostA -k -u all 0**

When issued by root or an Lava administrator, will checkpoint and kill
all checkpointable jobs on hostA. This is useful when a host needs to be
shut down or rebooted.

# SEE ALSO

bsub(1), bmod(1), brestart(1), bjobs(1), bqueues(1), bhosts(1),
libckpt.a(3), lsb.queues(5), echkpnt(8), erestart(8), mbatchd(8)
