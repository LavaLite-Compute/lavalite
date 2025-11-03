# LavaLite Control and Data Plane Architecture

## Overview

LavaLite separates scheduling logic from job execution, following a **control plane / data plane** model similar to modern networking systems.

| Plane | Purpose | Typical Components |
|--------|----------|--------------------|
| **Control Plane** | Make decisions about resource allocation, job scheduling, and cluster state. | `lim`, `mbatchd` |
| **Data Plane** | Execute those decisions by running and supervising jobs on worker nodes. | `sbatchd` |

This separation keeps scheduling policies and state management independent from low-level job execution, improving robustness and fault isolation.

---

## Control Plane

The control plane is responsible for maintaining the global cluster view and making scheduling decisions.

### Components

#### `lim` — Load Information Manager
- Maintains node inventory and load metrics.
- Periodically exchanges host status with other `lim` daemons.
- Detects failures and propagates host state changes.
- Coordinates master/backup failover for control services.

#### `mbatchd` — Master Batch Daemon
- Central scheduler and policy engine.
- Queries `lim` for up-to-date node information.
- Matches queued jobs to available resources.
- Dispatches start requests to `sbatchd` daemons.
- Receives job status, completion, and accounting data.

Together, `lim` and `mbatchd` represent the **control plane core**.
`lim` provides visibility; `mbatchd` provides decisions.

### Responsibilities
- Cluster topology and health monitoring.
- Scheduling policy evaluation and placement decisions.
- Authentication and job dispatch.
- High-availability coordination between master and backup.

---

## Data Plane

The data plane executes jobs and enforces resource limits on compute nodes.

### Component

#### `sbatchd` — Slave Batch Daemon
- Runs on every execution host.
- Receives job start instructions from `mbatchd`.
- Forks and monitors user processes.
- Manages job I/O, signal propagation, and accounting.
- Reports job status and resource usage back to the control plane.

### Responsibilities
- Local job lifecycle management (start, stop, monitor).
- Resource isolation (CPU, memory, GPU, cgroups, containers).
- Output redirection and file staging.
- Real-time reporting to the control plane.

Once a job is running, the data plane operates autonomously;
`mbatchd` only observes and collects status updates.

---

## Interaction Flow

   +------------------- Control Plane --------------------+
   |                                                      |
   |  lim (cluster view)  <-->  mbatchd (scheduler)       |
   +------------------------------------------------------+
                         |
                         |  job dispatch / job end
                         v
   +-------------------- Data Plane -----------------------+
   |                                                      |
   |  sbatchd (execution host) -> run job, monitor, report |
   +------------------------------------------------------+


1. **LIM** gathers host metrics and propagates state.
2. **MBATCHD** queries LIM and decides placement.
3. **MBATCHD → SBATCHD:** dispatches job start.
4. **SBATCHD** executes and supervises the job.
5. **SBATCHD → MBATCHD:** reports completion and accounting.
6. **LIM/MBATCHD** update the cluster state accordingly.

---

## Modernization Goals

| Legacy Behavior | Modernized Design |
|------------------|------------------|
| `mbatchd` started by `sbatchd` on master | `lim` supervises control-plane daemons |
| Blocking I/O with `select()` | Unified non-blocking `epoll` event loop |
| Insecure RPC messages | Authenticated control channel using `auth_token { uid, gid, nonce }` |
| Monolithic global state files | In-memory tables with explicit sync RPCs |
| Undefined failover | Backup LIM + backup MBATCHD for HA |

---

## Summary

- **Control Plane:** cluster intelligence — LIM + MBATCHD
  decides *what should run where*.
- **Data Plane:** node-level execution — SBATCHD
  makes those decisions real.

This separation clarifies roles, improves scalability, and enables future extensions such as container orchestration or hybrid Slurm/Kubernetes interoperability.

---

## Control/Data Plane Message Flow (Protocol Overview)

This section catalogs the key RPCs and their flow between components.
Conventions:
- **Dir.** is from the perspective of the arrow (A → B).
- All control-channel messages carry a **correlation id** (`corr_id`), **auth token**, and **monotonic timestamp**.
- Messages are **idempotent** when possible; retries must not duplicate effects.

### 1) LIM ⇄ MBATCHD (Control-Plane State)

| Msg                           | Dir.                  | Purpose                                   | Key Fields                                                      | Notes                                  |
|-------------------------------|-----------------------|-------------------------------------------|------------------------------------------------------------------|----------------------------------------|
| `LIM_HELLO`                   | MBATCHD → LIM         | Register scheduler with active LIM        | `version`, `node`, `pid`, `features[]`                           | First contact (after LIM election).     |
| `LIM_HELLO_ACK`               | LIM → MBATCHD         | Acknowledge + limits, timers              | `ok`, `intervals`, `capabilities`, `backup_info`                 | Includes HA role info.                  |
| `HOST_SNAPSHOT_REQ`           | MBATCHD → LIM         | Request full cluster view                 | `since_epoch`                                                    | Bootstrap or catch-up.                  |
| `HOST_SNAPSHOT_REPLY`         | LIM → MBATCHD         | Full host inventory + statuses            | `hosts[] {name,state,labels,loads,alloc}`, `epoch`               | Chunked if large.                       |
| `HOST_UPDATE`                 | LIM → MBATCHD         | Incremental host delta                    | `changes[]`                                                      | Batched; coalesced by host.             |
| `HEARTBEAT`                   | MBATCHD ↔ LIM         | Liveness & drift                          | `ts_mono`, `skew`                                                | Missed heartbeats trigger failover.     |
| `ROLE_SWITCH`                 | LIM → MBATCHD         | Master/backup role change                 | `new_role`, `reason`, `term`                                     | MBATCHD adjusts external behavior.      |

