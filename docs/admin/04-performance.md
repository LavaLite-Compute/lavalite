# LavaLite Administrator Guide — Performance

## Overview

This chapter describes the performance characteristics of the core
client commands (`bsub`, `bjobs`, `bhist`) as a function of job table
size, based on benchmark results collected against LavaLite 1.0.0.

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

Example:

```sh
bperf --submit 1000 --bjobs 100 --bhist 100 --queue myqueue
```

## Job Submission Latency

`bsub` latency is flat regardless of job table size:

| Manifest size | avg    | p99   |
|---------------|--------|-------|
| 1,000         | 5.1ms  | 6.1ms |
| 6,000         | 5.1ms  | 6.3ms |
| 16,000        | 5.2ms  | 6.7ms |

Submission performance does not degrade as the job table grows. No
administrative action is required to keep `bsub` responsive.

## bjobs and bhist Latency

`bjobs` and `bhist` latency grows with job table size. Unlike `bsub`,
this growth is not flat:

| Manifest size | bjobs avg | bjobs p99 | bhist avg | bhist p99 |
|---------------|-----------|-----------|-----------|-----------|
| 1,000         | 20.8ms    | 30.4ms    | 11.2ms    | 19.0ms    |
| 6,000         | 102.8ms   | 109.8ms   | 71.8ms    | 105.5ms   |
| 16,000        | 53.3ms    | 62.5ms    | 264.0ms   | 294.0ms   |

`bhist` latency grows faster than the job table itself: roughly a
24x increase in latency for a 16x increase in job count. This is a
known limitation, not expected to scale linearly.

The `bjobs` figure at 16,000 is lower than at 6,000 because `mbd`
rotated the event manifest during the benchmark run, reducing the
number of live (non-historical) jobs held in memory at that point.
It is not a measurement of `bjobs` at a stable 16,000-job table and
should not be compared directly against the 6,000 row.

### Why bhist Slows Down

`bhist` reconstructs job history by scanning every event manifest file
under `var/state/mbd` (the live `manifest` plus any rotated
`manifest.N` archives) and matching each event against an in-memory job
table using a linear search.

Two factors compound the cost as the job table grows:

- Each event lookup is O(n) against the accumulated job table, making
  a full scan O(jobs × events).
- Once `mbd` rotates the manifest, `bhist` must scan every archive in
  full on every invocation; there is currently no index to skip
  directly to the events for a requested job.

A single job_id lookup (`bhist <job_id>`) does not suffer the O(jobs ×
events) matching cost, since the in-memory table never grows past one
entry, but it does still scan every archive file in full.

### Planned Improvement

A hash-indexed lookup (job_id → archive offset, built once per archive
at rotation time) is planned for 1.1 to remove both costs. This is
tracked as a known limitation for 1.0.0, not a regression to be fixed
before release.

## Recommendations

- Job tables under ~5,000 entries perform well with no special
  configuration.
- Sites expecting larger historical job tables should monitor `bhist`
  latency and consider running it less frequently (for example, from
  a periodic reporting job rather than interactively) until the 1.1
  indexed lookup is available.
- `bsub` and job dispatch are unaffected regardless of table size.
