# **LavaLite Administration Guide**

**Author:** *LavaLite Project*
**Date:** _Today_

---

## 📘 Overview

LavaLite is a lightweight batch system derived from Platform Lava 1.0.
This guide describes how to manage its daemons, verify system health, and understand its installation layout.

---

## 📁 Installation Layout

LavaLite installs into a self-contained directory under `/opt/lavalite-*major.minor.bug*`.
The layout separates binaries, configuration, logs, and runtime data.

### 📂 Directory Tree
lavalite-0.1/
|- bin/
|- etc/
|- include/
|- lib/
|- log/
|- sbin/
|- share/
|  |- man/
|     |- man1/
|     |- man5/
|     |- man8/
|- work/
|- info/

---

## ⚙️ Configuration

LavaLite currently retains the flat Platform Lava-style configuration structure.
All config files reside in the `etc/` directory.
etc/
|- lsb.hosts
|- lsb.params
|- lsb.queues
|- lsb.users
|- lsf.cluster.lavalite
|- lsf.conf
|- lsf.shared

---

## 🛠️ Systemd Units

Users must install the following unit files manually:
lavalite-lim.service              # load information manager
lavalite-sbatchd.service      # per-node batch daemon
lavalite-mbatchd.service      # master scheduler

**Important:**
**`sbatchd` is the parent of `mbatchd`. You must stop `sbatchd` before stopping `mbatchd`, or before running `mbatchd` in debug mode.**

---

## 🧩 `llctl` Command

The `llctl` command is the unified control interface for LavaLite.
It wraps systemd operations and provides internal daemon control.

### 🔄 Daemon Lifecycle
llctl lim start|stop
llctl sbatchd start|stop
llctl mbatchd start|stop

### 🔐 Internal Operations
llctl lim lock|unlock              # clears on reboot
llctl mbatchd queue open|close Q1
llctl mbatchd host open|close nodeA

### 🧪 Examples

llctl sbatchd stop
llctl mbatchd start
llctl lim lock
llctl mbatchd queue close Q1
llctl mbatchd host open nodeA
llctl status

---

## 🩺 Health Checks

Each daemon supports a basic health probe.

### `mbatchd`

- Connect to `/run/lavalite/mbatchd.sock`
- Send `PING`
- Expect `PONG <version>`

### `sbatchd` and `lim`

- Ping their local sockets
- Verify heartbeat timestamps are current

---

## 🔐 Security

Use SSH keys for remote operations.
Optional sudo rules can allow controlled access to systemd units.

---

## 🧯 Troubleshooting

- Run `sync` if virtiofs caching causes delays.
- Use `journalctl -u lavalite-mbatchd` for logs.
- Verify SSH access between nodes if `llctl` times out.
