# LavaLite Administrator's Guide — Configuration

All configuration files live in `LL_CONF_DIR` (default `/opt/lavalite/etc`).

## ll.conf

Main cluster environment file. Sourced as a shell script by all daemons
and commands at startup.

Mandatory parameters:

| Parameter         | Description                              |
|-------------------|------------------------------------------|
| LL_CLUSTER_NAME   | Cluster name, used in logs               |
| LL_CONF_DIR       | Path to this directory                   |
| LL_STATE_DIR      | Persistent state directory               |
| LL_LOG_DIR        | Log directory                            |
| LL_MBD_HOST       | Hostname of the master host              |
| LL_MBD_PORT       | TCP port for mbd                         |
| LL_SBD_PORT       | TCP port for sbd                         |
| LL_DEFAULT_QUEUE  | Queue used when --queue is not specified |

See `man 5 ll.conf` for the full parameter reference.

## llb.queues

Defines batch queues. Each queue is a `Begin Queue` / `End Queue` block.

Mandatory per queue: `QUEUE_NAME`, `PRIORITY`, `HOSTS`.

See `man 5 llb.queues` for the full reference and examples.

## llb.hosts

Defines execution hosts, GPU devices, token pools, and host groups.
Contains one or more named sections:

- `Begin Host / End Host` — host resources
- `Begin Gpu / End Gpu` — GPU devices per host
- `Begin TokenPool / End TokenPool` — floating license pools
- `Begin HostGroup / End HostGroup` — named groups of hosts
- `Begin Sim / End Sim` — simulated hosts for testing

See `man 5 llb.hosts` for the full reference and examples.

## Verification

After writing configuration, verify before starting daemons:

```
# Check mbd can read config
/opt/lavalite/sbin/mbd --help

# After starting, check hosts and queues
bhosts
bqueues
```
