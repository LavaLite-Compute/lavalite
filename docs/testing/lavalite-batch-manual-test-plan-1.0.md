# LavaLite Batch Manual Test Plan 1.0

## Purpose

This document defines the initial manual test plan for the LavaLite batch project.

The scope is the basic batch flow:

- `bsub`
- `bkill` bkill has options to stop, resume and terminate jobs
- pending jobs
- held jobs
- running jobs
- suspended jobs
- mbd restart and event replay
- sbd unavailable/available transitions

This is the first manual validation pass before adding larger chaos/stress tests.

## Test Assumptions

The test environment has:

- `mbd` available
- `sbd` available
- LavaLite client commands installed:
  - `bsub`
  - `bkill`
  - `bjobs`
  - `bqueues`
  - `bhosts`
- A configured queue, for example `cpu`
- A configured host, for example `buntu24`
- Access to the mbd sysevents file
- Debug/assert counter checks enabled in mbd

Example job command:

```sh
sleep 86400
```

Long-running jobs are useful for interactive debugging because they keep the system in a stable state while daemons, log and events files are inspected. Short-running jobs are better suited for bulk testing.

Useful inspection commands:

```sh
bjobs -a
bqueues
bhosts
tail -f /path/to/sysevents
```

Replace `/path/to/sysevents` with the real LavaLite sysevents path.

## General Validation Rules

For every test:

1. Verify client command result.
2. Verify `bjobs -a` state.
3. Verify queue counters.
4. Verify host counters where applicable.
5. Verify sysevents entries.
6. Restart `mbd`.
7. Restart `sbd`
7. Verify replay restores the expected state.
8. Confirm no counter assertion fails.

The system should never produce negative counters.

Expected counter invariants:

```text
host.num_jobs >= 0
host.num_run  >= 0
host.num_susp >= 0

queue.num_jobs >= 0
queue.num_pend >= 0
queue.num_run  >= 0
queue.num_susp >= 0
```

---

# Test 1: Submit and Kill Pending Job While sbd Is Down

## Goal

Verify that a job submitted while sbd is unavailable remains pending, can be killed while pending, writes correct events, and is restored correctly after mbd restart.

## Setup

Stop sbd:

```sh
# stop or kill sbd
```

Confirm host is unavailable or no dispatch can occur. Hosts are in unaval state and job are in
JOB_UNKNOWN state.

```sh
bhosts
```

## Steps

Submit a job:

```sh
bsub sleep 86400
```

Record the job ID.

Verify job is pending:

```sh
bjobs -a
```

Kill the pending job:

```sh
bkill <job_id>
```

Verify job is no longer pending and appears finished/exit as expected:

```sh
bjobs -a
```

Check sysevents:

```sh
tail -n 50 /path/to/sysevents
```

Expected events include:

```text
JOB_NEW
JOB_FINISH or equivalent pending-kill/exit event
```

Restart mbd:

```sh
# restart mbd
```

Verify replay restores the final state:

```sh
bjobs -a
bqueues
bhosts
```

## Expected Result

- Job is submitted.
- Job remains pending because sbd is down.
- `bkill` terminates the pending job.
- Sysevents contain the submit and final state transition.
- After mbd restart, the killed job is not restored as pending.
- Queue pending counters are correct.
- No negative counters.
- No `mbd_assert_counters()` failure.

---

# Test 2: Submit Held Job and Kill It

## Goal

Verify that `bsub -H` creates a held job, writes correct events, can be killed, and replay restores the final state.

## Setup

sbd may be up or down. The job must remain held.

## Steps

Submit held job:

```sh
bsub -H sleep 86400
```

Record job ID.

Verify held/pending-suspended state:

```sh
bjobs -a
```

Check sysevents:

```sh
tail -n 50 /path/to/sysevents
```

Expected events include:

```text
JOB_NEW
```

The job should be marked held or pending-suspended depending on LavaLite state representation.

Kill the held job:

```sh
bkill <job_id>
```

Verify final state:

```sh
bjobs -a
```

Restart mbd:

```sh
# restart mbd
```

Verify replay:

```sh
bjobs -a
bqueues
bhosts
```

## Expected Result

- Job is created in held/pending-suspended state.
- Job does not dispatch.
- `bkill` removes/finishes the held job.
- Replay does not resurrect it as pending.
- Queue counters are correct.
- No negative counters.
- No counter assertion failure.

---

# Test 3: sbd Down, Submit Held Job, Stop and Resume Pending Job

## Goal

Verify pending-suspended state survives replay and can be resumed back to pending.

## Setup

Stop sbd:

```sh
# stop or kill sbd
```

Confirm dispatch cannot happen:

```sh
bhosts
```

## Steps

Submit held job:

```sh
bsub -H sleep 86400
```

Record job ID.

Stop the job:

```sh
bkill -s STOP <job_id>
```

Verify job is pending suspended:

```sh
bjobs -a
bqueues
```

Check sysevents:

```sh
tail -n 50 /path/to/sysevents
```

Restart mbd:

```sh
# restart mbd
```

Verify job is still pending suspended:

```sh
bjobs -a
bqueues
```

Resume the job:

