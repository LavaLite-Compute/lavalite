# README-RES_REQ.md

## Purpose

This document defines the syntax and semantics of resource requirement
expressions used in LavaLite via the `-R` option. These expressions are
evaluated using a C-based math evaluation library, replacing the legacy Tcl
interpreter. The system supports host filtering (`select[]`) and ranking
(`order[]`) based on real-time resource metrics.

---

## 📊 Supported Resource Variables

The following variables are available for use in `select[]` and `order[]`
clauses. They include the original 11 load indices from Lava, plus any custom
resources defined in `lsf.shared`.

| Variable Name | Description                        | Type     | Notes                          |
|---------------|------------------------------------|----------|--------------------------------|
| `r1m`         | 1-minute CPU load average          | float    | Typically normalized per core |
| `r5m`         | 5-minute CPU load average          | float    |                                |
| `r15m`        | 15-minute CPU load average         | float    |                                |
| `ut`          | CPU utilization                    | float    | Percentage (0–100)             |
| `pg`          | Paging rate                        | float    | Pages/sec                      |
| `io`          | Disk I/O rate                      | float    | Blocks/sec                     |
| `ls`          | Run queue length                   | integer  | Number of runnable processes  |
| `it`          | Idle time                          | float    | Seconds                        |
| `tmp`         | Temporary space available          | float    | MB or GB depending on config  |
| `swp`         | Swap space available               | float    | MB or GB                       |
| `mem`         | Free memory                        | float    | MB or GB                       |
| `resX`        | Custom resource `X` from lsf.shared| float/int| Defined in `lsf.shared`       |

> Note: Custom resources (`resX`) are dynamically loaded from `lsf.shared` and
> may include GPU count, license tokens, or site-specific metrics.

---

## 🔣 Supported Operators

### Arithmetic
- `+`, `-`, `*`, `/`, `%` (modulo)
- `()` for grouping

### Comparison
- `==`, `!=`, `>`, `<`, `>=`, `<=`

### Logical
- `&&` (AND)
- `||` (OR)
- `!` (NOT)

---

## ✅ `select[]` Clause: Host Filtering

The `select[]` clause filters hosts based on resource expressions. Only hosts
that evaluate the expression to true are considered eligible.

### Examples

```bash
# Hosts with at least 4GB memory and low CPU load
-R "select[mem >= 4096 && r1m < 1.0]"

# Idle time over 600 seconds or swap space above 2GB
-R "select[it > 600 || swp > 2048]"

# Require at least 2 GPUs
-R "select[resGPU >= 2]"

# Complex expression with grouping
-R "select[(mem > 8192 && r5m < 0.5) || tmp > 10240]"

# Prefer hosts with lowest 1-minute load
-R "select[mem >= 4096] order[r1m:asc]"

# Prefer hosts with most free memory
-R "select[r1m < 1.0] order[mem:desc]"

# Prefer hosts with highest idle time
-R "select[ut < 50] order[it:desc]"

# Multi-level sort: first by idle time, then by swap space
-R "select[mem > 2048] order[it:desc,swp:desc]"

# Prefer hosts with lowest paging rate and lowest CPU utilization
-R "select[mem > 8192] order[pg:asc,ut:asc]"

# Custom resource: prefer hosts with most GPUs
-R "select[resGPU >= 1] order[resGPU:desc]"

# Tie-breaker: sort by memory, then by host name (default)
-R "select[r1m < 2.0] order[mem:desc]"

