/* Copyright (C) LavaLite Contributors
 * GPL v2
 */

#include "base/lib/ll.lim.h"

__thread int lserrno = 0;

static uint16_t initialized;
static struct lim_master master;
static uint16_t lim_port;
static int lim_chan_tcp = -1;
static uint16_t master_known;
static int conntimeout;
static int recvtimeout;

int ll_init(void)
{
    if (initialized)
        return 0;

    char *conf_dir = getenv("LL_ENVDIR");
    if (conf_dir == NULL)
        return -1;

    char path[PATH_MAX];
    int cc = snprintf(path, sizeof(path), "%s/ll.conf", conf_dir);
    if (cc < 0 || cc >= (int)sizeof(path))
        return -1;

    if (ll_conf_load(ll_params, PARAMS_COUNT, path) < 0)
        return -1;

    if (ll_params[LL_LIM_PORT].val == NULL)
        return -1;

    if (! ll_atoi(ll_params[LL_LIM_PORT].val, (int *)&lim_port))
        return -1;

    if (! ll_atoi(ll_params[LL_API_CONNTIMEOUT].val, &conntimeout))
        return -1;

    if (! ll_atoi(ll_params[LL_API_RECVTIMEOUT].val, &recvtimeout))
        return -1;

    chan_init();

    initialized = 1;
    return 0;
}

/*
 * Build a request buffer containing only a protocol header with the
 * given opcode. Returns encoded byte count, -1 on error.
 */
static int build_hdr_request(int opcode, char *buf, size_t bufsz)
{
    struct protocol_header hdr;
    init_protocol_header(&hdr);
    hdr.operation = opcode;

    XDR xdrs;
    xdrmem_create(&xdrs, buf, bufsz, XDR_ENCODE);
    if (!ll_encode_msg(&xdrs, NULL, NULL, &hdr)) {
        xdr_destroy(&xdrs);
        return -1;
    }
    int len = (int)xdr_getpos(&xdrs);
    xdr_destroy(&xdrs);
    return len;
}
/*
 * Send a single UDP request to the local LIM and receive a reply.
 * rep must be a caller-supplied buffer of rep_size bytes.
 * Returns 0 on success, -1 on error.
 */
static int call_lim_udp(int opcode, char *rep, size_t rep_size)
{
    if (ll_init() < 0)
        return -1;

    int ch = chan_udp_client();
    if (ch < 0)
        return -1;

    char req[LL_BUFSIZE_256];
    int req_len = build_hdr_request(opcode, req, sizeof(req));
    if (req_len < 0) {
        chan_close(ch);
        return -1;
    }

    struct sockaddr_in lim_addr = {
        .sin_family      = AF_INET,
        .sin_addr.s_addr = htonl(INADDR_LOOPBACK),
        .sin_port        = htons(lim_port),
    };

    if (chan_send_dgram(ch, req, (size_t)req_len, &lim_addr) < 0) {
        chan_close(ch);
        return -1;
    }

    struct sockaddr_in from;
    if (chan_recv_dgram(ch, rep, rep_size, &from, recvtimeout * 1000) < 0) {
        chan_close(ch);
        return -1;
    }
    chan_close(ch);
    return 0;
}

/*
 * Ask local LIM for current master via UDP. Caches result. Idempotent.
 */
static int lim_get_master(void)
{
    if (master_known)
        return 0;

    char reply[LL_BUFSIZE_256];
    if (call_lim_udp(LIM_GET_MASTER_NAME, reply, sizeof(reply)) < 0)
        return -1;

    struct protocol_header reply_hdr;
    struct wire_master wm;
    XDR xdrs;

    memset(&wm, 0, sizeof(wm));
    xdrmem_create(&xdrs, reply, sizeof(reply), XDR_DECODE);
    if (!xdr_pack_hdr(&xdrs, &reply_hdr)) {
        xdr_destroy(&xdrs);
        return -1;
    }
    if (reply_hdr.status != LIM_OK) {
        xdr_destroy(&xdrs);
        return -1;
    }
    if (!xdr_wire_master(&xdrs, &wm)) {
        xdr_destroy(&xdrs);
        return -1;
    }
    xdr_destroy(&xdrs);

    if (get_host_by_name(wm.hostname, &master.host) < 0)
        return -1;

    master_known = 1;
    return 0;
}

char *ll_mastername(void)
{
    if (lim_get_master() < 0)
        return NULL;

    static __thread char buf[MAXHOSTNAMELEN];
    strncpy(buf, master.host.name, MAXHOSTNAMELEN - 1);
    buf[MAXHOSTNAMELEN - 1] = '\0';
    return buf;
}

