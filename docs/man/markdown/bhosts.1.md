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

**bhosts**

**bhosts** **--close** *host*

**bhosts** **--open** *host*

# DESCRIPTION

Without options, displays the current status and resource usage of all
hosts in the cluster.

The **--close** and **--open** options require administrator privileges.

# OPTIONS

**--close** *host*
:   Close the named host. A closed host does not accept new jobs.
    Running jobs are not affected.

**--open** *host*
:   Open a previously closed host, making it available for job dispatch.

**--help**
:   Print usage to stderr and exit.

**--version**
:   Print version to stderr and exit.

# OUTPUT

Displays a table with the following columns:

**HOST_NAME**
:   Hostname.

**STATE**
:   Host state. Values: **ok**, **unavail**, **ok|closed**, **unavail|closed**.

**MAX**
:   Maximum number of jobs allowed on the host.

**NCPU**
:   Total CPU slots.

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
