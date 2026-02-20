# lavalite_mbdsbd_protocol.md
# LavaLite mbatchd ↔ sbatchd Protocol and Job Lifecycle Pipeline

This document defines the authoritative protocol contract between:

- **mbatchd (MBD)** — scheduler and state authority
- **sbatchd (SBD)** — execution daemon

It describes:

- job lifecycle pipeline
- signal handling
- stage barrier semantics
- replay invariants
- idempotency guarantees

The code must reflect this document exactly.

---

# 1. Design Principles

The protocol is designed to guarantee:

- explicit state
- explicit lifecycle stages
- no implicit state derivation
- idempotent message handling
- replayable history via `lsb.events`
- strict pipeline barriers
- no generic status opcode

Each protocol message has a single meaning.
Each stage advancement is explicit.
There is no reconstructed state.

---

# 2. Layering Rules

The protocol implementation is strictly layered.

---

## 2.1 Wire Layer (`wire_*`)

Responsibility:
- Define protocol structures
- Perform XDR encode/decode
- Represent only on-the-wire format

Must NOT:
- Modify job state
- Log events
- Make scheduling decisions

It answers:

> What bytes go on the wire?

---

## 2.2 Logic Adapter Layer (`ll_*`)

Responsibility:
- Translate `wire_*` structures into daemon structures
- Perform structural validation and normalization

Must NOT:
- Mutate global job state
- Perform lifecycle transitions

It answers:

> How does this message map to internal structures?

---

## 2.3 Daemon Logic Layer (MBD / SBD)

Responsibility:
- Apply explicit job state changes
- Log lifecycle events
- Enqueue ACK barriers
- Enforce resend logic
- Maintain lifecycle invariants

Only this layer may change job state.

It answers:

> What does this message mean for the system?

---

# 3. Protocol Operations (Authoritative Enum)

```c
typedef enum  {
    BATCH_NEW_JOB,                 // MBD → SBD
    BATCH_NEW_JOB_REPLY,           // SBD → MBD
    BATCH_NEW_JOB_REPLY_ACK,       // MBD → SBD

    BATCH_JOB_EXECUTE,             // SBD → MBD
    BATCH_JOB_EXECUTE_ACK,         // MBD → SBD

    BATCH_JOB_FINISH,              // SBD → MBD
    BATCH_JOB_FINISH_ACK,          // MBD → SBD

    BATCH_JOB_SIGNAL,              // MBD → SBD
    BATCH_JOB_SIGNAL_REPLY,        // SBD → MBD

    BATCH_JOB_SIGNAL_MANY,         // future
    BATCH_JOB_SIGNAL_MANY_REPLY    // future

} mbd_sbd_ops_t;
```

---

# 4. Lifecycle Pipeline

The lifecycle consists of three strict stages.

Each stage forms a durability barrier.

---

## 4.1 Stage 1 — NEW_JOB

Flow:

1. MBD → SBD: BATCH_NEW_JOB
2. SBD → MBD: BATCH_NEW_JOB_REPLY
3. MBD → SBD: BATCH_NEW_JOB_REPLY_ACK

Meaning:

- BATCH_NEW_JOB dispatches a job.
- BATCH_NEW_JOB_REPLY confirms SBD accepted the job and returned PID.
- BATCH_NEW_JOB_REPLY_ACK confirms MBD committed and logged the stage.

Barrier rule:

SBD must not proceed to EXECUTE until it receives
BATCH_NEW_JOB_REPLY_ACK.

---

## 4.2 Stage 2 — EXECUTE (Virtual Stage)

Flow:

1. SBD → MBD: BATCH_JOB_EXECUTE
2. MBD → SBD: BATCH_JOB_EXECUTE_ACK

Meaning:

EXECUTE indicates that exec() occurred and execution metadata exists.

Important:

EXECUTE does NOT change job state.

It may update:
- PID
- PGID
- execution metadata
- timestamps

It never forces RUN.
It never overrides suspend.
It never overrides finish.

Barrier rule:

BATCH_JOB_EXECUTE_ACK confirms:
- metadata committed
- event logged
- resend timer cleared

---

## 4.3 Stage 3 — FINISH (Terminal Stage)

Flow:

1. SBD → MBD: BATCH_JOB_FINISH
2. MBD → SBD: BATCH_JOB_FINISH_ACK

Meaning:

Job has terminated.

MBD must:
- transition to DONE or EXIT
- log terminal event
- move job to finished list
- send BATCH_JOB_FINISH_ACK

Terminal rule:

DONE and EXIT dominate all future protocol messages.

---

# 5. Signal Pipeline

Flow:

1. MBD → SBD: BATCH_JOB_SIGNAL
2. SBD → MBD: BATCH_JOB_SIGNAL_REPLY

Signals express intent.

STOP:
- PEND → PSUSP
- RUN → USUSP

CONT:
- resume to previous baseline

TERM/KILL/INT:
- lead to FINISH stage

Signal intent and resulting state are logged separately.

---

# 6. Ordering Rules

The protocol must tolerate:

- duplicate messages
- out-of-order delivery
- resend after restart

Rules:

1. FINISH dominates everything.
2. EXECUTE never mutates lifecycle state.
3. Duplicate EXECUTE updates metadata only.
4. Duplicate FINISH is ignored after terminal.
5. Duplicate NEW_JOB_REPLY is safe.

---

# 7. Replay Invariants (lsb.events)

Replay algorithm:

1. Reconstruct job.
2. Apply explicit transitions in order.
3. DONE/EXIT dominate.
4. EXECUTE does not alter state.
5. Duplicate stages are ignored.

The event log is the source of truth.

---

# 8. Hard Invariants

- One opcode = one handler.
- Barrier ACK required for NEW_JOB_REPLY, EXECUTE, FINISH.
- EXECUTE is virtual.
- FINISH is terminal.
- No generic status message.
- No derived state reconstruction.
