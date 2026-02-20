# LavaLite Resource Model (Detailed Specification)

This document describes the resource model used by LavaLite, including
host-based resources, floating resources, versioning semantics, and the
APIs used to collect, update, and snapshot resource state inside the
system.

The goal is to provide a unified, audit-friendly, and extensible
resource subsystem that replaces the legacy `elim → esten → lim`
pipe-based mechanism.

---

## 1. Overview

LavaLite maintains a unified internal representation of resources
available to the scheduler. Resources fall into two broad categories:

- **Host-based resources**
  Tied to a specific host. Examples: CPU, memory, swap, GPUs, NUMA
  quantities, network bandwidth.

- **Floating resources**
  Not tied to a specific host. Examples: licenses, Lustre space, tokens,
  quotas, external capacity pools.

Both categories share a common representation and update mechanism.

The system is designed to support:

- atomic updates
- versioned state
- coherent snapshots
- extensibility without ABI breakage
- clean separation between collection and scheduling logic

---

## 2. Host Representation

Hosts are represented internally using `struct ll_host`:

```c
struct ll_host {
    int family;                 /* AF_INET / AF_INET6 */
    socklen_t salen;
    struct sockaddr_storage sa; /* canonical socket addr */
    char name[LL_HOSTNAME_MAX]; /* canonical hostname or "" */
    char addr[LL_HOSTNAME_MAX]; /* numeric IP string */
};
```

## 3. Resource Representation and Types
```
struct ll_resource {

    const char *name;           /* "gpu", "mem", "swap", "license_x", ... */
    float value;                /* quantity or capacity */
    enum ll_resource_type type; /* LL_RES_HOST or LL_RES_FLOATING */
    uint64_t version;           /* monotonic version for state tracking */
};
```
```
enum ll_resource_type {
    LL_RES_HOST,
    LL_RES_FLOATING
};
```

## 4. Resource Flow Architecture
```

                   +----------------------+
                   |      User Space      |
                   |  (collectors, admin) |
                   +----------+-----------+
                              |
                              | ll_resource_update()
                              v
                   +----------------------+
                   |         LIM          |
                   |  authoritative state |
                   +----------+-----------+
                              |
                              | ll_resource_snapshot()
                              v
                   +----------------------+
                   |      Scheduler       |
                   |  (placement logic)   |
                   +----------------------+
                              |
                              | scheduling decisions
                              v
                   +----------------------+
                   |        Cluster       |
                   +----------------------+
```

## 5. Usage example

```
#include "ll_resource.h"
#include "ll_host.h"

int main(void)
{
    struct ll_host host = {
        .family = AF_INET,
        .salen = sizeof(struct sockaddr_in),
        .name = "node001",
        .addr = "10.0.0.1"
    };

    struct ll_resource gpu_res = {
        .name = "gpu",
        .value = 4.0,                 /* 4 GPUs installed */
        .type = LL_RES_HOST,
        .version = 42                 /* incremented by caller */
    };

    int rc = ll_resource_update(&host, &gpu_res);
    if (rc != 0) {
        fprintf(stderr, "Failed to update GPU resource\n");
        return 1;
    }

    printf("Resource update successful\n");
    return 0;
}
```
