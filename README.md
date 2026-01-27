## What Is **LavaLite**?

## What Is **LavaLite**?

**LavaLite** is a modern, independent, open‑source continuation of the original open‑source Platform Lava
scheduler. It preserves the lineage but not the baggage: the old codebase serves as historical documentation, not a blueprint. We kept the ideas that worked, removed the subsystems that never did, and rewrote the rest with modern clarity and scale in mind. The goal is simple — a scheduler that can run millions of jobs reliably while staying small, predictable, and easy to reason about. **LavaLite** is its own system now: leaner, cleaner, and built for real throughput, while remaining completely independent of Platform Computing, IBM, NVIDIA, or any other vendor.

**LavaLite** is a lightweight, high‑throughput job scheduler built for clusters where job volume matters more than tightly coupled parallel execution. It excels at running thousands of short, independent jobs with minimal overhead and consistent behavior.

Beyond traditional HPC, **LavaLite** is a natural fit for HTC workloads: massive job arrays, embarrassingly parallel tasks, data‑driven pipelines, and large‑scale scientific workflows. It performs well on supercomputers, research clusters, bioinformatics labs, media render farms, and cloud batch environments.

**LavaLite** is also well‑suited for AI and machine‑learning pipelines — from distributed GPU training to hyperparameter sweeps and inference farms. Its straightforward interface and low runtime overhead make it ideal for researchers and engineers pushing modern computational throughput.

Inspired by Platform Lava 1.0 and refined using classical algorithmic discipline, **LavaLite** is designed for **speed, simplicity, and clarity**. It is not a clone — it is a clean **reimplementation and a reinvention**.

**LavaLite originates from the last GPLv2‑licensed Platform Lava 1.0 codebase and evolves independently from there.**

### MPI Workloads

LavaLite can run MPI workloads through standard job submission. The scheduler
allocates `-n` hosts for the job, and the user (or their workflow tools)
constructs an MPI hostfile at runtime based on the provided allocation.
This model supports distributed GPU training, multi-node inference, and
other MPI-based workloads without requiring tight parallel scheduling.

A simple hostfile template mechanism is planned, allowing users to define
how allocated nodes should be rendered into `mpirun`-compatible hostfiles.

## GPU Support & Data Center Modeling (Planned)

LavaLite’s architecture makes GPU-aware scheduling and data center modeling
natural extensions of the core system. These capabilities are planned as
**near-future development**, building on the lightweight dispatch model already in place:

- **GPU visibility and accounting** (host-level and job-level)
- **device allocation masks** for CUDA, MIG, and ROCm
- **GPU resource selection policies** (constraints, device classes, vendor extensions)
- **rack-aware and topology-aware placement**, improving bandwidth locality
- scheduling models that incorporate **NUMA domains, racks, rows, power zones, and cooling limits**
- **template-based hostfile generation** for GPU-based MPI workloads

These features will extend LavaLite beyond HTC workloads to support **GPU-intensive AI
training and data center–scale resource management**, while keeping scheduling behavior
**clear, predictable, and easy to reason about.**


## Origins & Philosophy
LavaLite builds on the GPLv2-licensed foundation originally released by Platform Computing. It maintains a clear lineage while avoiding historical entanglements, focusing instead on clarity, performance, and modern design.

Our approach is shaped by the rigor of classical algorithm design, as exemplified in Donald Knuth’s The Art of Computer Programming. LavaLite is optimized for
high-throughput computing (HTC) and engineered for speed, simplicity, and transparency.

LavaLite is not a clone — it is a clean reimplementation guided by the principles that inspired its lineage. The goal is direct: well-understood behavior, minimal overhead, and algorithms that remain readable years later.

## Getting started

LavaLite currently supports **Rocky Linux 8**, **Rocky Linux  9** and **Ubuntu 24.04**.
Compatibility with other distributions (e.g., Debian, AlmaLinux) is
planned for future releases.

## Quick Install

```bash
git clone https://github.com/LavaLiteProject/LavaLite.git
cd LavaLite
make
sudo make install
Installs in /opt/lavalite-0.1.0
```
LavaLite installs itself under /opt/lavala-<version> in a self-contained
directory.  This approach avoids polluting system paths like /usr/local and
allows multiple versions to coexist seamlessly. Multiple version can be
supported using for example environmental modules, which is the recommended
approach for managing HPC software.

## Documentation

LavaLite documentation is evolving and currently hosted in the repository:

- https://github.com/LavaLite-Compute/lavalite/tree/master/docs/projects

These documents outline internal design notes, early interfaces, and ongoing
development directions. They are actively maintained as core functionality
stabilizes.


## Roadmap

- Clean, modular scheduler core
- REST API for job submission and monitoring
- Container-native job support (e.g., Docker/Singularity)
- Metrics and logging improvements
- CI/CD integration and test coverage
- Web-based dashboard (experimental)

## License
LavaLite is licensed under the GNU General Public License v2. All contributions must be
compatible with GPLv2.


## References

- *The C Programming Language*, Brian W. Kernighan and Dennis M. Ritchie. Prentice Hall, 2nd Edition, 1988.
- *Advanced Programming in the UNIX Environment*, W. Richard Stevens, Stephen A. Rago. Addison-Wesley, 3rd Edition, 2013.
- *TCP/IP Illustrated, Volume 1: The Protocols*, W. Richard Stevens. Addison-Wesley, 1st Edition, 1994.
- ISO/IEC 9899:1999 — Programming Languages — C (C99). International Organization for Standardization.
- IEEE Std 1003.1 — POSIX.1-2017 — Standard for Information Technology — Portable Operating System Interface. The Open Group / IEEE.
- *The Art of Computer Programming*, Donald E. Knuth. Addison-Wesley. All rights reserved to the author.

## Contact

For issues and feature requests, please use the
[GitHub Issues page](https://github.com/lavalite/livelite/issues).

Maintainer: lavalite.compute@gmail.com
Project email: livelite@lavalite-compute.io
