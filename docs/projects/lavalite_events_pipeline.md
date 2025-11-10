# LavaLite Event Stream Pipeline

## Overview
LavaLite’s scheduler daemon is **single-threaded and asynchronous**.
It logs every state transition to a durable, append-only journal file called
`lsb.events`. This log is the authoritative timeline of all cluster activity:
job submissions, starts, completions, host state changes, configuration reloads,
and internal signals.

Rather than exposing REST endpoints from inside the scheduler, LavaLite keeps the
core daemon focused on dispatch and queue management.  External components read
`lsb.events` in real time and publish updates to stream consumers.
This separation ensures that monitoring and visualization never interfere with
the scheduler’s critical path.


clients
│
▼
┌──────────────────────────┐
│ LavaLite daemon │
│ (async, single thread) │
└────────────┬─────────────┘
│
▼
lsb.events
(append-only log)
│
▼
┌─────────────────────┐
│ Loader / Tailer │
│ reads log, emits │
│ structured events │
└─────────────────────┘
│
▼
Streaming Backend
(Redis Streams / NATS / etc.)
│
▼
┌─────────────────────┐
│ Consumers │
│ • Grafana exporter │
│ • REST gateway │
│ • CLI / analytics │
└─────────────────────┘

---

## 1. The `lsb.events` File
Each event is written as one line of JSON or a compact binary record.
Example (JSON Lines format):

```json
{"seq":20142,"ts":"2025-10-31T13:44:09Z","type":"JOB_START","jobid":4242,"host":"nodeA"}
{"seq":20143,"ts":"2025-10-31T13:45:12Z","type":"JOB_DONE","jobid":4242,"status":0}
