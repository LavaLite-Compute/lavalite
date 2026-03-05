## What Is **LavaLite**?

**LavaLite** is a modern, independent, open-source workload scheduler derived from the last GPLv2-licensed release of Platform Lava 1.0. The historical codebase serves as reference material, not as a direct continuation.

LavaLite is a clean reimplementation with a redesigned core and a simplified,
deterministic architecture built from scratch—while still preserving the conceptual lessons and operational patterns that proved effective in the original.

LavaLite is built for high-throughput and high-performance computing (HTC/HPC)
environments where job volume and predictable behavior are critical. It is optimized for running large numbers of short, independent jobs with minimal scheduling overhead.

The system is designed around a deterministic architecture:

- Explicit job states
- Well-defined state transitions
- Durable on-disk state representation
- Deterministic recovery after restart

Its behavior is intended to be understandable, reconstructable, and operationally predictable.

LavaLite supports traditional HPC clusters as well as HTC workloads such as large job arrays, data-processing pipelines, and distributed scientific workflows. It is also suitable for AI and machine-learning workloads where scheduling overhead and state consistency are critical.

The project evolves independently and focuses on:

- Clear operational semantics
- Controlled configuration surface
- Minimal subsystem coupling
- High sustained throughput

## About the Name

The term *Lite* in **LavaLite** does not indicate a reduced-feature
or limited edition, it reflects the project's design philosophy: a lightweight,
minimal, and deterministic architecture focused on clarity,
reliability, and operational predictability rather than feature accumulation.

**LavaLite is not a feature clone of legacy systems. It is a deliberately constrained design aimed at reducing combinatorial complexity while maintaining scalability.**

## Getting Started

LavaLite currently supports:

- Rocky Linux 8
- Rocky Linux 9
- Ubuntu 24.04

To begin, follow the installation guide:

https://github.com/LavaLite-Compute/lavalite/blob/master/docs/projects/ADMIN-lavalite-0.1.1.md

## Documentation

Project documentation is maintained in the repository:

https://github.com/LavaLite-Compute/lavalite/tree/master/docs/projects

These documents describe internal design, interfaces, and operational details.

## Road to 1.0

The 0.1.x series establishes the core: reliable job lifecycle, durable state,
and predictable recovery under failure. The focus is correctness and operational
stability, not feature breadth.

Work planned for 1.0 includes:

- Multi-host job allocation (`-n`)
- `eauth` cluster authentication (design complete, implementation pending)
- Failover and master election
- GPU and resource-aware scheduling

No timeline is committed. 1.0 ships when it is ready and verified.

## License

LavaLite is licensed under the GNU General Public License v2. All contributions must be
compatible with GPLv2.

## Contact

For issues and feature requests, please use the
[GitHub Issues page](https://github.com/LavaLite-Compute/lavalite/issues).

Maintainer: lavalite.compute@gmail.com
