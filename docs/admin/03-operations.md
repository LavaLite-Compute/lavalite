# LavaLite Administrator Guide — Operations

## Overview

This chapter describes the day-to-day operation of a LavaLite cluster.

Typical administrative tasks include:

- Starting daemons
- Stopping daemons
- Restarting daemons
- Verifying cluster health
- Inspecting logs
- Verifying scheduler recovery

## Service Units

LavaLite does not require a specific service manager configuration.

By default, both daemons run in the foreground and write log messages to
standard error.

Examples:

```sh
mbd
```

```sh
sbd
```

This behavior is useful during development, testing, and debugging.

Example systemd service files are provided in:

```text
<srcdir>/etc
```

These files are intended as examples and a starting point for site
integration.

Administrators may customize them as required by local operational
policies.

Typical deployments use:

```sh
systemctl start lavalite-mbd
systemctl start lavalite-sbd
```

but alternative service management approaches are equally valid.

## Starting the Scheduler

Start the master daemon:

```sh
systemctl start lavalite-mbd
```

Verify:

```sh
systemctl status lavalite-mbd
```

Expected result:

```text
active (running)
```

## Starting Execution Hosts

Start the slave daemon on each execution host:

```sh
systemctl start lavalite-sbd
```

Verify:

```sh
systemctl status lavalite-sbd
```

Expected result:

```text
active (running)
```

## Stopping the Scheduler

Stop the master daemon:

```sh
systemctl stop lavalite-mbd
```

Effects:

- New requests cannot be processed.
- Running jobs continue executing on execution hosts.
- `sbd` instances automatically reconnect when `mbd` returns.

## Stopping an Execution Host

Stop an execution daemon:

```sh
systemctl stop lavalite-sbd
```

Effects:

- Running jobs are not terminated.
- Job execution continues.
- Job state is recovered when `sbd` restarts.

## Restarting the Scheduler

Restart the master daemon:

```sh
systemctl restart lavalite-mbd
```

After restart:

- Event replay reconstructs scheduler state.
- Connected execution hosts reconnect automatically.
- Pending and running jobs become visible again.

Verify:

```sh
bjobs --all
```

## Restarting an Execution Host

Restart the execution daemon:

```sh
systemctl restart lavalite-sbd
```

After restart:

- Running jobs are rediscovered.
- Job state is resynchronized with `mbd`.
- Dispatch resumes normally.

## Job Launch Failures

If `sbd` cannot start a job before the user payload begins
execution, the job is automatically returned to the pending
queue.

This allows the scheduler to retry the job on a subsequent
dispatch attempt.

A launch failure is not considered a user job failure.

If the user command starts successfully and later exits with
a non-zero status, the job is reported as `EXIT`.

## Cluster Health Checks

### Verify Daemon Status

```sh
systemctl status lavalite-mbd
systemctl status lavalite-sbd
```

### Verify Host Visibility

```sh
bhosts
```

Verify:

- Expected hosts are visible.
- Hosts are in state `ok`.

### Verify Queue Visibility

```sh
bqueues
```

Verify:

- Expected queues are visible.
- Queues are open.

### Verify Job Visibility

```sh
bjobs --all
```

Verify:

- Running jobs are visible.
- Pending jobs are visible.

## Log Files

Log files are written under:

```text
LL_LOG_DIR
```

Typical files:

```text
mbd.log
sbd.log
```

View recent messages:

```sh
tail -100 ${LL_LOG_DIR}/mbd.log
tail -100 ${LL_LOG_DIR}/sbd.log
```

Follow logs in real time:

```sh
tail -f ${LL_LOG_DIR}/mbd.log
```

### Debug Logging

Increase log verbosity:

```sh
LL_LOG_MASK=LOG_DEBUG
```

Restart the affected daemon.

Return to normal operation:

```sh
LL_LOG_MASK=LOG_WARNING
```

Debug logging should normally be enabled only during investigation.

## Persistent State

Persistent scheduler state is stored under:

```text
LL_STATE_DIR
```

The scheduler reconstructs state from persistent files during startup.

Administrative operations should never modify files in:

```text
LL_STATE_DIR
```

while daemons are running.

## Event Replay

The scheduler applies state changes first, then records them as
durable events. This keeps the event manifest consistent with state
that was actually applied.

During startup, `mbd` replays these events and reconstructs scheduler
state.

Administrators can validate recovery by:

```sh
systemctl restart lavalite-mbd

bjobs --all
bhist
```

Verify that jobs remain visible after restart.

## Recovery Validation

A basic recovery test:

1. Submit a long-running job.
2. Verify the job is running.
3. Restart `mbd`.
4. Verify the job remains visible.
5. Restart `sbd`.
6. Verify the job remains visible.
7. Verify the job completes successfully.

Example:

```sh
bsub sleep 3600

systemctl restart lavalite-mbd

bjobs

systemctl restart lavalite-sbd

bjobs
```

## Recommended Operational Checks

Daily:

```sh
bhosts
bqueues
bjobs --all
```

Weekly:

```sh
bhist
```

Review:

- Failed jobs
- Queue utilization
- Host utilization

Before production deployment:

```text
docs/testing/
```

Execute:

```text
30-mbd-restart.md
31-sbd-restart.md
32-event-replay.md
```

to validate recovery behavior.
