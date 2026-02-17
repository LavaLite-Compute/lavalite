
## What Is **LavaCore**?

**LavaCore** is a modern, independent, open-source workload scheduler derived from the last GPLv2-licensed release of Platform Lava 1.0. The historical codebase serves as reference material, not as a direct continuation. LavaCore is a clean reimplementation with a redesigned core and simplified architecture.

LavaCore is built for high-throughput computing (HTC) environments where job volume and predictable behavior are more critical than tightly coupled parallel execution. It is optimized for running large numbers of short, independent jobs with minimal scheduling overhead.

The system is designed around a deterministic architecture:

- Explicit job states
- Well-defined state transitions
- Durable on-disk state representation
- Deterministic recovery after restart

Its behavior is intended to be understandable, reconstructable, and operationally predictable.

LavaCore supports traditional HPC clusters as well as HTC workloads such as large job arrays, data-processing pipelines, and distributed scientific workflows. It is also suitable for AI and machine-learning workloads where scheduling overhead and state consistency are critical.

The project evolves independently and focuses on:

- Clear operational semantics
- Controlled configuration surface
- Minimal subsystem coupling
- High sustained throughput

LavaCore is not a feature clone of legacy systems. It is a deliberately constrained design aimed at reducing combinatorial complexity while maintaining scalability.


### MPI Workloads

LavaCore can run MPI workloads through standard job submission. The scheduler allocates `-n` hosts for a job, and the user (or workflow tooling) generates an MPI hostfile based on the provided allocation.

This model supports distributed GPU training, multi-node inference, and other MPI-based workloads without requiring tightly coupled parallel scheduling semantics.

A hostfile template mechanism may be introduced to simplify generation of `mpirun`-compatible hostfiles.

## GPU Support & Data Center Modeling

GPU-aware scheduling and data center modeling are natural extensions of the current architecture. Potential extensions include:

- GPU visibility and accounting (host-level and job-level)
- Device allocation masks for CUDA, MIG, and ROCm
- GPU resource selection policies and constraints
- Rack-aware and topology-aware placement
- Scheduling models incorporating NUMA domains, racks, rows, and power zones
- Template-based hostfile generation for GPU-based MPI workloads

Any such extensions will preserve deterministic scheduling behavior and explicit state semantics.

## Getting Started

LavaCore currently supports:

- Rocky Linux 8
- Rocky Linux 9
- Ubuntu 24.04

To begin, follow the installation guide:

https://github.com/LavaLite-Compute/lavalite/blob/master/docs/projects/INSTALL-lavacore-0.1.0.md

## Documentation

Project documentation is maintained in the repository:

https://github.com/LavaLite-Compute/lavalite/tree/master/docs/projects

These documents describe internal design, interfaces, and operational details.


## License
LavaLite is licensed under the GNU General Public License v2. All contributions must be
compatible with GPLv2.


## Contact

For issues and feature requests, please use the
[GitHub Issues page](https://github.com/lavalite/livelite/issues).

Maintainer: lavalite.compute@gmail.com
Project email: livelite@lavalite-compute.io
