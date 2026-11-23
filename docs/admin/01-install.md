# LavaLite Administrator Guide — Installation

## Requirements

LavaLite requires:

- Linux x86_64 or aarch64
- Kernel 5.14 or later
- cgroup v2
- a service manager (systemd examples provided)
- OpenSSL 1.1.1 or later
- NTP or chrony

Verify cgroup v2:

```sh
mount | grep cgroup2
```

Expected output:

```text
cgroup on /sys/fs/cgroup type cgroup2 (...)
```

## Build Requirements

- A C compiler supporting C17: GCC 8 or later, or clang 6 or later.
  `configure` appends `-std=gnu17` to `CFLAGS`, so the language level
  does not depend on the compiler's default.
- autoconf, automake, libtool, pkg-config
- OpenSSL development headers
- libtirpc development headers
- pandoc, to build the man pages

Debian/Ubuntu:

```sh
apt install build-essential autoconf automake libtool pkg-config \
    libssl-dev libtirpc-dev pandoc
```

Rocky/RHEL:

```sh
dnf install gcc make autoconf automake libtool pkgconf-pkg-config \
    openssl-devel libtirpc-devel pandoc
```

If `pandoc` is not found on Rocky/RHEL, enable EPEL or CRB.

## Create the LavaLite Administrator Account

Create a dedicated, unprivileged account to run `mbd`. The examples
use the user and group `lavalite:lavalite`; another account may be used
if the configuration and file permissions are adjusted accordingly.

`sbd` runs as `root` because it manages cgroups and
executes jobs on behalf of cluster users.

## Build and Install

Build LavaLite as admin or ordinary user:

```sh
./bootstrap
./configure
make
sudo make install
```

Compiler flags are passed to `configure`, `-std=gnu17` is appended to
them:

```sh
./configure CFLAGS="-ggdb -O0 -Wall"
```

Do not override `CFLAGS` on the `make` command line, it replaces the
configured flags and drops `-std=gnu17`.

By default LavaLite is installed under a versioned directory, for example
`/opt/lavalite-<version>` (e.g. `/opt/lavalite-2.0.1`).

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

## Authentication Keys

Every request and every daemon message is signed with HMAC-SHA256 and
checked by its receiver. Two keys, 32 bytes each, in `$LL_CONF_DIR`:

| Key             | Signs                        | Owner and mode            | Hosts                       |
|-----------------|------------------------------|---------------------------|-----------------------------|
| `ll.auth.key`   | client requests to `mbd`     | `root:lavalite 0640`      | every host                  |
| `ll.daemon.key` | every `mbd` <-> `sbd` message | `lavalite:lavalite 0600`  | hosts running `mbd` or `sbd` |

Users cannot read either key. Their requests are signed by `llsign`,
a small helper installed setgid to the mbd group, see `llsign(8)`. It
puts the caller's real uid and gid in the request, from the kernel,
and signs only client requests.

The security invariant: a non-root user can obtain an authenticated
request only for their own uid. They cannot obtain one for another
uid, nor any request accepted as a daemon operation. Root on a node
is outside it, as with ssh.

Only the mbd user may belong to the mbd group: whoever is in it can
read `ll.auth.key` and sign requests as any user. `post-install.sh`
warns about other members.

Generate the keys once, on one host:

```sh
dd if=/dev/urandom of="$LL_CONF_DIR/ll.auth.key" \
    bs=32 count=1 status=none

dd if=/dev/urandom of="$LL_CONF_DIR/ll.daemon.key" \
    bs=32 count=1 status=none
```

Copy `ll.auth.key` to every host that runs a client or daemon.
Copy `ll.daemon.key` only to hosts running `mbd` or `sbd`; a
client-only submission host must not have it. Each key must be
identical on all hosts that use it. `post-install.sh` sets the
owner and mode of both files.

`mbd` does not start without both keys, `sbd` without the daemon key;
the log names the missing file.

Messages older than `LL_AUTH_MAX_AGE` seconds, default 60, are
rejected: keep the clocks of all hosts in sync with NTP or chrony.
Within that window a captured message can be sent again; capturing
one needs root on a node or access to the network path.

Every execution host must know the users' uids, from the same passwd,
LDAP or other name service as the submission hosts. A host where the
job's uid is unknown, or belongs to another user name than on the
submission host, rejects the job: it returns to pending, the host is
tainted for a while and the job runs elsewhere. The reason is in the
`sbd` log.

