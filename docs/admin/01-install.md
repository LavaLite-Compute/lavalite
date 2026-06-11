# LavaLite Administrator Guide — Installation

## Requirements

LavaLite requires:

- Linux x86_64
- Kernel 5.14 or later
- cgroup v2
- systemd
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

Build LavaLite:

```sh
./configure --prefix=/opt/lavalite-1.0.0
make
make install
```

LavaLite is installed under a versioned directory, for example
`/opt/lavalite-1.0.0`. Sites may create a convenience symlink without
the version number if local policy requires it, but this is not
mandatory.

It is recommended that paths in configuration files reflect the real
installation location rather than a symlink, to avoid ambiguity during
upgrades.

## Post-Install Setup

**This step is mandatory. Do not proceed to configuration until it is complete.**

A post-install script is provided in etc/post-install.sh that outlines the steps that must be performed after installation. The script can be run directly or the steps performed manually.

It must be run as root with `LL_CONF_DIR` set in the environment and
`LL_MBD_USER` already defined in `ll.conf`.

```sh
export LL_CONF_DIR=/opt/lavalite/etc
/opt/lavalite/etc/post-install.sh
```

The script performs the following operations:

- Creates `var/state/mbd` owned by the mbd user, mode `750`
- Creates `var/state/sbd` owned by root, mode `750`
- Creates `var/log` mode `755`
- Sets `bin/bhist` to `root:mbd_group` mode `2755` (setgid)

### Why bhist is setgid

The event log under `var/state/mbd` is readable only by the mbd user
and group. Regular users cannot read it directly.

`bhist` runs setgid with the mbd primary group, which allows it to open
the event log on behalf of any user. It then filters records by uid and
returns only the caller's own jobs. The raw event log is never exposed
to unprivileged users.

## Directory Layout

A typical installation contains:

```text
/opt/lavalite
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
└── auth.key
```

### Master State

```text
var/state/mbd/
├── eventlog    (mode 640, mbd_user:mbd_group)
├── hosts
├── job_id_seq
├── jobs
└── queues
```

This directory is maintained by `mbd`.

### Execution Host State

```text
var/state/sbd/
├── jobs
└── state
```

This directory is maintained by `sbd`.

### Simulator State

When simulated hosts are configured:

```text
var/state/
├── sim1/
├── sim2/
└── ...
```

Each simulator instance maintains its own local `sbd` state.

## Daemon Working Directory and Core Files

Core files are an essential debug tool when a daemon crashes
unexpectedly. The following is a suggested setup; actual configuration
depends on site policy and how systemd units are configured.

A suggested working directory for the daemons:

```sh
mkdir /var/log/lavalite
chown -R lavalite:lavalite /var/log/lavalite
```

A suggested core pattern (relative path, no directory prefix):

```sh
sysctl -w kernel.core_pattern=core.%e.%p
```

Inspect the current setting:

```sh
sysctl kernel.core_pattern
```

Site administrators should adjust these settings to match local policy.

## Ownership and Permissions

The installation tree should be readable by all users.

Runtime state and log directories must be writable by the daemon that
maintains them.

Typical ownership:

```text
auth.key                   lavalite:lavalite 644

var/state/mbd              lavalite:lavalite 750
var/state/mbd/eventlog     lavalite:lavalite 640
var/log                    lavalite:lavalite 755

var/state/sbd              root:root         750

bin/bhist                  root:lavalite     2755
```

## Authentication Key

LavaLite uses a shared authentication key to sign requests exchanged
between clients, `mbd`, and `sbd`.

```sh
dd if=/dev/urandom bs=32 count=1 | base64 \
    > /opt/lavalite/etc/auth.key
```

Set permissions:

```sh
chown lavalite:lavalite /opt/lavalite/etc/auth.key
chmod 644 /opt/lavalite/etc/auth.key
```

The key must be world readable because user commands and applications
linked against `libllbatch` generate HMAC signatures locally and must
be able to read it.

Copy the same file to every execution host.

## Configure LL_CONF_DIR

All commands and daemons require:

```sh
export LL_CONF_DIR=/opt/lavalite/etc
```

Most installations configure this through an environment module or shell
initialization script.

Verify:

```sh
echo $LL_CONF_DIR
```

## Create Configuration Files

See `02-configuration.md` for the complete reference.

Create `ll.conf`, `llb.queues`, and `llb.hosts` under `$LL_CONF_DIR`
before starting any daemon.

## Install systemd Units

Example systemd unit files are provided in `etc/` as a starting point
for sites that use systemd. Review and modify them according to your
site policy before installing.

```sh
cp /opt/lavalite/lib/systemd/lavalite-mbd.service \
    /etc/systemd/system/

cp /opt/lavalite/lib/systemd/lavalite-sbd.service \
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
