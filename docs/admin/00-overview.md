# LavaLite Administrator Guide

Version 2.0.1

## What is LavaLite

LavaLite is a scheduler for batch jobs and persistent services on
Linux compute clusters.

It is designed for high-performance computing (HPC) and other
batch-oriented workloads that require scheduling, resource allocation,
and execution across multiple hosts.

Its architecture emphasizes simplicity, transparency, and recoverability.
Scheduler state is stored in files and changes are recorded through
durable events. There is no external database and no implicit scheduler
state. Cluster state can be reconstructed after daemon restart from the
recorded events.

Users submit jobs using `bsub`, monitor and control them using
commands such as `bjobs`, `bhist`, and `bkill`. The scheduler dispatches
jobs to execution hosts based on resource availability, queue policy,
host configuration, token pools, and GPU requirements.

LavaLite also runs services: long-running applications such as
Jupyter or code-server, defined by the administrator in
`llb.services` and started by users with `bservices`. Each instance is
an ordinary job running in its own network namespace, reached through
an endpoint on its execution host.

## Architecture

LavaLite consists of two daemons, a client API, and command-line
tools. `mbd` is the central scheduler; `sbd` runs on each execution
host.

### mbd (master batch daemon)

`mbd` runs on a single management host.

Responsibilities:

- Job management
- Queue management
- Scheduling
- Cluster state management
- Event logging
- State recovery

### sbd (slave batch daemon)

`sbd` runs on every execution host.

Responsibilities:

- Job execution
- Resource enforcement
- Process tracking
- Job state reporting
- Recovery after restart
- Network namespaces and endpoints of service instances

`sbd` connects to `mbd` and registers. It does not listen on a
port, and `mbd` never initiates a connection to it.

CPU and memory allocations are enforced using Linux cgroup v2 controls.
`sbd` places each job into a dedicated cgroup and uses
that cgroup for resource accounting, resource enforcement, and job
control operations such as suspend, resume, and terminate.

A job lives until the last process in its cgroup exits, and `sbd`
never kills job processes on its own. There is no automatic cleanup
kill when the job script exits, and this lifecycle rule introduces no
timeout of its own: a process the script left running keeps the job
running. Job processes are signaled only on request, by `bkill` or by
a limit the user or the queue set, and the signal reaches every
process in the cgroup, also one that left the process group with
`setsid`. The job exit status is the exit status of the job script.

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
bservices
```
These commands use the same API available to applications.

## Architecture Diagram

The major components are shown below:

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
    +-- bservices
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

`mbd` maintains scheduler state, performs dispatch decisions, records
durable events in the event manifest, and reconstructs scheduler state
during startup.

`sbd` manages the execution lifecycle of jobs on execution hosts. It
maintains durable execution state, enforces resources, performs job
control operations, recovers jobs after restart, and reports state
changes back to `mbd`.

## Security

LavaLite authenticates requests using HMAC-SHA256. The authentication mechanism uses two separate keys:

- `ll.auth.key` authenticates client requests to `mbd`.
- `ll.daemon.key` authenticates communication between `mbd` and `sbd`.

Both keys are stored in `LL_CONF_DIR` and must be protected from ordinary users. Client requests are signed through the privileged `llsign` helper, which uses the calling user's UID and GID.

Authentication keys are 32-byte binary values. Key files must contain at least 32 bytes; only the first 32 bytes are used. Additional bytes are ignored and provide no additional security.

Keys generated by LavaLite contain exactly 32 random bytes.

## Design Principles

### Durable State

LavaLite stores scheduler state using persistent files.

Scheduler state can be reconstructed after daemon restart without
requiring an external database.

### Event-Based Recovery

Every significant scheduler action is applied to in-memory state, then
recorded as a durable event in the event manifest. Events are only
persisted once they are known to be valid and applied, not before —
this keeps the manifest consistent with what actually happened, rather
than with what was merely attempted.

After restart, `mbd` replays the event manifest and reconstructs
scheduler state.

### Absolute Time

The daemons work in absolute time, seconds since the epoch. Times
given by users, `bsub -b` and `bsub -t`, are read in the local
time zone of the `bsub` process and converted to absolute time at
submission, see `bsub(1)`. The time zone of `mbd` and `sbd` only
changes how times are printed in their logs.

### Simple Administration

Cluster configuration is stored in text files:

```text
ll.conf
llb.queues
llb.hosts
llb.services
```

No external database is required.

### Minimal Dependencies

LavaLite depends only on standard Linux facilities:

- service manager (systemd examples provided)
- cgroup v2
- OpenSSL
- `iproute2` and `nftables`, on the execution hosts of service queues

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
