# LavaLite 0.9.0 Release Notes

**Tag:** `lavalite-0.9.0`
**Previous release:** `lavalite-0.1.1`
**Status:** Beta

## What's new since 0.1.1

- cgroup v2 resource collection at job finish (mem, swap, cpu time)
- `bhist` command — job history from event log and sidecar files
- Complete man page set for all commands, daemons, and config files
- Administrator's guide
- Service files updated: `LSF_ENVDIR` replaced by `LL_CONF_DIR`
- LIM decoupled from the scheduler — no longer a dependency for job dispatch

## What works

- Job submission, dispatch, execution, and completion
- **Parallel jobs** — first-class support via `--cpus` and `--nhosts`;
  MPI-ready with `LL_HOSTS` and `LL_FIRST_HOST` set in the job environment
- **GPU scheduling** — first-class support for GPU and MIG instances;
  typed GPU matching via `--gpu-type`
- cgroup v2 resource enforcement (memory, CPU) and usage reporting
- Token pool gating for floating license resources
- Job signals and suspension
- mbd/sbd restart and recovery without job loss
- Event log compaction
- HMAC-SHA256 authentication between mbd and sbd
- Full command set: `bsub`, `bjobs`, `bkill`, `bqueues`, `bhosts`,
  `btokens`, `bgroup`, `bhist`

## Known limitations

- `bhist` requires `LL_STATE_DIR` accessible from the front-end node
- Job dependencies (`bsub --dependency`) not yet implemented
- `--begin` and `--terminate` implemented but not fully tested
- LIM builds and installs but is not integrated into the scheduler

## Upgrade from 0.1.1

No migration tool. Deploy fresh. The event log format has changed and
is not backward compatible with 0.1.1.
