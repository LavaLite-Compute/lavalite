## What Is LavaLite?

LavaLite is a modern, independent continuation of the original open-source
Platform Lava / OpenLava scheduler. It is a from-scratch modernization,
cleanup, and redesign effort built on that heritage—yet completely independent
of Platform Computing, IBM, or any other vendor.

LavaLite is a lightweight, high-throughput job scheduler designed for
clusters that prioritize volume over parallelism. It does not target tightly
coupled MPI workloads. Instead, it excels at running thousands of short,
independent jobs with minimal overhead.

Beyond traditional HPC, LavaLite is a natural fit for HTC workloads:
massive job arrays, embarrassingly parallel tasks, data-driven pipelines, and
large-scale scientific workflows. It’s useful not only on supercomputers but
also in bioinformatics labs, media render farms, and cloud batch systems.

LavaLite is also a strong match for AI and machine-learning pipelines—from
distributed GPU training to hyperparameter sweeps and inference farms. Its
simple interface and lightweight design make it ideal for researchers and
engineers pushing the limits of modern computational workloads.

Inspired by Platform Lava 1.0 and refined with algorithmic principles drawn
from Donald Knuth’s teachings, LavaLite is built for speed, simplicity, and
clarity. It’s not just a revival—it’s a reinvention.

## Origins & Philosophy
LavaLite is built on a clean, GPLv2-licensed foundation originally released by Platform Computing.
It maintains a clear lineage and avoids legacy entanglements, focusing instead on clarity,
performance, and modern design.

Our philosophy draws from the algorithmic elegance championed by **Donald Knuth**, especially the
principles laid out in *The Art of Computer Programming*. LavaLite is optimized for
**high-throughput computing (HTC)**— is engineered for speed,
simplicity, and transparency. LavaLite still support MPI workloads by generating dymic hostfile if needed.

LavaLite isn’t a clone. It’s a clean slate.

We aim for speed and simplicity, built on the algorithmic elegance that inspired our design.

## Getting started

LavaLite currently supports **Rocky Linux 8**, **Rocky Linux  9** and **Ubuntu 24.02**.
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

## Contact

For issues and feature requests, please use the
[GitHub Issues page](https://github.com/lavalite/livelite/issues).

Maintainer: lavalite.compute@gmail.com
Project email: livelite@lavalite-compute.io
