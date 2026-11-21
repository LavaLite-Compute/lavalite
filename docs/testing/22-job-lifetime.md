# Job Lifetime Tests

## Purpose

Verify the job lifecycle rule:

- A job lives until the last process in its cgroup exits. The job
  script exiting does not end the job.
- `sbd` never kills job processes on its own. No cleanup kill when the
  script exits, no timeout introduced by this rule.
- The job exit status is the exit status of the job script, also when
  the job ends much later, unless a terminating signal is sent on
  request after the script exited: then it is that signal.
- Every signal, from `bkill` or from `--terminate`, reaches every
  process in the job cgroup, also one that left the process group
  with `setsid`, also after the script exited.

## Prerequisites

- Environment validation completed.
- At least one execution host with cgroup v2 (see
  `90-things-to-know.md`). Check the `sbd` log has no
  `cgroup init failed`.
- Root on the execution host, to look at the job cgroup and restart
  `sbd`.
- A directory visible on the execution host for the job scripts below,
  for example `$HOME/lt`.

The job cgroup is `LL_CGROUP_ROOT/job_<jobid>`, default
`/sys/fs/cgroup/lavalite/job_<jobid>`. Its `cgroup.events` shows what
`sbd` sees:

```sh
cat /sys/fs/cgroup/lavalite/job_<jobid>/cgroup.events
cat /sys/fs/cgroup/lavalite/job_<jobid>/cgroup.procs
```

`populated 1` the job is alive, `populated 0` it can finish.

`bhist` prints the raw wait status: exit code 3 is
`exit_status: 768`.

### Job scripts

```sh
mkdir -p $HOME/lt

cat > $HOME/lt/linger.sh <<'EOF'
#!/bin/sh
sleep 60 &
exit 3
EOF

cat > $HOME/lt/escape.sh <<'EOF'
#!/bin/sh
setsid sleep 3600 &
exit 0
EOF

cat > $HOME/lt/nousr2.sh <<'EOF'
#!/bin/sh
setsid sh -c 'trap "" USR2; sleep 3600' &
exit 0
EOF

chmod 755 $HOME/lt/*.sh
```

## TEST-330: Script exits, child keeps the job running

### Commands

```sh
bsub $HOME/lt/linger.sh
sleep 10
bjobs <jobid>
```

On the execution host:

```sh
cat /sys/fs/cgroup/lavalite/job_<jobid>/cgroup.events
```

Wait about 60 seconds.

```sh
bjobs -a <jobid>
bhist <jobid>
```

### Expected Result

- After 10 seconds the script has exited, the job is still `RUN`,
  `cgroup.events` shows `populated 1`.
- When `sleep` exits the job goes to `EXIT`.
- `bhist` shows `exit_status: 768`, the script's `exit 3`.

### Pass Criteria

The job is `RUN` until `sleep` exits, then `EXIT` with the script exit
status. The end time is about 60 seconds after the start, not 0.

---

## TEST-331: setsid does not escape the job

### Commands

```sh
bsub $HOME/lt/escape.sh
sleep 10
bjobs <jobid>
```

On the execution host:

```sh
cat /sys/fs/cgroup/lavalite/job_<jobid>/cgroup.procs
ps -o pid,pgid,sid,cmd -p <pid from cgroup.procs>
```

### Expected Result

- The job is `RUN`.
- `cgroup.procs` lists the `sleep`. Its pgid and sid are its own, not
  the job's: it left the process group and is still in the cgroup.

### Pass Criteria

The job stays `RUN` while the `sleep` that called `setsid` runs.

Leave the job running for TEST-332.

---

## TEST-332: Stop and resume reach the escaped process

### Commands

With the job of TEST-331:

```sh
bkill --signal stop <jobid>
sleep 3
bjobs <jobid>
```

On the execution host:

```sh
cat /sys/fs/cgroup/lavalite/job_<jobid>/cgroup.events
ps -o pid,stat,cmd -p <sleep pid>
```

