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

- **Rejecting host.** A dispatch rejected by `sbd` goes back to
  pending and the host is tainted for 60 seconds, `ok|tainted` in
  `bhosts`. A broken host (missing `ip`, bad state dir) gets one job a
  minute, not one every tick. Look at the `sbd` log, not at the job.

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

- **Apptainer binds only `$HOME`, `/tmp` and the cwd.** A shared area
  like `/export` is invisible inside the container, and the job fails
  with `No such file or directory` although the file exists on the
  host. Two ways, both site configuration:
  - `bind path = /export` in `/etc/apptainer/apptainer.conf` on every
    execution host: all containers see it, LavaLite or not;
  - `--bind /export` in the service `COMMAND`: only that service.

- **An instance keeps the `COMMAND` it was started with.** A change in
  `llb.services` applies to new instances only; delete and start the
  existing ones again.

- **`QUEUE` is mandatory for a service.** A block with an error
  (missing `COMMAND`/`QUEUE`/`PORT`, unknown queue or key) is not
  configured; the `mbd` log says why, the other services load.

- **Removing a service with live instances** makes them `ORPHAN` in
  `bjobs`: frozen, still charged on their hosts, not listed by
  `bservices`. Their processes keep running. Only `bkill -s kill` ends
  an orphan. Restoring the block heals them, same endpoint and token
  (TEST-248). Delete the instances (`bservices -d`) before removing
  the service.

- **`RESTART` belongs to the instance.** It is fixed when the
  instance is created and kept across `mbd` restarts. Changing
  `RESTART` in `llb.services` affects new instances only; instances
  already pending or running keep the policy they were started with.

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

- **`bsub` without `--queue`** prints `is submitted to the default
  queue`: the queue name is not in the submit reply. `bjobs` shows it.

## Configuration

- **mbd stays up on errors in services and queue membership** and
  creates all it can: a bad service is skipped, an unknown host in a
  queue is skipped, a queue with no valid host stays defined and
  schedules nothing. A required `ll.conf` parameter that is missing
  (`LL_STATE_DIR`, `LL_MBD_PORT`, `LL_MBD_HOST`, `LL_MBD_USER`,
  `LL_DEFAULT_QUEUE`), a `LL_DEFAULT_QUEUE` that names no queue, a
  port in use, or a syntax error in `llb.hosts` or `llb.queues` still
  stops `mbd`: the reason is in the `mbd` log. After a restart check
  the line `hosts=N queues=N groups=N services=N services_skipped=N`
  in the `mbd` log.

- **`HOSTS = all`** in `llb.queues` means every host in `llb.hosts`.
  `all` is reserved: a host, sim or HostGroup named `all` stops `mbd`.
  A queue with no `HOSTS` line stops `mbd` too; there is no implicit
  all.

- **Lowering capacity drains a host or a token pool.** Lowering
  `CPU`, `MEM`, `STORAGE`, the GPUs or a pool's `AVAILABLE` below what
  running jobs use is legal: the jobs keep running, `bhosts -l` shows
  used above total, `btokens` shows a negative `FREE`, and nothing new
  is dispatched until it fits again.

- **GPU ids are counted, not tracked per job.** After a GPU count is
  lowered under running jobs the free count is right only once the
  host has drained.

## Environment

- **sbd detects a dead link to mbd within about 90 seconds.** `sbd`
  sends nothing while idle; TCP keepalive on its socket to `mbd`
  (probes after 60s of silence, 3 every 10s) and a 90s
  `TCP_USER_TIMEOUT` for unacknowledged data close a dead connection,
  and `sbd` reconnects. A hung `mbd` whose kernel still answers is not
  detected this way.

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
