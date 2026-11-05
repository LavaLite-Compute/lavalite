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
:   Directory where daemons write persistent state,
    including the event manifest, job sidecars, host state,
    queue state, and execution host state.

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

**LL_MBD_PORT**
:   TCP port for **mbd**.

**LL_MBD_HOST**
:   Hostname of the master host running **mbd**.

**LL_MBD_USER**
:   User account under which **mbd** runs.

## Scheduling

**LL_DEFAULT_QUEUE**
:   Queue used when a job is submitted without **--queue**.

## Finished job retention

**LL_SBD_JOB_FINISH_RETAIN**
:   Maximum number of finished jobs retained by sbd after completion.
    When the limit is exceeded, the oldest acknowledged finished jobs
    are removed. Default: 100.

**LL_MBD_JOB_FINISH_THRESHOLD**
:   Number of finished jobs older than **LL_MBD_JOB_FINISH_PERIOD**,
    the ones a compaction can drop, that triggers a compaction of the
    event manifest. Checked every 5 minutes. Default: 1000.

**LL_MBD_JOB_FINISH_PERIOD**
:   Seconds a finished job is kept by mbd, visible with
    **bjobs --done**, before a compaction may drop it. Younger
    finished jobs are kept across compactions and mbd restarts.
    0 drops every finished job at each compaction. Default: 3600.

## Timeouts (optional)

**LL_API_CONNTIMEOUT**
:   Timeout in seconds for client-to-mbd connection. Default: 10.

**LL_API_RECVTIMEOUT**
:   Timeout in seconds waiting for a reply from mbd. Default: 30.

**LL_SBD_CONNTIMEOUT**
:   Timeout in seconds for mbd-to-sbd connection.

**LL_SBD_READTIMEOUT**
:   Timeout in seconds waiting for a reply from sbd.

# EXAMPLE

    LL_CLUSTER_NAME=mycluster
    LL_STATE_DIR=/opt/lavalite-2.0.0/var/state
    LL_CONF_DIR=/opt/lavalite-2.0.0/etc
    LL_LOG_DIR=/opt/lavalite-2.0.0/var/log
    LL_LOG_MASK=LOG_WARNING
    LL_MBD_PORT=33124
    LL_MBD_HOST=master01
    LL_MBD_USER=lavalite
    LL_SBD_JOB_FINISH_RETAIN=100
    LL_MBD_JOB_FINISH_THRESHOLD=1000
    LL_MBD_JOB_FINISH_PERIOD=3600
    LL_DEFAULT_QUEUE=normal

# SEE ALSO

**mbd**(8), **sbd**(8), **llb.queues**(5), **llb.hosts**(5)
