# LavaLite C Coding & Project Invariants Guide

This document defines the **official C coding style** and **architectural invariants** for the LavaLite project.

It serves as:
- a coding guide
- a design reference
- an onboarding document
- a consistency contract across the codebase

All new code **must** follow this guide. Pull requests that diverge may be rejected or asked to revise before merge.

---

# 0. Philosophy

> If it needs more than a Unix syscall, a struct, and libc — rethink it.

> Accept what the hardware gives you. Make it fast, make it clear, make it stop. Everything else is noise.

> Stability isn't the absence of problems — it's the ability to recover gracefully.

> Simplicity and clarity over cleverness; minimal code, maximal intent.

> Mathematicians chase proofs; computer scientists chase models; practitioners chase uptime.

> Explicit. Verbose. Clear. Implicit nothing.

> Reuse the simple primitives that already work.

> True man reads the man page. That's why it's called man.

> Fossil in → LavaLite out.

> LavaLite Principle #7: If you don't handle errors, errors will handle you.

LavaLite prioritizes:

- clarity
- explicitness
- readability
- testability
- scheduling correctness

We avoid:

- legacy OpenLava/LSF quirks
- magic macros
- implicit behavior
- clever but opaque constructs

> Consistency beats ingenuity.
> Readable code ships; clever code breaks.

When in doubt, ask what Unix and K&R would do — and do that.

---

## C Modules and State

Each C translation unit (`.c` file) is treated as a self-contained module.

Module state is stored in `static` variables and is private to the translation unit.
Functions operating on module state are considered module "methods" and **must not**
pretend to be pure or reusable.

As a rule:
- functions that operate on module state must take `void` and use the module globals directly
- functions that take parameters must be genuinely reusable and must not rely on module globals

This avoids "fake functional purity": functions that appear pure or parameter-driven
but secretly depend on global state or perform irreversible side effects
(logging, I/O, process termination).

The translation unit itself acts as the namespace.
Explicit namespacing or artificial abstraction is avoided in favor of clarity,
explicit lifecycle, and honest semantics.

---

# 1. Project Architectural Invariants

These invariants define the **design boundaries** of LavaLite. Code that violates these rules is considered incorrect even if it compiles.

## 1.1 Layering and boundaries

| Layer | Responsibility | Must NOT do |
|--------|---------------|-------------|
| wire_* | protocol structs, XDR encode/decode | scheduling logic |
| chan_* | networking channel: fd mgmt, epoll, buffered writes | scheduling logic, job selection |
| ll_*   | helper library, translation between wire and internal structs | socket I/O, XDR |
| Daemons (mbatchd, lim, sbatchd) | scheduling, job lifecycle, mastership | write(), send(), direct socket I/O |

Layering rule:

```
wire_*   <-->   ll_*   <-->   mbatchd / lim / sbatchd
          ^           ^
        chan_*     external API: ls_* / lsb_*
```

Nothing jumps layers.

---

## 1.2 Channels and message sending

Daemons **never call write(), send(), or chan_write()** directly.

All outbound messaging must use:

```c
struct chan_buf *buf = chan_alloc_buf(ch, len);
memcpy(buf->data, payload, len);
chan_enqueue(ch, buf);
```

Actual network I/O happens only inside the channel subsystem:

```
chan_epoll() → dowrite()
```

where backpressure, partial writes, disconnects, and retries are handled.

| Allowed in daemons | Forbidden |
|-------------------|----------|
| chan_alloc_buf() | write() |
| chan_enqueue() | send() |
| memcpy(payload) | chan_write() |
| async send via epoll | blocking I/O |

---

## 1.3 Scheduling invariants (start_job)

A job **must not start** unless the SBD for the target node is fully connected:

```c
host_node->sbd_node != NULL
host_node->sbd_node->chanfd != -1
```

If violated:

```c
LS_ERRX("sbd on node %s is not connected, cannot schedule", host_node->host);
return ERR_UNREACH_SBD;
```

On SBD disconnect:
- free mbd_client_node / sbd node
- set `hData->sbd_node = NULL`
- scheduler must mark the node as non-runnable

