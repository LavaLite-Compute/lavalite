# LavaLite Administrator's Guide — Installation

## Requirements

- Linux x86_64, kernel 5.14 or later (Rocky/Alma 9 or equivalent)
- cgroup v2 enabled and mounted at `/sys/fs/cgroup`
- systemd
- OpenSSL (for HMAC-SHA256 authentication)
- NTP or chrony on all hosts (clock skew must stay below 300 seconds)

## Environment variables

`LL_CONF_DIR` must be set on every host before any daemon or command will
run. It points to the directory containing `ll.conf` and `auth.key`.
Typically set via an environment modules file, but any mechanism works.

## Directory layout

Install under a versioned directory with a stable symlink:

```
/opt/lavalite-0.9.0/      versioned installation
/opt/lavalite/            symlink -> lavalite-0.9.0
```

Subdirectories:

```
bin/          user commands (bsub, bjobs, bkill, ...)
sbin/         daemons (mbd, sbd)
etc/          configuration files
var/state/    persistent daemon state <--- must be owned by lavalite admin
var/log/      daemon log files <--- must be owned be the lavalite admin
```

The state and log directories must be owned by the lavalite admin uid.

## Shared filesystem

`ll.conf` is sourced at startup by all daemons and commands. It defines
`LL_STATE_DIR`, the path where mbd writes job state and history. This
directory must be accessible from the master host and any front-end nodes
where users run commands. Execution hosts do not need access.

In a typical HPC environment this is an NFS mount.

## Configuration

`make install` does not install configuration files. You must create them
manually:

```
cp <srcdir>/etc/ll.conf.example /opt/lavalite/etc/ll.conf
```

See the Configuration Guide for details.

## System user

Create a dedicated user for mbd on the master host:

```
useradd -r -s /sbin/nologin lavalite
```

sbd runs as root to manage cgroups and job execution on behalf of any user.

## Build from source

```
./configure --prefix=/opt/lavalite-0.9.0
make
make install
ln -sfn /opt/lavalite-0.9.0 /opt/lavalite
```

## Authentication key

Generate the shared HMAC key and set permissions:

```
dd if=/dev/urandom bs=32 count=1 | base64 > /opt/lavalite/etc/auth.key
chmod 644 /opt/lavalite/etc/auth.key
chown lavalite:lavalite /opt/lavalite/etc/auth.key
```

Copy `auth.key` to the same path on every execution host.

## systemd service units

```
cp /opt/lavalite/lib/systemd/lavalite-mbd.service /etc/systemd/system/
cp /opt/lavalite/lib/systemd/lavalite-sbd.service /etc/systemd/system/
systemctl daemon-reload
```

Enable mbd on the master host:

```
systemctl enable --now lavalite-mbd
```

Enable sbd on each execution host:

```
systemctl enable --now lavalite-sbd
```
