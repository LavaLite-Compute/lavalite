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
- functions that operate on module state take `void` and use the module globals directly
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

```
struct chan_buf *buf = chan_alloc_buf(ch, len);
memcpy(buf->data, payload, len);
chan_enqueue(ch, buf);    // actual write is deferred
```

Actual network I/O happens only inside the channel subsystem:

```
chan_epoll() → dowrite()
```

where backpressure, partial writes, disconnects, and retries are handled.

**Summary**

| Allowed in daemons | Forbidden |
|-------------------|----------|
| chan_alloc_buf() | write() |
| chan_enqueue() | send() |
| memcpy(payload) | chan_write() |
| async send via epoll | blocking I/O |

---

## 1.3 Scheduling invariants (start_job)

A job **must not start** unless the SBD for the target node is fully connected:

```
host_node->sbd_node != NULL
host_node->sbd_node->chanfd != -1
```

If violated:

```
LS_ERR("sbd on node %s is not connected cannot schedule", host_node->host);
return ERR_UNREACH_SBD;
```

On SBD disconnect:
- free mbd_client_node / sbd node
- set `hData->sbd_node = NULL`
- scheduler must mark the node as non-runnable

**Hard invariant:**
No job enters RUN state without an active SBD.

---

## 1.4 Host identity and trust model

The canonical hostname is determined at socket accept time:

```
client->host = canonical_resolved_host
```

Rules:
- never trust remote-supplied hostnames
- scheduler and lookups always use canonical host
- identity and cluster consistency depend on canonicalization, not request payloads

---

## 1.5 Error handling invariant

Use thread-local `lserrno` and set it before returning on errors.

Do not use deprecated `cherrno`.

Example:

```
if (!host) {
    lserrno = LLE_NO_HOST;
    return -1;
}
```

### Logging and error reporting (LS_XXX macros)

All logging in LavaLite **must** use the `LS_XXX` macros (`LS_ERR`, `LS_WARN`,
`LS_INFO`, `LS_DEBUG`, etc.).

These macros **already and automatically** include:
- the function name (`__func__`)
- the system error description (`%m`, when applicable)

As a consequence:
- **Do not** embed `__func__` in log messages
- **Do not** append `%m` or `strerror(errno)` manually
- Log messages must contain **only semantic information**, not plumbing

Correct usage:

`LS_ERR("sbd not connected host=%s job=%s", host->host, job_id);`

Incorrect usage:

`LS_ERR("%s: sbd not connected host=%s job=%s %m", func, host->host, job_id);`


Repeating function names or error strings leads to duplicated, noisy logs and
violates the project logging invariants. All contributors must assume that

`LS_XXX` macros always provide full contextual information.

---

## 1.6 Authentication & request identity (`eauth`)

LavaLite provides a **signed authentication token system** for user identity enforcement.
This layer is implemented and tested, but **not yet integrated into request processing**.
Once integrated, it will authenticate users before job submission or job operations.

### Concepts

A token contains:
- user, uid, gid
- host of origin
- timestamp
- nonce (anti-replay)

A detached signature covers the token for integrity.

### Enforcement today

User authentication is available where implemented.
**Daemon-to-daemon authentication is planned but not active.**

### Current boundary

Inter-daemon communication currently relies on **host identity and verified cluster node membership as defined in `lsb.hosts`**, providing controlled participation until token-based daemon authentication is enabled.

### Future invariant

Once integrated:

> No scheduling or job operation will run without user identity validated via eauth.
> Daemons will authenticate each other using signed tokens to prevent unauthorized cluster control.

---

# 2. C Coding Style

LavaLite follows a strict `.clang-format` based style.
**Do not hand-format code.**
Always format before commit.

## 2.1 Formatting usage

```
clang-format -i src/file.c
find . -name '*.[ch]' -exec clang-format -i {} +
```

Pull requests may be rejected for formatting violations.

---

## 2.2 Indentation & layout

- indent: 4 spaces
- no tabs
- soft limit: 80 columns
- always use braces

Correct:

```
if (x) {
    return 1;
}
```

Incorrect:

```
if (x) return 1;
```

---

## 2.3 Functions and prototypes

In `.c` files:

```
int ll_queue_init(struct ll_queue *q)
{
    ...
}
```

In header files:

```
int ll_queue_init(struct ll_queue *);
```

Parameter names are omitted in headers to reduce diff noise.

---

## 2.4 Pointer style

```
char *name;
struct hData *host;
```

The asterisk binds to the variable.

---

## 2.5 Variable declarations

Declare one variable per line:

```
int argc;
int count;
char *path;
char *host;
```

---

## 2.6 Boolean type

Use `bool_t`.
Use lowercase literals `true` and `false`.

Do not use:
- `bool`
- `<stdbool.h>`
- uppercase `TRUE` / `FALSE`

---

## 2.7 Include order

```
#include <system headers>

#include "own_header.h"
#include "other_project_header.h"
```

`SortIncludes: false` in `.clang-format` preserves ordering intentionally.

---
## Comments

Code must be readable without comments.

Comments are **not** used to explain what the code does, line by line, or to restate obvious logic.
Such comments inevitably become obsolete and misleading.

Comments are allowed **only** to document:
- invariants that are not visible from the local code
- design decisions and rationale ("why", not "what")
- non-obvious constraints or intentional limitations
- counterintuitive behavior that must not be "cleaned up"

Comments must describe **rules**, **contracts**, or **assumptions**, not control flow.
If a comment can be removed without losing essential information, it should not exist.

# 3. Miscellaneous rules

- avoid complex macros
- prefer `enum` or `static const` over `#define` where appropriate
- internal functions should be `static`
- functions should be reasonably short
- avoid “clever” tricks
- do not reinvent memory or logging subsystems

---

# 4. Provenance & legal considerations

To maintain independence from OpenLava/LSF:
- avoid names implying derivation
- rewrite legacy behavior instead of patching
- never copy proprietary implementations
- separate protocol semantics from scheduling logic

---

# 5. Quick checklist before commit

```
[ ] clang-format applied
[ ] no write/send/chan_write in daemon code
[ ] all outbound messages use chan_alloc_buf + chan_enqueue
[ ] start_job checks SBD connected
[ ] canonical host used; remote-supplied hostnames ignored
[ ] eauth considered for job/user identity
[ ] lserrno set before returning errors
[ ] pointer style correct
[ ] one variable per line
[ ] no tabs, no trailing whitespace
[ ] no complex macros or clever hacks
[ ] layering boundaries respected
```

---

# 6. Welcome

Welcome to LavaLite.
Make code that is:

- boring
- readable
- correct
- fast

When unsure, delete or simplify.

```
clang-format
```

is your friend.

```
chan_enqueue
```

is your safety rope.

Failing tests are feedback, not criticism.

Let's build something serious.
