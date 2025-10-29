# LavaLite Control Plane vs Data Plane — Architecture Proposal

> Purpose: clarify roles, startup/HA flow, and interfaces between **LIM**, **mbatchd**, and **sbatchd**, modernizing LavaLite around a clean control/data separation.

---

## 1) Executive Summary

* **Control plane**: `LIM` (cluster state + leader election) and `mbatchd` (scheduling).
* **Data plane**: `sbatchd` on each worker (job execution only).
* **Key change**: **LIM (master) starts and supervises `mbatchd`**. `sbatchd` no longer starts `mbatchd`.
* **HA**: master/backup LIM; backup takes over, starts a fresh `mbatchd`.

---

## 2) Terminology

* **Control plane**: decides *what should happen* (state, policy, scheduling).
* **Data plane**: *executes* decisions (run/monitor jobs).
* **FIB (analogy)**: scheduler decisions distilled to actionable dispatch commands.
* **VIP**: virtual IP (optional) that always points to the master control endpoint.

---

## 3) Current (Legacy) Behavior & Issues

* `sbatchd` on master host can start `mbatchd` → **couples data plane to control plane**.
* Failover semantics are implicit and brittle; unclear ownership of `mbatchd` lifecycle.
* Split-brain risks in multi-node deployments if two `sbatchd` attempt control actions.

---

## 4) Proposed Architecture (Clean Split)

### Roles

* **LIM (control, HA orchestrator)**

  * Cluster discovery, heartbeats, resource inventory.
  * **Leader election** (master/backup or quorum later).
  * **Starts & supervises `mbatchd`** on the elected master node.
  * Exposes fast, local control endpoint to `mbatchd` (Unix socket).
* **mbatchd (control, scheduler)**

  * Queues, priorities, placement decisions.
  * Pulls resource state from LIM; dispatches to sbatchd.
* **sbatchd (data plane)**

  * Pure execution agent: start/monitor/stop jobs; stream I/O; report status.
  * Registers to current master `mbatchd`; no orchestration responsibilities.

### Control/Data Separation

* `LIM + mbatchd` = **control plane**.
* `sbatchd` = **data plane**.

---

## 5) Boot & Supervision Flow

1. **All nodes**: `lim` starts at boot.
2. **Leader election**: LIMs elect a **master**; others become **backup**.
3. **Start scheduler**: Master LIM **starts `mbatchd`** (systemd or fork/exec + watchdog).
4. **Registration**:

   * `mbatchd → LIM(master)`: announce ready (version/build/token).
   * `sbatchd(all) → mbatchd(master)`: register resources.
5. **Steady state**: LIM maintains heartbeats; `mbatchd` schedules; `sbatchd` runs jobs.

---

## 6) Leader Election (v1 Simple, v2 Quorum)

### v1: Master/Backup with Heartbeats

* Static priority or tie-breaker (node ID).
* Master sends heartbeats; backup monitors `T_fail = N * interval`.
* On master loss: backup **assumes leadership**, starts `mbatchd`, claims VIP/lock.

### v2: Quorum (Future)

* 3+ LIMs using a tiny Raft-like consensus or witness node to avoid split-brain.

**Fencing/Locking**

* Use one of:

  * VIP ownership (ARP/NDP),
  * Lock file on shared storage (`/var/run/lavalite/master.lock`),
  * Single-writer token store.

---

## 7) Control/Data Channels

* **LIM ⇄ mbatchd**: Unix domain socket (local-only), request/response; small binary protocol.
* **mbatchd ⇄ sbatchd**: TCP Fast Channel with reconnect, token auth.
* **Security**: short-lived HMAC or Ed25519 tokens; rotation by LIM; TLS optional later.

---

## 8) API Surface Changes (Minimal)

