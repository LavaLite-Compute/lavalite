/* Copyright (C) LavaLite Contributors
 * GPL v2
 */

#include "include/base/lib/ll.lim.h"

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

    char *conf_dir = getenv("LSF_ENVDIR");
    if (conf_dir == NULL)
        return -1;

    char path[PATH_MAX];
    int cc = snprintf(path, sizeof(path), "%s/lsf.conf", conf_dir);
    if (cc < 0 || cc >= (int)sizeof(path)) {
        return -1;
    }

    if (ll_conf_load(ll_params, LL_PARAMS_COUNT, path) < 0)
        return -1;

    if (ll_params[LSF_LIM_PORT].val == NULL) {
        return -1;
    }

    int port;
    if (ll_atoi(ll_params[LSF_LIM_PORT].val, &port) < 0)
        return -1;
    lim_port = (uint16_t)port;

    if (ll_atoi(ll_params[LSF_API_CONNTIMEOUT].val, &conntimeout) < 0)
        return -1;

    if (ll_atoi(ll_params[LSF_API_RECVTIMEOUT].val, &port) < 0)
        return -1;

    initialized = 1;
    return 0;
}

static int lim_get_master(void)
{
    if (master_known)
        return 0;

    if (ll_init() < 0)
        return -1;

    struct sockaddr_in lim_addr = {
        .sin_family      = AF_INET,
        .sin_addr.s_addr = htonl(INADDR_LOOPBACK),
        .sin_port        = htons(lim_port),
    };
    int ch = chan_client_socket(AF_INET, SOCK_DGRAM, 0);
    if (ch < 0)
        return -1;

    struct protocol_header hdr;
    init_protocol_hdr(&hdr);
    hdr.operation = LIM_GET_MASTER_NAME;
    hdr.status    = LIM_OK;

    char req[LL_BUFSIZE_256];
    XDR xdrs;

    xdrmem_create(&xdrs, req, sizeof(req), XDR_ENCODE);
    if (!xdr_pack_hdr(&xdrs, &hdr)) {
        xdr_destroy(&xdrs);
        chan_close(ch);
        return -1;
    }
    size_t len = xdr_getpos(&xdrs);
    xdr_destroy(&xdrs);
    if (chan_send_dgram(ch, req, len, &lim_addr) < 0) {
        chan_close(ch);
        return -1;
    }

    char reply[LL_BUFSIZE_256];
    struct sockaddr_storage from;
    if (chan_recv_dgram(ch, reply, sizeof(reply), &from,
                        conntimeout * 1000) < 0) {
        chan_close(ch);
        return -1;
    }
    chan_close(ch);

    struct wire_master wm;
    xdrmem_create(&xdrs, reply, sizeof(reply), XDR_DECODE);
    if (!xdr_wire_master(&xdrs, &wm)) {
        xdr_destroy(&xdrs);
        return -1;
    }
    xdr_destroy(&xdrs);

    if (get_host_by_name(master.host.name, &master.host) < 0)
        return -1;

    master.tcp_port = wm.tcp_port;

    master_known = 1;
    return 0;
}

char *ls_getmastername(void)
{
    if (ll_init() < 0)
        return NULL;

    if (lim_get_master() < 0)
        return NULL;

    static __thread char buf[MAXHOSTNAMELEN];
    strncpy(buf, master.host.name, MAXHOSTNAMELEN);
    buf[MAXHOSTNAMELEN - 1] = 0;

    return buf;
}

/*
 * Send req_len bytes from req to LIM master via TCP.
 * Reads reply header, allocates *rep to reply_hdr->length bytes,
 * reads payload into it. Caller owns *rep and must free it.
 * Returns 0 on success, -1 on error.
 */
static int call_lim_tcp(const void *req, size_t req_len,
                        void **rep, struct protocol_header *reply_hdr)
{
    if (lim_get_master() < 0)
        return -1;

    if (lim_chan_tcp < 0) {
        lim_chan_tcp = chan_client_socket(AF_INET, SOCK_STREAM, 0);
        if (lim_chan_tcp < 0)
            return -1;
        struct sockaddr_in addr;
        get_host_addrv4(&master.host, &addr);
        addr.sin_family = AF_INET;
        addr.sin_port = master.tcp_port;

        if (chan_connect(lim_chan_tcp, &addr, conntimeout * 1000, 0) < 0) {
            if (errno == ECONNREFUSED)
                lserrno = LL_ERR_LIM_DOWN;
            chan_close(lim_chan_tcp);
            lim_chan_tcp = -1;
            return -1;
        }
    }

    // chan talks buff
    struct chan_buffer sndbuf = {.data = (void *)req, .len = req_len};
    struct chan_buffer rcvbuf = {0};

    if (chan_rpc(lim_chan_tcp, &sndbuf, &rcvbuf, reply_hdr,
                 recvtimeout * 1000) < 0) {
        chan_close(lim_chan_tcp);
        lim_chan_tcp = -1;
        return -1;
    }

    if (reply_hdr->status != LIM_OK) {
        lserrno = reply_hdr->status;  /* let caller interpret */
        free(rcvbuf.data);
        chan_close(lim_chan_tcp);
        lim_chan_tcp = -1;
        return -1;
    }

    *rep = rcvbuf.data;
    return 0;
}

struct ll_load *ls_load(int *n)
{
    (void)call_lim_tcp(NULL, 0, NULL, NULL);
    return NULL;
}
