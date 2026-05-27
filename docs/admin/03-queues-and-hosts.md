# LavaLite Administrator's Guide — Queues and Hosts

## Managing queues

Open a closed queue:

```
bqueues --open queuename
```

Close a queue (stops dispatch, does not affect running jobs):

```
bqueues --close queuename
```

Display queue status and counters:

```
bqueues
```

## Managing hosts

Open a closed host:

```
bhosts --open hostname
```

Close a host (stops dispatch to that host, does not affect running jobs):

```
bhosts --close hostname
```

Display host status and resource usage:

```
bhosts
```

## Host groups

Host groups are defined in `llb.hosts` under `Begin HostGroup / End HostGroup`.
A queue is bound to a host group via the `HOSTS` parameter in `llb.queues`.
Jobs submitted to a queue are dispatched only to hosts in that group.

Display current group membership:

```
bgroups
```

## Token pools

Token pools gate job dispatch on a shared resource counter (e.g. software
licenses). Defined in `llb.hosts` under `Begin TokenPool / End TokenPool`.

Jobs request tokens with `bsub --pool name=N`. A job will not be dispatched
until N tokens are available.

Display current token pool usage:

```
btokens
```

## GPU scheduling

GPUs are defined per host in `llb.hosts` under `Begin Gpu / End Gpu`.
Jobs request GPUs with `bsub --gpus N` and optionally a specific type
with `bsub --gpu-type name`.

The `GPU_TYPE` field in `llb.hosts` is matched against `--gpu-type` at
dispatch time. Use `full` for a complete GPU device. For MIG instances,
use the MIG profile name (e.g. `3g.40gb`).
