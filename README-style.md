# LavaLite C Coding Style Guide

This document defines the **canonical C coding style** for the LavaLite project.

The goals are:
- consistent and modern formatting
- readability and maintainability
- no legacy OpenLava/LSF quirks
- fully enforceable using `clang-format`

This style is required for all new code contributions.

---

## 1. Indentation & Whitespace

- Indentation: **4 spaces** (never tabs)
- `TabWidth = 4`, `IndentWidth = 4`
- **No trailing whitespace**
- Soft line limit: **100 columns**

Example:

```c
if (cond) {
    do_something();
}

# 0. Using `.clang-format` (Mandatory)

The repository contains a `.clang-format` file that defines the **official LavaLite style**.

### Format all modified files before committing:

```bash
git clang-format
