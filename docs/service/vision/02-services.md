# E4 LavaLite Service Jobs

A service is not a separate orchestration system. It is an HPC scheduler
object with a public interface inside the cluster.

## Job model

- New field on the job record: `job_class` = `BATCH` | `SERVICE`.
- Batch state machine unchanged: `PEND -> RUN -> DONE/EXIT`.
- Service state machine: `PEND -> RUN -> EXIT (restart) -> RUN -> ... -> DEAD`.
  Separate table from batch. Dispatch keyed on `job_class`, one branch,
  in the exit handler.
- Restart on exit: sbd reports `EXIT` to mbd, same as today. mbd holds
  the restart policy (`max_restarts`, static backoff table: 1s, 5s, 30s)
  and tells sbd when to re-exec, same node. sbd is the executor, not the
  decision-maker — mbd is the controller for this, same as it is for
  everything else.
- Submission: `bsvc submit`, own verb. `bsub` untouched. Shares job
  struct, wire protocol, manifest format.

## Replication

N replicas = N array-job entries, existing mechanism, spread across N
nodes. Each replica is a peer. Node dies -> replica becomes an ordinary
`EXIT`'d job. No detection, no reschedule. Pool degrades to N-1 until
resubmitted by a human or cron.

## Discovery

New manifest events, same log `bhist` reads:

- `SVC_START` — name, host, port, job_id
- `SVC_DEAD`  — name, host, port, job_id

mbd writes these, and as a direct side effect rewrites one ascii file,
`mbd.services`, on shared storage (NFS):

```
# service      sbd:port
webapp         node3:8080
webapp         node7:8080
pytorch-svc    node5:9001
r-svc          node9:8000
```

One line per live replica. Different backend under a different name is
just a different line — no tagging, no selection logic. Rewrite is
tmp-file + rename, atomic, no partial reads.

Client: read file, filter by name, try `host:port` top to bottom, short
connect timeout, first ACK wins. Dead entry -> connect fails -> next
line. All entries dead -> outage, no retry logic beyond that.

## New surface

- 1 field (`job_class`)
- 1 verb (`bsvc submit`)
- 2 manifest events (`SVC_START`, `SVC_DEAD`)
- 1 restart-backoff table in sbd
- 1 dispatch branch in mbd's exit handler
- 1 derived file (`mbd.services`)

## First workload

`python3 -m http.server` (or one-route Flask) as a `SERVICE` job, 1+
replicas, no restart policy yet. Flow: read `mbd.services` -> connect
-> GET -> page. Proves `job_class`, manifest events, file handoff. R
and Jupyter come after this works.
