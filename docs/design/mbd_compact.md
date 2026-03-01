# mbd_compact — Design Decisions

This document records **why**, not what. The code describes what.
Decisions recorded here are stable; implementation details are not.

---

## Why a separate process and not in-process

mbd is a single-thread epoll loop. It must never block. Compacting lsb.events
is an O(n) file operation — read all records, write survivors, rename. On a
busy cluster lsb.events can be large. Doing this inside mbd would stall
scheduling for the duration.

A separate process keeps mbd's epoll loop unaffected. The compact runs at its
own pace, mbd never waits for it.

## Why a permanent child and not fork-per-compact

Forking on demand from a large process is expensive even with COW. A permanent
child pays the fork cost once at startup. It also owns its own state — threshold
tracking, retry logic, sleep interval — without burdening mbd.

The child is tied to mbd's lifetime: it is launched at mbd startup and exits
when mbd exits. No external process manager needed.

## Why TCP client and not socketpair or pipe

LavaLite's inter-process communication is already built on TCP channels with
`call_mbd()`, XDR encoding, and `packet_header`. Using the same mechanism for
`mbd_compact` means no new IPC primitive, no new framing, no new error handling
path. `mbd_compact` connects, sends `BATCH_COMPACT_DONE`, receives
`BATCH_COMPACT_ACK`, disconnects. One connection per compact cycle — same
pattern as `bkill`.

A socketpair would have required registering a raw fd in mbd's epoll loop
outside the chan layer, breaking the channel abstraction. Pipes have the same
problem. TCP through the existing chan layer costs nothing architecturally.

## Why call_mbd and not epoll-driven in mbd_compact

`mbd_compact` is not a daemon. It does one thing: compact, then notify. It has
no other events to handle, no clients to serve. Making it epoll-driven would be
complexity for its own sake. A blocking `call_mbd()` is correct and honest.

## Why rename(vapor) and not in-place rewrite

`rename()` is atomic at the filesystem level. Either the old lsb.events is
visible or the new one is — never a partial file. If `mbd_compact` crashes
mid-write, `lsb.events.vapor` is left behind and lsb.events is untouched. mbd
continues appending to the original file without data loss.

In-place rewrite has no such guarantee. A crash mid-truncation corrupts the
file.

## Why mbd reopens lsb.events and not mbd_compact

mbd owns lsb.events. It has the file open for append. After `rename()`,
mbd's fd still points to the old inode. Only mbd can close and reopen its own
fd at the right moment — after the rename is complete and confirmed via
`BATCH_COMPACT_ACK`. `mbd_compact` has no business touching mbd's file
descriptors.

## Why BATCH_COMPACT_DONE/ACK in the existing enum

LavaLite uses a single flat `mbdReqType` enum for all operations. A separate
enum would collide with existing operation numbers unless the type were changed
everywhere. Adding to the existing enum is the safe, zero-surprise choice. The
comment in `daemonout.h` already warns about this.

## Why compactor/ is its own directory

`mbd_compact` is not a user command — it does not belong in `cmd/`. It is not
a daemon — it does not belong in `daemons/`. It is a maintenance tool with its
own lifecycle and its own `main()`. A dedicated directory makes the boundary
explicit and leaves room for `lsb.acct` compaction when that comes.

## Why the threshold is size-based and not count-based

`stat()` is one syscall. Counting finished jobs older than `clean_period` would
require scanning the in-memory job list or reading lsb.events — both more
expensive and both requiring coordination with mbd. Size is a good enough proxy:
a large file means many records, and the compact will remove the stale ones.

## Why clean_period is not configurable in this release

The LSF `lsf.conf` parser from 1993 is a known balrog. Adding a new parameter
to it for a feature that defaults correctly is not worth the fight in 0.1.1.
The default (1 hour) is correct for the target workload. When the parser is
replaced, `COMPACT_CLEAN_PERIOD` becomes a proper `lsf.conf` parameter.

## What was deliberately not done

- No SIGHUP to mbd. Signal handlers in a complex daemon are a source of
  subtle races. The TCP notification is synchronous with the epoll loop.

- No shared memory or mmap between mbd and mbd_compact. No shared state is
  needed — mbd_compact reads lsb.events from disk, mbd writes it. The rename
  is the synchronization point.

- No index file. LSF maintained a job index file for bhist performance.
  For 0.1.1, bhist reads lsb.events and lsb.acct sequentially. An index
  is a future optimization, not a correctness requirement.
