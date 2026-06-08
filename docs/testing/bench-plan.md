# LavaLite Benchmark Plan

## Objective

Measure round-trip latency and throughput for the three primary client
operations (bsub, bjobs, bhist) as a function of job table size and cluster
size.  Results establish a baseline for regression testing and capacity
planning.

## Tool

`bperf` — Python benchmark driver, same style as `chaos`.  Measures
wall-clock round-trip per call: client connect → mbd processing → response.

## Cluster configurations

| Phase | Hosts         | Notes                        |
|-------|---------------|------------------------------|
| 1     | 3 (1+2 sim)   | buntu24 + sim1 + sim2        |
| 2     | TBD           | extend to more physical hosts |

## Job table sizes

0, 1K, 5K, 10K jobs.

Jobs used for pre-fill are `sleep 3600` so they stay in RUN state and do
not drain the table during measurement.

## Measurements per data point

| Command              | Iterations | Notes                        |
|----------------------|------------|------------------------------|
| `bperf --submit`     | 1000       | latency + throughput         |
| `bperf --bjobs`      | 100        | latency + throughput         |
| `bperf --bhist`      | 100        | latency + throughput         |

Reported metrics: min, max, avg, p50, p99, total time, tput (ops/sec).

## Procedure

### 1. Baseline (0 jobs)

```bash
bkill 0                          # drain any leftover jobs
bjobs -a | wc -l                 # verify table is empty
bperf --submit 1000 --bjobs 100 --bhist 100
```

### 2. Pre-fill to target size

```bash
# example: 1000 jobs
for i in $(seq 1 1000); do
    bsub -o /dev/null -e /dev/null sleep 3600
done
bjobs -a | wc -l                 # verify count
```

Repeat for 5K and 10K.

### 3. Measure at each size

```bash
bperf --submit 1000 --bjobs 100 --bhist 100
```

### 4. Teardown between runs

```bash
bkill 0
bjobs -a | wc -l                 # verify drained before next prefill
```

## Results table

Fill in after each run.

### Phase 1 — 3 hosts (buntu24 + sim1 + sim2)

#### submit (1000 iterations)

| Jobs | min (ms) | max (ms) | avg (ms) | p50 (ms) | p99 (ms) | tput (/s) |
|------|----------|----------|----------|----------|----------|-----------|
| 0    | 3.7      | 8.8      | 5.2      | 5.1      | 6.4      | 193.7     |
| 1K   | 3.5      | 11.4     | 5.1      | 5.1      | 6.4      | 194.5     |
| 5K   | 3.7      | 14.2     | 5.2      | 5.1      | 6.5      | 192.4     |
| 10K  | 4.0      | 23.5     | 5.2      | 5.1      | 7.6      | 190.2     |

#### bjobs (100 iterations)

| Jobs | min (ms) | max (ms) | avg (ms) | p50 (ms) | p99 (ms) | tput (/s) |
|------|----------|----------|----------|----------|----------|-----------|
| 0    | 15.5     | 26.3     | 18.9     | 18.8     | 26.3     | 52.8      |
| 1K   | 29.3     | 41.3     | 33.6     | 33.0     | 41.3     | 29.7      |
| 5K   | 84.8     | 120.1    | 93.7     | 92.7     | 120.1    | 10.7      |
| 10K  | 168.4    | 200.6    | 178.9    | 177.6    | 200.6    | 5.6       |

#### bhist (100 iterations)

| Jobs | min (ms) | max (ms) | avg (ms) | p50 (ms) | p99 (ms) | tput (/s) |
|------|----------|----------|----------|----------|----------|-----------|
| 0    | 223.6    | 269.0    | 240.9    | 240.5    | 269.0    | 4.2       |
| 1K   | 316.0    | 350.5    | 332.3    | 333.5    | 350.5    | 3.0       |
| 5K   | 586.9    | 668.2    | 621.4    | 621.9    | 668.2    | 1.6       |
| 10K  | 1833.4   | 2370.4   | 2147.7   | 2222.5   | 2370.4   | 0.5       |

## Notes

- All measurements taken with no concurrent load unless stated otherwise.
- mbd RSS recorded before and after each run (`ps -o rss= -p $(pgrep -x mbd)`).
- Extend Phase 2 table by copying Phase 1 table and updating host count.

## Observations — Phase 1

**submit** — flat across all table sizes. avg 5.2ms, p99 stable at 6.4-7.6ms,
tput ~192/s. Submit cost is independent of table size. Dominated by TCP
round-trip + XDR encode/decode.

**bjobs** — linear with table size. ~18ms per 1K jobs of overhead. 178ms at
10K is acceptable for an interactive query returning the full table.

**bhist** — non-linear at 10K. 0→5K grows ~95ms/1K jobs (linear), but 5K→10K
jumps from 621ms to 2147ms. Event log read from disk does not scale linearly —
likely due to log file size growth. Needs investigation before extending to
larger clusters.
