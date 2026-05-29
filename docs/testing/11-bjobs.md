# bjobs Tests

## Purpose

Verify that `bjobs` displays batch jobs correctly and reports the
user-visible job states documented for LavaLite 1.0.

This test file focuses only on job visibility and filtering. Job
submission is covered in `10-bsub-basic.md`. Job control operations are
covered in `13-bkill.md`.

## Prerequisites

- Environment validation completed.
- At least one queue is open.
- At least one execution host is available.
- The test user can submit jobs.
- The commands `bsub`, `bjobs`, and `bkill` are available in `PATH`.

## Job State Summary

`bjobs` may display the following states:

- `PEND` — waiting to be dispatched
- `HELD` — held and not eligible for dispatch
- `RUN` — executing on a host
- `SUSP` — suspended
- `DONE` — completed successfully
- `EXIT` — completed with a non-zero exit status

## TEST-110: Display active jobs

### Commands

```sh
bsub sleep 3600
bjobs
```

### Expected Result

- `bsub` returns a valid job ID.
- `bjobs` displays the submitted job.
- The job state is either `PEND` or `RUN`.

Example:

```text
JOBID  USER   STAT  QUEUE  PRIO  FROM_HOST  EXEC_HOST  JOB_NAME  SUBMIT_TIME
123    david  RUN   cpu    30    buntu24    sim1       -         May 29 16:10
```

### Pass Criteria

The submitted job appears in `bjobs`.

### Cleanup

```sh
bkill --signal kill <jobid>
```

---

## TEST-111: Display a specific job

### Commands

```sh
bsub sleep 3600
bjobs <jobid>
```

### Expected Result

Only the specified job is displayed.

### Pass Criteria

`bjobs <jobid>` displays the requested job and does not display unrelated
jobs.

### Cleanup

```sh
bkill --signal kill <jobid>
```

---

## TEST-112: Display HELD jobs

### Commands

```sh
bsub --hold sleep 3600
bjobs
```

### Expected Result

The submitted job is displayed with state `HELD`.

Example:

```text
JOBID  USER   STAT  QUEUE  PRIO  FROM_HOST  EXEC_HOST  JOB_NAME  SUBMIT_TIME
124    david  HELD  cpu    30    -          -          -         May 29 16:15
```

### Pass Criteria

The held job appears in `bjobs` with state `HELD`.

### Cleanup

```sh
bkill --signal kill <jobid>
```

---

## TEST-113: Display RUN jobs

### Commands

```sh
bsub sleep 3600
bjobs --run
```

### Expected Result

The submitted job is displayed with state `RUN`.

If the job is still `PEND`, wait for dispatch and run `bjobs --run`
again.

### Pass Criteria

The running job appears in `bjobs --run`.

### Cleanup

```sh
bkill --signal kill <jobid>
```

---

## TEST-114: Display SUSP jobs

### Commands

```sh
bsub sleep 3600
bkill --signal stop <jobid>
bjobs
```

### Expected Result

The submitted job is displayed with state `SUSP`.

### Pass Criteria

The suspended job appears in `bjobs` with state `SUSP`.

### Cleanup

```sh
bkill --signal kill <jobid>
```

---

## TEST-115: Display DONE jobs

### Commands

```sh
bsub /bin/true
bjobs --done
```

### Expected Result

The submitted job eventually appears with state `DONE`.

If the job is not visible immediately, run:

```sh
bjobs --done
```

again after the scheduler has processed completion.

### Pass Criteria

The completed job appears in `bjobs --done` with state `DONE`.

---

## TEST-116: Display EXIT jobs

### Commands

```sh
bsub /bin/false
bjobs --done
```

### Expected Result

The submitted job eventually appears with state `EXIT`.

If the job is not visible immediately, run:

```sh
bjobs --done
```

again after the scheduler has processed completion.

### Pass Criteria

The failed job appears in `bjobs --done` with state `EXIT`.

---

## TEST-117: Display pending jobs with reasons

### Setup

Create a job that cannot be dispatched immediately. One way is to request
resources that are not available in the test cluster.

### Commands

```sh
bsub --gpus 999 sleep 3600
bjobs --pend
```

### Expected Result

The submitted job is displayed with state `PEND`.

A pending reason is displayed.

### Pass Criteria

`bjobs --pend` displays the pending job and includes a pending reason.

### Cleanup

```sh
bkill --signal kill <jobid>
```

---

## TEST-118: Verify default user filtering

### Commands

```sh
bjobs
```

### Expected Result

Only jobs belonging to the current user are displayed.

### Pass Criteria

No jobs owned by other users are displayed.

---

## TEST-119: Verify administrator all-user view

### Commands

```sh
bjobs --all
```

### Expected Result

All active jobs are displayed.

This command may require administrator privileges.

### Pass Criteria

`bjobs --all` succeeds for an administrator user and displays jobs from
all users when such jobs exist.

---

## TEST-120: Verify invalid job ID handling

### Commands

```sh
bjobs 99999999
```

### Expected Result

`bjobs` reports that the job was not found and exits with a non-zero
status.

### Pass Criteria

The command fails cleanly and does not crash.

## Completion Criteria

The `bjobs` test set passes when:

- Active jobs are visible.
- Specific job lookup works.
- `HELD`, `RUN`, `SUSP`, `DONE`, and `EXIT` states are displayed
  correctly.
- Pending jobs can be shown with pending reasons.
- Invalid job IDs are handled cleanly.
