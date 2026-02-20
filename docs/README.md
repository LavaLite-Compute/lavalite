# LavaLite Documentation Layout

LavaLite documentation is organized by purpose.

Only the location of a document determines its authority.

---

## Directory Structure

admin/
    Operational documentation.
    Describes how to install, configure, and run LavaLite.

design/
    Architectural and internal documentation.
    Explains how LavaLite works and why.

man/
    Manual pages installed with the system.
    Only implemented and supported commands are documented here.

archive/
    Historical, experimental, or superseded material.
    Documents in this directory may not reflect current behavior.

---

## Documentation Discipline

### admin/

Documents under `admin/` must describe observable behavior.

If instructions cannot be executed successfully on a clean system,
the document is incorrect.

Operational documentation must remain minimal and current.

---

### design/

Design documents explain internal mechanisms such as:

- execution pipeline
- protocols
- file formats
- scheduler behavior
- validation procedures

These documents describe intent and invariants, not usage steps.

---

### man/

Manual pages document supported user and administrator commands.

A command must not have a man page unless it is implemented
and operational.

---

### archive/

The archive preserves development history and design evolution.

Archived documents are retained for reference only and are
not authoritative.

---

## Two-Document Rule

LavaLite must remain understandable from two documents:

1. admin/INSTALL.md      — how to run the system
2. design/PIPELINE.md    — how the system works

All other documentation is secondary.

---

## General Rules

- Prefer deletion or archival over rewriting obsolete material.
- Do not duplicate operational instructions across documents.
- Implementation behavior is authoritative.
- Documentation must follow the code.
