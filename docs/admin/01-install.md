# LavaLite Administrator's Guide — Installation

## Requirements

- Linux x86_64, kernel 5.14 or later (Rocky/Alma 9 or equivalent)
- cgroup v2 enabled and mounted at `/sys/fs/cgroup`
- systemd
- OpenSSL (for HMAC-SHA256 authentication)
- NTP or chrony on all hosts (clock skew must stay below 300 seconds)

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
var/state/    persistent daemon state
var/log/      daemon log files
```

## Shared filesystem

`LL_STATE_DIR` must be accessible from the master host and any front-end
nodes where users run commands. Execution hosts do not need access.

In a typical HPC environment this is an NFS mount. The master host writes
job state and history to `LL_STATE_DIR`; front-end nodes read it for
commands like `bhist`.

`LL_CONF_DIR` must be accessible from all hosts — master, execution, and
front-end. It contains `ll.conf` and `auth.key`.

## System user

Create a dedicated user for mbd on the master host:

```
useradd -r -s /sbin/nologin lavalite
```

sbd runs as root.

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
chmod 600 /opt/lavalite/etc/auth.key
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
