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

**-q**, **--queue** *queue*
:   Submit the job to the specified queue.
    If omitted, the default queue is used.

**-J**, **--name** *name*
:   Assign a name to the job. The name need not be unique.

**-P**, **--project** *name*
:   Associate the job with a project name. Used for accounting.

**-C**, **--comment** *text*
:   Arbitrary user-defined string, ignored by the scheduler.

## Resources

**-n**, **--cpus** *n*
:   Request *n* CPU slots per host. Default is 1.

**-N**, **--nhosts** *n*
:   Request *n* distinct execution hosts. If combined with **--cpus**,
    the total CPU count is **--cpus** multiplied by **--nhosts**.
    Default is 1.

**-M**, **--mem** *size*
:   Request *size* memory per host. Accepts a plain integer (MB) or a
    suffix: K, M, G. Example: **--mem 4G**.
    The limit is enforced via cgroup memory controller on the execution host.

**-s**, **--storage** *size*
:   Request *size* local scratch storage per host. Accepts a plain
    integer (MB) or a suffix: K, M, G. Example: **--storage 100G**.

**-g**, **--gpus** *n*
:   Request *n* GPUs per host. Default is 0. With **--nhosts** above 1
    the job gets no **CUDA_VISIBLE_DEVICES**: run it with **--exclusive**
    and all the GPUs of each host, see **llb.hosts**(5).

**-G**, **--gpu-model** *name*
:   Require a specific GPU model, as defined in llb.hosts. Example:
    **--gpu-model a100**. If omitted, any GPU model satisfies the request.
    Requires **--gpus**.

**-x**, **--exclusive**
:   Run the job in exclusive mode. The host is not shared with other
    jobs for the duration of this job.

## Cluster resources

**-T**, **--tokens** *name*=*N*
:   Request *N* tokens from the named token pool. This option may be
    repeated to request tokens from multiple pools.

## Placement

**-m**, **--machines** "*host* [*host* ...]"
:   Restrict execution to the listed hosts or host groups.

## I/O

**-o**, **--stdout** *path*
:   Write standard output to *path*. The string **%J** in the path
    is replaced by the job ID. Default is **stdout.**_jobid_ in the
    working directory.

**-e**, **--stderr** *path*
:   Write standard error to *path*. The string **%J** in the path
    is replaced by the job ID. Default is **stderr.**_jobid_ in the
    working directory.

**-i**, **--stdin** *path*
:   Read standard input from *path*. Default is /dev/null.

## Scheduling

**-H**, **--hold**
:   Hold the job in HELD state after submission. The job will not be
    scheduled until released with **bkill --signal CONT**.

**-a**, **--array** *start*-*end*[:*stride*]
:   Submit an array job: *end* - *start* + 1 (or fewer, with *stride*)
    independent jobs from a single command. *start* must be 1 or
    greater; array elements are numbered from 1. *stride* defaults
    to 1. Each element is dispatched, scheduled, and accounted as an
    ordinary job. The array as a whole is identified by the first
    element's job ID; see **bjobs**(1) for how elements are
    displayed and **bkill**(1) for how to signal an array or a
    single element.

**-b**, **--begin** [*day*:]*hour*:*minute*
:   Do not dispatch the job before the specified time.
    Fields are, from right: minute, hour, day.

**-t**, **--terminate** [*day*:]*hour*:*minute*
:   Terminate the job at the specified deadline. The job receives SIGUSR2
    and is killed if it does not exit within one minute.

The times of **-b** and **-t** are read in the local time zone of the
**bsub** process: the host's time zone, or **TZ** if set. **bsub**
converts them at submission to an absolute time, seconds since the
epoch, and that is what **mbd** and **sbd** use; their time zone does
not change it. A time already past means its next occurrence: tomorrow
for *hour*:*minute*, next month when a *day* is given.

**-W**, **--time** [*hours*:]*minutes*
:   Run limit, counted from the moment the job starts executing; time
    spent pending or being dispatched does not count. When the limit is
    reached mbd sends SIGTERM, then SIGKILL one minute later. A job that
    is redispatched keeps counting from its first start.

**-w**, **--dependency** *expr*
:   Hold the job until *expr* evaluates true. *expr* is built from the
    terms **done(**_id_**)**, **exit(**_id_**)**, and **ended(**_id_**)**,
    where *id* is a job ID. **ended(**_id_**)** is equivalent to
    **done(**_id_**) || exit(**_id_**)**. Terms may be combined with
    **&&**, **||**, **!**, and parentheses; **&&** binds tighter than
    **||**. The referenced job ID must exist at submit time.
    The expression is consumed when the job is first dispatched: if
    the job later returns to pending (for example after a reject by
    sbd), it is not evaluated again. This holds for **!** too.

## Informational

**-h**, **--help**
:   Print usage to stdout and exit.

**-V**, **--version**
:   Print version to stdout and exit.

# OUTPUT

On successful submission, prints the job ID to stdout, with the queue
name when **--queue** was given:

    Job <42> is submitted to queue <normal>.
    Job <43> is submitted to the default queue.

# EXAMPLES

Submit a simple job:

    bsub sleep 60

Submit an MPI job with 32 ranks across 4 hosts:

    bsub --cpus 32 --nhosts 4 --name myjob --stdout myjob.%J.out mpirun ./sim

Submit a GPU job requiring 2 A100s per host:

    bsub --gpus 2 --gpu-model a100 --mem 32G ./train.sh

Hold a job on submit, release later:

    bsub --hold ./setup.sh
    bkill --signal CONT 43

Submit an array of 10 jobs:

    bsub --array 1-10 ./process_chunk.sh

Submit a job that waits on two prior jobs to finish successfully:

    bsub --dependency "done(101) && done(102)" ./merge_results.sh

Request tokens from a license pool:

    bsub --tokens hspice=4 ./sim.sh

# SEE ALSO

**bjobs**(1), **bkill**(1), **bqueues**(1), **bhosts**(1),
**bgroup**(1), **btokens**(1), **bhist**(1), **mbd**(8), **sbd**(8),
**llb.hosts**(5), **llb.queues**(5)