### 2) MBATCHD → SBATCHD (Dispatch) and Reports Back

| Msg                           | Dir.                  | Purpose                                   | Key Fields                                                      | Notes                                  |
|-------------------------------|-----------------------|-------------------------------------------|------------------------------------------------------------------|----------------------------------------|
| `JOB_START_REQ`               | MBATCHD → SBATCHD     | Start job on host                         | `job_id`, `array_index`, `uid,gid`, `cmd`, `env[]`, `cwd`, `resv{cpu,mem,gpu}`, `io{out,err}`, `limits`, `token` | Idempotent: duplicate requests must no-op if same `job_id`. |
| `JOB_START_ACK`               | SBATCHD → MBATCHD     | Accepted/Rejected                         | `job_id`, `ok`, `pid`, `err_code`, `diagnostic`                 | If rejected, scheduler may re-place.    |
| `JOB_STATUS_UPDATE`           | SBATCHD → MBATCHD     | Periodic/triggered status                 | `job_id`, `state`, `usage{cpu_time,mem,max_rss,gpu}`, `rc`, `signal`, `ts` | Coalesce bursts, rate-limit.            |
| `JOB_IO_NOTICE`               | SBATCHD → MBATCHD     | Optional: I/O files ready (staging)       | `job_id`, `paths[]`, `sizes[]`, `checksums[]`                   | For postproc/accounting.                |
| `JOB_END`                     | SBATCHD → MBATCHD     | Terminal status                            | `job_id`, `final_state`, `rc`, `signal`, `usage`, `exit_info`   | MBATCHD commits accounting.             |
| `JOB_SIGNAL_REQ`              | MBATCHD → SBATCHD     | Send signal to job                         | `job_id`, `signal`                                              | Ack with `JOB_SIGNAL_ACK`.              |
| `JOB_SIGNAL_ACK`              | SBATCHD → MBATCHD     | Signal delivery result                     | `job_id`, `ok`, `diagnostic`                                    |                                         |
| `JOB_KILL_REQ`                | MBATCHD → SBATCHD     | Terminate job (hard)                       | `job_id`, `reason`                                              | Followed by `JOB_END`.                  |

### 3) SBATCHD → LIM (Optional Direct Host Signals)

Normally SBATCHD reports to MBATCHD only. In degraded mode (scheduler failover),
`HOST_LOCAL_STATE` may inform LIM so it can inform the next MBATCHD.

| Msg                 | Dir.              | Purpose                        | Key Fields                               |
|---------------------|-------------------|--------------------------------|-------------------------------------------|
| `HOST_LOCAL_STATE`  | SBATCHD → LIM     | Minimal host status (degraded) | `host`, `running_jobs_count`, `health`    |

### 4) Authentication & Security

- Control messages include `auth_token { uid, gid, nonce, exp }` signed/MACed with a cluster key.
- Tokens are **short-lived**; refresh via `AUTH_REFRESH` when expiring.

| Msg             | Dir.            | Purpose                      | Key Fields                              |
|-----------------|-----------------|------------------------------|------------------------------------------|
| `AUTH_REFRESH`  | any → Auth Svc  | Obtain/renew token           | `principal`, `capabilities`, `nonce`     |
| `AUTH_REPLY`    | Auth Svc → any  | Token + expiry               | `token`, `exp`, `session_id`             |

### 5) Failure, Retry, and Idempotency

- All requests carry `corr_id` (128-bit). Retries re-use the same `corr_id`.
- Server side keeps a small **request journal** per peer: `(corr_id → last_result)`.
- **Idempotent ops** (e.g., `JOB_START_REQ` with same `job_id`) must not duplicate processes.
- Backoff: `retry = min(base * 2^k + jitter(0..J), max_backoff)`.
- Heartbeat miss thresholds:
  - LIM ↔ MBATCHD: `HB=1s`, `miss=5` → suspect; `miss=10` → failover.
  - MBATCHD ↔ SBATCHD: `HB=2s`, `miss=5` → host stale.

### 6) Minimal Sequence Diagrams

**Job dispatch happy path**
MBATCHD SBATCHD
| JOB_START_REQ(job_id=42) |
|--------------------------->|
| JOB_START_ACK(ok,pid=123) |
|<---------------------------|
| |
| (job running) |
|<----- JOB_STATUS_UPDATE ---|
| |
|<--------- JOB_END ---------|
| (final rc/usage) |


**Signal and kill**

MBATCHD SBATCHD
| JOB_SIGNAL_REQ(TERM) |
|-------------------------->|
| JOB_SIGNAL_ACK(ok) |
|<--------------------------|
| |
| (if no exit) |
| JOB_KILL_REQ(KILL) |
|-------------------------->|
| JOB_END |
|<--------------------------|


**Control-plane bootstrap**

MBATCHD LIM
| LIM_HELLO |
|----------------------------->|
| LIM_HELLO_ACK |
|<-----------------------------|
| HOST_SNAPSHOT_REQ |
|----------------------------->|
| HOST_SNAPSHOT_REPLY(full) |
|<-----------------------------|
| (incremental HOST_UPDATE...) |


### 7) Message Encoding & Transport

- **Transport:** TCP (default). UDP may be used for heartbeats (optional).
- **Framing:** fixed header `{ magic, ver, type, len, corr_id }` + payload (XDR/flat LE).
- **Compression:** optional for large snapshots (zstd).
- **Versioning:** `type` + `ver` permit additive fields; unknown fields ignored.

### 8) Control/Data Boundaries (Checklist)

- Control plane owns: queue state, placement decisions, policy, accounting commit.
- Data plane owns: process tree, cgroups, stdout/err files, local retries.
- Never block control-plane event loop on data-plane I/O; use async RPC with timeouts.
- All state transitions are **event-sourced** (loggable) to enable HA replay.

---
