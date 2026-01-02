# Contributing to LavaLite

Thank you for your interest in contributing to **LavaLite**.

LavaLite is a clean rewrite built on the ideas of Platform Lava 1.0 (2007), not its implementations.
We maintain strict code hygiene, independent lineage, and modern design.

If you contribute, you become part of that responsibility.

---

## What We Expect

- **Clarity over cleverness**
  Make code boring, readable, correct, and fast.

- **No legacy imports**
  Do *not* reintroduce OpenLava, Volclava, or Platform code.
  We reuse concepts, not code.

- **GPLv2 compatibility**
  All contributions must be legally compatible.
  Provenance matters.

- **Stay within the architecture**
  Respect layer boundaries: `wire_*`, `chan_*`, `ll_*`, daemons.
  No direct `write()` in daemons.

- **Follow the coding guide** *(mandatory)*
  The coding + invariants guide is here:
  `docs/projects/lavalite_coding_guide.md`

  When in doubt, ask what Unix and K&R would do — and do that.

---

## Workflow

1. Fork the repository
2. Create a focused branch for your change
3. Run `clang-format` on every `.c` and `.h`
4. Ensure your change respects architectural invariants
5. Write meaningful commit messages
6. Open a pull request

Small commits with clear intent are preferred over large speculative changes.

---

## Testing

```
./configure
make
make check
```

Tests should be small, direct, and reproducible.
If the change affects job logic, scheduling, or message passing, add or update tests accordingly.

---

## Documentation

If your change affects behavior, update:

- man pages where relevant
- `docs/projects/` if behavior or invariants evolve
- the coding guide only when core rules change
  *(not for stylistic preferences)*

---

## Behavior Over Style

Format changes without behavioral changes are discouraged unless they fix drift from `.clang-format`.

If you touch code, improve clarity.
If you change behavior, document it.
If you refactor, simplify.

---

## Final Words

LavaLite is a modern HPC system, not a museum.
We honor the past by writing better code, not by copying old code.

Welcome aboard.