Then:

```sh
bkill --signal cont <jobid>
sleep 3
bjobs <jobid>
```

Look at `cgroup.events` and `ps` again.

### Expected Result

- After `stop`: `USUSP`, `frozen 1`, the `sleep` is frozen.
- After `cont`: `RUN`, `frozen 0`.

### Pass Criteria

The freeze reaches a process outside the job process group, after the
script exited.

---

## TEST-333: Other signals reach the escaped process

### Commands

With the job of TEST-331:

```sh
bkill --signal term <jobid>
sleep 5
bjobs -a <jobid>
bhist <jobid>
```

On the execution host:

```sh
pgrep -f 'sleep 3600'
```

### Expected Result

- The `sleep` is gone, the cgroup emptied, the job finished.
- The job is `EXIT`, `bhist` shows `exit_status: 15`: the script
  was already reaped with `exit 0`, the signal sent on request replaces
  its status.
- The `sbd` log shows `cgroup signal 15 sent` and
  `exit_status=0xf from signal`.

### Pass Criteria

SIGTERM goes through the cgroup, not `killpg`, reaches the `setsid`
process, and the job ends `EXIT`.

---

## TEST-334: bkill kill after the script exited

### Commands

```sh
bsub $HOME/lt/escape.sh
sleep 10
bkill --signal kill <jobid>
sleep 5
bjobs -a <jobid>
```

On the execution host:

```sh
pgrep -f 'sleep 3600'
ls /sys/fs/cgroup/lavalite/ | grep job_<jobid>
```

### Expected Result

- The job is `EXIT` at once, `mbd` does not wait for `sbd` on kill.
- No `sleep 3600` left on the host.
- Once the `sbd` log shows `finish_acked and freed` for the job, its
  cgroup is removed.

### Pass Criteria

`cgroup.kill` reaches the escaped process and the cgroup is cleaned
up.

---

## TEST-335: --terminate after the script exited

### Commands

```sh
bsub --terminate $(date -d '+2 min' +%H:%M) $HOME/lt/nousr2.sh
```

Wait until the deadline plus about 70 seconds.

```sh
bjobs -a <jobid>
```

The `sbd` log on the execution host.

### Expected Result

- At the deadline: `terminate_time reached, sending SIGUSR2`. The
  process ignores it, the job stays `RUN`.
- About 60 seconds later: `grace expired, sending SIGKILL`.
- The job finishes.

### Pass Criteria

SIGKILL is sent once after the grace period, through the cgroup, and
reaches the escaped process although the script exited long before.

---

## TEST-336: sbd restart while the job lingers

### Commands

```sh
bsub $HOME/lt/linger.sh
```

After about 10 seconds, the script has exited, restart `sbd` on the
execution host. Then wait about 60 seconds.

```sh
bjobs -a <jobid>
bhist <jobid>
```

### Expected Result

- The job stays `RUN` across the restart until `sleep` exits.
- `bhist` shows `exit_status: 768`: the script status saved at reap
  survives the restart.
- The `sbd` log after the restart does not show `read exit file` nor
  `declare exited` for the job.

### Pass Criteria

Final state `EXIT` with the script's exit status, not the fallback.

---

## TEST-337: Script with no children, unchanged behaviour

### Commands

```sh
bsub /bin/true
bsub /bin/false
sleep 10
bjobs -a <jobid1> <jobid2>
```

### Expected Result

`DONE` and `EXIT` within a few seconds, as before.

### Pass Criteria

A job whose processes all end with the script finishes at once.

## Completion Criteria

The job lifetime test set passes when:

- A job stays `RUN` until the last process in its cgroup exits.
- Its exit status is the script's, also across an `sbd` restart.
- `setsid` does not take a process out of the job.
- stop, cont, term, kill and `--terminate` reach every process of the
  job after the script exited.
- A job without leftover processes finishes as before.
