// Copyright (C) LavaLite Contributors
// GPL v2

#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <stdlib.h>
#include <arpa/inet.h>

#include "batch/sbd/snamespace.h"

/*
 * Service network pool: 10.200.0.0/16.
 *
 * Divide the pool into /30 subnets, four addresses per slot:
 *
 *   slot 0: 10.200.0.0/30
 *       .0  network
 *       .1  SBD end
 *       .2  service end
 *       .3  broadcast
 *
 *   slot 1: 10.200.0.4/30
 *       .4  network
 *       .5  SBD end
 *       .6  service end
 *       .7  broadcast
 *
 * There are 65536 / 4 = 16384 available slots.
 */
#define SVC_NET_BASE 0x0ac80000U /* 10.200.0.0 */
#define SVC_NET_POOL_ADDRS 65536
#define SVC_NET_SLOT_ADDRS 4
#define SVC_NET_NSLOTS     (SVC_NET_POOL_ADDRS / SVC_NET_SLOT_ADDRS)

static uint16_t next_slot;

static int slot_alloc(uint16_t *slot)
{
    *slot = next_slot;

    next_slot++;
    if (next_slot >= SVC_NET_NSLOTS)
        next_slot = 0;

    return 0;
}

/*
 * Convert a slot into the two usable addresses of its /30.
 *
 * For example, slot 64:
 *
 *   subnet       10.200.1.0/30
 *   SBD          10.200.1.1
 *   service      10.200.1.2
 *   broadcast    10.200.1.3
 *
 * Assigning 10.200.1.1/30 to ll_sbdX causes Linux to install:
 *
 *   10.200.1.0/30 dev ll_sbdX proto kernel scope link src 10.200.1.1
 */
static void namespace_addresses(struct snamespace *ns, uint16_t slot)
{
    uint32_t base;

    base = SVC_NET_BASE + ((uint32_t)slot * SVC_NET_SLOT_ADDRS);

    ns->sbd_addr.s_addr = htonl(base + 1);
    ns->svc_addr.s_addr = htonl(base + 2);
}

static int namespace_names(struct snamespace *ns, uint16_t slot)
{
    int n;

    n = snprintf(ns->name, sizeof(ns->name),
                 "svc%ld", (long)ns->job_id);
    if (n < 0 || (size_t)n >= sizeof(ns->name))
        return -1;

    n = snprintf(ns->sbd_if, sizeof(ns->sbd_if),
                 "ll_sbd%u", slot);
    if (n < 0 || (size_t)n >= sizeof(ns->sbd_if))
        return -1;

    n = snprintf(ns->svc_if, sizeof(ns->svc_if),
                 "ll_svc%u", slot);
    if (n < 0 || (size_t)n >= sizeof(ns->svc_if))
        return -1;

    return 0;
}

static int snamespace_init(struct snamespace *ns, int64_t job_id)
{
    uint16_t slot;

    memset(ns, 0, sizeof(*ns));
    ns->job_id = job_id;

    if (slot_alloc(&slot) < 0)
        return -1;

    if (namespace_names(ns, slot) < 0)
        return -1;

    namespace_addresses(ns, slot);

    return 0;
}

static int namespace_create(const struct snamespace *ns)
{
    char cmd[PATH_MAX];

    snprintf(cmd, sizeof(cmd), "ip netns add %s", ns->name);

    if (system(cmd) != 0)
        return -1;

    return 0;
}

static int namespace_destroy(const struct snamespace *ns)
{
    char cmd[PATH_MAX];

    snprintf(cmd, sizeof(cmd), "ip netns del %s", ns->name);

    if (system(cmd) != 0)
        return -1;

    return 0;
}

static int namespace_veth_create(const struct snamespace *ns)
{
    char cmd[PATH_MAX];
    int n;

    /*
     * Equivalent:
     *
     *   ip link add ll_sbdX type veth peer name ll_svcX
     */
    n = snprintf(cmd, sizeof(cmd),
                 "ip link add %s type veth peer name %s",
                 ns->sbd_if, ns->svc_if);
    if (n < 0 || (size_t)n >= sizeof(cmd))
        return -1;

    if (system(cmd) != 0)
        return -1;

    /*
     * Move the service end into the service namespace:
     *
     *   ip link set ll_svcX netns svcX
     */
    n = snprintf(cmd, sizeof(cmd),
                 "ip link set %s netns %s",
                 ns->svc_if, ns->name);
    if (n < 0 || (size_t)n >= sizeof(cmd))
        return -1;

    if (system(cmd) != 0)
        return -1;

    return 0;
}

int snamespace_create(int64_t job_id, struct snamespace *ns)
{
    if (snamespace_init(ns, job_id) < 0)
        return -1;

    /*
     * Example for:
     *
     *   namespace: svc1
     *   SBD iface: ll_sbd1
     *   SVC iface: ll_svc1
     *   SBD addr:  10.200.1.1/30
     *   SVC addr:  10.200.1.2/30
     *
     * Create the service network namespace:
     *
     *   ip netns add svc1
     *
     * Create the veth pair in the SBD namespace:
     *
     *   ip link add ll_sbd1 type veth peer name ll_svc1
     *
     * Move the service end into the service namespace:
     *
     *   ip link set ll_svc1 netns svc1
     *
     * Configure the SBD end:
     *
     *   ip addr add 10.200.1.1/30 dev ll_sbd1
     *   ip link set ll_sbd1 up
     *
     * Configure the service end:
     *
     *   ip netns exec svc1 ip addr add 10.200.1.2/30 dev ll_svc1
     *   ip netns exec svc1 ip link set ll_svc1 up
     *
     * Bring up loopback in the service namespace:
     *
     *   ip netns exec svc1 ip link set lo up
     */

    /* Equivalent:
     * ip netns add svc<job_id>
     */
    if (namespace_create(ns) < 0)
        return -1;

    if (namespace_veth_create(ns) < 0) {
        namespace_destroy(ns);
        return -1;
    }

    return 0;
}

int snamespace_destroy(struct snamespace *ns)
{
    /*
     * Equivalent:
     *
     *   ip netns del svc<job_id>
     */
    return namespace_destroy(ns);
}
