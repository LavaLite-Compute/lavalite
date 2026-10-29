---
title: LLB.QUEUES
section: 5
header: LavaLite Configuration Files
footer: LavaLite
date: 2026
---

# NAME

llb.queues - LavaLite queue configuration

# DESCRIPTION

**llb.queues** defines the batch queues available in the cluster.
Each queue is defined in a **Begin Queue** / **End Queue** block.
**mbd** reads this file at startup.

# FORMAT

Each queue is defined as:

    Begin Queue
    PARAMETER = VALUE
    ...
    End Queue

Lines starting with **#** are comments. Blank lines are ignored.
Multiple queues may be defined in the same file.

# PARAMETERS

**QUEUE_NAME**
:   The logical name of the queue. Used with **bsub --queue** and
    displayed by **bqueues**. Required.

**PRIORITY**
:   Integer scheduling priority. When multiple jobs are pending across
    queues, jobs from higher-priority queues are dispatched first.
    Required.

**HOSTS**
:   Hosts this queue dispatches jobs to: space-separated host names,
    host groups defined in **llb.hosts**, or **all** for every host in
    **llb.hosts**. Required. A name not in **llb.hosts** is skipped
    with a log line. A queue without **HOSTS**, or left with no valid
    host, is kept and schedules nothing.

**USERS**
:   Space-separated list of users allowed to submit to this queue.
    If omitted or **all**, all users are allowed.

**MAX_JOBS**
:   Maximum number of running and suspended jobs in the queue. If
    omitted, unlimited. **0** dispatches nothing: the queue accepts
    jobs and keeps them pending. Optional.

**DESCRIPTION**
:   Human-readable description of the queue. Optional.

# ERRORS

An error in a queue block never stops **mbd**. It is logged with the
queue name and what **mbd** did instead:

- a bad **MAX_JOBS** or **PRIORITY** value: the default, as if omitted;
- an unknown key or a line without **=**: the line is ignored;
- a missing **HOSTS**: the queue is kept with no host;
- a block without **QUEUE_NAME**, or a name defined twice: the block
  is skipped.

Jobs of a queue that schedules nothing stay pending, with their
pending reason. Check the **mbd** log after every restart.

# EXAMPLE

    Begin Queue
    QUEUE_NAME  = normal
    PRIORITY    = 30
    HOSTS       = group-cpu
    DESCRIPTION = General purpose CPU workloads
    End Queue

    Begin Queue
    QUEUE_NAME  = gpu
    PRIORITY    = 50
    HOSTS       = group-gpu
    DESCRIPTION = GPU workloads
    End Queue

    Begin Queue
    QUEUE_NAME  = normal
    PRIORITY    = 10
    HOSTS       = all
    DESCRIPTION = Every host in llb.hosts
    End Queue

    Begin Queue
    QUEUE_NAME  = priority
    PRIORITY    = 100
    HOSTS       = group-cpu
    USERS       = alice bob
    DESCRIPTION = High priority queue for selected users
    End Queue

# SEE ALSO

**bqueues**(1), **bsub**(1), **llb.hosts**(5), **mbd**(8)
