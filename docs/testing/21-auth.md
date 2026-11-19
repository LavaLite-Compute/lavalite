# Authentication Tests

## Purpose

Verify the authentication of requests and daemon messages, and the
security invariant:

> A non-root user can obtain an authenticated request only for their
> own uid. They cannot obtain one for another uid, nor any request
> accepted as a daemon operation.

These tests cover:

- the two keys, `ll.auth.key` and `ll.daemon.key`, and their
  permissions, see *Authentication Keys* in the install guide
- `llsign`, the setgid helper that signs client requests, see
  `llsign(8)`
- the checks in `mbd` and `sbd`, see `lavalite(7)`

Root on a node is outside the invariant and is not tested.

## Prerequisites

- Environment validation completed.
- `etc/post-install.sh` run as root after `make install`.
- Two normal users, here `alice` and `bob`, not in the mbd group.
- At least one execution host, here `worker1`.
- Root on the master and on `worker1`, to change files and restart
  daemons.

Keep a copy of both keys before the tests that change them:

```sh
cp -p $LL_CONF_DIR/ll.auth.key $LL_CONF_DIR/ll.daemon.key /root/
```

## Tests

### TEST-312: Key and helper permissions

#### Commands

```sh
ls -l $LL_CONF_DIR/ll.auth.key $LL_CONF_DIR/ll.daemon.key
ls -l /opt/lavalite-<version>/libexec/llsign
ls -l /opt/lavalite-<version>/bin/bhist
```

#### Expected Result

```text
-rw-r-----  root     lavalite  ll.auth.key
-rw-------  lavalite lavalite  ll.daemon.key
-rwxr-sr-x  root     lavalite  libexec/llsign
-rwxr-sr-x  root     lavalite  bin/bhist
```

#### Pass Criteria

The client key is readable by the mbd group only, the daemon key by
the mbd user only, `llsign` and `bhist` are setgid to the mbd group.

### TEST-313: Users cannot read the keys

#### Commands

As `alice`:

```sh
cat $LL_CONF_DIR/ll.auth.key
cat $LL_CONF_DIR/ll.daemon.key
```

#### Expected Result

Both fail with `Permission denied`.

#### Pass Criteria

No user can read either key.

### TEST-314: Commands work for a normal user

#### Commands

As `alice`:

```sh
bqueues
bhosts
bsub sleep 60
bjobs
bkill <jobid>
bhist <jobid>
```

#### Expected Result

Every command works as before. `bjobs` and `bhist` show the job as
owned by `alice`.

#### Pass Criteria

Requests signed by `llsign` are accepted by `mbd`, with the uid of
the caller.

### TEST-315: A user cannot act on another user's job

#### Commands

As `bob`:

```sh
bsub sleep 300
```

As `alice`:

```sh
bkill <bob_jobid>
bmove -t <queue> <bob_jobid>
```

As `bob`:

```sh
bjobs <bob_jobid>
```

#### Expected Result

Both commands of `alice` are refused: `bkill: failed to signal job
<id>: Operation not permitted`, and the same reason from `bmove`.
The job of `bob` keeps running.

#### Pass Criteria

The uid `mbd` sees is the one of the user who ran the command.

#### Cleanup

```sh
bkill <bob_jobid>      # as bob
```

### TEST-316: A key of one's own is rejected

#### Commands

As `alice`:

```sh
mkdir ~/fake
cp $LL_CONF_DIR/ll.conf $LL_CONF_DIR/llb.* ~/fake/
head -c 32 /dev/urandom > ~/fake/ll.auth.key
LL_CONF_DIR=~/fake bqueues
grep 'failed validate' /opt/lavalite-<version>/var/log/mbd.log.* | tail -1
```

#### Expected Result

`bqueues` fails. The `mbd` log has
`failed validate message opcode=BATCH_QUEUE_INFO ... Permission denied`.

#### Pass Criteria

A request signed with any key but the cluster's is rejected. Pointing
`LL_CONF_DIR` elsewhere does not help: `llsign` signs with whatever
key it finds there, and `mbd` only accepts the real one.

#### Cleanup

```sh
rm -rf ~/fake
```

### TEST-317: llsign signs only well formed client requests

#### Commands

As `alice`:

```sh
L=/opt/lavalite-<version>/libexec/llsign
echo hello | $L; echo $?
head -c 64 /dev/zero | $L; echo $?
```

#### Expected Result

