# LavaLite MVP Customer Validation Test Plan (v0.1.0-mvp)

This document is the **customer-facing** and **internal** validation checklist
for the LavaLite **v0.1.0-mvp** release.

It is meant to be executed on a **small cluster** (2–8 hosts) and proves the MVP:
- base daemons and commands work (lsid/lsload/lshosts)
- batch lifecycle works (bsub/bjobs/bkill)
- visibility and policy basics work (bqueues/bhosts/busers)
- transport is stable (no crashes, no protocol corruption)

**Important:** Job arrays are explicitly out of scope for MVP.

---

## 0. Scope and Assumptions

### In scope
Base:
- `lsid` (UDP)
- `lsload` (TCP)
- `lshosts` (TCP)

Batch:
- `bsub` (covered by included test plan)
- `bqueues`
- `bjobs` (covered by included test plan)
- `bkill` (covered by included test plan)
- `bhosts`
- `busers`

### Out of scope (MVP)
- job arrays
- failover / HA
- packaging (RPM/DEB)
- large-scale performance claims

### Test environment assumptions
- 1 master host (LIM master host) running: `lim`, `mbatchd`
- N execution hosts running: `sbatchd` (and lim slave if applicable)
- Hosts resolve to canonical names consistently
- Firewalls permit required UDP/TCP traffic between daemons
- Time is roughly synchronized (NTP recommended)

---

## 1. Pre-flight Checklist

### 1.1 Version and PATH sanity
Commands:
- `which lsid lsload lshosts bsub bjobs bkill bqueues bhosts busers`
- `lsid`

Verify:
- all commands execute (no missing binaries, no wrong old installs)
- `lsid` identifies the cluster and master as expected

Pass criteria:
- no crashes
- output is coherent and matches configured cluster name/master

---

## 2. Base Validation (UDP/TCP plumbing)

### 2.1 lsid (UDP)
Command:
- `lsid`

Verify:
- returns cluster identification
- identifies master host
- no timeouts / hangs

Expected:
- correct cluster name
- correct master host identity
- no error output

---

### 2.2 lshosts (TCP)
Command:
- `lshosts`

Verify:
- all configured hosts appear
- hostnames are canonical and consistent

Expected:
- host list matches expected nodes
- no duplicated hosts with different names (alias vs canonical)
- no timeouts

---

### 2.3 lsload (TCP)
Command:
- `lsload`

Verify:
- load information returned for hosts
- output is non-empty and stable across repeated calls

Expected:
- each host reports values (even if basic/minimal)
- repeated runs do not cause disconnect storms or daemon issues

---

## 3. Batch Core Lifecycle (MVP Gate)

This gate is satisfied by executing the dedicated signal-handling test plan:

- **LavaLite Signal Handling Test Plan (MVP)**

This plan covers:
- submit held job (PSUSP)
- resume held job (PEND)
- pending STOP/CONT semantics
- running STOP/CONT semantics (USUSP/RUN)
- KILL semantics for pending and running jobs
- signaling finished jobs returns the correct semantic error
- transport stability checks

**Execute the full plan and record results.**

Pass criteria:
- all expected states match
- LSBE_* codes are semantic outcomes
- `-1` used only for internal/transport failures
- no crashes, no protocol corruption, no unexpected disconnects

(Reference doc: `lavalite_bsub_test.md` in this repository.)
NOTE: This document is included as part of the v0.1.0-mvp acceptance gate.

---

## 4. Batch Visibility Commands

These tests validate read-only inspection commands work and remain coherent
while jobs transition through states.

### 4.1 bqueues
Command:
- `bqueues`

Verify:
- queues are listed
- queue status is coherent

Expected:
- at least default queue is visible (or configured queues)
- no hangs / no empty output due to transport failures

---

### 4.2 bhosts
Command:
- `bhosts`

Verify:
- hosts appear with status
- expected hosts are runnable/unrunnable depending on configuration

Expected:
- all execution hosts listed
- master host appears according to configuration
- no hangs

---

### 4.3 busers
Command:
- `busers`

Verify:
- user summary is visible
- counts update after submitting jobs

Suggested procedure:
1. run `busers` (baseline)
2. submit a few jobs (see Section 3 plan)
3. run `busers` again

Expected:
- user appears (if supported in MVP)
- pending/running counts reflect recent submissions within reasonable time

If `busers` is only partially implemented in MVP, document the limitation
in Known Issues and ensure it fails gracefully (no crash).

---

## 5. Multi-host MVP Validation (Minimum)

MVP must demonstrate jobs can execute on multiple hosts and control stays coherent.

### 5.1 Multi-host placement smoke test
Procedure:
1. Submit multiple long-running jobs:
   - `bsub -o /dev/null sleep 300`
   - repeat until jobs are distributed
2. For each JOBID:
   - `bjobs JOBID`

Verify:
- `EXEC_HOST` becomes populated for running jobs
- jobs spread across multiple execution hosts (if capacity exists)

Expected:
- at least 2 distinct execution hosts used (on a multi-host cluster)
- no job enters RUN without an active connected sbatchd on the target host

---

### 5.2 Cross-host signaling smoke test
Pick two running jobs on different execution hosts.

Commands:
- `bkill -s STOP JOBID_A`
- `bkill -s STOP JOBID_B`
- `bkill -s CONT JOBID_A`
- `bkill -s CONT JOBID_B`
- `bkill -s KILL JOBID_A`
- `bkill -s KILL JOBID_B`

Verify:
- state transitions correct per job
- no cross-talk (wrong job signaled)
- no orphan processes remain running after job finishes

---

## 6. Stability Checks (Always On)

During the entire test run, observe:

