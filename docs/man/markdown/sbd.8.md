---
title: SBD
section: 8
header: LavaLite System Administration
footer: LavaLite
date: 2026
---

# NAME

sbd - LavaLite slave batch daemon

# SYNOPSIS

**sbd** [*options*]

# DESCRIPTION

**sbd** is the slave batch daemon. It runs on each execution host in the
cluster, receives jobs dispatched by **mbd**, forks and executes job
scripts, enforces resource limits via cgroup v2, performs job control
operations such as suspend, resume, and terminate, and reports
job status back to **mbd**.

One **sbd** instance runs per execution host. It registers with **mbd**
at startup and maintains a persistent connection for job dispatch and
status reporting.

# OPTIONS

**-n**, **--non_root**
:   Run in non-root mode. Disables operations that require root
    privileges such as cgroup setup and user switching. Useful for
    testing.

**-c** *dir*, **--confdir** *dir*
:   Set the configuration directory, equivalent to setting the
    **LL_CONF_DIR** environment variable.

**-s** *name*:*port*, **--simulator** *name*:*port*
:   Run in simulator mode. Register with **mbd** as the host *name*
    on the given *port*. Used for testing without real execution hosts.

**-o** *N*, **--op_timer** *N*
:   Operation timer interval in seconds. Controls the main maintenance
    loop frequency. Default is 1.

**-r** *N*, **--resend_timer** *N*
:   Resend ACK timeout in seconds. How long to wait before resending
    an unacknowledged message to **mbd**. Default is 1.

**-V**, **--version**
:   Print version to stderr and exit.

**-h**, **--help**
:   Print usage to stderr and exit.

# ENVIRONMENT

**LL_CONF_DIR**
:   Directory containing LavaLite configuration files. Must be set
    unless **--confdir** is specified.

# FILES

*$LL_CONF_DIR*/auth.key
:   HMAC-SHA256 authentication key shared with **mbd**.

# SEE ALSO

**mbd**(8), **bjobs**(1), **bkill**(1), **ll.conf**(5)