**Hard invariant:** no job enters RUN state without an active SBD.

---

## 1.4 Host identity and trust model

The canonical hostname is determined at socket accept time:

```c
client->host = canonical_resolved_host;
```

Rules:
- never trust remote-supplied hostnames
- scheduler and lookups always use canonical host
- identity and cluster consistency depend on canonicalization, not request payloads

---

## 1.5 Error handling invariant

Use `lserrno` and set it before returning on errors.
Do not use deprecated `cherrno`.

```c
if (host == NULL) {
    lserrno = LLE_NO_HOST;
    return -1;
}
```

Functions must either return -1 and set `lserrno`, or return a well-defined project error code. Mixing styles is forbidden.

### Logging and error reporting (LS_XXX macros)

All logging **must** use the `LS_XXX` macros (`LS_ERR`, `LS_ERRX`, `LS_WARNING`, `LS_INFO`, `LS_DEBUG`).

| Macro | Includes `%m` (errno) | Use when |
|-------|----------------------|----------|
| `LS_ERR` | yes | syscall failed, errno is meaningful |
| `LS_ERRX` | no | logic error, no errno involved |
| `LS_WARNING` | yes | non-fatal, errno meaningful |
| `LS_INFO` | no | informational |
| `LS_DEBUG` | no | debug |

These macros already include `__func__` automatically.

- **Do not** embed `__func__` in log messages
- **Do not** append `%m` or `strerror(errno)` manually
- Log messages must contain **only semantic information**

Correct:
```c
LS_ERRX("sbd not connected host=%s job=%s", host->host, job_id);
```

Incorrect:
```c
LS_ERR("%s: sbd not connected host=%s job=%s %m", __func__, host->host, job_id);
```

---

## 1.6 Authentication & request identity (`eauth`)

LavaLite provides a **signed authentication token system** for user identity enforcement.
Implemented and tested, but not yet integrated into request processing.

Identity must be cryptographically provable, not topologically assumed.
Trusting a request because it originates from inside the cluster network or from a known host is insufficient.

A token contains:
- user, uid, gid
- host of origin
- timestamp
- nonce (anti-replay)

A detached signature covers the token for integrity.

**Current boundary:** inter-daemon communication relies on host identity and verified cluster node membership as defined in `lsb.hosts` until token-based daemon authentication is enabled.

**Future invariant:** no scheduling or job operation will run without user identity validated via eauth.

---

## 1.7 Job State Machine Invariant

The LavaLite job state machine must be **explicit, minimal, and mechanically verifiable**.

- Job states are explicit.
- State transitions are explicit.
- No state is inferred.
- No state is derived indirectly.
- No helper may "compute" a state transition implicitly.

State changes must be written explicitly:

```c
job->jStatus = RUN;
```

with the surrounding logic making the reason unambiguous.

A job state change must always correspond to a real system event:
- submission, scheduling, SBD acknowledgment, signal delivery, process exit, resource update, explicit user action.

Adding a new state requires a documented justification, explicit transition rules, defined entry and exit conditions. If a state cannot be described in one sentence, it does not belong in the system.

### No `POST_DONE` (ever)

`POST_DONE` is **forbidden**. Zero semantic, structural, or behavioral traces anywhere in the pipeline. Any logic previously relying on `POST_DONE` must use explicit `DONE` / `EXIT` states with clear transitions.

---

# 2. C Coding Style

LavaLite follows a strict `.clang-format` based style.
**Do not hand-format code. Always format before commit.**

```
clang-format -i src/file.c
find . -name '*.[ch]' -exec clang-format -i {} +
```

---

## 2.1 Indentation & layout

- indent: 4 spaces
- no tabs
- soft limit: 80 columns
- always use braces
- one statement per line, always

Correct:
```c
if (x) {
    return 1;
}
```

Incorrect:
```c
if (x) return 1;
if (x)
    return 1;  /* no braces */
```

---

## 2.2 Functions and prototypes

