# LavaLite Services -- Initial Use Case Discussion

## Purpose

This document captures an initial design discussion about **services** in LavaLite. It is intentionally focused on the **user workflow**, not on the implementation.

The objective is to explore a simpler scheduling model in which both traditional batch jobs and long-running interactive services are managed by the same scheduler. Instead of introducing a separate resource manager for interactive workloads, services become a first-class workload alongside batch jobs.

------------------------------------------------------------------------

# Motivation

Modern HPC users no longer work exclusively through SSH terminals.

Many users now expect browser-based development environments such as:

-   JupyterLab
-   VS Code Server
-   RStudio Server
-   visualization tools
-   lightweight dashboards

These applications do not replace the scheduler.

Instead, they provide a persistent interactive workspace from which
users develop, debug, visualize results, and submit batch jobs.

------------------------------------------------------------------------

# Example Scenario

Three researchers share the cluster:

-   Alice
-   Bob
-   Sandro

Each starts a personal service.

```
  User     Service          Resources
  -------- ---------------- ---------------------
  Alice    JupyterLab       2 CPU, 8 GB, 1 GPU
  Bob      VS Code Server   2 CPU, 4 GB
  Sandro   JupyterLab       4 CPU, 16 GB, 1 GPU
```

These services provide persistent interactive workspaces rather than
large-scale computation. They typically require modest resources
compared to production batch jobs.

A Jupyter service may be started with or without GPU resources. A GPU
allows interactive CUDA development, visualization and small-scale
experiments, while larger computations are normally submitted as
independent batch jobs.

------------------------------------------------------------------------

# Typical Workflow

Alice opens her browser and connects to:

``` text
https://cluster.example.org/services/alice/jupyter
```

The browser connects to Alice's running Jupyter service.

From Alice's perspective, Jupyter becomes her primary development
environment. She can:

-   edit code
-   inspect datasets
-   visualize results
-   debug programs
-   open an interactive shell
-   submit LavaLite batch jobs

When computation becomes expensive, she submits a normal batch job.

``` bash
bsub --gpus 4 --cpus 32 --mem 256G python train.py
```

The training runs as an ordinary LavaLite batch job while the Jupyter
service continues to provide the interactive environment.

------------------------------------------------------------------------

# Interactive Development Loop

``` text
Browser
    │
    ▼
Personal Jupyter Service
    │
    ▼
Edit and test code
    │
    ▼
Submit batch job
    │
    ▼
Training executes
    │
    ▼
Inspect results
    │
    ▼
Modify code
    │
    ▼
Submit next batch job
```

The service supports interactive development.

The batch job performs the heavy computation.

------------------------------------------------------------------------

# Resource Allocation

A service allocated a fixed set of resources when it is started.

For example:

``` text
Alice's Jupyter

2 CPU
8 GB
1 GPU
```

These resources remain assigned to the service while it is running.

When additional resources are required, the user submits a separate
batch job.

``` bash
bsub --cpus 32 --mem 256G --gpus 4 python train.py
```

The scheduler allocates a new set of resources for the batch job while
the interactive service continues running independently.

This separation keeps the interactive environment responsive while
allowing large computations to be scheduled, accounted for, and managed
as ordinary LavaLite jobs.

------------------------------------------------------------------------

# Why This Is Interesting

Both interactive services and traditional batch jobs are managed by the
same scheduler.

LavaLite remains responsible for:

-   placement
-   CPU allocation
-   memory allocation
-   GPU allocation
-   accounting
-   priorities
-   queues

Services do not introduce a second resource manager. They use the same
scheduling infrastructure already used for batch jobs while providing a
different workload lifecycle.

------------------------------------------------------------------------

# Open Questions

The following questions remain open:

-   What exactly defines a LavaLite Service?
-   What lifecycle should a service have?
-   Should services have dedicated queues?
-   Should services count toward fair-share?
-   Can services be suspended and resumed?
-   Should services automatically restart after node failures?
-   How are services exposed through URLs?
-   Should LavaLite include a reverse proxy or integrate with an
    existing one?
-   How are users authenticated?
-   Should users be allowed multiple concurrent services?

These questions can be addressed after the workflow itself has been
validated.

------------------------------------------------------------------------

# Current Conclusion

A LavaLite Service is not intended to replace batch scheduling.

Instead, it provides a persistent interactive workspace from which users
prepare, launch, monitor and analyze ordinary LavaLite batch jobs.

The scheduler remains the single authority responsible for resource
allocation across the cluster.

Rather than integrating separate systems to manage interactive services and batch jobs, LavaLite explores a different approach: treating both as native workloads of the same scheduler. By extending the scheduler instead of introducing a second resource manager, the architecture remains simple, consistent and transparent while supporting modern HPC workflows.
