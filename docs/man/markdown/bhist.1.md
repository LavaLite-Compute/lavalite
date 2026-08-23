---
title: BHIST
section: 1
header: LavaLite User Commands
footer: LavaLite
date: 2026
---

# NAME

bhist - display job history and detailed job information

# SYNOPSIS

**bhist** [*options*] [*job_id*]

**bhist** [**--help** | **--version**]

# DESCRIPTION

Displays job history and detailed information for jobs known to the
LavaLite scheduler.

Without arguments, **bhist** displays jobs belonging to the current
user. When a *job_id* is specified, detailed information and lifecycle
events for that job are displayed.

Unlike **bjobs**(1), which provides a compact summary view of jobs,
**bhist** provides detailed information including submission
parameters, execution hosts, resource requests, command lines, and
job lifecycle events.

# OPTIONS

**-u** *user*, **--user** *user*
:   Show jobs belonging to the specified user. Restricted to admins
    unless *user* is the calling user.

**-l**
:   Display additional job information including timestamps,
    execution details, resource usage, and exit status.

**-r**
:   Show only jobs currently running.

**-p**
:   Show only jobs currently pending.

**-h**, **--help**
:   Print usage information and exit.

**-V**, **--version**
:   Print version information and exit.

# ARGUMENTS

*job_id*
:   Display detailed information and lifecycle events for the specified
    job. If *job_id* is an array's ID, displays every element of the
    array instead of a single job; use *array_id*[*index*] to display
    one element.

# JOB STATES

The following job states may be displayed:

**PEND**
:   Waiting to be dispatched.

**HELD**
:   Held by the user and not eligible for dispatch.

**RUN**
:   Executing on one or more hosts.

**SUSP**
:   Suspended.

**DONE**
:   Completed successfully.

**EXIT**
:   Completed with a non-zero exit status.

# OUTPUT

When invoked without a *job_id*, **bhist** displays a detailed summary
for each matching job, including:

- Job identifier
- User
- Queue
- Current state
- Submission time
- Working directory
- Resource requests
- Command
- Dispatch status

When invoked with a *job_id*, **bhist** additionally displays the job
lifecycle history, including events such as:

- Submission
- Dispatch
- Fork
- Suspension
- Resume
- Completion
- Exit

## Array Jobs

A bare array_id (see **bsub**(1) **--array**) displays every element
of the array, one block per element:

    Job <142[1]>  User <david>  Queue <sys-1.1>  Status <HELD>
      Job ID:       142
      Array range:  1-3:1
      ...
    Job <142[2]>  User <david>  Queue <sys-1.1>  Status <HELD>
      Job ID:       143
      Array range:  1-3:1
      ...

Each element's **Job <...>** header uses *array_id*[*index*] — the
same form **bjobs**(1) displays — but the **Job ID:** field inside
the block is that element's own job ID, which **bkill**(1) and
**bhist** also accept directly, the same as an ordinary job.

Use *array_id*[*index*] to display a single element instead of the
whole array:

    bhist 142[2]

# EXAMPLES

Display jobs for the current user:

    bhist

Display jobs for a specific user:

    bhist --user alice

Display detailed information for job 42:

    bhist 42

Display every element of an array:

    bhist 142

Display a single array element:

    bhist 142[2]

Display extended information:

    bhist -l 42

Display only currently running jobs:

    bhist -r

Display only currently pending jobs:

    bhist -p

# SEE ALSO

**bjobs**(1), **bsub**(1), **bkill**(1), **bmove**(1),
**bpriority**(1), **mbd**(8)