- no daemon crashes / core dumps
- no protocol "UNKNOWN opcode" errors
- no retry storms
- no repeated duplicate state-change events for no-op actions
- logs show semantic outcomes (not plumbing noise)

If a failure is observed:
- record exact commands run
- record JOBID(s)
- capture relevant daemon logs (mbatchd/sbatchd/lim)
- capture whether failure is deterministic or intermittent

---

## 7. Final Acceptance Criteria

The v0.1.0-mvp release is acceptable if:

- Base commands (lsid/lsload/lshosts) pass
- Signal Handling Test Plan passes in full (Section 3)
- bqueues/bhosts/busers do not crash and provide coherent output
- Multi-host smoke tests pass (placement + signaling)
- No transport corruption or daemon crashes occur during testing

---

## Appendix A: Results Template

Record:

- Cluster size / hostnames
- Build and install method
- Test date
- Tester(s)
- Pass/fail per section
- Notes / issues / log pointers
---

## Appendix B – Expected Output Examples (Reference Only)

These examples show the **shape** of correct output.
Exact formatting, timestamps, or column alignment may differ.

They are meant to help testers recognize healthy behavior,
not to enforce byte-for-byte matching.

---

### B.1 lsid (UDP)

Command:
lsid

Example shape:

My cluster name is lavalite_cluster
My master name is master01

Healthy indicators:
- cluster name matches configuration
- master hostname is correct and canonical
- no timeout or error messages

---

### B.2 lshosts (TCP)

Command:
lshosts

Example shape:

HOST_NAME     type    model    cpus    status
master01      X86_64  local    8       ok
node01        X86_64  local    16      ok
node02        X86_64  local    16      ok

Healthy indicators:
- all configured hosts listed
- no duplicated host entries with alias names
- status field coherent (e.g. ok/unavail as expected)

---

### B.3 lsload (TCP)

Command:
lsload

Example shape:

HOST_NAME     status    r15s   r1m   r15m   ut    pg
master01      ok        0.1    0.2   0.3    5%    0
node01        ok        0.0    0.1   0.2    2%    0
node02        ok        0.2    0.4   0.5    7%    0

Healthy indicators:
- numeric load values present
- status column present
- repeated calls stable (no daemon instability)

---

### B.4 bqueues

Command:
bqueues

Example shape:

QUEUE_NAME    PRIO    STATUS    MAX    JL/U
normal        30      Open      -      -

Healthy indicators:
- at least one queue visible
- STATUS is coherent (Open/Closed as configured)
- no empty output unless intentionally configured

---

### B.5 bhosts

Command:
bhosts

Example shape:

HOST_NAME     STATUS     JL/U    MAX    NJOBS
node01        ok         -       -      1
node02        ok         -       -      2

Healthy indicators:
- execution hosts listed
- NJOBS reflects running jobs
- status coherent with scheduler view

---

### B.6 busers

Command:
busers

Example shape:

USER       NJOBS    PEND    RUN    USUSP
tester     3        1       2      0

Healthy indicators:
- current user appears (if implemented in MVP)
- counters change after submitting/killing jobs
- no crash if user summary minimal

If busers is partially implemented in MVP,
it must fail gracefully (no segmentation fault, no protocol error).

---

### B.7 bjobs – Pending (PSUSP)

Command:
bjobs JOBID

Example shape:

JOBID   USER     STAT    QUEUE     FROM_HOST   EXEC_HOST   COMMAND
123     tester   PSUSP   normal    master01    -           sleep 86400

Healthy indicators:
- STAT=PSUSP
- EXEC_HOST empty or '-'

---

### B.8 bjobs – Pending (PEND)

Example shape:

JOBID   USER     STAT    QUEUE     FROM_HOST   EXEC_HOST   COMMAND
123     tester   PEND    normal    master01    -           sleep 86400

Healthy indicators:
- STAT=PEND
- no EXEC_HOST

---

### B.9 bjobs – Running (RUN)

Example shape:

JOBID   USER     STAT    QUEUE     FROM_HOST   EXEC_HOST   COMMAND
124     tester   RUN     normal    master01    node01      sleep 86400

Healthy indicators:
- STAT=RUN
- EXEC_HOST populated
- EXEC_HOST matches expected node

---

### B.10 bjobs – User Suspended (USUSP)

Example shape:

JOBID   USER     STAT    QUEUE     FROM_HOST   EXEC_HOST   COMMAND
124     tester   USUSP   normal    master01    node01      sleep 86400

Healthy indicators:
- transition RUN → USUSP after STOP
- EXEC_HOST remains populated

---

### B.11 bjobs – Finished (DONE / EXIT)

Example shape:

JOBID   USER     STAT    QUEUE     FROM_HOST   EXEC_HOST   COMMAND
124     tester   DONE    normal    master01    node01      sleep 86400

or

JOBID   USER     STAT    QUEUE     FROM_HOST   EXEC_HOST   COMMAND
125     tester   EXIT    normal    master01    node02      sleep 86400

Healthy indicators:
- final state stable
- no flip-flopping between states
- no phantom RUN after EXIT

---

## Appendix C – What Is NOT Acceptable

The following indicate MVP failure:

- segmentation faults in any daemon
- UNKNOWN opcode or protocol corruption errors
- jobs stuck indefinitely in transitional states
- EXEC_HOST populated for PEND/PSUSP jobs
- job remains running at OS level after scheduler marks EXIT/DONE
- repeated duplicate state transitions for no-op signals
- transport disconnect storm after simple STOP/CONT

If any of the above occur, capture:
- exact command sequence
- JOBID(s)
- relevant daemon logs
- reproduction steps
