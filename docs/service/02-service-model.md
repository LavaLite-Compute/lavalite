# LavaLite Services – Service Model

## Purpose

This document defines the initial conceptual model for **services** in LavaLite.

It builds on the use-case discussion in `docs/service/01-use-case.md` and goes one level deeper into the scheduler architecture.

The goal is not to define the final implementation, protocol, or command-line interface. The goal is to describe what a service **means** inside LavaLite.

---

# Definition

A LavaLite Service is a long-running workload managed by the scheduler.

Unlike a batch job, a service is not expected to finish by itself.

A batch job has a finite lifecycle:

```text
submit → pending → running → done/exit
```

A service has a persistent lifecycle:

```text
create → pending → running → stopped/failed/deleted
```

The key difference is not the resources it requests.

The key difference is the lifecycle.

A service may request the same schedulable resources as a batch job:

- CPUs
- memory
- storage
- GPUs
- GPU model
- host placement
- future resource types such as licenses or tokens

The scheduler remains responsible for deciding where the service runs.

---

# Service vs Batch Job

A batch job represents work that should complete.

A service represents something that should remain available.

For example:

| Workload | Expected Behavior |
|----------|-------------------|
| Batch training job | Run once, finish, release resources |
| MPI simulation | Run once, finish, release resources |
| Jupyter service | Keep running until stopped |
| VS Code service | Keep running until stopped |
| Model endpoint | Keep running until stopped or failed |

A successful batch job exits.

A successful service continues running.

This distinction affects scheduling, monitoring, restart behavior, accounting, and user expectations.

---

# First-Class Workload

A service should be treated as a first-class workload, not as an external system bolted onto the scheduler.

This means LavaLite should know:

- who owns the service
- what resources it requested
- where it is running
- what command started it
- whether it is reachable
- whether it has stopped, failed, or been deleted
- how it should be accounted for

The service may internally reuse job execution machinery, but conceptually it is not just a normal batch job with a long sleep time.

---

# Resource Model

A service receives a scheduler allocation.

For example:

```text
Service: alice/jupyter

Resources:
    CPU: 2
    MEM: 8G
    GPU: 1
```

Those resources remain assigned while the service is running.

If Alice submits a large training job from inside Jupyter, that training job receives a separate allocation:

```bash
bsub --cpus 32 --mem 256G --gpus 4 python train.py
```

The service and the batch job are independently scheduled and accounted for.

This separation allows the interactive service to remain responsive while larger computations are handled as ordinary LavaLite batch jobs.

---

# Ownership

A service has an owner.

The owner is normally the user who created it.

For example:

```text
alice/jupyter
bob/vscode
sandro/jupyter
```

The owner controls the service unless an administrator intervenes.

The scheduler should enforce the same general visibility and permission rules used elsewhere in LavaLite:

- users can see and manage their own services
- administrators can see and manage all services
- service resource usage is charged to the owner

---

# Naming

A service should have a stable name.

A possible naming model is:

```text
<user>/<service-name>
```

Examples:

```text
alice/jupyter
bob/vscode
sandro/jupyter
```

This makes service identity independent of host placement.

If Alice's Jupyter service is restarted on another node, the logical service name remains the same.

---

# Placement

A service is placed by the scheduler.

The user should not need to know in advance which host will run the service.

For example:

```text
alice/jupyter → node07:49152
```

The scheduler records the selected host and port.

A separate access layer may expose that service through a stable URL:

```text
https://cluster.example.org/services/alice/jupyter
```

The URL is stable from the user's perspective, even if the internal host and port change after a restart.

---

# Network Exposure

A service differs from a batch job because users must be able to connect to it.

Therefore, a service needs some form of network exposure.

At the conceptual level, LavaLite needs to know:

- the service name
- the owner
- the host where it runs
- the port where it listens
- the public URL or access path

The first implementation does not need to solve all proxying problems.

It only needs a clear model for recording and reporting where the service is reachable.

Possible approaches include:

- LavaLite records `host:port` and prints it to the user
- LavaLite integrates with an existing reverse proxy
- LavaLite later provides a small service proxy component

This document does not choose between these options.

---

# Lifecycle

A service lifecycle should be explicit.

A possible initial lifecycle is:

```text
CREATED
PENDING
RUNNING
STOPPED
FAILED
DELETED
```

## CREATED

The service definition exists, but it has not yet been scheduled.

## PENDING

The service is waiting for resources.

## RUNNING

The service has an active allocation and a running process.

## STOPPED

The service was intentionally stopped by the owner or an administrator.

## FAILED

The service exited unexpectedly or could not be started.

## DELETED

The service definition has been removed.

---

# Recovery

Services participate in LavaLite's durable recovery model in the same way as batch jobs.

