\

# NAME

**bpeek** - displays the stdout and stderr output of an unfinished job

# SYNOPSIS

**bpeek **\[**-f**\] \[**-q ***queue_name* \| **-m ***host_name* \| **-J
***job_name* \| *job_ID* \| **\"***job_ID***\[***index_list***\]\"**\]**
** .PP **bpeek **\[**-h** \| **-V**\]

# DESCRIPTION

Displays the standard output and standard error output that have been
produced by one of your unfinished jobs, up to the time that this
command is invoked.

By default, displays the output using the command cat.

This command is useful for monitoring the progress of a job and
identifying errors. If errors are observed, valuable user time and
system resources can be saved by terminating an erroneous job.

# OPTIONS

**-f**

:   

    Displays the output of the job using the command tail -f.

```{=html}
<!-- -->
```

**-q ***queue_name* 

:   

    Operates on your most recently submitted job in the specified queue.

```{=html}
<!-- -->
```

**-m** *host_name*

:   

    Operates on your most recently submitted job that has been
    dispatched to the specified host.

```{=html}
<!-- -->
```

**-J** *job_name*

:   

    Operates on your most recently submitted job that has the specified
    job name.

```{=html}
<!-- -->
```

*job_ID* \| **\"***job_ID***\[***index_list***\]\" **

:   

    Operates on the specified job.

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

tail(1), bsub(1), bjobs(1), bhist(1), bhosts(1), bqueues(1)
