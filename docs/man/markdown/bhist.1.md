---
title: BHIST
section: 1
header: LavaLite User Commands
footer: LavaLite
date: 2026
---

# NAME

bhist - display historical job information

# SYNOPSIS

**bhist** [*options*] [*job_id*]

**bhist** [**--help** | **--version**]

# DESCRIPTION

Displays information about finished jobs from the job history log.
Without options, shows finished jobs for the current user.

# OPTIONS

**-u** *user*, **--user** *user*
:   Show finished jobs for the specified user.

**-l**, **--long**
:   Display detailed job information including resource usage,
    timestamps, and exit status.

**--help**
:   Print usage to stderr and exit.

**--version**
:   Print version to stderr and exit.

# ARGUMENTS

*job_id*
:   Show history for a specific job ID.

# OUTPUT

Displays a table with the following columns:

**JOBID**
:   Job ID.

**USER**
:   User who submitted the job.

**STAT**
:   Final job state: **DONE** (exit status 0), **EXIT** (non-zero exit status),
    or **RUN** for jobs still executing.

**QUEUE**
:   Queue the job was submitted to.

**EXEC_HOSTS**
:   Host or hosts where the job ran.

**JOB_NAME**
:   Job name as given at submission.

**SUBMIT_TIME**
:   Time the job was submitted.

**END_TIME**
:   Time the job finished. Displays **-** for jobs still running.

In long mode (**-l**), displays additional detail per job including
resource usage (CPU time, memory, swap), exit status, and all timestamps.

# EXAMPLES

Show your finished jobs:

    bhist

Show finished jobs for a specific user:

    bhist -u alice

Show detailed information for a specific job:

    bhist -l 42

# SEE ALSO

**bsub**(1), **bjobs**(1), **bkill**(1), **mbd**(8)
