# LavaLite Administrator Guide

Version 1.0

## What is LavaLite

LavaLite is an open-source batch scheduler for Linux clusters.

LavaLite is designed for HPC, EDA, AI, and simulation
clusters.

Its architecture emphasizes simplicity, transparency, and
recoverability. Scheduler state is stored in files, changes are
recorded through durable events, and cluster state can be reconstructed
after daemon restart without requiring an external database.

Users submit jobs using `bsub` and monitor and control them using
commands such as `bjobs`, `bhist`, and `bkill`. The scheduler dispatches
jobs to execution hosts based on resource availability, queue policy,
host configuration, token pools, and GPU requirements.

## Architecture

LavaLite consists of two daemons, a client API, and a set of command-line
tools.

### mbd

The Master Batch Daemon (`mbd`) runs on a single management host.

Responsibilities:

- Job submission
- Queue management
- Scheduling
- Cluster state management
- Event logging
- State recovery

### sbd

The Slave Batch Daemon (`sbd`) runs on every execution host.

Responsibilities:

- Job execution
- Resource enforcement
- Process tracking
- Job state reporting
- Recovery after restart


CPU and memory allocations are enforced using Linux cgroup v2 controls.
The execution daemon places each job into a dedicated cgroup and uses
that cgroup for resource accounting, resource enforcement, and job
control operations such as suspend, resume, and terminate.

GPU allocation is scheduler-enforced and prevents conflicting placement,
but does not currently provide operating-system-level device isolation.

### Client API

Applications communicate with the scheduler through the LavaLite batch
API (`llbatch`).

The API provides functions for:

- Job submission
- Job control
- Queue management
- Host management
- Scheduler queries

The API communicates with `mbd` using the LavaLite network protocol.

### Command-Line Tools

User and administrator commands are built on top of the `llbatch` API.

Common commands:

```text
bsub
bjobs
bhist
bkill
bmove
bpriority
bhosts
bqueues
bgroups
btokens
```
These commands are thin wrappers around the same API available to
applications.

## Architecture Diagram

LavaLite consists of two daemons, a client API, and a set of
command-line tools.

```
Applications
    |
    +-- bsub
    +-- bjobs
    +-- bhist
    +-- bkill
    +-- bmove
    +-- bpriority
    +-- bhosts
    +-- bqueues
    +-- bgroups
    +-- btokens
    |
llbatch API (libllbatch)
    |
    +-- TCP/XDR/HMAC
    |
          +-- sbd (host1)
mbd ------+
          +-- sbd (host2)
          +-- sbd (host3)
```

Applications and command-line tools use the same public API
(`libllbatch`). The API communicates with `mbd` using the LavaLite
network protocol, which uses TCP transport, XDR message encoding, and
HMAC-SHA256 authentication.

`mbd` maintains scheduler state, performs dispatch decisions, and
records durable events. `sbd` executes jobs on execution hosts and
reports state changes back to `mbd`.

## Design Principles

### Durable State

LavaLite stores scheduler state using persistent files.

Scheduler state can be reconstructed after daemon restart without
requiring an external database.

### Event-Based Recovery

Every significant scheduler action is written to the event log before
being applied to in-memory state.

After restart, `mbd` replays the event log and reconstructs scheduler
state.

### Simple Administration

Cluster configuration is stored in text files:

```text
ll.conf
llb.queues
llb.hosts
```

No external database is required.

### Minimal Dependencies

LavaLite depends only on standard Linux facilities:

- systemd
- cgroup v2
- OpenSSL

## Directory Layout

A typical installation contains:

```text
bin/
    User commands

sbin/
    Scheduler daemons

etc/
    Configuration files

var/log/
    Scheduler logs

var/state/
    Persistent scheduler state
```

Details are described in the installation guide.

## Documentation Structure

This guide is organized as follows:

```text
00-overview.md
01-install.md
02-configuration.md
03-queues-and-hosts.md
04-operations.md
```
