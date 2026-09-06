// Copyright (C) LavaLite Contributors
// GPL v2

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <net/if.h>
#include <sched.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <linux/if_link.h>
#include <linux/veth.h>

#include "base/lib/ll.syslog.h"
#include "batch/sbd/snamespace.h"
#include "batch/sbd/sbd.h"

#define NETNS_DIR "/run/netns"

/*
 * Service network pool: 10.200.0.0/16.
 *
 * Divide the pool into /30 subnets:
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
 * There are 16384 available slots.
 */
#define SVC_NET_BASE       0x0ac80000U /* 10.200.0.0 */
#define SVC_NET_POOL_ADDRS 65536U
#define SVC_NET_SLOT_ADDRS 4U
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

static void namespace_addresses(struct snamespace *ns, uint16_t slot)
{
    uint32_t base;
    char sbd_addr[INET_ADDRSTRLEN];
    char svc_addr[INET_ADDRSTRLEN];

    base = SVC_NET_BASE + ((uint32_t)slot * SVC_NET_SLOT_ADDRS);

    ns->sbd_addr.s_addr = htonl(base + 1);
    ns->svc_addr.s_addr = htonl(base + 2);

    if (inet_ntop(AF_INET, &ns->sbd_addr, sbd_addr, sizeof(sbd_addr)) == NULL)
        strcpy(sbd_addr, "<invalid>");

    if (inet_ntop(AF_INET, &ns->svc_addr, svc_addr, sizeof(svc_addr)) == NULL)
        strcpy(svc_addr, "<invalid>");

    LL_DEBUG("netns slot=%u sbd=%s svc=%s", slot, sbd_addr, svc_addr);
}

