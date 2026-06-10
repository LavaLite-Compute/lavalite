---
title: LLB.HOSTS
section: 5
header: LavaLite Configuration Files
footer: LavaLite
date: 2026
---

# NAME

llb.hosts - LavaLite host, GPU, token pool, and host group configuration

# DESCRIPTION

**llb.hosts** defines the execution hosts, GPU devices, token pools,
and host groups available in the cluster. **mbd** reads this file at
startup.

The file contains one or more named sections, each delimited by
**Begin** *Section* / **End** *Section* blocks.

Within each section, the column header defines the file format. The order of columns is **mandatory** and must not be changed. LavaLite parsers use positional fields and do not support reordering, inserting, or removing columns. Modifying the column layout may result in configuration errors or undefined scheduler behaviour.
Fields that have no value must be specified as a single dash (-).
Empty fields are not permitted.

# SECTIONS
## Begin Host / End Host

Defines the execution hosts and their schedulable resources.

    Begin Host
    HOST_NAME   MXJ   CPU   MEM    STORAGE   GPU_MODEL   GPU_IDS
    node01        8    32    128G     1T      A100       0,1
    node02        8    32    128G     1T      -          -
    End Host

Each line after the header defines one host. Columns are
whitespace-separated. The column order is mandatory and must not be
changed. Lines starting with **#** are comments.

For hosts without GPUs, use **-** for both **GPU_MODEL** and
**GPU_IDS**.

**HOST_NAME**
:   Hostname as returned by **hostname**(1). Must be reachable from
    the master host.

**MXJ**
:   Maximum number of concurrent job slots on this host.

**CPU**
:   Number of CPU cores available for scheduling.

**MEM**
:   Total memory available. Accepts a plain integer (MB) or a suffix:
    K, M, G, T.

**STORAGE**
:   Total local scratch storage available. Same suffix rules as MEM.

**GPU_MODEL**
:   GPU model available on this host, for example **A100**, **H100**,
    **RTX4090**, or **MI300X**. Use **-** if the host has no GPUs.

**GPU_IDS**
:   Comma-separated list of GPU device identifiers available for
    scheduling on this host. For example **0,1,2,3**. These identifiers
    are used by the scheduler to allocate GPUs to jobs. Use **-** if
    the host has no GPUs.

GPU allocation in LavaLite is currently scheduler-enforced rather than
kernel-enforced. The master daemon tracks GPU availability, allocates
GPU device identifiers to jobs, and the execution daemon sets the
**CUDA_VISIBLE_DEVICES** environment variable before starting the job.
This ensures that well-behaved CUDA applications only see the GPUs
assigned to them.

LavaLite does not currently restrict direct access to GPU device files
through Linux cgroups or other kernel mechanisms. GPU isolation is
therefore cooperative: jobs are expected to respect the assigned
**CUDA_VISIBLE_DEVICES** setting.

## Begin TokenPool / End TokenPool

Defines floating token pools for license or resource gating.
Jobs request tokens with **bsub --pool**.

    Begin TokenPool
    POOL_NAME  AVAILABLE
    hspice     10
    matlab      5
    End TokenPool

**POOL_NAME**
:   Name of the token pool. Referenced by **bsub --pool** and
    displayed by **btokens**.

**AVAILABLE**
:   Total number of tokens in the pool.

## Begin HostGroup / End HostGroup

Defines named groups of hosts. Host groups are referenced by
**HOSTS** in **llb.queues** and by **bsub --machines**.

    Begin HostGroup
    GROUP_NAME    GROUP_MEMBER
    group-cpu     (node01 node02)
    group-gpu     (gpu01 gpu02)
    End HostGroup

**GROUP_NAME**
:   Name of the host group.

**GROUP_MEMBER**
:   Parenthesised, space-separated list of host names belonging to
    the group.

## Begin Sim / End Sim

Defines simulated hosts for testing without real execution hardware.
Simulated hosts are registered by **sbd --simulator**.

```
Begin Sim
NAME          REAL_HOST   PORT  MXJ  CPU  MEM    STORAGE   GPU_MODEL  GPU_IDS
sim1          buntu24   33126    4     8  20G    100G      A100      0-1
End Sim
```

**NAME**
:   Name the simulated host registers as with **mbd**.

**REAL_HOST**
:   Physical host where the simulating **sbd** process runs.

**PORT**
:   Port the simulating **sbd** listens on.

**MXJ**, **CPU**, **MEM**, **STORAGE**,  **GPU_MODEL**,  **GPU_IDS**
:   Same meaning as in the **Host** section.

# EXAMPLE

    Begin Host
    HOST_NAME   MXJ   CPU   MEM    STORAGE   GPU_MODEL   GPU_IDS
    node01        8    64    256G      2T      -          -
    node02        8    64    256G      2T      -          -
    gpu01         4    32    512G      4T      H100       0,1,2,3
    gpu02         4    32    512G      4T      A100       0,1
    End Host

    Begin TokenPool
    POOL_NAME  AVAILABLE
    hspice     20
    End TokenPool

    Begin HostGroup
    GROUP_NAME    GROUP_MEMBER
    group-cpu     (node01 node02)
    group-gpu     (gpu01 gpu02)
    End HostGroup

This configuration defines two CPU-only hosts and two GPU hosts.
The gpu01 host provides four NVIDIA H100 GPUs, while gpu02 provides
two NVIDIA A100 GPUs. Jobs submitted with GPU requirements are
scheduled only on hosts that satisfy the requested GPU model and
available GPU count.

For example:

    bsub --gpus 1 --gpu-model H100 myjob.sh

requests one NVIDIA H100 GPU. The scheduler allocates a free GPU
device identifier and the execution daemon exports the allocation
through the **CUDA_VISIBLE_DEVICES** environment variable before
starting the job.

# SEE ALSO

**bhosts**(1), **bgroup**(1), **btokens**(1), **bsub**(1),
**llb.queues**(5), **mbd**(8), **sbd**(8)
