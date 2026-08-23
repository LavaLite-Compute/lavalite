---
title: BKILL
section: 1
header: LavaLite User Commands
footer: LavaLite
date: 2026
---

# NAME

bkill - send a signal to a batch job

# SYNOPSIS

**bkill** [**-s** | **--signal**] *signal* *job_id* [*job_id* ...]

**bkill** [**-s** | **--signal**] *signal* *array_id*[[*index*]]

# DESCRIPTION

Sends a signal to one or more batch jobs. The job must belong to the
current user unless the user has administrator privileges.

A bare array ID signals every element of that array, equivalent to
issuing the signal to each element individually. Use
*array_id*[*index*] to target a single element instead.

*job_id* **0** signals every job the calling user owns. A manager may
signal any user's jobs this way.

# OPTIONS

**-s** *signal*, **--signal** *signal*
:   Signal to send. Symbolic names (case-insensitive) or a numeric
    signal value are accepted.

**--help**
:   Print usage to stderr and exit.

**--version**
:   Print version to stderr and exit.

# SIGNALS

The following symbolic names are accepted:

**kill**
:   SIGKILL. Unconditionally terminates the job. The job enters EXIT state.

**term**
:   SIGTERM. Requests graceful termination.

**stop**
:   SIGSTOP. Suspends the job. The job enters USUSP state.

**tstp**
:   SIGTSTP. Sends a terminal stop signal to the job.

**cont**
:   SIGCONT. Resumes a suspended job. Also used to release a job
    submitted with **bsub --hold**.

**hup**
:   SIGHUP.

*number*
:   Any valid signal number.

# EXAMPLES

Kill a job:

    bkill --signal kill 42

Suspend a job:

    bkill --signal stop 42

Resume a suspended job:

    bkill --signal cont 42

Release a held job:

    bkill --signal cont 43

Send SIGUSR1 to multiple jobs:

    bkill --signal 10 42 43 44

Kill every element of array 142:

    bkill --signal kill 142

Kill a single element of array 142:

    bkill --signal kill 142[3]

Kill every job you own:

    bkill --signal kill 0

# SEE ALSO

**bsub**(1), **bjobs**(1), **mbd**(8)
