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
:   Host group that this queue dispatches jobs to. Must match a group
    defined in **llb.hosts**. Required.

**USERS**
:   Space-separated list of users allowed to submit to this queue.
    If omitted, all users are allowed.

**DESCRIPTION**
:   Human-readable description of the queue. Optional.

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
    QUEUE_NAME  = priority
    PRIORITY    = 100
    HOSTS       = group-cpu
    USERS       = alice bob
    DESCRIPTION = High priority queue for selected users
    End Queue

# SEE ALSO

**bqueues**(1), **bsub**(1), **llb.hosts**(5), **mbd**(8)
