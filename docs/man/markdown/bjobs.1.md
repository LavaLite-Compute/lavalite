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
:   Show a specific job by ID. Mutually exclusive with filter options.

# OUTPUT

Jobs are displayed in a table with the following columns:

    JOBID  USER     STAT  QUEUE    FROM_HOST  EXEC_HOST  JOB_NAME  SUBMIT_TIME

**STAT** values:

- **PEND** — waiting to be dispatched
- **RUN** — executing on a host
- **DONE** — completed successfully (exit status 0)
- **EXIT** — completed with non-zero exit status
- **USUSP** — suspended by the user
- **SSUSP** — suspended by the scheduler

# EXAMPLES

Show your active jobs:

    bjobs

Show all pending jobs with reasons:

    bjobs --pend

Show a specific job:

    bjobs 42

Show all finished jobs:

    bjobs --done

# SEE ALSO

**bsub**(1), **bkill**(1), **bqueues**(1), **bhosts**(1), **bhist**(1),
**mbd**(8)
