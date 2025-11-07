
Default weights: `w1=1.0`, `w2=0.0`, `w3=0.0`.

Future options:
- Pack vs. spread modes.
- Affinity or anti-affinity.
- Data locality and cache metrics.

### 5.2 Allocation

| Job Type | Action |
|-----------|--------|
| **Exclusive** | Pick one empty host with best score. |
| **Non-exclusive** | Take top-K candidates (default K = 10) by score, allocate greedily until required slots are satisfied. |

If the job cannot get enough slots → `pend_reason(INSUFFICIENT_SLOTS)`.

---

## 6. Dispatch and Accounting

When a job is dispatched:

1. Send dispatch command to target `sbatchd`.
2. Mark job as `RUNNING`.
3. Update counters:
   - `host.free_slots -= n;`
   - `host.mxj_total++`, `host.mxj_per_user[user]++`;
   - `user.running_jobs++`, `project.running_jobs++`.
4. Log decision details.

---

## 7. Pending Reasons

| Code | Meaning |
|------|----------|
| `NO_HOST` | No host matched requirements. |
| `INSUFFICIENT_SLOTS` | Not enough free slots. |
| `LIMITS` | User or project limits reached. |
| `START_TIME` | Job scheduled for future execution. |
| `ON_HOLD` | Manually held job. |
| `LICENSE` | Required license unavailable. |

---

## 8. Data Structures

| Entity | Key Fields |
|---------|-------------|
| **Host** | `hostname`, `total_slots`, `free_slots`, `mxj_total`, `mxj_per_user[]`, `features[]`, `labels[]`, `load1` |
| **User** | `uid`, `running_jobs`, `slots_in_use`, `project_id` |
| **Queue** | `max_running`, `priority`, `host_bitmap`, `policy_flags` |
| **Job** | `jobid`, `user`, `queue`, `nslots`, `flags` (`exclusive`, `hold`), `start_time`, `features[]` |

Implementation notes:
- Bitsets are used for host filtering.
- A small heap or partial sort yields the top-K candidates efficiently.

---

## 9. Pseudocode Summary

```c
for job in priority_order() {
    if (!ready(job)) { pend(job, reason); continue; }
    if (!within_limits(job.user, job.project)) { pend(job, LIMITS); continue; }

    bitset C = hosts_ready & queue_hosts[job.q] & features[job.req] & labels[job.req];
    filter_mxj(C, job.user);
    if (job.exclusive) filter_empty(C);

    if (empty(C)) { pend(job, NO_HOST); continue; }

    candidates = topK(C, 10, score_free_slots);
    alloc = allocate_slots(candidates, job.slots, job.exclusive);

    if (!alloc.ok) { pend(job, INSUFFICIENT_SLOTS); continue; }

    dispatch(job, alloc);
    update_accounting(job, alloc);
}

