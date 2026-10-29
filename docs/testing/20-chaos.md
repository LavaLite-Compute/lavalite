# Chaos

## Purpose

Run the cluster under load while daemons are killed, frozen and
stopped, and check that every job and service instance reaches its
end state and `mbd` never fails on its own.

The work is split by role, each program in its own window:

- `jload`, run by ordinary users: submits jobs, arrays and
  dependencies, bkills some of its own jobs, then waits until every
  job it submitted is `DONE` or `EXIT`.
- `sload`, run by ordinary users: starts service instances with a run
  limit, checks their endpoints, deletes some, abandons some starts,
  then waits until every instance is `FINISH`.
- `chaos`, run by the administrator or root: submits nothing; kills,
  freezes and stops `mbd` and `sbd`, then checks that every daemon is
  up and that `mbd` did not restart, assert or log `bad counters` on
  its own.

Users judge their own work, chaos judges the daemons. `jload` and
`sload` also run on their own, without `chaos`, as plain load. A failure in
`jload` or `sload` is matched to what `chaos` did at that time through
the timestamps in the three logs.

The programs are in `src/tests/chaos`.

## Prerequisites

- All daemons run under systemd with `Restart=on-failure`, sims
  included: chaos only kills, systemd brings them back.
- `LL_ASSERT_COUNTERS=1` in `ll.conf` for the run.
- The administrator has passwordless `sudo` and ssh keys to every host
  in the hosts file.
- `LL_CONF_DIR` set in every window.
- For `sload`: a service in `llb.services`, `echotest` by default.
  Run `sload` on the login node, a host not in the service queue: see
  Services in the administrator guide. On an execution host the
  endpoint check is skipped for instances running there.
- One `sload` per user and service: its final check looks at all
  instances of the user.

## TEST-280: Chaos run

### Commands

Hosts file, see `src/tests/chaos/hosts`:

```
mbd buntu24
sbd worker4
sbd worker5
```

Window 1, dario:

```sh
jload --seconds 1800 --submit-every 2 --array-every 10 --dep-every 7 \
      --bkill-every 15
```

Window 2, maria:

```sh
jload --seconds 1800 --submit-every 3 --max-cpus 2 --max-nhosts 2 \
      --max-gpus 1
```

Window 3, sofia:

```sh
sload --service echotest --seconds 1800 --time 10
```

Window 4, administrator:

```sh
chaos --hosts-file hosts --seconds 1800 --kill-every 60 \
      --freeze-every 90 --freeze-secs 40 --stop-every 300
```

Start all four within a minute. `jload` and `sload` then wait for
their jobs and instances, `sload` at most its run limit.

### Expected Result

- Every program ends with `result=ok` and exit status 0.
- `chaos` reports `mbd_unexpected_restarts=0`.
- `bhosts` shows every host `ok`, `bjobs -u all` no `UNKNOWN` job.

### Pass Criteria

No job or instance is lost or stuck, and `mbd` fails only when chaos
kills it.

## Notes

- A failed `bsub` or `bservices` while `mbd` restarts is counted, not
  judged: `bsub_fail` and `start_fail` in the final line.
- `--freeze-secs` above 20 lets the pid timeout disconnect the frozen
  `sbd`, see `18-robustness.md`.
- `pchaos` tests pending job replay with an `mbd` of its own and no
  `sbd`; run it alone, not during a chaos run.
