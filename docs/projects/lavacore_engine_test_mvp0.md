# Lava Compute Engine – MVP Test Plan (v0.1.0)

## Introduction

This document defines the validation plan for the v0.1.0 MVP.
The goal is to verify signal semantics, state transitions, restart determinism,
multi-host correctness, and stress stability before tagging the release.

## Engineering Invariants

The following invariants must hold at all times and across daemon restarts:

1. **State Exclusivity**
   - A job must have exactly one primary state at any time
     (PEND, PSUSP, RUN, USUSP/SSUSP, EXIT, DONE).
   - No illegal state combinations.

2. **Deterministic Restart**
   - After mbatchd or sbatchd restart, job state must remain identical.
   - Replay must not introduce new transitions.
   - No unintended ZOMBI resurrection.

3. **Signal Idempotency**
   - Repeated STOP/CONT/KILL must not create duplicate transitions.
   - No-op signals must not emit JOB_STATUS events.

4. **Domain Separation**
   - Pending-domain reasons must never mutate EXIT semantics.
   - EXIT/DONE reason fields must not affect state during replay.
   - Generally reason fields must not implicitly mutate job state during replay.

5. **Transport vs Semantic Errors**
   - LSBE_* codes represent semantic result.
   - -1 is reserved for transport/internal failure only.

6. **No Silent State Mutation**
   - State transitions must originate only from explicit actions
     (signal, scheduler decision, daemon event).

7. **Eventually Terminal Lifecycle**
   - Under stress, all jobs must eventually reach EXIT or DONE.
   - No job may remain indefinitely in intermediate state.

## Scope

This document validates:

- Signal semantics
- State machine correctness
- Restart stability
- Multi-host behavior
- Basic stress integrity

---

### A. Pending Job Tests
### B. Running Job Tests
### C. Finished Job Tests
### D. Restart Stability
### E. Multi-Host
### F. Stress

---

# A. Pending Job Tests

## A1 – Submit Held Job (Atomic PSUSP)

Replace `JOBID` with the real numeric job id.

Command:
bsub -H -o /dev/null sleep 86400

Verify:
bjobs JOBID

Expected:
- STAT=PSUSP
- No EXEC_HOST

---

## A2 – Resume Held Job (PSUSP → PEND)

Command:
bkill -s CONT JOBID

Verify:
bjobs JOBID

Expected:
- STAT=PEND

---

## A3 – STOP on Plain PEND (PEND → PSUSP)

Precondition:
Job is STAT=PEND

Command:
bkill -s STOP JOBID

Expected:
- STAT=PSUSP

---

## A4 – Duplicate STOP on PSUSP

Precondition:
Job is STAT=PSUSP

Command:
bkill -s STOP JOBID

Expected:
- LSBE_JOB_SUSP
- State remains PSUSP
- No duplicate JOB_STATUS event

---

## A5 – CONT on Plain PEND (No-op)

Precondition:
Job is STAT=PEND

Command:
bkill -s CONT JOBID

Expected:
- Still STAT=PEND
- No newstatus event

---

## A6 – Kill Pending Job

Command:
bkill -s KILL JOBID

Expected:
- Transition to EXIT/DONE
- No EXEC_HOST
- Restart-safe (remains EXIT after mbatchd restart)

---

## A7 – HUP on Pending Job

Command:
bkill -s HUP JOBID

Expected:
- EXIT
- No ZOMBI after restart

---

## A8 – Unsupported Numeric Signal

Command:
bkill -s 23 JOBID

Expected:
- Signal rejected
- No state change
- No JOB_STATUS event

---

# B. Running Job Tests

## B1 – Submit Running Job

Command:
bsub -o /dev/null sleep 86400

Wait until:
bjobs JOBID

Expected:
- STAT=RUN
- EXEC_HOST populated

---

## B2 – STOP Running Job (RUN → USUSP)

Command:
bkill -s STOP JOBID

Expected:
- STAT=USUSP

---

## B3 – CONT Running Job (USUSP → RUN)

Command:
bkill -s CONT JOBID

Expected:
- STAT=RUN

---

## B4 – Duplicate CONT on RUN

Command:
bkill -s CONT JOBID

Expected:
- No crash
- State remains RUN
- No duplicate JOB_STATUS event

---

## B5 – Kill Running Job

Command:
bkill -s KILL JOBID

Expected:
- EXIT/DONE
- Restart-safe

---

# C. Finished Job Tests

## C1 – Signal Finished Job

Precondition:
Job is DONE or EXIT

Command:
bkill -s STOP JOBID

Expected:
- LSBE_JOB_FINISH
- No crash
- No state mutation

---

# D. Restart Stability

Repeat representative scenarios from A and B, but:

- Restart mbatchd during:
  - RUN
  - USUSP
  - EXIT
- Restart sbatchd during RUN
- Restart both daemons

Expected:
- No state corruption
- No unexpected ZOMBI
- No UNKNOWN status
- No duplicate transitions

---

# E. Multi-Host Validation

Environment:
- 1 master host
- 4–5 execution hosts (VMs)

Tests:

1. Submit jobs until multiple hosts are utilized.
2. STOP/CONT/KILL jobs on different execution hosts.
3. Restart one execution host while jobs are RUN.
4. Restart mbatchd during active workload.

Expected:
- Correct host tracking
- No lost jobs
- No phantom RUN jobs
- No replay corruption

---

# F. Basic Stress Validation (1000 Jobs)

Submit mixed workload:

- 700 × sleep 1
- 200 × sleep 5
- 100 × sleep 30

During execution:

- STOP 20 random jobs
- CONT 20 random jobs
- KILL 20 random jobs
- Restart mbatchd once
- Restart one sbatchd once

Expected:

- No deadlocks
- No daemon crash
- No memory growth anomaly
- No ZOMBI regression
- Final job counts consistent
- All jobs eventually EXIT/DONE

---

# Transport Stability Checks

During all tests verify:

- No core dumps
- No unintended disconnects
- No UNKNOWN opcode logs
- LSBE_* codes represent semantic result
- -1 only used for transport/internal failures
- No duplicate JOB_STATUS for no-op actions

---

# MVP Acceptance Criteria

The following must be true before tagging v0.1.0:

- Signal semantics stable
- Restart replay deterministic
- No ZOMBI regression
- Multi-host behavior correct
- 1000-job stress stable
