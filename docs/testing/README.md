# LavaLite Testing

This directory contains the manual test plan for LavaLite.

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

- `12-bkill.md`
  Job termination, suspension, resume, and held-job release through signals.

- `13-bmove.md`
  Moving pending or held jobs between queues.

- `14-bpriority.md`
  Changing job priority and verifying scheduling order.

- `15-bsub-options.md`
  Resource, placement, I/O and scheduling option combinations.

- `16-run-limit.md`
  `--time` run limit, `--terminate` deadline, expiry while pending, `DISP`.

- `17-bservices.md`
  Service instances: start, endpoint, delete, restart, token, run limit.

- `18-robustness.md`
  Silent and rejecting `sbd`, orphan jobs, `bkill -s kill`, replay and
  finished job retention.

### Configuration

- `19-config-changes.md`
  Configuration changes and mbd restart: configuration errors, lowering
  host capacity under running jobs.

### Reference

- `90-things-to-know.md`
  Known limits and by-design behaviour. Read before filing a failure.

- `bench-plan.md`
  Performance benchmark plan.

### Planned

- `bhist`, `bhosts`, `bqueues`, `bgroups`, `btokens` command tests.
- `sbd` restart and event replay tests.

## Automated Tests

`src/tests/system/run_tests.sh` runs the scripted subset as a normal
user against a running cluster. Each script prints `PASS`, `FAIL` or
`SKIP`. The service scripts use the service named by `$SVC`, default
`echotest`, and skip when it is not configured.

`src/tests/sim/simqa.sh` runs the robustness scenarios of
`18-robustness.md` on one box, as root, with its own configuration,
`mbd` and two sim `sbd`s. It needs no running cluster.

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
