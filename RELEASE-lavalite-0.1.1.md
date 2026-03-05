# LavaLite 0.1.1 Release Notes

**Tag:** `lavalite-0.1.1`
**Previous release:** `lavacore-0.1.0`
**Status:** Developer Preview (Alpha)

LavaLite is a modern, open-source workload scheduler for HTC/HPC environments.
It is built around a deterministic architecture: explicit job states, well-defined
state transitions, durable on-disk state, and predictable recovery after restart.

This release stabilizes job lifecycle management, adds log compaction, and
hardens the mbd/sbd protocol against restart and failure scenarios. The project
was renamed from LavaCore to LavaLite.

---

## Project rename

The project name reverts to **LavaLite**. Binaries, paths, and configuration
files use the LavaLite naming. The previous tag `lavacore-0.1.0` remains valid
as a baseline reference.

---

## New: log compaction (`mbd_compact`)

`mbd_compact` is a new maintenance process that compacts `lsb.events` by
removing records for jobs that have finished and aged past `clean_period`
(default: 1 hour).

- Runs as a child of mbd, communicates via Unix socketpair registered in the
  epoll channel layer
- Two-pass algorithm: first pass identifies expired jobs, second pass writes
  survivors to a temp file, then renames atomically
- mbd receives `BATCH_COMPACT_DONE`, reopens its `lsb.events` fd, replies with
  `BATCH_COMPACT_ACK`
- Compactor monitors mbd liveness and exits cleanly when mbd terminates
- Triggered by file size threshold; mbd frees finished jobs from memory at
  compact time using the same age policy applied to the log

---

## New: sbd job storage compaction

On `finish_ack`, sbd now removes finished jobs from memory and cleans up the
job directory. The archive pruner runs asynchronously with a rate limit to
avoid bursty I/O.

---

## mbd stability

Multiple memory corruption and crash fixes. mbd is significantly more stable
under sustained workloads and across restart scenarios.

- Fixed stdout/stderr handling: jobs no longer produce spurious warnings when
  `-o`/`-e` are not specified
- Fixed `bjobs` crashes under sustained job throughput
- Fixed job loss on host unreachable events and execution resource leaks
- Integrated cleanup into mbd shutdown path

---

## sbd stability

- Fixed job corruption on sbd restart
- Fixed stdout/stderr path when `-o`/`-e` not specified in `bsub`
- Decoupled job lifetime from systemd: job exit detection is now reliable
  across sbd restarts
- Improved mbd/sbd connection handling on sbd restart
- Aligned archive directory permissions with production layout

---

## mbd/sbd protocol

- Initial host state is now `HOST_STAT_UNREACH` until sbd connects and registers
- Added `RUN→UNKNOWN` transition when sbd becomes unreachable mid-job
- Added explicit job resync protocol on sbd reconnect
- Fixed dispatch failure handling: job correctly returns to pending queue,
  counters updated, ACK always sent
- Fixed fork() failure path and sbd cleanup corruption

---

## bhist / bacct

- `bhist` now scans current `lsb.events` before rotated files, fixing slow
  lookups for recently finished jobs
- Fixed duplicated history entries for recycled job IDs
- `bhist` and `bacct` are now installed binaries, config-driven

---

## Build / infrastructure

- Fixed typo in `lsf.shared` configuration template
- Documentation reorganized and structure established
- Fixed systemd integration: set KillMode=process to prevent systemd from
  killing running jobs when sbd is restarted

---

## Testing

- Chaos testing harness introduced: injects mbd/sbd restarts and verifies
  job completion under failure conditions
- `mbd_compact` test suite added
