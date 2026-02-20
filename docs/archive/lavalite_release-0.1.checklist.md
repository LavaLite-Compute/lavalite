# LavaLite MVP Release Checklist (Multi-Host, No HA)

This document defines the **Minimum Viable Product (MVP)** release scope,
acceptance criteria, and the required test gates.

The MVP goal is simple:

> LavaLite can submit, schedule, start, signal, and finish jobs across multiple hosts
> using sbatchd + mbatchd + lim, with explicit state transitions and stable transport.

This MVP is intended for **small clusters** and **early adopters**.

---

## 0. MVP Scope

### In scope (MVP)

- Multi-host operation:
  - lim master + multiple execution hosts
  - sbatchd runs on execution hosts
  - mbatchd runs on the lim/master host
- Core job lifecycle:
  - submit
  - hold/resume (PSUSP ↔ PEND)
  - start (PEND → RUN)
  - stop/continue for running jobs (RUN ↔ USUSP)
  - kill (KILL → DONE/EXIT)
  - completion and correct final state
- Transport stability:
  - no disconnect storms
  - no protocol opcode errors
  - retries/timers behave correctly
- Installation via `make install`

### Explicitly NOT in scope (MVP)

- Job arrays
- Failover / HA for lim or mbatchd
- Advanced queue policies, fairshare, preemption, backfill
- Advanced authentication enforcement (eauth is present but not fully integrated)
- Large-scale performance guarantees (1000+ nodes / huge throughput)
- Full packaging (RPM/DEB) – MVP is source install

---

## 1. Deployment Model (MVP)

### Roles

- **LIM master host**
  - runs: `lim`, `mbatchd`
- **Execution hosts**
  - run: `sbatchd` (and lim slave if applicable)

### Decoupling rule

- sbatchd must be runnable and restartable independently on execution hosts.
- mbatchd is started manually on the lim master host.
- No failover: if the lim master host is down, the cluster is down (MVP limitation).

---

## 2. Installation (MVP)

### Build and install

- `./configure ...`
- `make -j`
- `make install`

### Runtime prerequisites

- Working host resolution (canonical names)
- Shared paths configured if required for job spools/logs
- Required ports open between master and execution hosts
- Time reasonably in sync between nodes (NTP recommended)

---

## 3. Startup Order (MVP)

1. Start lim on all nodes (master elected or configured).
2. Start mbatchd on the lim master host.
3. Start sbatchd on each execution host.
4. Verify connectivity and that execution hosts appear runnable.

---

## 4. Acceptance Criteria (Release Gates)

MVP can be released only if **all gates** pass.

### Gate A — Signal semantics test plan (single host)

Run the full test plan:

- Pending job signals (PSUSP/PEND)
- Running job signals (RUN/USUSP)
- Finished job behavior
- Transport stability checks

Pass criteria:
- All expected states match
- No crashes
- No unintended disconnects
- Error codes are semantic (LSBE_*), and -1 is reserved for internal/transport failures

### Gate B — Multi-host scheduling and execution

#### B1 — Multi-host placement

Commands (examples):
- Submit N long jobs and confirm they spread across hosts:
  - `bsub -o /dev/null sleep 300`
  - repeat until several hosts are used

Verify:
- `bjobs -l JOBID` shows EXEC_HOST populated correctly
- Different jobs run on different execution hosts when capacity exists
- No job enters RUN unless the target host has an active connected sbatchd

#### B2 — Multi-host signaling correctness

Pick jobs on different execution hosts and run:
- STOP → USUSP
- CONT → RUN
- KILL → DONE/EXIT

Verify:
- state transitions are correct
- the correct execution host is targeted
- no cross-host confusion (wrong job/wrong host)
- no orphan jobs left running after mbatchd believes they are dead

### Gate C — Restart sanity (minimal)

This is a **minimum** restart gate, not full HA.

#### C1 — Restart sbatchd on an execution host

- Run a job on host X
- Restart sbatchd on host X
- Verify:
  - mbatchd handles disconnect/reconnect cleanly
  - job state remains coherent
  - no scheduler panic / infinite retry storm

#### C2 — Restart mbatchd (manual)

- Stop mbatchd on master
- Start mbatchd again
- Verify:
  - cluster comes back
  - no protocol corruption
  - existing jobs are handled coherently (according to current MVP semantics)

If this is not fully correct yet, document the limitation explicitly under Known Issues.

---

## 5. Known Limitations (MVP)

- No job arrays.
- No failover:
  - lim master down => scheduling control down
  - mbatchd down => scheduling control down
- Small cluster expectation:
  - MVP is validated for small-node-count testing only.
- Authentication enforcement is not fully integrated yet.

---

## 6. Release Artifacts

For an MVP tag, include:

- git tag: `v0.1.0-mvp` (or equivalent)
- release notes (this document + known issues)
- minimal install instructions
- the signal handling test plan document referenced as Gate A

---

## 7. Next: Job Arrays Test Plan (Post-MVP)

Job arrays are explicitly post-MVP, but we define a future test plan shape now.

### Array acceptance goals (future)

- Submit array with N elements
- Partial signaling (subset of elements)
- Mixed outcomes:
  - some elements finish while signaling is in progress
  - error reporting is per-element, not global
- Robustness:
  - no daemon crashes under large array signaling
  - array element states remain consistent

Deliverable:
- a dedicated “Job Arrays Test Plan” document mirroring the MVP signal plan,
  but extended with per-element verification and mixed-result scenarios.
