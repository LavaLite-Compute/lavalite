// Copyright (C) LavaLite Contributors
// GPL v2

#include <arpa/inet.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <sched.h>

#include "base/lib/ll.syslog.h"
#include "batch/sbd/snamespace.h"
#include "batch/sbd/sbd.h"

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

/* Both ip(8) and nft(8) run once per job start/stop, never on a hot
 * path, so a blocking wait would normally be harmless -- except
 * snamespace_setup()/snamespace_destroy_job() run inline in sbd's
 * single-threaded event loop. If either command ever hung (stuck
 * netlink socket, whatever), an unbounded wait would stall the whole
 * daemon, not just this one job. Bound it, same convention already
 * used for chan_connect()'s 3s timeout elsewhere.
 */
#define CMD_TIMEOUT_SECS 3

static uint16_t next_slot;

static void namespace_addresses(struct snamespace *ns)
{
    uint16_t slot = next_slot;

    next_slot++;
    if (next_slot >= SVC_NET_NSLOTS)
        next_slot = 0;

    uint32_t base;

    base = SVC_NET_BASE + ((uint32_t)slot * SVC_NET_SLOT_ADDRS);

    ns->sbd_addr.s_addr = htonl(base + 1);
    ns->svc_addr.s_addr = htonl(base + 2);
}

static int namespace_names(struct snamespace *ns)
{
    int n;

    n = snprintf(ns->name, sizeof(ns->name),
                 "svc%ld", ns->job_id);
    if (n < 0 || (size_t)n >= sizeof(ns->name)) {
        errno = ENAMETOOLONG;
        return -1;
    }

    n = snprintf(ns->sbd_if, sizeof(ns->sbd_if),
                 "ll%lda", ns->job_id);
    if (n < 0 || (size_t)n >= sizeof(ns->sbd_if)) {
        errno = ENAMETOOLONG;
        return -1;
    }

    n = snprintf(ns->svc_if, sizeof(ns->svc_if),
                 "ll%ldb", ns->job_id);
    if (n < 0 || (size_t)n >= sizeof(ns->svc_if)) {
        errno = ENAMETOOLONG;
        return -1;
    }

    return 0;
}

static int snamespace_init(struct snamespace *ns, int64_t job_id)
{
    memset(ns, 0, sizeof(*ns));
    ns->job_id = job_id;

    if (namespace_names(ns) < 0)
        return -1;

    namespace_addresses(ns);

    char sbd_addr[INET_ADDRSTRLEN];
    char svc_addr[INET_ADDRSTRLEN];

    if (inet_ntop(AF_INET, &ns->sbd_addr, sbd_addr, sizeof(sbd_addr)) == NULL)
        strcpy(sbd_addr, "<invalid>");

    if (inet_ntop(AF_INET, &ns->svc_addr, svc_addr, sizeof(svc_addr)) == NULL)
        strcpy(svc_addr, "<invalid>");

    LL_DEBUG("netns=%s job_id=%ld ll%lda=%s ll%ldb=%s", ns->name, job_id,
             job_id, sbd_addr, job_id, svc_addr);

    return 0;
}

static int netns_path(char *buf, size_t len, const char *name)
{
    int n;

    n = snprintf(buf, len, "/run/netns/%s", name);
    if (n < 0 || (size_t)n >= len) {
        errno = ENAMETOOLONG;
        return -1;
    }

    return 0;
}

/*
 * Shared exec helper for ip(8) and nft(8) -- both are one-shot,
 * non-hot-path commands, so fork+exec+wait beats hand-rolling either
 * protocol (rtnetlink's attribute-tree layout for links/addrs, or
 * nftables' netlink batch format for the DNAT rule): both ip and nft
 * exist precisely because upstream decided those raw protocols are
 * worth wrapping, tested across every kernel this will ever run on.
 *
 * We only ever check the exit code, never parse stdout, so neither
 * command's human-readable output format can break us.
 *
 * Bounded wait: see CMD_TIMEOUT_SECS above.
 */
