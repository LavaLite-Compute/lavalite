# What Is LavaLite?

LavaLite is a modern open-source HPC workload scheduler focused on deterministic behavior, explicit state management, and operational simplicity.

The project is designed for high-performance computing environments where reliability, predictable recovery, and sustained scheduling throughput are critical. LavaLite emphasizes clear scheduler semantics, durable state handling, and low operational complexity rather than feature accumulation.

LavaLite is built around a small number of core principles:

* Explicit finite-state job lifecycle management
* Deterministic scheduler behavior
* Durable on-disk state representation
* Predictable restart and recovery semantics
* Minimal subsystem coupling
* Clear operational boundaries


Job state, host state, queue state, and inter-daemon coordination are designed around
explicit state transitions and replayable event flow. Inter-daemon and library-to-daemon
communication uses a simple explicit wire protocol with well-defined message formats and
acknowledgment events.

LavaLite supports traditional HPC clusters, parallel workloads, GPU-aware scheduling, distributed scientific computing, and AI/ML execution environments where scheduler correctness and operational visibility matter more than feature breadth.

The architecture favors:

* Simple and inspectable control flow
* Explicit resource accounting
* Stable protocol semantics
* Reduced combinatorial complexity
* High sustained scheduling throughput

Rather than accumulating loosely coupled features, LavaLite focuses on building a scheduler core that is operationally predictable, debuggable, and recoverable.

# About the Name

The term *Lite* in LavaLite does not mean reduced functionality.

It reflects the project's architectural philosophy:

* Lightweight core design
* Minimal operational surface area
* Deterministic behavior
* Reduced internal complexity
* Clear subsystem boundaries

The objective is not to implement every possible scheduler feature, but to build a robust and understandable HPC scheduling system with predictable operational semantics.

# Current Status

LavaLite 1.0.0 has been released.

This release establishes the initial stable foundation of the project, including:

* Core batch scheduling
* Multi-host scheduling
* GPU-aware scheduling
* Durable event logging and replay
* Restart and recovery support
* Administrative and user command-line tools
* Comprehensive administrator and user documentation

Current development on the `master` branch focuses on:

* Stability improvements
* Bug fixes
* Performance tuning
* Documentation enhancements
* User feedback and feature evaluation

The `lavalite-1.0.0` tag identifies the exact source tree used for the 1.0.0 release.

# Supported Platforms

LavaLite currently supports:

* Rocky Linux 8
* Rocky Linux 9
* Ubuntu 24.04

# Documentation

The primary documentation is located under:

```text
docs/admin
docs/testing
docs/man

# License

LavaLite is licensed under the GNU General Public License v2 (GPLv2).
All contributions must be compatible with GPLv2.

# Contact

Issues and feature requests:

* [https://github.com/LavaLite-Compute/lavalite/issues](https://github.com/LavaLite-Compute/lavalite/issues)

Maintainer:

* [lavalite.compute@gmail.com](mailto:lavalite.compute@gmail.com)
