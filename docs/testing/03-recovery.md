# LavaLite Recovery Tests

Tests for daemon restart and replay correctness. These tests verify that
mbd reconstructs correct state from eventlog after restart, and that sbd
survives restarts without losing running jobs.

Run lifecycle tests first. Recovery tests assume basic job flow works.

---

## Test 1: mbd restart with pending job

```sh
bsub --hold sleep 86400
systemctl restart lavalite-mbd
```

Steps:
1. Submit held job, record job ID
2. Verify job is HELD before restart
3. Restart mbd
4. Verify job is still HELD after restart — `bjobs -a`
5. Verify queue num_held is correct
6. Verify no counter assertion failure in mbd log

---

## Test 2: mbd restart with running job

```sh
bsub sleep 86400
# wait for RUN
systemctl restart lavalite-mbd
```

Steps:
1. Submit job, wait until RUN
2. Restart mbd
3. Verify job is still RUN after restart
4. Verify host and queue counters are correct
5. Let job finish naturally
6. Verify job reaches DONE
7. Verify counters return to baseline

---

## Test 3: mbd restart with suspended job

```sh
bsub sleep 86400
# wait for RUN
bkill --signal stop <jobid>
# verify USUSP
systemctl restart lavalite-mbd
```

Steps:
1. Verify job is USUSP before restart
2. Restart mbd
3. Verify job is still USUSP after restart
4. Verify host and queue counters are correct
5. Resume with `bkill --signal cont <jobid>`
6. Verify job returns to RUN

---

## Test 4: mbd restart with multiple jobs in different states

```sh
bsub --hold sleep 86400          # held
bsub sleep 86400                 # will run
bsub sleep 86400                 # pending if only one slot
systemctl restart lavalite-mbd
```

Steps:
1. Verify state of each job before restart
2. Restart mbd
3. Verify each job restores to its correct state
4. Verify all counters are correct
5. No counter assertion failure

---

## Test 5: sbd restart with running job

```sh
bsub sleep 86400
# wait for RUN
systemctl restart lavalite-sbd
```

Steps:
1. Verify job is RUN before sbd restart
2. Restart sbd
3. Verify sbd reconnects to mbd — `bhosts` shows host ok
4. Verify job state is correct after reconnect
5. Let job finish naturally
6. Verify job reaches DONE
7. Verify counters return to baseline

---

## Test 6: sbd disconnect and reconnect

```sh
bsub sleep 86400
# wait for RUN
systemctl stop lavalite-sbd
```

Steps:
1. Verify host goes unavail — `bhosts`
2. Verify job state transitions correctly (RUN → UNKNOWN expected)
3. Start sbd
4. Verify host returns to ok
5. Verify job state recovers
6. No counter assertion failure

---

## Test 7: mbd restart after compaction

Submit and complete enough jobs to trigger compaction, then restart mbd.

```sh
for i in $(seq 1 20); do bsub sleep 1; done
# wait for all to finish
systemctl restart lavalite-mbd
```

Steps:
1. Verify all jobs complete
2. Verify compaction ran — eventlog size decreased or archive file created
3. Restart mbd
4. Verify mbd starts cleanly
5. Verify `bhist` still shows finished jobs
6. No counter assertion failure

---

## Validation after every recovery test

```sh
bjobs -a
bqueues
bhosts
tail -n 20 $LL_LOG_DIR/mbd.log
```

Check:
- No negative counters
- No assertion failure in mbd log
- Job states match pre-restart state
- Host and queue counters consistent with job list
