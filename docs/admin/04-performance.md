# LavaLite Administrator Guide — Performance

## Overview

This chapter describes the performance of the core client commands
(`bsub`, `bjobs`, `bhist`) as a function of job table size, and of
bulk array submission as a function of array size.

The numbers below were measured for 2.0.0 with the same method as for
1.1.0, and are within noise of the 1.1.0 numbers: 2.0.0 has no
performance regression. `bhist` is slightly faster.

Administrators planning large job tables (10,000+ jobs retained in
history) should read this chapter before deployment.

## Manifest

The manifest is the durable event log maintained by `mbd` under
`var/state/mbd`. Every significant job event (submit, dispatch, signal,
finish, and so on) is applied to in-memory state, then appended to the
manifest to keep durable state consistent with it.

The live manifest is the file named `manifest`. When
`LL_MBD_JOB_FINISH_THRESHOLD` finished jobs are older than
`LL_MBD_JOB_FINISH_PERIOD`, `mbd` compacts it: the live jobs and the
recent finished ones are written to a new `manifest` and the old file
is renamed `manifest.N`. Rotated files (`manifest.1`, `manifest.2`,
...) are immutable archives; they are never written to again, and
never removed by LavaLite: see Manifest Archives in the operations
guide.

`bhist` reconstructs job history by reading every rotated archive,
oldest first, then the live manifest. `mbd` itself only ever appends
to the live manifest; it does not read old archives during normal
operation.

## Benchmark Tool

Measurements were collected with `bperf`, a round-trip latency
benchmark included with the source tree:

```text
src/tests/perf/bperf
```

`bperf` times individual client calls and reports min, max, avg, p50,
and p99 latency, plus throughput. `src/tests/perf/perf20.sh` runs the
whole method below.

## Methodology

Each size below was measured against an empty `mbd`, to isolate the
job table size as the only variable:

1. Kill every job of the measuring user and wait until `bjobs` shows
   none, so no `sbd` has a job left to report.
2. Stop `mbd`, remove the content of `var/state/mbd`, start `mbd`.
3. Submit the target job count in a single held array:
   `bsub --array 1-N -H sleep 4`. `-H` holds every element pending;
   nothing dispatches, so these numbers reflect read/write path cost
   with scheduling and job execution removed from the picture. Wall
   time of this command is the "array submit" figure below.
4. Run `bperf --bjobs 100 --bhist 100` three times, a few seconds
   apart, no other load on `mbd`. Report the average across the three
   runs.
5. Run `bperf --submit 100` for the single-job submit figure.

`ll.conf` for the run: `LL_LOG_MASK=LOG_INFO`,
`LL_ARRAY_MAX_SIZE=16000`, no `LL_ASSERT_COUNTERS`. Debug logging and
the counter checks cost a lot at these sizes and would be measured
instead of the commands.

This differs from a real production job table in one respect: every
job here only ever logs a single `NEW` manifest event (never
`START`/`FORK`/`FINISH`), so total event volume is lower than N would
imply for jobs that actually ran. The `bhist` numbers below should be
read as "N held/pending jobs," not "N completed jobs" — see
[bhist Latency](#bhist-latency) for a real-dispatch cross-check at
larger scale.

## Job Submission Latency

### Single-job submit (bsub)

`bsub` latency is flat regardless of job table size. Throughput is
one client submitting 100 jobs back to back, each `bsub` a new
process:

| Job table size | avg   | p99   | throughput |
|----------------|-------|-------|------------|
| 1,000          | 5.3ms | 7.8ms | 188/s      |
| 6,000          | 5.4ms | 6.6ms | 185/s      |
| 16,000         | 5.8ms | 40ms  | 172/s      |

The 16,000 p99 is a single outlier, the minimum and median are the
same as at the other sizes. Submission performance does not degrade
as the job table grows. Several clients submitting at the same time
were not measured.

### Bulk array submit

`bsub --array 1-N -H` expands the array fully at submit time inside a
single `mbd` request — `job_register()`'s index loop allocates a real
job_id per element before replying. This blocks `mbd`'s single-threaded
event loop for the whole call:

| Array size | submit time | ms/element | jobs/s |
|------------|-------------|------------|--------|
| 1,000      | 0.621s      | 0.621ms    | 1,610  |
| 6,000      | 3.385s      | 0.564ms    | 1,772  |
| 16,000     | 9.512s      | 0.594ms    | 1,682  |

An array is one request, so it submits about ten times faster per
job than single `bsub` calls. Cost is close to linear in array size. During this window `mbd`
cannot service any other request: a client that connects meanwhile
can time out. Size array submissions accordingly, especially against
a busy production `mbd`. `LL_ARRAY_MAX_SIZE` caps the array size,
1,000 by default.

## bjobs Latency

| Job table size | bjobs avg | bjobs p99 |
|----------------|-----------|-----------|
| 1,000          | 20.6ms    | 28.0ms    |
| 6,000          | 105.1ms   | 115.0ms   |
| 16,000         | 271.3ms   | 294.7ms   |

`bjobs -a` returns every job of the table, so latency scales with the
job count: a 16x increase in jobs (1,000→16,000) produced a 13x
increase in average latency.

Each job is sent as a fixed size record of about 4.9 KB, mostly room
for the run host list, built in `mbd` and decoded in the client: at
16,000 jobs a `bjobs -a` handles about 78 MB.

## bhist Latency

| Job table size | bhist avg | bhist p99 |
|----------------|-----------|-----------|
| 1,000          | 9.9ms     | 15.8ms    |
| 6,000          | 51.3ms    | 63.2ms    |
| 16,000         | 131.6ms   | 153.6ms   |

This fits a linear model closely (`time ≈ 1.8ms + 0.0081ms × job
count`; the model predicts 50.5ms at 6,000, measured 51.3ms).

`bhist` scans every rotated archive file in full on each invocation —
there is no index to skip directly to the events of a requested job.
Query time therefore scales with total event volume. Removing old
archives keeps it short.

### Large-scale, real-dispatch cross-check

The numbers above use held (never-dispatched) jobs, which only log a
single manifest event each. As a cross-check at a scale and event
volume closer to real production history: on a manifest with roughly
100,000 actually-dispatched jobs (100+ rotated `manifest.N` archives,
full `NEW`/`START`/`FORK`/`FINISH` event chains), a full `bhist`
history query dropped from **over 11 minutes to about 5.4 seconds**
between 1.0.0 and 1.1.0, when an O(jobs × events) lookup was replaced
by a job_id-keyed hash — roughly a **125x** improvement.

## Recommendations

- Job tables under ~5,000 entries perform well with no special
  configuration.
- `bsub` (single-job submit) and job dispatch are unaffected regardless
  of table size.
- Bulk array submission blocks `mbd` for the duration of the call,
  roughly linear in array size (~0.6ms/element) — avoid very large
  single array submissions against a busy production `mbd`.
- `bhist` time grows with the archives it reads: prune old
  `manifest.N` archives.
