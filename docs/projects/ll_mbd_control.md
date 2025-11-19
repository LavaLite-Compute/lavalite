# LavaLite LIM–MBD Supervision & Control Protocol
### Design Document (v1.0)

## Status
**Draft** — prepared for design iteration and upcoming implementation phase.

## Author
ChatGPT (design assistant)
Feedback and architectural decisions: bokassa bokisius

---

# 1. Introduction

This document defines the **supervision, lifecycle control, and failure-handling protocol** between:

- **LIM** — the Lightweight Infrastructure Manager (master node agent)
- **MBD (mbatchd)** — the LavaLite scheduler daemon

In traditional OpenLava/LSF designs:

- **sbatchd** (slave daemon) starts `mbatchd` on the master.
- Killing `mbatchd` causes `sbatchd` to restart it immediately.
- Shutting down `sbatchd` kills `mbatchd`, confusing admins.
- LIM was **never** part of the control plane for MBD.

In **LavaLite**, this model is redesigned so that LIM becomes the authoritative controller of the scheduler's lifecycle.

---

# 2. Goals

### 2.1 Functional Goals
- LIM is the **only entity allowed** to start, stop, restart, and supervise MBD.
- MBD must detect if LIM disappears and gracefully **pause or resign**.
- Provide a **clean, deterministic**, admin-facing mechanism to manage daemons:
  - `llctrl mbd start/stop/restart/status`
  - `llctrl sbatchd start/stop/restart/status`
- Provide a **reliable IPC channel** for:
  - supervision heartbeats
  - command/control messages
  - status propagation

### 2.2 Non-Goals
- No legacy ELIM model
- No heavy state machines
- No “policy engine” in LIM
- No opaque hang-prone pipes
- No global Tcl interpreters

---

# 3. High-Level Design Summary

The new architecture uses:

### **1. A supervisor socketpair**
Created when LIM launches MBD:
- One FD stays in LIM
- One FD is passed to MBD via fork+exec
- Provides fast, reliable, local signalling
- Used for:
  - LIM → MBD control commands
  - MBD → LIM heartbeat and resign notifications

### **2. A loopback UDP protocol channel**
Backed by a new LIM opcode:
- `LIM_MBD_REGISTER`
- Allows MBD to **re-register** with a restarted LIM
- Ensures LIM restarts are transparent
- Provides status negotiation and capability discovery

### **3. Robust Rules**
- **MBD cannot outlive LIM** unless explicitly allowed for maintenance mode
- Admin operations go through `llctrl`, never through systemd signals
- LIM always knows:
  - Is MBD running?
  - Is it responsive?
  - Is it paused?
  - Does it need restarting?

---

# 4. Components

## 4.1 LIM Supervisor
Responsible for:
- building the socketpair
- spawning MBD with correct environment and FD inheritance
- monitoring FD liveness
- receiving heartbeat pings
- sending commands:
  - STOP
  - PAUSE
  - RESUME
  - SHUTDOWN
  - RESTART

If MBD dies unexpectedly:
- LIM logs the reason
- LIM restarts MBD unless policy forbids it (maintenance mode)

## 4.2 MBD Lifecycle Logic
MBD behavior when linked to LIM:

- On startup, it:
  1. Reads the supervisor FD number from environment (`LAVALITE_SUP_FD`)
  2. Sends `REGISTER` message to LIM via socketpair
  3. Opens the loopback UDP channel and sends `LIM_MBD_REGISTER`

- Heartbeats flow from MBD → LIM every 2–5 seconds

If heartbeat ACK stops:
- LIM sends a soft termination → if no reply
- LIM force-kills and restarts MBD

If LIM disappears:
- MBD receives EOF on supervisor FD
- MBD enters **LL_MBD_PAUSED** state
- When LIM comes back and sends `LIM_MBD_REGISTER_ACK`, MBD resumes

---

# 5. Protocol

## 5.1 Supervisor Socket Messages (local, reliable)

### LIM → MBD
| Command | Description |
|---------|-------------|
| `CMD_START` | Only used internally after fork |
| `CMD_STOP` | Gracefully stop scheduler |
| `CMD_RESTART` | Graceful stop, then exec again |
| `CMD_PAUSE` | Enter LL_MBD_PAUSED state |
| `CMD_RESUME` | Wake up and resume scheduling |
| `CMD_SHUTDOWN_CLUSTER` | Admin intends full cluster shutdown |

### MBD → LIM
| Message | Description |
|---------|-------------|
| `MBD_HEARTBEAT` | Scheduler alive |
| `MBD_FATAL` | Internal fatal error |
| `MBD_PAUSED` | Paused due to LIM disappearance |
| `MBD_READY` | Startup completed |
| `MBD_REGISTER` | Initial handshake |

---

## 5.2 UDP Loopback Messages

OpCode: **`LIM_MBD_REGISTER`**

Used only for reconnecting after LIM restarts.

### MBD → LIM
- “I’m alive, here are my capabilities, here's my supervision FD number.”

### LIM → MBD
- ACK
- Assigns full or partial scheduling rights
- Optionally instructs MBD to enter PAUSED state until the cluster is ready

---

# 6. State Machine

