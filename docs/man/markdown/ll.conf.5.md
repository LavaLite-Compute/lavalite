---
title: LL.CONF
section: 5
header: LavaLite Configuration Files
footer: LavaLite
date: 2026
---

# NAME

ll.conf - LavaLite cluster environment configuration

# DESCRIPTION

**ll.conf** is sourced as a Bourne shell script by all LavaLite daemons
and commands at startup. It defines the cluster environment, directory
paths, network ports, and runtime parameters.

The location of **ll.conf** is determined by the **LL_CONF_DIR**
environment variable, which must be set before starting any daemon or
running any command. Alternatively, **mbd** and **sbd** accept a
**--confdir** option.

# FORMAT

The file uses standard shell variable assignment syntax:

    NAME=VALUE
    NAME="VALUE WITH SPACES"

Lines starting with **#** are comments. Blank lines are ignored.

# PARAMETERS

## Cluster identity

**LL_CLUSTER_NAME**
:   Logical name of the cluster. Used for identification in logs.

## Directories

**LL_CONF_DIR**
:   Directory containing configuration files (**ll.conf**, **llb.queues**,
    **llb.hosts**, **auth.key**).

**LL_STATE_DIR**
:   Directory where daemons write persistent state (job event log,
    job sidecars, compacted archives).

**LL_LOG_DIR**
:   Directory for daemon log files.

## Logging

**LL_LOG_MASK**
:   Minimum log level. Messages at this level and above are recorded.
    Values from highest to lowest: **LOG_EMERG**, **LOG_ALERT**,
    **LOG_CRIT**, **LOG_ERR**, **LOG_WARNING**, **LOG_NOTICE**,
    **LOG_INFO**, **LOG_DEBUG**.
    Default: **LOG_WARNING**.

## Network

**LL_LIM_PORT**
:   TCP port for the Load Information Manager. Must not conflict with
    other services.

**LL_MBD_PORT**
:   TCP port for **mbd**.

**LL_MBD_HOST**
:   Hostname of the master host running **mbd**.

**LL_MBD_USER**
:   User account under which **mbd** runs.

**LL_SBD_PORT**
:   TCP port for **sbd** on each execution host.

## Scheduling

**LL_DEFAULT_QUEUE**
:   Queue used when a job is submitted without **--queue**.

## Event log

**LL_EVENTS_MAX_SIZE**
:   Trigger log compaction when the event log reaches this size.
    Accepts a plain integer (bytes) or a suffix: K, M, G.
    Example: **100M**.

**LL_EVENTS_RETAIN**
:   How long to retain finished job records in the event log before
    they are eligible for compaction. Accepts a plain integer (seconds)
    or a suffix: **h** (hours), **d** (days).
    Example: **24h**.

## Timeouts (optional)

**LL_API_CONNTIMEOUT**
:   Timeout in seconds for client-to-mbd connection. Default: 10.

**LL_API_RECVTIMEOUT**
:   Timeout in seconds waiting for a reply from mbd. Default: 30.

**LL_SBD_CONNTIMEOUT**
:   Timeout in seconds for mbd-to-sbd connection.

**LL_SBD_READTIMEOUT**
:   Timeout in seconds waiting for a reply from sbd.

**LL_MAX_SCHED_STAY**
:   Maximum time in seconds a job may remain in the scheduler without
    being dispatched before an error is logged.

# EXAMPLE

    LL_CLUSTER_NAME=mycluster
    LL_STATE_DIR=/opt/lavalite/var/state
    LL_CONF_DIR=/opt/lavalite/etc
    LL_LOG_DIR=/opt/lavalite/var/log
    LL_LOG_MASK=LOG_WARNING
    LL_LIM_PORT=33123
    LL_MBD_PORT=33124
    LL_MBD_HOST=master01
    LL_MBD_USER=lavalite
    LL_SBD_PORT=33125
    LL_EVENTS_MAX_SIZE=100M
    LL_EVENTS_RETAIN=24h
    LL_DEFAULT_QUEUE=normal

# SEE ALSO

**mbd**(8), **sbd**(8), **llb.queues**(5), **llb.hosts**(5)
