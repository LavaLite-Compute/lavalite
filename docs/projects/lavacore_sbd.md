# sbd (sbd) Protocol Model
## ACK-Driven, Snapshot-Based, Restart-Safe

This document describes the **protocol and state model** of the current
`sbd` (SBD) implementation in LavaCore Edition.

The design is fully ACK-driven, restart-safe by construction,
and explicitly separates:

- immutable wire input (`job->spec`)
- runtime state (`struct sbd_job`)
- durable protocol truth (ACK flags)

No implicit state machine enum is used.

---

## 1. Core Design Principles

### 1. Immutable Wire Snapshot

`job->spec` is a deep copy of `jobSpecs` received from mbd.

After `sbd_job_create()`:

- `job->spec` is **never mutated**
- it represents the original dispatch snapshot from mbd
- it is not a runtime scratchpad

Fields such as `jobPid`, `jobPGid`, `jStatus` inside `spec`
must not be rewritten by sbd.

---

### 2. Runtime State Lives in `struct sbd_job`

Mutable execution state is stored only in `struct sbd_job`:

- `pid`
- `pgid`
- decoded `exec_cwd`
- ACK flags
- resend timestamps
- exit status
- finish time

There is no separate `job->state` enum.
Pipeline progression is derived from facts + ACK gates.

---

### 3. ACK Is Truth

The only durable protocol truth is represented by:

- `pid_acked`
- `execute_acked`
- `finish_acked`
- `exit_status_valid`

All pruning, cleanup, and progression decisions depend exclusively on these.

If an ACK is not set, the corresponding message must be eligible for resend.

There are no `*_sent` latches.

---

### 4. At-Least-Once Delivery

All protocol messages are sent with at-least-once semantics.

Resend is driven by:

- ACK flag == false
- resend interval timeout

mbd must be idempotent.

Duplicate NEW_JOB replies, EXECUTE events, or FINISH events
must not corrupt global state.

---

### 5. Message Reconstruction (No Reply Cache)

SBD does not cache serialized reply structures.

All outbound protocol messages are rebuilt from authoritative runtime state.
No serialized reply is stored for later reuse.

At dispatch time, SBD derives a **runtime job specification**
(`run_spec`) from the immutable wire `spec`. The wire `spec`
represents the original mbd snapshot and is never mutated.

The runtime spec contains only execution-relevant fields:

- execUid
- execUsername
- execHome
- execCwd (decoded absolute path)

Only the runtime spec is persisted and used for:

- resend
- restart recovery
- message reconstruction

The wire `spec` is immutable input and may be discarded once
the runtime spec has been fully derived.

Outbound messages are rebuilt as follows:

- NEW_JOB reply:
  - jobId
  - pid
  - pgid
  - jStatus (explicitly set by SBD, never taken from spec)

- EXECUTE:
  - pid
  - pgid
  - execUid (from runtime spec)
  - execUsername (from runtime spec)
  - execHome (from runtime spec)
  - execCwd (from runtime spec)

- FINISH:
  - exit_status
  - timestamps
  - resource usage snapshot

We use the wire snapshot only once: at job creation time, to derive the runtime
spec (exec user/home/uid and the decoded absolute cwd).

After that, all outbound messages are rebuilt only from runtime state (including
the runtime spec), never from the wire snapshot.

Runtime state is the single source of truth.

---

## 2. Execution Field Model

### Decoding

`sbd_job_create()` performs decoding:

- `spec.cwd` may be empty, relative, or absolute
- `spec.subHomeDir` is assumed sanitized
- `exec_cwd` is reconstructed as an absolute path

After decoding:

- `exec_cwd` is always absolute and non-empty
- `spec.cwd` is never used for execution

### Persistence

The job record must persist:

- pid
- pgid
- exec_uid (or spec.userId)
- exec_username (or spec.userName)
- exec_home (spec.subHomeDir)
- exec_cwd (decoded absolute path)
- ACK flags
- exit_status_valid
- exit_status
- finish time (if available)

`spec.cwd` is not persisted.

The decoded execution path is authoritative.

---

## 3. Restart Semantics

On sbd restart:

1. Job records are loaded.
2. ACK flags define protocol truth.
3. No handshake with mbd is required.
4. Resend logic resumes automatically.

Resend eligibility is derived from:

- `!pid_acked`
- `!execute_acked`
- `exit_status_valid && !finish_acked`

No in-memory "sent" flags exist.

Timeout-based resend uses:

- `reply_last_send`
- `execute_last_send`
- `finish_last_send`

These are optimization timestamps only.
They do not define protocol truth.

---

## 4. Pipeline Without Explicit State

There is no `SBD_JOB_RUNNING` or similar enum.

Pipeline stage is derived:

- `pid >= 0` → process spawned
- `pid_acked` → PID snapshot committed in mbd
- `execute_acked` → EXECUTE committed
- `exit_status_valid` → waitpid captured exit
- `finish_acked` → FINISH committed

This removes redundant state and prevents drift.

---

## 5. Distributed State Model

mbd:
- Owns global event log (source of truth)
- Commits events after ACK

sbd:
- Owns execution reality (pid, cwd, exit status)
- Guarantees at-least-once delivery

There is no shared mutable state.
There is no implicit synchronization.
There is no handshake-based recovery.

State converges via idempotent replay.

---

## 6. Invariants

After `sbd_job_create()`:

- `job->spec` is immutable
- `exec_cwd` is absolute and non-empty
- `pid == -1`
- all ACK flags == false

At runtime:

- Messages must never be gated by "sent" flags
- Only ACK flags prevent resend
- Message payloads are reconstructed from runtime truth

---

## 7. Philosophy

- Facts, not flags
- ACKs, not assumptions
- Rebuild, don’t cache
- Immutable input, mutable runtime
- Idempotent distributed protocol

The system must tolerate:

- sbd crash
- mbd crash
- network partition
- duplicate delivery
- restart at any point in the pipeline

Without requiring coordination or out-of-band repair.
