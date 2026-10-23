# Configuration Changes and mbd Restart

## Purpose

Verify what happens when the administrator changes the configuration
and restarts `mbd`, including the edge cases: errors in the files and
capacity lowered under running jobs. Removed objects and orphan
jobs are covered by `18-robustness.md`.

The rule under test: **mbd stays up and creates all it can.** An
object with a configuration error is not created, the reason is
logged, everything else is loaded. Work already running is not lost.

## Prerequisites

- Administrator access: edit files in `$LL_CONF_DIR`, restart `mbd`.
- Access to the `mbd` log.
- Keep a copy of the original configuration and restore it after
  each test.

After every restart look for the summary line in the `mbd` log:

```text
conf_init: hosts=N queues=N groups=N services=N services_skipped=N
```

## TEST-300: Summary line

### Commands

Restart `mbd` with the normal configuration, then:

```sh
grep services_skipped <mbd log>
```

### Expected Result

The counts match `llb.hosts`, `llb.queues` and `llb.services`,
`services_skipped=0`.

### Pass Criteria

One line tells what was loaded.

---

## TEST-301: Service without QUEUE

### Setup

Add to `llb.services`:

```
Begin Service
SERVICE_NAME = noqueue
TYPE         = user
COMMAND      = sleep 3600
PORT         = 8888
End Service
```

### Commands

Restart `mbd`, then:

```sh
bservices
bservices -s noqueue
grep noqueue <mbd log>
```

### Expected Result

- `mbd` is running.
- The log has `service=noqueue missing QUEUE` and
  `service=noqueue not configured`; summary `services_skipped=1`.
- `bservices` does not list `noqueue`; starting it fails.
- All other services are listed and work.

### Pass Criteria

One bad service never stops `mbd` and never lands in the batch queue.

---

## TEST-302: Service with an unknown key or queue

### Setup

Two blocks: one with `COLOR = blue`, one with `QUEUE = nosuchqueue`.

### Commands

Restart `mbd`, check the log and `bservices`.

### Expected Result

Both skipped with their reason (`unknown service key=COLOR`,
`QUEUE=nosuchqueue not found`), `services_skipped=2`, `mbd` running.

### Pass Criteria

Same as TEST-301.

---

## TEST-303: Unknown host in a queue

### Setup

In `llb.queues` add a host not in `llb.hosts` to an existing queue:

```
HOSTS = worker4 worker5 ghost1
```

### Commands

Restart `mbd`, then:

```sh
grep ghost1 <mbd log>
bqueues -l <queue>
bsub -q <queue> sleep 10
```

### Expected Result

- Log: `queue=<queue> host=ghost1 not in llb.hosts, skipped`.
- `bqueues -l` lists only the real hosts.
- Jobs run as before.

### Pass Criteria

The queue keeps its valid hosts, `mbd` running.

---

## TEST-304: Queue with no valid host

### Setup

A queue whose `HOSTS` are all unknown:

```
Begin Queue
QUEUE_NAME  = broken
PRIORITY    = 5
HOSTS       = ghost2
End Queue
```

### Commands

Restart `mbd`, then:

```sh
bqueues -l broken
bsub -q broken sleep 10
bjobs -p
```

### Expected Result

- Log: `queue=broken has no valid host, it will not schedule`.
- `bqueues -l broken` shows `Hosts: none`.
- The job stays `PEND`, reason `no eligible hosts in queue`.

### Pass Criteria

The queue exists, schedules nothing, its jobs are visible and can be
killed.

### Cleanup

```sh
bkill <jobid>
```

---

## TEST-305: Duplicate host in a queue

### Setup

```
HOSTS = worker4 worker5 worker5
```

### Commands

Restart `mbd`, `grep "listed twice" <mbd log>`.

### Expected Result

`queue=<queue> host=worker5 listed twice in HOSTS, ignored`, no
`: Success` at the end of the line.

### Pass Criteria

Warning only, the queue works.

---

## TEST-306: Lower CPU under running jobs

### Setup

A host with `CPU = 2`. Submit two 1-CPU jobs to it:

```sh
bsub -m <host> sleep 3600
bsub -m <host> sleep 3600
```

Set `CPU = 1` for that host in `llb.hosts`, restart `mbd`. For the
strictest check set `LL_ASSERT_COUNTERS=1` in `ll.conf`.

### Commands

```sh
bhosts -l <host>
bjobs
bsub -m <host> sleep 60
bjobs -p
```

Kill one of the two running jobs, then the other, and watch the
pending one.

### Expected Result

- `mbd` restarts and stays up.
- `bhosts -l` shows `CPU: 2/1`: oversubscribed, the truth.
- The new job stays `PEND` until both old jobs are gone, then runs.

### Pass Criteria

Lowering capacity drains the host, nothing is killed, nothing new is
dispatched until it fits.

---

## TEST-307: Lower memory under running jobs

### Setup

As TEST-306 with `--mem 1000` jobs on a host with `MEM = 4G`, then
lower to `MEM = 1G`.

### Expected Result

`bhosts -l` shows `MEM_MB: 2000/1024`. A new `--mem 500` job pends
until enough memory is released.

### Pass Criteria

No huge free memory: the counter goes negative, it does not wrap.

---

## TEST-308: Lower GPUs under running jobs

### Setup

As TEST-306 with `--gpus 1` jobs on a host with `GPU_IDS = 0,1`,
then lower to `GPU_IDS = 0`.

### Expected Result

- `mbd` restarts (no assert).
- Log: `GPU count lowered under running jobs`.
- `bhosts -l` shows `GPU: 1/1`; no new GPU job until the host drains.

### Pass Criteria

`mbd` up, GPU jobs keep running.

---

## Completion Criteria

The configuration test set passes when:

- A bad service, an unknown queue host, a queue with no hosts and a
  duplicate host never stop `mbd`; each is logged with its reason.
- The summary line counts what was loaded and skipped.
- Lowering CPU, memory or GPUs under running jobs keeps `mbd` up and
  drains the host.
- Removing a service with live instances: see TEST-248 in
  `18-robustness.md`.
