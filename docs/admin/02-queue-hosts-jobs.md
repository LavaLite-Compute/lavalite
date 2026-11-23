# LavaLite Administrator Guide — Queues, Hosts, and Jobs

## Overview

A job is the unit of work in LavaLite. Users submit jobs to queues;
the scheduler matches each job's requirements against the hosts and
other resources available to its queue. A queue provides scheduling
policy, hosts provide CPU, memory, storage, and GPUs, and token pools
represent shared resources such as software licenses. A job starts only
when its scheduling conditions and resource requirements can be met.

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
- Defining services, managing their instances, and inspecting their
  network setup on the execution host

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
- Still accepts new jobs, they stay pending
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

Token pools represent host-independent resources whose availability controls job dispatch.

A token pool is a shared bucket of tokens. Jobs acquire the requested tokens when they start and return them to the pool when they finish. If insufficient tokens are available, the job remains pending.

A floating software license is a typical example: jobs can run on different hosts, but the total number of concurrent license users is limited by the number of available tokens.

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

A complete submission example is given under *Submitting Jobs* below.

## Jobs and Submission

### What Is a Job?

A batch job is a command submitted with `bsub`. LavaLite gives it a
unique job ID and records its queue, requested resources, and scheduling
conditions. Submission does not mean immediate execution: a submitted
job remains pending until it is eligible and `mbd` finds a suitable
allocation.

The normal path is:

1. `bsub` submits the command and its requirements to `mbd`.
2. `mbd` places the job in a queue and evaluates its scheduling conditions.
3. When resources are available, `mbd` allocates host resources and any
   requested tokens, then dispatches the job to `sbd`.
4. `sbd` starts the job on the execution host, applies cgroup limits,
   and reports its state to `mbd`.
5. When the job ends, its allocation is released and its execution
   history remains available through `bhist`.

A queue is not an execution host: it defines where and under which
policy jobs may run. Jobs are dispatched in first-come, first-served
order by default. Host selection is made by the scheduler, subject to
queue policy and each job's requirements.

### Submitting Jobs

Submit a command to the default queue:

```sh
bsub sleep 60
```

On successful submission, `bsub` prints the assigned job ID:

```text
Job <42> is submitted to the default queue.
```

Choose a queue, provide a job name, and request resources:

```sh
bsub --queue batch --name simulation --cpus 4 --mem 8G ./simulate
```

The request is for four CPU slots and 8 GB of memory on one host.
Unless specified otherwise, a job requests one CPU slot on one host.
`--storage` requests local scratch capacity; `--gpus` requests GPUs,
optionally constrained by `--gpu-model`. With `--exclusive`, the host
cannot be shared with other jobs for the duration of the job.

Restrict placement to a particular host or host group:

```sh
bsub --machines "worker1" ./simulate
```

Request a shared license token in addition to host resources:

```sh
bsub --tokens license=1 sleep 3600
```

The job remains pending if its tokens or other required resources are
unavailable. To observe and release the token allocation in this example:

```sh
btokens
bkill --signal kill <jobid>
btokens
```

The final `btokens` shows the tokens returned after the job has ended.

### Job Input and Output

A job executes on an execution host rather than in the interactive
shell that submitted it. By default its standard input is `/dev/null`;
its standard output and standard error go to `stdout.<jobid>` and
`stderr.<jobid>` in the submission working directory.

Output paths can be selected explicitly; `%J` expands to the job ID:

```sh
bsub --stdout result.%J.out --stderr result.%J.err ./simulate
```

Only a subset of the submission environment is passed to the job;
full inheritance of the interactive desktop environment is not
guaranteed. See `bsub(1)` for all options and environment details.

### Scheduling Conditions and Job States

A submitted job normally starts in `PEND` and is dispatched when it is
eligible and resources are available. Other conditions can delay it:

- `--hold` submits a job in `HELD` state until it is released with
  `bkill --signal cont <jobid>`.
- `--begin` prevents dispatch before a specified time.
- `--dependency` waits for the required state of earlier jobs; see
  *Job Dependencies* below.

`bjobs` reports `PEND` (waiting), `DISP` (dispatched but not started),
`RUN` (executing), `SUSP` (suspended), `HELD`, `DONE` (successful
completion), or `EXIT` (nonzero exit status), among other diagnostic
states. Use `bjobs --pend` to inspect pending jobs and their reasons.

