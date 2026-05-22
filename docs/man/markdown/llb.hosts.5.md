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

# SECTIONS

## Begin Host / End Host

Defines the execution hosts and their schedulable resources.

    Begin Host
    HOST_NAME   MXJ   CPU   MEM    STORAGE
    node01        8    32    128G   1T
    node02        8    32    128G   1T
    End Host

Each line after the header defines one host. Columns are
whitespace-separated. Lines starting with **#** are comments.

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

## Begin Gpu / End Gpu

Defines GPU devices per host. Each line describes one GPU device or
a set of identical devices on a host.

    Begin Gpu
    HOST_NAME   GPU_ID   GPU_MODEL   GPU_TYPE   COUNT
    node01      0        H100        full       4
    node01      4        H100        3g.40gb    2
    End Gpu

**HOST_NAME**
:   Host where the GPU resides. Must be defined in the **Host** section.

**GPU_ID**
:   Starting device index as reported by **nvidia-smi**.

**GPU_MODEL**
:   Model name, used for display purposes.

**GPU_TYPE**
:   Type name used to match **bsub --gpu-type**. Use **full** for a
    complete GPU. For MIG instances, use the MIG profile name
    (e.g. **3g.40gb**, **2g.20gb**).

**COUNT**
:   Number of devices of this type at the given GPU_ID.

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

    Begin Sim
    NAME          REAL_HOST   PORT    MXJ  CPU  MEM   STORAGE
    worker1-sim   buntu24     33126     8  128   2T    100T
    End Sim

**NAME**
:   Name the simulated host registers as with **mbd**.

**REAL_HOST**
:   Physical host where the simulating **sbd** process runs.

**PORT**
:   Port the simulating **sbd** listens on.

**MXJ**, **CPU**, **MEM**, **STORAGE**
:   Same meaning as in the **Host** section.

# EXAMPLE

    Begin Host
    HOST_NAME   MXJ   CPU   MEM    STORAGE
    node01        8    64    256G   2T
    node02        8    64    256G   2T
    gpu01         4    32    512G   4T
    End Host

    Begin Gpu
    HOST_NAME   GPU_ID   GPU_MODEL   GPU_TYPE   COUNT
    gpu01       0        H100        full       4
    End Gpu

    Begin TokenPool
    POOL_NAME  AVAILABLE
    hspice     20
    End TokenPool

    Begin HostGroup
    GROUP_NAME    GROUP_MEMBER
    group-cpu     (node01 node02)
    group-gpu     (gpu01)
    End HostGroup

# SEE ALSO

**bhosts**(1), **bgroup**(1), **btokens**(1), **bsub**(1),
**llb.queues**(5), **mbd**(8), **sbd**(8)