static int namespace_names(struct snamespace *ns, uint16_t slot)
{
    int n;

    n = snprintf(ns->name, sizeof(ns->name),
                 "svc%ld", (long)ns->job_id);
    if (n < 0 || (size_t)n >= sizeof(ns->name)) {
        errno = ENAMETOOLONG;
        return -1;
    }

    n = snprintf(ns->sbd_if, sizeof(ns->sbd_if),
                 "ll_sbd%u", slot);
    if (n < 0 || (size_t)n >= sizeof(ns->sbd_if)) {
        errno = ENAMETOOLONG;
        return -1;
    }

    n = snprintf(ns->svc_if, sizeof(ns->svc_if),
                 "ll_svc%u", slot);
    if (n < 0 || (size_t)n >= sizeof(ns->svc_if)) {
        errno = ENAMETOOLONG;
        return -1;
    }

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

static int netns_path(char *buf, size_t len, const char *name)
{
    int n;

    n = snprintf(buf, len, "%s/%s", NETNS_DIR, name);
    if (n < 0 || (size_t)n >= len) {
        errno = ENAMETOOLONG;
        return -1;
    }

    return 0;
}

static int netns_mount_create(const char *name)
{
    char path[PATH_MAX];
    int fd;

    if (netns_path(path, sizeof(path), name) < 0)
        return -1;

    if (mkdir(NETNS_DIR, 0755) < 0 && errno != EEXIST)
        return -1;

    /*
     * iproute2 normally ensures /run/netns is a shared mount.
     * For now we only need the bind mount to keep the namespace alive.
     */
    fd = open(path, O_RDONLY | O_CREAT | O_EXCL, 0);
    if (fd < 0)
        return -1;

    close(fd);

    /*
     * This process is already inside the newly-created network namespace.
     * Bind mounting /proc/self/ns/net keeps it alive after this process exits.
     */
    if (mount("/proc/self/ns/net", path, NULL, MS_BIND, NULL) < 0) {
        int saved_errno = errno;

        unlink(path);
        errno = saved_errno;
        return -1;
    }

    return 0;
}

static int netns_create_child(const char *name)
{
    if (unshare(CLONE_NEWNET) < 0)
        return -1;

    if (netns_mount_create(name) < 0)
        return -1;

    return 0;
}

static int rtnl_addattr(struct nlmsghdr *nlh, size_t maxlen, int type,
                        const void *data, size_t len)
{
    size_t attr_len = RTA_LENGTH(len);
    size_t new_len = NLMSG_ALIGN(nlh->nlmsg_len) + RTA_ALIGN(attr_len);
    struct rtattr *rta;

    if (new_len > maxlen) {
        errno = EMSGSIZE;
        return -1;
    }

    rta = (struct rtattr *)((char *)nlh + NLMSG_ALIGN(nlh->nlmsg_len));
    rta->rta_type = type;
    rta->rta_len = attr_len;

    if (len != 0)
        memcpy(RTA_DATA(rta), data, len);

    nlh->nlmsg_len = new_len;
    return 0;
}

static struct rtattr *rtnl_nest_start(struct nlmsghdr *nlh, size_t maxlen,
                                      int type)
{
    struct rtattr *nest;

    nest = (struct rtattr *)((char *)nlh + NLMSG_ALIGN(nlh->nlmsg_len));

    if (rtnl_addattr(nlh, maxlen, type, NULL, 0) < 0)
        return NULL;

    return nest;
}

static void rtnl_nest_end(struct nlmsghdr *nlh, struct rtattr *nest)
{
    nest->rta_len = (char *)nlh + NLMSG_ALIGN(nlh->nlmsg_len)
        - (char *)nest;
}

static int rtnl_talk(struct nlmsghdr *nlh)
{
    struct sockaddr_nl addr;
    char buf[4096];
    int fd;

    fd = socket(AF_NETLINK, SOCK_RAW | SOCK_CLOEXEC, NETLINK_ROUTE);
    if (fd < 0)
        return -1;

    memset(&addr, 0, sizeof(addr));
    addr.nl_family = AF_NETLINK;

    nlh->nlmsg_flags |= NLM_F_ACK;
    nlh->nlmsg_seq = 1;

    if (sendto(fd, nlh, nlh->nlmsg_len, 0,
               (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }

    for (;;) {
        struct nlmsghdr *h;
        ssize_t n;

        n = recv(fd, buf, sizeof(buf), 0);
        if (n < 0) {
            close(fd);
            return -1;
        }

        for (h = (struct nlmsghdr *)buf; NLMSG_OK(h, n);
             h = NLMSG_NEXT(h, n)) {
            struct nlmsgerr *err;

            if (h->nlmsg_type != NLMSG_ERROR)
                continue;

            err = NLMSG_DATA(h);

            if (err->error == 0) {
                close(fd);
                return 0;
            }

            errno = -err->error;
            close(fd);
            return -1;
        }
    }
}

static int rtnl_addr_add(const char *ifname, const struct in_addr *addr,
                         uint8_t prefix_len)
{
    struct {
        struct nlmsghdr nlh;
        struct ifaddrmsg ifa;
        char buf[256];
    } req;
    unsigned int index;

    index = if_nametoindex(ifname);
    if (index == 0)
        return -1;

    memset(&req, 0, sizeof(req));

    req.nlh.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifaddrmsg));
    req.nlh.nlmsg_type = RTM_NEWADDR;
    req.nlh.nlmsg_flags = NLM_F_REQUEST | NLM_F_CREATE | NLM_F_EXCL;

    req.ifa.ifa_family = AF_INET;
    req.ifa.ifa_prefixlen = prefix_len;
    req.ifa.ifa_scope = RT_SCOPE_UNIVERSE;
    req.ifa.ifa_index = index;

    if (rtnl_addattr(&req.nlh, sizeof(req), IFA_LOCAL,
                     addr, sizeof(*addr)) < 0)
        return -1;

    if (rtnl_addattr(&req.nlh, sizeof(req), IFA_ADDRESS,
                     addr, sizeof(*addr)) < 0)
        return -1;

    if (rtnl_talk(&req.nlh) < 0)
        return -1;

    return 0;
}

static int rtnl_veth_create(const char *host_if, const char *peer_if)
{
    struct {
        struct nlmsghdr nlh;
        struct ifinfomsg ifm;
        char buf[1024];
    } req;
    struct rtattr *linkinfo;
    struct rtattr *infodata;
    struct rtattr *peer;
    struct ifinfomsg *peer_ifm;

    memset(&req, 0, sizeof(req));

    req.nlh.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifinfomsg));
    req.nlh.nlmsg_type = RTM_NEWLINK;
    req.nlh.nlmsg_flags = NLM_F_REQUEST | NLM_F_CREATE | NLM_F_EXCL;
    req.ifm.ifi_family = AF_UNSPEC;

    if (rtnl_addattr(&req.nlh, sizeof(req), IFLA_IFNAME,
                     host_if, strlen(host_if) + 1) < 0)
        return -1;

    linkinfo = rtnl_nest_start(&req.nlh, sizeof(req), IFLA_LINKINFO);
    if (linkinfo == NULL)
        return -1;

    if (rtnl_addattr(&req.nlh, sizeof(req), IFLA_INFO_KIND,
                     "veth", sizeof("veth")) < 0)
        return -1;

    infodata = rtnl_nest_start(&req.nlh, sizeof(req), IFLA_INFO_DATA);
    if (infodata == NULL)
        return -1;

    peer = rtnl_nest_start(&req.nlh, sizeof(req), VETH_INFO_PEER);
    if (peer == NULL)
        return -1;

    if (NLMSG_ALIGN(req.nlh.nlmsg_len) + sizeof(*peer_ifm) > sizeof(req)) {
        errno = EMSGSIZE;
        return -1;
    }

    peer_ifm = (struct ifinfomsg *)
        ((char *)&req + NLMSG_ALIGN(req.nlh.nlmsg_len));
    memset(peer_ifm, 0, sizeof(*peer_ifm));
    peer_ifm->ifi_family = AF_UNSPEC;
    req.nlh.nlmsg_len += sizeof(*peer_ifm);

    if (rtnl_addattr(&req.nlh, sizeof(req), IFLA_IFNAME,
                     peer_if, strlen(peer_if) + 1) < 0)
        return -1;

    rtnl_nest_end(&req.nlh, peer);
    rtnl_nest_end(&req.nlh, infodata);
    rtnl_nest_end(&req.nlh, linkinfo);

    return rtnl_talk(&req.nlh);
}

