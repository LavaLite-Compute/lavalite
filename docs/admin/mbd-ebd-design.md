# LavaLite: mbd + ebd Design

## Principle

mbd schedules. ebd answers questions.

Two daemons, clear boundary, no overlap.

---

## mbd

**Responsibilities**

- Accept jobs (`bsub`)
- Schedule jobs onto hosts
- Kill jobs (`bkill`)
- Simple live queries: PEND+RUN job list, host status, queue status
- Write `job.events` append-only log

**Does not**

- Serve detailed job history
- Serve finished job records
- Read from sqlite
- Block on disk I/O for queries

**Wire operations served**

- `JOB_SUBMIT`
- `JOB_KILL`
- `JOB_INFO`      — live jobs only, short format, 80 columns
- `HOST_INFO`
- `QUEUE_INFO`

**State on disk**

```
var/work/mbd/
    job.events          # append-only event log
    jobs/BUCKET/job_id/ # per-job directory: submit file, script, rusage
```

---

## ebd (events daemon)

**Responsibilities**

- Tail `job.events` continuously
- Write parsed events into sqlite database
- Serve detailed job queries: `bjobs -l`, `bhist`, user/queue/time filters

**Must run on**

The mbd host. `job.events` and `jobs/` are local filesystem. ebd has direct read access, no network hop.

**Does not**

- Write to `job.events`
- Talk to mbd
- Modify any scheduler state

**sqlite schema (sketch)**

```sql
CREATE TABLE jobs (
    job_id       INTEGER PRIMARY KEY,
    user_id      INTEGER,
    queue        TEXT,
    from_host    TEXT,
    exec_host    TEXT,
    submit_time  INTEGER,
    start_time   INTEGER,
    end_time     INTEGER,
    status       INTEGER,
    exit_code    INTEGER,
    cwd          TEXT,
    name         TEXT
);

CREATE INDEX idx_user  ON jobs(user_id);
CREATE INDEX idx_queue ON jobs(queue);
CREATE INDEX idx_time  ON jobs(submit_time);
```

rusage written as a sidecar file beside the submit file, ebd reads it on job completion and stores it in a separate `rusage` table or as a blob column.

**Wire operations served**

- `JOB_DETAIL`   — full record for one job_id
- `JOB_HIST`     — query by user, queue, time range, status
- `JOB_RUSAGE`   — resource usage for a completed job

**ebd port**

Separate port from mbd. Clients connect directly. No proxy through mbd.

---

## Client tool mapping

| Tool        | Talks to | Notes                              |
|-------------|----------|------------------------------------|
| `bsub`      | mbd      | submit + optional wait             |
| `bkill`     | mbd      | synchronous                        |
| `bjobs`     | mbd      | PEND+RUN, short format, 80 columns |
| `bjobs -l`  | ebd      | full detail, any status            |
| `bhist`     | ebd      | historical queries                 |
| `bqueues`   | mbd      | queue status                       |
| `bhosts`    | mbd      | host status                        |

---

## job.events tail mechanism

ebd maintains a cursor: last byte offset read from `job.events`.
On each poll interval (1s) it reads new records from cursor forward.
On `job.events` rotation (compaction rename) ebd detects inode change,
reopens the file, resets cursor to 0.

No inotify dependency — plain stat + inode check is portable and simple.

---

## Open questions

- ebd restart recovery: replay `job.events` from beginning or checkpoint cursor to disk?
  Safest: checkpoint cursor + sqlite transaction. On crash replay from last checkpointed offset.
- rusage format: write as KEY=VALUE sidecar (consistent with submit sidecar) or binary XDR?
  KEY=VALUE is simpler, human readable, no versioning pain.
- ebd config: same `ll.conf` as mbd, separate port parameter `LL_EBD_PORT`.
