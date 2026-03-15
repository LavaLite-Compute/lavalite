\

# NAME

**lsb.acct** - Batch job log file of Lava

# DESCRIPTION

The master batch daemon (see mbatchd(8)) generates a record for each job
completion or failure. The record is appended to the job log file
lsb.acct. The file is located in LSB_SHAREDIR/*\<clustername\>*/logdir,
where LSB_SHAREDIR must be defined in lsf.conf(5) and *\<clustername\>*
is the name of the Lava cluster, as returned by lsid(1). See mbatchd(8)
for the description of LSB_SHAREDIR. The job log file is an ASCII file
with one record per line. The fields of a record are separated by
blanks. If the value of some field is unavailable, \"\" is logged for
character string, 0 for time and number, and -1 for resource usage.

# Fields

Fields of each job record are ordered in the following sequence:

Event type (%s)

:   

    Which is always \"**JOB_FINISH**\"

```{=html}
<!-- -->
```

Version Number (%s)

:   

    Version number of the log file format

```{=html}
<!-- -->
```

Event Time (%d)

:   

    Time the event was logged (in seconds since the epoch)

```{=html}
<!-- -->
```

jobId (%d)

:   

    ID for the job

```{=html}
<!-- -->
```

userId (%d)

:   

    UNIX user ID of the submitter

```{=html}
<!-- -->
```

options (%d)

:   

    Bit flags for job processing

```{=html}
<!-- -->
```

numProcessors (%d)

:   

    Number of processors initially requested for execution

```{=html}
<!-- -->
```

submitTime (%d)

:   

    Job submission time

```{=html}
<!-- -->
```

beginTime (%d)

:   

    Job start time - the job should be started at or after this time

```{=html}
<!-- -->
```

termTime (%d)

:   

    Job termination deadline - the job should be terminated by this time

```{=html}
<!-- -->
```

startTime (%d) - 

:   

    Job dispatch time - time job was dispatched for execution

```{=html}
<!-- -->
```

userName (%s) 

:   

    User name of the submitter

```{=html}
<!-- -->
```

queue (%s)

:   

    Name of the job queue to which the job was submitted

```{=html}
<!-- -->
```

resReq (%s)

:   

    Resource requirement specified by the user

```{=html}
<!-- -->
```

dependCond (%s)

:   

    Job dependency condition specified by the user

```{=html}
<!-- -->
```

preExecCmd (%s)

:   

    Pre-execution command specified by the user

```{=html}
<!-- -->
```

fromHost (%s)

:   

    Submission host name

```{=html}
<!-- -->
```

cwd (%s)

:   

    Current working directory

```{=html}
<!-- -->
```

inFile (%s)

:   

    Input file name (%s)

```{=html}
<!-- -->
```

outFile (%s)

:   

    output file name

```{=html}
<!-- -->
```

errFile (%s)

:   

    Error output file name

```{=html}
<!-- -->
```

jobFile (%s)

:   

    Job script file name

```{=html}
<!-- -->
```

numAskedHosts (%d)

:   

    Number of host names to which job dispatching will be limited

```{=html}
<!-- -->
```

askedHosts (%s)

:   

    List of host names to which job dispatching will be limited (%s for
    each); blank if the last field value is 0. If there is more than one
    host name, then each additional host name will be returned in its
    own field

```{=html}
<!-- -->
```

numExHosts (%d)

:   

    Number of processors used for execution** **

```{=html}
<!-- -->
```

execHosts (%s)

:   

    List of execution host names (%s for each); blank if the last field
    value is 0

```{=html}
<!-- -->
```

jStatus (%d)

:   

    Job status. The number 32 represents **EXIT**, 64 represents
    **DONE**

```{=html}
<!-- -->
```

hostFactor (%f)

:   

    CPU factor of the first execution host

```{=html}
<!-- -->
```

jobName (%s)

:   

    Job name

```{=html}
<!-- -->
```

command (%s)

:   

    Complete batch job command specified by the user

```{=html}
<!-- -->
```

lsfRusage

:   

    The following fields contain resource usage information for the job.
    If the value of some field is unavailable (due to job abortion or
    the difference among the operating systems), -1 will be logged.
    Times are measured in seconds, and sizes are measured in KBytes.

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

mailUser (%s)

:   

    Name of the user to whom job related mail was sent

```{=html}
<!-- -->
```

projectName (%d)

:   

    Project name

```{=html}
<!-- -->
```

exitStatus (%d)

:   

    UNIX exit status of the job

```{=html}
<!-- -->
```

maxNumProcessors (%d)

:   

    Maximum number of processors specified for the job

```{=html}
<!-- -->
```

loginShell (%s)

:   

    Login shell used for the job

```{=html}
<!-- -->
```

timeEvent (%s)

:   

    Time event string for the job - JobScheduler only

```{=html}
<!-- -->
```

idx (%d)

:   

    Job array index

```{=html}
<!-- -->
```

maxRMem (%d)

:   

    Maximum resident memory usage in KBytes of all processes in the job

```{=html}
<!-- -->
```

maxRSwap (%d)

:   

    Maximum virtual memory usage in KBytes of all processes in the job

```{=html}
<!-- -->
```

inFileSpool (%s)

:   

    Spool input file

```{=html}
<!-- -->
```

commandSpool (%s)

:   

    Spool command file

# SEE ALSO

## Related Topics

lsb.events(5), lsb.params(5), lsf.conf(5), mbatchd(8), bsub(1), lsid(1)

## Files

\$LSB_SHAREDIR/*\<clustername\>*/logdir/lsb.acct
