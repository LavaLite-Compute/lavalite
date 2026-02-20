% LLCTRL(1) LavaLite Control Utility | LavaLite Documentation
% LavaLite Project
% November 2025

# NAME
**llctrl** — Unified administrative control utility for LavaLite daemons

# SYNOPSIS
**llctrl** [*GLOBAL OPTIONS*] *target* *action* [*ARGS...*]

# DESCRIPTION
**llctrl** is the unified, SSH-based administrative tool for managing
LavaLite cluster daemons.
It replaces the legacy **lsadmin** and **badmin** commands.

`llctrl` executes control and configuration operations on remote hosts
using SSH with key-based authentication and password-less sudo for
specific system commands (typically systemd unit management and
configuration checks).

Unlike legacy tools, `llctrl` has **no interactive shell**; all commands
are executed directly on the command line or through automation scripts.

# OPTIONS
**Global options:**

| Option | Description |
|---------|-------------|
| `--hosts <list>` | Comma-separated list of target hosts (e.g. `--hosts node1,node2`) |
| `--file <path>` | Path to a file containing one host per line |
| `--parallel <N>` | Number of parallel SSH sessions (default: 16) |
| `--timeout <SECONDS>` | Timeout for each SSH operation (default: 10) |
| `-v`, `--verbose` | Print executed SSH commands and their output |
| `-h`, `--help` | Display usage information and exit |

If no hosts are provided, future versions will use the cluster registry
as the default host list.

# COMMANDS

## LIM (Load Information Manager)
| Action | Description |
|---------|-------------|
| **start** | Start `lavalite-lim` via `systemctl start lavalite-lim` |
| **stop** | Stop the `lavalite-lim` daemon |
| **restart** | Restart the daemon via `systemctl restart lavalite-lim` |
| **reload** | Reload configuration without restart |
| **config-check** | Validate configuration syntax |
| **status** | Show service status summary |

**Example:**
llctrl --hosts lim1,lim2 lim restart

---

## SBATCH (Batch Daemon)
| Action | Description |
|---------|-------------|
| **start** | Start `lavalite-sbatchd` via `systemctl start lavalite-sbatchd` |
| **stop** | Stop the `lavalite-sbatchd` daemon |
| **restart** | Restart `lavalite-sbatchd` |
| **reload** | Reload configuration without restart |
| **config-check** | Validate batch daemon configuration |
| **status** | Show systemd status summary |

**Example:**


---

## HOST
| Action | Description |
|---------|-------------|
| **drain** *<host>* | Mark a host as unavailable for new jobs |
| **undrain** *<host>* | Re-enable job scheduling on the host |
| **status** | Show current host states (drained, ok, unreachable) |

**Examples:**
llctrl host drain worker3
llctrl host undrain worker3


---

## QUEUE
| Action | Description |
|---------|-------------|
| **enable** *<queue>* | Enable the specified queue for scheduling |
| **disable** *<queue>* | Disable queue temporarily |
| **status** | Display queue enable/disable status |

**Examples:**
llctrl queue disable geo
llctrl queue enable cpu-heavy


---

## CLUSTER
| Action | Description |
|---------|-------------|
| **reconfig** | Reload configuration across all nodes |
| **status** | Display overall cluster daemon status |

**Example:**
llctrl cluster reconfig

# EXIT STATUS
| Code | Meaning |
|------|----------|
| **0** | All hosts succeeded |
| **1** | One or more hosts failed |
| **2** | SSH or connectivity error |
| **3** | Invalid command or parameters |
| **4** | Timeout waiting for host replies |

Each host produces one summary line:
[OK] lim1: lavalite-lim restarted
[ERR] lim2: connection timed out


# ENVIRONMENT
| Variable | Description | Default |
|-----------|-------------|----------|
| **LL_SSH** | SSH binary path | `/usr/bin/ssh` |
| **LL_SSH_OPTS** | Extra SSH options | `-o BatchMode=yes -o ConnectTimeout=5` |
| **LL_PARALLEL** | Maximum concurrent SSH sessions | `16` |
| **LL_TIMEOUT** | SSH command timeout (seconds) | `10` |

# SECURITY
- Uses SSH public-key authentication only; password logins are disabled.
- Dedicated management account (recommended: *lavalite-admin*).
- Requires password-less sudo for a limited set of commands:
  - `/usr/bin/systemctl restart lavalite-lim`
  - `/usr/bin/systemctl restart lavalite-sbatchd`
  - `/usr/sbin/lavalite-lim --config-check`
  - `/usr/sbin/lavalite-sbatchd --config-check`

# FILES
| Path | Description |
|-------|-------------|
| `/etc/sudoers.d/lavalite` | Sudo rules for management user |
| `/usr/bin/llctrl` | Main executable |
| `/var/log/lavalite/ctrl.log` | Optional local audit log (future) |

# EXAMPLES
Restart all core daemons:
llctrl --file allnodes.txt lim restart
llctrl --file allnodes.txt sbatch restart

Check configuration before reload:
llctrl --hosts controller lim config-check
llctrl --hosts worker[1-4] sbatch config-check


Drain and re-enable a host:

llctrl host drain worker5
llctrl host undrain worker5

Disable and enable a queue:
llctrl queue disable gpu
llctrl queue enable gpu

# COMPATIBILITY
Replaces the following legacy commands:

| Legacy | Replacement |
|---------|-------------|
| `lsadmin` | `llctrl lim ...` |
| `badmin` | `llctrl sbatch ...` |

# SEE ALSO
**lavalite-lim(8)**, **lavalite-sbatchd(8)**, **systemctl(1)**, **ssh(1)**

# AUTHORS
LavaLite Project — modern HPC/HTC scheduler and control system.
Documentation maintained by the LavaLite development team.
