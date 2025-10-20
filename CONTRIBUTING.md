# Contributing to Lavalite

Thank you for your interest in contributing to Lavalite. This project is a clean rewrite rooted in Platform Lava 1.0 (2007), and we maintain strict lineage hygiene.

## Guidelines

- **No legacy code**: Do not reintroduce code from OpenLava, Volclava, or other forks.
- **License**: All contributions must be compatible with GPLv2.
- **Code style**: Use modern C conventions. Prefer clarity over cleverness.
- **Commits**: Write meaningful messages. Avoid referencing legacy scaffolds.
- **Testing**: Include sanity macros and minimal reproducible examples.
- **Documentation**: Update man pages and README when behavior changes.

## Build & Test

```bash
./configure
make
make check
``
