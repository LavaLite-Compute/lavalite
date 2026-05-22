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

**bqueues** **--close** *queue*

**bqueues** **--open** *queue*

# DESCRIPTION

Without options, displays the current status and counters for all queues
in the cluster.

The **--close** and **--open** options require administrator privileges.

# OPTIONS

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

# SEE ALSO

**bsub**(1), **bjobs**(1), **bhosts**(1), **ll.queues**(5), **mbd**(8)
