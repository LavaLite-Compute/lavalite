# LavaLite bhist Tests

Tests for job history and resource usage accounting. These tests verify
that the usage sidecar is written correctly at job finish and that bhist
displays accurate information at all three verbosity levels.

`bhist` is the primary job status and history command in LavaLite. It
covers both live and finished jobs, replacing the need for `bjobs` in
most workflows. Run lifecycle tests first — bhist tests require completed
jobs.

---

## Test 1: Basic history after job completes

```sh
bsub --name histtest sleep 5
# wait for DONE
bhist
```

Steps:
1. Verify job appears in tabular output
2. Verify STAT is DONE
3. Verify NAME column shows `histtest`
4. Verify PROJECT column is present (shows `-` if not set)
5. Verify SUBMIT_TIME and END_TIME are present and correct

---

## Test 2: Long format (-l)

```sh
bsub --name restest --mem 512M sleep 10
# wait for DONE
bhist -l <jobid>
```

Expected output format:
```
Job <N>  User <user>  Queue <queue>  Status <DONE>  Name <restest>  Project <->
  Submitted: <time>  CWD: <path>
  Resources: 1 host(s)  1 cpu(s)/host  0 gpu(s)/host  512 MB mem
  Command:   sleep 10
  Dispatched: <time>  Host: <host>  PID: <pid>  Ended: <time>  Exit: 0
```

Steps:
1. Verify single dense block, no section headers
2. Verify Resources line shows correct mem (512 MB)
3. Verify Command shows `sleep 10`
4. Verify dispatch line shows host, PID, end time, exit status
5. Verify stdin/stdout/stderr lines are absent (default redirects suppressed)

---

## Test 3: Very long format (-ll)

```sh
bhist -ll <jobid>
```

Steps:
1. Verify all fields present: CWD, Command, stdin, stdout, stderr
2. Verify stdin/stdout/stderr show `-` for default redirects
3. Verify Forked time is present
4. Verify Wall time is computed correctly (end_time - dispatch_time)
5. Verify State and Exit status lines present
6. Verify From host and Exec hosts present
7. Verify CPU time, Max memory, Max swap shown if usage was recorded

---

## Test 4: Failed job history

```sh
bsub --name failtest /bin/false
# wait for EXIT
bhist
bhist -l <jobid>
```

Steps:
1. Verify STAT is EXIT in tabular output
2. Verify long format dispatch line shows non-zero Exit status
3. Verify END_TIME is present in tabular output

---

## Test 5: History by user

```sh
bhist -u <username>
```

Steps:
1. Verify only jobs for that user are shown
2. Verify job IDs, states, and times are correct
3. Verify `-u` and job_id are mutually exclusive (error if both given)

---

## Test 6: History for specific job

```sh
bhist <jobid>
bhist -l <jobid>
bhist -ll <jobid>
```

Steps:
1. Verify correct job shown at all three verbosity levels
2. Verify tabular output is one line
3. Verify `-l` output is 5-6 lines
4. Verify `-ll` output includes all fields

---

## Test 7: IO redirects in long format

```sh
bsub --name iotest -o /tmp/out.log -e /tmp/err.log sleep 2
# wait for DONE
bhist -l <jobid>
bhist -ll <jobid>
```

Steps:
1. Verify `-l` shows stdout and stderr lines (non-default redirects present)
2. Verify `-ll` always shows stdin/stdout/stderr regardless

---

## Test 8: Usage sidecar file

After a job completes, verify the usage sidecar was written:

```sh
cat $LL_STATE_DIR/mbd/jobs/<bucket>/<jobid>/usage
```

Expected format:
```
pid=<N>
mem_mb=<N>
swap_mb=<N>
cpu_time=<N.NN>
```

Steps:
1. Verify file exists
2. Verify all four fields are present
3. Verify values are non-negative
4. Verify cpu_time matches `bhist -ll` output

---

## Test 9: History after mbd restart

```sh
bsub sleep 5
# wait for DONE
systemctl restart lavalite-mbd
bhist <jobid>
bhist -ll <jobid>
```

Steps:
1. Verify job still appears in bhist after mbd restart
2. Verify all fields correct — usage sidecar is on disk, unaffected by restart
3. Verify live pending/running jobs also restored correctly

---

## Test 10: Multiple finished jobs

```sh
for i in $(seq 1 5); do bsub sleep 2; done
# wait for all to finish
bhist
```

Steps:
1. Verify all 5 jobs appear in tabular output
2. Verify all show DONE
3. Verify no duplicate rows

---

## Test 11: Compaction and archive overlap

This test verifies that jobs live at compact time appear correctly in
bhist despite being present in multiple archive files.

```sh
# Set low retention to force compact
export LL_JOB_FINISH_RETAIN=2

# Submit 4 jobs, 2 slots available
bsub sleep 60   # job 1 - will run
bsub sleep 60   # job 2 - will run
bsub sleep 5    # job 3 - pending
bsub sleep 5    # job 4 - pending

# Kill running jobs to trigger compact
bkill 1 2
# Wait for compact (jobs 3 and 4 will now run and finish)
# Wait for all done
bhist
bhist -l
```

Steps:
1. Verify all 4 jobs appear in `bhist` output
2. Verify jobs 1 and 2 show EXIT, jobs 3 and 4 show DONE
3. Verify `bhist -l` shows exactly one dispatch block per job — no
   duplicates despite jobs 3 and 4 appearing in two archive files
4. Verify eventlog is compacted (check file size or sequence number)

---

## Notes

- `bhist` reads event archives and sidecars from `LL_STATE_DIR` directly.
  Requires shared filesystem access from the front-end node.
- Three verbosity levels: default tabular, `-l` dense block, `-ll` full detail.
- `-ll` is the only level that always shows stdio redirects and usage stats.
- Usage values for `sleep` jobs show low cpu_time and near-zero memory —
  this is correct.
- A job live at compact time appears in both the chronological archive and
  the compact checkpoint. bhist deduplicates by dispatch_time — no double
  dispatch blocks will appear.
- GPU accounting in bhist is planned for a future release.
- `bjobs` is on the deprecation path. `bhist` covers live and finished jobs
  at all verbosity levels.
