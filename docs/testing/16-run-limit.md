# Run Limit, Deadline and DISP Tests

## Purpose

Verify the two time limits and the DISP display:

- `--terminate` is an absolute deadline, enforced by `sbd`
  (SIGUSR2, SIGKILL one minute later).
- `--time` is a run limit counted from the first start, enforced by
  `mbd` (SIGTERM, SIGKILL one minute later). Time spent pending or
  dispatched does not count.
- A job whose time ran out while pending is never dispatched, it
  goes to `EXIT`.
- `DISP` is shown by `bjobs` for a job dispatched to `sbd` that has
  not started yet (no pid).

## Prerequisites

- Environment validation completed.
- At least one execution host is available.
- Root access to the execution host for TEST-196.

Run limit timing is checked by `mbd` on every scheduler tick
(`--sched_timer`, default 2s): allow a few seconds of slack on every
expected time below.

## TEST-190: Run limit kills a running job

### Commands

```sh
bsub --time 1 sleep 3600

bjobs <jobid>
```

Wait about 70 seconds.

```sh
bjobs -a <jobid>
bhist -l <jobid>
```

### Expected Result

- The job runs for about one minute, then is signaled and exits.
- `bhist -l` shows `Run limit:    0:01` and a `Signal  signal: 15`
  event about 60 seconds after `Forked`.

### Pass Criteria

Final state is `EXIT`, signal 15 is logged at the limit.

---

## TEST-191: Run limit escalates to SIGKILL

### Commands

```sh
bsub --time 1 "sh -c 'trap \"\" TERM; sleep 3600'"
```

Wait about 130 seconds.

```sh
bhist -l <jobid>
```

### Expected Result

- `Signal  signal: 15` at the limit, ignored by the job.
- `Signal  signal: 9` one minute later.

### Pass Criteria

Final state is `EXIT` after the SIGKILL.

---

## TEST-192: Reject invalid run limit values

### Commands

```sh
bsub --time 0 sleep 1
bsub --time -5 sleep 1
bsub --time 1:xx sleep 1
bsub --time abc sleep 1
```

### Expected Result

Each command fails with `bsub: --time: invalid limit`.

### Pass Criteria

No job is submitted, exit status is non-zero.

---

## TEST-193: Run limit format

### Commands

```sh
bsub --time 90 sleep 1
bsub --time 1:30 sleep 1

bhist -l <jobid1>
bhist -l <jobid2>
```

### Expected Result

Both show `Run limit:    1:30`.

### Pass Criteria

`[hours:]minutes` is parsed, minutes may exceed 59.

---

## TEST-194: Pending time does not count

### Commands

```sh
bsub --hold --time 1 sleep 3600
```

Wait 90 seconds, then release it:

```sh
bkill --signal cont <jobid>
bjobs <jobid>
```

### Expected Result

The job dispatches and runs a full minute before being signaled.

### Pass Criteria

In `bhist -l` the `Signal` event is about 60 seconds after `Forked`,
not after `Submitted`.

---

## TEST-195: Deadline passed while pending

### Commands

Submit held with a deadline one minute from now:

```sh
bsub --hold --terminate $(date -d '+1 min' +%H:%M) sleep 3600
```

Wait until the deadline has passed, then release it:

```sh
bkill --signal cont <jobid>
sleep 5
bjobs -a <jobid>
bhist -l <jobid>
```

### Expected Result

- The job goes to `EXIT` without being dispatched.
- `bhist -l` has no `Dispatched` event.
- The `mbd` log shows `time expired while pending`.

### Pass Criteria

Final state `EXIT`, never `RUN`.

---

## TEST-196: DISP display

### Setup

Root on the execution host. Freeze `sbd` so it cannot answer the
dispatch:

```sh
kill -STOP $(pidof sbd)
```

### Commands

```sh
bsub --machines <host> sleep 60
sleep 5
bjobs <jobid>
```

Then thaw `sbd`:

```sh
kill -CONT $(pidof sbd)
sleep 3
bjobs <jobid>
```

### Expected Result

- While `sbd` is stopped the job shows `DISP`.
- After `SIGCONT` it shows `RUN`.
- `bjobs --run` lists the job in both cases.

### Pass Criteria

`DISP` is visible only between dispatch and fork.

### Cleanup

```sh
bkill --signal kill <jobid>
```

---

## TEST-197: Run limit survives mbd restart

### Commands

```sh
bsub --time 3 sleep 3600
```

After about one minute restart `mbd` (administrator), then wait.

### Expected Result

The job is signaled about 3 minutes after its `Forked` event, not 3
minutes after the `mbd` restart.

### Pass Criteria

`bhist -l` signal time minus fork time is about 180 seconds.

## Completion Criteria

The run limit test set passes when:

- `--time` kills a running job at the limit, SIGTERM then SIGKILL.
- Invalid limits are rejected by `bsub`.
- Pending and dispatch time do not count against the limit.
- A job out of time while pending exits without dispatch.
- `DISP` is shown between dispatch and fork.
- The limit survives an `mbd` restart.
