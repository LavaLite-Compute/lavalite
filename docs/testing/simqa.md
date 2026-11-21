# Running simqa.sh

`src/tests/sim/simqa.sh` runs the automated robustness and job
lifetime scenarios on one box. It builds its own configuration in a
scratch directory, starts its own `mbd` and two sim `sbd`s, runs the
scenarios, prints a summary and tears everything down. It needs no
running cluster and does not touch an installed one.

## What it starts

- `mbd`, as `SIMQA_USER`, on port `SIMQA_PORT`.
- Two sim `sbd`s, `worker1` and `worker2`, as root. They are
  `sbd --simulator`, cloned from the real host `SIMQA_HOST`, 4 CPUs,
  8G memory and two GPUs `H100 0-1` each.
- `SIMQA_HOST` itself is in the configuration with no `sbd`: it stays
  `unavail`, some scenarios submit to it on purpose.
- The job cgroups go under their own root, `/sys/fs/cgroup/simqa`,
  not the default `/sys/fs/cgroup/lavalite`.

## Next to an installed cluster

`simqa.sh` can run on a box where LavaLite is installed and running,
also an execution host with jobs: its own port, configuration, state
and cgroup root keep the two apart. The sim job ids start at 1, under
the same cgroup root they would share the `job_<id>` of the real jobs.

One thing is shared: at the end `simqa.sh` kills every `sleep 9xxx`
of `SIMQA_USER`. Use a test user that has no real jobs, the default
`lavalite` is one.

Queues `normal` and `extra`, a token pool `lic` of 2, the keys and
`ll.conf` are all generated. Requests are signed by `llsign` from the
build tree.

## Requirements

- A built tree: `simqa.sh` runs `mbd`, `sbd` and the commands from
  `src/batch` next to it, nothing installed is used.
- Root: the sim `sbd`s run as root, `mbd` refuses to.
- A test user, default `lavalite`, that owns the jobs and is the
  LavaLite administrator of the sim cluster.
- `SIMQA_HOST` must resolve to the address the clients connect from.
  `localhost` works on most boxes.
- The `mbd` port `SIMQA_PORT` free.
- For the job lifetime scenarios, cgroup v2 as for a real execution
  host, see `docs/admin/01-install.md`. Without it the three scenarios
  that need the cgroup print `SKIP`, the others still run.

## Running

From the top of the tree:

```sh
sudo ./src/tests/sim/simqa.sh
```

With another user:

```sh
sudo SIMQA_USER=qatest ./src/tests/sim/simqa.sh
```

`t_terminate_setsid` alone takes up to 3 minutes: it waits for a
`--terminate` deadline with minute granularity and the 60 seconds
grace.

### Environment

| Variable | Default | Meaning |
|---|---|---|
| `SIMQA_USER` | `lavalite` | job owner and LavaLite administrator |
| `SIMQA_HOST` | `localhost` | the real host the sims are cloned from |
| `SIMQA_BIN` | `src/batch` next to the script | build tree to run |
| `SIMQA_PORT` | `43124` | `mbd` port |
| `SIMQA_KEEP` | unset | `1` keeps the scratch directory |
| `SIMQA_ONLY` | all | scenarios to run, space separated |

### Running some scenarios

```sh
sudo SIMQA_ONLY="t_linger t_setsid_signals" ./src/tests/sim/simqa.sh
```

The names are the `t_` functions of the script, listed below.
`t_counters` checks that `mbd` survived with consistent counters,
add it last when running a subset.

## Output

Every scenario prints a `START` line with its name and time, then one
or more results:

```text
START: linger 18:42:10
PASS: linger
START: setsid_signals 18:42:31
FAIL: setsid_signals: no SUSP, state=RUN
```

`PASS` and `FAIL` are counted, `SKIP` is not. The last line is the
summary:

```text
Results: 22 passed, 1 failed
```

The exit status is 0 only when nothing failed.

## Reading a failure

On a failure, or with `SIMQA_KEEP=1`, the scratch directory is kept
and its path printed (`kept /tmp/simqa.XXXXXX`). It has:

| Path | Content |
|---|---|
| `conf/` | the generated `ll.conf`, `llb.hosts`, `llb.queues`, keys |
| `log/mbd.log.*` | `mbd` log, `LOG_DEBUG` |
| `log/worker1.log.*`, `log/worker2.log.*` | sim `sbd` logs |
| `state/` | `mbd` manifest and `sbd` state files |
| `mbd.out`, `sbd.*.out` | daemon stdout and stderr, assertions end up here |
| `jobs/` | the job scripts of the job lifetime scenarios |

The job cgroups are kept too (`kept /sys/fs/cgroup/simqa`), with the
processes still in them: `cgroup.procs`, `cgroup.events`, the memory
and CPU files of each `job_<id>`. The next run kills what is left
there and removes them before it starts.

The `FAIL` line says what was expected. Search the logs for the job
id: `grep job=<id> log/*`.

Between scenarios `settle` kills every job and waits for both workers
`ok`, so one scenario's leftovers do not leak into the next.

## Scenarios

### Robustness, `18-robustness.md`

| Scenario | Test |
|---|---|
| `t_pid_timeout` | TEST-230 silent `sbd` is disconnected |
| `t_pid_timeout_mbd_restart` | TEST-231 no timeout across `mbd` restart |
| `t_taint` | TEST-235 rejecting `sbd` taints its host |
| `t_kill9_running` | TEST-250 `bkill -s kill`, running job |
| `t_kill9_disp` | TEST-251 `bkill -s kill`, dispatched job |
| `t_kill9_disconnected` | TEST-253 `bkill -s kill`, disconnected host |
| `t_kill9_all` | TEST-254 `bkill -s kill 0` |
| `t_orphans` | orphan jobs after a configuration mistake |
| `t_token_pool_orphan` | TEST-247 removed token pool |
| `t_replay_machines_tokens` | TEST-260 replay keeps `--machines` and `--tokens` |
| `t_retention` | TEST-261 finished jobs kept for the period |
| `t_suspend_restart` | TEST-263 suspended job across `mbd` restarts |
| `t_gpu_multihost` | TEST-264 GPUs of a multi-host job |
| `t_manifest_removed` | `mbd` dies when its manifest is removed |
| `t_finish_reused_id` | TEST-265 finish for a reused job id |
| `t_counters` | TEST-270 `mbd` alive, counters consistent, run last |

### Configuration, `19-config-changes.md`

| Scenario | Test |
|---|---|
| `t_user_max_cpus` | TEST-323 `USER_MAX_CPUS` |

### Job lifetime, `22-job-lifetime.md`

| Scenario | Test |
|---|---|
| `t_linger` | TEST-330 child keeps the job running |
| `t_setsid_signals` | TEST-331..333 `setsid`, stop, cont, term |
| `t_setsid_kill9` | TEST-334 `bkill -s kill` after the script exited |
| `t_terminate_setsid` | TEST-335 `--terminate` after the script exited |
| `t_linger_sbd_restart` | TEST-336 `sbd` restart while the job lingers |
| `t_no_children` | TEST-337 script with no children |

### Not in the default list

`t_system` runs `src/tests/system/run_tests.sh` against the sim
cluster as a regression run:

```sh
sudo SIMQA_ONLY=t_system ./src/tests/sim/simqa.sh
```

`bsub_gpu` fails there, the sims have no real GPU, and `bsub_io`
needs `/export/soft/tmp`.
