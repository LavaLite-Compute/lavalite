# Robustness Tests

## Purpose

Verify how LavaLite behaves when an execution host or the
configuration does not cooperate: a silent `sbd`, an `sbd` that
rejects jobs, a configuration change that orphans jobs, a job killed
with `-9` on a host that may or may not answer, and the retention of
finished jobs across compaction and restart.

These tests cover:

- pid timeout, see *Unresponsive Execution Hosts* in the operations guide
- host taint, see *Job Launch Failures*
- orphan jobs, see *Orphan Jobs*
- `bkill -s kill`, see `bkill(1)`
- `LL_MBD_JOB_FINISH_PERIOD`, see `ll.conf(5)`

## Prerequisites

- Environment validation completed.
- At least two execution hosts, here `worker1` and `worker2`, and one
  configured host without `sbd`, here `nodeX`.
- Root on the execution hosts, to stop and resume `sbd`.
- Administrator access to the configuration and to `mbd` restart.
- `LL_ASSERT_COUNTERS=1` in `ll.conf`, so a counter error stops `mbd`.

`sbd` is stopped and resumed with signals, never killed:

```sh
kill -STOP $(pidof sbd)      # on the execution host
kill -CONT $(pidof sbd)
```

Stop `sbd` *before* submitting when a test needs the job in `DISP`:
the dispatch then waits in the socket buffer and there is no race.

## Automation

`src/tests/sim/simqa.sh` runs most of these tests on one box, with
two sim `sbd`s, `worker1` and `worker2`, cloned from a real host
without `sbd`. It needs root and a test user:

```sh
sudo SIMQA_USER=lavalite ./src/tests/sim/simqa.sh
```

`SIMQA_ONLY="t_taint t_orphans"` runs a subset, `SIMQA_KEEP=1` keeps
the scratch directory with logs, state and manifest.
`SIMQA_ONLY=t_system` runs `src/tests/system/run_tests.sh` against
the same sim cluster as a regression run; `bsub_gpu` fails there, the
sims have no GPU, and `bsub_io` needs `/export/soft/tmp`. Each test below
names its automated scenario, or says **manual** when it needs a real
network, `gdb` or a service.

---

## Pid timeout

### TEST-230: Silent sbd is disconnected

Automated: `t_pid_timeout`.

#### Commands

```sh
kill -STOP $(pidof sbd)                 # worker1
bsub --machines worker1 sleep 600
bjobs <jobid>                           # DISP
# wait 20 to 40 seconds
bjobs <jobid>; bhosts
kill -CONT $(pidof sbd)                 # worker1
bjobs <jobid>
```

#### Expected Result

- `DISP`, then `UNKNOWN` within 40 seconds, `worker1` `unavail`.
- `mbd` logs `job=N no pid from sbd=worker1 in S secs, disconnecting it`
  once.
- After resume, `RUN` on `worker1`, same job, not requeued.

#### Pass Criteria

The job reaches `RUN` on `worker1` without being dispatched again.

### TEST-231: No timeout while sbd is disconnected across mbd restart

Automated: `t_pid_timeout_mbd_restart`.

#### Commands

As TEST-230, but restart `mbd` while `sbd` is stopped, then wait 45
seconds before resuming `sbd`.

#### Expected Result

- After the restart the job is `UNKNOWN`; no `no pid` log line for it
  while `sbd` is stopped.
- After resume, `RUN` on `worker1`.

#### Pass Criteria

No timeout fires for a job whose `sbd` is not connected.

### TEST-232: Lost reply, sbd reconnects (manual)

#### Commands

```sh
iptables -A OUTPUT -p tcp --dport $LL_MBD_PORT -j DROP   # worker1
bsub --machines worker1 sleep 600
# wait for UNKNOWN
iptables -D OUTPUT -p tcp --dport $LL_MBD_PORT -j DROP
```

#### Expected Result

`UNKNOWN`, then `RUN` once `sbd` reconnects. How long `sbd` takes to
notice depends on TCP retransmission and keepalive.

#### Pass Criteria

The job reaches `RUN` without being dispatched again.

#### Note

