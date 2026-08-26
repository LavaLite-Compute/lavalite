# LavaLite Services: Bridging HPC Scheduling and Long-Running Services

## Executive summary

LavaLite Services extends the traditional HPC batch model to long-running interactive and network services without introducing a separate orchestration stack.

The core idea is deliberately simple: **a service is still a LavaLite job**. The existing scheduler remains responsible for placement, resource allocation, lifecycle, accounting, and execution. A small service layer adds the missing concept of a stable network endpoint.

This allows services such as Jupyter, code-server, model/API servers, development environments, and other containerized applications to run on HPC resources while preserving the deterministic operational model of an HPC scheduler.

The goal is not to reproduce Kubernetes. It is to provide a lightweight HPC-native path from:

> “submit a computation”

to:

> “give this user a persistent service backed by scheduled cluster resources.”

---

## Why this matters

Modern HPC environments increasingly need to support workloads that do not naturally look like traditional batch jobs:

- Jupyter and interactive development environments
- AI inference and model-serving endpoints
- web-based scientific applications
- visualization services
- development tools such as code-server
- internal APIs and long-running research services
- GPU-backed interactive environments

These workloads still need the things an HPC scheduler is good at:

- resource allocation
- GPU selection
- CPU and memory limits
- user identity
- queue and policy enforcement
- execution-host selection
- lifecycle management
- accounting

The missing piece is normally **service discovery and stable network access**.

LavaLite Services adds that piece while continuing to use the existing scheduler and execution infrastructure.

---

## High-level architecture

```text
                         LavaLite cluster
        +-----------------------------------------------+
        |                                               |
        |              +----------------+               |
        |              |      MBD       |               |
        |              | scheduler +    |               |
        |              | service mgr    |               |
        |              +-------+--------+               |
        |                      |                        |
        |             control  | channels               |
        |                      |                        |
        |       +--------------+---------------+        |
        |       |                              |        |
        |       v                              v        |
        | +-----------+                  +-----------+  |
        | |    SPD    |                  |    SBD    |  |
        | | service   |                  | execution |  |
        | | proxy     |                  | daemon    |  |
        | +-----+-----+                  +-----+-----+  |
        |       |                              |        |
        |       |                              v        |
        |       |                       +-------------+ |
        |       |                       | Service Job | |
        |       |                       | Jupyter /   | |
        |       |                       | API / etc.  | |
        |       |                       +------+------+ |
        |       |                              ^        |
        |       +---------- TCP relay ----------+        |
        |                                               |
        +-----------------------------------------------+
                ^
                |
          stable endpoint
                |
             user/client
```

The important architectural property is that the new service components reuse LavaLite's existing control-plane concepts.

`spd`, the service proxy daemon, behaves like another managed LavaLite daemon from a networking and channel-management perspective. The service-specific functionality is concentrated in endpoint allocation and TCP proxying rather than introducing a second control plane.

---

## Service definition

Services are defined administratively.

For example:

```text
Begin Service
SERVICE_NAME = echotest
TYPE = user
IMAGE = /export/service_images/python-slim.sif
COMMAND = python3 /home/david/lavalite-test/echo_server.py 8888
PORT = 8888
End Service
```

The general form is:

```text
Begin Service
SERVICE_NAME = <name>
TYPE         = user | daemon
IMAGE        = <sif path>
COMMAND      = <cmd>
PORT         = <port>
QUEUE        = <queue>
RESTART      = always | on-failure | never
End Service
```

The administrator therefore controls the service definition, image, command, backend port, scheduling policy, and restart behavior.

For containerized services, images are prepared as SIF files and placed in a known cluster location. This follows the traditional HPC model in which applications are installed, validated, and made available by the site rather than dynamically pulling arbitrary software during execution.

---

## User experience

The intended user operation is deliberately small:

```text
$ bservice jupyter
http://cluster.example:3001
```

The user asks for a named service.

LavaLite handles:

1. creation of a service instance;
2. allocation of a stable frontend port;
3. creation of the underlying LavaLite job;
4. scheduling onto an execution host;
5. service startup;
6. connection of the stable frontend endpoint to the actual backend;
7. return of the usable URL.

The user does not need to know which compute node ultimately runs the service.

---

## Service creation flow

The creation is intentionally split into two phases.

