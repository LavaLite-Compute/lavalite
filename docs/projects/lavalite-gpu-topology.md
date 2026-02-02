# Lava Compute Engine – Topology & Resource Request Foundation

## 1. Topology model

Lava models the physical cluster as a hierarchy:

- **Host**
  - **NUMA node**
    - CPU set
    - Memory
    - Devices (GPU, NIC, etc.)

This model is read from a simple, line‑oriented configuration file and exposed via an internal topology API and the `btopo` command.

---

## 2. Topology configuration format (INI‑style sketch)

The topology file is intentionally minimal, diff‑friendly, and Unix‑like.

```ini
[host node01]

[numa 0]
cpus = 0-15
mem  = 64G
device = gpu0

[numa 1]
cpus = 16-31
mem  = 64G
device = gpu1

[device gpu0]
type  = gpu
model = A100
numa  = 0
pci   = 0000:65:00.0
link  = gpu1:nvlink

[device gpu1]
type  = gpu
model = A100
numa  = 1
pci   = 0000:66:00.0
link  = gpu0:nvlink

## 7. Topology C Structures (foundation)

The topology file is parsed into a compact in‑memory representation suitable for fast scheduling decisions.
The model is intentionally static‑sized to avoid dynamic allocation in the hot path.

```c
#define TOPO_MAX_NUMA     8
#define TOPO_MAX_DEV      32
#define TOPO_MAX_LINKS    8

enum topo_dev_type {
    TOPO_DEV_GPU = 1,
    TOPO_DEV_NIC,
    TOPO_DEV_OTHER
};

enum topo_link_type {
    TOPO_LINK_NVLINK = 1,
    TOPO_LINK_PCIE,
    TOPO_LINK_OTHER
};

struct topo_link {
    int peer_dev_idx;          // index into host->dev[]
    enum topo_link_type type;  // NVLINK / PCIE / OTHER
};

struct topo_device {
    char name[32];             // "gpu0"
    enum topo_dev_type type;   // gpu / nic / other
    char model[32];            // "A100"
    int numa_id;               // NUMA node affinity
    char pci[32];              // PCI BDF: "0000:65:00.0"

    int nlinks;                // number of interconnect links
    struct topo_link link[TOPO_MAX_LINKS];
};

struct topo_numa {
    int id;                    // NUMA index
    cpu_set_t cpus;            // CPU bitmap
    uint64_t mem_bytes;        // memory in bytes
};

struct topo_host {
    char hostName[64];
    int hostNo;

    int nnuma;
    struct topo_numa numa[TOPO_MAX_NUMA];

    int ndev;
    struct topo_device dev[TOPO_MAX_DEV];
};