char *ll_clustername(void)
{
    char reply[LL_BUFSIZE_256];
    if (call_lim_udp(LIM_GET_CLUSTER_NAME, reply, sizeof(reply)) < 0)
        return NULL;

    struct protocol_header reply_hdr;
    struct wire_cluster wc;
    XDR xdrs;

    memset(&wc, 0, sizeof(wc));
    xdrmem_create(&xdrs, reply, sizeof(reply), XDR_DECODE);
    if (!xdr_pack_hdr(&xdrs, &reply_hdr)) {
        xdr_destroy(&xdrs);
        return NULL;
    }
    if (reply_hdr.status != LIM_OK) {
        xdr_destroy(&xdrs);
        return NULL;
    }
    if (!xdr_wire_cluster(&xdrs, &wc)) {
        xdr_destroy(&xdrs);
        return NULL;
    }
    xdr_destroy(&xdrs);

    static __thread char buf[LL_BUFSIZE_64];
    strncpy(buf, wc.name, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = 0;
    return buf;
}

/*
 * Send req_len bytes to LIM master via TCP, receive reply.
 * On success: *rep points to allocated payload (caller must free),
 *             reply_hdr is filled.
 * On error: returns -1, sets lserrno.
 * Keeps a persistent TCP connection; reconnects on failure.
 */
static int call_lim_tcp(const void *req, size_t req_len,
                        void **rep, struct protocol_header *reply_hdr)
{
    if (lim_get_master() < 0) {
        lserrno = LL_ERR_NO_MASTER;
        return -1;
    }

    if (lim_chan_tcp < 0) {
        lim_chan_tcp = chan_tcp_client();
        if (lim_chan_tcp < 0)
            return -1;

        struct sockaddr_in addr;
        get_host_addrv4(&master.host, &addr);
        addr.sin_family = AF_INET;
        addr.sin_port   = htons(lim_port);

        if (chan_connect(lim_chan_tcp, &addr, conntimeout * 1000, 0) < 0) {
            lserrno = LL_ERR_LIM_DOWN;
            chan_close(lim_chan_tcp);
            lim_chan_tcp = -1;
            return -1;
        }
    }

    struct chan_buffer sndbuf = {.data = (void *)req, .len = req_len};
    struct chan_buffer rcvbuf = {0};

    // timeout is in seconds
    if (chan_rpc(lim_chan_tcp, &sndbuf, &rcvbuf, reply_hdr, recvtimeout) < 0) {
        lserrno = LL_ERR_LIM_DOWN;
        chan_close(lim_chan_tcp);
        lim_chan_tcp = -1;
        return -1;
    }

    if (reply_hdr->status != LIM_OK) {
        lserrno = LL_ERR_LIM_DOWN;
        free(rcvbuf.data);
        chan_close(lim_chan_tcp);
        lim_chan_tcp = -1;
        return -1;
    }

    *rep = rcvbuf.data;
    return 0;
}

/*
 * Returns a malloc'd array of n ll_load structs, n written to *nloads.
 * Caller must free.
 */
struct ll_host_load *ll_hostload(int *nloads)
{
    char req[LL_BUFSIZE_256];
    int req_len = build_hdr_request(LIM_GET_LOAD, req, sizeof(req));
    if (req_len < 0)
        return NULL;

    void *rep = NULL;
    struct protocol_header reply_hdr;
    if (call_lim_tcp(req, (size_t)req_len, &rep, &reply_hdr) < 0)
        return NULL;

    struct wire_loads wls;
    memset(&wls, 0, sizeof(struct wire_loads));

    XDR xdrs;
    xdrmem_create(&xdrs, rep, (unsigned int)reply_hdr.length, XDR_DECODE);
    bool_t ok = xdr_wire_load_array(&xdrs, &wls);
    xdr_destroy(&xdrs);
    free(rep);
    if (!ok || wls.nloads == 0) {
        xdr_free((xdrproc_t)xdr_wire_load_array, &wls);
        lserrno = LL_ERR_PROTO;
        return NULL;
    }

    struct ll_host_load *loads = calloc(wls.nloads, sizeof(struct ll_host_load));
    if (!loads) {
        xdr_free((xdrproc_t)xdr_wire_load_array, &wls);
        return NULL;
    }
    for (uint32_t i = 0; i < wls.nloads; i++) {
        strncpy(loads[i].hostname, wls.loads[i].hostname, MAXHOSTNAMELEN - 1);
        loads[i].hostname[MAXHOSTNAMELEN - 1] = 0;
        loads[i].status      = wls.loads[i].status;
        loads[i].num_metrics = NUM_METRICS;
        memcpy(loads[i].li, wls.loads[i].li, NUM_METRICS * sizeof(float));
    }
    xdr_free((xdrproc_t)xdr_wire_load_array, &wls);
    *nloads = (int)wls.nloads;
    return loads;
}

/*
 * Returns a malloc'd array of n ll_host structs, n written to *nhosts.
 * Caller must free.
 */
struct ll_host_info *ll_hostinfo(int *nhosts)
{
    char req[LL_BUFSIZE_256];
    int req_len = build_hdr_request(LIM_GET_HOSTS, req, sizeof(req));
    if (req_len < 0)
        return NULL;

    void *rep = NULL;
    struct protocol_header reply_hdr;
    if (call_lim_tcp(req, (size_t)req_len, &rep, &reply_hdr) < 0)
        return NULL;

    struct wire_hosts whs;
    memset(&whs, 0, sizeof(struct wire_hosts));
    XDR xdrs;
    xdrmem_create(&xdrs, rep, (unsigned int)reply_hdr.length, XDR_DECODE);

    bool_t ok = xdr_wire_host_array(&xdrs, &whs);
    xdr_destroy(&xdrs);
    free(rep);
    if (!ok || whs.nhosts == 0) {
        lserrno = LL_ERR_PROTO;
        return NULL;
    }

    struct ll_host_info *hosts = calloc(whs.nhosts, sizeof(struct ll_host_info));
    if (!hosts) {
        xdr_free((xdrproc_t)xdr_wire_host_array, &whs);
        return NULL;
    }

    for (uint32_t i = 0; i < whs.nhosts; i++) {
        strncpy(hosts[i].host_name, whs.hosts[i].hostname, MAXHOSTNAMELEN - 1);
        hosts[i].host_name[MAXHOSTNAMELEN - 1] = 0;
        strncpy(hosts[i].host_type, whs.hosts[i].machine, LL_BUFSIZE_32 - 1);
        hosts[i].host_type[LL_BUFSIZE_32 - 1] = 0;
        hosts[i].num_cpus  = whs.hosts[i].num_cpus;
        hosts[i].max_mem   = whs.hosts[i].max_mem;
        hosts[i].max_swap  = whs.hosts[i].max_swap;
        hosts[i].max_tmp   = whs.hosts[i].max_tmp;
        hosts[i].is_master = whs.hosts[i].is_candidate;
    }
    xdr_free((xdrproc_t)xdr_wire_host_array, &whs);
    *nhosts = (int)whs.nhosts;
    return hosts;
}

/*
 * Returns a malloc'd ll_cluster. Caller must free.
 */
struct ll_cluster_info *ll_clusterinfo(void)
{
    char req[LL_BUFSIZE_256];
    int req_len = build_hdr_request(LIM_GET_CLUSTER_NAME, req, sizeof(req));
    if (req_len < 0)
        return NULL;

    void *rep = NULL;
    struct protocol_header reply_hdr;
    if (call_lim_tcp(req, (size_t)req_len, &rep, &reply_hdr) < 0)
        return NULL;

    struct wire_cluster wc;
    memset(&wc, 0, sizeof(wc));
    XDR xdrs;
    xdrmem_create(&xdrs, rep, (unsigned int)reply_hdr.length, XDR_DECODE);
    bool_t ok = xdr_wire_cluster(&xdrs, &wc);
    xdr_destroy(&xdrs);
    free(rep);

    if (!ok) {
        lserrno = LL_ERR_PROTO;
        return NULL;
    }

    struct ll_cluster_info *cl = calloc(1, sizeof(struct ll_cluster_info));
    if (cl == NULL)
        return NULL;

    strncpy(cl->cluster_name, wc.name, sizeof(cl->cluster_name) - 1);
    cl->cluster_name[sizeof(cl->cluster_name) - 1] = '\0';

    strncpy(cl->manager_name, wc.admin, sizeof(cl->manager_name) - 1);
    cl->manager_name[sizeof(cl->manager_name) - 1] = '\0';

    if (master_known) {
        strncpy(cl->master_name, master.host.name, MAXHOSTNAMELEN - 1);
        cl->master_name[MAXHOSTNAMELEN - 1] = '\0';
    }

    return cl;
}