A running job remains alive until the last process in its cgroup exits.
The job script's exit does not by itself cause `sbd` to kill remaining
processes. Job processes are signaled when requested or when a
configured limit requires termination. The job's reported exit status
is the job script's exit status; see the architecture overview for the
full lifecycle rule.

### Resource Allocation

A job may request CPUs, memory, storage, GPUs, host placement, and
tokens. All requests must be satisfied for dispatch. CPU and memory
limits are enforced through cgroup v2 on the execution host; GPU
allocation prevents conflicting placement but does not currently
provide operating-system-level device isolation.

For jobs spanning several hosts, resources are reserved on each host,
but the job script starts on the first host only. See *Multi-Host Jobs*
below for the launcher and process-control implications.

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

### Display Job History

`bjobs` shows what `mbd` holds now: active jobs and the recently
finished ones. `bhist` reads the manifest and its archives on disk:
it also shows jobs `mbd` has already dropped, and the events of each
job in order.

Without arguments `bhist` shows one block per job of the calling
user, `-u user` for another user. With a job id it adds the job's
events:

```sh
bhist 109139
```

```text
Job <109139>  User <sofia>  Queue <batch>  Status <DONE>
  Submitted:    Oct 31 16:24:19
  Submit host:  buntu24
  Name:         -
  CWD:          /home/sofia/tmp
  Command:      sleep 10
  Priority:     50
  stdout:       /dev/null
  stderr:       /dev/null
  Requested resources:  2 host(s)  4 cpu(s)/host  1 gpu(s)/host  0 MB mem

  Oct 31 16:24:21  Dispatched to:  sim1 sim3
                  GPU devices:  0
  Oct 31 16:24:21  Forked  pid: 2988
  Oct 31 16:24:32  Finished  state: DONE exit_status: 0

  CPU time:     0.00 sec
  Max memory:   1 MB
  Max swap:     0 MB
```

`bhist -r` and `bhist -p` show only the running or pending jobs. Every
`bhist` reads all the manifest archives: see Manifest Archives in the
operations guide for pruning them.

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

### Array Size Limit

The number of elements in an array is capped by `LL_ARRAY_MAX_SIZE`
in `ll.conf` (default `1000`). The limit applies to the element
count, not the numeric range — `--array 1-100000:1000` has only 100
elements and is unaffected by a lower cap, while `--array 1-2000`
exceeds the default.

A submission over the configured limit is rejected before any
element is created:

```sh
bsub --array 1-5000 myprogram
```

```
mbd: array size 5000 exceeds LL_ARRAY_MAX_SIZE (1000)
```

Array elements are fully created at submission time — there is no
separate array entity, so each element requires its own job record.
This work happens synchronously before `bsub` returns, so submission
time grows with element count. Requesting an array near the
configured limit can noticeably delay `bsub`'s return; this is
expected, not a hang. Raising `LL_ARRAY_MAX_SIZE` trades a higher
per-submission ceiling for a longer worst-case delay.

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

## Job Dependencies

Job dependencies allow a job to remain pending until one or more previously
submitted jobs reach the required state. A dependency condition is specified at
submission with `--dependency` and may use `done(jobid)`, `exit(jobid)`, and
`ended(jobid)`. Conditions can be combined with `&&`, `||`, and parentheses.

For example, submit a job that runs only after job 101 completes successfully:

```sh
bsub --dependency 'done(101)' postprocess.sh
```

Run a job after both jobs 101 and 102 complete successfully:

```sh
bsub --dependency 'done(101) && done(102)' merge-results.sh
```

A condition may also react to failure or simply wait for completion regardless
of exit status:

```sh
bsub --dependency 'done(101) || exit(101)' cleanup.sh
bsub --dependency 'ended(101) && ended(102)' final-report.sh
```

A typical use case is a multi-stage workflow. Several independent simulation
jobs can be submitted first, followed immediately by a post-processing job that
depends on all simulations completing successfully. The dependent job remains
pending without requiring the user to monitor the earlier jobs or manually
submit the next stage when they finish.

## Multi-Host Jobs

A job submitted with `--nhosts` greater than 1 receives a list of
allocated hosts in `LL_HOSTS`, for example `hostA 4,hostB 4`. Each
entry gives the host name and the number of CPUs allocated there.

