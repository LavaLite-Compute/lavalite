# LavaCore LIM Protocol

The **LavaCore Load Information Manager (LIM)** protocol provides
lightweight, connectionless, UDP-based telemetry between cluster nodes.

The design is intentionally minimal:

- One fixed header
- Two message types
- No ACK chatter
- Timer-based liveness detection
- Fully in-memory state

This document reflects the current multi-host MVP implementation.

---

## 1. Transport

| Aspect | Description |
|--------|-------------|
| Transport | UDP |
| Topology | 1 Master + N Slaves |
| Model | Master beacon + Slave load reporting |
| Failure Detection | Timer-based (3 missed intervals) |

---

## 2. Protocol Header

All messages begin with a fixed-size header.

```c
struct packet_header {
    int32_t sequence;   // request/response correlation
    int32_t operation;  // message type / opcode
    int32_t version;    // e.g. 0x00010000
    int32_t length;     // payload bytes
    int32_t reserved;   // future use (flags / error)
};
```

### 2.1 Operation Codes

```c
enum lim_opcode {
    LIM_MASTER_BEACON,  // Master → slaves
    LIM_LOAD_REPORT     // Slave → master
};
```

---

## 3. Master Beacon (LIM_MASTER_BEACON)

The master periodically broadcasts a beacon on the LIM UDP port.

Purpose:

- Advertise cluster identity
- Advertise master hostname
- Allow slaves to learn active master
- Reset slave-side inactivity timer

### 3.1 Beacon Payload

```c
struct master_beacon {
    char     cluster[LL_BUFSIZ_32];
    char     hostname[MAXHOSTNAMELEN];
    uint32_t hostNo;
    uint32_t seqno;
    uint16_t tcp_port;
};
```

### Fields

| Field | Description |
|-------|-------------|
| cluster | Cluster name |
| hostname | Master hostname |
| hostNo | Master host index |
| seqno | Monotonic master sequence |
| tcp_port | Future TCP control channel |

### Slave Behavior

On receiving beacon:

- Record master hostname/IP
- Store `hostNo`
- Reset `master_last_seen`
- Enter normal reporting mode

If 3 consecutive beacons are missed:

- Mark master invalid
- Stop sending load reports
- Wait for next valid beacon

No ACK is sent.

---

## 4. Slave Load Report (LIM_LOAD_REPORT)

Each slave periodically sends its load information to the master.

Purpose:

- Update host load indices
- Update host status
- Maintain cluster scheduling state

### 4.1 Load Payload

```c
struct wire_load_update {
    uint32_t hostNo;
    uint32_t seqNo;
    uint32_t status0;
    uint32_t nidx;
    float    li[LIM_NIDX];
};
```

### Fields

| Field | Description |
|-------|-------------|
| hostNo | Host index |
| seqNo | Monotonic host sequence |
| status0 | Host status flags |
| nidx | Number of valid load indices |
| li[] | Load index array |

---

## 5. Built-In Load Indexes

Historically there are 11 built-in load indices:

| Index | Name |
|-------|------|
| 0 | r15s |
| 1 | r1m |
| 2 | r15m |
| 3 | ut |
| 4 | pg |
| 5 | io |
| 6 | ls |
| 7 | it |
| 8 | tmp |
| 9 | swp |
| 10 | mem |

Where:

- `r15s`, `r1m`, `r15m` = load averages
- `ut` = CPU utilization
- `pg` = paging rate
- `io` = disk I/O rate
- `ls` = login sessions
- `it` = idle time
- `tmp` = tmp space
- `swp` = swap space
- `mem` = memory availability

`LIM_NIDX` currently equals 11.

---

## 6. Host Status

`status0` carries host state flags.

Current semantics:

- `LIM_UNAVAIL == 0` → Host OK
- Non-zero values represent unavailable or degraded states

Master updates host scheduling eligibility based on `status0`
and inactivity detection.

---

## 7. Inactivity Detection

### 7.1 Master Side

Master tracks last load update per host.

If 3 consecutive load reports are missed:

- Host marked unavailable
- Host removed from scheduling pool

No explicit disconnect required.

---

### 7.2 Slave Side

Slave tracks last received beacon.

If 3 consecutive beacons are missed:

- Master marked invalid
- Slave stops reporting
- Waits for next valid beacon

---

## 8. Protocol Exchange Diagram

```
                    (UDP)

             +------------------+
             |      MASTER      |
             +------------------+
                      |
                      |  LIM_MASTER_BEACON
                      |----------------------------------->
                      |
             <-----------------------------------
                      |  LIM_LOAD_REPORT
                      |
             +------------------+
             |      SLAVE       |
             +------------------+
```

Detailed timeline:

```
MASTER SIDE                          SLAVE SIDE
-----------                          -----------

every 4s:
send BEACON  --------------------->   receive BEACON
                                      reset master timer

                                      every T seconds:
                                      send LOAD_REPORT  -------->

receive LOAD_REPORT
update host->loadIndex[]
update host->status0
reset host timer

if 3 missed LOAD_REPORT:
    mark host UNAVAIL

                                      if 3 missed BEACON:
                                          master INVALID
                                          stop reporting
```

---

## 9. Design Characteristics

- UDP only
- Fixed header
- Versioned protocol
- No ACKs
- No persistent connections
- Timer-based liveness
- Fully in-memory master state

The system converges automatically without coordination storms.

---

## 10. Current MVP State

Working features:

- Multi-host UDP exchange
- Master beacon
- Slave master discovery
- Slave load reporting
- Master load aggregation
- Host inactivity detection
- Master inactivity detection
- Verified via `lsload`

Not yet implemented:

- Master election
- Quorum logic
- Split-brain prevention
- Persistent cluster state

This implementation establishes the distributed LIM baseline
for LavaCore.
