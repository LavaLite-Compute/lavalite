# Disaggregated Computing in LavaLite
## A Natural Fit for a Modular Scheduler Architecture

This document describes how disaggregated computing maps cleanly onto
LavaLite’s architecture. Although the term was not part of the original
design vocabulary, the model aligns directly with the principles that
LavaLite is built on: modularity, composability, and explicit resource
management.

---

## 1. Overview

**Disaggregated computing** refers to breaking apart the traditional
monolithic “server” into independent resource pools:

- CPU providers
- Memory providers
- GPU/accelerator providers
- Storage providers
- Network or fabric endpoints

Each resource type becomes an independent, network‑addressable component
rather than a fixed bundle tied to a single physical host.

LavaLite’s architecture already supports this model without requiring
special subsystems or cloud‑specific logic.

---

## 2. Why Disaggregation Fits LavaLite

LavaLite treats hosts as **resource providers**, not indivisible
machines. The scheduler does not assume that CPU, memory, and GPUs must
come from the same physical node. Instead, it interacts with a unified
resource model:

- resources are versioned
- resources are updated explicitly
- resources are discovered through the LIM
- scheduling decisions are based on declared availability

This makes disaggregated computing a natural extension of the existing
design.

### Key architectural properties that enable this:

- **Decoupled resource model**
  Resources are first‑class objects, not attributes of a monolithic host.

- **Composable scheduling pipeline**
  LavaLite behaves like a Unix toolchain: each stage does one thing
  well, and capacity is added by adding providers.

- **Explicit resource commitment**
  Once a resource is allocated, it is committed. No load‑based or
  time‑based heuristics.

- **Bursting as “adding more providers”**
  Remote clusters, cloud nodes, or external pools are simply additional
  resource endpoints.

---

## 3. Bursting and Disaggregation

Bursting is often described as “sending jobs to the cloud.”
In LavaLite, bursting is simply: add more resource providers to
the pipeline.

This applies equally to:

- cloud VMs
- remote clusters
- GPU pools
- memory‑heavy nodes
- storage endpoints

The scheduler sees additional capacity; the origin does not matter.

This
Each stage is independent and composable.
LavaLite applies the same principle to compute resources.

---

## 4. Resource Providers as Logical Hosts

In a disaggregated environment, a “host” is a logical construct:

- a CPU pool may expose N cores
- a memory pool may expose M GB
- a GPU pool may expose K devices
- a storage pool may expose capacity or IOPS

LavaLite can represent each of these as:

```c
struct ll_resource { ... };
struct ll_host { ... };
```

 is the same philosophy as Unix pipelines:

 cat | grep | sed

## 5. Current vs Future Architecture

Today, LavaLite is a unified system:

Lava performs:

- scheduling
- resource evaluation
- dispatch
- host selection
- job launch
- state tracking

This is intentional: a clean, minimal, monolithic scheduler is easier to
build, reason about, and stabilize in early versions.

### Future Direction

The long‑term architecture naturally evolves into a pipeline:


Where:

- **scheduler**
  Pure placement logic. No host management, no dispatch, no lifecycle.

- **lava**
  Execution layer, resource plumbing, job launch, state propagation.

- **resource_providers**
  Hosts, pools, cloud nodes, remote clusters, or disaggregated resources.

### ASCII Architecture Diagram

```
[JOBS] ---> [SCHEDULER] ---> [LAVA] ---> [RESOURCE PROVIDERS]
```

This split is not required today, but the current design already makes
it possible:

- clean layer boundaries (`ll_*`, `chan_*`, `wire_*`)
- explicit resource model
- no cross‑layer contamination
- no hidden state
- no LSF‑style monolithic subsystems

LavaLite is being built in a way that allows modularity to emerge
naturally when needed, without forcing premature abstraction.
