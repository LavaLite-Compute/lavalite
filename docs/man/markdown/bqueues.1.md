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

**bqueues**

**bqueues** **-l**

**bqueues** **--close** *queue*

**bqueues** **--open** *queue*

# DESCRIPTION

Without options, displays the current status and counters for all queues
in the cluster.

The **--close** and **--open** options require administrator privileges.

# OPTIONS

**-l**, **--long**
:   Display detailed information for each queue, including description,
    users, and hosts. Long lines are wrapped at 79 columns.

**--close** *queue*
:   Close the named queue. A closed queue does not accept new jobs and
    does not dispatch pending jobs. Running jobs are not affected.

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
:   Maximum number of jobs allowed in the queue simultaneously.

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

**USED_CPUS**
:   CPU slots currently in use by this queue.

**USED_HOSTS**
:   Hosts currently running jobs from this queue.

## Long format (-l)

Each queue is displayed as a block with the following fields:

**Description**
:   Human-readable description, if configured.

**Priority, Status, Max jobs**
:   Same as the tabular columns.

**Users**
:   Users allowed to submit to this queue. **all** if unrestricted.

**Hosts**
:   Hosts eligible to run jobs from this queue.

**Jobs**
:   Running, pending, held, and suspended counts.

**Resources**
:   CPU slots and hosts currently in use.

# SEE ALSO

**bsub**(1), **bjobs**(1), **bhosts**(1), **ll.queues**(5), **mbd**(8)