With a one-way drop the outcome depends on the size of the dispatch.
TCP sends only about 14 KB without acknowledgement: a larger
`BATCH_NEW_JOB`, for example a user with a big environment, reaches
`sbd` only in part. `sbd` never sees the job, the pid timeout closes
the channel on the `mbd` side, and the idle `sbd` does not notice.
`sbd` keepalive on its `mbd` socket finds the dead connection within
about 90 seconds once the rule is removed, drops it and reconnects;
the job comes back through `BATCH_JOB_MISSING` and is dispatched
again.

---

## Host taint

### TEST-235: Rejecting sbd taints its host

Automated: `t_taint`, which makes `sbd` reject by replacing its job
directory with a plain file.

#### Commands

```sh
# on worker1, make sbd fail to create the job directory
bsub --machines worker1 sleep 600
bhosts
bjobs <jobid>
# restore, wait 60 seconds
bhosts; bjobs <jobid>
```

#### Expected Result

- `mbd` logs `job=N rejected by sbd` once and
  `host=worker1 tainted for 60 secs`.
- `bhosts` shows `ok|tainted`; the job is `PEND` and is not sent to
  `worker1` again while the host is tainted.
- After 60 seconds `bhosts` shows `ok` and the job runs.

#### Pass Criteria

One reject only during the taint, and the taint expires by itself.

### TEST-236: Taint cleared by sbd restart (manual)

#### Commands

Taint `worker1` as in TEST-235, then restart its `sbd`.

#### Expected Result

`bhosts` shows `ok` as soon as `sbd` registers again.

#### Pass Criteria

The taint does not outlive the `sbd` that caused it.

---

## Orphan jobs

All automated in `t_orphans`, one configuration mistake checked from
several sides. Setup: a running job in queue `extra` on `worker1`
holding one `lic` token, a pending job in `extra`, a running job on
`worker2`, a running job in `normal` on `worker1` holding one `lic`
token. Then remove queue `extra` and host `worker2` from the
configuration and restart `mbd`.

### TEST-240: Orphans are marked

#### Expected Result

The three jobs referencing `extra` or `worker2` are `ORPHAN`; the job
in `normal` on `worker1` is `RUN`. `mbd` logs
`queue=extra not in the config, stale` and
`host=worker2 not in the config, stale`.

#### Pass Criteria

Exactly the jobs referencing a missing object are `ORPHAN`.

### TEST-241: Orphans are frozen

#### Commands

```sh
bkill <orphan>
bmove --to normal <orphan>
bpriority --priority 1 <orphan>
```

#### Expected Result

All three refused with `Operation not permitted`.

#### Pass Criteria

No orphan changes state.

### TEST-242: Orphans hold what they hold

#### Commands

```sh
bhosts; btokens
bsub --machines worker1 sleep 600     # three times
```

#### Expected Result

- `worker1` counts the orphan: `NJOBS` 2, `lic` used 2.
- Of the three new jobs two run and one pends: `worker1` has four
  cpus and the orphan holds one.

#### Pass Criteria

The scheduler never hands out what an orphan holds.

### TEST-243: Removed host cannot register

#### Expected Result

`mbd` logs `register from unknown host worker2`, `bhosts` does not
list `worker2`.

#### Pass Criteria

The `sbd` of a removed host stays out.

### TEST-244: Finish of a frozen job is not acknowledged

#### Expected Result

When the orphan on `worker1` ends, `mbd` logs
`job=N is orphan, finish from sbd=worker1 not acked`, every second.
`sbd` keeps the job.

#### Pass Criteria

The finish stays on `sbd` until the job heals.

### TEST-245: Restore the configuration, orphans heal

#### Commands

Restore queue `extra` and host `worker2`, restart `mbd`.

#### Expected Result

- The job that ended while frozen becomes `DONE`: `sbd` resends the
  finish and it is acknowledged.
- The job on `worker2` is `RUN` with its original pid.
- The pending orphan is `PEND`.

#### Pass Criteria

Every orphan continues exactly where it was.

### TEST-246: Kill an orphan

#### Commands

Remove the configuration again, restart `mbd`, then

```sh
bkill -s kill <running orphan> <pending orphan>
```

#### Expected Result

Both `EXIT` at once.

#### Pass Criteria

`-9` is the only signal an orphan accepts, and it ends the job.

### TEST-247: Removed token pool makes an orphan

Automated: `t_token_pool_orphan`.

#### Expected Result

A running job holding `lic` is `ORPHAN` after the `lic` pool is
removed and `mbd` restarted, and `RUN` again once the pool is back.

