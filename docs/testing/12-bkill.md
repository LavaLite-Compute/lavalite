# bkill Tests

## Purpose

Verify that `bkill` correctly delivers signals to jobs and that the
resulting state transitions are visible through `bjobs` and `bhist`.

## Prerequisites

- Environment validation completed.
- At least one execution host is available.
- The test user can submit and control jobs.

## TEST-120: Kill a running job

### Commands

```sh
bsub sleep 3600

bjobs

bkill --signal kill <jobid>

bjobs <jobid>
```

### Expected Result

The job is terminated.

### Pass Criteria

The job no longer appears as `RUN`.

Verify with:

```sh
bhist <jobid>
```

The final state is `EXIT`.

---

## TEST-121: Kill a pending job

### Setup

Create a job that cannot dispatch.

Example:

```sh
bsub --gpus 999 sleep 3600
```

### Commands

```sh
bjobs --pend

bkill --signal kill <jobid>
```

### Expected Result

The pending job is removed from the active queue.

### Pass Criteria

The job no longer appears in:

```sh
bjobs
```

and `bhist` reports state `EXIT`.

---

## TEST-122: Kill a held job

### Commands

```sh
bsub --hold sleep 3600

bjobs

bkill --signal kill <jobid>
```

### Expected Result

The held job is terminated.

### Pass Criteria

The job leaves state `HELD`.

Verify:

```sh
bhist <jobid>
```

shows final state `EXIT`.

---

## TEST-123: Suspend a running job

### Commands

```sh
bsub sleep 3600

bkill --signal stop <jobid>

bjobs <jobid>
```

### Expected Result

The job enters state `SUSP`.

### Pass Criteria

`bjobs` reports:

```text
STAT=SUSP
```

---

## TEST-124: Resume a suspended job

### Setup

Use a suspended job from TEST-123.

### Commands

```sh
bkill --signal cont <jobid>

bjobs <jobid>
```

### Expected Result

The job leaves state `SUSP`.

### Pass Criteria

The job returns to state `RUN`.

---

## TEST-125: Release a held job

### Commands

```sh
bsub --hold sleep 3600

bjobs <jobid>

bkill --signal cont <jobid>

bjobs <jobid>
```

### Expected Result

The job leaves state `HELD`.

### Pass Criteria

The job becomes eligible for dispatch.

The job eventually reaches state:

```text
RUN
```

or

```text
PEND
```

depending on resource availability.

### Cleanup

```sh
bkill --signal kill <jobid>
```

---

## TEST-126: Send SIGHUP

### Commands

```sh
bsub sleep 3600

bkill --signal hup <jobid>
```

### Expected Result

The signal is accepted and delivered.

### Pass Criteria

The command succeeds.

Verify resulting behavior using:

```sh
bjobs <jobid>
bhist <jobid>
```

---

## TEST-127: Send numeric signal

### Commands

```sh
bsub sleep 3600

bkill --signal 1 <jobid>
```

### Expected Result

The signal is accepted.

### Pass Criteria

The command succeeds and the job reacts according to the signal.

### Cleanup

```sh
bkill --signal kill <jobid>
```

---

## TEST-128: Multiple job IDs

### Commands

```sh
bsub sleep 3600
bsub sleep 3600
bsub sleep 3600

bkill --signal kill <jobid1> <jobid2> <jobid3>
```

### Expected Result

All specified jobs receive the signal.

### Pass Criteria

All jobs leave state `RUN`.

---

## TEST-129: Invalid signal name

### Commands

```sh
bkill --signal banana 123
```

### Expected Result

The command fails.

### Pass Criteria

A useful error message is displayed and the command exits with non-zero
status.

---

## TEST-140: Invalid job ID

### Commands

```sh
bkill --signal kill 99999999
```

### Expected Result

The command fails.

### Pass Criteria

A useful error message is displayed and the command exits with non-zero
status.

## Completion Criteria

The `bkill` test set passes when:

- Running jobs can be terminated.
- Pending jobs can be terminated.
- Held jobs can be terminated.
- Running jobs can be suspended.
- Suspended jobs can be resumed.
- Held jobs can be released.
- Multiple job IDs are handled correctly.
- Invalid signals are rejected.
- Invalid job IDs are handled cleanly.
