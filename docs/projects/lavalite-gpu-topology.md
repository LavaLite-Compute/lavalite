# LavaLite GPU Topology and Scheduling

## Overview

Modern GPU workloads (AI training, inference, rendering) require topology-aware scheduling. GPU-to-GPU communication bandwidth varies dramatically based on physical interconnect topology, and poor placement can severely impact performance.

## GPU Interconnect Types

GPUs communicate through different physical links with vastly different bandwidths:

| Link Type | Bandwidth (Bidirectional) | Use Case | Latency |
|-----------|---------------------------|----------|---------|
| **NVLink** | 600-900 GB/s | Multi-GPU training, tight coupling | ~1-2 μs |
| **NVSwitch** | Shared fabric, 900 GB/s per GPU | DGX systems, all-to-all | ~1-2 μs |
| **PCIe Gen4 x16** | ~64 GB/s | General purpose, loose coupling | ~5-10 μs |
| **PCIe Gen3 x16** | ~32 GB/s | Older systems | ~5-10 μs |
| **Cross-NUMA/Socket** | PCIe + NUMA overhead | Avoid if possible | ~20-50 μs |
| **Cross-Host (Network)** | 100-400 Gb/s (12-50 GB/s) | Distributed training | ~100-500 μs |

**Key insight**: NVLink is 10-30x faster than PCIe. For multi-GPU jobs, topology matters enormously.

## Typical GPU Topologies

### Single-Host DGX-Style (8x A100 with NVSwitch)

```
Host: dgx-node-01
├── CPU Socket 0 (NUMA node 0)
│   ├── GPU 0 ─┐
│   ├── GPU 1 ─┤
│   ├── GPU 2 ─┤
│   └── GPU 3 ─┤── All connected via NVSwitch (full bandwidth any-to-any)
├── CPU Socket 1 (NUMA node 1)  │
│   ├── GPU 4 ─┤
│   ├── GPU 5 ─┤
│   ├── GPU 6 ─┤
│   └── GPU 7 ─┘
```

**Scheduling rule**: Any GPU subset works well. All have full NVSwitch connectivity.

### Dual-Socket Server (4x GPU, NVLink Pairs)

```
Host: gpu-node-42
├── CPU Socket 0 (NUMA node 0)
│   ├── GPU 0 ←→ GPU 1  (NVLink)
│   └── GPU 1
├── CPU Socket 1 (NUMA node 1)
│   ├── GPU 2 ←→ GPU 3  (NVLink)
│   └── GPU 3

GPU 0 ↔ GPU 2: PCIe only (~64 GB/s)
GPU 0 ↔ GPU 3: PCIe + cross-socket (~32 GB/s)
```

**Scheduling rule**:
- For 2-GPU jobs: prefer (0,1) or (2,3) pairs
- For 4-GPU jobs: all GPUs OK but expect mixed bandwidth
- CPU affinity: pin to same NUMA node as GPU

### Multi-Host Rack (Network-Connected)

```
Rack 5, ToR Switch
├── Host gpu-r5-n1 (8x A100 NVSwitch)
│   └── GPUs 0-7: Full NVSwitch fabric
├── Host gpu-r5-n2 (8x A100 NVSwitch)
│   └── GPUs 0-7: Full NVSwitch fabric
└── Host gpu-r5-n3 (8x A100 NVSwitch)
    └── GPUs 0-7: Full NVSwitch fabric

Inter-host: 8x 200Gb InfiniBand (HDR) = ~200 GB/s aggregate
```

**Scheduling rule**:
- Prefer single-host for ≤8 GPUs
- If multi-host needed, keep within same rack/ToR
- NCCL will use InfiniBand but 4-5x slower than NVLink

## What the Scheduler Needs to Track

### Per-GPU Information

```c
struct gpu_info {
    // Identification
    char model[64];              // "NVIDIA A100-SXM4-80GB"
    int gpu_id;                  // Local GPU index (0-7)
    char pcie_bus_id[16];        // "0000:07:00.0"
    char uuid[64];               // "GPU-abc123..."

    // Capabilities
    int cuda_compute_cap;        // 80 (for A100), 89 (H100)
    int memory_mb;               // 81920 for 80GB A100
    int memory_bandwidth_gbps;   // 2039 for A100

    // Topology
    int numa_node;               // Which CPU socket is "close"
    uint64_t cpu_affinity_mask;  // Which CPU cores are local

    // Interconnect (filled by topology detection)
    int nvlink_peers[8];         // GPU IDs directly connected via NVLink
    int nvlink_peer_count;
    bool has_nvswitch;           // True for DGX-style full fabric

    // State
    bool allocated;              // Currently in use?
    int job_id;                  // Which job owns it (if allocated)
};
```

### Per-Host GPU Topology

