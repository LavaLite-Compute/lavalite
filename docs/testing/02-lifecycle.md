# LavaLite Lifecycle Tests

Tests for job state transitions. Each test covers a specific path through
the job state machine. Verify counters and eventlog at each step.

Requires mbd and sbd running unless noted.

---

## Test 1: Submit, run, finish normally

The basic path. Everything else builds on this.

```sh
bsub sleep 5
```

Steps:
1. Verify job enters RUN — `bjobs -a`, `bhosts`, `bqueues`
2. Wait for completion
3. Verify job reaches DONE
4. Verify host and queue counters return to baseline
5. Check eventlog: JOB_NEW → JOB_START → JOB_FINISH

---

## Test 2: Submit, run, fail

```sh
bsub /bin/false
```

Steps:
1. Verify job enters RUN
2. Verify job reaches EXIT (non-zero exit status)
3. Verify counters return to baseline
4. Check eventlog: JOB_NEW → JOB_START → JOB_FINISH

---

## Test 3: Submit pending (sbd down)

Stop sbd before submitting.

```sh
systemctl stop lavalite-sbd
bsub sleep 86400
```

Steps:
1. Verify host state is unavail — `bhosts`
2. Verify job is PEND — `bjobs -a`
3. Verify queue num_pend increases
4. Start sbd
5. Verify job dispatches and enters RUN

---

## Test 4: Kill pending job

With sbd down:

```sh
bsub sleep 86400
bkill <jobid>
```

Steps:
1. Verify job is PEND
2. Kill it
3. Verify job is gone from active list
4. Verify queue counters return to baseline
5. Check eventlog: JOB_NEW → JOB_FINISH

---

## Test 5: Submit held job

```sh
bsub --hold sleep 86400
```

Steps:
1. Verify job state is HELD — `bjobs -a`
2. Verify queue num_held increases
3. Verify job does not dispatch even with sbd running
4. Release with `bkill --signal cont <jobid>`
5. Verify job transitions to PEND then RUN

---

## Test 6: Kill held job

```sh
bsub --hold sleep 86400
bkill <jobid>
```

Steps:
1. Verify job is HELD
2. Kill it
3. Verify job reaches EXIT
4. Verify queue counters return to baseline

---

## Test 7: Suspend running job

```sh
bsub sleep 86400
# wait for RUN
bkill --signal stop <jobid>
```

Steps:
1. Verify job enters RUN
2. Send STOP
3. Verify job state is SUSP — `bjobs -a`
4. Verify host num_susp increases, num_run decreases
5. Verify queue counters update correctly

---

## Test 8: Resume suspended job

Continuing from Test 7:

```sh
bkill --signal cont <jobid>
```

Steps:
1. Verify job returns to RUN
2. Verify counters update correctly
3. Let job finish, verify DONE

---

## Test 9: Kill suspended job

```sh
bsub sleep 86400
# wait for RUN
bkill --signal stop <jobid>
# verify USUSP
bkill --signal kill <jobid>
```

Steps:
1. Verify job was USUSP
2. Verify job reaches EXIT
3. Verify host and queue counters return to baseline

---

## Test 10: Multiple jobs, queue ordering

```sh
bsub --name job1 sleep 86400
bsub --name job2 sleep 86400
bsub --name job3 sleep 86400
```

Steps:
1. Verify all three jobs dispatch if resources allow
2. If only one slot available, verify FIFO order — job1 runs first
3. Kill all jobs, verify counters return to baseline

---

## eventlog reference

Expected event sequences:

Normal completion:
```
JOB_NEW → JOB_START → JOB_FINISH
```

Killed pending:
```
JOB_NEW → JOB_FINISH
```

Suspended running:
```
JOB_NEW → JOB_START → JOB_SIGNAL
```

Killed suspended:
```
JOB_NEW → JOB_START → JOB_SIGNAL → JOB_FINISH
```
