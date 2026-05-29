---
title: BMOVE
section: 1
header: LavaLite User Commands
footer: LavaLite
date: 2026
---

# NAME

bmove - move a pending or held job to a different queue

# SYNOPSIS

**bmove** **--to** *queue* *job_id*

**bmove** [**--help** | **--version**]

# DESCRIPTION

Moves a job in PEND or HELD state to a different queue. The job retains
its job ID, resources, and submission context. Only the queue assignment
changes.

The user must have access to the destination queue. A non-admin user
cannot move a job to a queue they are not a member of. The destination
queue need not be open; a closed queue accepts jobs but does not schedule
them until reopened.

Moving a running job is not supported.

# OPTIONS

**--to** *queue*
:   Destination queue name. Required.

**--help**, **-h**
:   Print usage to stderr and exit.

**--version**, **-V**
:   Print version to stdout and exit.

# OUTPUT

On success, prints a confirmation to stdout:

    Job <42> moved to queue <gpu>.

On failure, prints an error message to stderr and exits with a non-zero
status. Common errors:

- Job not found: ESRCH
- Job is running or finished: EINVAL
- User not allowed in destination queue: EPERM

# EXAMPLES

Move job 42 from its current queue to the gpu queue:

    bmove --to gpu 42

Move a held job to a lower-priority queue before releasing:

    bmove --to low 17
    bkill --signal CONT 17

# SEE ALSO

**bsub**(1), **bjobs**(1), **bkill**(1), **bqueues**(1), **bhist**(1),
**bpriority**(1), **mbd**(8), **llb.queues**(5)
