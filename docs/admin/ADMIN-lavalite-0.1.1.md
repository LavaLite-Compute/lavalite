# ADMIN-lavalite-0.1.1.md

Administrator reference for **LavaLite 0.1.1**.

Covers build, installation, configuration, runtime layout, hostname resolution,
service management, and operational notes. Follow the steps in order.

Status: **Developer Preview (Alpha)** — validates protocol design, architecture,
and basic scheduler functionality. Runs real jobs. Does not yet include failover
or production-grade security.

---

## Prerequisites

- Linux distribution (tested: Ubuntu 24.04, Rocky Linux 8/9, Fedora)
- Standard build toolchain (gcc/clang, make, autotools, libc headers)
- systemd (for service management)
- Shared installation path across cluster nodes (e.g., NFS)
- Uniform UID/GID across nodes
- Forward and reverse hostname resolution for all cluster nodes (see section 1)

---

## Hostname Resolution

LavaLite daemons identify cluster nodes by hostname. Both forward and reverse
resolution must work on every node before starting any daemon.

Daemons use `getaddrinfo()` (name → address) and `getnameinfo()` (address → name).
If either direction fails, daemons will fail to register, misidentify peers,
or silently refuse connections.

**Requirements**

For each node in the cluster:

- Forward: the node's short hostname must resolve to its cluster IP address.
- Reverse: the cluster IP address must resolve back to that same hostname.
- `hostname -s` must return the name used in `lsf.cluster.lavalite`.

Short hostnames are used in configuration. Fully-qualified names must also
resolve consistently if used in configuration files.

**Name service**

LavaLite uses the system resolver and requires no specific name service backend.
DNS, NIS, `/etc/hosts`, or any combination via `nsswitch.conf` all work, provided
the naming is consistent across every node. The mechanism does not matter;
consistency does.

Example using `/etc/hosts` for a two-node cluster:

```
192.168.1.10   master
192.168.1.11   worker1
```

```bash
# Forward
getent hosts master
getent hosts worker1

# Reverse
getent hosts 192.168.1.10
getent hosts 192.168.1.11
```

Both directions must return consistent results.
Verify forward and reverse resolution on each node. Misconfigured hostname
resolution is a common source of daemon registration failures and silent
connection refusals.

---

## Repository and Release Tag

- **Product name:** LavaLite
- **Git tag:** `lavalite-0.1.1`
- **Installation prefix:** `/opt/lavalite-0.1.1`
- **Source directory:** `lavalite`

All steps assume the source directory is named `lavalite`.

## Build and Install

LavaLite uses autotools with out-of-tree builds. Generated files stay separate
from the source tree; multiple platform builds can coexist.

Clone the repository and checkout the release tag.

```
git clone https://github.com/LavaLite-Compute/lavalite.git
cd lavalite

git checkout lavalite-0.1.1
git checkout -b release-0.1.1 lavalite-0.1.1
```

Generate the build system:

```
./bootstrap
```

Configure, build and install:

```
../configure --prefix=/opt/lavalite-0.1.1

make
sudo make install
```

**Create the version-independent symlink**

```
sudo ln -sfn /opt/lavalite-0.1.1 /opt/lavalite
```

All configuration, service files, and environment variables reference
`/opt/lavalite`. Future upgrades install a new versioned directory and
update this symlink — nothing else needs to change.

**Set runtime directory ownership**
```bash
sudo chown lavalite:lavalite /opt/lavalite-0.1.1/var/log
sudo chown lavalite:lavalite /opt/lavalite-0.1.1/var/work
```

`var/work/mbd` is created and initialized by `lavalite-mbd` on first startup,
owned by `lavalite`. `var/work/sbd` and its subdirectories are created and
initialized by `lavalite-sbd` on first startup, owned by `root`.

## Service User and Group

Create a system account for the daemons. The service files and documentation
assume `lavalite:lavalite`. Adapt if your site uses a different account.

## Installation Layout and Runtime Ownership

**Directory tree**

```
/opt/lavalite-0.1.1
├── bin
├── etc
├── include
├── lib
├── sbin
├── share
└── var
    ├── log
    └── work
        ├── mbd
        └── sbd
            ├── jobs
            └── state
```

**Privilege model**

`lim` and `mbd` run as the `lavalite` service user — they create and append
log and state files.

`sbd` starts as root — it must call `setuid()` to the submitting user before
launching jobs. SBD creates per-job directories as root then drops privileges
to the job user.

This split is intentional and governs all ownership below.

**Directory roles**

`var/log` — daemon logs, writable by `lavalite`.

`var/work/mbd` — MBD runtime storage, writable by `lavalite`.

`var/work/sbd` — SBD protected storage root, owned by `root`. Users must not
write here directly.

`var/work/sbd/jobs` — per-job runtime storage, created and `chown`ed by SBD
to the submitting user.

`var/work/sbd/state` — authoritative per-job protocol state. Daemon-private.
Restart-safe.

**Ownership table**

| Path                       | Owner     | Mode |
|----------------------------|-----------|------|
| `var`                      | root      | 0755 |
| `var/log`                  | lavalite  | 0755 |
| `var/work`                 | root      | 0755 |
| `var/work/mbd`             | lavalite  | 0755 |
| `var/work/sbd`             | root      | 0755 |
| `var/work/sbd/jobs`        | root      | 0755 |
| `var/work/sbd/state`       | root      | 0755 |
| `var/work/sbd/jobs/<job>`  | submitter | 0700 |
| `var/work/sbd/state/<job>` | root      | 0700 |


