# LavaLite Administrator Guide — Installation

## Requirements

LavaLite requires:

- Linux x86_64
- Kernel 5.14 or later
- cgroup v2
- a service manager (systemd examples provided)
- OpenSSL
- NTP or chrony

Verify cgroup v2:

```sh
mount | grep cgroup2
```

Expected output:

```text
cgroup on /sys/fs/cgroup type cgroup2 (...)
```

## Create the LavaLite Administrator Account

Create a dedicated account for running `mbd` typically `lavalite:lavalite`
but it can be any non privileged user.


The master daemon (`mbd`) runs as the `lavalite` user.

The slave daemon (`sbd`) runs as `root` because it manages cgroups and
executes jobs on behalf of cluster users.

## Build and Install

Build LavaLite as admin or ordinary user:

```sh
./bootstrap
./configure
make
sudo make install
```
By default LavaLite is installed under a versioned directory, for example
`/opt/lavalite-<version>` (e.g. `/opt/lavalite-1.1.0`).

This directory forms the root of the LavaLite installation. All
executables, configuration files, logs, and persistent state are located
beneath it:

    bin/
    sbin/
    etc/
    var/log/
    var/state/

Configuration files reference the installation root directly as shown
in the examples in the `etc` directory.


## Configure LL_CONF_DIR

All commands and daemons **require**:

```sh
export LL_CONF_DIR=/opt/lavalite-<version>/etc
```

Most installations configure this through an environment module or shell
initialization script.

Verify:

```sh
echo $LL_CONF_DIR
```

## Authentication Key

LavaLite uses a shared authentication key to sign requests exchanged
between clients, `mbd`, and `sbd`.

Generate the key:

```sh
dd if=/dev/urandom bs=32 count=1 | base64 \
    > $LL_CONF_DIR/auth.key
```

Set permissions:

```sh
chown lavalite:lavalite $LL_CONF_DIR/auth.key
chmod 644 $LL_CONF_DIR/auth.key
```

The key must be world readable because user commands and applications
linked against `libllbatch` generate HMAC signatures locally and must
be able to read it.

Copy the same file to every execution host.

## Configuration Files

LavaLite configuration is stored in:

```text
ll.conf
llb.queues
llb.hosts
```

All files reside under:

```text
$LL_CONF_DIR
```

Example configuration files are provided in the installation `etc`
directory and should be customized for the local site.

### ll.conf

`ll.conf` defines cluster-wide settings such as:

- Cluster name
- State directory
- Log directory
- Network ports
- Default queue
- Logging configuration

Example:

```sh
LL_CLUSTER_NAME=lavalite
LL_STATE_DIR=/opt/lavalite-<version>/var/state
LL_CONF_DIR=/opt/lavalite-<version>/etc
LL_LOG_DIR=/opt/lavalite-<version>/var/log
```

See:

```text
man 5 ll.conf
```

for the complete reference.

### llb.hosts

`llb.hosts` defines:

- Execution hosts
- GPU devices
- Token pools
- Host groups
- Simulator hosts

See:

```text
man 5 llb.hosts
```

for the complete reference.

### llb.queues

`llb.queues` defines:

- Scheduling queues
- Queue priorities
- Host group assignments
- User access controls

See:

```text
man 5 llb.queues
```

for the complete reference.

## Post-Install Setup

**This step is mandatory. The daemons require the runtime state
directories and permissions created by this step.**

A post-install script is provided in etc/post-install.sh that outlines the steps
that must be performed after installation. The script can be run directly or the steps performed manually.

```sh
export LL_CONF_DIR=/opt/lavalite-<version>/etc
/opt/lavalite-<version>/etc/post-install.sh
```

The script performs the following operations:

- chown lavalite:lavalite `var/state/mbd`
- chown lavalite:lavalite `var/state/sbd`
- chown lavalite:lavalite  `var/log`
- Sets `bin/bhist` to `root:lavalite` mode `2755` (setgid)

### Why bhist is setgid

The event manifest under `var/state/mbd` is readable only by the mbd
user and group.

`bhist` runs setgid with the mbd primary group, allowing it to read the
event manifest and report only records visible to the calling user.

## Directory Layout

A typical installation contains:

```text
/opt/lavalite-<version>
├── bin
├── sbin
├── etc
├── lib
├── share
└── var
    ├── log
    └── state
        ├── mbd
        └── sbd
```

### Commands

```text
bin/
├── bsub
├── bjobs
├── bhist       (setgid mbd_group after post-install)
├── bkill
├── bmove
├── bpriority
├── bhosts
├── bqueues
├── bgroups
└── btokens
```

