\

# NAME

**bmig** - migrates checkpointable or rerunnable jobs

# SYNOPSIS

**bmig **\[**-f**\] \[*job_ID*** \|
\"***job_ID***\[***index_list***\]\"**\] \...

**bmig **\[**-f**\] \[**-J** *job_name*\] \[**-m** **\"***host_name*
\...**\" \| -m** **\"***host_group *\...**\"**\] \[**-u ***user_name***
\| -u*** user_group*** \| -u all**\] \[**0**\]

**bmig **\[**-h** \| **-V**\]

# DESCRIPTION

Migrates one or more of your checkpointable and rerunnable jobs. Lava
administrators and root can migrate jobs submitted by other users.

By default, migrates one job, the most recently submitted job, or the
most recently submitted job that also satisfies other specified options
(-u and -J). Specify 0 (zero) to migrate multiple jobs.

To migrate a job, both hosts must be binary compatible, run the same OS
version, have access to the executable, have access to all open files
(Lava must locate them with an absolute path name), and have access to
the checkpoint directory.

Only started jobs can be migrated (i.e., running or suspended jobs);
pending jobs cannot be migrated.

When a checkpointable job is migrated, Lava checkpoints and kills the
job (similar to the -k option of bchkpnt(1)) then restarts it on the
next available host. If checkpoint is not successful, the job is not
killed and remains on the host. If a job is being checkpointed when bmig
is issued, the migration is ignored. This situation may occur if
periodic checkpointing is enabled.

When a rerunnable job is migrated, Lava kills the job (similar to
bkill(1)) then restarts it from the beggining on the next available
host.

The environment variable LSB_RESTART is set to Y when a migrating job is
restarted or rerun.

A job is made rerunnable by specifying the -r option on the command line
using bsub(1) and bmod(1), or automatically by configuring RERUNNABLE in
lsb.queues(5).

A job is made checkpointable by specifying the location of a checkpoint
directory on the command line using the -k option of bsub(1) and
bmod(1), or automatically by configuring CHKPNT in lsb.queues(5).

# OPTIONS

**-f**

:   

    Forces a checkpointable job to be checkpointed even if
    non-checkpointable conditions exist (these conditions are
    OS-specific).

```{=html}
<!-- -->
```

*job_ID* \| **\"***job_ID***\[***index_list***\]\"** \| **0**

:   

    Specifies the job ID of the jobs to be migrated. The -J and -u
    options are ignored.

> If you specify a job ID of **0** (zero), all other job IDs are
> ignored, and all jobs that satisfy the -J and -u options are migrated.

> If you do not specify a job ID, the most recently submitted job that
> satisfies the -J and -u options is migrated.

**-J** *job_name*

:   

    Specifies the job name of the job to be migrated. Ignored if a job
    ID other than 0 (zero) is specified.

```{=html}
<!-- -->
```

**-m** \"*host_name* \...**\"** \| **-m** **\"***host_group* \...**\"**

:   

    Only migrates jobs dispatched to the specified hosts.

```{=html}
<!-- -->
```

**-u** **\"***user_name***\"** \| **-u** **\"***user_group***\"** \| **-u all**

:   

    Specifies that only jobs submitted by these users are to be
    migrated.

> If the reserved user name all is specified, jobs submitted by all
> users are to be migrated. Ignored if a job ID other than 0 (zero) is
> specified.

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

bsub(1), brestart(1), bchkpnt(1), bjobs(1), bqueues(1), bhosts(1),
bugroup(1), mbatchd(8), lsb.queues(5), kill(1)
