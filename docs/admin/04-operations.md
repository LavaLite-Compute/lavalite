# LavaLite Administrator's Guide — Operations

## Starting and stopping

Start mbd on the master host:

```
systemctl start lavalite-mbd
```

Start sbd on each execution host:

```
systemctl start lavalite-sbd
```

Stop mbd (sbd instances will lose connection and reconnect when mbd restarts):

```
systemctl stop lavalite-mbd
```

Stop sbd (running jobs are NOT killed — they continue running and are
detected on sbd restart):

```
systemctl stop lavalite-sbd
```

## Checking status

```
systemctl status lavalite-mbd
systemctl status lavalite-sbd
bhosts
bqueues
bjobs --all
```

## Log files

Daemon logs are written to `LL_LOG_DIR`. Log verbosity is controlled by
`LL_LOG_MASK` in `ll.conf`.

For debugging, set `LL_LOG_MASK=LOG_DEBUG` and restart the daemon.
For production, `LOG_WARNING` is recommended.

## Event log and compaction

mbd writes a persistent event log to `LL_STATE_DIR/mbd/job.events`.
This file grows as jobs are submitted and finished. Compaction runs
automatically when the file reaches `LL_EVENTS_MAX_SIZE`. Finished jobs
older than `LL_EVENTS_RETAIN` are removed from the log at compact time.

Job history and usage sidecars are stored under:

```
LL_STATE_DIR/mbd/jobs/<bucket>/<jobid>/
    submit      job submission parameters
    usage       resource usage at job finish
```

## sbd restart and job recovery

sbd is designed to restart without losing running jobs. On restart, sbd
reconnects to mbd and resyncs job state. Jobs that finished while sbd
was down are detected and reported. Do not change `KillMode` in the
service unit.

## Chaos testing

Two test scripts are provided for validation before production deployment:

**chaos** — injects mbd and sbd restarts under load and verifies that
running jobs complete correctly across daemon failures.

**pchaos** — stress tests the pending job cycle: submits a large number
of jobs and verifies they all dispatch and finish correctly without
loss or duplication.

Run both during staging on a test cluster before deploying to production.
