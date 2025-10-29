# **LavaLite Modernization Manifest**

**Author:** LavaLite Contributors
**Date:** _Today_

---

## 🧭 Vision

LavaLite is a lightweight High Throughput Computing (HTC) scheduler designed to continue the legacy of Platform Lava while evolving it into a modern, data-driven, and extensible system.

The goal is not to reinvent job scheduling, but to make the proven LSF model **2025-ready**: minimal, clean, and open.

---

## 💡 Rationale

There is a need for simple batch execution and analytics workflows that do not rely on container orchestration or heavyweight HPC schedulers.

Simple does not mean unsophisticated — it means fast, intuitive, and maintainable. **LavaLite** addresses this need with a lightweight scheduler rooted in the legacy of Platform Lava 1.0 (2007).

---

## 🔑 Core Principles

1. **KISS & K&R C** – Minimal, standard C. No decoration.
2. **Functional Parity with LSF** – Preserve existing user workflows (`bsub`, `bjobs`, `bhosts`, etc.).
3. **Modernization through Simplification** – Remove obsolete components (`res/nios`, i18n catalogs) and replace them with minimal equivalents.
4. **Data-Driven Design** – Replace static analyzers with open data exports for Python, R, NumPy, and Grafana.
5. **Streaming by Default** – Event-driven state reporting via Redis, ZeroMQ, or WebSockets.
6. **Open Interfaces** – REST or gRPC endpoints for job submission and monitoring.
7. **Scriptable Analytics** – Let users analyze data directly with R or Python instead of proprietary GUIs.
8. **Compatibility Layer** – Transitional `#define`s for CamelCase → `snake_case` migration.
9. **Transparent Scheduling Logic** – Algorithms remain explicit, inspectable, and modifiable.
10. **No Decoration** – Every line of code has purpose.

---

## 🛠️ Modernization Roadmap

### Phase 1 – Core Cleanup
- Remove legacy i18n, RES/NIOS components
- Fix unsafe `sprintf()` use

### Phase 2 – Libraries and Daemons
- Rebuild batch library, `mbatchd`, `sbatchd`
- Replace inter-daemon channels with sockets

### Phase 3 – Data Streaming Layer
- Publish metrics via Redis or REST
- Store in SQLite or NoSQL backend

### Phase 4 – CLI & API Modernization
- Maintain backward compatibility
- Add JSON/YAML output
- Add REST endpoints

### Phase 5 – Analytics and Prediction
- Expose data to Python/R notebooks
- Integrate predictive models

### Phase 6 – Long-Term
- Add optional container/Kubernetes integration
- Introduce plugin framework

---

## 🧠 Design Philosophy

> _“Code is not decor, it's machinery.”_

LavaLite must remain small enough for one engineer to audit in a week.
Reliability and clarity outweigh trendiness.

---

## 🧪 Summary

LavaLite brings the proven batch scheduling model into the data-centric era by modernizing through **subtraction**, not addition.

It aims to be the **researcher's scheduler** —
small, understandable, streamable, and open.
