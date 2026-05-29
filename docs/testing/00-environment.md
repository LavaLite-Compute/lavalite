# Test Environment

## Purpose

Verify that the LavaLite test environment is correctly configured
before executing any functional tests.

## Prerequisites

- LavaLite is installed.
- Configuration files are present.
- `mbd` is running.
- At least one `sbd` is running.
- Test user account exists.
- At least one queue is configured.
- At least one execution host is configured.

## TEST-001: Verify daemons are running

### Commands

```sh
ps -ef | grep mbd
ps -ef | grep sbd
```

### Expected Result

- One `mbd` process is running.
- One or more `sbd` processes are running.

### Pass Criteria

All required daemons are running.

## TEST-002: Verify queue configuration

### Commands

```sh
bqueues
```

### Expected Result

- At least one queue is displayed.
- Queue status is `open`.

### Pass Criteria

At least one usable queue exists.

## TEST-003: Verify host configuration

### Commands

```sh
bhosts
```

### Expected Result

- At least one host is displayed.
- Host state is `ok`.

### Pass Criteria

At least one execution host is available.

## TEST-004: Verify host groups

### Commands

```sh
bgroups
```

### Expected Result

- At least one host group is displayed.

### Pass Criteria

Host groups are visible.

## TEST-005: Verify token pools

### Commands

```sh
btokens
```

### Expected Result

- Command completes successfully.

### Pass Criteria

Token pool configuration can be queried.

## TEST-006: Verify job submission

### Commands

```sh
bsub sleep 5
```

### Expected Result

- A valid job identifier is returned.

Example:

```text
Job <123> is submitted to queue <cpu>.
```

### Pass Criteria

Job submission succeeds.

## TEST-007: Verify job visibility

### Commands

```sh
bjobs
```

### Expected Result

Previously submitted job is visible.

### Pass Criteria

Submitted jobs are displayed by `bjobs`.

## TEST-008: Verify job history

### Commands

```sh
bhist
```

### Expected Result

Job information is displayed.

### Pass Criteria

`bhist` returns job information without errors.

## Environment Validation

The environment is considered ready for functional testing when all
tests in this document pass.