## Configuration Files

LavaLite configuration is stored in:

```text
ll.conf
llb.queues
llb.hosts
llb.services
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

### llb.services

`llb.services` defines the services users can start with
`bservices`: the command, the application port, the queue, the
restart policy, an optional run limit and an optional token variable.

See:

```text
man 5 llb.services
```

for the complete reference.

### Service Hosts

The execution hosts of a service queue need:

- `iproute2` (`ip`) and `nftables` (`nft`).
- IP forwarding, `net.ipv4.ip_forward = 1`: the endpoint traffic is
  forwarded from the host's interface to the instance's namespace.
- The service network `10.200.0.0/16` free on the host: each instance
  gets a `/30` from it, on a veth pair local to the host.
- The endpoint ports, 30000 and up, reachable from the login node.

Users start instances and connect to them from the login node, a host
that is not in the service queue: see Services in the queues, hosts
and jobs guide.

## Post-Install Setup

**This step is mandatory. The daemons require the runtime state
directories and permissions created by this step.**

The installed `etc/post-install.sh` sets up runtime directories and
permissions. Run it after installation, or perform the equivalent
steps manually.

```sh
export LL_CONF_DIR=/opt/lavalite-<version>/etc
/opt/lavalite-<version>/etc/post-install.sh
```

The script performs the following operations:

- Sets `var/state/mbd`, `var/state/sbd`, and `var/log` ownership
  to `lavalite:lavalite`
- Sets `bin/bhist` to `root:lavalite` mode `2755` (setgid)
- Sets `libexec/llsign` to `root:lavalite` mode `2755` (setgid)
- Sets `etc/ll.auth.key` to `root:lavalite` mode `0640` and
  `etc/ll.daemon.key` to `lavalite:lavalite` mode `0600`, if present
- Warns if a user other than the mbd user is in the mbd group

Run it again after every `make install`: install resets the owner and
mode of `bhist` and `llsign`.

### Why bhist is setgid

The event manifest under `var/state/mbd` is readable only by the mbd
user and group.

`bhist` runs setgid with the mbd primary group, allowing it to read the
event manifest and report only records visible to the calling user.

### Why llsign is setgid

`ll.auth.key` is readable only by the mbd group. `llsign` runs setgid
with that group, so it can sign a user's request with the key while
the user cannot read it. Setgid, not setuid root: its one privilege is
reading the key, a bug in it cannot give root on the host.

## Directory Layout

A typical installation contains:

```text
/opt/lavalite-<version>
├── bin
├── sbin
├── libexec
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
├── bservices
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

### Helpers

```text
libexec/
└── llsign      (setgid mbd_group after post-install)
```

### Configuration

```text
etc/
├── ll.conf
├── llb.queues
├── llb.hosts
├── llb.services
├── ll.auth.key
└── ll.daemon.key   (hosts running mbd or sbd only)
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
ll.auth.key                root:lavalite     640
ll.daemon.key              lavalite:lavalite 600

var/state/mbd              lavalite:lavalite 750
var/state/mbd/manifest     lavalite:lavalite 640
var/log                    lavalite:lavalite 755

var/state/sbd              root:root         755

bin/bhist                  root:lavalite     2755
libexec/llsign             root:lavalite     2755
```

## Create Configuration Files

See [Configuration Files](#configuration-files) above for the file
reference, or `man 5 ll.conf` / `man 5 llb.hosts` / `man 5 llb.queues`
for the complete per-file reference.

Create `ll.conf`, `llb.queues`, and `llb.hosts` under `$LL_CONF_DIR`
before starting any daemon.

## Install systemd Units

`make install` puts the units in `lib/systemd`. They are templates
for the site to adapt:

- Replace `<version>` in the paths with the installed tree, e.g.
  `/opt/lavalite-2.0.1`.
- `User=` in `lavalite-mbd.service` must be `LL_MBD_USER` of `ll.conf`.

```sh
cp /opt/lavalite-<version>/lib/systemd/lavalite-*.service \
    /etc/systemd/system/
# edit <version> and User= in /etc/systemd/system/lavalite-*.service
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

Simulated hosts, a debug tool, run as `sbd -s <name>` on their
`REAL_HOST`, by hand or with a copy of `lavalite-sbd-sim.service` per
sim.

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
