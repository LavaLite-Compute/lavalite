# INSTALL-lavacore-0.1.0.md

This document describes how to build and install **LavaCore 0.1.0** from the Git repository.

Follow the steps exactly and in order to obtain a working installation.

Status: **Developer Preview (Alpha)**

This preview is intended to validate protocol design, architecture, and basic scheduler functionality.
It runs real jobs and executes real workloads, but it does not yet include failover or production-grade security.

---

## 0. Prerequisites

- Linux distribution (tested: Ubuntu 24.04, Rocky Linux 8/9, Fedora)
- Standard build toolchain (gcc/clang, make, autotools, libc headers)
- systemd (for service management)
- Shared installation path across cluster nodes (e.g., NFS)
- Uniform UID/GID across nodes

---

## Repository Naming

At the time of writing, the LavaCore source repository directory is still named:

```
lavalite
```

This is expected and does not affect functionality.

- **Product name:** LavaCore
- **Git tag:** `lavacore-0.1.0`
- **Installation prefix:** `/opt/lavacore-0.1.0`
- **Source directory:** `lavalite`

All steps in this document assume the source directory is named `lavalite`.

Renaming or symlinking the source tree is not required.

---

## 1. Get the Source and Checkout the Release Tag

Clone the repository:

```bash
git clone https://github.com/LavaLite-Compute/lavalite.git
cd lavalite
git checkout lavacore-0.1.0
```

Checkout the release tag:

```bash
git checkout -b release-0.1.0 lavacore-0.1.0
```

This creates a local branch from the `lavacore-0.1.0` tag to avoid detached
HEAD mode.

---

## 2. Build (Out-of-Tree / VPATH Build)

LavaCore uses a standard autotools build system.

Out-of-tree builds keep generated files separate from the source tree and allow multiple platform builds.

---

### 2.1 Create a Build Directory

Example for Rocky Linux 9:

```bash
cat /etc/redhat-release
Rocky Linux release 9.4 (Blue Onyx)
```

Create a platform-specific build directory:

```bash
cd ~/lavalite
mkdir -p build_rocky9
```

---

### 2.2 Bootstrap Autotools (Source Tree)

When building from Git, `configure` is not committed and must be generated.

Run bootstrap from the source directory:

```bash
cd ~/lavalite
./bootstrap
```

This generates the `configure` script in the source tree.

---

### 2.3 Configure (Build Directory)

Run configure from the build directory:

```bash
cd ~/lavalite/build_rocky9
../configure --prefix=/opt/lavacore-0.1.0
```

Notes:

- `../configure` refers to the script generated in the source tree.
- All build artifacts remain in `build_rocky9`.
- The build directory can be deleted safely without affecting the source.
- By default lavacore will install into ```/opt/lavacore-0.1.0```

---

### 2.4 Build and Install

```bash
cd ~/lavalite/build_rocky9
make -j
sudo make install
```

After installation, you may create a version-independent symlink:

```bash
sudo ln -sfn /opt/lavacore-0.1.0 /opt/lavacore
```

This symlink provides a stable logical installation path.

Future upgrades can install a new version (e.g. `/opt/lavacore-0.1.1`)
and update the symlink to point to the new directory without modifying:

- systemd service files
- configuration paths
- environment variables
- module files

All configuration should reference `/opt/lavacore`, not the versioned directory.

---

## 3. Create Service User and Group

```bash
sudo groupadd -r lavacore
sudo useradd -r -g lavacore -s /usr/sbin/nologin -d /opt/lavacore lavacore
```

Verify:

```bash
id lavacore
```

For development environments, the shell may be temporarily enabled.
For production-style deployments, keep the account non-login.

---

## 4. Installation Layout

## 4. Installation Layout and Runtime Ownership Model

After installation, the tree looks like:

```
/opt/lavacore-0.1.0
├── bin
├── etc
├── include
├── lib
├── sbin
├── share
└── var
    ├── log
    work
├── mbd
│   └── info
└── sbd
    ├── jobs
    └── state
```

