# LavaLite 2.0.0 Release Notes

## Overview

LavaLite 2.0.0 adds services: long-running applications such as
Jupyter or code-server, started by users on demand and reached over
the network. A service instance is an ordinary job with its own
network namespace, so it is scheduled, accounted, signaled and
recovered by the same machinery as every batch job.

The release also hardens the daemons for failures: hosts that stop
answering, daemons killed or stopped under load, configuration
errors, and recovery from the manifest.

## New in 2.0.0

### Services

- Services are defined by the administrator in `llb.services`: the
  command, the application port, the queue, the restart policy, an
  optional run limit and an optional token variable.
- `bservices -s name` starts an instance and prints its endpoint,
  `http://host:port`. `bservices -d` deletes one, by job id or by
  endpoint. `bservices` lists the user's instances, `bservices -l`
  the service definitions.
- Each instance runs in its own network namespace on the execution
  host. The endpoint is a DNAT rule from a host port, starting at
  30000, to the application port inside the namespace. The namespaces
  use addresses from `10.200.0.0/16`, one `/30` per instance.
- An instance gets a random token in the variable named by
  `TOKEN_ENV`, for applications that protect their endpoint with it.
- `RESTART = always` brings an instance back after its host fails,
  with the same job id. `RESTART = never` lets it end.
- `bservices --time` limits the life of an instance, capped by the
  service's `RUNLIMIT`. An instance that reaches its run limit ends
  and is not restarted.
- `bjobs` and `bservices` show `DISP` for a job dispatched to a host
  that has not started it yet.

Use services from the login node, not from the execution hosts: see
Services in the administrator guide.

### Failure handling

- **Unresponsive hosts.** If a connected `sbd` does not report the
  pid of a dispatched job within 20 seconds, `mbd` disconnects it.
  Its jobs show as `UNKNOWN` and are resynchronized when `sbd`
  registers again. No job is killed or requeued.
- **Host taint.** A host whose `sbd` rejects a job, because it
  cannot create the job directories, the service namespace or the
  process, is tainted for 60 seconds and gets no new jobs. The job
  runs elsewhere. This closes the 1.1.0 limitation of a host that
  kept rejecting the same job on every scheduler tick.
- **Dead `mbd` link.** `sbd` uses TCP keepalive and a user timeout
  on its connection to `mbd`, and notices a dead link in about 90
  seconds instead of waiting for TCP retransmissions to give up.
- **Orphan jobs.** A job that references a queue, host, service or
  token pool removed from the configuration is frozen as an orphan
  instead of being lost. It keeps what it holds and comes back when
  the configuration is fixed. An orphan job can be killed with
  `bkill -s KILL <job_id>`.
- **Configuration errors.** An error in a configuration file is
  logged and the faulty entry is skipped or set to its default.
  `mbd` keeps running.
- **Removed manifest.** `mbd` stops at its next write if the
  manifest was removed while it runs, instead of starting a new one
  without the jobs it holds in memory.
- **User identity on execution hosts.** `sbd` rejects a job whose
  uid is unknown on the host, or belongs there to another user name
  than on the submission host. The job runs elsewhere.

### Manifest and history

- A manifest compaction is triggered by the number of finished jobs
  older than `LL_MBD_JOB_FINISH_PERIOD`, the ones it can drop, not
  by all finished jobs. A busy cluster no longer rewrites the
  manifest without dropping anything.
- `bhist` reads the manifest archives in order, oldest first, the
  current manifest last. A job whose events are spread over several
  archives is shown in the right order.
- Manifest archives are never removed by LavaLite. The administrator
  guide describes how to prune them.

### Packaging and operation

- systemd units for `mbd` and `sbd` in `etc/`, with `Restart=on-failure`.
- Each daemon changes to its own state directory, where a core file
  lands when `kernel.core_pattern` writes to the current directory.
- `sbd` no longer listens on a port: it connects to `mbd` and
  registers. `mbd` never connects to `sbd`.

## Bug Fixes

- **Stop and continue on multi-host jobs.** The run and suspend
  counters were moved on the first host only. The other hosts of the
  job drifted, and with `LL_ASSERT_COUNTERS=1` `mbd` aborted.
- **GPUs of multi-host jobs.** GPUs were marked in use on the first
  host only, but freed on every host when the job ended: the job
  freed the GPU of another job, and two jobs could get the same GPU.
- **Suspended job lost across a restart.** A suspend was not written
  to the manifest: after an `mbd` restart the job showed `RUN`, a
  continue was ignored, and the job stayed frozen on its host.
- **`bhist` order.** Archives were read in directory order, and a
  job's events could appear out of order or with a wrong status.

## Removed

- The `sbd` listening port.

## Testing

Besides the system tests in `src/tests/system`, 2.0.0 was tested with:

- `src/tests/sim/simqa.sh`: robustness scenarios on a one box cluster
  of simulated hosts, `docs/testing/18-robustness.md`.
- `src/tests/chaos`: `jload` and `sload` keep the cluster busy as
  ordinary users, while `chaos` kills, freezes and stops `mbd` and
  `sbd`. Runs of one to two hours with several users, see
  `docs/testing/20-chaos.md`.

## Platform Support

Tested on:

- Rocky Linux 8
- Rocky Linux 9
- Ubuntu 24.04

## Known Limitations

- **Multi-host jobs start on the first host only.** LavaLite
  allocates the hosts and gives the list to the job in `LL_HOSTS`.
  Starting processes on the other hosts is up to the application or
  its launcher. A multi-host job with GPUs gets no
  `CUDA_VISIBLE_DEVICES`: run it exclusive with all the GPUs of each
  host, see `llb.hosts(5)`.
- **The shared key authenticates the cluster, not the user.** It is
  world readable, and whoever reads it can sign a request with any
  uid. The trust boundary is the cluster.
