# LavaLite 1.0 Testing

This directory contains the manual test plan for LavaLite 1.0.

The goal is to verify the documented user-visible behavior of the
LavaLite commands, daemons, persistent state, and event replay logic.

## Test Files

### Environment

- `00-environment.md`
  Basic cluster setup, required configuration, daemon startup, and sanity checks.

### User Commands

- `10-bsub-basic.md`
  Basic job submission, resource options, queue selection, held jobs, and I/O paths.

- `11-bjobs.md`
  Job listing and visible job states: `PEND`, `HELD`, `RUN`, `SUSP`, `DONE`, `EXIT`.

- `12-bhist.md`
  Job history, detailed job view, lifecycle events, dispatch records, and fork records.

- `13-bkill.md`
  Job termination, suspension, resume, and held-job release through signals.

- `14-bmove.md`
  Moving pending or held jobs between queues.

- `15-bpriority.md`
  Changing job priority and verifying scheduling order.

### Cluster State Commands

- `20-bhosts.md`
  Host listing, resource counters, host close/open behavior.

- `21-bqueues.md`
  Queue listing, queue counters, queue close/open behavior.

- `22-bgroups.md`
  Host group listing and host group membership validation.

- `23-btokens.md`
  Token pool listing, token allocation, and token release.

### Recovery and Persistence

- `30-mbd-restart.md`
  Restarting `mbd` and verifying state recovery.

- `31-sbd-restart.md`
  Restarting `sbd` and verifying running job behavior.

- `32-event-replay.md`
  Verifying event replay, job reconstruction, and `bhist` correctness.

## Test Format

Each test file should use the same structure:

```markdown
# Title

## Purpose

What this test validates.

## Prerequisites

Required daemons, users, queues, hosts, or configuration.

## Tests

### TEST-NNN: Test name

#### Commands

Commands to run.

#### Expected Result

Expected command output or state transition.

#### Pass Criteria

The exact condition that makes the test pass.
```

## General Rules

Run tests from a normal user account unless the test explicitly requires
administrator privileges.

Use simple commands such as:

```sh
sleep 60
/bin/true
/bin/false
hostname
```

Prefer short jobs for normal tests and long-running jobs only where a
state transition must be observed.

When a test creates a job, record the job ID and use it consistently in
later commands.

When a test depends on daemon restart or event replay, verify the result
with both:

```sh
bjobs
bhist <jobid>
```

The manual tests should follow the command behavior documented in
`docs/man/markdown`.