Following a scheduler restart, the scheduler reconstructs the service definitions and their last known state from durable scheduler data.

The objective is to restore the logical cluster state that existed before the restart rather than creating a new one.

For each service, LavaLite determines whether the service process is still running on its execution host.

If the service is still active, it returns to the **RUNNING** state.

If the service is no longer active, LavaLite applies the configured restart policy.

For example:

- **never** – the service remains stopped or failed.
- **on-failure** – the service is restarted only if it terminated unexpectedly.
- **always** – the service is scheduled again until explicitly stopped or deleted.

Recovery must preserve user intent.

A service intentionally stopped by its owner must remain stopped after recovery.

Likewise, a deleted service must not reappear simply because it existed previously.

The goal of recovery is to restore the scheduler's previous state.

# Restart Semantics

Restart behavior is one of the main differences between services and batch jobs.

A batch job normally exits once.

A service may need to be restarted.

Possible restart policies include:

| Policy | Meaning |
|--------|---------|
| never | Do not restart automatically |
| on-failure | Restart only if the service exits unexpectedly |
| always | Keep the service running until explicitly stopped |

The initial LavaLite implementation could start with a simple policy, such as `on-failure` or `always`, and evolve later.

The important design point is that restart behavior belongs to the service model.

It should not be hidden inside user scripts.

---

# Failure Semantics

A failed service should be visible.

For example:

```text
SERVICE          USER    STAT     HOST     RESOURCES
alice/jupyter    alice   FAILED   node07   2CPU 8G 1GPU
```

Failure should not silently disappear.

Users and administrators should be able to inspect why the service failed, just as they inspect job history today.

---

# Accounting

Services consume resources while they are running.

Therefore they should be accounted for.

At minimum, accounting should include:

- owner
- service name
- start time
- stop time
- host
- requested CPUs
- requested memory
- requested GPUs
- exit or failure reason

A service that runs for three days should be visible as a three-day resource consumer.

This is important because persistent services can otherwise become forgotten allocations.

---

# Queues and Policy

Services may need policy controls.

Possible policies include:

- which users can create services
- which hosts may run services
- maximum CPUs per service
- maximum memory per service
- maximum GPUs per service
- maximum number of services per user
- idle timeout
- restart limits

These policies could be implemented through service-specific queues or through extensions to existing queue configuration.

This document does not choose the configuration format.

---

# Relationship to Existing LavaLite Components

Services should reuse LavaLite concepts where possible.

Relevant existing concepts include:

- users and managers
- queues
- host placement
- resource allocation
- durable scheduler state
- execution daemons
- command-line clients
- accounting and history

The objective is not to introduce a second scheduler.

The objective is to extend LavaLite's workload model so that services and batch jobs can coexist under the same authority.

---

# Goals

The initial goal of LavaLite Services is to extend the scheduler with a new workload type that supports persistent interactive applications.

A service should:

- remain under scheduler control for its entire lifetime
- consume scheduled resources just like any other workload
- survive scheduler restarts through durable recovery
- provide a stable interactive environment for the user
- coexist naturally with traditional batch jobs

The objective is to support modern interactive HPC workflows while preserving LavaLite's simple scheduler architecture.

---

# Initial Service Examples

## JupyterLab

```text
Service: alice/jupyter
Command: jupyter-lab --no-browser --port <assigned-port>
Resources: 2 CPU, 8G, 1 GPU
Restart: on-failure
Expose: web
```

## VS Code Server

```text
Service: bob/vscode
Command: code-server --bind-addr 0.0.0.0:<assigned-port>
Resources: 2 CPU, 4G
Restart: on-failure
Expose: web
```

## Model Endpoint

```text
Service: alice/model-api
Command: python serve.py --port <assigned-port>
Resources: 4 CPU, 16G, 1 GPU
Restart: always
Expose: web
```

---

# Open Questions

The following questions remain open:

- Is a service implemented as a new workload type or as a special job type?
- Should services have their own persistent state files?
- Should service events live in the same event manifest as jobs?
- Should service history be shown by `bhist`, a new command, or both?
- Should services use existing queues or service-specific queues?
- How should service ports be allocated?
- Should LavaLite provide a reverse proxy or only publish host and port?
- What is the minimum safe authentication model?
- Should services support idle timeout?
- Should services support resize, or should users stop and recreate them?
- What should happen to services during host maintenance?

---
# Current Conclusion

A LavaLite Service is a persistent, schedulable workload.

It uses the same scheduler authority as batch jobs, but it has a different lifecycle and user expectation.

Batch jobs are expected to finish.

Services are expected to remain available.

Together, batch jobs and services extend the scheduler to support both finite and persistent workloads using a common scheduling model.
