# llctrl Design Document

## 📌 Overview

`llctrl` is a lightweight Bash-based command-line utility for managing remote daemons and configurations across clusters or distributed systems. It wraps `pdsh` and `pdcp` to provide scalable, parallel control over nodes, targeting environments ranging from full HPC clusters to low-level devices like Arista switches.

---

## 🎯 Goals

- Provide a simple CLI for starting, stopping, checking, and reconfiguring remote daemons
- Avoid reinventing low-level socket or process control logic
- Ensure compatibility with minimal environments (no Python or heavy dependencies)
- Support host grouping and parallel execution via `pdsh`
- Enable configuration distribution via `pdcp`

---

## 🧱 Architecture

### Components

| Component              | Description                                      |
|------------------------|--------------------------------------------------|
| `llctrl.sh`            | Main CLI entry point                             |
| `lib/pdsh_helpers.sh`  | Reusable wrappers for `pdsh` and `pdcp`          |
| `config/groups.conf`   | Optional static host group definitions           |
| `logs/llctrl.log`      | Optional execution log file                      |

### Dependencies

- Bash (POSIX-compliant)
- `pdsh` and `pdcp`
- SSH access to target nodes (passwordless recommended)

---

## 📊 System Diagram

```mermaid
flowchart TD
    A[User CLI Input] --> B[llctrl.sh]
    B --> C{Action}
    C --> D1[Start Daemon]
    C --> D2[Stop Daemon]
    C --> D3[Status Check]
    C --> D4[Push Config]
    C --> D5[Check Config]

    D1 --> E1[pdsh -R ssh -g group "systemctl start"]
    D2 --> E2[pdsh -R ssh -g group "systemctl stop"]
    D3 --> E3[pdsh -R ssh -g group "systemctl status"]
    D4 --> E4[pdcp -g group config.conf]
    D5 --> E5[pdsh -R ssh -g group "cat config.conf"]

    E1 --> F[Remote Nodes]
    E2 --> F
    E3 --> F
    E4 --> F
    E5 --> F
llctrl <action> <daemon> [-g group] [--config file] [--dry-run]
llctrl start mydaemon -g compute
llctrl stop mydaemon -g storage
llctrl status mydaemon -g all
llctrl push-config /etc/mydaemon.conf -g compute
llctrl check-config /etc/mydaemon.conf -g compute