## LIM States
```
INIT → RUNNING → (RESTARTING_MBD) → RUNNING → SHUTTING_DOWN
```

## MBD States
```
INIT
  → REGISTERING
  → RUNNING
  → PAUSED (LIM vanished or sent CMD_PAUSE)
  → RESIGNED (LIM gone permanently)
  → STOPPED
```

Transitions:

- LIM restart → MBD moves PAUSED → REGISTERING → RUNNING
- LIM stop → MBD moves PAUSED → RESIGNED
- LIM CMD_STOP → MBD → STOPPED

---

# 7. Failure Handling

## 7.1 LIM Crashes Unexpectedly
MBD detects FD = EOF → enters PAUSED
LIM restarts → sends `LIM_MBD_REGISTER_ACK` → MBD resumes

## 7.2 MBD Crashes Unexpectedly
LIM:
1. detects socketpair EOF
2. logs crash
3. restarts MBD unless forbidden by maintenance mode

## 7.3 Network partitions between nodes
Irrelevant — supervision is local.
Scheduler suspension logic remains unaffected.

---

# 8. Administrative Control (llctrl)

### New commands:
```
llctrl mbd start
llctrl mbd stop
llctrl mbd restart
llctrl mbd status
llctrl sbatchd start
llctrl sbatchd stop
llctrl sbatchd status
```

All actions:
- go through LIM
- trigger commands through supervisor socketpair
- never manipulate PIDs directly
- never depend on systemd

---

# 9. Implementation Plan

## Phase 1 — Foundation
- Add socketpair creation in LIM
- Modify LIM spawning logic for MBD
- Add environment export of FD
- Add heartbeat loop in MBD
- Add supervisor thread/task in LIM

## Phase 2 — UDP registration channel
- Add new opcode `LIM_MBD_REGISTER`
- Add reconnection logic in MBD
- Add ACK logic in LIM

## Phase 3 — llctrl integration
- Implement CLI mapping to supervisory commands
- Expose LIM RPC for mbd control
- Add status reporting

## Phase 4 — Hardening
- Timeouts
- Graceful shutdown
- Crash analysis
- Logging improvements

---

# 10. Open Questions

- Should we allow MBD to continue in “isolated mode” if LIM is gone?
- Should LIM reject MBD registrations during cluster maintenance windows?
- Should supervisor heartbeats be tunable?

---

# 11. Conclusion

This redesign transforms LavaLite into a **clean, modern, deterministic scheduler architecture** where:

- LIM controls all resource management
- LIM controls all daemon lifecycle
- MBD becomes a supervised worker rather than the “cluster brain”
- Admin operations become predictable
- Restarts become safe
- No more sbatchd-driven madness
- No more accidental MBD resurrection loops

This design is compatible with:
- modern HPC practices
- minimal code
- maximal reliability
- and LavaLite’s “clean C, no bullshit” philosophy.

---
# Appendix A — Socketpair Example (C Skeleton)

```c
int sv[2];
socketpair(AF_UNIX, SOCK_SEQPACKET, 0, sv);

/* LIM keeps sv[0]
   passes sv[1] to mbatchd */
```

---

# Appendix B — Environment Passing

```
LAVALITE_SUP_FD=7
execve("/usr/lib/lavalite/mbatchd", ..., envp)
```

---

# Appendix C — Heartbeat Payload Format

```
struct mbd_heartbeat {
    uint32_t version;
    uint32_t state;
    uint64_t timestamp;
    uint32_t flags;
};
```
---
 # Pictures

                          +------------------------------+
                          |         HPC Clients          |
                          |   (bsub, lsload, lshosts)    |
                          +---------------+--------------+
                                          |
                                          |
                                          v
                               [ TCP / UDP Requests ]
                                          |
                                          v
+--------------------------------------------------------------------------+
|                              LIM (Master)                                |
|                                                                          |
|   +-------------------+          +------------------+                    |
|   | LIM_MBD_REGISTER | <-------> |  MBD Registration|                    |
|   +-------------------+          |  & Heartbeat FSM |                    |
|                                  +------------------+                    |
|                                                                          |
|   +----------------+     Supervisory Control     +---------------------+ |
|   |  LIM Policy   |  --------------------------> |   MBD State Table   | |
|   |   Engine      |                              |  (RUNNING/PAUSED/  | |
|   +----------------+                             |   DISABLED/DEAD)   | |
|                                                                          |
+--------------------------------------------------------------------------+
                                          |
                           Periodic Heartbeat + Status
                                          |
                                          v
+--------------------------------------------------------------------------+
|                               MBD (Job Scheduler)                        |
|                                                                          |
|   +-------------------+        +----------------------+                  |
|   | Supervisor Client |  <---- | LIM Supervision Port |                  |
|   | (heartbeat sender)|        |     (UDP loopback)   |                  |
|   +-------------------+        +----------------------+                  |
|                                                                          |
|        +----------------------+        +---------------------------+     |
|        | Scheduling Engine    |        |  SBD Communication Layer  |     |
|        +----------------------+        +---------------------------+     |
|                                                                          |
+--------------------------------------------------------------------------+
                                          |
                                          |
                                          v
                              +----------------------+
                              |      SBD (per host)  |
                              +----------------------+


---

End of Document.
