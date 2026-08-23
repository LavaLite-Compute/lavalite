# LavaLite 1.1.0 Release Notes

## Overview

LavaLite 1.1.0 builds on the 1.0.0 baseline with job arrays, job
dependencies, and a significant `bhist` performance fix. It also closes
a data-integrity bug present since 1.0.0 that could silently discard
archived job history on daemon restart.

## New in 1.1.0

### Job Arrays

- `bsub --array START-END[:STRIDE]` submits an array of independent
  jobs from a single command. Elements are numbered from `START`
  (must be 1 or greater); `STRIDE` defaults to 1.
- Each element is a normal job for scheduling, accounting, and
  dispatch purposes. The array as a whole is identified by its first
  element's job ID.
- `bkill` supports both whole-array (`N`) and single-element (`N[M]`)
  targeting.
- `bjobs`/`bhist` display array elements as `array_id[index]`; a bare
  array ID aggregates every element.

### Job Dependencies

- `bsub --dependency EXPR` (short form `-w`) holds a job until `EXPR`
  evaluates true.
- `EXPR` is built from `done(id)`, `exit(id)`, and `ended(id)` terms
  (`ended` is `done || exit`), combined with `&&`, `||`, `!`, and
  parentheses. `&&` binds tighter than `||`.
- Dependency state is tracked with reference counting so a still-needed
  target job is never purged from the manifest by compaction while a
  dependent job is waiting on it, and survives an `mbd` restart.

### `bhist`

- Fixed an O(n²) job lookup in `bhist`'s history reconstruction. A
  full history query that previously took **over 11 minutes** at
  ~100,000 accumulated jobs now completes in about **5 seconds** —
  roughly a 125x improvement. Point lookups (`bhist <job_id>`) improve
  similarly.
- The fix scales with real event volume, not job count, so query time
  stays flat as the cluster's job history grows.
- New `-r`/`-p` flags filter history output down to jobs currently
  running or currently pending, mirroring `bjobs -r`.

## Bug Fixes

### Manifest archive data loss on restart (all versions since 1.0.0)

A directory-scan bug in `mbd`'s manifest compaction meant that, on
every daemon restart, `mbd` failed to recognize any previously
archived manifest file. As a result every restart reset its internal
archive counter to zero and began reusing already-used archive
filenames — silently overwriting and permanently destroying whatever
job history had been archived by the prior run, with no error or log
message.

This was found during pre-1.1.0 chaos testing and did not affect any
known production deployment. It is fixed in this release; archived job
history is now correctly preserved across restarts.

### Removed arbitrary packet size limit

A client-side response size check could reject legitimate `bjobs -a`
output on clusters with a large number of jobs, aborting the
connection before the full response was read. This left `mbd`
attempting to write to a socket the client had already abandoned,
which could crash the daemon with `SIGPIPE` (see Reliability below).
The client-side limit has been removed; `bjobs -a` is now confirmed
working cleanly at ~100,000 jobs.

### Missing job-pending event on sbd-reported job loss

When `sbd` reported a job as missing (e.g. after an `sbd` restart lost
track of a still-registered job), `mbd` correctly reclaimed the host's
resources and returned the job to pending in memory, but never wrote
the corresponding pending-state event to the manifest. This left the
manifest's history inconsistent with what actually happened: a
replay (at `mbd` startup, or in `bhist`) could see two dispatch
records for the same job with nothing in between, misreconstructing
the job as dispatched to two hosts at once and, in one observed case,
aborting `mbd` on startup via an internal consistency check.

Fixed in two parts: the missing event is now logged, and manifest
replay no longer trusts stale per-job host-assignment state left over
from a previous dispatch record, so a similar gap in the future
degrades gracefully instead of corrupting replayed state. Found via
extended chaos testing combining job arrays with induced `sbd`
restarts.

### Reliability

- `mbd` and `sbd` no longer inherit the launching process's signal
  mask and disposition; both now start with signals reset to default
  and `SIGPIPE` explicitly ignored, so a client disconnecting mid-response
  can no longer bring down the daemon.

## Removed

### `lim` daemon and `ls*` commands

The `lim` daemon and all `ls`-prefixed CLI tools (`lsid`, `lsload`,
`lsclusterinfo`) have been removed. Only the shared library code
remains. `lim` also historically provided master/mastership failover
between LIM instances, a capability LavaLite does not currently use
(it runs a single master); this is not lost functionality for the
current architecture.

## Major Features (from 1.0.0)

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
- Manifest-log-based history reconstruction
- Persistent sidecar job information

### Recovery

- Durable manifest logging
- Manifest replay during startup
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

--tokens

--machines

--stdin
--stdout
--stderr

--hold
--array
--begin
--terminate
--dependency
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
- Job Arrays

## Platform Support

Current validation targets:

- Rocky Linux 8
- Rocky Linux 9
- Ubuntu 24.04

## Known Limitations

- **Scheduling loop is O(pending) per tick for two cases.** A closed
  queue that keeps accepting submissions, and a job with a far-future
  `begin_time`, both sit on `pend_jobs_list` and get walked on every
  scheduler tick even though neither is dispatchable yet. Not a
  correctness issue, but a large enough backlog of either can add
  measurable per-tick scheduling overhead. Deferred to 2.0.
- **No backoff/quarantine for a host that repeatedly rejects
  dispatch.** If a host keeps rejecting jobs sbd hands it (for
  example, a persistent local misconfiguration), `mbd` will keep
  redispatching the same job(s) to that host on every scheduler tick
  rather than backing off. Workaround: close the affected host
  (`bhosts --close`) until the underlying issue is resolved.
