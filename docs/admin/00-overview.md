# LavaLite Administrator's Guide — Overview

LavaLite 0.9.0

## What is LavaLite

LavaLite is an open-source batch job scheduler for Linux HPC clusters.
It dispatches jobs submitted by users to execution hosts, enforces
resource limits via cgroup v2, and tracks job history.

## Design principles

LavaLite is built around durable, file-based state. All job events are
written to an on-disk event log before any in-memory state changes.
After a restart, mbd replays the log and reconstructs full cluster state
with no external database required. sbd survives restarts without losing
running jobs.

This design makes the system predictable: what you see on disk is what
the scheduler knows.

## Architecture

Two daemons:

- **mbd** — master batch daemon. Runs on one host. Manages the job queue,
  scheduling, and cluster state.
- **sbd** — slave batch daemon. Runs on each execution host. Executes jobs,
  enforces limits, reports status.
- **commands and API** — user commands (`bsub`, `bjobs`, `bkill`, ...) and
  the `llbatch` C API. Connect to mbd to submit jobs, query state, and
  perform administrative operations.

Communication is TCP with XDR encoding, authenticated via HMAC-SHA256.

## Document structure

- `01-install.md` — installation and directory layout
- `02-configuration.md` — ll.conf, llb.queues, llb.hosts
- `03-queues-and-hosts.md` — queue and host management
- `04-operations.md` — starting, stopping, monitoring the cluster
- `05-troubleshooting.md` — common problems and diagnostics

## See also

`man 7 lavalite` for a concise reference version of this overview.
