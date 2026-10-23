# bservices Tests

## Purpose

Verify service instances: start, endpoint, delete, restart
(relocation), token, run limit, and recovery.

A service instance is an ordinary job with `JOB_FLAG_SERVICE`. Its
job ID is the stable handle, the endpoint `host:port` changes when the
instance restarts on another host.

## Prerequisites

- Environment validation completed.
- The test user runs `bservices` from a host that is *not* the
  execution host (see `90-things-to-know.md`, local access).
- `ip` (iproute2) and `nft` installed on the execution hosts.
- `llb.services` contains the echo service from
  `src/tests/services/`:

```
Begin Service
SERVICE_NAME = echotest
TYPE         = user
COMMAND      = apptainer exec /export/service_images/python-slim.sif python3 <path>/echo_server.py $LL_SVC_PORT
PORT         = 8888
TOKEN_ENV    = ECHO_TOKEN
RESTART      = always
RUNLIMIT     = 0:10
End Service
```

`$LL_SVC_PORT` is expanded by the job shell to `PORT`. `RUNLIMIT`
of 10 minutes keeps forgotten instances from living forever.

## TEST-200: List configured services

### Commands

```sh
bservices
```

### Expected Result

`echotest` is listed with its queue and `always`, no instances.

### Pass Criteria

Every `Begin Service` block is listed.

---

## TEST-201: Start an instance

### Commands

```sh
bservices -s echotest
```

### Expected Result

The command blocks until the instance runs, then prints:

```text
Job <N> http://<host>:<port>/?token=<token>
```

### Pass Criteria

- `bservices` shows the instance `RUN` with `HOST:PORT` and token.
- `bjobs <N>` shows `RUN`, job name `echotest`.

---

## TEST-202: Reach the endpoint

### Commands

From a host other than the execution host:

```sh
curl -d hello http://<host>:<port>/anything
```

### Expected Result

The request is echoed back.

### Pass Criteria

The body `hello` is in the reply.

---

## TEST-203: Job environment

### Commands

On the execution host:

```sh
ps -ef | grep echo_server
ip netns list
```

### Expected Result

- The server runs with port `8888`, `$LL_SVC_PORT` was expanded.
- A netns `svc<N>` exists.

### Pass Criteria

The app listens on `PORT` inside `svc<N>`.

---

## TEST-204: bkill refuses a service job

### Commands

```sh
bkill <N>
```

### Expected Result

The command fails with `bkill: job <N> is a service, use bservices -d N`.

### Pass Criteria

The instance is still `RUN`.

---

## TEST-205: Delete by job ID

### Commands

```sh
bservices -d <N>
bservices -a
bjobs -a <N>
```

On the execution host:

```sh
ip netns list
nft list tables
```

### Expected Result

- `Service <N> is being deleted`.
- Instance `FINISH`, job `EXIT`.
- No `svc<N>` netns and no `svc<N>` nft table left.

### Pass Criteria

The instance is gone and the host is clean.

---

## TEST-206: Delete by endpoint

### Commands

Start a new instance, then:

```sh
bservices -d <host>:<port>
```

Start another one, then:

```sh
bservices -d http://<host>:<port>
```

### Expected Result

Both deletions succeed.

### Pass Criteria

Both instances end `FINISH`.

---

## TEST-207: Delete a pending instance

### Setup

Administrator closes the service queue:

```sh
bqueues --close <queue>
```

### Commands

Terminal 1:

```sh
bservices -s echotest
```

Terminal 2:

```sh
bservices
bservices -d <N>
```

### Expected Result

- The instance is `PEND`.
- After the delete, terminal 1 returns `Operation canceled`.

### Pass Criteria

A pending instance can be deleted by job ID and the waiting client is
released.

### Cleanup

```sh
bqueues --open <queue>
```

---

## TEST-208: Start blocks while the queue is busy

### Setup

Close the queue as in TEST-207.

### Commands

```sh
bservices -s echotest
```

Wait at least one minute, then open the queue.

### Expected Result

The command keeps waiting and prints the endpoint once the instance
runs. It does not fail with `Connection timed out`.

### Pass Criteria

The client waits as long as the queue does. `^C` interrupts it.

---

## TEST-209: Restart after a crash (relocation)

### Commands

Start an instance, then on the execution host kill the app:

```sh
pkill -u <user> -f echo_server
```

Then:

```sh
bservices
bhist -l <N>
```

### Expected Result

- The instance goes `PEND`, then `RUN` again, possibly on another
  host and port.
- Same job ID, same token.
- `bhist -l` shows a second `Dispatched` and `Forked`.

### Pass Criteria

The service restarts under the same job ID.

---

## TEST-210: RESTART=never

### Setup

A copy of `echotest` named `echoonce` with `RESTART = never`.

### Commands

Start it with `bservices -s echoonce`, kill the app as in TEST-209, then `bservices -a`.

### Expected Result

The instance ends `FINISH`, no new dispatch.

### Pass Criteria

`bhist -l` has one `Dispatched`.

---

## TEST-211: Token visibility

### Commands

As the owner and as another user:

```sh
bservices
```

### Expected Result

