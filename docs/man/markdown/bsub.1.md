---
title: BSUB
section: 1
header: LavaLite User Commands
footer: LavaLite
date: 2026
---

# NAME

bsub - submit a batch job to LavaLite

# SYNOPSIS

**bsub** [*options*] *command* [*arguments*]

**bsub** [**--help** | **--version**]

# DESCRIPTION

Submits a job for batch execution and assigns it a unique numeric job ID.
The job runs on a host that satisfies all requirements when conditions on
the job, host, and queue are met.

Jobs are dispatched in FCFS order by default.

A subset of the submission environment is passed to the job. Full
environment inheritance is not guaranteed; variables specific to the
desktop session or display server are not propagated.

# OPTIONS

## Job identity

**--queue** *queue*
:   Submit the job to the specified queue.
    If omitted, the default queue is used.

**--name** *name*
:   Assign a name to the job. The name need not be unique.

**--comment** *text*
:   Arbitrary user-defined string, ignored by the scheduler.

## Resources

**--cpus** *n*
:   Request *n* CPU slots (MPI ranks). Default is 1.

**--nhosts** *n*
:   Request *n* distinct execution hosts. If combined with **--cpus**,
    the number of CPUs must be a multiple of the number of hosts.
    Default is 1.

**--mem** *size*
:   Request *size* memory per host. Accepts a plain integer (MB) or a
    suffix: K, M, G. Example: **--mem 4G**.
    The limit is enforced via cgroup memory controller on the execution host.

**--gpus** *n*
:   Request *n* GPUs per host. Default is 0.

**--gpu-type** *name*
:   Require a specific GPU type, as defined in llb.hosts. Example:
    **--gpu-type a100**. If omitted, any GPU type satisfies the request.
    Requires **--gpus**.

**--exclusive**
:   Run the job in exclusive mode. The host is not shared with other
    jobs for the duration of this job.

## Placement

**--machines** "*host* [*host* ...]"
:   Restrict execution to the listed hosts or host groups.

## I/O

**--stdout** *path*
:   Write standard output to *path*. The string **%J** in the path
    is replaced by the job ID. Default is **stdout.**_jobid_ in the
    working directory.

**--stderr** *path*
:   Write standard error to *path*. The string **%J** in the path
    is replaced by the job ID. Default is **stderr.**_jobid_ in the
    working directory.

**--stdin** *path*
:   Read standard input from *path*. Default is /dev/null.

## Scheduling

**--hold**
:   Hold the job in PSUSP state after submission. The job will not be
    scheduled until released with **bkill --signal CONT**.

**--begin** [*day*:]*hour*:*minute*
:   Do not dispatch the job before the specified time.
    Fields are, from right: minute, hour, day.

**--terminate** [*day*:]*hour*:*minute*
:   Terminate the job at the specified deadline. The job receives SIGUSR2
    and is killed if it does not exit within ten minutes.

**--wall** [*hours*:]*minutes*
:   Set the wall clock run limit. The job receives SIGUSR2 when the limit
    is reached and is killed if it does not exit within ten minutes.

**--dependency** '*expression*'
:   Hold the job until the dependency expression evaluates to true.
    Supported conditions:

    **done(**_job_id_**)** - job completed successfully.

    **exit(**_job_id_**)** - job exited with non-zero status.

    **ended(**_job_id_**)** - job finished, regardless of exit status.

    Conditions can be combined with **&&**, **||**, and **!**.
    Use parentheses to control precedence.

## Informational

**--help**
:   Print usage to stderr and exit.

**--version**
:   Print version to stderr and exit.

# OUTPUT

On successful submission, prints the job ID and queue name to stdout:

    Job <42> is submitted to queue <normal>.

# EXAMPLES

Submit a simple job:

    bsub sleep 60

Submit an MPI job with 32 ranks across 4 hosts:

    bsub --cpus 32 --nhosts 4 --name myjob --stdout myjob.%J.out mpirun ./sim

Submit a GPU job requiring 2 A100s per host:

    bsub --gpus 2 --gpu-type a100 --mem 32G ./train.sh

Submit with a dependency:

    bsub --dependency 'done(41)' ./postprocess

Hold a job on submit, release later:

    bsub --hold ./setup.sh
    bkill --signal CONT 43

# SEE ALSO

**bjobs**(1), **bkill**(1), **bqueues**(1), **bhosts**(1),
**bmgroup**(1), **bswitch**(1), **mbd**(8), **sbd**(8), **lsb.hosts**(5),
**lsb.queues**(5)
