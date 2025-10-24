# LavaLite LIM Protocol

The **LavaLite Load Information Manager (LIM)** protocol provides lightweight,
one-way telemetry from slave nodes to a single cluster master.
It replaces legacy multi-flag designs with a compact binary format that scales
linearly to tens of thousands of nodes.

---

## 1. Overview

| Aspect | Description |
|---------|--------------|
| **Purpose** | Report per-host load and static metrics to the master |
| **Direction** | Slave → Master (no return traffic) |
| **Transport** | UDP (preferred), TCP optional |
| **Architecture** | Master is the root of the cluster; all queries from `mbatchd`, CLI tools, etc. are answered by the master using its in-memory state |
| **Philosophy** | *KISS — one header, one message type, no chatter* |

---

## 2. Message Header

### 2.1 Master operation

The master broadcasts a single announce packet (`LIM_MASTER_ANN`) at regular
intervals (default every 4 s). The announce may include a compact list of
active hosts so that other masters or late-joining slaves can synchronize
their view.

All packets start with a fixed 12-byte header aligned on 4-byte boundaries.

| Field | Type | Bytes | Description |
|--------|------|--------|-------------|
| `op_code` | `uint32_t` | 4 | Operation code (`LIM_SLAVE_REPORT`) |
| `len` | `uint32_t` | 4 | Payload length in bytes |
| `extra` | `uint32_t` | 4 | Reserved / flags / sequence (0 for now) |

```c
/* Operation codes used by both master and slave
 */
enum lim_opcode {
    LIM_MASTER_ANN   = 10,  /* Master → slaves: announce / host window   */
    LIM_SLAVE_REPORT = 11,  /* Slave → master: periodic load report      */
    LIM_MASTER_SYNC  = 12,  /* Master ↔ master: optional sync (future)   */
    LIM_ACK          = 13   /* Generic ACK (optional)                    */
};
struct lim_hdr {
    uint32_t op_code;   /* LIM_SLAVE_REPORT only for telemetry          */
    uint32_t len;       /* payload bytes following header               */
    uint32_t extra;     /* reserved / flags / sequence (0 for now)      */
};
```

### 2.2 Slave operation

Each slave sends one compact payload describing its current state. The slave
uses its own interval plust a jitter to avoid byte floods at the master.

```c
#define LIM_SLAVE_REPORT  11u
struct lim_slave_payload {
    double    vec[11];   /* load1, load5, load15,
                            mem_used, mem_free,
                            swap_used, swap_free,
                            ncpus, cpu_user, cpu_sys, cpu_idle */
    uint32_t  stats[4];  /* jobs_running, jobs_pending,
                            procs_total, flags */
};
```

## 3. Security

Management network should be trusted or firewalled.
Optional 32-byte token or HMAC is prepanded to the payload, The external
authentication mechanism is used with signatures and encryption
Oversized or malformed packets are silently dropped.

## 5. Poisson process

Unlike systems that coordinate synchronized heartbeats, LavaLite LIM uses
independent timers on each node. Because every daemon starts at a slightly
different moment and applies a small, stable jitter, the aggregate stream of
telemetry packets arriving at the master naturally forms a Poisson-like
process — smooth, statistically uniform, and self-balancing over time.

This means there are no traffic spikes, no “thundering herd” after a master
switch, and no need for global coordination.
Each node behaves as an independent agent; together, they produce a perfectly
steady flow of updates that scales effortlessly to tens of thousands of hosts.

In marketing terms: the cluster “just breathes”—steady, graceful, and
predictable—no central clock, no synchronization storms, just autonomous
agents converging fast.
