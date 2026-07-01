# LavaLite Services – User Experience

## Purpose

This document describes the user experience provided by LavaLite Services.

It intentionally avoids implementation details and focuses on how users interact with persistent workspaces within an HPC environment.

---

# The Workspace

Alice is a computational scientist.

Every morning she opens her web browser and signs in to the HPC center.

Once authenticated, her personal workspace is immediately available.

The workspace is persistent.

It already contains the notebooks, terminals, files and applications exactly as she left them the previous day.

Alice does not need to start the workspace.

She simply connects to it.

---

# Working

The workspace becomes Alice's primary working environment.

From there she can:

- develop applications
- edit source code
- inspect datasets
- visualize results
- open terminal sessions
- monitor running jobs
- submit new batch jobs

The workspace remains available throughout the day.

---

# Batch Computing

Large computations continue to run as ordinary LavaLite batch jobs.

The workspace is used to prepare, submit, monitor and analyze those jobs.

Interactive work and batch computation complement each other while remaining separate activities.

---

# Leaving

When Alice finishes for the day, she simply closes her browser.

The workspace continues to exist.

Running notebooks, editors and terminal sessions remain available.

The next time Alice connects, she resumes exactly where she left off.

---

# User Expectations

Users should not need to understand the underlying infrastructure.

From their perspective, the experience is simple.

Their workspace is always available.

Their work is preserved between sessions.

They can submit and monitor batch jobs without leaving the workspace.

The underlying scheduler transparently manages resources, placement and recovery.

---

# Current Vision

LavaLite Services provide persistent interactive workspaces for HPC users.

The goal is to make connecting to an HPC environment as natural as opening a web browser, while preserving the strengths of traditional batch scheduling.