LavaLite starts the job on the first host only. The application or
launcher must start processes on the remaining hosts, for example
with SSH, Ray, or MPI. Processes started this way are outside the
LavaLite job cgroup and are not reached by `bkill`.

A multi-host job with `--gpus` holds its GPUs on every host but gets
no `CUDA_VISIBLE_DEVICES`. Run it with `--exclusive` and all the GPUs
of each host: every GPU of a host is then the job's, and the framework
picks one per process by its local rank. See `llb.hosts(5)`.

Starting the job on every host is planned for a future release.

## Services

A service is a long-running application, such as Jupyter or
code-server, defined in `llb.services`. Users start instances of it
with `bservices`. Each instance is a job in the service's `QUEUE`,
subject to the same dispatch, limits, and accounting as other jobs.
It runs in a
network namespace on an execution host and is reached through
`http://host:port`.

The job ID is the stable handle of an instance. The endpoint is not:
when an instance with `RESTART = always` ends, because the
application exited, was killed or its host failed, it starts again
with the same job ID, possibly on another host and port. `bservices`
always shows the current endpoint.

### Define a Service

Services are defined in `$LL_CONF_DIR/llb.services`, one block each:

```
Begin Service
SERVICE_NAME = jupyter
COMMAND      = apptainer exec --bind $HOME /images/jupyter.sif jupyter lab --ip=0.0.0.0 --port=$LL_SVC_PORT --no-browser
PORT         = 8888
TOKEN_ENV    = JUPYTER_TOKEN
RESTART      = always
RUNLIMIT     = 8:00
QUEUE        = service
End Service
```

- `COMMAND` runs as is in the job's shell. The application must
  listen on `$LL_SVC_PORT`, which is `PORT`, on all addresses
  (`0.0.0.0`): an application listening on `127.0.0.1` only is not
  reachable through the endpoint.
- `PORT` is the port inside the namespace, the same for every
  instance. The endpoint port on the host is picked by `sbd`.
- `QUEUE` must be in `llb.queues`. Its hosts need the setup of
  Service Hosts in the installation guide.
- `RESTART`, `RUNLIMIT` and `TOKEN_ENV`: see `llb.services(5)`.

`mbd` reads `llb.services` at startup. After a change:

```sh
systemctl restart lavalite-mbd
```

A wrong block does not stop `mbd`: the service is not configured and
the `mbd` log says why. Check the result:

```sh
bservices
bservices -l
```

`bservices` lists every configured service with its queue and restart
policy, `-l` the full definition of each.

### Start, List and Delete Instances

Start an instance:

```sh
bservices -s jupyter
```

The command blocks until the instance runs, then prints its job ID
and URL, with the token when the service has `TOKEN_ENV`:

```text
Job <3> http://worker4:30000/?token=3adf689f99445585f4564b12f98de424
```

`-t [hours:]minutes` gives the instance a shorter run limit than
`RUNLIMIT`. The limit counts from the first start, a restart does not
reset it.

Only the instance owner can see its token. `bservices` shows `-`
to everyone else, including managers.

List the instances:

```sh
bservices
bservices -a
```

A user sees their own instances, a manager all of them. `-a` adds the
finished ones `mbd` still holds. The instance is also a job: `bjobs`,
`bhist` and `bjobs -m` show it like any other.

Delete an instance, by job ID or by endpoint:

```sh
bservices -d 3
bservices -d worker4:30000
```

A deleted instance is not restarted. `bkill` refuses service jobs;
use `bservices -d` to delete an instance.

### Use Services from the Login Node

Users start instances and connect to their endpoints from the login
or frontend node, a host that is not in the service queue's `HOSTS`.
The endpoint is a DNAT rule in the `prerouting` chain of the execution
host, and that chain only sees traffic arriving from other hosts: a
connection from the execution host to itself never passes through it
and gets nothing. From any other host the endpoint works.

So keep the login node out of the service queue. Users reach the
endpoint from their workstation through the login node, for example
for the instance above:

```sh
ssh -L 8888:worker4:30000 user@login
```

then open `http://localhost:8888/?token=...` in the browser. Routing
beyond the login node is the site's own.

### Network Setup on the Execution Host

When a service instance starts, `sbd` allocates the lowest available
endpoint port in the range 30000–46383. Each port identifies a slot:

```text
s = endpoint_port - 30000
```

