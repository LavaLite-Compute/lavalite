---
title: BPRIORITY
section: 1
header: LavaLite User Commands
footer: LavaLite
date: 2026
---

# NAME

bpriority - change the scheduling priority of a job

# SYNOPSIS

**bpriority** **--priority** *N* *job_id*

**bpriority** [**--help** | **--version**]

# DESCRIPTION

Changes the scheduling priority of a job. The scheduler dispatches
jobs in order of priority (higher values first), breaking ties by
submission time (older jobs first).

A job inherits the priority of its queue at submission time. A user
may lower the priority of their own jobs to defer them behind others.
A user may not raise a job's priority above the queue priority; only
an admin may do so.

This follows the same model as Unix **nice**(1): users can be nice to
others by lowering their own priority, but cannot take precedence over
the queue limit without privilege.

Administrators may raise a job's priority above its original value.
Since pending jobs are scheduled in order of queue priority then job
priority, this effectively moves the job ahead of lower-priority work
in the same queue.

The priority change is logged and visible in **bhist**(1).

# OPTIONS

**--priority** *N*, **-p** *N*
:   New priority value. Must be a non-negative integer. For non-admin
    users, the value cannot exceed the priority of the job's current
    queue.

**--help**, **-h**
:   Print usage to stderr and exit.

**--version**, **-V**
:   Print version to stdout and exit.

# OUTPUT

On success, prints a confirmation to stdout:

    Job <42> priority set to 20.

On failure, prints an error message to stderr and exits with a non-zero
status. Common errors:

- Job not found: ESRCH
- Job is already finished: EINVAL
- Not the job owner and not admin: EPERM
- Priority exceeds queue limit (non-admin): EPERM

# EXAMPLES

Lower the priority of job 42 to let other jobs go first:

    bpriority --priority 10 42

Raise the priority of a job as admin:

    bpriority --priority 80 17

# SEE ALSO

**bsub**(1), **bjobs**(1), **bmove**(1), **bhist**(1), **bqueues**(1),
**mbd**(8), **llb.queues**(5), **nice**(1)
