# lavalite_job_signalling.md

Status: draft (WIP)
Last updated: 2026-01-12

## Purpose

Define a simple, daemon-grade job signalling path for LavaLite, starting with `bkill`,
then extending to `bstop`/`bresume`.

Goals:
- Make `bkill` useful early for testing sbatchd/mbatchd behavior.
- Keep the wire protocol small and explicit (no “signal mask array” legacy).
- Be restart-friendly: pending actions must survive daemon restarts and replay.
- Keep behavior deterministic and easy to reason about.

Non-goals (for now):
- Full LSF compatibility of exotic signal features.
- Complex signal batching/masks, per-host fanout, or “action scripts”.
- Scheduling logic changes.

---

## User-facing interface

### bkill
We start with a minimal and explicit interface:

- `bkill <jobid>` → default action: terminate (KILL)
- `bkill -s <name> <jobid>` → explicit action

Accepted signal names (initial set):
- `kill`   → terminate job
- `stop`   → stop/suspend job
- `cont`   → continue/resume job

Notes:
- We accept a string on the CLI for usability.
- We do NOT send the raw string over the wire (see protocol).

CLI string mapping:
- `kill` → `LL_ACT_KILL`
- `stop` → `LL_ACT_STOP`
- `cont` → `LL_ACT_CONT`

Future (not now):
- support numeric POSIX signals (e.g. `-s 9`)
- support `term` vs `kill` separation
- support “force” modes with timeouts

---

## Design principles

1. **Explicit action enum on the wire**
   - Avoid string parsing/versioning in XDR.
   - Avoid legacy signal masks/arrays.

2. **Idempotent semantics**
   - Re-sending the same action must be safe.
   - Useful for restart/replay and flaky networks.

3. **mbatchd is the source of truth**
   - mbatchd persists “pending action” intent.
   - sbatchd executes, replies with ACK.
   - mbatchd clears pending action only after ACK.

4. **Prefer signalling process groups**
   - Jobs often spawn children; signalling only the wrapper PID is insufficient.
   - We should signal PGID where available.

---

## Wire protocol

### Operation
Use a dedicated opcode (name may vary depending on existing enum layout):

- `BATCH_JOB_SIG` (or `BATCH_JOB_ACTION`)

Request payload:
- job_id (int64)
- action (enum)
- request_id / sequence (already in packet header)

Reply payload:
- job_id (int64)
- action (enum)
- status code (int32)
- optional errno-like field (int32) for debugging

### Action enum

```c
typedef enum {
    LL_ACT_KILL = 1,  /* terminate */
    LL_ACT_STOP = 2,  /* suspend */
    LL_ACT_CONT = 3,  /* resume */
} ll_job_action_t;
