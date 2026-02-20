# LavaLite Control Utility (`llctrl`)

## Design Rationale

Historically, cluster administration in LSF and OpenLava was done through
two separate tools — `lsadmin` and `badmin` — that communicated with the
daemons using proprietary RPC protocols. These interfaces were fragile,
tightly coupled to daemon internals, and difficult to secure or extend.

LavaLite replaces them with a single, minimal tool: **`llctrl`**.

`llctrl` uses **SSH as its transport layer**, invoking well-defined
commands remotely (`systemctl restart`, `--config-check`, etc.).
This approach has several advantages:

- **Simplicity** — no custom daemons or control sockets.
- **Security** — leverages existing SSH authentication, logging, and
  audit mechanisms.
- **Portability** — works on Linux, EOS, and any UNIX system with SSH.
- **Clarity** — operations are transparent shell commands, not hidden RPCs.
- **Resilience** — no dependency on a single control service.

Interactive shells are deliberately omitted; `llctrl` is a pure
**command-line utility** designed for automation, scripting, and
parallel orchestration.

---

## Overview

`llctrl` is the unified administrative interface for all LavaLite daemons.

It replaces the legacy `lsadmin` and `badmin` tools and provides a
single, consistent command-line interface for starting, stopping,
reloading, and verifying LavaLite components across the cluster.

The tool operates over **SSH**, using key-based authentication and
passwordless `sudo` privileges for specific system commands.

---

## Requirements

### SSH Access

- Each LavaLite node must run an SSH server (typically `sshd`).
- The control host must be able to reach each node on TCP port 22.
- SSH key-based authentication is **required**.

### Dedicated User

Create a dedicated management user on all nodes, e.g.:

```bash
useradd -m lavalite-admin
```

# `llctrl` Command Reference

## Command Syntax


---

## Global Options

| Option | Description |
|---------|-------------|
| `--hosts <list>` | Comma-separated list of target hosts (e.g. `--hosts lim1,lim2`) |
| `--file <path>` | Path to file containing one host per line |
| `--parallel <N>` | Number of parallel SSH sessions (default: 16) |
| `--timeout <SECONDS>` | Timeout for each SSH command (default: 10) |
| `-v`, `--verbose` | Print executed SSH commands and their output |
| `-h`, `--help` | Display help message and exit |

Example:
```bash
llctrl --hosts node1,node2 --parallel 8 --timeout 5 lim restart
| Command                   | Description                                                    |
| ------------------------- | -------------------------------------------------------------- |
| `llctrl lim start`        | Start `lavalite-lim` daemon via `systemctl start lavalite-lim` |
| `llctrl lim stop`         | Stop the `lavalite-lim` daemon                                 |
| `llctrl lim restart`      | Restart the `lavalite-lim` daemon                              |
| `llctrl lim reload`       | Reload the LIM configuration without restart                   |
| `llctrl lim config-check` | Validate LIM configuration syntax                              |
| `llctrl lim status`       | Show status summary via `systemctl status lavalite-lim`        |


llctrl --hosts lim1,lim2 lim restart

| Command                      | Description                                     |
| ---------------------------- | ----------------------------------------------- |
| `llctrl sbatch start`        | Start `lavalite-sbatchd` daemon                 |
| `llctrl sbatch stop`         | Stop `lavalite-sbatchd` daemon                  |
| `llctrl sbatch restart`      | Restart the daemon                              |
| `llctrl sbatch reload`       | Reload configuration without restart            |
| `llctrl sbatch config-check` | Validate batch daemon configuration             |
| `llctrl sbatch status`       | Show `systemctl status lavalite-sbatchd` output |

llctrl host drain worker3
llctrl host undrain worker3

| Command                        | Description                                    |
| ------------------------------ | ---------------------------------------------- |
| `llctrl queue enable <queue>`  | Enable the specified queue for job submission  |
| `llctrl queue disable <queue>` | Disable queue for maintenance                  |
| `llctrl queue status`          | List queues with their enabled/disabled states |

| Command                   | Description                                    |
| ------------------------- | ---------------------------------------------- |
| `llctrl cluster reconfig` | Reload configuration across all nodes          |
| `llctrl cluster status`   | Show overall status of LIM and SBATCHD daemons |

| Code | Meaning                          |
| ---- | -------------------------------- |
| `0`  | All operations succeeded         |
| `1`  | One or more hosts failed         |
| `2`  | SSH or connectivity error        |
| `3`  | Invalid command or parameters    |
| `4`  | Timeout waiting for host replies |

llctrl --file nodes.txt lim restart
llctrl --file nodes.txt sbatch restart