#### Pass Criteria

Token pools follow the same rule as queues and hosts.

### TEST-248: Orphan service (manual)

#### Commands

Start a service, remove it from `llb.services`, restart `mbd`, then
restore it and restart again.

#### Expected Result

The instance job is `ORPHAN`, then running again with the same
endpoint and token. While frozen the instance is not listed by
`bservices`, its service is not in the config; `bjobs` shows it
`ORPHAN`.

#### Pass Criteria

The service link survives the freeze.

---

## bkill -s kill

### TEST-250: Running job, sbd connected

Automated: `t_kill9_running`.

#### Expected Result

- `EXIT` at once, the host is free at once.
- `sbd` logs `sig=9`, then `finish_acked and freed`.
- No process left on the host.

#### Pass Criteria

The job ends in `mbd` before `sbd` answers, and `sbd` drains.

### TEST-251: Dispatched job, messages cross

Automated: `t_kill9_disp`.

#### Commands

```sh
kill -STOP $(pidof sbd)                 # worker1
bsub --machines worker1 sleep 600
bkill -s kill <jobid>                   # job is DISP
kill -CONT $(pidof sbd)
```

#### Expected Result

- `EXIT` while `DISP`.
- After resume `mbd` logs `job=N finished, pid=P from sbd=worker1
  acked and discarded`, then the finish takes the duplicate path.
- No process left.

#### Pass Criteria

A pid reply crossing the kill does not bring the job back.

### TEST-252: Reject crossing the kill (manual, gdb)

#### Commands

Break in `sbd_job_new` on `worker1`, submit, `bkill -s kill` the job,
make `sbd` reject it, continue.

#### Expected Result

`mbd` logs `job=N finished, reject from sbd=worker1 dropped`, no
assert.

#### Pass Criteria

A reject for a killed job is dropped.

### TEST-253: Disconnected host

Automated: `t_kill9_disconnected`.

#### Expected Result

- The job is `UNKNOWN`, then `EXIT` at once on `bkill -s kill`.
- The kill does not reach `sbd`: the process runs to its end.
- When `sbd` returns, its finish is acknowledged as a duplicate and
  `sbd` frees the job.

#### Pass Criteria

Documented behaviour: on an unreachable host the process may outlive
the kill.

### TEST-254: bkill -s kill 0

Automated: `t_kill9_all`.

#### Expected Result

Every running and pending job of the user `EXIT`, no process left.

#### Pass Criteria

The bulk kill has the same effect as killing each job.

---

## Replay and retention

### TEST-260: Replay keeps --machines and --tokens

Automated: `t_replay_machines_tokens`.

#### Commands

Submit a pending job with `--machines nodeX --tokens lic=1`, finish
another job, set `LL_MBD_JOB_FINISH_THRESHOLD=1`, restart `mbd` twice.

#### Expected Result

The compacted manifest still has `"nodeX" "lic=1"` in the job's
`JOB_NEW`, and the job is still `PEND`, pinned to `nodeX`.

#### Pass Criteria

Compaction after a restart does not drop placement or tokens.

### TEST-261: Finished jobs kept for the period

Automated: `t_retention`, with `LL_MBD_JOB_FINISH_THRESHOLD=2` and
`LL_MBD_JOB_FINISH_PERIOD=20`. A restart checks the threshold at once,
so the test needs no five minute wait.

#### Commands

Finish two jobs, wait 22 seconds, finish two more, restart `mbd`.

#### Expected Result

- `mbd` logs `compaction finished jobs dropped=2 kept=2`.
- `bjobs --done` lists only the two recent jobs, after the restart.

#### Pass Criteria

Recent finished jobs survive compaction and restart, old ones go.

### TEST-262: Threshold checked every 5 minutes (manual)

#### Expected Result

With the default `MANIFEST_CHECK_TIME` of 300 seconds, `manifest
rebuild` appears in the `mbd` log at most once every five minutes.

#### Pass Criteria

No rewrite every scheduler tick.

---

## Global

### TEST-270: mbd alive, counters consistent

Automated: `t_counters`, run last.

#### Expected Result

`mbd` still running, no `bad counters` and no assertion in its log
after every test above, with `LL_ASSERT_COUNTERS=1`.

#### Pass Criteria

No test leaves the counters inconsistent.
