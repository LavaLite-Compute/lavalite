# Things to Know

Behaviour that is by design, or known and not fixed yet. Read before
filing a test failure.

## Services

- **No local access to an endpoint.** The DNAT rule for `ext_port`
  lives in the `prerouting` chain, which only sees traffic arriving
  from another host. `curl host:ext_port` from the execution host
  itself gets nothing. Test from another host, or on the execution
  host use the netns address: `ip netns exec svc<N> ip addr`, then
  `curl 10.200.x.y:<PORT>`.

- **iproute2 and nft are required** on execution hosts. Without `ip`
  sbd logs only `namespace setup failed: No such file or directory`
  and rejects the dispatch.

- **Reject loop.** A dispatch rejected by `sbd` goes back to pending
  and is retried every scheduler tick, with no backoff. A broken host
  (missing `ip`, bad state dir) gets the same job every 2 seconds.
  Look at the `sbd` log, not at the job.

- **Endpoint changes on restart.** The job ID is the stable handle.
  After a relocation look the endpoint up again with `bservices`.

- **Routing beyond the login node** is the site's business. Users
  reach compute endpoints through `ssh -L` from the login node.

- **COMMAND is run by the job shell as is.** Runtime, image and binds
  are the site's. The environment has `HOME`, `USER`, `LL_SVC_PORT`
  and, when `TOKEN_ENV` is set, `LL_SVC_TOKEN` plus the `TOKEN_ENV`
  name. Do not put the token on the command line: `ps` shows argv to
  every user. Prefer an app that reads it from the environment.

- **Apptainer passes the environment** into the container. Other
  runtimes may not, and may create their own netns or cgroup; such a
  runtime needs flags in `COMMAND` and is not tested.

- **`bservices -a` shows a stale endpoint** for `FINISH` instances.

- **`bservices` shows `PEND` during the dispatch window**, `bjobs`
  shows `DISP` for the same job.

## Time limits

- **Two limits.** `--terminate` is an absolute time, enforced by
  `sbd`. `--time` / `RUNLIMIT` is a duration, enforced by `mbd`.

- **The run limit counts from the first start** and is never reset:
  not by a restart of the service, not by a requeue after an `sbd`
  failure. A batch job requeued by an infrastructure failure keeps
  counting too.

- **Precision is one scheduler tick** (`--sched_timer`, default 2s).
  Then one minute of grace between SIGTERM and SIGKILL.

- **Out of time while pending** means `EXIT` without dispatch. The
  check runs in the scheduler loop, which only runs when a host has
  free slots: on a full cluster the job stays `PEND` until a slot
  frees.

- **`bkill -s cont` on a held job** past its deadline releases it
  straight to `EXIT`.

## Display

- **`DISP` is not a state.** It is `RUN` without a pid: dispatched,
  not yet forked. `bjobs --run` includes it. `bhist` shows
  `Dispatched` without `Forked`.

- **`UNKNOWN`** is shown for `RUN`/`SUSP` jobs whose host `sbd` is
  disconnected, for example right after an `mbd` restart.

- **`bsub` prints `queue <default>`** when `--queue` is not given,
  even if the default queue has another name. `bjobs` shows the real
  queue.

## Environment

- **Without cgroup v2 job control falls back to the process group.**
  `sbd` implements SIGSTOP, SIGCONT and SIGKILL through the job cgroup
  (`cgroup.freeze`, `cgroup.kill`), which also reaches processes that
  left the job's process group. On a cgroup v1 host `sbd` logs
  `cgroup init failed, resource enforcement disabled`, memory and CPU
  are not enforced, and these signals go through `killpg` instead
  (`no cgroup, using killpg` in the `sbd` log). A process that called
  `setsid()` escapes them. On a hybrid host
  `LL_CGROUP_ROOT=/sys/fs/cgroup/unified/lavalite` gives freeze and
  kill, still without memory and CPU limits.

- **A failed kill is not reported to the user.** `bkill` and
  `bservices -d` return as soon as `mbd` queued the signal. If `sbd`
  fails it, only the `mbd` and `sbd` logs say so, and a deleted
  service instance stays `RUN` (no longer restartable).

- **Per-host state.** `sbd` state directories must be local to each
  host, not shared over NFS.

- **`make install` resets permissions.** Run `post-install.sh` after
  every install, or `bhist` fails with `EACCES` on the manifest.
