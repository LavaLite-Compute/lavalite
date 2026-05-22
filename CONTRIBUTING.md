# LavaLite Coding Guide and Project Invariants

This document defines the C coding style and architectural invariants for LavaLite.
It serves as coding guide, design reference, and onboarding document.

All new code must follow this guide.

---

## Philosophy

LavaLite prioritizes clarity, explicitness, and correctness over cleverness.

- correct and fast
- explicit over implicit
- boring over clever
- readable over compact

When in doubt, delete or simplify. If it needs a comment to explain what it
does, rewrite it.

> Consistency beats ingenuity. Readable code ships; clever code breaks.

---

## Architecture and layering

Three components:

- **mbd** — master batch daemon. Job scheduling, job lifecycle, cluster state.
- **sbd** — slave batch daemon. Job execution, cgroup management, status reporting.
- **commands and API** — user commands and the `llbatch` C API.

Two libraries:

- **libllbase** (`src/base/lib/`) — base helpers: channels, config, wire protocol, lists. Prefix: `ll_*`.
- **libllbat** (`src/batch/lib/`) — batch layer: API, log, submit. Prefix: `llb_*`.

Layer model:

```
wire_*  <-->  ll_*  <-->  mbd / sbd
         ^          ^
       chan_*     external API: ll_* / llb_*
```

Nothing jumps layers. Wire code has no scheduling logic. Daemons do no
direct socket I/O. The channel subsystem owns all network I/O.

---

## Key invariants

**No job enters RUN state without an active sbd connection.**
Before dispatch, verify the target host has a connected sbd channel.
If not, return the job to pending.

**State changes are explicit assignments.**
No state is inferred or derived. `POST_DONE` is forbidden — use explicit
`JOB_DONE` or `JOB_EXITED`.

**Durable state before in-memory state.**
All job events are written to the event log before any in-memory change.
Recovery replays the log. What is on disk is what the scheduler knows.

**Never trust remote-supplied hostnames.**
Always use the canonical host resolved at connection time.

**Authentication is mandatory.**
All mbd/sbd communication is HMAC-SHA256 authenticated. No message is
processed without a valid signature.

---

## C coding style

**Formatting**: use `clang-format` before every commit. A `.clang-format`
file is at the repo root. Never hand-format.

**Indentation**: 4 spaces, no tabs. 80-column soft limit.

**Braces**: always use braces, even for single-statement bodies.

**Variables**: one per line, declared close to first use (C99). No pedantic
column alignment.

**Pointers**: asterisk binds to the variable: `char *p`, `struct foo *f`.

**Booleans**: use `bool_t` from `<rpc/types.h>`. Lowercase `true`/`false`.
Do not use `<stdbool.h>`.

**Integers**: use `stdint.h` types (`int32_t`, `uint64_t`, etc.).

**Buffer sizes**: use `LL_BUFSIZ_32`, `LL_BUFSIZ_64`, `LL_BUFSIZ_1K` etc.
from the base library. No magic numbers for buffer sizes.

**Macros**: avoid unless structurally necessary (bit flags, protocol values).
Macros must not hide logic or contain control flow. Prefer `enum` or
`static const`.

**Ternary operators**: avoid. Use explicit `if`/`else`. Allowed only for
trivial, side-effect-free expressions where intent is obvious.

**Control flow**: use `continue` and early `return` to reduce nesting.
Avoid deep `if`/`else` chains.

**Functions**: keep short. Internal functions must be `static`. Functions
that take parameters must be genuinely reusable — no hidden global
dependencies.

**Module state**: store in `static` variables, private to the translation
unit. The translation unit is the namespace.

**Header prototypes**: omit parameter names: `int ll_queue_init(struct ll_queue *);`

**Include order**: system headers, blank line, own header, other project headers.

**Error handling**: use `LS_ERR` and `LS_ERRX` macros. Do not embed
`__func__` or `%m` manually — the macros include them. Set `errno`
appropriately before returning errors.

**Comments**: document invariants, design decisions, non-obvious constraints,
and counterintuitive behavior. Do not restate control flow.

**Naming**: `mbd_*` for mbd internals, `sbd_*` for sbd internals, `ll_*`
for base library, `llb_*` for batch library.

---

## Provenance

LavaLite is a clean reimplementation. Do not copy proprietary code.
Rewrite legacy behavior rather than porting it.

---

## The short version

Write code that is boring, readable, correct, and fast.
