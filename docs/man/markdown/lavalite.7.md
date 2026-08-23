---
title: LAVALITE
section: 7
header: LavaLite Overview
footer: LavaLite
date: 2026
---

# NAME

lavalite - LavaLite batch scheduling system overview

# DESCRIPTION

LavaLite is a distributed batch job scheduler for Linux clusters. It
dispatches jobs submitted by users to execution hosts, enforces resource
limits, and tracks job history.

# ARCHITECTURE

LavaLite consists of two daemons:

**mbd** (master batch daemon)
:   Runs on the master host. Manages the job queue, scheduling, and
    cluster state. All client commands connect to **mbd**.

**sbd** (slave batch daemon)
:   Runs on each execution host. Receives jobs from **mbd**, forks and
    executes job scripts, enforces resource limits via cgroup v2, and
    reports completion back to **mbd**.

Communication between **mbd** and **sbd** uses an XDR-encoded wire
protocol over TCP, authenticated with HMAC-SHA256.

# INSTALLATION

## Directory layout

A default installation uses a versioned directory only, with no
symlink:

    /opt/lavalite-<version>/

This lets multiple versions coexist on the same host, which a stable
symlink would preclude. A site that only ever runs one version and
prefers a fixed path may set up its own **/opt/lavalite** ->
**/opt/lavalite-**_version_ symlink, but this is not the default and
LavaLite does not assume it exists anywhere.

Subdirectories:

    bin/        user commands (bsub, bjobs, bkill, ...)
    sbin/       daemons (mbd, sbd)
    etc/        configuration files
    var/state/  persistent daemon state
    var/log/    daemon log files

## System user

Create a dedicated user for **mbd**:

    useradd -r -s /sbin/nologin lavalite

**sbd** must run as root to manage cgroups and switch user identity
before executing jobs.

# CONFIGURATION

Edit the three configuration files in **LL_CONF_DIR** (for example
**/opt/lavalite-**_version_**/etc**):

**ll.conf**
:   Cluster environment: paths, ports, log level, event log parameters.
    See **ll.conf**(5).

**llb.queues**
:   Queue definitions: name, priority, host group, allowed users.
    See **llb.queues**(5).

**llb.hosts**
:   Host resources, GPU devices, token pools, and host groups.
    See **llb.hosts**(5).

# AUTHENTICATION

All communication between **mbd** and **sbd** is authenticated using
HMAC-SHA256. Both daemons must share the same key file.

## Generating the key

    dd if=/dev/urandom bs=32 count=1 | base64 > $LL_CONF_DIR/auth.key
    chmod 644 $LL_CONF_DIR/auth.key
    chown lavalite:lavalite $LL_CONF_DIR/auth.key

The key file must be readable by **mbd** (runs as the lavalite user)
and by root (for **sbd**). It must be world-readable.

## Replay protection

Each authenticated message carries a timestamp. Messages older than
**LL_AUTH_MAX_AGE** seconds (default: 300) are rejected to prevent
replay attacks. Ensure clock skew between master and execution hosts
is kept below this threshold. Use NTP or chrony on all hosts.

# CGROUP SETUP

**sbd** uses cgroup v2 to enforce memory and CPU limits per job.
The systemd service unit for **sbd** must include:

    Delegate=yes

This grants **sbd** a delegated cgroup subtree. Do not remove this
directive. Without it, **sbd** cannot create per-job cgroups.

**sbd** must run as root for cgroup management. Jobs are executed under
the submitting user's identity after the cgroup is set up.

## Job survival across sbd restarts

The systemd service unit for **sbd** must also include:

    KillMode=process

Do not change this to **control-group** or **mixed**. **sbd** is a
pure executor: it forks job processes and enforces them via per-job
cgroups, but **sbd** itself is expected to be stopped, restarted, and
upgraded independently of the jobs it is running. Job processes must
outlive an **sbd** restart; **sbd** reattaches to them afterward via
its per-host state directory rather than re-launching them.

systemd's **control-group** KillMode sends the stop signal to every
process in the unit's cgroup tree, which includes the per-job cgroups
nested under it — restarting **sbd** would kill every running job on
that host along with the daemon. **process** KillMode signals only
the main **sbd** process, leaving job cgroups and their processes
untouched.

This case is easy to miss in testing: nothing exercises "restart
**sbd** while jobs are running" as distinct from a clean start, so an
accidental change to KillMode will not surface until a real upgrade
or crash recovery kills running jobs on that host.

# SYSTEMD SERVICES

Install the service units:

    cp lavalite-mbd.service /etc/systemd/system/
    cp lavalite-sbd.service /etc/systemd/system/
    systemctl daemon-reload

Enable and start on the master host:

    systemctl enable --now lavalite-mbd

Enable and start on each execution host:

    systemctl enable --now lavalite-sbd

**mbd** and **sbd** connect to each other automatically. **sbd**
initiates the connection to **mbd** at startup and reconnects on failure.

## Service files

**/etc/systemd/system/lavalite-mbd.service**
:   Unit for the master batch daemon. Runs as the lavalite user.
    Requires network and remote filesystems to be available.

**/etc/systemd/system/lavalite-sbd.service**
:   Unit for the slave batch daemon. Runs as root. Uses
    **KillMode=process** (see CGROUP SETUP) so that running jobs are
    not killed when **sbd** is restarted. Uses **Delegate=yes** for
    cgroup management.

# FIRST-TIME SETUP CHECKLIST

1. Create the lavalite system user on the master host.
2. Install LavaLite under **/opt/lavalite-**_version_.
3. Write **ll.conf**, **llb.queues**, and **llb.hosts**.
4. Generate **auth.key** and set correct permissions.
5. Install and enable **lavalite-mbd.service** on the master host.
6. Copy **ll.conf** and **auth.key** to each execution host.
7. Install and enable **lavalite-sbd.service** on each execution host.
8. Verify with **bhosts** and **bqueues** from the master host.

# FILES

*$LL_CONF_DIR/ll.conf*
:   Cluster environment configuration.

*$LL_CONF_DIR/llb.queues*
:   Queue configuration.

*$LL_CONF_DIR/llb.hosts*
:   Host configuration.

*$LL_CONF_DIR/auth.key*
:   HMAC-SHA256 authentication key.

*/etc/systemd/system/lavalite-mbd.service*
:   systemd unit for mbd.

*/etc/systemd/system/lavalite-sbd.service*
:   systemd unit for sbd.

# SEE ALSO

**mbd**(8), **sbd**(8), **bsub**(1), **bjobs**(1), **bhosts**(1),
**bqueues**(1), **ll.conf**(5), **llb.queues**(5), **llb.hosts**(5)
