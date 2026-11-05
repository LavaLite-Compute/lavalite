---
title: BHOSTS
section: 1
header: LavaLite User Commands
footer: LavaLite
date: 2026
---

# NAME

bhosts - display host information and manage host availability

# SYNOPSIS

**bhosts** [**-l**] [*host*...]

**bhosts** **--close** *host*

**bhosts** **--open** *host*

# DESCRIPTION

Without options, displays the current status and resource usage of all
hosts in the cluster.

The **--close** and **--open** options require administrator privileges.

# OPTIONS

*host*...
:   Show only the named hosts, in the order given. An unknown name is
    reported on stderr and the exit status is non-zero.

**-l**, **--long**
:   One block per host with used/total for each resource. Used can
    exceed total when the host capacity was lowered in **llb.hosts**
    while jobs were running: the host gets no new jobs until it drains.

**-c**, **--close** *host*
:   Close the named host. A closed host does not accept new jobs.
    Running jobs are not affected.

**-o**, **--open** *host*
:   Open a previously closed host, making it available for job dispatch.

**-h**, **--help**
:   Print usage to stderr and exit.

**-v**, **--version**
:   Print version to stderr and exit.

# OUTPUT

Displays a table with the following columns:

**HOST_NAME**
:   Hostname.

**STATE**
:   Host state. Values: **ok**, **unavail**, **ok|closed**, **unavail|closed**.
    **|tainted** is appended while the host is tainted: its **sbd**
    rejected a job and the host gets no new jobs for 60 seconds.
    The taint clears by itself, or when **sbd** reconnects.

**NCPU**
:   Total CPU slots. This is the sole scheduling capacity dial for the
    host; there is no separate maximum-jobs limit.

**MEM**
:   Total memory (M, G, T suffixes).

**STOR**
:   Total local storage (M, G, T suffixes).

**NGPU**
:   Total GPUs.

**NJOBS**
:   Current total jobs on the host.

**RUN**
:   Running jobs.

**SUSP**
:   Suspended jobs.

**USED_CPU**
:   CPU slots in use.

**USED_MEM**
:   Memory in use.

**USED_STOR**
:   Storage in use.

**USED_GPU**
:   GPUs in use.

# SEE ALSO

**bqueues**(1), **bjobs**(1), **bgroup**(1), **llb.hosts**(5), **mbd**(8)
