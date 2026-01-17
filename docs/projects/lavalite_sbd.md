# sbatchd (sbd) Protocol Model
## ACKs, Latches, and Restart-Safe Delivery

This document describes the **protocol and state model** used by the new
`sbatchd` (SBD) implementation in Lava Community Edition.

The design explicitly separates **durable protocol state** from
**in-memory latches**, and assumes **at-least-once delivery** with
**idempotent receivers**.

---

## 1. Design Principles

1. **ACK is truth**
   - Only `*_acked` flags represent durable protocol state.
   - Cleanup, state transitions, and pruning depend exclusively on ACKs.

2. **SENT is a latch**
   - `*_sent` flags mean *“attempted once in this daemon lifetime”*.
   - They are **not protocol state** and must never block retries.

3. **At-least-once delivery**
   - Messages may be resent after crashes, restarts, or lost packets.
   - Receivers (mbatchd) must be **idempotent**.

4. **Restart-safe by construction**
   - After restart, any unacked message must be eligible for resend.

5. **No implicit ordering assumptions**
   - Exit, FINISH, EXECUTE, and replies may become valid in unexpected orders.
   - Assertions must not encode timing assumptions.

---

## 2. Durable State vs Latches

### Durable (persisted in job record)

These survive restart and define protocol truth:

- `pid_acked`
- `reply_acked`
- `execute_acked`
- `finish_acked`
- `exit_status_valid`

### Latches (in-memory only)

These are optimizations to avoid busy loops:

- `reply_sent`
- `execute_sent`
- `finish_sent`
- `reply_last_send`
- `execute_last_send`
- `finish_last_send`

**Latches must never prevent resend when `*_acked == 0`.**

---

## 3. Restart Semantics

On sbatchd startup / replay:

```c
if (!job->reply_acked) {
    job->reply_sent = 0;
    job->reply_last_send = 0;
}

if (!job->execute_acked) {
    job->execute_sent = 0;
    job->execute_last_send = 0;
}

if (!job->finish_acked) {
    job->finish_sent = 0;
    job->finish_last_send = 0;
}