```c
struct host_gpu_topology {
    char hostname[64];
    int gpu_count;
    struct gpu_info gpus[16];    // Max 16 GPUs per host

    // Topology type (scheduler hint)
    enum {
        GPU_TOPO_NVSWITCH,       // DGX-style, any combo OK
        GPU_TOPO_NVLINK_PAIRS,   // GPU pairs, prefer pairs
        GPU_TOPO_PCIE_ONLY,      // No NVLink, locality doesn't matter much
        GPU_TOPO_MIXED           // Complex, need careful placement
    } topology_type;
};
```

## Topology Detection

Read from `nvidia-smi topo -m` output:

```
GPU0    GPU1    GPU2    GPU3    mlx5_0  CPU Affinity    NUMA Affinity
GPU0     X      NV12    SYS     SYS     SYS     0-31            0
GPU1    NV12     X      SYS     SYS     SYS     0-31            0
GPU2    SYS     SYS      X      NV12    SYS     32-63           1
GPU3    SYS     SYS     NV12     X      SYS     32-63           1
```

**Legend:**
- `X`: Self
- `NV#`: NVLink (# = number of links, e.g., NV12 = 12 links)
- `SYS`: PCIe path
- `NODE`: Same NUMA node but different PCIe root
- `PHB`: Same PCIe host bridge

**Parsing strategy:**
1. Run `nvidia-smi topo -m` at host startup
2. Parse matrix to build `gpu_info.nvlink_peers[]`
3. Detect topology type (all NVLink = NVSwitch, pairs = paired, etc.)
4. Store in host's GPU topology structure

## Scheduling Policies

### Policy 1: Exclusive GPU Allocation (Simple)

User requests:
```bash
bsub -gpu "num=4:mode=exclusive" ./train.py
```

Scheduler:
1. Find host with ≥4 free GPUs
2. Allocate any 4 GPUs (if NVSwitch topology)
3. Set `CUDA_VISIBLE_DEVICES=0,1,2,3` (remapped to physical IDs)
4. Mark GPUs as allocated

**Implementation:**
```c
int schedule_exclusive_gpus(struct host_gpu_topology *host,
                           int num_gpus,
                           int *out_gpu_ids) {
    int found = 0;
    for (int i = 0; i < host->gpu_count && found < num_gpus; i++) {
        if (!host->gpus[i].allocated) {
            out_gpu_ids[found++] = host->gpus[i].gpu_id;
            host->gpus[i].allocated = true;
        }
    }
    return found == num_gpus;
}
```

### Policy 2: NVLink-Aware Placement (Better)

User requests:
```bash
bsub -gpu "num=4:mode=exclusive:nvlink=yes" ./train.py
```

Scheduler:
1. Find host with ≥4 free GPUs
2. **Prefer GPUs that are NVLink-connected**
3. Fall back to any 4 if NVLink group unavailable

**Implementation sketch:**
```c
// For NVLink-pair topology, try to allocate connected pairs
int schedule_nvlink_aware(struct host_gpu_topology *host,
                         int num_gpus,
                         int *out_gpu_ids) {
    if (host->topology_type == GPU_TOPO_NVSWITCH) {
        // Any GPUs work, use simple allocation
        return schedule_exclusive_gpus(host, num_gpus, out_gpu_ids);
    }

    if (host->topology_type == GPU_TOPO_NVLINK_PAIRS && num_gpus == 2) {
        // Try to find a connected pair
        for (int i = 0; i < host->gpu_count; i++) {
            if (!host->gpus[i].allocated) {
                for (int j = 0; j < host->gpus[i].nvlink_peer_count; j++) {
                    int peer = host->gpus[i].nvlink_peers[j];
                    if (!host->gpus[peer].allocated) {
                        out_gpu_ids[0] = i;
                        out_gpu_ids[1] = peer;
                        host->gpus[i].allocated = true;
                        host->gpus[peer].allocated = true;
                        return 1;
                    }
                }
            }
        }
    }

    // Fallback: any available GPUs
    return schedule_exclusive_gpus(host, num_gpus, out_gpu_ids);
}
```

### Policy 3: NUMA-Aware CPU Affinity

For GPU jobs, pin CPU cores near the GPU's NUMA node for best memory access:

```c
void set_cpu_affinity_for_gpu(int gpu_id, struct gpu_info *gpu) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);

    // Add CPUs from the GPU's NUMA node
    for (int i = 0; i < 64; i++) {
        if (gpu->cpu_affinity_mask & (1ULL << i)) {
            CPU_SET(i, &cpuset);
        }
    }

    sched_setaffinity(0, sizeof(cpuset), &cpuset);
}
```

## Job Submission Interface

Extend `bsub` GPU resource syntax:

```bash
# Basic: any 2 GPUs
bsub -gpu "num=2" ./train.py

# Exclusive mode (no sharing)
bsub -gpu "num=4:mode=exclusive" ./train.py

# Request NVLink connectivity
bsub -gpu "num=4:mode=exclusive:nvlink=yes" ./train.py

# Specific GPU model
bsub -gpu "num=8:mode=exclusive:gmodel=A100" ./train.py

# Request minimum memory
bsub -gpu "num=2:gmem=40GB" ./inference.py

# Multi-node GPU job (MPI-style)
bsub -n 16 -gpu "num=4:mode=exclusive:nvlink=yes" mpirun ./distributed_train.py
```

## Integration Points in LavaLite

### 1. Host Registration (LIM)

When a host starts, detect GPU topology:

```c
// In lim daemon startup
struct host_gpu_topology* detect_gpu_topology(void) {
    // Run: nvidia-smi --query-gpu=index,name,memory.total,uuid,... --format=csv
    // Run: nvidia-smi topo -m
    // Parse and build topology structure
    // Return NULL if no GPUs present
}
```

### 2. Resource Tracking (mbatchd)

Track GPU availability per host:

```c
struct host_resources {
    char hostname[64];
    int total_cores;
    int free_cores;
    int total_mem_mb;
    int free_mem_mb;

    // GPU resources
    struct host_gpu_topology *gpu_topo;  // NULL if no GPUs
    int total_gpus;
    int free_gpus;
};
```

### 3. Job Dispatch (mbatchd → sbatchd)

When dispatching GPU job:

```c
struct job_gpu_allocation {
    int num_gpus;
    int gpu_ids[16];           // Physical GPU IDs
    bool require_nvlink;
    int min_gmem_mb;
};

// mbatchd decides allocation, sends to sbatchd
void dispatch_gpu_job(struct job *job, struct host_resources *host) {
    struct job_gpu_allocation alloc;

    if (!allocate_gpus(host, job->gpu_req, &alloc)) {
        // Can't satisfy GPU requirements
        return;
    }

    // Send job + GPU allocation to sbatchd
    send_job_start(host, job, &alloc);
}
```

### 4. Job Execution (sbatchd)

Set up GPU environment before exec:

```c
void setup_gpu_environment(struct job_gpu_allocation *alloc) {
    // Build CUDA_VISIBLE_DEVICES string
    char visible_devices[256] = "";
    for (int i = 0; i < alloc->num_gpus; i++) {
        char buf[16];
        sprintf(buf, "%d%s", alloc->gpu_ids[i],
                i < alloc->num_gpus - 1 ? "," : "");
        strcat(visible_devices, buf);
    }
    setenv("CUDA_VISIBLE_DEVICES", visible_devices, 1);

    // Set CPU affinity if available
    if (alloc->num_gpus > 0) {
        struct gpu_info *gpu = get_gpu_info(alloc->gpu_ids[0]);
        set_cpu_affinity_for_gpu(alloc->gpu_ids[0], gpu);
    }
}
```

## Monitoring and Reporting

### GPU Status Command

```bash
$ llgpu
HOST            GPU  MODEL           MEM   STATE    JOB_ID   UTIL
gpu-r5-n1       0    A100-80GB       80GB  alloc    123456   95%
gpu-r5-n1       1    A100-80GB       80GB  alloc    123456   94%
gpu-r5-n1       2    A100-80GB       80GB  free     -        0%
gpu-r5-n1       3    A100-80GB       80GB  free     -        0%
gpu-r5-n2       0    A100-80GB       80GB  alloc    123457   87%
gpu-r5-n2       1    A100-80GB       80GB  alloc    123457   89%
```

### Enhanced bjobs

```bash
$ bjobs -gpu
JOBID   USER    STAT  QUEUE   GPUS  GPU_HOSTS           SUBMIT_TIME
123456  alice   RUN   gpu     2     gpu-r5-n1[0-1]      Nov 20 10:23
123457  bob     RUN   gpu     2     gpu-r5-n2[0-1]      Nov 20 10:45
123458  carol   PEND  gpu     4     -                   Nov 20 11:02
```

## Implementation Priorities

1. **Phase 1 (MVP)**: Basic GPU counting and exclusive allocation
   - Track GPU presence per host
   - Simple `bsub -gpu "num=N"` support
   - Set `CUDA_VISIBLE_DEVICES`

2. **Phase 2**: Topology detection and NVLink awareness
   - Parse `nvidia-smi topo -m`
   - Prefer NVLink-connected GPUs
   - NUMA-aware CPU pinning

3. **Phase 3**: Advanced features
   - GPU memory requirements
   - MPS (Multi-Process Service) sharing
   - GPU utilization monitoring
   - Multi-node GPU jobs

## References

- NVIDIA GPU Topology: `nvidia-smi topo --help`
- NCCL Performance: https://docs.nvidia.com/deeplearning/nccl/user-guide/docs/
- DGX A100 Architecture: https://www.nvidia.com/en-us/data-center/dgx-a100/
- CUDA Environment Variables: https://docs.nvidia.com/cuda/cuda-c-programming-guide/

## Notes

- GPU topology detection requires `nvidia-smi` on compute nodes
- NVSwitch detection: if all GPUs show NVLink to all others, it's NVSwitch
- Some systems use AMD GPUs (MI250X, MI300): similar concepts apply but different tools (`rocm-smi`)
- InfiniBand/RoCE detection for multi-host: parse `/sys/class/infiniband/` or use `ibstat`
