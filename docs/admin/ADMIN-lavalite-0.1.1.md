# ADMIN-lavalite-0.1.1.md

Administrator reference for **LavaLite 0.1.1**.

Covers build, installation, configuration, runtime layout, hostname resolution,
service management, and operational notes. Follow the steps in order.

Status: **Developer Preview (Alpha)** — validates protocol design, architecture,
and basic scheduler functionality. Runs real jobs. Does not yet include failover
or production-grade security.

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

Verify forward and reverse resolution on each node before proceeding:

```bash
# Forward
getent hosts master
getent hosts worker1

# Reverse
getent hosts 192.168.1.10
getent hosts 192.168.1.11
```

Both directions must return consistent results.

---

## 2. Repository and Release Tag

- **Product name:** LavaLite
- **Git tag:** `lavalite-0.1.1`
- **Installation prefix:** `/opt/lavalite-0.1.1`
- **Source directory:** `lavalite`

All steps assume the source directory is named `lavalite`.

---

## 3. Get the Source

```bash
git clone https://github.com/LavaLite-Compute/lavalite.git
cd lavalite
git checkout lavalite-0.1.1
git checkout -b release-0.1.1 lavalite-0.1.1
```

The local branch avoids detached HEAD mode.

---

## 4. Build

LavaLite uses autotools with out-of-tree builds. Generated files stay separate
from the source tree; multiple platform builds can coexist.

**Create a build directory**

```bash
cd ~/lavalite
mkdir -p build_rocky9        # adjust name for your platform
```

**Bootstrap autotools**

```bash
cd ~/lavalite
./bootstrap
```

This generates `configure` in the source tree.

**Configure**

```bash
cd ~/lavalite/build_rocky9
../configure --prefix=/opt/lavalite-0.1.1
```

All build artifacts remain in `build_rocky9`. The directory can be deleted
without affecting the source.

**Build and install**

```bash
make
sudo make install
```

**Create the version-independent symlink**

```bash
sudo ln -sfn /opt/lavalite-0.1.1 /opt/lavalite
```

All configuration, service files, and environment variables reference
`/opt/lavalite`. Future upgrades install a new versioned directory and
update this symlink — nothing else needs to change.

---

## 5. Service User and Group

```bash
sudo groupadd -r lavalite
sudo useradd -r -g lavalite -s /usr/sbin/nologin -d /opt/lavalite lavalite
id lavalite
```

Keep the account non-login in production. The shell may be temporarily enabled
during development.

---

## 6. Installation Layout and Runtime Ownership

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

**Set ownership**

```bash
sudo chown -R lavalite:lavalite /opt/lavalite/var/log
sudo mkdir -p /opt/lavalite-0.1.1/var/work/mbd
sudo chown -R lavalite:lavalite /opt/lavalite/var/work/mbd
```

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

---

## 7. Configuration

Configuration files are not installed by `make install` — copy them manually
to prevent accidental overwriting during upgrades:

```bash
cd ~/lavalite
sudo cp etc/lsf* etc/lsb* /opt/lavalite-0.1.1/etc/
```

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

## 8. Cluster Roles

LavaLite uses a master/worker model:

- **Master**: runs `lavalite-lim` and `lavalite-mbd`
- **Workers**: run `lavalite-lim` and `lavalite-sbd`

Only one master is supported. Failover is not supported in this release.
`lavalite-mbd` **must** not be started on worker nodes.

---

## 9. Core File Configuration

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

`fs.suid_dumpable = 1` is required for `sbd`, which calls `setuid()` before
executing jobs.

To debug a crash:

```bash
gdb /opt/lavalite/sbin/mbd /var/crash/core.mbd.<pid>.<timestamp>
(gdb) bt
```

---

## 10. systemd Service Files

**Install**

```bash
cd ~/lavalite
sudo cp etc/lavalite-*.service /etc/systemd/system/
sudo systemctl daemon-reload
```

**Verify unit configuration**

`lavalite-lim.service` — runs as `lavalite`:

```
ExecStart=/opt/lavalite/sbin/lim
User=lavalite
Group=lavalite
Environment=LSF_ENVDIR=/opt/lavalite/etc
```

`lavalite-mbd.service` — runs as `lavalite`:

```
ExecStart=/opt/lavalite/sbin/mbd
User=lavalite
Group=lavalite
Environment=LSF_ENVDIR=/opt/lavalite/etc
```

`lavalite-sbd.service` — runs as root. No `User=` directive:

```
ExecStart=/opt/lavalite/sbin/sbd
Environment=LSF_ENVDIR=/opt/lavalite/etc
```

Running `sbd` under a non-root user breaks `setuid()` and job execution.

**Startup ordering**

LIM must start first. The unit files enforce this:

```
Requires=lavalite-lim.service
After=lavalite-lim.service
```

**Enable**

All nodes:

```bash
sudo systemctl enable lavalite-lim lavalite-sbd
```

Master only:

```bash
sudo systemctl enable lavalite-mbd
```

**Start**

Master:

```bash
sudo systemctl start lavalite-lim lavalite-mbd
```

Workers:

```bash
sudo systemctl start lavalite-lim lavalite-sbd
```

```bash
systemctl status lavalite-lim lavalite-mbd lavalite-sbd
```
---

## 11. Verify Installation

```bash
lsid
lsload
bhosts
```

Workers should transition from `unavail` to `ok` once registered.

```bash
ls /opt/lavalite/var/log
journalctl -u lavalite-lim
journalctl -u lavalite-mbd
journalctl -u lavalite-sbd
```

---

## 12. Known Limitations

**Authentication**

`eauth` is not yet implemented. Client requests include a placeholder payload;
daemons accept without validation. Deploy only in trusted environments.

The design (shared secret + nonce, cryptographic signature) is complete and
will be integrated in the future release 1.0.

**Host availability**

`lsload` is authoritative. If LIM on a host is unreachable, `lsload` reports
`unavail`. `bhosts` may show `ok` optimistically — this is temporary while
host state tracking is rebuilt. Use `lsload` to determine whether a host is up.

**`bsub -m`**

Host selection via `bsub -m <host>` is not yet available. The command returns
an error.

---
