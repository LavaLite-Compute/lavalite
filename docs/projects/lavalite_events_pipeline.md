# lavalite_events_pipeline.md

This document defines the complete MBD ↔ SBD job lifecycle pipeline
and the PID-centric recovery protocol used in LavaLite.

This reflects actual historical behavior which we currently maintain:

- JOB_STAT_RUN is set by MBD at dispatch time.
- There is no explicit JOB_STAT_EXECUTE state.
- Arrival of pid from SBD triggers logging of JOB_EXECUTE.
- Internal job state remains JOB_STAT_RUN.
- Recovery is driven strictly by deterministic PID reconciliation.

Design goals:

- No implicit transitions
- No duplicate history entries
- No inference
- Deterministic restart convergence
- No introduction of new job states


---------------------------------------------------------------------

# 1. Job Lifecycle

## 1.1 Dispatch Phase (RUN)

1. User submits job.
2. MBD selects execution host H.
3. MBD sends BATCH_NEW_JOB to SBD(H).
4. MBD sets internal job state to JOB_STAT_RUN.

At this moment:

- Job is RUN from scheduler perspective.
- No PID exists yet.
- No JOB_EXECUTE event is logged.
- Execution is not yet proven.

RUN is a scheduling decision only.


## 1.2 PID Phase (Execution Gate)

1. SBD forks.
2. SBD obtains pid.
3. SBD sends PID-report to MBD.
4. MBD:
   - Persists pid
   - Logs JOB_EXECUTE
   - Sends ACK to SBD

Internal job state remains JOB_STAT_RUN.

Example history:

```
"JOB_START" "1" 1771086493 102 4 0 0 100.0 1 "zos" "" "" 0 0
"JOB_START_ACCEPT" "1" 1771086493 102 7691 7691 0
"JOB_EXECUTE" "1" 1771086494 102 -1 7691 "/home/lavalite" "/home/lavacore" "lavacore" 7691 0
```

There is no JOB_STAT_EXECUTE state.
JOB_EXECUTE is a durable event triggered by PID arrival.

**Once the process has a PID, we know it’s actually running.
That’s the first solid proof of execution. From that point we log JOB_EXECUTE and
allow all later events.**

## 1.3 Finish Phase

1. Job exits.
2. SBD sends BATCH_JOB_FINISH.
3. MBD:
   - Persists terminal state
   - Logs final job event
   - ACKs SBD
4. SBD sets finish_acked = true.

SBD retains durable per-job state for recovery under LSB_SHAREDIR/sbd/state.

---------------------------------------------------------------------

# 1.4 Exact Execution Sequence (PID + GO Barrier)

Execution is two-phase and strictly ordered.

1. MBD dispatches job.
   - Job state set to JOB_STAT_RUN.

2. SBD forks.
   - Kernel assigns PID.
   - Child is created but blocked (waiting for GO).

3. SBD sends PID report to MBD.

4. MBD:
   - Persists PID in LSB_SHAREDIR/mbd/lsb.events
   - Logs JOB_EXECUTE
   - Sends ACK

5. SBD receives PID ACK from MBD.
   - Set pid_acked = true.
   - Persist pid_acked = true in the SBD state file (durable write).

6. SBD creates the GO file.
   - The GO file is the release barrier.
   - Child process proceeds only after GO exists.

- Ordering guarantee:<br>
  State write (pid_acked=true) must complete before GO is created.
  GO must never exist without pid_acked=true durably recorded.


7. Child process detects GO and proceeds to execute payload.

Key properties:

- PID existence alone does not release execution.
- pid_acked = true is the durability confirmation.
- GO creation is the execution release barrier.

---------------------------------------------------------------------

# 2. Durable State

## 2.1 MBD Persistent Data

MBD persists job state and events in:

LSB_SHAREDIR/mbd/lsb.events

For each job, MBD records:

- job_id
- job_file
- exec_host
- pid (once received; required for execution phase)
- JOB_EXECUTE event
- terminal status event (DONE / EXIT)

MBD does not store ack flags.

**A RUN job without a PID means dispatch occurred but execution
was never proven.**

## 2.2 SBD Persistent Data

Per job (stored under LSB_SHAREDIR/sbd/state/<jobfile>/):

- job_id
- job_file
- pid (valid PID, > 0)
- pid_acked (bool)
- execute_acked (bool)
- finish_acked (bool)

Rules:

