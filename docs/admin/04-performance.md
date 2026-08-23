# LavaLite Administrator Guide — Performance

## Overview

This chapter describes the performance characteristics of the core
client commands (`bsub`, `bjobs`, `bhist`) as a function of job table
size, and of bulk array submission as a function of array size.

The 1.0.0 baseline for `bsub` (single-job submission) still applies as
of 1.1.0 — its implementation didn't change. `bjobs` and `bhist` were
re-measured for this release using a clean methodology, described
below, since 1.1.0 replaced `bhist`'s history lookup and the original
1.0.0 numbers didn't have a documented measurement procedure.

Administrators planning large job tables (10,000+ jobs retained in
history) should read this chapter before deployment.

## Manifest

The manifest is the durable event log maintained by `mbd` under
`var/state/mbd`. Every significant job event (submit, dispatch, signal,
finish, and so on) is applied to in-memory state, then appended to the
manifest to keep durable state consistent with it.

The live manifest is the file named `manifest`. When it grows past an
internal size threshold, `mbd` rotates it: the current file is closed
and renamed `manifest.N`, and a new empty `manifest` is opened. Rotated
files (`manifest.1`, `manifest.2`, ...) are immutable archives; they
are never written to again.

`bhist` reconstructs job history by reading the live manifest and every
rotated archive. `mbd` itself only ever appends to the live manifest;
it does not read old archives during normal operation.

## Benchmark Tool

Measurements were collected with `bperf`, a round-trip latency
benchmark included with the source tree:

```text
src/test/perf/bperf
```

`bperf` times individual client calls and reports min, max, avg, p50,
and p99 latency, plus throughput.

## Methodology (1.1.0 bjobs/bhist/array-submit numbers)

Each checkpoint below was measured against a fresh, empty manifest —
`mbd` restarted with no prior history — to isolate manifest size as
the only variable:

1. Restart `mbd` against an empty `var/state/mbd`.
2. Submit the target job count in a single held array:
   `bsub --array 1-N -H sleep 4`. `-H` holds every element pending;
   nothing dispatches, so these numbers reflect read/write path cost
   with scheduling and job execution removed from the picture. Wall
   time of this command is the "array submit" figure below.
3. Run `bperf --bjobs 100 --bhist 100` three times, a few seconds
   apart, no other load on `mbd`.
4. Report the average across the three runs.

This differs from a real production job table in one respect: every
job here only ever logs a single `NEW` manifest event (never
`START`/`FORK`/`FINISH`), so total event volume is lower than N would
imply for jobs that actually ran. The `bhist` numbers below should be
read as "N held/pending jobs," not "N completed jobs" — see
[bhist Latency](#bhist-latency) for a real-dispatch cross-check at
larger scale that confirms this doesn't change the conclusion.

## Job Submission Latency

### Single-job submit (bsub)

`bsub` latency is flat regardless of job table size (1.0.0 baseline,
unchanged in 1.1.0):

| Manifest size | avg    | p99   |
|---------------|--------|-------|
| 1,000         | 5.1ms  | 6.1ms |
| 6,000         | 5.1ms  | 6.3ms |
| 16,000        | 5.2ms  | 6.7ms |

Submission performance does not degrade as the job table grows. No
administrative action is required to keep `bsub` responsive.

### Bulk array submit

`bsub --array 1-N -H` expands the array fully at submit time inside a
single `mbd` request — `job_register()`'s index loop allocates a real
job_id per element before replying. This blocks `mbd`'s single-threaded
event loop for the whole call:

| Array size | submit time | ms/element |
|------------|-------------|------------|
| 1,000      | 0.572s      | 0.572ms    |
| 6,000      | 3.474s      | 0.579ms    |
| 16,000     | 10.650s     | 0.666ms    |

Cost is close to linear in array size, with a slight uptick at 16,000
— plausibly `next_job_id()`'s collision-avoidance scan against a
growing job_id hash. During this window `mbd` cannot service any other
request; size array submissions accordingly, especially against a
busy production `mbd`.

## bjobs Latency

Measured per the methodology above (1.1.0, fresh manifest, held jobs,
3-run average):

| Manifest size | bjobs avg | bjobs p99 |
|---------------|-----------|-----------|
| 1,000         | 21.2ms    | 31.1ms    |
| 6,000         | 105.0ms   | 125.1ms   |
| 16,000        | 268.0ms   | 299.1ms   |

`bjobs` walks `pend_jobs_list` to build its display list, so latency
scales with pending job count — a 16x increase in jobs (1,000→16,000)
produced a 12.6x increase in average latency. This is the same list
implicated in the deferred-to-2.0 scheduling-loop limitation (see the
release notes' Known Limitations); `bjobs`' growth here is a
consequence of walking that list for display, a different code path
than the scheduler tick itself.

## bhist Latency

### Fixed in 1.1.0: O(n²) history lookup

Through 1.0.0, `bhist` matched every manifest event against an
in-memory job table using a linear search, making a full history scan
O(jobs × events). 1.1.0 replaced that lookup with a job_id-keyed hash,
removing the per-event linear scan entirely.

Measured per the methodology above:

| Manifest size | bhist avg | bhist p99 |
|---------------|-----------|-----------|
| 1,000         | 14.1ms    | 20.0ms    |
| 6,000         | 56.1ms    | 63.9ms    |
| 16,000        | 146.4ms   | 160.4ms   |

This fits a linear model closely (`time ≈ 5.7ms + 0.0084ms × job
count`; the model predicts 140ms at 16,000, measured 146ms) — the
O(jobs × events) → O(events) fix removed the quadratic term entirely
across this range, leaving a small fixed cost plus a genuinely linear
one.

`bhist` still scans every rotated archive file in full on each
invocation — there is currently no index to skip directly to the
events for a requested job. Query time therefore still scales with
total event volume, not job count, but no longer with the *square* of
either.

### Large-scale, real-dispatch cross-check

The numbers above use held (never-dispatched) jobs, which only log a
single manifest event each. As a cross-check at a scale and event
volume closer to real production history: on a manifest with roughly
100,000 actually-dispatched jobs (100+ rotated `manifest.N` archives,
full `NEW`/`START`/`FORK`/`FINISH` event chains), a full `bhist`
history query dropped from **over 11 minutes to about 5.4 seconds**
between 1.0.0 and 1.1.0 — roughly a **125x** improvement. Point
lookups (`bhist <job_id>`) improve similarly, since they no longer pay
the O(jobs) matching cost per event either.

## Recommendations

- Job tables under ~5,000 entries perform well with no special
  configuration.
- `bsub` (single-job submit) and job dispatch are unaffected regardless
  of table size.
- Bulk array submission blocks `mbd` for the duration of the call,
  roughly linear in array size (~0.6ms/element observed) — avoid very
  large single array submissions against a busy production `mbd`.
- `bhist` at large scale is substantially faster as of 1.1.0 — the
  O(n²) lookup that motivated avoiding interactive `bhist` on large job
  tables is fixed; that guidance no longer applies.
