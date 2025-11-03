# LavaLite MVP Checklist (HTC Lane)

A working **Minimum Viable Product** for LavaLite — focused on
high-throughput serial jobs, simple queueing, and remote control via SSH.

---

## 1. Core Components
- [ ] **lim** — Load collector (CPU, memory, alive)
- [ ] **sbatchd** — Job runner (fork/exec, cgroup, stdout/err)
- [ ] **mbatchd** — Scheduler + queue manager (FIFO or per-user fair cap)
- [ ] **CLI tools** — `bsub`, `bjobs`, `bkill`, `llctrl`
- [ ] **Config files** — `lavalite.conf`, `lim.conf`, `sbatchd.conf`, `queues.conf`
- [ ] **Persistence** — SQLite `lavalite.db` (jobs, history, users, queues)

---

## 2. Binaries / Services
- [ ] `/usr/sbin/lavalite-lim` (`lavalite-lim.service`)
- [ ] `/usr/sbin/lavalite-sbatchd` (`lavalite-sbatchd.service`)
- [ ] `/usr/sbin/lavalite-mbatchd` (`lavalite-mbatchd.service`)
- [ ] `/usr/bin/bsub`, `/usr/bin/bjobs`, `/usr/bin/bkill`
- [ ] `/usr/bin/llctrl` (pdsh/ssh wrapper)

---

## 3. Minimal Configs
- [ ] `lavalite.conf` — paths, database, logging
- [ ] `queues.conf` — at least one queue (`serial`); limits: MXJ/user, walltime default
- [ ] `sbatchd.conf` — work dir, cgroup toggle, stdout/err policy
- [ ] `lim.conf` — poll interval (3–5 s), jitter, host labels
- [ ] `/etc/sudoers.d/lavalite` for `lavalite-admin`

---

## 4. Protocol / IPC (keep tiny)
- [ ] **Submit**: `bsub → mbatchd` (local UNIX or loopback TCP)
- [ ] **Dispatch**: `mbatchd → sbatchd` (start job, env, cwd, limits)
- [ ] **Report**: `sbatchd → mbatchd` (START, EXIT, FAIL)
- [ ] **LIM → mbatchd**: periodic host metrics (cpu, mem, slots, drain)

---

## 5. Scheduling Policy (MVP)
- [ ] FIFO per queue
- [ ] Per-user running cap (`max_running_user`)
- [ ] Global queue cap (`max_running_queue`)
- [ ] Simple placement: host with free slot and not drained

---

## 6. Job Execution
- [ ] Per-job workdir: `/var/spool/lavalite/jobs/<jobid>/`
- [ ] Capture `stdout`/`stderr` to files (truncate/rotate)
- [ ] Timeout: wallclock kill (TERM → KILL)
- [ ] Retries: ≤ N attempts with 10 s backoff
- [ ] Record exit reason (exit code, signal, OOM, timeout)

---

## 7. Cgroup v2 Enforcement
- [ ] Create `/sys/fs/cgroup/lavalite/<jobid>/`
- [ ] Set `memory.max`, `pids.max`, optional `cpu.max`
- [ ] Kill via `cgroup.kill` or PGID sweep
- [ ] Clean up cgroup on exit

---

## 8. Logging & Metrics
- [ ] Syslog daemon facility; one line per state change
- [ ] Prometheus metrics:
  - `lavalite_queue_depth{queue=…}`
  - `lavalite_jobs_started_total{queue,user}`
  - `lavalite_jobs_failed_total{reason}`
  - `lavalite_dispatch_latency_ms`
  - `lavalite_host_free_slots{host}`
- [ ] Log rotation under `/var/log/lavalite/`

---

## 9. CLI Behavior
- [ ] `bsub [-q serial] [-o out] [-e err] [--time SECS] <cmd...>`
- [ ] `bjobs [-a] [jobid]` — show ID, state, host, exit, start/end, attempts
- [ ] `bkill <jobid>` — TERM→KILL; idempotent
- [ ] `llctrl` — SSH/pdsh wrapper: `lim|sbatch {status|restart|reload|config-check}`

---

## 10. Admin Plane (SSH / pdsh)
- [ ] Require OpenSSH + key authentication
- [ ] Management user: `lavalite-admin`
- [ ] `pdsh -R ssh -l lavalite-admin -w ^nodes.txt -- 'sudo systemctl restart lavalite-sbatchd'`
- [ ] Thin wrapper `llctrl` implemented

---

## 11. Persistence (SQLite)
- [ ] Tables: `jobs`, `job_events`, `queues`, `hosts`, `users`
- [ ] Crash recovery: reload queued/running jobs
- [ ] Minimal schema; single `schema.sql`

---

## 12. Build & Install
- [ ] `make && make install`
- [ ] systemd units enabled for lim/sbatchd/mbatchd
- [ ] tmpfiles rules for:
  - `/var/spool/lavalite 0755 lavalite lavalite -`
  - `/var/log/lavalite 0755 lavalite lavalite -`

---

## 13. MVP Test Plan

### A) Smoke Tests
- [ ] `llctrl --hosts controller lim status`
- [ ] `llctrl --file workers.txt sbatch status`
- [ ] `llctrl --hosts controller lim config-check`
- [ ] `llctrl --file workers.txt sbatch config-check`

### B) Basic Job Flow
- [ ] Submit 10 trivial jobs → all **DONE**
- [ ] Verify stdout/stderr capture
- [ ] Verify per-user running cap

### C) Timeout & Retries
- [ ] Job exceeds wallclock → **FAILED(timeout)**
- [ ] Job with retries → stops after N attempts

### D) Kill Semantics
- [ ] `bkill <jobid>` → job transitions to **KILLED**

### E) Cgroup Enforcement
- [ ] Job exceeding `memory.max` → **FAILED(OOM)**

### F) Throughput
- [ ] Burst 1 000 jobs → drain cleanly; record latency

### G) Recovery
- [ ] Restart `mbatchd`; queued/running jobs preserved

### H) Admin Plane
- [ ] `llctrl --file workers.txt sbatch restart` works
- [ ] `llctrl --file workers.txt sbatch status` reports correctly

---

## 14. Security & Ops
- [ ] SSH keys only (no password auth)
- [ ] Least-privilege sudo
- [ ] Hostkey verification enabled
- [ ] Dedicated `lavalite` user/group for daemons
- [ ] Log admin actions (user, host, command, time)

---

## 15. MVP Done Criteria
- [ ] End-to-end run of 1 000 jobs completes cleanly
- [ ] Per-user cap enforced
- [ ] Timeouts & retries functional
- [ ] Kill, drain, restart via `llctrl`
- [ ] Metrics exported and readable
- [ ] Crash recovery verified

---

### Notes
Mark each item as you implement or test it (`[x]` = done).
Once all boxes under **MVP Done Criteria** are checked, the core HTC lane is production-ready and you can begin adding advanced features (arrays, DAGs, predictive scaling, etc.).

