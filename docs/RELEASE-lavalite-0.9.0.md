# LavaLite 0.9.0 Release Notes

**Tag:** `lavalite-0.9.0`
**Previous release:** `lavalite-0.1.1`
**Status:** In Stabilization

## Overview

LavaLite 0.9.0 is the first release intended to provide a complete and
self-consistent HPC scheduling environment.

The release focuses on scheduler correctness, durable state management,
recovery behavior, operational visibility, and administrator usability.

Development is currently focused on stabilization, validation, and
recovery testing prior to release tagging.

## Major Features

### Core Scheduling

- Queue-based workload scheduling
- Host group based resource partitioning
- Queue priorities
- Per-job priorities
- Queue access control
- Queue-to-queue job movement (`bmove`)

### Resource Management

- CPU scheduling (`--cpus`)
- Multi-host scheduling (`--nhosts`)
- Memory-aware scheduling (`--mem`)
- Storage-aware scheduling (`--storage`)
- Exclusive host allocation (`--exclusive`)

### GPU Scheduling

- GPU-aware scheduling (`--gpus`)
- GPU model matching (`--gpu-model`)
- GPU and MIG resource support

### Token Pools

- Token-based scheduling
- Floating license resource control
- Multiple token pool requests per job

### Job Control

- Hold and release
- Suspension and resume
- Termination and signal delivery
- Job movement between queues
- Runtime priority modification

### Job History

- Historical job inspection using `bhist`
- Event-log-based history reconstruction
- Persistent sidecar job information

### Recovery

- Durable event logging
- Event replay during startup
- `mbd` restart recovery
- `sbd` restart recovery
- Persistent scheduler state
- Recovery without job loss

### Security

- HMAC-SHA256 authenticated communication
- Shared-key cluster authentication
- Request authentication through the API layer

## User Commands

### Job Submission and Control

```text
bsub
bjobs
bhist
bkill
bmove
bpriority
```

### Resource Inspection

```text
bhosts
bqueues
bgroups
btokens
```

### Advanced Job Submission

`bsub` supports:

```text
--queue
--name
--project
--comment

--cpus
--nhosts
--mem
--storage

--gpus
--gpu-model

--exclusive

--pool

--machines

--stdin
--stdout
--stderr

--begin
--terminate
```

## Documentation

The release includes:

```text
docs/admin
docs/testing
docs/man
```

Documentation now includes:

- Administrator Guide
- Command Reference
- Configuration Reference
- Operational Procedures
- Recovery Procedures
- Validation Test Plan

## Platform Support

Current validation targets:

- Rocky Linux 8
- Rocky Linux 9
- Ubuntu 24.04

## Current Stabilization Work

The following areas are undergoing active validation:

- Scheduler recovery behavior
- Event replay correctness
- Queue accounting consistency
- Resource accounting consistency
- Multi-host scheduling
- GPU scheduling
- Token pool scheduling
- Administrative workflows
- Manual failure testing
- Regression testing

## Known Limitations

- Job dependencies are not yet implemented.
- Begin and termination scheduling require additional validation.
- Production-scale testing is ongoing.
- The release has not yet been tagged.

## Release Criteria

The 0.9.0 release will be tagged after completion of:

```text
docs/testing/
```

including:

- Job lifecycle validation
- Queue administration validation
- Resource scheduling validation
- mbd restart validation
- sbd restart validation
- Event replay validation

and resolution of all identified release-blocking defects.
