---
title: BJOBS
section: 1
header: LavaLite User Commands
footer: LavaLite
date: 2026
---

# NAME

bjobs - display batch jobs

# SYNOPSIS

**bjobs** [*options*] [*job_id*]

**bjobs** [**--help** | **--version**]

# DESCRIPTION

Displays information about batch jobs. Without options, shows active
jobs for the current user.

# OPTIONS

**--all**
:   Show active jobs for all users. Requires administrator privileges.

**--pend**
:   Show pending jobs with the pending reason for each job.

**--run**
:   Show running jobs only.

**--done**
:   Show finished jobs (DONE and EXIT states).

**--help**
:   Print usage to stderr and exit.

**--version**
:   Print version to stderr and exit.

# ARGUMENTS

*job_id*
:   Show a specific job by ID. If *job_id* is an array's ID, shows
    every element of the array instead of a single job. Use
    *array_id*[*index*] to show one element. Mutually exclusive with
    filter options.

# OUTPUT

Jobs are displayed in a table with the following columns:

    JOBID  USER     STAT  QUEUE    FROM_HOST  EXEC_HOST  JOB_NAME  SUBMIT_TIME

**STAT** values:

- **PEND** — waiting to be dispatched
- **RUN** — executing on a host
- **DONE** — completed successfully (exit status 0)
- **EXIT** — completed with non-zero exit status
- **SUSP** — suspended job
- **HELD** — held job, not eligible for dispatch
- **UNKNOWN** — job state cannot currently be determined

## Array Jobs

Each element of an array job submitted with **bsub --array** is a
separate job, but **JOBID** displays it as *array_id*[*index*] rather
than the element's own job ID:

    JOBID    USER   STAT     QUEUE    PRI  RUN_HOSTS  JOB_NAME  SUBMIT_TIME
    122[5]   david  PEND     sys-1.1  70   -          -         Aug 08 12:46
    122[6]   david  PEND     sys-1.1  70   -          -         Aug 08 12:46

*array_id* is the array's first element's job ID; *index* is the
element's position within the submitted range. Passing *array_id*
alone shows every element of the array; use
*array_id*[*index*] to narrow to one. Use this
*array_id*[*index*] form with **bkill**(1) to signal a single
element, or *array_id* alone to signal every element of the array.
For full detail on one element, including its own job ID, see
**bhist**(1) with *array_id*[*index*] or **-l**.

# EXAMPLES

Show your active jobs:

    bjobs

Show all pending jobs with reasons:

    bjobs --pend

Show a specific job:

    bjobs 42

Show every element of an array:

    bjobs 142

Show all finished jobs:

    bjobs --done

# SEE ALSO

**bsub**(1), **bkill**(1), **bqueues**(1), **bhosts**(1), **bhist**(1),
**mbd**(8)