- If spawn fails, no PID is recorded and no durable state is kept.
- A job present in SBD state always has a valid PID.
- pid_acked means MBD has durably recorded the PID, and SBD can safely create
  the go file to unblock execution.

Invalid state:

- pid absent
- pid <= 0
- pid_acked = true without pid

---------------------------------------------------------------------

# 3. Recovery Goals

After restart of MBD, SBD, or both:

- No duplicate history entries
- No implicit state transitions
- No guessing
- Deterministic convergence

PID reconciliation is always the first step.

Execute and finish replay only occur after PID agreement.


---------------------------------------------------------------------

# 4. PID Recovery Model

We enumerate all PID-phase combinations explicitly.

Only PID phase is modeled here.
Execute and finish follow identical replay logic.


---------------------------------------------------------------------

# 4.1 SBD PID States

**pid present** means a valid PID value (> 0) is stored in the
SBD durable state record.

S1:<br>
```
  pid present
  pid_acked = true
  Meaning:
    PID durably recorded and acknowledged by MBD.
```

S2:<br>
```
  pid present
  pid_acked = false
  Meaning:
    PID durably recorded but MBD persistence not yet confirmed.
```

S3:<br>
```
  pid absent
  pid_acked = false
  Meaning:
    Durable state exists but PID missing.
    This indicates crash window or state corruption.
```
---------------------------------------------------------------------

# 4.2 MBD PID States (Reply to Sync)

M1:
  Job known
  pid present
  Meaning: PID is already recorded in LSB_SHAREDIR/mbd/lsb.events.

M2:
  Job known
  pid absent
  Meaning: job was dispatched (JOB_STAT_RUN) but PID was never recorded.

Any "job unknown" report is not a runtime state in current LavaCore.
Treat it as corruption / wrong cluster / stale state:
log error, ignore, and require operator cleanup.

MBD must persist PID and log JOB_EXECUTE before sending the PID ACK.
The ACK is an acknowledgement of a durable action, not just receipt.

---------------------------------------------------------------------
# 4.3 Cross Product Matrix (runtime only)

We reason about recovery as the cross-product of SBD durable state × MBD durable state.
Each pair maps to exactly one action (OK / resend / quarantine / fatal).

S1:<br>
  - pid present
  - pid_acked = true

S2:<br>
  - pid present
  - pid_acked = false

M1:<br>
  - job known, pid present

M2:<br>
  - job known, pid absent

MBD authoritative list gives:
  M1: job_id + pid
  M2: job_id + no_pid

SBD local gives:
  S1: pid_acked=true
  S2: pid_acked=false

M1 × S1:
  - No action (already consistent)

M1 × S2:
  - No special recovery action.
  - The pipeline will converge on its own after reconnect.
    (SBD will observe MBD has pid and will progress normally.)

M2 × S1:
  - KICK: push PID to MBD (MBD missing pid but SBD thinks it was acked).
  - This is the “impossible unless MBD lost state” case.

M2 × S2:
  - KICK: resend PID to MBD.
  - This is the normal “no ACK / no pid recorded” recovery case.

Only intervene when MBD has no pid (M2).
If MBD already has pid (M1), do nothing special.


# 8. Algorithm

Recovery rule (MBD authoritative, minimal):

MBD sends the authoritative RUN list for host H.
For each job in that list:

- If MBD already has pid:
    do nothing (pipeline will converge normally).

- If MBD has no pid:
    kick the pipeline by requesting/resending the PID exchange.


```
For each RUN job that MBD lists on host H:
  if MBD entry has no pid:
      SBD resend PID (kick PID exchange)
  else:
      do nothing (normal pipeline will handle)
```

---------------------------------------------------------------------

# 8. Special Failure Classes

8.1 PID Mismatch

If both sides have pid but differ:

- Log loudly
- Mark job inconsistent
- Do not attempt merge


8.2 Fork Failure Scenario

Case:

MBD dispatched (RUN set)
SBD never produced PID

State:

S3 × M2

SBD reports no pid.
MBD policy decides:

- requeue
- mark failed
- operator intervention
---------------------------------------------------------------------

# 9. Convergence Guarantee

All recoverable states converge to:

S1 × M1

Meaning:

- PID agreed
- JOB_EXECUTE logged
- Pipeline continues safely


---------------------------------------------------------------------

# 10. Core Principle

RUN is a scheduler decision.

PID triggers JOB_EXECUTE logging.

Recovery is deterministic PID reconciliation.

No new job states are introduced.
No implicit transitions are inferred.
All replay is explicit and idempotent.
