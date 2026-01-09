# Early SBD↔MBD Resilience Testing Guide

This document describes the **first-phase testing plan** for validating the
new non-blocking SBD↔MBD connection logic and ack-driven retry pipeline.

The goal is **fast feedback on correctness under failure**, not performance
or long-term stability.

This is a living document and will be extended as behavior stabilizes.

---

## Scope and Goals

### In Scope
- Non-blocking SBD → MBD connect and reconnect
- Link loss detection and recovery
- Ack-driven retry of protocol stages:
  - PID snapshot (NEW_JOB reply)
  - EXECUTE
  - FINISH
- Correct job lifecycle progression despite transient MBD outages

### Explicitly Out of Scope (for now)
- Persistent job reattachment after SBD restart
- MBD-side idempotency and replay
- Performance benchmarking
- Multi-user / quota enforcement

---

## Test Environment (Phase 1)

### Recommended Minimal Setup
- 1× MBD (may include LIM)
- 1× submit host (bsub client)
- 3–5× SBD worker nodes

A 6-VM laptop setup is sufficient and preferred for initial debugging.

### Requirements
- Passwordless SSH between nodes
- Independent daemon restart (MBD/SBD)
- Centralized or per-node log directories
- Ability to block network traffic (iptables)

---

## Observability Requirements

Before running tests, ensure logs clearly show:

- MBD link state transitions:
  - connect attempt
  - connected
  - link down (with reason)
- Per-job protocol stages:
  - PID snapshot enqueue
  - PID ACK reception
  - EXECUTE enqueue / ACK
  - FINISH enqueue / ACK

Minimum requirement: logs alone must explain *why a job is stuck*.

---

## Phase 1 Test Cases

### Test 1: Happy Path (Baseline)

**Purpose:** Verify normal pipeline with no failures.

Steps:
1. Start all daemons cleanly
2. Submit 50–100 short jobs
3. Wait for completion

Expected:
- All jobs reach FINISH_ACKED
- No retries triggered
- No link-down events

---

### Test 2: MBD Restart During Dispatch

**Purpose:** Validate retry of PID snapshot and EXECUTE stages.

Steps:
1. Start all daemons
2. Begin submitting jobs in a loop
3. Restart MBD while jobs are being dispatched
4. Let the system stabilize

Expected:
- SBD detects link down
- `*_sent` flags reset for non-acked stages
- After reconnect, pending PID / EXECUTE messages are resent
- Jobs eventually reach FINISH_ACKED

---

### Test 3: MBD Restart During Job Finish

**Purpose:** Validate FINISH retry behavior.

Steps:
1. Submit longer-running jobs (e.g. `sleep 10–30`)
2. Restart MBD shortly before jobs finish
3. Allow reconnect

Expected:
- exit_status captured locally
- FINISH deferred while link is down
- FINISH resent after reconnect
- FINISH_ACKED received

---

### Test 4: Network Partition (iptables)

**Purpose:** Validate detection of silent link loss.

Steps:
1. Submit running jobs
2. Block SBD → MBD traffic using iptables
3. Wait for failure detection
4. Restore network connectivity

Expected:
- SBD detects link loss (EPOLLERR / RDHUP)
- Pending stages marked for resend
- After reconnect, pipeline resumes
- No job is permanently stuck

---

## Known Limitations (Current Phase)

- Jobs may be lost if SBD restarts while they are running
- Duplicate EXECUTE / FINISH may be resent after reconnect
- MBD idempotency is not yet enforced

These are expected and will be addressed in later phases.

---

## Next Steps

Planned extensions:
- MBD-side idempotent ACK handling
- Job reattachment after SBD restart
- Periodic resend (timeout-based) instead of reconnect-only
- Automated stress harness
- Cloud-based 10–20 node testing
