# LavaLite Administrator's Guide — Troubleshooting

## sbd does not connect to mbd

- Check `LL_MBD_HOST` and `LL_MBD_PORT` in `ll.conf` on the execution host.
- Check firewall rules: mbd port must be reachable from all execution hosts.
- Check `auth.key` is identical on master and execution hosts and has mode 600.
- Check mbd is running: `systemctl status lavalite-mbd`.
- Check logs in `LL_LOG_DIR`.

## Jobs stay pending indefinitely

- Check `bhosts` — host must be in `ok` state, not `closed` or `unavail`.
- Check `bqueues` — queue must be `open`.
- Check the queue's `HOSTS` group includes the target host (`bgroup`).
- Check resource request: `--mem`, `--cpus`, `--gpus` must fit within what
  the host has available.
- Check token pools if the job uses `--pool`: `btokens` shows available tokens.

## Jobs fail immediately on dispatch

- Check sbd logs on the execution host.
- Check cgroup v2 is available: `mount | grep cgroup2`.
- Check `Delegate=yes` is set in `lavalite-sbd.service`.
- Check sbd is running as root.

## Authentication errors in logs

- Verify `auth.key` is identical on all hosts.
- Check clock skew: `chronyc tracking` on master and execution hosts.
  Skew must be below 300 seconds (default `LL_AUTH_MAX_AGE`).

## Enabling debug logging

Set in `ll.conf`:

```
LL_LOG_MASK=LOG_DEBUG
```

Restart the affected daemon. Revert to `LOG_WARNING` for production —
debug logging is verbose and will fill disk quickly under load.
