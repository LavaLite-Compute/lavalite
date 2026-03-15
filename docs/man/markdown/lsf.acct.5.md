\

# NAME

**lsf.acct** - Lava task log file

# DESCRIPTION

The Lava Remote Execution Server, RES (see **res**(8)), generates a
record for each task completion or failure. If the RES task logging is
turned on (see lsadmin(8)), it appends the record to the task log file
lsf.acct.\<*hostname*\>.

The task log file is an ASCII file with one task record per line. The
fields of each record are separated by blanks. The location of the file
is determined by the **LSF_RES_ACCTDIR** variable defined in the
lsf.conf file (see lsf.conf(5)). If this variable is not defined, or the
RES cannot access the log directory, the log file is created in **/tmp**
instead.

# Fields

The fields in a task record are ordered in the following sequence:

pid (%d)

:   

    Process ID for the remote task

```{=html}
<!-- -->
```

userName (%s)

:   

    User name of the submitter

```{=html}
<!-- -->
```

exitStatus (%d)

:   

    Task exit status

```{=html}
<!-- -->
```

dispTime (%ld)

:   

    Dispatch time - time at which the task was dispatched for execution

```{=html}
<!-- -->
```

termTime (%ld)

:   

    Completion time - time when task is completed/failed

```{=html}
<!-- -->
```

fromHost (%s)

:   

    Submission host name

```{=html}
<!-- -->
```

execHost (%s)

:   

    Execution host name

```{=html}
<!-- -->
```

cwd (%s)

:   

    Current working directory

```{=html}
<!-- -->
```

cmdln (%s)

:   

    Command line of the task

```{=html}
<!-- -->
```

lsfRusage 

:   

    The rest of the fields contain resource usage information for the
    task (see **getrusage**(2)). If any field is not available due to
    the difference among the operating systems, -1 will be logged. Times
    are measured in seconds, and sizes are measured in KBytes.

> ru_utime (%f)

> > User time used

> ru_stime (%f)

> > System time used

> ru_maxrss (%d)

> > Maximum shared text size

> ru_ixrss (%d)

> > Integral of the shared text size over time (in kilobyte seconds)

> ru_ismrss (%d)

> > Integral of the shared memory size over time (valid only on Ultrix)

> ru_idrss (%d)

> > Integral of the unshared data size over time

> ru_isrss (%d)

> > Integral of the unshared stack size over time

> ru_minflt (%d)

> > Number of page reclaims

> ru_magflt (%d)

> > Number of page faults

> ru_nswap (%d)

> > Number of times the process was swapped out

> ru_inblock (%d)

> > Number of block input operations

> ru_oublock (%d)

> > Number of block output operations

> ru_ioch (%d)

> > Number of characters read and written (valid only on HP-UX)

> ru_msgsnd (%d)

> > Number of System V IPC messages sent

> ru_msgrcv (%d)

> > Number of messages received

> ru_nsignals (%d)

> > Number of signals received

> ru_nvcsw (%d)

> > Number of voluntary context switches

> ru_nivcsw (%d)

> > Number of involuntary context switches

> ru_exutime (%d)

> > Exact user time used (valid only on ConvexOS)

# SEE ALSO

## Related Topics:

lsadmin(8), res(8), lsf.conf(5), getrusage(2)

## Files:

\$LSF_RES_ACCTDIR/lsf.acct.\<*hostname*\>
