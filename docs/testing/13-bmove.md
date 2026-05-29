# bmove Tests

## Purpose

Verify that `bmove` moves pending and held jobs between queues and
rejects invalid move requests.

## Prerequisites

- Environment validation completed.
- At least two queues are configured.
- The test user can submit jobs to both queues used in this test.
- The commands `bsub`, `bjobs`, `bhist`, `bmove`, and `bkill` are
  available in `PATH`.

The examples below use queues named:

```text
cpu
low
```

If the test cluster uses different queue names, replace them with valid
queue names from:

```sh
bqueues
```

## TEST-140: Move a held job to another queue

### Commands

```sh
bsub --queue cpu --hold sleep 3600

bjobs <jobid>

bmove --to low <jobid>

bjobs <jobid>
```

### Expected Result

- The job is submitted to queue `cpu`.
- The job is in state `HELD`.
- `bmove` succeeds.
- The job remains `HELD`.
- The queue changes from `cpu` to `low`.

### Pass Criteria

`bjobs <jobid>` shows the moved job in the destination queue.

### Cleanup

```sh
bkill --signal kill <jobid>
```

---

## TEST-131: Move a pending job to another queue

### Setup

Create a job that remains pending.

One way is to request resources that are not available in the test
cluster:

```sh
bsub --queue cpu --gpus 999 sleep 3600
```

### Commands

```sh
bjobs --pend

bmove --to low <jobid>

bjobs <jobid>
```

### Expected Result

- The job is in state `PEND`.
- `bmove` succeeds.
- The job remains pending unless resources become available.
- The queue changes from `cpu` to `low`.

### Pass Criteria

`bjobs <jobid>` shows the moved job in the destination queue.

### Cleanup

```sh
bkill --signal kill <jobid>
```

---

## TEST-132: Move a held job and then release it

### Commands

```sh
bsub --queue cpu --hold sleep 3600

bmove --to low <jobid>

bkill --signal cont <jobid>

bjobs <jobid>
```

### Expected Result

- The held job is moved to queue `low`.
- The job is released.
- The job becomes eligible for dispatch in the destination queue.

The final state may be:

```text
PEND
```

or:

```text
RUN
```

depending on resource availability.

### Pass Criteria

The job is no longer in state `HELD` and remains associated with the
destination queue.

### Cleanup

```sh
bkill --signal kill <jobid>
```

---

## TEST-133: Reject moving a running job

### Commands

```sh
bsub --queue cpu sleep 3600

bjobs <jobid>

bmove --to low <jobid>
```

### Expected Result

`bmove` fails because running jobs cannot be moved.

### Pass Criteria

- The command exits with non-zero status.
- A useful error message is displayed.
- The job remains in its original queue.

### Cleanup

```sh
bkill --signal kill <jobid>
```

---

## TEST-134: Reject moving a finished job

### Commands

```sh
bsub --queue cpu /bin/true

bhist <jobid>

bmove --to low <jobid>
```

### Expected Result

`bmove` fails because finished jobs cannot be moved.

### Pass Criteria

- The command exits with non-zero status.
- A useful error message is displayed.
- The job remains associated with its original queue.

---

## TEST-135: Reject move to unknown queue

### Commands

```sh
bsub --queue cpu --hold sleep 3600

bmove --to does_not_exist <jobid>
```

### Expected Result

`bmove` fails because the destination queue does not exist.

### Pass Criteria

- The command exits with non-zero status.
- A useful error message is displayed.
- The job remains in its original queue.

### Cleanup

```sh
bkill --signal kill <jobid>
```

---

## TEST-136: Reject unknown job ID

### Commands

```sh
bmove --to low 99999999
```

### Expected Result

`bmove` fails because the job does not exist.

### Pass Criteria

- The command exits with non-zero status.
- A useful error message is displayed.

---

## TEST-137: Verify move is visible in job history

### Commands

```sh
bsub --queue cpu --hold sleep 3600

bmove --to low <jobid>

bhist <jobid>
```

### Expected Result

`bhist` displays the job with the destination queue.

If move events are logged, the history also includes the queue move
event.

### Pass Criteria

The job history reflects the destination queue.

### Cleanup

```sh
bkill --signal kill <jobid>
```

## TEST-138: Reject move to unauthorized queue

### Setup

Create a queue that does not allow the current user.

Example:

```text
QUEUE_NAME = restricted
USERS = alice bob
```

Submit a held job:

```sh
bsub --queue cpu --hold sleep 3600
```

### Commands

```sh
bmove --to restricted <jobid>
```

### Expected Result

`bmove` fails because the user is not authorized to submit jobs to the
destination queue.

### Pass Criteria

- The command exits with non-zero status.
- A useful error message is displayed.
- The job remains in the original queue.
- The job state is unchanged.

Verify:

```sh
bjobs <jobid>
```

still shows the original queue.

### Cleanup

```sh
bkill --signal kill <jobid>
```

## Completion Criteria

The `bmove` test set passes when:

- Held jobs can be moved.
- Pending jobs can be moved.
- Moved held jobs can be released.
- Running jobs cannot be moved.
- Finished jobs cannot be moved.
- Unknown destination queues are rejected.
- Unknown job IDs are handled cleanly.
- The moved queue is visible through `bjobs` and `bhist`.
