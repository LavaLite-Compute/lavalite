## What Is LavaLite?

LavaLite is a lightweight, high-throughput job scheduler designed for clusters that
prioritize volume over parallelism. It does not focus on tightly coupled MPI
workloads, LavaLite excels at managing thousands of short, independent jobs with
minimal overhead.

LavaLite isn’t just about HPC—it’s also a natural fit for HTC workloads: massive job arrays,
embarrassingly parallel tasks, and data-intensive pipelines. LavaLite isn’t just for
supercomputers—it’s for bioinformatics labs, media farms, and cloud-scale batch systems too.

LavaLite is also a powerful fit for AI workflows—whether you're training models across
distributed GPUs, orchestrating hyperparameter sweeps, or managing inference pipelines
at scale. Its lightweight design and scriptable interface make it ideal for researchers
and engineers pushing the boundaries of machine learning.

Inspired by Platform Lava 1.0 and refined with algorithmic insights from Donald Knuth’s
teachings, LavaLite is built for speed, simplicity, and clarity. It’s not just a
revival—it’s a reinvention.

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