* Remove “`sbatchd starts mbatchd`” code path.
* Add **LIM→mbatchd** lifecycle hooks: `start`, `stop`, `status`.
* Add **LIM master announcement** socket for `mbatchd` discovery.
* `sbatchd` discovers `mbatchd` via DNS/VIP (e.g., `mbatchd.service.cluster.local`) or LIM hint.

---

## 9) Systemd Units (Reference)

**`/etc/systemd/system/lavalite-lim.service`**

```ini
[Unit]
Description=LavaLite LIM (cluster manager)
After=network-online.target
Wants=network-online.target

[Service]
User=lavalite
ExecStart=/usr/sbin/lavalite-lim --config /etc/lavalite/lim.conf
Restart=always
RestartSec=2

[Install]
WantedBy=multi-user.target
```

**`/etc/systemd/system/lavalite-mbatchd.service`**

```ini
[Unit]
Description=LavaLite mbatchd (scheduler)
After=network-online.target
ConditionPathExists=/var/run/lavalite/master.lock

[Service]
User=lavalite
ExecStart=/usr/sbin/lavalite-mbatchd --config /etc/lavalite/mbatchd.conf
Restart=always
RestartSec=2

[Install]
WantedBy=multi-user.target
```

**Master LIM actions (pseudo)**

```c
on_become_master() {
    create("/var/run/lavalite/master.lock");
    system("systemctl start lavalite-mbatchd.service");
}

on_lose_master() {
    system("systemctl stop lavalite-mbatchd.service");
    unlink("/var/run/lavalite/master.lock");
}
```

---

## 10) Failure Scenarios & Behavior

* **mbatchd crash**: LIM restarts it (backoff). Jobs keep running on sbatchd.
* **Master LIM crash**: Backup assumes leadership → starts mbatchd → sbatchd reconnects.
* **Network partition**: with 2 nodes, require a **witness/lock** to prevent split-brain.
* **Scheduler upgrade**: LIM stops mbatchd, replaces binary, restarts; backup takes over if needed.

---

## 11) Observability & Health

* Heartbeats across all daemons with latency and loss metrics.
* `/status` endpoint or syslog tags: `lim=master|backup`, `mbatchd=running`, `sbatchd=jobs=N`.
* Expose Prometheus metrics (future): queue depth, dispatch rate, host availability.

---

## 12) Security & Identity

* Node identity: host key or signed token from LIM.
* Tokens: short-lived, audience-limited (LIM→mbatchd, mbatchd→sbatchd).
* Optional mTLS for mbatchd⇄sbatchd when crossing L3 domains.

---

## 13) Migration Plan (Incremental)

1. **Phase 0**: keep current behavior; add `--lim-supervises-mbatchd` feature flag.
2. **Phase 1**: LIM starts/stops mbatchd; sbatchd path deprecated but still available.
3. **Phase 2**: remove sbatchd→mbatchd startup code; make LIM supervision the default.
4. **Phase 3**: introduce simple witness/lock; optional VIP.
5. **Phase 4**: (future) quorum-based election.

---

## 14) Open Questions

* Do we require a VIP, or is DNS flip sufficient?
* Preferred locking primitive: shared FS lock vs. VIP vs. token KV?
* Where to persist control state for warm restarts (jobs remain running)?
* Exact binary protocol for LIM⇄mbatchd (versioning, backward compatibility).

---

## 15) Glossary

* **LIM**: Load Information Manager (cluster monitor & master election).
* **mbatchd**: Master batch daemon (scheduler/control).
* **sbatchd**: Slave batch daemon (execution/data plane).
* **Control plane**: state, policy, decisions.
* **Data plane**: execution, job processes, I/O.
* **VIP**: Virtual IP for master endpoint stability.

---

## 16) Rationale & Benefits

* **Correctness**: clear ownership of `mbatchd` lifecycle, fewer edge cases.
* **Resilience**: HA by design; data plane keeps running across control restarts.
* **Clarity**: future contributors can reason about the system more easily.
* **Modernity**: aligns with Slurm/K8s patterns (controller vs. agent).