```sh
bkill -s CONT <job_id>
```

Verify job returns to pending:

```sh
bjobs -a
bqueues
```

Restart mbd again:

```sh
# restart mbd
```

Verify job is still pending:

```sh
bjobs -a
bqueues
```

## Expected Result

- Job remains pending because sbd is down.
- `bkill -s STOP` moves it to pending suspended.
- Replay restores pending suspended state.
- `bkill -s CONT` moves it back to pending.
- Replay restores pending state.
- Queue `num_pend` and `num_susp` counters are correct.
- No host run/suspend counters are modified because the job never ran.
- No counter assertion failure.

---

# Test 4: Running Job, Stop It, Restart mbd, Kill Suspended Job

## Goal

Verify that a running job can be suspended, survives mbd restart, and can be killed while suspended.

## Setup

Start sbd:

```sh
# start sbd
```

Verify host is available:

```sh
bhosts
```

## Steps

Submit a job:

```sh
bsub sleep 86400
```

Record job ID.

Wait until job is running:

```sh
bjobs -a
bhosts
bqueues
```

Stop the running job:

```sh
bkill -s STOP <job_id>
```

Verify job is suspended:

```sh
bjobs -a
bhosts
bqueues
```

Check sysevents:

```sh
tail -n 80 /path/to/sysevents
```

Expected events include:

```text
JOB_NEW
JOB_START
JOB_FORK
JOB_EXECUTE
JOB_SIGNAL
```

Restart mbd:

```sh
# restart mbd
```

Verify job is still suspended after replay:

```sh
bjobs -a
bhosts
bqueues
```

Kill the suspended job:

```sh
bkill <job_id>
```

Verify final state:

```sh
bjobs -a
bhosts
bqueues
```

## Expected Result

- Job dispatches and runs.
- Host `num_jobs` and `num_run` increment.
- `bkill -s STOP` changes job to suspended.
- Host/queue run counters decrement and suspend counters increment as expected.
- mbd replay restores suspended state.
- `bkill` terminates the suspended job.
- Suspended counters decrement correctly.
- No negative counters.
- No counter assertion failure.

---

# Test 5: Running Job Normal Completion

## Goal

Verify the normal successful path from submit to run to finish.

## Setup

Start sbd:

```sh
# start sbd
```

Verify host is available:

```sh
bhosts
```

## Steps

Submit a short job:

```sh
bsub sleep 5
```

Record job ID.

Verify job runs:

```sh
bjobs -a
bhosts
bqueues
```

Wait for completion:

```sh
sleep 10
bjobs -a
bhosts
bqueues
```

Check sysevents:

```sh
tail -n 80 /path/to/sysevents
```

Expected events include:

```text
JOB_NEW
JOB_START
JOB_FORK
JOB_EXECUTE
JOB_FINISH
```

Restart mbd:

```sh
# restart mbd
```

Verify replay preserves completed state:

```sh
bjobs -a
bhosts
bqueues
```

## Expected Result

- Job runs normally.
- Job finishes successfully.
- Host resources are released.
- Host `num_jobs` and `num_run` return to previous values.
- Queue `num_jobs` and `num_run` return to previous values.
- Replay does not resurrect the job as running.
- No negative counters.
- No counter assertion failure.

---

# Sysevents Review Checklist

For each job, inspect the sysevents sequence.

Typical running job sequence:

```text
JOB_NEW
JOB_START
JOB_FORK
JOB_EXECUTE
JOB_FINISH
```

Typical killed pending job sequence:

```text
JOB_NEW
JOB_FINISH or pending-kill equivalent
```

Typical stopped running job sequence:

```text
JOB_NEW
JOB_START
JOB_FORK
JOB_EXECUTE
JOB_SIGNAL
```

When replaying, verify:

- `JOB_NEW` restores the job.
- `JOB_START` restores run host information.
- `JOB_EXECUTE` restores execution information.
- `JOB_SIGNAL` restores stop/kill intent where applicable.
- `JOB_FINISH` removes job from active accounting.

---

# Counter Validation Checklist

After every command and after every mbd restart:

```sh
bjobs -a
bqueues
bhosts
```

Validate:

```text
No negative host counters.
No negative queue counters.
Pending jobs are counted as pending.
Held/pending-suspended jobs are counted as suspended or PSUSP according to design.
Running jobs are counted as running.
Suspended running jobs are counted as suspended.
Finished jobs are not counted as active.
```

If `mbd_assert_counters()` fires, stop and inspect the last event transition.

---

# Initial Pass/Fail Criteria

The manual test pass succeeds when:

- All five tests pass.
- mbd can restart and replay state correctly.
- sbd down/up behavior is predictable.
- Pending, running, suspended, killed, and finished jobs have correct state.
- Sysevents are sufficient to rebuild mbd state.
- Host and queue counters remain consistent.
- No negative counters appear.
- No counter assertion fails.

The manual test pass fails when:

- A job is resurrected into the wrong state after replay.
- A killed job becomes pending/running again after restart.
- A suspended job becomes running unexpectedly after replay.
- Queue or host counters drift from real job lists.
- Any counter becomes negative.
- `mbd_assert_counters()` fails.
