# LavaLite bhist Tests

Tests for job history and resource usage accounting. These tests verify
that the usage sidecar is written correctly at job finish and that bhist
displays accurate information.

Run lifecycle tests first. bhist tests require completed jobs.

---

## Test 1: Basic history after job completes

```sh
bsub --name histtest sleep 5
# wait for DONE
bhist
```

Steps:
1. Verify job appears in `bhist` output
2. Verify STAT is DONE
3. Verify SUBMIT_TIME and END_TIME are present and correct
4. Verify EXEC_HOST is correct

---

## Test 2: Long format resource usage

```sh
bsub --name restest --mem 512M sleep 10
# wait for DONE
bhist -l <jobid>
```

Steps:
1. Verify long format shows resource usage section
2. Verify cpu_time > 0
3. Verify mem_mb is present (may be small for a sleep job)
4. Verify swap_mb is present
5. Verify wall time is computed correctly (end_time - dispatch_time)

---

## Test 3: Failed job history

```sh
bsub --name failtest /bin/false
# wait for EXIT
bhist
```

Steps:
1. Verify job appears in `bhist`
2. Verify STAT is EXIT
3. Verify exit status is non-zero in long format

---

## Test 4: History by user

```sh
bhist -u <username>
```

Steps:
1. Verify only jobs for that user are shown
2. Verify job IDs, states, and times are correct

---

## Test 5: History for specific job

```sh
bhist <jobid>
```

Steps:
1. Verify correct job is shown
2. Verify all fields are correct

---

## Test 6: Usage sidecar file

After a job completes, verify the sidecar file was written:

```sh
cat $LL_STATE_DIR/mbd/jobs/<bucket>/<jobid>/usage
```

Expected format:

```
mem_mb=N
swap_mb=N
storage_mb=N
cpu_time=N.NN
```

Steps:
1. Verify file exists
2. Verify all four fields are present
3. Verify values are non-negative
4. Verify cpu_time matches `bhist -l` output

---

## Test 7: History after mbd restart

```sh
bsub sleep 5
# wait for DONE
systemctl restart lavalite-mbd
bhist <jobid>
```

Steps:
1. Verify job still appears in bhist after mbd restart
2. Verify resource usage is unchanged
3. Usage sidecar is on disk — mbd restart does not affect it

---

## Test 8: Multiple finished jobs

```sh
for i in $(seq 1 5); do bsub sleep 2; done
# wait for all to finish
bhist
```

Steps:
1. Verify all 5 jobs appear in bhist
2. Verify all show DONE
3. Verify no duplicates

---

## Notes

- `bhist` reads from `LL_STATE_DIR` directly — requires shared filesystem
  access from the front-end node
- Usage values for `sleep` jobs will show low cpu_time and near-zero memory
  which is correct
- GPU jobs should show GPU usage in future when GPU accounting is added
