# LavaLite 2.0.1 Release Notes

## Overview

LavaLite 2.0.1 fixes the authentication of requests. In 2.0.0 the
shared key was readable by every user, and whoever could read it could
sign a request with any uid, or register as an execution host. 2.0.1
closes that: users can no longer read any key, and a user can obtain
a signed request only for their own uid.

Thanks to the E4 Computer Engineering team, who found it testing 2.0.0.

The release also has the fixes and the command line changes made since
2.0.0, listed below. **The protocol and the command line changed: read
Upgrading from 2.0.0.**

## Security

- **Two keys.** `ll.auth.key` signs client requests and is now
  readable only by the mbd group (`root:lavalite 0640`).
  `ll.daemon.key`, new, signs every `mbd` <-> `sbd` message, `sbd`
  registration included, and is readable only by the mbd user
  (`lavalite:lavalite 0600`), on hosts running `mbd` or `sbd` only.
- **`llsign`.** A small helper in `libexec`, setgid to the mbd group,
  signs every client request. It writes the caller's real uid and gid
  from the kernel and signs only client operations. See `llsign(8)`.
- **The whole message is signed.** The HMAC covers header and
  payload, computed over the bytes on the wire. In 2.0.0 it covered
  the header only.
- **The invariant.** A non-root user can obtain an authenticated
  request only for their own uid. They cannot obtain one for another
  uid, nor any request accepted as a daemon operation. Root on a node
  is outside it, as with ssh.

Only the mbd user may be in the mbd group: a member can read
`ll.auth.key` and sign as any user. `post-install.sh` warns about
other members.

## Bug Fixes

- **A dependency lost after a reject.** A job sent back to pending
  by an `sbd` reject or a missing report could lose the job it
  depended on to compaction and stay pending forever; its second
  dispatch also released the dependency count again. The dependency
  is now consumed at the first dispatch, once, and persisted.
- **Build with GCC 15.** GCC 15 defaults to C23; the XDR function
  pointers now have explicit prototypes.

## Commands

- `bjobs -m` / `--machines` shows the jobs running on the given hosts,
  `bhist -m` the jobs that were dispatched to them.
- Every option of every command has a short and a long form.
- `-V`, `--version` everywhere. **`-v` is gone** from `bsub`, `bjobs`,
  `bkill`, `bhosts`, `bqueues`, `bgroups`, `btokens`, `bmove`,
  `bpriority` and `bservices`.
- `bhist` gets `--long`, `--run`, `--pend`, `--machines`.
- Daemon options use dashes: **`mbd --sched-timer`**, **`sbd
  --op-timer`**, **`sbd --resend-timer`**. The underscore forms are
  gone.

## Upgrading from 2.0.0

2.0.1 does not talk to 2.0.0: upgrade every host, daemons and
commands, at the same time.

1. Stop `mbd` and every `sbd`.
2. Install 2.0.1 on every host.
3. Generate the daemon key once, on one host:

   ```sh
   dd if=/dev/urandom bs=32 count=1 2>/dev/null | base64 \
       > $LL_CONF_DIR/ll.daemon.key
   ```

   Copy it to every host running `mbd` or `sbd`, not to submission
   hosts. The existing `ll.auth.key` can stay; to be safe, generate a
   new one too, since every user could read the old one, and copy it
   to every host.
4. Run `post-install.sh` as root on every host. It sets `llsign`
   setgid and the owner and mode of both keys.
5. Replace `--sched_timer`, `--op_timer`, `--resend_timer` with the
   dash forms in systemd overrides and scripts, and `-v` with `-V`.
6. Start `mbd` and the `sbd`s. `mbd` does not start without both
   keys, `sbd` without the daemon key; the log names the missing file.

## Testing

- `docs/testing/21-auth.md`: keys, `llsign`, forged uids, another key,
  a fake `sbd`.
- `src/tests/sim/simqa.sh`, with requests signed by `llsign`.

## Known Limitations

- **Replay within the window.** A captured message can be sent again
  within `LL_AUTH_MAX_AGE`, 60 seconds. Capturing one needs root on a
  node or access to the network path.
- **One `llsign` per request.** Every client request runs `llsign`,
  about a millisecond. Invisible for a user; a program sending
  thousands of requests pays it each time.
- **Replies are not signed.** `mbd` replies to clients carry no HMAC.
- **Multi-host jobs start on the first host only**, as in 2.0.0.