static int rtnl_link_set_netns(const char *ifname, int nsfd)
{
    struct {
        struct nlmsghdr nlh;
        struct ifinfomsg ifm;
        char buf[256];
    } req;
    unsigned int index;

    index = if_nametoindex(ifname);
    if (index == 0)
        return -1;

    memset(&req, 0, sizeof(req));

    req.nlh.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifinfomsg));
    req.nlh.nlmsg_type = RTM_NEWLINK;
    req.nlh.nlmsg_flags = NLM_F_REQUEST;
    req.ifm.ifi_family = AF_UNSPEC;
    req.ifm.ifi_index = index;

    if (rtnl_addattr(&req.nlh, sizeof(req), IFLA_NET_NS_FD,
                     &nsfd, sizeof(nsfd)) < 0)
        return -1;

    return rtnl_talk(&req.nlh);
}

static int rtnl_link_set_up(const char *ifname)
{
    struct {
        struct nlmsghdr nlh;
        struct ifinfomsg ifm;
    } req;
    unsigned int index;

    index = if_nametoindex(ifname);
    if (index == 0)
        return -1;

    memset(&req, 0, sizeof(req));

    req.nlh.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifinfomsg));
    req.nlh.nlmsg_type = RTM_NEWLINK;
    req.nlh.nlmsg_flags = NLM_F_REQUEST;
    req.ifm.ifi_family = AF_UNSPEC;
    req.ifm.ifi_index = index;
    req.ifm.ifi_flags = IFF_UP;
    req.ifm.ifi_change = IFF_UP;

    return rtnl_talk(&req.nlh);
}

static int rtnl_link_delete(const char *ifname)
{
    struct {
        struct nlmsghdr nlh;
        struct ifinfomsg ifm;
    } req;
    unsigned int index;

    index = if_nametoindex(ifname);
    if (index == 0)
        return -1;

    memset(&req, 0, sizeof(req));

    req.nlh.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifinfomsg));
    req.nlh.nlmsg_type = RTM_DELLINK;
    req.nlh.nlmsg_flags = NLM_F_REQUEST;
    req.ifm.ifi_family = AF_UNSPEC;
    req.ifm.ifi_index = index;

    return rtnl_talk(&req.nlh);
}

static int namespace_configure_child(int nsfd, const char *ifname,
                                     const struct in_addr *addr)
{
    pid_t pid;
    int status;

    pid = fork();
    if (pid < 0) {
        LL_ERR("fork");
        return -1;
    }

    if (pid == 0) {
        if (snamespace_enter(nsfd) < 0)
            _exit(errno ? errno : 1);

        if (rtnl_link_set_up("lo") < 0)
            _exit(errno ? errno : 1);

        if (rtnl_addr_add(ifname, addr, 30) < 0)
            _exit(errno ? errno : 1);

        if (rtnl_link_set_up(ifname) < 0)
            _exit(errno ? errno : 1);

        _exit(0);
    }

    if (waitpid(pid, &status, 0) < 0) {
        LL_ERR("waitpid");
        return -1;
    }

    if (!WIFEXITED(status)) {
        errno = EIO;
        return -1;
    }

    if (WEXITSTATUS(status) != 0) {
        errno = WEXITSTATUS(status);
        return -1;
    }

    return 0;
}


