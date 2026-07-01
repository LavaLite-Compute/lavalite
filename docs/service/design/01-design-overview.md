# LavaLite Services – Design Overview

## Purpose

This document describes the initial design direction for LavaLite Services.

It follows the user experience described in `docs/service/vision/01-user-experience.md`.

The goal is to explain how LavaLite can provide persistent user workspaces while keeping the system simple, transparent and consistent with the existing scheduler model.

This is not an implementation document.

It does not define protocol messages, C structures, state file formats or command-line syntax.

---

# Design Principle

A LavaLite Service is a persistent, schedulable workload.

It exists to provide an interactive workspace for a user.

The service is managed by LavaLite, consumes scheduled resources, and remains available until it is intentionally stopped or removed.

The key design principle is simple:

```text
Batch jobs are expected to finish.

Services are expected to remain available.
```

The difference is lifecycle, not resource type.

---

# Single Scheduler Authority

Services must remain under the same scheduler authority as batch jobs.

LavaLite should remain responsible for:

- deciding where services run
- allocating CPUs, memory, storage and GPUs
- enforcing user and administrator policy
- accounting for resource usage
- recovering state after restart
- reporting service status

Services should not introduce a second resource manager.

The scheduler owns the cluster resource model.

# Administrative Allocation

Services are administrator-provisioned workloads.

Unlike a batch job, a service is not normally created by the end user at the moment she wants compute resources.

Instead, the administrator defines which users may have persistent workspaces and what resources those workspaces may consume.

For example:

```text
alice/jupyter
    queue: interactive
    resources: 2 CPU, 8 GB, 1 GPU

bob/vscode
    queue: interactive
    resources: 2 CPU, 4 GB

sandro/jupyter
    queue: gpu-interactive
    resources: 4 CPU, 16 GB, 1 GPU

---

# Workload Types

LavaLite has two high-level workload types:

```text
Batch Job
    finite workload
    runs once
    completes
    releases resources

Service
    persistent workload
    remains available
    may be restarted
    releases resources only when stopped or removed
```

Both workload types use the same cluster resources.

Both are scheduled.

Both are accounted for.

Both participate in recovery.

They differ in lifecycle and user expectation.

The user still submit batch jobs from his/hers administrative allocation.

---

# User Workspace

A service represents a user workspace.

Examples include:

- JupyterLab
- VS Code Server
- RStudio Server
- visualization tools
- lightweight user web applications

The user connects to the workspace through a browser.

The user does not need to know which host or port was selected.

That information is internal scheduler state and may change over time.

The stable concept is the workspace identity, for example:

```text
alice/jupyter
bob/vscode
sandro/rstudio
```

---

# Ownership

A service has an owner.

The owner is normally the user for whom the workspace exists.

The owner is used for:

- visibility
- permissions
- accounting
- resource limits
- policy enforcement

Administrators may create, stop, inspect or remove services.

Ordinary users interact with the workspace, not with the service machinery.

---

# Resource Allocation

A service receives a resource allocation when it runs.

For example:

```text
alice/jupyter
    2 CPU
    8 GB memory
    1 GPU
```

Those resources remain assigned while the service is running.

Large computations are submitted separately as batch jobs.

For example, Alice may use her Jupyter workspace to submit:

```text
bsub --cpus 32 --mem 256G --gpus 4 python train.py
```

The batch job receives its own allocation.

The service and the batch job are separate scheduled workloads.

This keeps the interactive workspace responsive while large computations remain under normal batch scheduling.

---

# Placement and Access

The scheduler places a service on an execution host.

The service listens on a network port.

Internally, LavaLite must know:

- service owner
- service name
- execution host
- port
- current state
- requested resources

The user should see a stable access point.

For example:

```text
https://cluster.example.org/services/alice/jupyter
```

The first implementation does not need to solve the final portal or reverse-proxy design.

The minimum design requirement is that LavaLite records where the service is reachable and can report that information clearly.

---

# Lifecycle

A service lifecycle should be explicit.

A simple initial lifecycle is:

```text
PENDING
RUNNING
STOPPED
FAILED
DELETED
```

## PENDING

The service exists but is waiting for resources or placement.

## RUNNING

The service has an allocation and an active process.

## STOPPED

The service was intentionally stopped.

## FAILED

The service exited unexpectedly or could not be started.

## DELETED

The service definition was removed.

A stopped service preserves user intent.

A deleted service must not reappear during recovery.

---

# Restart Policy

Services may have a restart policy.

The restart policy describes what LavaLite should do when the service process exits unexpectedly.

A simple initial model is:

| Policy | Meaning |
|--------|---------|
| never | Do not restart automatically |
| on-failure | Restart only after unexpected failure |
| always | Keep running until intentionally stopped or deleted |

Restart policy belongs to the service model.

It should not be hidden in user scripts.

---

# Recovery

Services must participate in durable recovery.

After a scheduler restart, LavaLite reconstructs service definitions and their last known state from durable state.

The goal is to restore the logical cluster state that existed before the restart.

Recovery must preserve user intent.

For example:

| State before restart | Expected recovery behavior |
|----------------------|----------------------------|
| PENDING | Remain pending |
| RUNNING | Reconnect if still alive, otherwise apply restart policy |
| STOPPED | Remain stopped |
| FAILED | Remain failed or restart according to policy |
| DELETED | Remain deleted |

Recovery is not blind process recreation.

It is reconciliation between durable scheduler state and the actual execution state of the cluster.

---

# Administration Boundary

The user experience should remain simple.

Alice should connect to her workspace and work.

She should not need to know how the workspace is created, placed or restarted.

Administration is a separate concern.

Administrators decide:

- which users receive services
- which service types are available
- which resources each service may use
- which hosts may run services
- how access URLs are exposed
- when services are stopped or removed

This separation keeps the user experience simple while preserving administrator control.

---

# Relationship to Batch Jobs

Services do not replace batch jobs.

They complement them.

The workspace is used to prepare, submit, monitor and analyze batch jobs.

The batch system remains responsible for large computations.

This separation is important:

```text
Interactive workspace:
    small, persistent, responsive

Batch job:
    large, finite, scheduled computation
```

The service provides continuity.

The batch job provides compute scale.

---

# Relationship to Existing LavaLite Design

Services should reuse existing LavaLite concepts where possible.

Relevant existing concepts include:

- users
- administrators
- queues
- host placement
- resource allocation
- durable state
- execution hosts
- accounting
- history
- recovery

The design should add a new workload type without replacing the existing scheduler architecture.

The goal is not to build a separate system beside LavaLite.

The goal is to extend LavaLite so persistent workspaces and finite jobs can coexist under one resource model.

---

# Initial Scope

The initial service design should stay small.

The first useful target is:

```text
Keep one user-owned service running somewhere in the cluster,
with scheduled resources,
and report where the user can reach it.
```

The first implementation should not require:

- a full web portal
- automatic scaling
- multiple replicas
- complex service discovery
- service composition
- resize support
- advanced idle detection

Those topics can be considered later.

The first design must prove the core model.

---

# Current Conclusion

A LavaLite Service is a persistent workload managed by the scheduler.

It provides a user workspace.

It consumes scheduled resources.

It participates in recovery.

It complements batch jobs rather than replacing them.

The design goal is to provide a simple user experience while keeping the scheduler as the single authority for cluster resources.
