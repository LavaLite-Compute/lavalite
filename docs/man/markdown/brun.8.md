\

# NAME

**brun** - forces a job to run immediately

# SYNOPSIS

**brun** \[**-f**\]** -m \"***host_name \...***\"** *job_ID* .PP
**brun** \[**-f**\]** -m \"***host_name \...***\"**
**\"***job_ID***\[***index_list***\]\" ** .PP **brun **\[**-h** \|
**-V**\]

# DESCRIPTION

This command can only be used by Lava administrators.

Forces a pending job to run immediately on specified hosts.

A job which has been forced to run is counted as a running job, this may
violate the user, queue, or host job limits.

A job which has been forced to run cannot be preempted by other jobs
even if it is submitted to a preemptable queue and other jobs are
submitted to a preemptive queue.

By default, after the job is started, it is still subject to run windows
and suspending conditions.

# OPTIONS

**-f**

:   

    Allows the job to run without being suspended due to run windows or
    suspending conditions.

```{=html}
<!-- -->
```

**-m ***host_name *\... 

:   

    Required. Specify one or more hosts on which to run the job.

```{=html}
<!-- -->
```

*job_ID *\|* ***\"***job_ID***\[***index_list***\]\"**

:   

    Required. Specify the job to run, or specify one element of a job
    array.

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

# LIMITATIONS

You cannot force a job in SSUSP, USUSP or PSUSP state.
