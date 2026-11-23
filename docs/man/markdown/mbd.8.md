---
title: MBD
section: 8
header: LavaLite System Administration
footer: LavaLite
date: 2026
---

# NAME

mbd - LavaLite main batch daemon

# SYNOPSIS

**mbd** [**--confdir** *dir*] [**--sched-timer** *N*]

**mbd** [**--help** | **--version**]

# DESCRIPTION

**mbd** is the main batch daemon. It is the central scheduler and
resource manager for the LavaLite cluster. It maintains the job queue,
dispatches jobs to execution hosts, tracks job state, and manages host
and queue configuration. Scheduler state is reconstructed from durable
events during startup.

**mbd** must run on the master host. It reads configuration from
**ll.conf**, **llb.queues**, and **llb.hosts** at startup.

# OPTIONS

**-c**, **--confdir** *dir*
:   Set the configuration directory, equivalent to setting the
    **LL_CONF_DIR** environment variable. If not specified,
    **LL_CONF_DIR** must be set in the environment.

**-t**, **--sched-timer** *N*
:   Set the scheduler timer interval in seconds. Controls how frequently
    the scheduler runs its dispatch cycle. Default: 5.

**-h**, **--help**
:   Print usage to stderr and exit.

**-V**, **--version**
:   Print version to stderr and exit.

# ENVIRONMENT

**LL_CONF_DIR**
:   Directory containing LavaLite configuration files. Must be set
    unless **--confdir** is specified.

# FILES

*$LL_CONF_DIR*/ll.conf
:   Main configuration file.

*$LL_CONF_DIR*/llb.queues
:   Queue definitions.

*$LL_CONF_DIR*/llb.hosts
:   Host and host group definitions.

*$LL_CONF_DIR*/ll.daemon.key
:   Daemon key, signs every message to and from **sbd**. Required.

*$LL_CONF_DIR*/ll.auth.key
:   Client key, checks client requests. Required.

# SEE ALSO

**sbd**(8), **llsign**(8), **bsub**(1), **bjobs**(1), **bhosts**(1), **bqueues**(1),
**ll.conf**(5), **llb.queues**(5), **llb.hosts**(5)