- The first prints `llsign: short request len=6` and exits 1.
- The second prints `llsign: operation 0 is not a client request`
  and exits 1.

Nothing is written on stdout.

#### Pass Criteria

`llsign` is not a general signing service: it refuses anything that
is not a client request. The list of client operations is compiled
in; `BATCH_SBD_REGISTER` and the other daemon operations are not on
it.

### TEST-318: Commands without a setgid llsign

#### Commands

As root:

```sh
chmod g-s /opt/lavalite-<version>/libexec/llsign
```

As `alice`:

```sh
bqueues
```

As root, restore:

```sh
$LL_CONF_DIR/../etc/post-install.sh
```

#### Expected Result

`bqueues` fails, after `llsign: sign: Permission denied`. After
`post-install.sh`, `bqueues` works again.

#### Pass Criteria

Without its setgid bit `llsign` cannot read the key, and the command
fails with the reason, it does not hang or crash.

### TEST-319: Daemons need the daemon key

#### Commands

As root on the master:

```sh
systemctl stop lavalite-mbd
mv $LL_CONF_DIR/ll.daemon.key /root/ll.daemon.key.off
systemctl start lavalite-mbd
systemctl status lavalite-mbd
journalctl -u lavalite-mbd | tail -3
mv /root/ll.daemon.key.off $LL_CONF_DIR/ll.daemon.key
systemctl start lavalite-mbd
```

Repeat on `worker1` with `lavalite-sbd`.

#### Expected Result

Without the key the daemon does not start, and the log says
`auth_init failed, daemon key ll.daemon.key: No such file or
directory`. With the key back it starts.

`mbd` also refuses to start without `ll.auth.key`, with
`auth_load_key failed, client key ll.auth.key`.

#### Pass Criteria

A daemon without its key stops at startup with a message naming the
missing file.

### TEST-320: An sbd with another daemon key cannot register

#### Commands

As root on `worker1`:

```sh
systemctl stop lavalite-sbd
cp -p $LL_CONF_DIR/ll.daemon.key /root/ll.daemon.key.good
head -c 32 /dev/urandom > $LL_CONF_DIR/ll.daemon.key
systemctl start lavalite-sbd
```

On the master:

```sh
bhosts worker1
grep 'failed validate message opcode=BATCH_SBD_REGISTER' \
    /opt/lavalite-<version>/var/log/mbd.log.* | tail -1
```

Restore on `worker1`:

```sh
systemctl stop lavalite-sbd
cp -p /root/ll.daemon.key.good $LL_CONF_DIR/ll.daemon.key
systemctl start lavalite-sbd
```

#### Expected Result

`worker1` does not register: `bhosts` does not show it `ok`, and the
`mbd` log has the rejected `BATCH_SBD_REGISTER`. The other hosts are
not affected. After the restore `worker1` registers again.

#### Pass Criteria

Only a host with the cluster's daemon key can register as an
execution host. A user, who has no daemon key, cannot pose as one.

### TEST-321: post-install.sh warns on extra group members

#### Commands

As root:

```sh
usermod -aG lavalite alice
$LL_CONF_DIR/../etc/post-install.sh
gpasswd -d alice lavalite
$LL_CONF_DIR/../etc/post-install.sh
```

#### Expected Result

The first run prints
`post-install: WARNING user alice is in group lavalite, it can sign
requests as any user`. The second run prints no warning.

#### Pass Criteria

`post-install.sh` reports anyone but the mbd user in the mbd group.

### TEST-322: Old messages are rejected

Optional: needs a submission host where neither `mbd` nor `sbd` runs,
here `login1`, whose clock can be moved. `faketime` does not work:
`llsign` is setgid and the loader ignores `LD_PRELOAD`.

#### Commands

As root on `login1`:

```sh
systemctl stop chronyd
date -s '-2 min'
su alice -c bqueues
chronyc makestep || systemctl start chronyd
```

On the master:

```sh
grep 'failed validate' /opt/lavalite-<version>/var/log/mbd.log.* | tail -1
```

#### Expected Result

`bqueues` fails, the `mbd` log has `failed validate message ...
Connection timed out`. With the clock back in sync it works.

#### Pass Criteria

A request older than `LL_AUTH_MAX_AGE`, 60 seconds by default, is
rejected. Keep the clocks of all hosts in sync.

## Completion Criteria

All tests pass. The keys and `llsign` are back to the permissions of
TEST-312 at the end.