This section defines the ownership and permissions required for a correct
multi-user installation.

---

### 4.1 Service users and privilege boundaries

LavaCore runs with two different privilege models:

- `lim` and `mbd` run as the **service user** (typically `lavacore`).
  They must be able to create and append their log and state files.

- `sbd` starts as **root** because it must be able to `setuid()` to the
  submitting user when launching jobs. During job launch, SBD prepares
  per-job directories as root and then drops privileges to the job user.

This privilege split is intentional and affects directory ownership.

---
### 4.2 Directory roles

The runtime tree is split by purpose:

#### `var/log` (daemon logs)
- Contains log files written by the daemons.
- Must be writable by the service user (`lavacore` for `lim` and `mbd`).
- Log files may be readable by other users (policy decision), but only
  the service user must be able to create and rotate them.

---

#### `var/work` (runtime state and spools)
- Contains daemon state and per-job runtime files.
- Top-level is root-owned.
- Subdirectories are split by daemon responsibility.

---

#### `var/work/sbd`
- Owner: `root`
- Protected spool root for SBD.
- Users must not write here directly.
- Must be traversable (execute/search) so paths like
  `.../sbd/jobs/<jobfile>` can be accessed when appropriate.

---

#### `var/work/sbd/jobs`
- Owner: `root`
- Container directory for per-job runtime space.
- Users must not create entries here directly.
- Per-job directories are created by SBD and then chowned
  to the submitting user.

Contains:

    var/work/sbd/jobs/<jobfile>/
        job.sh
        go
        exit (optional)

---

#### `var/work/sbd/state`
- Owner: `root`
- Container directory for authoritative SBD protocol state.
- Users must not create entries here directly.
- Per-job state directories are created by SBD and remain daemon-owned.

Contains:

    var/work/sbd/state/<jobfile>/
        state

This directory contains durable protocol state only.
It is restart-safe and not part of the user-facing API.

---

### 4.3 Ownership and permissions (symbolic description)

The following rules must hold:

#### `/opt/lavacore-0.1.0/var`
- Owner: `root`
- Must be traversable by daemons and tools.

---

#### `var/log`
- Owner: service user (`lavacore`)
- Service user must have:
  - `S_IWUSR`
  - `S_IXUSR`
- Other users may have read access to log files (policy decision).

---

#### `var/work`
- Owner: `root`
- Must be traversable.

---

#### `var/work/sbd`
- Owner: `root`
- Mode typically `0755`
- Users must not write here.

---

#### `var/work/sbd/jobs`
- Owner: `root`
- Mode typically `0755`

Per-job directory:

    var/work/sbd/jobs/<jobfile>

- Owner: submitting user
- Mode: `0700`
- Contains:
  - `job.sh`
  - `go`
  - `exit` (optional)

This is user runtime space.

---

#### `var/work/sbd/state`
- Owner: `root`
- Mode typically `0755`

Per-job directory:

    var/work/sbd/state/<jobfile>

- Owner: daemon identity
  - `root` in normal mode
  - debug user in `sbd_debug`
- Mode: `0700`
- Contains:
  - `state` (authoritative protocol record)

This is daemon-private state.

---

### Design rationale

- `jobs/` = user runtime artifacts (debug-friendly).
- `state/` = durable daemon truth (restart-safe).
- Clear separation avoids mixing user-visible files with protocol state.
- Cleanup is delayed (~1 hour after `finish_acked`) to allow post-mortem inspection.


### Incorrect ownership will cause failures such as:

- `lim`/`mbd` cannot create log files under `var/log`
- `mbd` cannot write state/event files under `var/work/mbd`
- `sbd` cannot create protected spool directories correctly
- job launch fails after `setuid()` because the submitter cannot traverse
  or access per-job directories

---

### 4.5 Recommended ownership commands (minimal and explicit)

Set service-owned log directory:

```bash
sudo chown -R lavacore:lavacore /opt/lavacore-0.1.0/var/log
```

Set MBD runtime directory:

```bash
sudo mkdir -p /opt/lavacore-0.1.0/var/work/mbd
sudo chown -R lavacore:lavacore /opt/lavacore-0.1.0/var/work/mbd
```

SBD runtime directories are created by SBD on first start. They must remain
root-owned, and must be traversable (searchable) by non-root users so that
per-job directories owned by the submitter are reachable.

---

### 4.6 Verification checklist

**Verify ownership before starting services**

Before starting services, verify:

- `var/log` is writable by the service user
- `var/work/mbd` is writable by the service user
- `var/work/sbd` is root-owned
- `var/work/sbd/state` and `var/work/sbd/jfiles` are root-owned and searchable
  by non-root users (traversable), but not listable
- per-job directories created under `.../sbd/state/` and `.../sbd/jfiles/`
  are owned by the submitting user and only accessible to that user


---

## 5. Configuration (Legacy LSF_* Naming)

Configuration files are not installed automatically by `make install`.
They must be copied manually to prevent accidental overwriting during upgrades.

Change directory to the source tree and copy the configuration templates:

```bash
cd ~/lavalite
sudo cp -n etc/lsf* etc/lsb* /opt/lavacore-0.1.0/etc/
```

The `-n` flag prevents overwriting existing configuration files.

The `LSF_*` and `LSB_*` naming is retained for continuity with legacy Lava installations.

The configuration layout reflects the conceptual lineage from Platform Lava.

---

### Core Configuration Files

#### **lsf.conf**

This is the primary configuration file.

If you use the default installation paths, no changes are required.

Common variables:

```
LSF_CONFDIR=/opt/lavacore-0.1.0/etc
LSF_SERVERDIR=/opt/lavacore-0.1.0/sbin
LSF_LOGDIR=/opt/lavacore-0.1.0/var/log
LSB_SHAREDIR=/opt/lavacore-0.1.0/var/work
```

---

#### **lsf.shared**

Defines cluster-wide shared parameters.

Defaults are suitable for an initial installation.
The default cluster name is `lavacore`.

---

#### **lsf.cluster.lavacore**

Defines cluster membership and administrative settings.

You must configure:

- The cluster administrator (typically the `lavacore` user)
- The list of hosts in the cluster

Reasonable defaults are provided.

---

#### **lsb.hosts**

Defines batch hosts known to the system.

Each host must be explicitly listed.

The `MXJ` parameter specifies how many jobs may run concurrently on a host.

---

#### **lsb.queues**

Defines batch queues.

Reasonable defaults are provided for initial testing.

---

#### **lsb.params**

Defines batch system parameters.

Defaults are suitable for initial validation.

---

### Minimal Required Environment Variable

```
LSF_ENVDIR=/opt/lavacore-0.1.0/etc
```

The `LSF_ENVDIR` variable **must** be set in the environment of:

- All daemons
- All client commands (`lsid`, `lsload`, `bjobs`, etc.)

It specifies the directory containing the master configuration file (`lsf.conf`).

Using an environment module to manage this variable is recommended.
An example module file is included in the Appendix.

---

## 6. Cluster Roles (Important)

LavaCore uses a master/worker model.

- **Master host**: runs `lavacore-lim` and `lavacore-mbd`
- **Worker hosts**: run `lavacore-lim` and `lavacore-sbd`

Important constraints for this preview:

- Only **one master** is supported.
- `lavacore-mbd` must be started manually on the designated master.
- `lavacore-mbd` must **not** be started on worker nodes.
- Failover is not supported in this release.

The purpose of this preview is to validate protocol behavior and scheduler functionality.
High availability will be addressed in future versions.

---

## 7. Install and Enable systemd Service Files

LavaCore daemons are managed via systemd.
Each daemon has a dedicated unit file:

- `lavacore-lim.service`
- `lavacore-sbd.service`
- `lavacore-mbd.service` (master only)

---

### 7.1 Install Unit Files

Copy the service templates:

```bash
sudo cp lavalite/etc/lavacore-*.service /etc/systemd/system/
```

