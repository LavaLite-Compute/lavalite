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
:   Show jobs belonging to the specified user.

**-l**, **--long**
:   Display additional job information including timestamps,
    execution details, resource usage, and exit status.

**--help**
:   Print usage information and exit.

**--version**
:   Print version information and exit.

# ARGUMENTS

*job_id*
:   Display detailed information and lifecycle events for the specified
    job.

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

# EXAMPLES

Display jobs for the current user:

    bhist

Display jobs for a specific user:

    bhist --user alice

Display detailed information for job 42:

    bhist 42

Display extended information:

    bhist -l 42

# SEE ALSO

**bjobs**(1), **bsub**(1), **bkill**(1), **bmove**(1),
**bpriority**(1), **mbd**(8)
