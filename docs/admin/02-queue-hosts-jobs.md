# LavaLite Administrator Guide — Queues, Hosts, and Jobs

## Overview

Queues, hosts, and jobs can be administered while the scheduler is
running.

Typical administrative tasks include:

- Opening a queue
- Closing a queue
- Opening a host
- Closing a host
- Inspecting host groups
- Inspecting token pools
- Monitoring resource availability
- Inspecting and signaling jobs, including array jobs

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

## Job Administration

### Display Jobs

Display jobs and their state:

```sh
bjobs --all
```

Typical information includes:

- Job ID
- State
- Queue
- Execution host(s)
- Submit time

### Signal a Job

Terminate a job:

```sh
bkill --signal kill <jobid>
```

Suspend and resume a running job:

```sh
bkill --signal stop <jobid>
bkill --signal cont <jobid>
```

`bkill 0` signals every job the calling user owns. A manager may
signal any user's jobs.

### Move a Pending Job

Move a pending job to a different queue:

```sh
bmove <jobid> <queue_name>
```

### Change Job Priority

```sh
bpriority <jobid> <priority>
```

A non-manager cannot raise a job's priority above the queue's
priority.

## Job Arrays

A job array submits many jobs from a single specification. Each
element runs as an ordinary job — same queue policy, same dispatch,
same resource accounting — there is no separate array entity to
schedule.

### Submit an Array Job

```sh
bsub --array START-END[:STRIDE] command
```

Example, ten elements:

```sh
bsub --array 1-10 myprogram
```

Example, every other element from 1 to 20:

```sh
bsub --array 1-20:2 myprogram
```

`START` must be 1 or greater; arrays are numbered 1..N. `STRIDE`
defaults to 1.

Each array element is assigned its own job ID at submission. The
first element's job ID is also the array's ID, used to refer to the
array as a whole.

### Display Array Jobs

```sh
bjobs --all
```

Array elements are listed like any other job. `bhist` reports each
element individually as well.

### Signal an Array Job

Signal every element of an array:

```sh
bkill <array_id>
```

This is equivalent to `bkill 0` restricted to that array's elements.

Signal a single element:

```sh
bkill <array_id>[<index>]
```

Example, terminate element 3 of array 1042:

```sh
bkill --signal kill 1042[3]
```

An individual element's own job ID also works directly with `bkill`,
`bjobs`, and `bhist`, the same as for an ordinary job.

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
bjobs --all
```

Verify:

- Queues are visible.
- Hosts are visible.
- Host groups are correct.
- Token pools are correct.
- Jobs are visible.
