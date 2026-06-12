# LavaLite Administrator Guide — Configuration

## Overview

LavaLite configuration is stored in three files:

```text
ll.conf
llb.queues
llb.hosts
```

These files reside under:

```text
$LL_CONF_DIR
```

All daemons and commands read these files during startup.

Example configuration files are provided in:

```text
<srcdir>/etc
```

Administrators are expected to customize them for their site.

## LL_CONF_DIR

The `LL_CONF_DIR` environment variable is **mandatory**.

All LavaLite components use this variable during startup to locate the
cluster configuration.

This includes:

```text
mbd
sbd

bsub
bjobs
bhist
bkill
bmove
bpriority
bhosts
bqueues
bgroups
btokens

applications linked against libllbatch
```

The directory referenced by `LL_CONF_DIR` contains:

```text
ll.conf
llb.queues
llb.hosts
auth.key
```

If `LL_CONF_DIR` is not defined, LavaLite components cannot establish
their configuration baseline and will refuse to start.

Example:

```sh
export LL_CONF_DIR=/opt/lavalite/etc
```

Many installations configure this variable through an environment
module, but any mechanism is acceptable provided the variable is present
in the process environment before a LavaLite command or daemon is
started.


## Configuration Workflow

A typical deployment follows this process:

1. Configure cluster-wide settings in `ll.conf`.
2. Define execution hosts in `llb.hosts`.
3. Define queues in `llb.queues`.
4. Start or restart `mbd`.
5. Verify configuration using:

```sh
bhosts
bqueues
```

## ll.conf

`ll.conf` is the cluster environment file.

It is a Bourne shell style script sourced by all LavaLite commands and
daemons during startup.

Typical settings include:

- Cluster name
- Configuration directory
- State directory
- Log directory
- Network ports
- Default queue
- Logging configuration

Example:

```sh
LL_CLUSTER_NAME=lavalite
LL_STATE_DIR=/opt/lavalite/var/state
LL_CONF_DIR=/opt/lavalite/etc
LL_LOG_DIR=/opt/lavalite/var/log
```

See:

```text
man 5 ll.conf
```

for the complete parameter reference.

## llb.hosts

`llb.hosts` describes the resources available to the scheduler.

The file is used to define:

- Execution hosts
- GPU devices
- Token pools
- Host groups
- Simulator hosts

The scheduler uses this information when matching jobs to resources.

Typical administrative actions:

- Add a new execution host
- Add GPUs to a host
- Create a host group
- Create a token pool
- Create simulator hosts for testing

After modifying host definitions, restart the affected daemons and verify:

```sh
bhosts
bgroups
btokens
```

See:

```text
man 5 llb.hosts
```

for the complete reference.

## llb.queues

`llb.queues` defines scheduling queues.

Queues control:

- Scheduling priority
- Host group assignment
- User access
- Default policies

Typical administrative actions:

- Create a queue
- Change queue priority
- Restrict queue access
- Associate a queue with a host group

Verify queue configuration:

```sh
bqueues
```

See:

```text
man 5 llb.queues
```

for the complete reference.

## Validation

After any configuration change, verify that the scheduler can read the
updated configuration.

Typical checks:

```sh
bhosts
bqueues
```

Verify that:

- Expected hosts are visible.
- Expected queues are visible.
- Host groups appear correctly.
- Token pools appear correctly.