Reload systemd so it picks up the new units:

```bash
sudo systemctl daemon-reload
```

---

### 7.2 Startup Ordering and Cluster Semantics

**LIM must always start first.**

The current architecture assumes:

- No LIM failover support
- A single master LIM instance
- MBD depends on LIM for cluster membership and host communication
- SBD depends on LIM for node-level coordination

The master LIM is therefore the first process that must be running in the cluster.

Startup dependency is enforced in the unit files using:

```
Requires=lavacore-lim.service
After=lavacore-lim.service
```

This guarantees:

- `lavacore-mbd` will not start unless `lavacore-lim` is active
- `lavacore-mbd` is started only after `lavacore-lim`
- `lavacore-sbd` starts only after `lavacore-lim`

---

### 7.3 Enable Services

Enable LIM and SBD on all nodes:

```bash
sudo systemctl enable lavacore-lim
sudo systemctl enable lavacore-sbd
```

On the master node only, also enable MBD:

```bash
sudo systemctl enable lavacore-mbd
```

---

### 7.4 Starting the Cluster

On the master:

```bash
sudo systemctl start lavacore-lim
sudo systemctl start lavacore-mbd
```

On worker nodes:

```bash
sudo systemctl start lavacore-lim
sudo systemctl start lavacore-sbd
```

Verify status:

```bash
systemctl status lavacore-lim
systemctl status lavacore-mbd
systemctl status lavacore-sbd
```

---

### 7.5 Important Notes

- Always ensure `lavacore-lim` is running before starting `lavacore-mbd`.
- Restarting LIM while MBD is active is currently unsupported and may lead to inconsistent cluster state.
- In this version, the master LIM is the single source of truth.

Future versions may introduce failover or quorum-based master election,
but this release assumes a single authoritative LIM instance.

---

## 8. Start Services

On master:

```bash
sudo systemctl start lavacore-lim
sudo systemctl start lavacore-mbd
```

On workers:

```bash
sudo systemctl start lavacore-lim
sudo systemctl start lavacore-sbd
```

Verify:

```bash
systemctl status lavacore-lim
systemctl status lavacore-mbd
systemctl status lavacore-sbd
```

---

## 9. Verify Installation

```bash
lsid
lsload
```

Check logs:

```bash
ls /opt/lavacore-0.1.0/var/log
```

Check systemd logs if needed:

```bash
journalctl -u lavacore-lim
journalctl -u lavacore-mbd
journalctl -u lavacore-sbd
```

Workers should transition from `unavail` to `ok` once registered.

---

## 10. Authentication (MVP Status)

In this preview release, extended authentication (`eauth`) is not yet implemented.

Current behavior:

- Client includes placeholder authentication payload.
- Daemon does not validate `eauth`.
- Requests are accepted (`LSB_NO_ERROR`).

No cryptographic authentication is performed.

This version must only be deployed in trusted environments.
A proper authentication mechanism has been designed but is not yet integrated in this release.

The design is based on a cryptographic signature of a shared secret combined with a nonce to prevent replay attacks. Integration and full validation will be included in a future release.

---

## Appendix: Environment Modules (Optional)

Example module file (`0.1.0.lua`):

```lua
help([[
LavaCore 0.1.0
]])

whatis("Name: LavaCore")
whatis("Version: 0.1.0")
whatis("Category: HPC/HTC Workload Manager")

family("lavacore")

local root = "/opt/lavacore-0.1.0"

prepend_path("PATH", pathJoin(root, "bin"))
prepend_path("PATH", pathJoin(root, "sbin"))
prepend_path("LD_LIBRARY_PATH", pathJoin(root, "lib"))
prepend_path("MANPATH", pathJoin(root, "share", "man"))

setenv("LAVACORE_HOME", root)
setenv("LSF_ENVDIR", pathJoin(root, "etc"))
```

Load module:

```bash
module load lavacore
```

Verify:

```bash
module -t list
lavacore/0.1.0
```

---