static int run_cmd(char *const argv[])
{
    pid_t pid;
    int status;

    pid = fork();
    if (pid < 0) {
        LL_ERR("fork");
        return -1;
    }

    if (pid == 0) {
        execvp(argv[0], argv);
        _exit(127); /* execvp failed: binary not found or not executable */
    }

    time_t deadline = time(NULL) + CMD_TIMEOUT_SECS;

    for (;;) {
        pid_t w = waitpid(pid, &status, WNOHANG);

        if (w == pid)
            break;

        if (w < 0) {
            if (errno == EINTR)
                continue;
            LL_ERR("waitpid");
            return -1;
        }

        /* w == 0: still running */
        if (time(NULL) >= deadline) {
            LL_ERR("%s timed out after %ds, killing pid=%d", argv[0],
                   CMD_TIMEOUT_SECS, (int) pid);
            kill(pid, SIGKILL);
            waitpid(pid, &status, 0); /* reap, avoid zombie */
            errno = ETIMEDOUT;
            return -1;
        }

        struct timespec ts = {.tv_sec = 0, .tv_nsec = 20 * 1000 * 1000};
        nanosleep(&ts, NULL);
    }

    if (WIFEXITED(status) && WEXITSTATUS(status) == 127) {
        LL_ERR("%s: command not found -- is it installed?", argv[0]);
        errno = ENOENT;
        return -1;
    }

    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        LL_ERR("%s %s failed", argv[0], argv[1] ? argv[1] : "");
        errno = EIO;
        return -1;
    }

    return 0;
}

/*
 * DNAT: exposing the service inside the netns to the rest of the
 * cluster.
 *
 * Each job gets its own nft table, named the same as its netns
 * (svc<job_id>). The table's only chain is a base "prerouting" nat
 * hook holding one rule: rewrite the destination of anything arriving
 * on ext_port to svc_addr:app_port. Since it's a whole table, cleanup
 * is one command -- "nft delete table" -- no rule handles to track.
 */
static int snamespace_nat_destroy(const char *name)
{
    char *argv[] = {"nft", "delete", "table", "ip", (char *)name, NULL};

    return run_cmd(argv);
}

static int snamespace_nat_setup(const char *name, const struct in_addr *svc_addr,
                                int ext_port, int app_port)
{
    char addr[INET_ADDRSTRLEN];
    char dnat_to[INET_ADDRSTRLEN + 8];
    char dport[8];

    if (inet_ntop(AF_INET, svc_addr, addr, sizeof(addr)) == NULL) {
        errno = EINVAL;
        return -1;
    }

    snprintf(dnat_to, sizeof(dnat_to), "%s:%d", addr, app_port);
    snprintf(dport, sizeof(dport), "%d", ext_port);

    char *argv_table[] = {"nft", "add", "table", "ip", (char *)name, NULL};
    char *argv_chain[] = {"nft", "add", "chain", "ip", (char *)name, "prerouting",
                          "{", "type", "nat", "hook", "prerouting", "priority",
                          "dstnat", ";", "}", NULL};
    char *argv_rule[] = {"nft", "add", "rule", "ip", (char *)name, "prerouting",
                         "tcp", "dport", dport, "dnat", "to", dnat_to, NULL};

    if (run_cmd(argv_table) < 0)
        return -1;

    if (run_cmd(argv_chain) < 0) {
        snamespace_nat_destroy(name);
        return -1;
    }

    if (run_cmd(argv_rule) < 0) {
        snamespace_nat_destroy(name);
        return -1;
    }

    return 0;
}

/*
 * Host-side veth setup: create the pair, move svc_if into the job's
 * netns, address+up the sbd_if end that stays in the root netns.
 *
 * "ip link set svc_if netns name" takes the netns by name (resolved
 * via /run/netns), no fd needed for this -- only actually entering
 * the namespace later (snamespace_enter_job(), at spawn time) needs
 * one, and that stays a raw setns() call; see the comment there.
 */
static int ip_veth_setup(const char *sbd_if, const char *svc_if,
                         const char *name, const struct in_addr *sbd_addr)
{
    char addr[INET_ADDRSTRLEN];
    char addr_cidr[INET_ADDRSTRLEN + 4];

    if (inet_ntop(AF_INET, sbd_addr, addr, sizeof(addr)) == NULL) {
        errno = EINVAL;
        return -1;
    }
    snprintf(addr_cidr, sizeof(addr_cidr), "%s/30", addr);

    char *argv_veth[] = {"ip", "link", "add", (char *)sbd_if, "type", "veth",
                         "peer", "name", (char *)svc_if, NULL};
    char *argv_move[] = {"ip", "link", "set", (char *)svc_if, "netns",
                         (char *)name, NULL};
    char *argv_addr[] = {"ip", "addr", "add", addr_cidr, "dev",
                         (char *)sbd_if, NULL};
    char *argv_up[] = {"ip", "link", "set", (char *)sbd_if, "up", NULL};

    if (run_cmd(argv_veth) < 0)
        return -1;

    if (run_cmd(argv_move) < 0)
        goto fail;

    if (run_cmd(argv_addr) < 0)
        goto fail;

    if (run_cmd(argv_up) < 0)
        goto fail;

    return 0;

fail:
    {
        /* Deleting either end of a veth pair deletes both, even when
         * the peer has already moved into another (or since-deleted)
         * namespace -- this is enough to undo argv_veth/argv_move
         * regardless of which later step failed.
         */
        char *argv_del[] = {"ip", "link", "delete", (char *)sbd_if, NULL};
        run_cmd(argv_del);
    }
    return -1;
}