Do not create or modify `var/work/sbd`, `var/work/sbd/state`, or
`var/work/sbd/jobs` — `lavalite-sbd` initializes these on first startup.

**Verification**

Before starting services: `var/log` and `var/work/mbd` exist and are writable
by `lavalite`.

After starting `lavalite-sbd`: `var/work/sbd`, `var/work/sbd/state`, and
`var/work/sbd/jobs` exist, owned by `root`, mode `0755`.

After submitting a test job: `var/work/sbd/jobs/<jobfile>/` owned by submitting
user mode `0700`; `var/work/sbd/state/<jobfile>/` owned by root mode `0700`,
contains `state`. If manual ownership corrections are needed, the installation
is incorrect.

## Service Files

Service file templates are provided in `lavalite/etc`:

- `lavalite-lim.service` — install on master and all workers
- `lavalite-mbd.service` — install on master only
- `lavalite-sbd.service` — install on master and all workers

Review and adapt them to your site before installing. Once in place:
```bash
sudo systemctl daemon-reload
sudo systemctl enable lavalite-lim lavalite-mbd   # master
sudo systemctl enable lavalite-lim lavalite-sbd   # workers
```

## Configuration

Configuration files are not installed by `make install` — manual installation
prevents accidental overwriting during upgrades.
Start from the templates in `lavalite/etc`. Copy them to `/opt/lavalite-0.1.1/etc`,
review, and adapt to your site before starting any daemon.

The `LSF_*` / `LSB_*` naming is retained for continuity with legacy Lava
installations.

**Core configuration files**

`lsf.conf` — primary configuration. If using default paths, no changes required.
Key variables:

```
LSF_CONFDIR=/opt/lavalite-0.1.1/etc
LSF_SERVERDIR=/opt/lavalite-0.1.1/sbin
LSF_LOGDIR=/opt/lavalite-0.1.1/var/log
LSB_SHAREDIR=/opt/lavalite-0.1.1/var/work
```

`lsf.shared` — cluster-wide shared parameters. Default cluster name is `lavalite`.

`lsf.cluster.lavalite` — cluster membership. Configure the administrator
(typically `lavalite`) and host list. Host names must match what `getaddrinfo()`
returns (see section 1).

`lsb.hosts` — batch hosts. Each host must be listed explicitly. `MXJ` sets
maximum concurrent allocations per host.

`lsb.queues` — batch queue definitions. Defaults are suitable for initial testing.

`lsb.params` — batch system parameters. Defaults are suitable for initial validation.

**Required environment variable**

`LSF_ENVDIR` **must** be set for all daemons and client commands
(`lsid`, `lsload`, `bjobs`, etc.):

```
LSF_ENVDIR=/opt/lavalite-0.1.1/etc
```
---

## Cluster Roles

LavaLite uses a master/worker model:

- **Master**: runs `lavalite-lim` and `lavalite-mbd`
- **Workers**: run `lavalite-lim` and `lavalite-sbd`

Only one master is supported. Failover is not supported in this release.
`lavalite-mbd` **must** not be started on worker nodes.

---

## Core File Configuration

All cluster nodes must have consistent core dump configuration for post-mortem
debugging.

Create `/etc/sysctl.d/99-core-dump.conf`:

```
kernel.core_pattern = /var/crash/core.%e.%p.%t
kernel.core_uses_pid = 1
fs.suid_dumpable = 1
```

Apply:

```bash
sudo mkdir -p /var/crash
sudo chmod 1777 /var/crash
sudo sysctl -p /etc/sysctl.d/99-core-dump.conf
```

`fs.suid_dumpable = 1` is required for `sbd`.

---

##  Verify Installation

```bash
lsid
lsload
bhosts
```

Workers should transition from `unavail` to `ok` once registered.

Daemon logs are written to `/opt/lavalite/var/log`. Check them if a daemon
fails to start or a host does not register:

```bash
ls -lt /opt/lavalite/var/log/
tail -f /opt/lavalite/var/log/lim.log.<hostname>
tail -f /opt/lavalite/var/log/mbd.log
tail -f /opt/lavalite/var/log/sbd.log.<hostname>
tail -f /opt/lavalite/var/log/compactor.log.<hostname>

```

---

## Roadmap

The 0.1.x series validates core architecture: daemon lifecycle, job dispatch,
protocol correctness, and restart safety. The following work is planned for 1.0.

**Authentication**

0.1.x relies on network perimeter trust. 1.0 will introduce HMAC-based cluster
authentication using per-host shared keys and timestamp-bounded nonces.

**Capacity scheduling**

0.1.1 hosts have capacity measured in jobs: `MXJ` sets the maximum concurrent
jobs per host, but each job consumes one undifferentiated unit regardless of
what it actually needs. 1.0 will model host capacity in CPUs, memory, and GPUs.
A job requests a slice of that capacity; the scheduler allocates it and SBD
enforces it via cgroups. A job may still claim the entire host.

**Failover**

Master failover is not supported in 0.1.x. A passive standby mechanism is
planned for 1.0, allowing a designated backup master to take over MBD
responsibilities without losing job state.
