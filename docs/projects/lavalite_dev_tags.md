#
# LavaLite Development Release Path

This document tracks the internal LavaLite development milestones and tag progression.
Each tag represents a small, reviewable, reproducible step — **one theme per tag**, KISS-style.

---

## Tagging Policy

- Tags follow the format `lavalite-<semver>`.
- Use annotated tags (`git tag -a`) with a short description.
- Tags are chronological, not strictly semantic-versioned API promises.
- Every tag must build cleanly with:
  ```bash
  ./bootstrap.sh && ./configure && make -j
  make distcheck
  act -j lavalite-build     # local CI parity
```

| Tag                | Theme                                       | Highlights                                                                                                                                                                              |
| ------------------ | ------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **lavalite-0.1.0** | Initial import / restructure                | Baseline from Platform Lava 1.0 foundation; cleaned tree; Autotools bootstrap; repo layout.                                                                                             |
| **lavalite-0.1.1** | Header modernization / drop remote-exec     | Removed remote-exec path and I18N macros; cleaned headers and unused APIs; unified size limits.                                                                                         |
| **lavalite-0.1.2** | XDR cleanup + buffer unification + CI green | Centralized buffer sizes; rebuilt XDR array + string marshaling with correct padding; introduced `xdr_pack_hdr()`; consolidated XDR functions into one library; CI and `act` both pass. |

| Planned Tag        | Theme                       | Deliverables                                                                                                                        |
| ------------------ | --------------------------- | ----------------------------------------------------------------------------------------------------------------------------------- |
| **lavalite-0.1.3** | Minimal host lib            | New `lib.host.{c,h}` implementing `get_host_by_name()` / `get_host_by_addr()`; thread-safe `sockAdd2Str_()` replacement; IPv4-safe. |
| **lavalite-0.1.4** | Event loop → `epoll()`      | Replace `select()` with level-triggered `epoll`; integrate `eventfd` / `timerfd`; single-threaded reactor owns all mutable state.   |
| **lavalite-0.1.5** | `sbatchd` job launch path   | Idempotent start messages `(job, gen)`; acknowledgements `(job, gen, pid)`; back-pressure and admission control.                    |
| **lavalite-0.1.6** | Simplified resource grammar | KISS parser for `-R` requirements; explicit unmet-resource reporting; table-driven evaluator.                                       |
| **lavalite-0.1.7** | Job array parser v2         | Compact range syntax; deterministic ordering `(prio, ts, id)`; O(1) iterator.                                                       |

| Tag                 | Theme                    | Notes                                                                       |
| ------------------- | ------------------------ | --------------------------------------------------------------------------- |
| **lavalite-0.0.9**  | MBD decoupled from SBD   | LIM now starts MBD; SBD communication independent.                          |
| **lavalite-0.0.10** | Limit mastership fan-out | Restrict cluster mastership to 2 – 3 hosts max to reduce floods and storms. |
