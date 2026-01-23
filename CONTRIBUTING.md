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
Functions operating on module state are considered module "methods" and **must not** pretend to be pure or reusable.

Rules:

- functions that operate on module state take `void` and use module globals directly
- functions that take parameters must be genuinely reusable and must not rely on module globals

The translation unit itself acts as the namespace.
Explicit namespacing or artificial abstraction is avoided.

---

# 1. Project Architectural Invariants

These invariants define the **design boundaries** of LavaLite.
Code that violates these rules is considered incorrect even if it compiles.

---

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
chan_enqueue(ch, buf);
```

Actual network I/O happens only inside the channel subsystem.

**Summary**

| Allowed in daemons | Forbidden |
|-------------------|-----------|
| chan_alloc_buf()  | write()   |
| chan_enqueue()    | send()    |
| memcpy(payload)   | chan_write() |
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

Rules:

- never trust remote-supplied hostnames
- scheduler always uses canonical host
- identity depends on canonicalization

---

## 1.5 Error handling invariant

Use thread-local `lserrno`.
Do not use deprecated `cherrno`.

Logging must use `LS_XXX` macros.
They already include `__func__` and `%m`.

Correct:

```
LS_ERR("sbd not connected host=%s job=%s", host->host, job_id);
```

Incorrect:

```
LS_ERR("%s: sbd not connected host=%s job=%s %m", func, host->host, job_id);
```

---

## 1.6 Authentication & request identity (eauth)

Token contains:

- user, uid, gid
- host of origin
- timestamp
- nonce

Future invariant:
No scheduling or job operation without validated identity.

---

# 2. C Coding Style

LavaLite follows strict `.clang-format`.
Do not hand-format code.

---

## 2.1 Formatting usage

```
clang-format -i src/file.c
find . -name '*.[ch]' -exec clang-format -i {} +
```

---

## 2.2 Indentation & layout

- indent: 4 spaces
- no tabs
- 80-column soft limit
- always use braces

---

## 2.3 Functions and prototypes

In `.c`:

```
int ll_queue_init(struct ll_queue *q)
{
    ...
}
```

In `.h`:

```
int ll_queue_init(struct ll_queue *);
```

---

## 2.4 Pointer style

```
char *name;
struct hData *host;
```

---

## 2.5 Variable declarations

- one variable per line

---

## 2.6 Boolean type

- use bool_t
- use true/false lowercase

---

## 2.7 Include order

```
#include <system headers>

#include "own_header.h"
#include "other_header.h"
```

---

## 2.8 Naming conventions

Exported daemon APIs:

- mbd_* for mbatchd
- sbd_* for sbatchd

Internal static helpers use short names.

### No POST_DONE (ever)

Forbidden.
Must not exist in any form.

---

## 2.9 Macros and Ternary Operators

### Rules

- Avoid macros unless structurally necessary
- Macros must not hide logic or contain control flow
- Prefer enum or static const over macros
- Avoid ternary operators
- Allowed only for trivial, side-effect-free expressions
- Never use ternaries for branching or error handling

### Rationale

Macro and ternary operators are not banned, but they are controlled. Both
constructs are easy to abuse and tend to produce “clever” code that hides logic,
breaks readability, and complicates debugging. LLMs in particular overuse them and generate
opaque expressions that violate LavaLite’s clarity requirements. Use macros only
when they behave like constants (bit flags, protocol values) and never to wrap logic or create
hidden control flow. Use ternaries only for trivial, side-effect-free expressions where the
intent is obvious. If a macro or ternary makes the code harder to scan, harder to reason about,
or harder to maintain, rewrite it. The goal is explicit, boring, maintainable C — not cute tricks.

---

## Comments

Comments document:

- invariants
- design decisions
- non-obvious constraints
- counterintuitive behavior

Comments do not restate control flow.

---

# 3. Miscellaneous rules

- avoid complex macros
- prefer enum/static const
- internal functions should be static
- functions should be short
- avoid clever tricks
- do not reinvent memory/logging subsystems

---

# 4. Provenance & legal considerations

- avoid names implying derivation
- rewrite legacy behavior
- never copy proprietary implementations

---

# 5. Quick checklist before commit

- clang-format applied
- no write/send in daemons
- all outbound messages use chan_alloc_buf + chan_enqueue
- start_job checks SBD connected
- canonical host used
- lserrno set before errors
- pointer style correct
- one variable per line
- no tabs or trailing whitespace
- layering respected

---

# 6. Welcome

Write code that is:

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

Let’s build something serious.