int snamespace_create(const char *name)
{
    pid_t pid;
    int status;

    pid = fork();
    if (pid < 0) {
        LL_ERR("fork");
        return -1;
    }

    if (pid == 0) {
        if (netns_create_child(name) < 0)
            _exit(errno ? errno : 1);

        _exit(0);
    }

    if (waitpid(pid, &status, 0) < 0) {
        LL_ERR("waitpid");
        return -1;
    }

    if (!WIFEXITED(status)) {
        errno = EIO;
        return -1;
    }

    if (WEXITSTATUS(status) != 0) {
        errno = WEXITSTATUS(status);
        return -1;
    }

    return 0;
}

int snamespace_open(const char *name)
{
    char path[PATH_MAX];
    int fd;

    if (netns_path(path, sizeof(path), name) < 0)
        return -1;

    fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0)
        return -1;

    return fd;
}

int snamespace_enter(int nsfd)
{
    if (setns(nsfd, CLONE_NEWNET) < 0)
        return -1;

    return 0;
}

int snamespace_destroy(const char *name)
{
    char path[PATH_MAX];

    if (netns_path(path, sizeof(path), name) < 0)
        return -1;

    if (umount2(path, MNT_DETACH) < 0 && errno != ENOENT)
        LL_ERR("umount2 %s", path);

    if (unlink(path) < 0 && errno != ENOENT) {
        LL_ERR("unlink %s", path);
        return -1;
    }

    return 0;
}

int snamespace_setup(struct sbd_job *job)
{
    struct snamespace ns;
    int nsfd;

    /* Initialize the namespace description: namespace name, veth names,
     * slot and IP addresses.
     */
    if (snamespace_init(&ns, job->job_id) < 0)
        return -1;

    /* Create the network namespace and pin it under /run/netns.
     */
    if (snamespace_create(ns.name) < 0)
        goto fail;

    /* Open the namespace and obtain a file descriptor referring to it.
     */
    nsfd = snamespace_open(ns.name);
    if (nsfd < 0)
        goto fail_namespace;

    /* Create the veth pair. Both ends are initially in the host
     * network namespace.
     */
    if (rtnl_veth_create(ns.sbd_if, ns.svc_if) < 0)
        goto fail_fd;

    /* Move the service-side veth endpoint into the service network
     * namespace. After this, svc_if is no longer visible from the host
     * network namespace.It changes the interface's network namespace
     * association.
     */
    if (rtnl_link_set_netns(ns.svc_if, nsfd) < 0)
        goto fail_veth;

    /* Assign the host-side address and bring the veth endpoint up.
     */
    if (rtnl_addr_add(ns.sbd_if, &ns.sbd_addr, 30) < 0)
        goto fail_veth;

    if (rtnl_link_set_up(ns.sbd_if) < 0)
        goto fail_veth;

    /* Enter the service network namespace and configure its side:
     * bring loopback up, assign the service-side address and bring
     * svc_if up.
     */
    if (namespace_configure_child(nsfd, ns.svc_if, &ns.svc_addr) < 0)
        goto fail_veth;

    close(nsfd);
    return 0;

fail_veth:
    rtnl_link_delete(ns.sbd_if);

fail_fd:
    close(nsfd);

fail_namespace:
    snamespace_destroy(ns.name);

fail:
    return -1;
}

int snamespace_enter_job(const struct sbd_job *job)
{
    char nsname[LL_BUFSIZ_64];
    int nsfd;

    snprintf(nsname, sizeof(nsname), "svc%ld", job->job_id);

    nsfd = snamespace_open(nsname);
    if (nsfd < 0)
        return -1;

    if (snamespace_enter(nsfd) < 0) {
        close(nsfd);
        return -1;
    }

    close(nsfd);
    return 0;
}

int snamespace_destroy_job(const struct sbd_job *job)
{
    char nsname[LL_BUFSIZ_64];

    snprintf(nsname, sizeof(nsname), "svc%ld", job->job_id);

    if (snamespace_destroy(nsname) < 0)
        return -1;

    return 0;
}
