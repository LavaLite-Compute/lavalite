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

LavaLite schedules batch jobs and long-running services, such as Jupyter or code-server, on HPC clusters: serial and multi-host jobs, GPU workloads, scientific computing and AI/ML environments where scheduler correctness and operational visibility matter more than feature breadth. For a multi-host job LavaLite allocates the hosts and starts the job on the first one; starting processes on the others is up to the application or its launcher.

The architecture favors:

* Simple and inspectable control flow
* Explicit resource accounting
* Stable protocol semantics
* Reduced combinatorial complexity
* High sustained scheduling throughput

Rather than accumulating loosely coupled features, LavaLite focuses on building a scheduler core that is operationally predictable, debuggable, and recoverable.

# Get the code

Source code: https://codeberg.org/lavalitecompute/lavalite
(mirrored on GitHub: https://github.com/LavaLite-Compute/lavalite)

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

LavaLite 2.0.0 has been released.

2.0.0 adds services, long-running applications started by users on
demand and reached over the network, each instance an ordinary job in
its own network namespace. It also hardens the daemons for failures:
unresponsive and failing hosts, daemons killed or stopped under load,
configuration errors, and recovery from the manifest.

See `docs/RELEASE-lavalite-2.0.0.md` for the complete release notes.

Earlier releases:

* 1.1.0: job arrays, job dependencies, a ~125x `bhist` speedup,
  removal of the `lim` daemon. See `docs/RELEASE-lavalite-1.1.0.md`.
* 1.0.0: the initial stable foundation, batch, multi-host and GPU
  scheduling, durable event logging and replay, restart and recovery.

The `lavalite-2.0.0`, `lavalite-1.1.0` and `lavalite-1.0.0` tags
identify the exact source tree of each release.

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
```

# License

LavaLite is licensed under the GNU General Public License v2 (GPLv2).
All contributions must be compatible with GPLv2.

# Contact

Issues and feature requests:

* [https://codeberg.org/lavalitecompute/lavalite/issues](https://codeberg.org/lavalitecompute/lavalite/issues)

Maintainer:

* [lavalite.compute@gmail.com](mailto:lavalite.compute@gmail.com)