/*
 * Service-side configuration, run inside the job's netns via
 * "ip netns exec": bring loopback up, address+up svc_if.
 */
static int ip_netns_configure_svc_side(const char *name, const char *svc_if,
                                       const struct in_addr *svc_addr)
{
    char addr[INET_ADDRSTRLEN];
    char addr_cidr[INET_ADDRSTRLEN + 4];

    if (inet_ntop(AF_INET, svc_addr, addr, sizeof(addr)) == NULL) {
        errno = EINVAL;
        return -1;
    }
    snprintf(addr_cidr, sizeof(addr_cidr), "%s/30", addr);

    char *argv_lo[] = {"ip", "netns", "exec", (char *)name,
                       "ip", "link", "set", "lo", "up", NULL};
    char *argv_addr[] = {"ip", "netns", "exec", (char *)name,
                         "ip", "addr", "add", addr_cidr, "dev",
                         (char *)svc_if, NULL};
    char *argv_up[] = {"ip", "netns", "exec", (char *)name,
                       "ip", "link", "set", (char *)svc_if, "up", NULL};

    if (run_cmd(argv_lo) < 0)
        return -1;

    if (run_cmd(argv_addr) < 0)
        return -1;

    if (run_cmd(argv_up) < 0)
        return -1;

    return 0;
}

int snamespace_create(const char *name)
{
    char *argv[] = {"ip", "netns", "add", (char *)name, NULL};

    return run_cmd(argv);
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

/*
 * Called inline in the middle of the job-spawn path (child_exec_job()
 * in sjob.c), while still root, right before it drops privileges and
 * execs the real job. That ordering is the reason this stays a raw
 * setns() rather than "ip netns exec": ip netns exec enters the
 * namespace and immediately execs its argument -- there is no gap
 * for the caller to keep running its own code (drop privileges, cd
 * into the job's working directory) in between. setns() itself is a
 * single documented syscall, not an intricate protocol like
 * rtnetlink's link/addr setup was -- nothing here to gain by
 * outsourcing to an external command.
 */
int snamespace_enter(int nsfd)
{
    if (setns(nsfd, CLONE_NEWNET) < 0)
        return -1;

    return 0;
}

int snamespace_destroy(const char *name)
{
    char *argv[] = {"ip", "netns", "delete", (char *)name, NULL};

    return run_cmd(argv);
}

int snamespace_setup(struct sbd_job *job)
{
    struct snamespace ns;

    /* Initialize the namespace description: namespace name, veth names,
     * slot and IP addresses.
     */
    if (snamespace_init(&ns, job->job_id) < 0)
        return -1;

    /* Create the network namespace, pinned under /run/netns.
     */
    if (snamespace_create(ns.name) < 0)
        goto fail;

    /* Create the veth pair, move the service-side end into the job's
     * netns, address+up the end that stays in the root netns.
     */
    if (ip_veth_setup(ns.sbd_if, ns.svc_if, ns.name, &ns.sbd_addr) < 0)
        goto fail_namespace;

    /* Configure the service side: bring loopback up, address+up
     * svc_if, all inside the job's netns.
     */
    if (ip_netns_configure_svc_side(ns.name, ns.svc_if, &ns.svc_addr) < 0)
        goto fail_veth;

    /* Expose svc_addr:app_port to the cluster as node_ip:ext_port,
     * via a DNAT rule in the host's root netns.
     */
    if (snamespace_nat_setup(ns.name, &ns.svc_addr, job->ext_port,
                             job->app_port) < 0)
        goto fail_veth;

    return 0;

fail_veth:
    {
        char *argv_del[] = {"ip", "link", "delete", ns.sbd_if, NULL};
        run_cmd(argv_del);
    }

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

    /* Best-effort: a job that never got past snamespace_nat_setup()
     * still needs the netns torn down, so don't bail out on this.
     */
    snamespace_nat_destroy(nsname);

    if (snamespace_destroy(nsname) < 0)
        return -1;

    return 0;
}
