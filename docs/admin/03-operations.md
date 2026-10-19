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

`sbd` rejects a job only for its own failures, before the job
starts: it cannot create the job directories, write the job
script, set up the service namespace, or fork. The host is
then tainted for 60 seconds and receives no new jobs, so the
job is dispatched to another host. `bhosts` shows the host as
`ok|tainted`.

The taint clears by itself, or when `sbd` reconnects. A host
that keeps getting tainted has a local problem, such as a full
disk; check the `sbd` log on that host.

A launch failure is not considered a user job failure.

If the user command starts successfully and later exits with
a non-zero status, the job is reported as `EXIT`.

## Unresponsive Execution Hosts

When `mbd` dispatches a job, the job shows as `DISP` in `bjobs`
until `sbd` reports the pid of the started job. A healthy `sbd`
reports it within a few seconds and keeps resending it until
`mbd` acknowledges it.

If a connected `sbd` does not report the pid within 20 seconds,
`mbd` treats it as unresponsive and closes the connection:

- The host shows as `unavail` in `bhosts`.
- All jobs on the host show as `UNKNOWN` in `bjobs`.
- No job is killed or requeued.

`mbd` logs the event:

```
job=N no pid from sbd=HOST in S secs, disconnecting it
```

When `sbd` reconnects and registers again, the job state is
resynchronized:

- If the job is running, `sbd` reports the pid and the job
  becomes `RUN`.
- If `sbd` has no record of the job, the job returns to the
  pending queue and is dispatched again.

The usual cause is a network problem between `mbd` and the
execution host: messages lost in one direction are often lost
in the other as well. `sbd` may not see the disconnect at once.
It notices when its own sends fail, which depends on the TCP
retransmission and keepalive settings of the host.

If the host does not come back, check:

- Network connectivity between the `mbd` host and the
  execution host.
- `sbd` status on the execution host:

```sh
systemctl status lavalite-sbd
```

- The `mbd` and `sbd` logs.

Restarting `sbd` forces a new registration and resynchronizes
the jobs.

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

## Orphan Jobs

A job is an orphan when it references a queue, a service, a host or
a token pool that is no longer in the configuration. This happens when the
configuration is changed while the job is pending or running, and
`mbd` is restarted.

`mbd` replays the full history of the job and logs what is missing:

```
queue=QUEUE not in the config, stale
job=N state=RUN queue=QUEUE is orphan: it references a queue,
service or host not in the config, frozen until the config is fixed
```

An orphan is frozen:

- `bjobs` shows it as `ORPHAN`, with its queue and run host.
- It is not scheduled. It still holds what it held: a running orphan
  keeps its cpus, memory and tokens, and counts in its queue and on
  its hosts, as long as those are still in the configuration.
- `bmove` and `bpriority` are refused. `bkill` is refused except
  `bkill -s kill`, which ends the job for good, see below.
- Messages from `sbd` about the job are logged and not acknowledged.
  `sbd` keeps the job, and its exit status, and keeps reporting it.
- An `sbd` running on a host removed from the configuration cannot
  register.

Nothing about the job is lost. If the job is running, it keeps
running on its host.

To recover, restore the missing queue, service, host or token pool
in the configuration and restart `mbd`:

```sh
systemctl restart lavalite-mbd
```

The job replays as it was: pending, or running on its host with its
pid. `sbd` reports what happened in the meantime, for example that
the job finished, and the job continues normally.

If the missing queue, service, host or token pool is gone for good,
end the orphan instead:

```sh
bkill -s kill <jobid>
```

The job enters EXIT and releases what it held. If it was running on
a host that can still be reached, `sbd` kills it there.

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