```text
User
 |
 | bservice jupyter
 v
+------------------+
|       MBD        |
| service manager  |
+--------+---------+
         |
         | ADD service endpoint
         v
+------------------+
|       SPD        |
| allocate + bind  |
| frontend port    |
+--------+---------+
         |
         | port = 3001
         v
+------------------+
|       MBD        |
| synthesize job   |
| and submit       |
+--------+---------+
         |
         | ordinary LavaLite job
         v
+------------------+
|    Scheduler     |
| choose resources |
| + execution host |
+--------+---------+
         |
         v
+------------------+
|       SBD        |
| launch service   |
| job              |
+--------+---------+
         |
         | NEW_JOB_REPLY
         | execution_host
         v
+------------------+
|       MBD        |
| UPDATE proxy     |
+--------+---------+
         |
         | frontend 3001
         | -> execution_host:8888
         v
+------------------+
|       SPD        |
| endpoint ready   |
+--------+---------+
         |
         v
http://cluster.example:3001
```

### Why allocate the frontend first?

The public endpoint is reserved before the service job is launched.

This prevents a situation in which the scheduler successfully starts a service but the system subsequently discovers that it cannot expose it.

Once the job is created, the frontend endpoint already exists and is owned by the service instance.

---

## Stable frontend, dynamic backend

This is the central service abstraction.

```text
                    stable
                      |
                      v
              +----------------+
User -------->| proxy-host:3001 |
              +-------+--------+
                      |
                      | TCP relay
                      |
                      v
              +----------------+
              | r9c3:8888      |
              | service job    |
              +----------------+
                    dynamic
```

The two sides are intentionally independent:

```text
Frontend:
    proxy_host:allocated_port

Backend:
    execution_host:service_port
```

For example:

```text
ubuntu:3001 -> r9c3:8888
```

where:

- `3001` is allocated and bound by `spd`;
- `r9c3` is selected by the LavaLite scheduler;
- `8888` is defined by the service configuration.

The scheduler is free to choose a different execution host on another incarnation of the service.

The user-facing endpoint can remain stable.

---

## A service remains an HPC job

A key design decision is not to create an independent execution model for services.

The service manager synthesizes an ordinary LavaLite job. The normal scheduler then decides where that job runs.

Conceptually:

```text
                   +----------------------+
                   |   LavaLite Scheduler |
                   +----------+-----------+
                              |
              +---------------+---------------+
              |                               |
              v                               v
       Traditional job                  Service job

       simulation                       Jupyter
       MPI workload                     code-server
       batch analysis                   model server
       GPU computation                  scientific API
              |                               |
              +---------------+---------------+
                              |
                     same resource model
```

This means services can naturally inherit existing and future LavaLite capabilities such as:

- CPU allocation
- memory allocation and enforcement
- GPU scheduling
- host selection
- queues
- user policy
- accounting
- lifecycle tracking

The service layer does not need to duplicate the scheduler.

---

## Containers are an execution mechanism, not the scheduler

Apptainer provides a convenient packaging and runtime mechanism for services, particularly in HPC environments.

For example, the service job may ultimately execute something equivalent to:

```text
apptainer exec <options> /export/service_images/jupyter.sif <command>
```

But container placement remains a LavaLite decision.

```text
LavaLite
   |
   | selects host + resources
   v
SBD
   |
   | starts process
   v
Apptainer
   |
   v
service application
```

This keeps responsibility clear:

**LavaLite schedules. Apptainer packages and executes.**

---

## Event-driven lifecycle rather than continuous reconciliation

Another important difference from cloud-native orchestration is that LavaLite Services does not currently require a continuous desired-state reconciliation loop.

LavaLite already has an asynchronous, protocol-driven job lifecycle. Service state advances in response to concrete events:

```text
bservice request
      |
      v
ADD endpoint
      |
      v
job submission
      |
      v
scheduler dispatch
      |
      v
NEW_JOB_REPLY
      |
      v
UPDATE proxy backend
      |
      v
service running
      |
      +---- FINISH / failure / termination
                         |
                         v
                  REMOVE or restart
```

Rather than repeatedly comparing:

```text
desired state <-> observed state
```

the normal operating model is:

```text
state machine + asynchronous events
```

MBD already receives the events that describe important job transitions, and the service layer can react directly to those transitions.

This avoids introducing a periodic reconciliation mechanism simply because services are being added.

### Recovery and durability

Event-driven operation does not remove the need for recovery.

Daemon restart, host failure, lost communication, or an interrupted control-plane transition can leave persistent state that must be reconstructed and validated. Recovery therefore remains a major part of the service design, but it is conceptually different from continuous reconciliation.

The intended distinction is:

```text
Normal operation:
    asynchronous protocol events drive state transitions

Exceptional operation:
    durable state is used to recover and reconstruct service state
```

LavaLite already has a durable event model for batch jobs. Service recovery should build on the same philosophy rather than introducing an unrelated persistence mechanism.

Important recovery questions still to be designed and tested include:

- reconstructing service instances after MBD restart;
- reconstructing SPD frontend-port ownership and backend mappings;
- handling an SPD restart while service jobs remain alive;
- detecting service jobs that disappeared because an execution host failed;
- reconnecting a stable frontend endpoint to a restarted service incarnation;
- applying `RESTART = always | on-failure | never` consistently;
- deciding when a failed or partially-created service instance can be safely removed.

### Service history

Service lifecycle events should also become visible through LavaLite's historical view.

`bhist` already provides the durable timeline of a job. Because a service incarnation is represented by a real LavaLite job, its scheduling and execution history naturally remains available there.

Over time, service-specific events can complement that job history, for example:

```text
SERVICE_CREATED
ENDPOINT_ALLOCATED
JOB_SUBMITTED
BACKEND_REGISTERED
SERVICE_RUNNING
BACKEND_LOST
SERVICE_RESTARTED
SERVICE_REMOVED
```

This is valuable operationally: a service should not become an opaque object merely because it is long-running. Administrators should be able to answer the same questions they ask about batch jobs — where it ran, when it started, why it stopped, whether it restarted, and what happened to its endpoint.

The combination of **asynchronous events, durable history, and explicit recovery** is intended to preserve the deterministic character of LavaLite while extending it to services.

---

## Relationship to cloud-native orchestration

The objective is not to build a small Kubernetes clone.

Cloud-native orchestration solves a broad set of problems around distributed application deployment, networking, service discovery, storage, controllers, and application reconciliation.

LavaLite already has a different and valuable foundation: an HPC scheduler with explicit resources, users, jobs, queues, execution hosts, and deterministic placement.

The service work asks a narrower question:

> What is the minimum additional machinery required to make scheduled HPC resources usable as persistent services?

The current answer is:

```text
HPC scheduler
      +
service lifecycle
      +
stable endpoint / proxy
      =
HPC-native services
```

This provides a useful middle ground for organizations that need service-oriented workflows on HPC infrastructure without necessarily introducing a complete container-orchestration platform for every workload.

---

## Example: GPU-backed Jupyter

A future service definition could describe a GPU-backed Jupyter environment.

```text
             User
              |
              | bservice jupyter-gpu
              v
        +-----------+
        | LavaLite  |
        | scheduler |
        +-----+-----+
              |
              | allocate
              | CPU + RAM + GPU
              v
        +-------------+
        | GPU node    |
        |             |
        | Apptainer   |
        | Jupyter     |
        | :8888       |
        +------+------+
               ^
               |
          TCP proxy
               |
        +------+------+
        | SPD :3007   |
        +------+------+
               ^
               |
        browser / API
```

From the scheduler's perspective, this is still a resource-controlled job.

From the user's perspective, it is a URL.

That is the bridge the service architecture is intended to provide.

---

## Current implementation direction

The current development branch already establishes the principal building blocks:

- service configuration and service instances;
- `bservice` command;
- synthetic service-job creation;
- service proxy daemon (`spd`);
- frontend port allocation;
- `ADD`, `UPDATE`, and `REMOVE` proxy control operations;
- reuse of LavaLite channel management between MBD and SPD;
- TCP accept/connect/relay implementation;
- backend update when the scheduled job starts;
- service information collection and `bservice -l`.

The immediate engineering focus is validation of the proxy path and complete end-to-end service lifecycle.

---

## Strategic value

The interesting aspect of this work is not simply adding Jupyter to an HPC cluster.

It creates a general mechanism for treating **services as scheduled HPC workloads**.

That opens several directions:

```text
Traditional HPC                 Emerging service workloads
---------------                 --------------------------
batch simulation                Jupyter
MPI                             interactive development
GPU training                    inference endpoint
scientific executable           scientific web application
scheduled analysis              internal API
```

Rather than forcing these into two completely independent infrastructures, LavaLite can provide a common resource and lifecycle model underneath both.

This gives us room to develop our own practical solutions and operational insight around an area that is increasingly important in GPU- and AI-oriented HPC environments.

The design remains intentionally small: reuse the scheduler, reuse the execution model, reuse the control-plane infrastructure, and add only the service-specific mechanisms that are actually required.
