# ADMIN-lavalite-0.1.1.md

This document is the administrator reference for **LavaLite 0.1.1**.

It covers build, installation, configuration, runtime layout, hostname resolution,
service management, and operational notes.

Follow the steps in order for a correct installation.

Status: **Developer Preview (Alpha)**

This preview validates protocol design, architecture, and basic scheduler
functionality. It runs real jobs on real workloads. It does not yet include
failover or production-grade security.

---

## 0. Prerequisites

- Linux distribution (tested: Ubuntu 24.04, Rocky Linux 8/9, Fedora)
- Standard build toolchain (gcc/clang, make, autotools, libc headers)
- systemd (for service management)
- Shared installation path across cluster nodes (e.g., NFS)
- Uniform UID/GID across nodes
- Forward and reverse hostname resolution for all cluster nodes (see section 1)

---

## 1. Hostname Resolution

LavaLite daemons identify cluster nodes by hostname. Both forward and reverse
resolution must work correctly on every node, including the master.

Daemons use both `gethostbyname()` (name → address) and `gethostbyaddr()`
(address → name). If either direction fails, daemons will fail to register,
will misidentify peers, or will silently refuse connections.

### 1.1 Requirements

For each node in the cluster:

- Forward: the node's short hostname must resolve to its cluster IP address.
- Reverse: the cluster IP address must resolve back to that same hostname.
- The hostname returned by `hostname -s` must match the name in `lsf.cluster.lavalite`.

Short hostnames are used in configuration. Fully-qualified names must also
resolve consistently if used in configuration files.

### 1.2 /etc/hosts (this release)

In this release, hostname resolution is managed via `/etc/hosts`.
This file must be consistent across all cluster nodes.

Example `/etc/hosts` entries for a two-node cluster:

```
192.168.1.10   master
192.168.1.11   worker1
```

Both entries must appear on every node in the cluster, including the master itself.

Verify forward and reverse resolution on each node:

```bash
# Forward
getent hosts master
getent hosts worker1

# Reverse
getent hosts 192.168.1.10
getent hosts 192.168.1.11
```

Both directions must return consistent results before starting any daemon.

### 1.3 Enterprise name services

In enterprise deployments, `/etc/hosts` is typically replaced or supplemented
by DNS, NIS, or LDAP-backed `nsswitch.conf` entries. The resolution requirements
are identical: both forward and reverse must work for all cluster hostnames.

LavaLite does not require any specific name service; it uses the system resolver.
Verify resolution as shown above regardless of the backend.

---

## 2. Repository and Release Tag

The LavaLite source repository:

```
lavalite
```

- **Product name:** LavaLite
- **Git tag:** `lavalite-0.1.1`
- **Installation prefix:** `/opt/lavalite-0.1.1`
- **Source directory:** `lavalite`

All steps in this document assume the source directory is named `lavalite`.

---

## 3. Get the Source and Checkout the Release Tag

Clone the repository:

```bash
git clone https://github.com/LavaLite-Compute/lavalite.git
cd lavalite
git checkout lavalite-0.1.1
```

Create a local branch to avoid detached HEAD mode:

```bash
git checkout -b release-0.1.1 lavalite-0.1.1
```

---

## 4. Build (Out-of-Tree / VPATH Build)

LavaLite uses a standard autotools build system. Out-of-tree builds keep
generated files separate from the source tree and allow multiple platform builds.

### 4.1 Create a Build Directory

Example for Rocky Linux 9:

```bash
cat /etc/redhat-release
Rocky Linux release 9.4 (Blue Onyx)
```

```bash
cd ~/lavalite
mkdir -p build_rocky9
```

### 4.2 Bootstrap Autotools

When building from Git, `configure` is not committed and must be generated.
Run bootstrap from the source directory:

```bash
cd ~/lavalite
./bootstrap
```

### 4.3 Configure

Run configure from the build directory:

```bash
cd ~/lavalite/build_rocky9
../configure --prefix=/opt/lavalite-0.1.1
```

- `../configure` refers to the script generated in the source tree.
- All build artifacts remain in `build_rocky9`.
- The build directory can be deleted without affecting the source.

### 4.4 Build and Install

```bash
cd ~/lavalite/build_rocky9
make -j
sudo make install
```

After installation, create a version-independent symlink:

```bash
sudo ln -sfn /opt/lavalite-0.1.1 /opt/lavalite
```

This symlink provides a stable logical path. Future upgrades install a new
versioned directory and update the symlink. All configuration, service files,
and environment variables should reference `/opt/lavalite`, not the versioned path.

---

## 5. Create Service User and Group

```bash
sudo groupadd -r lavalite
sudo useradd -r -g lavalite -s /usr/sbin/nologin -d /opt/lavalite lavalite
```

Verify:

```bash
id lavalite
```

For development, the shell may be temporarily enabled. For production-style
deployments, keep the account non-login.

---

## 6. Installation Layout and Runtime Ownership

### 6.1 Directory tree

After installation:

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
        ├── sbd
        │   ├── jobs
        │   └── state
```

### 6.2 Privilege model

LavaLite runs with two privilege models:

- `lim` and `mbd` run as the `lavalite` service user. They must be able to
  create and append log and state files.

- `sbd` starts as **root** because it must call `setuid()` to the submitting
  user when launching jobs. During launch, SBD creates per-job directories as
  root and then drops privileges to the job user.

This privilege split is intentional and governs all ownership requirements below.

### 6.3 Directory roles

**`var/log`** — daemon logs, writable by service user `lavalite`.

**`var/work/mbd`** — MBD runtime state, writable by service user `lavalite`.

**`var/work/sbd`** — SBD protected spool root, owned by `root`. Users must not
write here directly.

**`var/work/sbd/jobs`** — container for per-job runtime space. Created by SBD.
Per-job directories are created by SBD and then `chown`ed to the submitting user.

**`var/work/sbd/state`** — authoritative per-job protocol state. Daemon-private.
Restart-safe. Not part of the user-facing API.

### 6.4 Ownership summary

| Path                       | Owner      | Mode  | Notes                        |
|----------------------------|------------|-------|------------------------------|
| `var`                      | root       | 0755  | traversable                  |
| `var/log`                  | lavalite   | 0755  | daemon log files             |
| `var/work`                 | root       | 0755  | traversable                  |
| `var/work/mbd`             | lavalite   | 0755  | MBD state                    |
| `var/work/sbd`             | root       | 0755  | SBD spool root               |
| `var/work/sbd/jobs`        | root       | 0755  | per-job dirs created by SBD  |
| `var/work/sbd/state`       | root       | 0755  | per-job dirs created by SBD  |
| `var/work/sbd/jobs/<job>`  | submitter  | 0700  | user runtime space           |
| `var/work/sbd/state/<job>` | root       | 0700  | daemon protocol state        |

### 6.5 Set ownership

```bash
sudo chown -R lavalite:lavalite /opt/lavalite/var/log
sudo mkdir -p /opt/lavalite-0.1.1/var/work/mbd
sudo chown -R lavalite:lavalite /opt/lavalite/var/work/mbd
```

Do not create or modify `var/work/sbd`, `var/work/sbd/state`, or
`var/work/sbd/jobs`. These are initialized by `lavalite-sbd` on first startup.

### 6.6 Verification checklist

Before starting services:

- `var/log` exists and is writable by `lavalite`.
- `var/work/mbd` exists and is writable by `lavalite`.

After starting `lavalite-sbd`:

- `var/work/sbd` exists, owned by `root`.
- `var/work/sbd/state` exists, owned by `root`, mode `0755`.
- `var/work/sbd/jobs` exists, owned by `root`, mode `0755`.

After submitting a test job:

- `var/work/sbd/jobs/<jobfile>/` owned by submitting user, mode `0700`.
- `var/work/sbd/state/<jobfile>/` owned by root, mode `0700`, contains `state`.

If ownership corrections are necessary after these steps, the installation is
incorrect.

---

## 7. Configuration

Configuration files are not installed by `make install`. They must be copied
manually to prevent accidental overwriting during upgrades.

```bash
cd ~/lavalite
sudo cp etc/lsf* etc/lsb* /opt/lavalite-0.1.1/etc/
```

The `LSF_*` and `LSB_*` naming is retained for continuity with legacy Lava
installations and reflects the conceptual lineage from Platform Lava.

### 7.1 Core configuration files

**`lsf.conf`** — primary configuration file. If using default installation
paths, no changes are required. Key variables:

```
LSF_CONFDIR=/opt/lavalite-0.1.1/etc
LSF_SERVERDIR=/opt/lavalite-0.1.1/sbin
LSF_LOGDIR=/opt/lavalite-0.1.1/var/log
LSB_SHAREDIR=/opt/lavalite-0.1.1/var/work
```

**`lsf.shared`** — cluster-wide shared parameters. Defaults are suitable for
initial installation. Default cluster name is `lavalite`.

**`lsf.cluster.lavalite`** — cluster membership and administrative settings.
You must configure the cluster administrator (typically the `lavalite` user)
and the list of hosts. Host names here must match what hostname resolution
returns (see section 1).

**`lsb.hosts`** — batch hosts known to the system. Each host must be listed
explicitly. `MXJ` specifies maximum concurrent jobs per host.

**`lsb.queues`** — batch queue definitions. Defaults are suitable for initial
testing.

**`lsb.params`** — batch system parameters. Defaults are suitable for initial
validation.

### 7.2 Required environment variable

```
LSF_ENVDIR=/opt/lavalite-0.1.1/etc
```

`LSF_ENVDIR` must be set in the environment of all daemons and all client
commands (`lsid`, `lsload`, `bjobs`, etc.). It points to the directory
containing `lsf.conf`. Using an environment module to manage this is recommended
(see Appendix A).

---

## 8. Cluster Roles

LavaLite uses a master/worker model:

- **Master host**: runs `lavalite-lim` and `lavalite-mbd`
- **Worker hosts**: run `lavalite-lim` and `lavalite-sbd`

Constraints for this release:

- Only one master is supported.
- `lavalite-mbd` must be started manually on the designated master.
- `lavalite-mbd` must not be started on worker nodes.
- Failover is not supported.

---

## 9. Core File Configuration

When a daemon crashes, the kernel writes a core file for post-mortem debugging.
By default the location and naming are system-dependent. LavaLite requires a
consistent configuration across all cluster nodes.

Create `/etc/sysctl.d/99-core-dump.conf`:

```
kernel.core_pattern = /var/crash/core.%e.%p.%t
kernel.core_uses_pid = 1
fs.suid_dumpable = 1
```

Create the directory and apply:

```bash
sudo mkdir -p /var/crash
sudo chmod 1777 /var/crash
sudo sysctl -p /etc/sysctl.d/99-core-dump.conf
```

`fs.suid_dumpable = 1` is required for `sbd`, which runs as root and calls
`setuid()` before executing jobs.

Core files land in `/var/crash` on the node where the crash occurred:

```bash
gdb /opt/lavalite/sbin/mbd /var/crash/core.mbd.<pid>.<timestamp>
(gdb) bt
```

Apply this configuration to all nodes including the master.

---

## 10. systemd Service Files

LavaLite daemons are managed via systemd. Each daemon has a dedicated unit file:

- `lavalite-lim.service`
- `lavalite-sbd.service`
- `lavalite-mbd.service` (master only)

### 10.1 Install unit files

```bash
cd ~/lavalite
sudo cp etc/lavalite-*.service /etc/systemd/system/
sudo systemctl daemon-reload
```

### 10.2 Verify unit file configuration

Before enabling services, verify each unit references the correct binary path,
user identity, and environment.

**`lavalite-lim.service`** — must run as `lavalite`:

```
ExecStart=/opt/lavalite/sbin/lim
User=lavalite
Group=lavalite
Environment=LSF_ENVDIR=/opt/lavalite/etc
```

**`lavalite-mbd.service`** — must run as `lavalite`:

```
ExecStart=/opt/lavalite/sbin/mbd
User=lavalite
Group=lavalite
Environment=LSF_ENVDIR=/opt/lavalite/etc
```

**`lavalite-sbd.service`** — must run as root. Do not add a `User=` directive:

```
ExecStart=/opt/lavalite/sbin/sbd
Environment=LSF_ENVDIR=/opt/lavalite/etc
```

Running `sbd` under a non-root user will prevent `setuid()` transitions and
break job execution.

### 10.3 Startup ordering

LIM must always start first. MBD and SBD depend on LIM for cluster membership
and coordination. The unit files enforce this:

```
Requires=lavalite-lim.service
After=lavalite-lim.service
```

### 10.4 Enable services

On all nodes:

```bash
sudo systemctl enable lavalite-lim
sudo systemctl enable lavalite-sbd
```

On the master node only:

```bash
sudo systemctl enable lavalite-mbd
```

### 10.5 Start services

On the master:

```bash
sudo systemctl start lavalite-lim
sudo systemctl start lavalite-mbd
```

On worker nodes:

```bash
sudo systemctl start lavalite-lim
sudo systemctl start lavalite-sbd
```

Verify:

```bash
systemctl status lavalite-lim
systemctl status lavalite-mbd
systemctl status lavalite-sbd
```

Restarting LIM while MBD is active is unsupported and may produce inconsistent
cluster state in this release.

---

## 11. Verify Installation

```bash
lsid
lsload
```

Workers should transition from `unavail` to `ok` once registered.

Check daemon logs:

```bash
ls /opt/lavalite/var/log
journalctl -u lavalite-lim
journalctl -u lavalite-mbd
journalctl -u lavalite-sbd
```

---

## 12. Known Limitations (0.1.1)

### Authentication

Extended authentication (`eauth`) is not yet implemented. Client requests
include a placeholder payload; daemons accept without validation. Deploy only
in trusted environments.

The authentication design (cryptographic signature of a shared secret with a
nonce to prevent replay attacks) is complete and will be integrated in a future
release.

### Host availability

`lsload` is the authoritative source of host availability. If LIM on a host is
unreachable, `lsload` reports it as `unavail`.

`bhosts` reflects the scheduler's local host table and may be optimistic (showing
`ok`) even when a host is unreachable. This is temporary while host state
tracking is rebuilt.

**Operator rule:** use `lsload` to determine whether a host is up. If `lsload`
shows a host as `unavail`, jobs will not dispatch there.

### `bsub -m`

Host selection via `bsub -m <host>` is planned but not available in this release.
The command returns an error and the job is not submitted.

---

## Appendix A: Environment Modules (Optional)

Example module file (`0.1.1.lua`):

```lua
help([[
LavaLite 0.1.1
]])

whatis("Name: LavaLite")
whatis("Version: 0.1.1")
whatis("Category: HPC/HTC Workload Manager")

family("lavalite")

local root = "/opt/lavalite-0.1.1"

prepend_path("PATH", pathJoin(root, "bin"))
prepend_path("PATH", pathJoin(root, "sbin"))
prepend_path("LD_LIBRARY_PATH", pathJoin(root, "lib"))
prepend_path("MANPATH", pathJoin(root, "share", "man"))

setenv("LAVALITE_HOME", root)
setenv("LSF_ENVDIR", pathJoin(root, "etc"))
```

Load:

```bash
module load lavalite
module -t list
lavalite/0.1.1
```
