# bpriority Tests

## Purpose

Verify that `bpriority` changes job scheduling priority and rejects
invalid priority changes.

## Prerequisites

- Environment validation completed.
- At least one queue is open.
- The test user can submit jobs.
- The commands `bsub`, `bjobs`, `bhist`, `bpriority`, and `bkill` are
  available in `PATH`.

## TEST-140: Lower priority of a held job

### Commands

```sh
bsub --hold sleep 3600

bjobs <jobid>

bpriority --priority 10 <jobid>

bjobs <jobid>
```

### Expected Result

- The job is submitted successfully.
- The job is in state `HELD`.
- `bpriority` succeeds.
- The job priority changes to `10`.

### Pass Criteria

`bjobs <jobid>` shows priority `10`.

### Cleanup

```sh
bkill --signal kill <jobid>
```

---

## TEST-141: Lower priority of a pending job

### Setup

Create a job that remains pending.

Example:

```sh
bsub --gpus 999 sleep 3600
```

### Commands

```sh
bjobs --pend

bpriority --priority 10 <jobid>

bjobs <jobid>
```

### Expected Result

- The job is in state `PEND`.
- `bpriority` succeeds.
- The job priority changes to `10`.

### Pass Criteria

`bjobs <jobid>` shows priority `10`.

### Cleanup

```sh
bkill --signal kill <jobid>
```

---

## TEST-142: Reject priority increase above queue priority for normal user

### Commands

```sh
bsub --hold sleep 3600

bpriority --priority 9999 <jobid>
```

### Expected Result

`bpriority` fails because a normal user cannot raise a job priority
above the queue priority.

### Pass Criteria

- The command exits with non-zero status.
- A useful error message is displayed.
- The job priority remains unchanged.

### Cleanup

```sh
bkill --signal kill <jobid>
```

---

## TEST-143: Allow administrator to raise priority

### Commands

```sh
bsub --hold sleep 3600

bpriority --priority 9999 <jobid>
```

### Expected Result

When run by an administrator, `bpriority` succeeds.

### Pass Criteria

`bjobs <jobid>` shows the new priority.

### Cleanup

```sh
bkill --signal kill <jobid>
```

---

## TEST-144: Reject priority change for finished job

### Commands

```sh
bsub /bin/true

bhist <jobid>

bpriority --priority 10 <jobid>
```

### Expected Result

`bpriority` fails because finished jobs cannot be reprioritized.

### Pass Criteria

- The command exits with non-zero status.
- A useful error message is displayed.

---

## TEST-145: Reject unknown job ID

### Commands

```sh
bpriority --priority 10 99999999
```

### Expected Result

`bpriority` fails because the job does not exist.

### Pass Criteria

- The command exits with non-zero status.
- A useful error message is displayed.

---

## TEST-146: Reject invalid priority value

### Commands

```sh
bsub --hold sleep 3600

bpriority --priority -1 <jobid>
```

### Expected Result

`bpriority` fails because priority must be a non-negative integer.

### Pass Criteria

- The command exits with non-zero status.
- A useful error message is displayed.
- The job priority remains unchanged.

### Cleanup

```sh
bkill --signal kill <jobid>
```

---

## TEST-147: Verify priority is visible in job history

### Commands

```sh
bsub --hold sleep 3600

bpriority --priority 10 <jobid>

bhist <jobid>
```

### Expected Result

`bhist` displays the updated job priority.

If priority change events are logged, the history also includes the
priority change event.

### Pass Criteria

The updated priority is visible through job inspection.

### Cleanup

```sh
bkill --signal kill <jobid>
```

---

## TEST-148: Verify scheduling order by priority

### Setup

This test requires at least two pending jobs competing for the same
resources.

Create two held jobs in the same queue:

```sh
bsub --hold --name low_priority sleep 3600
bsub --hold --name high_priority sleep 3600
```

Set different priorities:

```sh
bpriority --priority 10 <low_jobid>
bpriority --priority 20 <high_jobid>
```

Release both jobs:

```sh
bkill --signal cont <low_jobid>
bkill --signal cont <high_jobid>
```

### Expected Result

When only one job can be dispatched, the higher-priority job is selected
first.

### Pass Criteria

The job with priority `20` dispatches before the job with priority `10`.

### Cleanup

```sh
bkill --signal kill <low_jobid>
bkill --signal kill <high_jobid>
```

## Completion Criteria

The `bpriority` test set passes when:

- Job priority can be lowered.
- Pending and held jobs can be reprioritized.
- Invalid priority values are rejected.
- Unknown job IDs are rejected.
- Finished jobs cannot be reprioritized.
- Normal users cannot raise priority above the queue priority.
- Administrators can raise priority when permitted.
- Scheduling order follows priority when jobs compete for the same
  resources.