### Daemons

```text
sbin/
├── mbd
└── sbd
```

### Configuration

```text
etc/
├── ll.conf
├── llb.queues
├── llb.hosts
└── ll.auth.key
```

### Master State

```text
var/state/mbd/
├── manifest    (mode 640, mbd_user:mbd_group)
├── hosts
├── job_id_seq
├── jobs
└── queues
```

This directory is maintained by `mbd`.

### Execution Host State

```text
var/state/sbd/
└── hostname
    ├── jobs
    └── state
```

This directory is maintained by `sbd`, one per host.

### Simulator State

When simulated hosts are configured, each simulator has its own
directory next to the real hosts, named after the sim:

```text
var/state/sbd/
├── worker4/
├── sim1/
├── sim2/
└── ...
```

### Shared State

`var/state` may be local to each host or on a shared filesystem. Each
daemon only uses its own directory, `mbd/` or `sbd/<hostname>`, so
hosts never share files. A fast NFS server is fine for small and
medium clusters.

## Daemon Working Directory and Core Files

Each daemon changes to its own state directory at startup, also when
started by hand:

```text
mbd        var/state/mbd
sbd        var/state/sbd/<hostname>
sim sbd    var/state/sbd/<sim name>
```

A daemon that crashes leaves its core file there, next to its state,
one directory per host also when `var/state` is shared. The units set
`LimitCORE=infinity`.

Where the kernel writes core files is set by `kernel.core_pattern`,
for the whole host. Core files land in the working directory only if
the pattern is a plain file name:

```sh
# /etc/sysctl.d/50-core.conf
kernel.core_pattern = core.%e.%p.%t
```

```sh
sysctl --system
sysctl kernel.core_pattern
```

If the pattern starts with `|`, a handler takes the core instead:

- `|/usr/lib/systemd/systemd-coredump`, the default on RHEL and Rocky:
  cores are kept in `/var/lib/systemd/coredump`, find them with
  `coredumpctl list mbd` and open the last one with `coredumpctl gdb
  mbd`.
- `|/usr/share/apport/apport`, the default on Ubuntu: apport drops the
  core of a program that is not from a package, LavaLite daemons
  included. Set the plain pattern above, or install
  `systemd-coredump`.

Choose per site policy; the plain pattern keeps everything LavaLite
needs in one tree.

## Ownership and Permissions

The installation tree should be readable by all users.

Runtime state and log directories must be writable by the daemon that
maintains them.

Typical ownership:

```text
ll.auth.key                lavalite:lavalite 644

var/state/mbd              lavalite:lavalite 750
var/state/mbd/manifest     lavalite:lavalite 640
var/log                    lavalite:lavalite 755

var/state/sbd              root:root         755

bin/bhist                  root:lavalite     2755
```

## Create Configuration Files

See [Configuration Files](#configuration-files) above for the file
reference, or `man 5 ll.conf` / `man 5 llb.hosts` / `man 5 llb.queues`
for the complete per-file reference.

Create `ll.conf`, `llb.queues`, and `llb.hosts` under `$LL_CONF_DIR`
before starting any daemon.

## Install systemd Units

`make install` puts the units in `lib/systemd`. Review them against
your site before installing:

- `User=` in `lavalite-mbd.service` must be `LL_MBD_USER` of `ll.conf`.
- `LL_CONF_DIR` and the paths use the `/opt/lavalite` symlink to the
  active version.

```sh
cp /opt/lavalite-<version>/lib/systemd/lavalite-*.service \
    /etc/systemd/system/
systemctl daemon-reload
```

Enable `mbd` on the master host:

```sh
systemctl enable --now lavalite-mbd
```

Enable `sbd` on each execution host:

```sh
systemctl enable --now lavalite-sbd
```

Simulated hosts use the template `lavalite-sbd-sim@.service`, one
instance per `Begin Sim` row, named `NAME:PORT`, on the sim's
`REAL_HOST`:

```sh
systemctl enable --now lavalite-sbd-sim@sim1:33126
```

All units restart their daemon when it dies (`Restart=on-failure`).
`sbd` units use `KillMode=process`: running jobs survive a restart of
`sbd`.

## Verification

Verify daemon status:

```sh
systemctl status lavalite-mbd
systemctl status lavalite-sbd
```

Verify scheduler visibility:

```sh
bhosts
bqueues
```

Expected result:

- `mbd` is active.
- `sbd` is active.
- At least one queue is visible.
- At least one execution host is visible.
