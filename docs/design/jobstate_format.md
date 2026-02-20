# LavaLite Job State File Format

## Overview

Each running or recently executed job managed by SBD has an associated
**state file** stored under the SBD runtime directory.

The state file represents the authoritative local execution state of a job.

It is:

- owned by `root`
- written and updated exclusively by SBD
- used for restart recovery
- used to drive pipeline advancement
- independent from MBD memory state

The state file allows SBD to reconstruct execution progress after
daemon restart without requiring resubmission or external inference.

---

## Location

State files are stored under:

    $LAVALITE_WORKDIR/sbd/state/<jobfile>/state

Example:

    /opt/lavalite/var/work/sbd/state/1771553115.202/state

The directory name matches the job file identifier.

---

## File Properties

- plain text
- newline-separated key/value pairs
- deterministic write model
- rewritten atomically
- root-owned
- not user-modifiable

Example permissions:

    -rw------- root root state

Users must never modify this file.

---

## Example State File

    version=1
    job_id=202
    pid=51810
    pgid=51810
    pid_acked=1
    time_pid_acked=1771553117
    execute_acked=1
    time_execute_acked=1771553117
    finish_acked=0
    time_finish_acked=0
    exit_status_valid=0
    exit_status=0
    end_time=0
    exec_cwd=/opt/lavalite-0.1.0/var/work
    exec_home=/home/david
    exec_uid=1000
    exec_user=david
    jobfile=1771553115.202

---

## Field Description

### version

State file format version.

Allows future backward-compatible extensions.

---

### job_id

Scheduler job identifier assigned by MBD.

---

### pid

Process ID of the launched job leader.

Valid only after successful fork/exec.

---

### pgid

Process group ID associated with the job.

Used for signal delivery and cleanup.

---

### pid_acked

Indicates that SBD successfully created the job process
and acknowledged PID creation internally.

Values:

    0  PID not acknowledged
    1  PID acknowledged

---

### time_pid_acked

Unix timestamp when PID acknowledgement occurred.

---

### execute_acked

Indicates that execution has entered the runnable phase.

Represents advancement past process creation into
confirmed execution.

---

### time_execute_acked

Unix timestamp marking execution acknowledgement.

---

### finish_acked

Indicates that job completion has been observed and
acknowledged locally.

Completion detection may occur via:

- process reaping
- exit file detection
- recovery reconciliation

---

### time_finish_acked

Timestamp of completion acknowledgement.

---

### exit_status_valid

Indicates whether a valid exit status has been recorded.

Values:

    0  unknown
    1  valid

---

### exit_status

Exit code reported by the executed job.

Meaningful only when `exit_status_valid=1`.

---

### end_time

Unix timestamp marking job completion time.

Zero indicates completion not yet finalized.

---

### exec_cwd

Execution working directory used when launching the job.

---

### exec_home

User home directory captured at execution time.

---

### exec_uid

Numeric UNIX user identifier used for execution.

---

### exec_user

Username associated with execution.

---

### jobfile

Identifier of the generated job file associated with
this execution instance.

---

## Ownership and Authority

The state file is owned by **root** and maintained solely by SBD.

It serves as the authoritative local execution record.

MBD does not reconstruct execution state directly; instead,
pipeline reconciliation relies on SBD state persistence.

---

## Restart Semantics

During SBD restart:

1. State directories are scanned.
2. State files are parsed.
3. Pipeline stage is reconstructed.
4. Missing acknowledgements are regenerated if required.
5. Execution continuity is restored.

This guarantees deterministic recovery even if SBD terminates
unexpectedly.

---

## Pipeline Role

The state file drives pipeline invariants:

    PID acknowledged
        → execution acknowledged
            → finish acknowledged
                → exit status valid

State advancement is monotonic.

No pipeline stage may regress.

---

## Design Principles

The LavaLite state record follows strict rules:

- explicit state instead of inference
- monotonic progression
- crash-safe persistence
- human-readable debugging
- restart determinism

The file intentionally duplicates some runtime information to
eliminate ambiguity during recovery.

---

## Non-Goals

The state file is not intended for:

- user inspection workflows
- external modification
- monitoring APIs
- accounting storage

It exists solely for execution correctness and recovery.