- The owner sees the token.
- Another user sees `-` in the `TOKEN` column.

### Pass Criteria

The token is shown only to its owner.

---

## TEST-212: Ownership on delete

### Commands

As another user:

```sh
bservices -d <N>
```

As the administrator:

```sh
bservices -d <N>
```

### Expected Result

- Another user: `Operation not permitted`.
- Administrator: deleted.

### Pass Criteria

Only the owner or a manager can delete.

---

## TEST-213: Run limit

### Commands

```sh
bservices -s echotest --time 1
```

Wait about 70 seconds.

```sh
bservices -a
bhist -l <N>
```

### Expected Result

- The instance is signaled at one minute and ends `FINISH`.
- It is *not* restarted although `RESTART = always`.
- `bhist -l` shows `Run limit:    0:01`, one `Dispatched`.

### Pass Criteria

The run limit stops the service for good.

---

## TEST-214: RUNLIMIT is a cap

### Commands

```sh
bservices -s echotest --time 20
bservices -s echotest
```

### Expected Result

- `--time 20` above `RUNLIMIT = 0:10` fails with `Invalid argument`.
- Without `--time` the instance gets `Run limit:    0:10` in
  `bhist -l`.

### Pass Criteria

The user may only ask for less than `RUNLIMIT`.

---

## TEST-215: Restart does not reset the clock

### Commands

```sh
bservices -s echotest --time 3
```

After one minute kill the app as in TEST-209. Let it restart.

### Expected Result

The instance is stopped about 3 minutes after its *first* `Forked`
event, and not restarted.

### Pass Criteria

Signal time minus first fork time is about 180 seconds.

---

## TEST-216: mbd restart with a running instance

### Commands

Start an instance, restart `mbd`, then:

```sh
bservices
bjobs <N>
```

### Expected Result

- `bjobs` may show `UNKNOWN` until `sbd` reconnects, then `RUN`.
- Same endpoint and token, `curl` still works.

### Pass Criteria

The instance survives the `mbd` restart.

---

## TEST-217: Unknown service and bad arguments

### Commands

```sh
bservices -s nosuchservice
bservices -d 99999999
bservices -d banana
bservices -s echotest --time abc
```

### Expected Result

Each command fails with a useful message and non-zero status.

### Pass Criteria

No instance is created.

---

## TEST-218: Interrupt a pending start

### Setup

Close the service queue as in TEST-207.

### Commands

```sh
bservices -s echotest
```

Press `^C` while it waits, then:

```sh
bservices -a
bhist -l <N>
```

The job ID is in `bservices -a` or `bjobs -a`.

### Expected Result

- The instance is `FINISH`, `bhist -l` says `Never dispatched`.
- The `mbd` log has `client gone while pending, deleting`.

### Pass Criteria

Leaving before the endpoint withdraws the request, like `bkill` on a
pending job.

### Cleanup

```sh
bqueues --open <queue>
```

---

## TEST-219: Show without starting

### Commands

```sh
bservices echotest
bservices echotest nosuchservice
bservices -l
bservices -l echotest
bqueues <queue>
bhosts <host>
```

### Expected Result

- `bservices echotest` shows the service and its instances, it does
  not start one: `bjobs` has no new job.
- `-l` prints one block per service with its definition: queue,
  type, port, restart, run limit, token env, command, instance counts.
  No instance rows.
- An unknown name prints `no such service` and the exit status is
  non-zero; the known names are still shown.

### Pass Criteria

A name selects what is shown, as in `bqueues` and `bhosts`. Only `-s`
starts an instance.

---

## TEST-220: Bad option combinations

### Commands

```sh
bservices -s echotest -d 1
bservices -t 5
bservices -s echotest otherservice
bservices -d 1 -l
```

### Expected Result

Each prints the usage and exits non-zero. No instance is started or
deleted.

### Pass Criteria

One action per command: show, start or delete.

---

## TEST-221: DISP

### Setup

Root on the execution hosts of the service queue. Freeze `sbd` on all
of them, as in TEST-196:

```sh
kill -STOP $(pidof sbd)
```

### Commands

Terminal 1:

```sh
bservices -s echotest
```

Terminal 2, after a few seconds:

```sh
bservices
bjobs <N>
```

Then thaw `sbd` (`kill -CONT`).

### Expected Result

- While `sbd` is stopped `bservices` and `bjobs` both show `DISP`.
- After `SIGCONT` terminal 1 prints the endpoint, both show `RUN`.

### Pass Criteria

`bservices` and `bjobs` agree on the state.

## Completion Criteria

The `bservices` test set passes when:

- Services are listed, started and reachable.
- Instances can be deleted by job ID, endpoint, also while pending.
- `bkill` refuses service jobs.
- A crashed instance restarts under the same job ID and token,
  unless `RESTART = never`.
- Tokens are shown only to their owner, deletes are owner or manager.
- Run limits stop a service for good and survive restarts.
- An instance survives an `mbd` restart.
- `^C` on a pending start deletes the instance.
- A name only shows a service, `-s` starts, `-l` shows the definition.
- `bservices` shows `DISP` as `bjobs` does.
