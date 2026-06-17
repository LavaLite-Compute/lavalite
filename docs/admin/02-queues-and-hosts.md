# LavaLite Administrator Guide — Queues and Hosts

## Overview

Queues and hosts can be administered while the scheduler is running.

Typical administrative tasks include:

- Opening a queue
- Closing a queue
- Opening a host
- Closing a host
- Inspecting host groups
- Inspecting token pools
- Monitoring resource availability

These operations do not require modifying configuration files.

## Queue Administration

### Display Queues

Display queue status and counters:

```sh
bqueues
```

Typical information includes:

- Queue name
- Queue status
- Queue priority
- Host group
- Job counters

### Open a Queue

Open a queue:

```sh
bqueues --open queue_name
```

New jobs may be dispatched immediately if resources are available.

### Close a Queue

Close a queue:

```sh
bqueues --close queue_name
```

Closing a queue:

- Prevents new dispatches
- Does not affect running jobs
- Does not remove pending jobs

### Verify Queue State

```sh
bqueues
```

Verify that the queue state matches the requested administrative action.

## Host Administration

### Display Hosts

Display host status and resource usage:

```sh
bhosts
```

Typical information includes:

- Host name
- Host state
- CPU capacity
- Memory capacity
- Storage capacity
- GPU capacity
- Running jobs

### Open a Host

Open a host:

```sh
bhosts --open host_name
```

The scheduler may immediately dispatch pending jobs to the host.

### Close a Host

Close a host:

```sh
bhosts --close host_name
```

Closing a host:

- Prevents new dispatches
- Does not terminate running jobs
- Does not affect completed jobs

### Verify Host State

```sh
bhosts
```

Verify that the host state matches the requested administrative action.

## Host Groups

Host groups provide a logical collection of hosts.

Queues dispatch jobs to host groups rather than directly to individual
hosts.

Display host groups:

```sh
bgroups
```

Typical uses include:

- CPU hosts
- GPU hosts
- Large-memory hosts
- Development hosts
- Production hosts

### Verify Host Group Membership

```sh
bgroups
```

Confirm that hosts appear in the expected groups.

## Token Pools

Token pools represent shared resources whose availability controls job
dispatch.

Display token pool status:

```sh
btokens
```

Typical information includes:

- Pool name
- Total tokens
- Available tokens
- Allocated tokens

Jobs requesting unavailable tokens remain pending until sufficient
tokens become available.

### Verify Token Allocation

Submit a token-consuming job:

```sh
bsub --pool license=1 sleep 3600
```

Observe token usage:

```sh
btokens
```

Terminate the job:

```sh
bkill --signal kill <jobid>
```

Verify that the tokens are returned to the pool.

## Administrative Maintenance

A common maintenance workflow is:

1. Close a queue or host.
2. Allow running jobs to complete.
3. Perform maintenance.
4. Reopen the queue or host.
5. Verify dispatch resumes.

Example:

```sh
bhosts --close gpu01

# perform maintenance

bhosts --open gpu01
```

## Verification

Verify scheduler visibility:

```sh
bqueues
bhosts
bgroups
btokens
```

Verify:

- Queues are visible.
- Hosts are visible.
- Host groups are correct.
- Token pools are correct.
