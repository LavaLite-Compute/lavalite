# INSTALL-lavacore-0.1.0.md

This document describes how to build and install **LavaCore 0.1.0** from the Git repository.

Follow the steps exactly in order to get a working installation.

Status: **Developer Preview (Alpha)**

---

## 0. Prerequisites

- A supported Linux distribution (tested: Ubuntu 24.04, Rocky Linux 8/9, Fedora)
- Standard build toolchain (compiler, make, autotools, libc headers, etc.)
- systemd (for service management)

---

## 1. Get the Source and Checkout the Release Tag

Clone the repository:

```bash
git clone https://github.com/<your-org>/lavacore.git
cd lavacore
```

Checkout the 0.1.0 release tag:

```bash
git checkout lavacore-0.1.0
```

---

## 2. Build (Out-of-Tree / VPATH Build)

Create a separate build directory:

```bash
mkdir build
cd build
```

Generate `configure` (required when building from git):

```bash
../bootstrap
```

Configure the installation prefix:

```bash
../configure --prefix=/opt/lavacore
```

Build:

```bash
make -j$(nproc)
```

---

## 3. Create User and Group

Create the dedicated service group and user:

```bash
sudo groupadd -r lavacore
sudo useradd -r -g lavacore -s /usr/sbin/nologin -d /opt/lavacore lavacore
```

Verify:

```bash
id lavacore
```

Note: for debugging on a development machine you may temporarily change the shell.
For production deployments keep service accounts non-login.

---

## 4. Install

Install as root:

```bash
sudo make install
```

Installation prefix:

```
/opt/lavacore
```

---

## 5. Create Runtime Directories and Fix Ownership

Create required directories:

```bash
sudo mkdir -p /opt/lavacore/var/log
sudo mkdir -p /opt/lavacore/var/work
```

Ensure the runtime tree is owned by the service user:

```bash
sudo chown -R lavacore:lavacore /opt/lavacore/var
```

---

## 6. Configure LavaCore (LSF_* Naming Is Legacy)

Copy the template config:

```bash
sudo cp /opt/lavacore/etc/lsf.conf.template /opt/lavacore/etc/lsf.conf
```

Edit the config:

```bash
sudo vi /opt/lavacore/etc/lsf.conf
```

### Minimal required variable

Only one variable is strictly required:

```
LSF_ENVDIR=/opt/lavacore/etc
```

### Common variables (recommended)

These are typically set for a standard installation:

```
LSF_CONFDIR=/opt/lavacore/etc
LSF_SERVERDIR=/opt/lavacore/sbin
LSF_LOGDIR=/opt/lavacore/var/log
LSB_SHAREDIR=/opt/lavacore/var/work
```

Notes:

- The `LSF_*` / `LSB_*` variable naming is retained for continuity with legacy systems.
- LavaCore uses these variables to locate config, binaries, logs, and working directories.

---

## 7. Install systemd Service Files

Copy the service templates:

```bash
sudo cp contrib/systemd/lavacore-*.service /etc/systemd/system/
```

Verify that each service file points to `/opt/lavacore` (no `/opt/lavalite` leftovers).

Reload systemd:

```bash
sudo systemctl daemon-reload
```

Enable services:

```bash
sudo systemctl enable lavacore-lim
sudo systemctl enable lavacore-sbd
sudo systemctl enable lavacore-mbd
```

---

## 8. Start Services

Start daemons in order:

```bash
sudo systemctl start lavacore-lim
sudo systemctl start lavacore-mbd
sudo systemctl start lavacore-sbd
```

Check status:

```bash
systemctl status lavacore-lim
systemctl status lavacore-mbd
systemctl status lavacore-sbd
```

Expected:

```
Active: active (running)
```

---

## 9. Verify Installation

Verify cluster identity:

```bash
lsid
```

Verify load collection:

```bash
lsload
```

Inspect log files:

```bash
ls -l /opt/lavacore/var/log
```

If needed, inspect systemd logs:

```bash
journalctl -u lavacore-lim
journalctl -u lavacore-mbd
journalctl -u lavacore-sbd
```

---

## 10. Install Additional Worker Nodes

Repeat the process on each worker host:

1. Build and install under `/opt/lavacore`
2. Create user/group `lavacore`
3. Ensure `/opt/lavacore/var` ownership is `lavacore:lavacore`
4. Install config and systemd services
5. Start services

After starting, verify from the master:

```bash
lsload
```

Workers should transition from `unavail` to `ok` once registration completes.

---

## Troubleshooting Notes

- If a daemon fails early, verify `LSF_ENVDIR` points to `/opt/lavacore/etc`.
- If `mbd` fails with missing directories, verify `/opt/lavacore/var/work` exists and is writable by `lavacore`.
- If logs are missing, verify `LSF_LOGDIR` is set and writable.

---

End of document.