For job `N`, `sbd` creates:

- **Network namespace:** `svcN`, accessible through `/run/netns/svcN`.
- **Veth pair:** `llNa` stays on the execution host; `llNb` moves into
  `svcN`. Both use addresses from the slot's `/30` subnet:
  - Host (`llNa`): `10.200.0.0 + 4s + 1`
  - Namespace (`llNb`): `10.200.0.0 + 4s + 2`
- **Namespace routes:** the loopback interface is up, and the default
  route points to `llNa`.
- **nftables table:** `ip svcN`, with two NAT chains:
  - `prerouting` forwards TCP connections to the host's endpoint port
    (`30000 + s`) to `llNb` at the service's configured `PORT`.
  - `postrouting` masquerades outbound traffic from the namespace,
    allowing replies to return through the execution host.

The same slot determines the endpoint port and both IP addresses.
For example, job 3 in slot 0 has endpoint port 30000, host address
`10.200.0.1/30`, and namespace address `10.200.0.2/30`:

```text
  login node                       worker4
  ----------                       -------------------------------------------
  curl worker4:30000  --->  eth0   nft table ip svc3
                                     prerouting: tcp dport 30000
                                                 dnat to 10.200.0.2:8888
                                     postrouting: ip saddr 10.200.0.2
                                                 masquerade
                                        |
                                   ll3a 10.200.0.1/30
                                        |  veth pair
                                   +----|--------------------------+
                                   |  ll3b 10.200.0.2/30   netns   |
                                   |  jupyter on 0.0.0.0:8888 svc3 |
                                   +-------------------------------+
```

Network resources are named after the job ID, so they can be inspected
without knowing the slot. The instance's processes remain in the job's
cgroup. `sbd` saves the allocated slot in its state file; restarting
`sbd` does not change a running instance's endpoint.

When the instance finishes, `sbd` retains its network resources until
all processes in the job's cgroup have exited and `mbd` has acknowledged
the finish. It then removes the namespace, veth pair, and nftables table
and releases the slot.

### Inspect an Instance

On the execution host, as root, for job 3:

```sh
ip netns list
ip netns pids svc3
```

`svc3` is listed, and the application's processes are in it.

```sh
ip link show ll3a
ip -n svc3 addr
ip -n svc3 route
```

`ll3a` is up on the host with `10.200.0.1/30`. In `svc3`: `lo` and
`ll3b` up, `10.200.0.2/30`, `default via 10.200.0.1`.

```sh
ip netns exec svc3 ss -ltnp
```

The application listens on `0.0.0.0:8888`, the `PORT`.

```sh
nft list table ip svc3
```

```text
table ip svc3 {
	chain prerouting {
		type nat hook prerouting priority dstnat; policy accept;
		tcp dport 30000 dnat to 10.200.0.2:8888
	}

	chain postrouting {
		type nat hook postrouting priority srcnat; policy accept;
		ip saddr 10.200.0.2 masquerade
	}
}
```

Then from the login node:

```sh
curl -s -o /dev/null -w '%{http_code}\n' http://worker4:30000/
```

Any HTTP status means the application answered.

### Troubleshooting

The endpoint does not answer:

- The connection comes from the execution host itself: try from the
  login node, see above.
- `sysctl net.ipv4.ip_forward` is `0` on the execution host: set it to
  `1`, see Service Hosts in the installation guide.
- `ip netns exec svcN ss -ltnp` shows nothing on `PORT`, or only
  `127.0.0.1`: the application did not start, listens on another port
  or on localhost only. Check `COMMAND` and the job's output.
- A firewall on the execution host drops the endpoint ports, 30000
  up: open them from the login node.

A running instance always has its namespace, veth pair and table: if
any step of the setup fails, `sbd` undoes the others and rejects the
job before it starts. The instance goes back to pending, the host is
tainted for 60 seconds, `ok|tainted` in `bhosts`, and the instance is
dispatched to another host. The `sbd` log of the tainted host says
which `ip` or `nft` command failed. A common one is
`netns=svcN cannot be created`: a namespace with that name was left on
the host. Remove it:

```sh
ip netns del svcN
nft delete table ip svcN
```

After `bservices -d N` and the job's end, nothing of it is left on
the host: no `svcN` in `ip netns list`, no `llNa` in `ip link`, no
table `svcN` in `nft list tables`.

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
