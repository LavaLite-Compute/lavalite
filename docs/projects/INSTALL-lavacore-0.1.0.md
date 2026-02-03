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

---

### 2.4 Build and Install

```bash
cd ~/lavalite/build_rocky9
make -j
sudo make install
```

After installation, you may create a version-independent symlink:

```bash
sudo ln -s /opt/lavacore-0.1.0 /opt/lavacore
```

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

The installation tree will look like:

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
    └── work
```

Ensure runtime directories are owned by the service user:

```bash
sudo chown -R lavacore:lavacore /opt/lavacore-0.1.0/var
```

This step is mandatory.

The `lim` and `mbd` daemons run as the `lavacore` user.
If ownership is incorrect:

- Log files cannot be created in `var/log`
- State and event files cannot be written in `var/work`
- `mbd` will fail during initialization

Note: `sbd` runs as root, but correct ownership of the runtime
directories is still required for consistent cluster operation.

Verify ownership before starting services.
---

## 5. Configuration (Legacy LSF_* Naming)

Copy template configuration:

```bash
sudo cp /opt/lavacore-0.1.0/etc/lsf.conf.template \
        /opt/lavacore-0.1.0/etc/lsf.conf
```

Minimal required variable:

```
LSF_ENVDIR=/opt/lavacore-0.1.0/etc
```

Common variables:

```
LSF_CONFDIR=/opt/lavacore-0.1.0/etc
LSF_SERVERDIR=/opt/lavacore-0.1.0/sbin
LSF_LOGDIR=/opt/lavacore-0.1.0/var/log
LSB_SHAREDIR=/opt/lavacore-0.1.0/var/work
```

The `LSF_*` and `LSB_*` naming is retained for continuity with legacy Lava installations.

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

## 7. Install systemd Service Files

Copy service templates:

```bash
sudo cp lavalite/etc/lavacore-*.service /etc/systemd/system/
```

Reload systemd:

```bash
sudo systemctl daemon-reload
```

Enable services:

```bash
sudo systemctl enable lavacore-lim
sudo systemctl enable lavacore-sbd
```

On the master only:

```bash
sudo systemctl enable lavacore-mbd
```

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
env | grep LSF
```

---