In `.c` files:
```c
int ll_queue_init(struct ll_queue *q)
{
    ...
}
```

In header files — types only, no parameter names:
```c
int ll_queue_init(struct ll_queue *);
```

---

## 2.3 Pointer style

```c
char *name;
struct hData *host;
```

The asterisk binds to the variable, not the type.

---

## 2.4 Variable declarations

Declare one variable per line, close to first use (C99):

```c
int count = 0;
char *path = NULL;
```

Not in a block at the top of the function.

---

## 2.5 Boolean type

Use `bool` from `<stdbool.h>`.
Use lowercase literals `true` and `false`.

Do not use:
- `bool_t` from `<rpc/types.h>`
- uppercase `TRUE` / `FALSE`

---

## 2.6 Include order

```c
#include <system_header.h>

#include "own_header.h"
#include "other_project_header.h"
```

Each header includes only what it needs. No umbrella includes.
`SortIncludes: false` in `.clang-format` preserves ordering intentionally.

---

## 2.7 Naming conventions

Exported functions follow daemon-specific prefixes:

- `mbd_*` — exported by mbatchd
- `sbd_*` — exported by sbatchd
- `lim_*` — exported by lim
- `ll_*`  — exported by the base library

Internal `static` helpers use concise, context-local names without prefix.

---

## 2.8 No ternary operators

Ternaries hide logic. Use explicit `if`/`else`.

Incorrect:
```c
const char *name = cluster.name ? cluster.name : "(none)";
```

Correct:
```c
const char *name;
if (cluster.name != NULL) {
    name = cluster.name;
} else {
    name = "(none)";
}
```

---

## 2.9 No complex macros

Prefer `enum` or `static const` over `#define` for constants.
Avoid function-like macros. If you need one, document why no function works.

---

# 3. Comments

Code must be readable without comments.

Comments are allowed **only** to document:
- invariants not visible from the local code
- design decisions and rationale ("why not X", not "what")
- non-obvious constraints or intentional limitations
- counterintuitive behavior that must not be "cleaned up"

If a comment can be removed without losing essential information, remove it.

Comments must describe **rules**, **contracts**, or **assumptions** — not control flow.

> VP-level engineer or not, we all leave a little time capsule of "WTF" behind.
> Document the why. The what rots with the code.

---

# 4. Miscellaneous rules

- internal functions must be `static`
- functions must be reasonably short — if it does not fit on a screen, split it
- do not reinvent memory or logging subsystems
- no `NULL` placeholder entries in parameter tables — fix the code that depends on them
- use `MAXHOSTNAMELEN` from `<sys/param.h>`, not project-defined hostname limits
- load index count: `enum { LIM_NLOAD_INDX = 11 };` in `lim.h` — not a global, not a define

---

# 5. Provenance & legal considerations

To maintain independence from OpenLava/LSF:
- avoid names implying derivation
- rewrite legacy behavior instead of patching
- never copy proprietary implementations
- separate protocol semantics from scheduling logic

---

# 6. Quick checklist before commit

```
[ ] clang-format applied
[ ] no write/send/chan_write in daemon code
[ ] all outbound messages use chan_alloc_buf + chan_enqueue
[ ] start_job checks SBD connected
[ ] canonical host used; remote-supplied hostnames ignored
[ ] eauth considered for job/user identity
[ ] lserrno set before returning errors
[ ] pointer style correct: char *p not char* p
[ ] one variable per line, declared near use
[ ] no tabs, no trailing whitespace
[ ] no ternary operators
[ ] no complex macros
[ ] no statements on one line with if/else
[ ] layering boundaries respected
[ ] mandatory config parameters validated, fatal on missing
[ ] no POST_DONE anywhere
```

---

# 7. Welcome

Welcome to LavaLite.

Make code that is boring, readable, correct, and fast.
When unsure, delete or simplify.

`clang-format` is your friend.
`chan_enqueue` is your safety rope.
Failing tests are feedback, not criticism.

> Nice, time to let the robot chew through the fossils.
> Yes. Someone has suffered here before.
> Let's build something serious.
