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

Create a dedicated account for running `mbd`:

```sh
useradd -r -s /sbin/nologin lavalite
```

Verify:

```sh
id lavalite
```

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

Create a stable installation symlink:

```sh
ln -sfn /opt/lavalite-1.0.0 /opt/lavalite
```

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
```

### Commands

```text
bin/
├── bsub
├── bjobs
├── bhist
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

### Runtime Data

```text
var/
├── log
└── state
```

## Runtime State

Scheduler state is stored under:

```
var/state
```

## Daemon working directory and core file

```
mkdir /var/log/lavalite
chown -R lavalite:lavalite  /var/log/lavalite
```

Core files are unfortunate but essential debug tool should something
go unexpectedly wrong. The daemons will create a core file in this directory.
Make sure you have relative path settings like:

```
sysctl -w kernel.core_pattern=core.%e.%p
```
Use this command to inspect the current settings:

```
sysctl kernel.core_pattern
```

### Master State

```text
var/state/mbd/
├── eventlog
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

## Ownership and Permissions

The installation tree should be readable by all users.

Runtime state and log directories must be writable by the daemon that
maintains them.

Typical ownership:

```text
auth.key                   lavalite:lavalite 600

var/state/mbd             lavalite:lavalite
var/log/mbd.log*          lavalite:lavalite

var/state/sbd             root:root
var/log/sbd.log*          root:root
```

The authentication key must not be readable by unprivileged users.

## Authentication Key

LavaLite uses a shared authentication key to sign requests exchanged
between clients, `mbd`, and `sbd`.

```sh
dd if=/dev/urandom bs=32 count=1 | base64 \
    > /opt/lavalite/etc/auth.key
```

Set permissions:

```sh
chmod 644 /opt/lavalite/etc/auth.key
chown lavalite:lavalite /opt/lavalite/etc/auth.key
```

Copy the same file to every execution host.

LavaLite authenticates requests at the API layer. User commands and applications
linked against `libllbatch` generate authenticated requests directly.

```text
Applications
    |
    +-- bsub
    +-- bjobs
    +-- bhist
    +-- bkill
    +-- ...
    |
libllbatch
    |
    +-- HMAC(auth.key)
    |
mbd
```

Because client applications generate request signatures locally, the
authentication key must be readable by users.

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

Create:

```text
ll.conf
llb.queues
llb.hosts
```

under:

```text
$LL_CONF_DIR
```

Configuration details are described in:

```text
02-configuration.md
```

## Install systemd Units

Install service files:

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
