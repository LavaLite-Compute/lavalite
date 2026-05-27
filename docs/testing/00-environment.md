# LavaLite Test Environment

## Purpose

This document describes the test environment and conventions used across
all LavaLite manual test plans.

## Cluster requirements

- One master host running `mbd`
- One or more execution hosts running `sbd`
- LavaLite commands installed: `bsub`, `bjobs`, `bkill`, `bqueues`,
  `bhosts`, `bgroup`, `btokens`, `bhist`
- At least one queue configured (e.g. `cpu`)
- At least one host configured (e.g. `buntu24`)
- `LL_LOG_MASK=LOG_DEBUG` in `ll.conf` during testing
- `mbd_assert_counters()` enabled in mbd build

## Useful paths

Set these for your environment:

```sh
export LL_SYSEVENTS=/opt/lavalite/var/state/mbd/eventlog
export LL_LOG_DIR=/opt/lavalite/var/log
```

## Useful commands

```sh
bjobs -a                        # all jobs all states
bqueues                         # queue counters
bhosts                          # host counters
tail -f $LL_LOG_DIR/mbd.log     # mbd log
```

## Starting and stopping daemons

```sh
systemctl start lavalite-mbd
systemctl stop  lavalite-mbd
systemctl start lavalite-sbd
systemctl stop  lavalite-sbd
```

Or directly during testing:

```sh
/opt/lavalite/sbin/mbd
/opt/lavalite/sbin/sbd
```

## Job commands used in tests

Long-running job (stays in RUN for inspection):

```sh
bsub sleep 86400
```

Short job (completes quickly):

```sh
bsub sleep 5
```

Failing job (produces EXIT state):

```sh
bsub /bin/false
```

## General validation rules

After every test step verify:

1. `bjobs -a` — job state is correct
2. `bqueues` — queue counters are correct
3. `bhosts` — host counters are correct where applicable
4. `$LL_SYSEVENTS` — expected events are present
5. No negative counters anywhere
6. No `mbd_assert_counters()` failure in mbd log

## Counter invariants

These must hold at all times:

```
host.num_jobs  >= 0
host.num_run   >= 0
host.num_susp  >= 0

queue.num_jobs >= 0
queue.num_pend >= 0
queue.num_held >= 0
queue.num_run  >= 0
queue.num_susp >= 0
```

## Cleanup between tests

```sh
bkill <jobid>
```

Wait for job to reach DONE or EXIT, then verify counters are back to
baseline before starting the next test.

## Recording results

For each test record:

- PASS / FAIL
- Any unexpected behavior
- eventlog excerpt if relevant
- mbd log excerpt on failure
