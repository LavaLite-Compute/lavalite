---
title: BQUEUES
section: 1
header: LavaLite User Commands
footer: LavaLite
date: 2026
---

# NAME

bqueues - display queue information and manage queue availability

# SYNOPSIS

**bqueues** [**-l**] [*queue*...]

**bqueues** **--close** *queue*

**bqueues** **--open** *queue*

# DESCRIPTION

Without options, displays the current status and counters for all queues
in the cluster.

The **--close** and **--open** options require administrator privileges.

# OPTIONS

*queue*...
:   Show only the named queues, in the order given. An unknown name is
    reported on stderr and the exit status is non-zero.

**-l**, **--long**
:   Display detailed information for each queue, including description,
    priority, status, job limit, users, hosts, job counts, and resource
    usage. User and host lists are wrapped at 79 columns.

**--close** *queue*
:   Close the named queue. A closed queue still accepts new jobs but
    does not dispatch them: they stay pending with reason queue closed.
    Running jobs are not affected.

**--open** *queue*
:   Open a previously closed queue, restoring normal operation.

**--help**
:   Print usage to stderr and exit.

**--version**
:   Print version to stderr and exit.

# OUTPUT

Displays a table with the following columns:

**QUEUE_NAME**
:   Queue name.

**PRIO**
:   Queue priority. Higher values are dispatched first.

**STATUS**
:   Queue status: **open** or **closed**.

**MAX**
:   Maximum number of running and suspended jobs in the queue,
    configured by **MAX_JOBS** in **llb.queues**.
    Pending and held jobs do not count toward this limit.
    When the limit is reached, pending jobs wait until a slot becomes
    available. An omitted setting means unlimited, displayed as **-**.
    **0** dispatches nothing.

**NJOBS**
:   Total jobs in the queue (pending + held + running + suspended).

**PEND**
:   Pending jobs.

**HELD**
:   Held jobs.

**RUN**
:   Running jobs.

**SUSP**
:   Suspended jobs.

## Long format (-l)

Each queue is displayed as a block with the following fields:

**QUEUE**
:   Queue name.

**Description**
:   Human-readable description, if configured.

**Priority**
:   Queue priority. Higher values are dispatched first.

**Status**
:   Queue status: **open** or **closed**.

**Max jobs**
:   Maximum number of running and suspended jobs in the queue,
    as described under **MAX**. No limit is displayed as **unlimited**.

**Users**
:   Users allowed to submit to this queue. **all** if unrestricted.

**Hosts**
:   Hosts eligible to run jobs from this queue, **HOSTS = all**
    expanded to the host names. **none** if the queue has no valid
    host: it schedules nothing.

**Jobs**
:   Running, pending, held, and suspended job counts.

**Usage**
:   CPU slots in use by jobs in the queue, and the sum of the hosts
    they use: a host used by several jobs is counted once per job.

# SEE ALSO

**bsub**(1), **bjobs**(1), **bhosts**(1), **llb.queues**(5), **mbd**(8)
